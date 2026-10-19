// cloud3ds.cpp — ver cloud3ds.h.
#include "platform/3ds/cloud3ds.h"

#include <3ds.h>
#include <curl/curl.h>
#include <malloc.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace ps {
// Configuración de Firebase de la web (js/core/firebase-config.js, la saca
// tools/gen_data.js): no es secreta, va en la web; lo que protege las
// partidas son las reglas de Firestore.
extern const char* const kWebApiKey;
extern const char* const kWebProject;

namespace cloud3ds {

namespace {


std::string g_dir;
Account g_acc;
cloud::Config g_cfg;
bool g_soc = false;
u32* g_socbuf = nullptr;
Thread g_thr = nullptr;
LightEvent g_ev;
volatile bool g_quit = false;
volatile int g_state = JOB_IDLE;
int g_kind = 0;                  // 1 = entrar + sincronizar, 2 = sincronizar
std::string g_name, g_pass, g_err, g_curl_err;
Json g_local, g_result;

std::string path(const char* f) { return g_dir + "/" + f; }

bool read_file(const std::string& p, std::string& out) {
  FILE* f = std::fopen(p.c_str(), "rb");
  if (!f) return false;
  char b[4096];
  size_t n;
  out.clear();
  while ((n = std::fread(b, 1, sizeof(b), f)) > 0) out.append(b, n);
  std::fclose(f);
  return true;
}

void save_account() {
  Json o = Json::object();
  o.set("name", Json::str(g_acc.name));
  o.set("uid", Json::str(g_acc.uid));
  o.set("refresh", Json::str(g_acc.refresh));
  o.set("dev", Json::str(g_acc.dev));
  o.set("delta", Json::num(double(g_acc.delta)));
  o.set("lastSync", Json::num(double(g_acc.last_sync)));
  const std::string t = o.dump();
  const std::string tmp = path("cuenta.tmp"), cur = path("cuenta.json");
  FILE* f = std::fopen(tmp.c_str(), "wb");
  if (!f) return;
  std::fwrite(t.data(), 1, t.size(), f);
  std::fclose(f);
  std::remove(cur.c_str());
  std::rename(tmp.c_str(), cur.c_str());
}

int64_t now_ms() { return int64_t(osGetTime()) - 2208988800000LL; }

// ---- HTTP con curl ----
size_t on_body(char* p, size_t sz, size_t n, void* ud) { static_cast<std::string*>(ud)->append(p, sz * n); return sz * n; }
size_t on_header(char* p, size_t sz, size_t n, void* ud) {
  const std::string h(p, sz * n);
  if (h.size() > 5 && (h.compare(0, 5, "Date:") == 0 || h.compare(0, 5, "date:") == 0)) {
    const time_t t = curl_getdate(h.substr(5).c_str(), nullptr);
    if (t > 0) static_cast<cloud::HttpResp*>(ud)->date_ms = int64_t(t) * 1000;
  }
  return sz * n;
}
bool http(const cloud::HttpReq& rq, cloud::HttpResp& rs, void*) {
  CURL* c = curl_easy_init();
  if (!c) return false;
  curl_slist* hs = nullptr;
  for (const auto& h : rq.headers) hs = curl_slist_append(hs, h.c_str());
  curl_easy_setopt(c, CURLOPT_URL, rq.url.c_str());
  curl_easy_setopt(c, CURLOPT_CUSTOMREQUEST, rq.method.c_str());
  if (rq.method == "POST") {
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, rq.body.c_str());
    curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, long(rq.body.size()));
  }
  curl_easy_setopt(c, CURLOPT_HTTPHEADER, hs);
  curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, on_body);
  curl_easy_setopt(c, CURLOPT_WRITEDATA, &rs.body);
  curl_easy_setopt(c, CURLOPT_HEADERFUNCTION, on_header);
  curl_easy_setopt(c, CURLOPT_HEADERDATA, &rs);
  curl_easy_setopt(c, CURLOPT_CAINFO, "romfs:/cacert.pem");
  curl_easy_setopt(c, CURLOPT_USERAGENT, "PokeSurvivor3DS/1.0");
  curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(c, CURLOPT_TIMEOUT, 25L);
  curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
  const CURLcode r = curl_easy_perform(c);
  if (r != CURLE_OK) g_curl_err = curl_easy_strerror(r);
  long st = 0;
  curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &st);
  rs.status = r == CURLE_OK ? int(st) : 0;
  curl_slist_free_all(hs);
  curl_easy_cleanup(c);
  return r == CURLE_OK;
}

bool net_up() {
  if (g_soc) return true;
  g_socbuf = static_cast<u32*>(memalign(0x1000, 0x100000));
  if (!g_socbuf || R_FAILED(socInit(g_socbuf, 0x100000))) { std::free(g_socbuf); g_socbuf = nullptr; return false; }
  curl_global_init(CURL_GLOBAL_DEFAULT);
  g_soc = true;
  return true;
}

void worker(void*) {
  while (!g_quit) {
    LightEvent_Wait(&g_ev);
    if (g_quit) break;
    if (g_state != JOB_BUSY) continue;
    bool ok = false;
    if (!net_up()) {
      g_err = "No hay conexión a internet (¿wifi apagado?).";
    } else {
      cloud::Client c;
      c.cfg = g_cfg;
      c.http = http;
      c.clock_delta = g_acc.delta;
      cloud::Session s;
      s.name = g_acc.name; s.uid = g_acc.uid; s.refresh_token = g_acc.refresh;
      bool in = true;
      if (g_kind == 1) {
        in = cloud::login(c, g_name, g_pass, s);
        g_pass.assign(g_pass.size(), '\0');
        g_pass.clear();
      }
      Json merged;
      if (in && cloud::sync(c, s, g_local, g_acc.dev, now_ms(), merged)) {
        g_acc.on = true;
        g_acc.name = s.name; g_acc.uid = s.uid; g_acc.refresh = s.refresh_token;
        g_acc.delta = c.clock_delta;
        g_acc.last_sync = now_ms();
        save_account();
        g_result = merged;
        ok = true;
      } else {
        g_err = c.error.empty() ? "No se ha podido sincronizar." : c.error;
        if (g_kind == 2 && c.error.find("caducado") != std::string::npos) { g_acc.on = false; save_account(); }
      }
    }
    g_state = ok ? JOB_DONE : JOB_FAILED;
  }
}

bool start(int kind) {
  if (g_state == JOB_BUSY) return false;
  if (!g_thr) {
    LightEvent_Init(&g_ev, RESET_ONESHOT);
    s32 prio = 0x30;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    g_thr = threadCreate(worker, nullptr, 256 * 1024, prio + 1, -2, false);
    if (!g_thr) return false;
  }
  g_kind = kind;
  g_err.clear();
  g_state = JOB_BUSY;
  LightEvent_Signal(&g_ev);
  return true;
}
}  // namespace

void init(const char* dir) {
  g_dir = dir;
  g_cfg = cloud::Config();
  g_cfg.api_key = kWebApiKey;
  g_cfg.project = kWebProject;
  // Pruebas: nube.json en la SD apunta a otro Firebase (el emulador del VPS).
  std::string t;
  Json o;
  if (read_file(path("nube.json"), t) && o.parse(t) && o.is_obj()) {
    if (o["auth"].is_str()) { g_cfg.auth_base = o["auth"].as_str() + "/identitytoolkit.googleapis.com"; g_cfg.token_base = o["auth"].as_str() + "/securetoken.googleapis.com"; }
    if (o["fs"].is_str()) g_cfg.fs_base = o["fs"].as_str();
    if (o["project"].is_str()) g_cfg.project = o["project"].as_str();
    if (o["key"].is_str()) g_cfg.api_key = o["key"].as_str();
  }
  g_acc = Account();
  if (read_file(path("cuenta.json"), t) && o.parse(t) && o.is_obj()) {
    g_acc.name = o["name"].as_str(); g_acc.uid = o["uid"].as_str(); g_acc.refresh = o["refresh"].as_str();
    g_acc.dev = o["dev"].as_str();
    g_acc.delta = int64_t(o["delta"].as_num(0)); g_acc.last_sync = int64_t(o["lastSync"].as_num(0));
    g_acc.on = !g_acc.refresh.empty();
  }
  if (g_acc.dev.empty()) {
    // Nombre de esta consola en la libreta: "ctr" + 6 letras/cifras al azar.
    static const char A[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    uint64_t x = svcGetSystemTick() ^ (uint64_t(osGetTime()) << 17);
    g_acc.dev = "ctr";
    for (int i = 0; i < 6; ++i) { x = x * 6364136223846793005ull + 1442695040888963407ull; g_acc.dev += A[(x >> 33) % 36]; }
    save_account();
  }
}

const Account& account() { return g_acc; }

void logout() {
  g_acc.on = false;
  g_acc.uid.clear(); g_acc.refresh.clear();
  save_account();
}

bool start_login(const std::string& name, const std::string& pass, const Json& local) {
  if (g_state == JOB_BUSY) return false;
  g_name = name; g_pass = pass; g_local = local;
  return start(1);
}

bool start_sync(const Json& local) {
  if (g_state == JOB_BUSY || !g_acc.on) return false;
  g_local = local;
  return start(2);
}

JobState state() { return JobState(g_state); }
const Json& result() { return g_result; }
const std::string& error() { return g_err; }
void finish() { if (g_state != JOB_BUSY) { g_state = JOB_IDLE; g_result = Json(); g_local = Json(); } }

int probe_https(std::string& curl_error) {
  if (!net_up()) { curl_error = "socInit"; return 0; }
  cloud::HttpReq rq;
  rq.method = "POST";
  rq.url = "https://securetoken.googleapis.com/v1/token?key=" + g_cfg.api_key;
  rq.headers.push_back("Content-Type: application/x-www-form-urlencoded");
  rq.body = "grant_type=refresh_token&refresh_token=prueba-de-la-3ds";
  cloud::HttpResp rs;
  g_curl_err.clear();
  http(rq, rs, nullptr);
  curl_error = g_curl_err;
  return rs.status;
}

void shutdown() {
  if (g_thr) {
    g_quit = true;
    LightEvent_Signal(&g_ev);
    threadJoin(g_thr, 3000000000ull);
    threadFree(g_thr);
    g_thr = nullptr;
  }
  if (g_soc) { curl_global_cleanup(); socExit(); std::free(g_socbuf); g_soc = false; }
}

}  // namespace cloud3ds
}  // namespace ps

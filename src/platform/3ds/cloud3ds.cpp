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
int g_kind = 0;                  // 1 = entrar + sincronizar, 2 = sincronizar, 3 = entrar con Google + sincronizar
// Entrar con Google: el código que se enseña (lo escribe el hilo) y cancelar.
LightLock g_glock;
std::string g_gcode, g_gurl;
volatile bool g_gcancel = false;
volatile bool g_gqr = false;     // el código es un QR (servicio «vincular» del servidor del proyecto)
// El servidor del proyecto (servicio «vincular»: la página del QR). Pruebas:
// "vincular" en nube.json lo cambia.
std::string g_vinc = "https://pokesurvivor3ds.duckdns.org";
// Cliente OAuth «TV y dispositivos de entrada limitada» del proyecto de la web
// (lo crea quien administra pokesurvivor-free en Google Cloud). Vacío: el botón
// lo explica. Pruebas: google.json en la SD ({"client_id":..,"client_secret":..,
// "device":..,"token":..}) lo cambia.
const char kGoogleClientId[] = "";
const char kGoogleClientSecret[] = "";
std::string g_name, g_pass, g_err, g_curl_err;
Json g_local, g_result, g_remote;

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

LightLock g_net_lock;
bool g_net_lock_init = false;
bool net_up() {
  if (!g_net_lock_init) { LightLock_Init(&g_net_lock); g_net_lock_init = true; }   // (lo hace init, en el hilo principal)
  LightLock_Lock(&g_net_lock);
  const bool ok = g_soc || (soc_up() && (curl_global_init(CURL_GLOBAL_DEFAULT), g_soc = true));
  LightLock_Unlock(&g_net_lock);
  return ok;
}
bool g_socinit = false;
bool soc_up_impl() {
  if (g_socinit) return true;
  g_socbuf = static_cast<u32*>(memalign(0x1000, 0x100000));
  if (!g_socbuf || R_FAILED(socInit(g_socbuf, 0x100000))) { std::free(g_socbuf); g_socbuf = nullptr; return false; }
  g_socinit = true;
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
      if (g_kind == 3 && g_cfg.google_client_id.empty()) {
        // Entrar con Google por QR: la consola pide un código al servidor del
        // proyecto y lo enseña como QR; la página explica cómo mandar la sesión
        // de Google desde pokesurvivor.com y la consola la recoge aquí.
        in = false;
        cloud::HttpReq rq;
        rq.method = "POST";
        rq.url = g_vinc + "/vincular/api/nuevo";
        cloud::HttpResp rs;
        Json n;
        if (!http(rq, rs, nullptr)) c.error = "No se encuentra el servidor de PokéSurvivor 3DS.";
        else if (rs.status == 429) c.error = "Demasiados códigos seguidos: espera unos minutos.";
        else if (rs.status != 200 || !n.parse(rs.body) || !n["code"].is_str() || !n["url"].is_str()) c.error = "El servidor no ha dado un código.";
        else {
          g_gqr = true;
          LightLock_Lock(&g_glock); g_gcode = n["code"].as_str(); g_gurl = n["url"].as_str(); LightLock_Unlock(&g_glock);
          const std::string ask = g_vinc + "/vincular/api/espera?code=" + n["code"].as_str() + "&secret=" + n["secret"].as_str();
          const u64 end = osGetTime() + u64(n["ttl"].as_num(900)) * 1000;
          bool gone = false;
          while (!g_gcancel && !g_quit && !in && !gone && osGetTime() < end) {
            for (int k = 0; k < 20 && !g_gcancel && !g_quit; ++k) svcSleepThread(100000000ll);   // (cada 2 s)
            if (g_gcancel || g_quit) break;
            cloud::HttpReq q;
            q.url = ask;
            cloud::HttpResp r;
            Json j;
            if (!http(q, r, nullptr)) continue;   // (un corte de wifi: se sigue esperando)
            if (r.status == 200 && j.parse(r.body) && j["refresh"].is_str()) {
              if (!cloud::session_from_refresh(c, j["refresh"].as_str(), j["nombre"].as_str(), s)) gone = true;
              else in = true;
            } else if (r.status == 404) {
              gone = true;
              c.error = "El código ha caducado: vuelve a intentarlo.";
            }
          }
          if (!in && c.error.empty()) c.error = g_gcancel ? "Cancelado." : "El código ha caducado: vuelve a intentarlo.";
          LightLock_Lock(&g_glock); g_gcode.clear(); LightLock_Unlock(&g_glock);
        }
      } else if (g_kind == 3) {
        // Entrar con Google: código en la consola, se acepta en el móvil.
        cloud::GoogleDevice d;
        in = cloud::google_start(c, d);
        if (in) {
          LightLock_Lock(&g_glock); g_gcode = d.user_code; g_gurl = d.verification_url; LightLock_Unlock(&g_glock);
          int r = 0;
          const u64 end = osGetTime() + u64(d.expires_in) * 1000;
          while (!g_gcancel && !g_quit && osGetTime() < end) {
            for (int k = 0; k < d.interval * 10 && !g_gcancel && !g_quit; ++k) svcSleepThread(100000000ll);   // (0,1 s)
            if (g_gcancel || g_quit) break;
            if ((r = cloud::google_poll(c, d, s)) != 0) break;
          }
          if (r != 1) { in = false; if (g_gcancel) c.error = "Cancelado."; else if (r == 0) c.error = "El código ha caducado: vuelve a intentarlo."; }
          LightLock_Lock(&g_glock); g_gcode.clear(); LightLock_Unlock(&g_glock);
        }
      }
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
        g_remote = c.last_remote;
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
  if (!g_net_lock_init) { LightLock_Init(&g_net_lock); g_net_lock_init = true; }
  g_dir = dir;
  g_cfg = cloud::Config();
  g_cfg.api_key = kWebApiKey;
  g_cfg.project = kWebProject;
  g_cfg.google_client_id = kGoogleClientId;
  g_cfg.google_client_secret = kGoogleClientSecret;
  LightLock_Init(&g_glock);
  // Pruebas: nube.json en la SD apunta a otro Firebase (el emulador del VPS).
  std::string t;
  Json o;
  if (read_file(path("nube.json"), t) && o.parse(t) && o.is_obj()) {
    if (o["auth"].is_str()) { g_cfg.auth_base = o["auth"].as_str() + "/identitytoolkit.googleapis.com"; g_cfg.token_base = o["auth"].as_str() + "/securetoken.googleapis.com"; }
    if (o["fs"].is_str()) g_cfg.fs_base = o["fs"].as_str();
    if (o["project"].is_str()) g_cfg.project = o["project"].as_str();
    if (o["key"].is_str()) g_cfg.api_key = o["key"].as_str();
    if (o["vincular"].is_str()) g_vinc = o["vincular"].as_str();
  }
  if (read_file(path("google.json"), t) && o.parse(t) && o.is_obj()) {
    if (o["client_id"].is_str()) g_cfg.google_client_id = o["client_id"].as_str();
    if (o["client_secret"].is_str()) g_cfg.google_client_secret = o["client_secret"].as_str();
    if (o["device"].is_str()) g_cfg.google_device_url = o["device"].as_str();
    if (o["token"].is_str()) g_cfg.google_token_url = o["token"].as_str();
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

bool start_google(const Json& local) {
  if (g_state == JOB_BUSY) return false;
  g_local = local;
  g_gcancel = false;
  g_gqr = false;
  return start(3);
}
void cancel_google() { g_gcancel = true; }
bool google_qr() { return g_gqr; }
bool google_code(std::string& code, std::string& url) {
  LightLock_Lock(&g_glock);
  code = g_gcode; url = g_gurl;
  LightLock_Unlock(&g_glock);
  return !code.empty();
}

bool start_sync(const Json& local) {
  if (g_state == JOB_BUSY || !g_acc.on) return false;
  g_local = local;
  return start(2);
}

JobState state() { return JobState(g_state); }
const Json& result() { return g_result; }
const Json& last_remote() { return g_remote; }
const std::string& error() { return g_err; }
void finish() { if (g_state != JOB_BUSY) { g_state = JOB_IDLE; g_result = Json(); g_local = Json(); } }

bool soc_up() { return soc_up_impl(); }

bool http_get(const std::string& url, std::string& body, int& status, std::string& err) {
  if (!net_up()) { status = 0; err = "No hay conexión a internet."; return false; }
  cloud::HttpReq rq;
  rq.method = "GET";
  rq.url = url;
  cloud::HttpResp rs;
  const bool ok = http(rq, rs, nullptr);
  status = rs.status;
  body.swap(rs.body);
  if (!ok) err = "Sin conexión con el servidor.";
  return ok && status == 200;
}

int probe_https(std::string& curl_error) {
  if (!net_up()) { curl_error = "socInit"; return 0; }
  cloud::HttpReq rq;
  rq.method = "POST";
  rq.url = "https://securetoken.googleapis.com/v1/token?key=" + g_cfg.api_key;
  rq.headers.push_back("Content-Type: application/x-www-form-urlencoded");
  rq.headers.push_back("Referer: " + g_cfg.referer);
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
  if (g_soc) { curl_global_cleanup(); g_soc = false; }
  if (g_socinit) { socExit(); std::free(g_socbuf); g_socbuf = nullptr; g_socinit = false; }
}

}  // namespace cloud3ds
}  // namespace ps

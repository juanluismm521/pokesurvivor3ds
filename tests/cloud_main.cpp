// cloud_main.cpp — ps_cloud: la sincronización de la 3DS (core/cloudsync)
// desde el PC, con curl. Para probarla contra el emulador de Firebase y la
// web de verdad (tools/cloud_e2e.js).
//
//   ps_cloud <usuario> <contraseña> <partida.json> <aparato> [salida.json]
//   ps_cloud --google - <partida.json> <aparato> [salida.json]   (entrar con Google:
//            PS_GOOGLE_DEVICE, PS_GOOGLE_TOKEN = direcciones de Google; PS_GOOGLE_ID)
//   ps_cloud --qr - <partida.json> <aparato> [salida.json]       (entrar con Google por QR:
//            PS_VINCULAR = servidor «vincular», p. ej. http://127.0.0.1:8000)
//   variables: PS_AUTH (http://127.0.0.1:9099), PS_API (https://ice.pokesurvivor.com/v1/d),
//              PS_PROJECT (demo-pokesurvivor), PS_KEY (clave), PS_NOW (hora en ms)
//              PS_SECO=archivo: «en seco», la escritura (PUT /save) NO se manda: se deja
//              en ese archivo y se contesta 200 (para probar cuentas reales sin tocarlas)
#include <curl/curl.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <string>

#include "core/cloudsync.h"

using namespace ps;

namespace {
size_t on_body(char* p, size_t sz, size_t n, void* ud) { static_cast<std::string*>(ud)->append(p, sz * n); return sz * n; }
size_t on_header(char* p, size_t sz, size_t n, void* ud) {
  std::string h(p, sz * n);
  if (h.size() > 5 && (h.compare(0, 5, "Date:") == 0 || h.compare(0, 5, "date:") == 0)) {
    const time_t t = curl_getdate(h.substr(5).c_str(), nullptr);
    if (t > 0) static_cast<cloud::HttpResp*>(ud)->date_ms = int64_t(t) * 1000;
  }
  return sz * n;
}
bool http(const cloud::HttpReq& rq, cloud::HttpResp& rs, void*) {
  if (const char* seco = std::getenv("PS_SECO"); seco && *seco && rq.method == "PUT" && rq.url.size() > 5 && rq.url.compare(rq.url.size() - 5, 5, "/save") == 0) {
    if (FILE* f = std::fopen(seco, "wb")) { std::fwrite(rq.body.data(), 1, rq.body.size(), f); std::fclose(f); }
    std::fprintf(stderr, "(en seco: no se escribe; la escritura, en %s)\n", seco);
    rs.status = 200; rs.body = "{\"rev\":0}";
    return true;
  }
  CURL* c = curl_easy_init();
  if (!c) return false;
  curl_slist* hs = nullptr;
  for (const auto& h : rq.headers) hs = curl_slist_append(hs, h.c_str());
  curl_easy_setopt(c, CURLOPT_URL, rq.url.c_str());
  curl_easy_setopt(c, CURLOPT_CUSTOMREQUEST, rq.method.c_str());
  if (rq.method == "POST" || rq.method == "PUT") curl_easy_setopt(c, CURLOPT_POSTFIELDS, rq.body.c_str());
  curl_easy_setopt(c, CURLOPT_HTTPHEADER, hs);
  curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, on_body);
  curl_easy_setopt(c, CURLOPT_WRITEDATA, &rs.body);
  curl_easy_setopt(c, CURLOPT_HEADERFUNCTION, on_header);
  curl_easy_setopt(c, CURLOPT_HEADERDATA, &rs);
  curl_easy_setopt(c, CURLOPT_TIMEOUT, 20L);
  const CURLcode r = curl_easy_perform(c);
  long st = 0;
  curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &st);
  rs.status = r == CURLE_OK ? int(st) : 0;
  if (const char* d = std::getenv("PS_DUMP")) if (FILE* f = std::fopen(d, "ab")) { std::fprintf(f, "%s %s\n%s\n<< %d %s\n", rq.method.c_str(), rq.url.c_str(), rq.url.find("/v1/d/") != std::string::npos ? rq.body.c_str() : "(…)", rs.status, rs.body.c_str()); std::fclose(f); }
  if (std::getenv("PS_VERBOSE")) std::fprintf(stderr, "%s %s → %d %s %s\n", rq.method.c_str(), rq.url.c_str(), rs.status, r == CURLE_OK ? "" : curl_easy_strerror(r), rs.body.substr(0, 200).c_str());
  curl_slist_free_all(hs);
  curl_easy_cleanup(c);
  return r == CURLE_OK;
}
const char* env(const char* k, const char* def) { const char* v = std::getenv(k); return v && *v ? v : def; }
}  // namespace

int main(int argc, char** argv) {
  if (argc < 5) { std::fprintf(stderr, "uso: ps_cloud usuario contraseña partida.json aparato [salida.json]\n"); return 2; }
  curl_global_init(CURL_GLOBAL_DEFAULT);
  cloud::Client c;
  const std::string auth = env("PS_AUTH", "http://127.0.0.1:9099");
  c.cfg.auth_base = auth + "/identitytoolkit.googleapis.com";
  c.cfg.token_base = auth + "/securetoken.googleapis.com";
  c.cfg.api_base = env("PS_API", "https://ice.pokesurvivor.com/v1/d");
  c.cfg.project = env("PS_PROJECT", "demo-pokesurvivor");
  c.cfg.api_key = env("PS_KEY", "fake-api-key");
  c.http = http;
  c.sleep_ms = [](int ms, void*) { std::fprintf(stderr, "(se acaba de guardar: espera %.1f s)\n", ms / 1000.0); usleep(useconds_t(ms) * 1000); };
  std::string text;
  if (FILE* f = std::fopen(argv[3], "rb")) { char b[8192]; size_t n; while ((n = std::fread(b, 1, sizeof(b), f)) > 0) text.append(b, n); std::fclose(f); }
  Json local;
  if (!local.parse(text)) { std::fprintf(stderr, "partida no válida\n"); return 2; }
  cloud::Session s;
  if (std::string(argv[1]) == "--qr") {
    // Como la consola: pide un código, enseña la página y espera la sesión.
    const std::string base = env("PS_VINCULAR", "https://pokesurvivor3ds.duckdns.org");
    cloud::HttpReq rq; rq.method = "POST"; rq.url = base + "/vincular/api/nuevo";
    cloud::HttpResp rs;
    Json n;
    if (!http(rq, rs, nullptr) || rs.status != 200 || !n.parse(rs.body)) { std::fprintf(stderr, "qr: el servidor no contesta\n"); return 1; }
    std::printf("QR: %s\n", n["url"].as_str().c_str());
    std::fflush(stdout);
    bool got = false;
    for (int t = 0; t < 300 && !got; ++t) {
      cloud::HttpReq q; q.url = base + "/vincular/api/espera?code=" + n["code"].as_str() + "&secret=" + n["secret"].as_str();
      cloud::HttpResp r;
      Json j;
      if (http(q, r, nullptr) && r.status == 200 && j.parse(r.body)) {
        if (!cloud::session_from_refresh(c, j["refresh"].as_str(), j["nombre"].as_str(), s)) { std::fprintf(stderr, "qr: %s\n", c.error.c_str()); return 1; }
        got = true;
      } else if (r.status == 404) { std::fprintf(stderr, "qr: el código ha caducado\n"); return 1; }
      else usleep(500000);
    }
    if (!got) { std::fprintf(stderr, "qr: nadie ha entregado la sesión\n"); return 1; }
    std::printf("qr: entra como %s\n", s.name.c_str());
  } else if (std::string(argv[1]) == "--google") {
    c.cfg.google_client_id = env("PS_GOOGLE_ID", "cliente-tv");
    c.cfg.google_client_secret = env("PS_GOOGLE_SECRET", "secreto");
    c.cfg.google_device_url = env("PS_GOOGLE_DEVICE", c.cfg.google_device_url.c_str());
    c.cfg.google_token_url = env("PS_GOOGLE_TOKEN", c.cfg.google_token_url.c_str());
    cloud::GoogleDevice d;
    if (!cloud::google_start(c, d)) { std::fprintf(stderr, "google: %s\n", c.error.c_str()); return 1; }
    std::printf("entra en %s y escribe %s\n", d.verification_url.c_str(), d.user_code.c_str());
    std::fflush(stdout);
    int r = 0;
    for (int t = 0; t < d.expires_in && (r = cloud::google_poll(c, d, s)) == 0; t += d.interval) sleep(unsigned(d.interval));
    if (r != 1) { std::fprintf(stderr, "google: %s\n", c.error.c_str()); return 1; }
    std::printf("google: entra como %s\n", s.name.c_str());
  } else if (!cloud::login(c, argv[1], argv[2], s)) { std::fprintf(stderr, "login: %s\n", c.error.c_str()); return 1; }
  const int64_t now = std::atoll(env("PS_NOW", "0")) ? std::atoll(env("PS_NOW", "0")) : int64_t(std::time(nullptr)) * 1000;
  Json merged;
  // PS_RUNQ=fichero: la cola de reclamos de partidas (se cobra y se reescribe con lo que queda)
  Json runq = Json::array();
  const std::string qpath = env("PS_RUNQ", "");
  if (!qpath.empty()) { std::string t; if (FILE* f = std::fopen(qpath.c_str(), "rb")) { char b[8192]; size_t n; while ((n = std::fread(b, 1, sizeof(b), f)) > 0) t.append(b, n); std::fclose(f); } runq.parse(t); if (!runq.is_arr()) runq = Json::array(); }
  // PS_PULL="n,gen,t10|t1": tirar en el gacha del servidor
  const std::string pl = env("PS_PULL", "");
  if (!pl.empty()) {
    cloud::PullReq rq;
    char use[8] = "t1";
    std::sscanf(pl.c_str(), "%d,%d,%7s", &rq.n, &rq.gen, use);
    rq.t10 = std::string(use) == "t10";
    rq.op = "p" + std::to_string(now) + "ctr";
    Json results; std::string code;
    if (!cloud::pull(c, s, local, argv[4], now, rq, merged, results, code, qpath.empty() ? nullptr : &runq)) { std::fprintf(stderr, "pull: %s (%s)\n", c.error.c_str(), code.c_str()); return 1; }
    std::printf("tirada: %s\n", results.dump().c_str());
  } else if (!cloud::sync(c, s, local, argv[4], now, merged, qpath.empty() ? nullptr : &runq)) { std::fprintf(stderr, "sync: %s\n", c.error.c_str()); return 1; }
  if (!qpath.empty()) { const std::string q = runq.dump(); if (FILE* f = std::fopen(qpath.c_str(), "wb")) { std::fwrite(q.data(), 1, q.size(), f); std::fclose(f); } std::printf("cola: quedan %d\n", int(runq.size())); }
  const std::string out = merged.dump();
  if (argc > 5) { FILE* f = std::fopen(argv[5], "wb"); if (f) { std::fwrite(out.data(), 1, out.size(), f); std::fclose(f); } }
  std::printf("ok uid=%s monedas=%g reloj=%lld\n", s.uid.c_str(), merged["coins"].as_num(), (long long)c.clock_delta);
  { const std::string ap = cloud::profile_name(c, s); if (!ap.empty()) std::printf("apodo: %s\n", ap.c_str()); }   // (el nombre que pone la consola)
  curl_global_cleanup();
  return 0;
}

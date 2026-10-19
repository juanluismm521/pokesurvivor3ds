// cloud_main.cpp — ps_cloud: la sincronización de la 3DS (core/cloudsync)
// desde el PC, con curl. Para probarla contra el emulador de Firebase y la
// web de verdad (tools/cloud_e2e.js).
//
//   ps_cloud <usuario> <contraseña> <partida.json> <aparato> [salida.json]
//   variables: PS_AUTH (http://127.0.0.1:9099), PS_FS (http://127.0.0.1:8089),
//              PS_PROJECT (demo-pokesurvivor), PS_KEY (clave), PS_NOW (hora en ms)
#include <curl/curl.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>
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
  CURL* c = curl_easy_init();
  if (!c) return false;
  curl_slist* hs = nullptr;
  for (const auto& h : rq.headers) hs = curl_slist_append(hs, h.c_str());
  curl_easy_setopt(c, CURLOPT_URL, rq.url.c_str());
  curl_easy_setopt(c, CURLOPT_CUSTOMREQUEST, rq.method.c_str());
  if (rq.method == "POST") curl_easy_setopt(c, CURLOPT_POSTFIELDS, rq.body.c_str());
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
  c.cfg.fs_base = env("PS_FS", "http://127.0.0.1:8089");
  c.cfg.project = env("PS_PROJECT", "demo-pokesurvivor");
  c.cfg.api_key = env("PS_KEY", "fake-api-key");
  c.http = http;
  std::string text;
  if (FILE* f = std::fopen(argv[3], "rb")) { char b[8192]; size_t n; while ((n = std::fread(b, 1, sizeof(b), f)) > 0) text.append(b, n); std::fclose(f); }
  Json local;
  if (!local.parse(text)) { std::fprintf(stderr, "partida no válida\n"); return 2; }
  cloud::Session s;
  if (!cloud::login(c, argv[1], argv[2], s)) { std::fprintf(stderr, "login: %s\n", c.error.c_str()); return 1; }
  const int64_t now = std::atoll(env("PS_NOW", "0")) ? std::atoll(env("PS_NOW", "0")) : int64_t(std::time(nullptr)) * 1000;
  Json merged;
  if (!cloud::sync(c, s, local, argv[4], now, merged)) { std::fprintf(stderr, "sync: %s\n", c.error.c_str()); return 1; }
  const std::string out = merged.dump();
  if (argc > 5) { FILE* f = std::fopen(argv[5], "wb"); if (f) { std::fwrite(out.data(), 1, out.size(), f); std::fclose(f); } }
  std::printf("ok uid=%s monedas=%g reloj=%lld\n", s.uid.c_str(), merged["coins"].as_num(), (long long)c.clock_delta);
  curl_global_cleanup();
  return 0;
}

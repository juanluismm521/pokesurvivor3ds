// dns_main.cpp — herramienta: ¿llega el juego al servidor del proyecto si el DNS de
// casa no lo encuentra (o lo manda a 0.0.0.0)? Usa el mismo código que la consola:
// las descargas con curl y src/core/dns_fallback.h (HTTPS de verdad) y el juego online
// con relay::Client. Pensado para correr con un DNS de mentira (ver PROGRESS, 31-10-2026):
//   unshare -m sh -c 'mount --bind resolv.falso /etc/resolv.conf; build-pc/ps_dns'
#include <cstdio>
#include <string>

#include "core/dns_fallback.h"
#include "core/relay_client.h"

using namespace ps;

static size_t body(char* p, size_t sz, size_t n, void* ud) { static_cast<std::string*>(ud)->append(p, sz * n); return sz * n; }

int main() {
  curl_global_init(CURL_GLOBAL_DEFAULT);
  int fails = 0;
  // 1) Descarga por HTTPS (como prog3ds: programa/ultimo.json)
  const std::string url = std::string("https://") + relay::kProjectHost + "/datos/programa/ultimo.json";
  CURL* c = curl_easy_init();
  std::string b;
  curl_easy_setopt(c, CURLOPT_URL, url.c_str());
  curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, body);
  curl_easy_setopt(c, CURLOPT_WRITEDATA, &b);
  curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(c, CURLOPT_TIMEOUT, 25L);
  CURLcode r = curl_easy_perform(c);
  std::printf("descarga, primer intento: %s\n", curl_easy_strerror(r));
  curl_slist* res = nullptr;
  if (dnsfb::retry_by_address(c, r, url) && (res = dnsfb::dns_fallback(c, url))) {
    b.clear();
    r = curl_easy_perform(c);
    std::printf("descarga, por la dirección de reserva: %s\n", curl_easy_strerror(r));
  }
  long st = 0;
  curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &st);
  const bool ok_http = r == CURLE_OK && st == 200 && b.find("\"actual\"") != std::string::npos;
  std::printf("descarga: %s (HTTP %ld, %zu bytes, certificado comprobado)\n", ok_http ? "BIEN" : "MAL", st, b.size());
  if (!ok_http) fails++;
  curl_slist_free_all(res);
  curl_easy_cleanup(c);
  // 2) Juego online (relay::Client, como net3ds)
  relay::Client rc;
  const bool open = rc.open(relay::kProjectHost, relay::kDefaultPort);
  relay::RoomInfo rooms[8];
  const int n = open ? rc.list(rooms, 8) : -1;
  const bool ok_relay = open && rc.error.empty();
  std::printf("online: %s (%s%s)\n", ok_relay ? "BIEN" : "MAL", open ? "conectado, salas: " : rc.error.c_str(), open ? std::to_string(n).c_str() : "");
  if (!ok_relay) fails++;
  return fails;
}

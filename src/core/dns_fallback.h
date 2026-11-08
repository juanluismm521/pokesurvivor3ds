// dns_fallback.h — descargas por curl cuando el DNS de casa no encuentra el servidor
// del proyecto (31-10-2026: un usuario de Reddit; hay routers, operadores y
// bloqueadores que no resuelven *.duckdns.org o lo mandan a 0.0.0.0). Se reintenta
// diciéndole a curl dónde está ese nombre (CURLOPT_RESOLVE con relay::fallback_address):
// el HTTPS sigue siendo con el nombre, certificado incluido. Lo usan cloud3ds (consola)
// y tests/dns_main.cpp (ps_dns, para probarlo en PC con un DNS de mentira).
#pragma once
#include <curl/curl.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "core/relay_client.h"

namespace ps {
namespace dnsfb {

inline std::string url_host(const std::string& url, int& port) {
  size_t a = url.find("://");
  if (a == std::string::npos) return "";
  port = url.compare(0, 5, "https") == 0 ? 443 : 80;
  a += 3;
  size_t b = url.find_first_of(":/", a);
  std::string h = url.substr(a, b == std::string::npos ? std::string::npos : b - a);
  if (b != std::string::npos && url[b] == ':') port = std::atoi(url.c_str() + b + 1);
  return h;
}
// La dirección por la que reintentar, distinta de la que acaba de fallar (`tried`):
// la última buena o, si es ésa, la del servidor del proyecto que trae el juego.
// Direcciones de reserva de otros servidores (08-11-2026): el de la partida de
// pokesurvivor.com (Cloudflare; si cambian, sólo se usan cuando falla el DNS).
struct Known { const char* host; const char* ip[2]; };
constexpr Known kKnown[] = {
    {"ice.pokesurvivor.com", {"188.114.96.5", "188.114.97.5"}},
};
inline bool pick_address(const std::string& host, const char* tried, char* ip, int cap) {
  if (relay::fallback_address(host.c_str(), ip, cap) && !(tried && !std::strcmp(tried, ip))) return true;
  if (host == relay::kProjectHost && !(tried && !std::strcmp(tried, relay::kProjectIp))) {
    std::snprintf(ip, size_t(cap), "%s", relay::kProjectIp);
    return true;
  }
  for (const Known& k : kKnown) {
    if (host != k.host) continue;
    for (const char* a : k.ip) if (!(tried && !std::strcmp(tried, a))) { std::snprintf(ip, size_t(cap), "%s", a); return true; }
  }
  return false;
}
inline const char* tried_ip(CURL* c) {
  char* ip = nullptr;
  return curl_easy_getinfo(c, CURLINFO_PRIMARY_IP, &ip) == CURLE_OK && ip && *ip ? ip : nullptr;
}
// ¿Merece la pena probar por la dirección de reserva? Si el DNS no lo encuentra, o si
// no se ha podido conectar (o se agota el tiempo sin recibir nada) en la dirección que
// dio: 0.0.0.0 de un bloqueador, la de un operador que redirige…
inline bool retry_by_address(CURL* c, CURLcode r, const std::string& url) {
  if (r != CURLE_COULDNT_RESOLVE_HOST && r != CURLE_COULDNT_CONNECT && r != CURLE_OPERATION_TIMEDOUT) return false;
  int port = 0;
  const std::string host = url_host(url, port);
  char ip[48];
  return !host.empty() && pick_address(host, r == CURLE_COULDNT_RESOLVE_HOST ? nullptr : tried_ip(c), ip, sizeof(ip));
}
inline curl_slist* dns_fallback(CURL* c, const std::string& url) {
  int port = 0;
  const std::string host = url_host(url, port);
  char ip[48];
  if (host.empty() || !pick_address(host, tried_ip(c), ip, sizeof(ip))) return nullptr;
  curl_slist* r = curl_slist_append(nullptr, (host + ":" + std::to_string(port) + ":" + ip).c_str());
  curl_easy_setopt(c, CURLOPT_RESOLVE, r);
  return r;
}
// Cuando el DNS sí va, se apunta la dirección buena (la de reserva será ésa).
inline void remember_ip(CURL* c, const std::string& url) {
  int port = 0;
  const std::string host = url_host(url, port);
  char* ip = nullptr;
  if (!host.empty() && curl_easy_getinfo(c, CURLINFO_PRIMARY_IP, &ip) == CURLE_OK && ip && std::strchr(ip, '.')) relay::remember_address(host.c_str(), ip);
}


}  // namespace dnsfb
}  // namespace ps

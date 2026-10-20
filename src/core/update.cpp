// update.cpp — ver update.h.
#include "core/update.h"

#include <cstdio>
#include <cstring>

#include "core/json.h"
#include "core/monocypher.h"

namespace ps {

// La pública de /root/.config/pokesurvivor/firma.key (tools/cache/firmar publica).
const uint8_t kUpdateKey[32] = {
    0x46, 0x60, 0xbc, 0x94, 0x06, 0x5e, 0xeb, 0x37, 0x70, 0x81, 0x6b, 0xbf, 0x46, 0xc2, 0xaa, 0x17,
    0x33, 0xbd, 0x76, 0xb4, 0xf2, 0x2f, 0xf7, 0xb3, 0xb9, 0xf6, 0x65, 0x2e, 0x7e, 0x52, 0xa4, 0x34};

static_assert(sizeof(crypto_blake2b_ctx) <= sizeof(UpdateHasher::ctx), "crypto_blake2b_ctx");

namespace {
std::string hex(const uint8_t* b, size_t n) {
  static const char H[] = "0123456789abcdef";
  std::string s;
  for (size_t i = 0; i < n; ++i) { s += H[b[i] >> 4]; s += H[b[i] & 15]; }
  return s;
}
bool unhex(const std::string& s, uint8_t* out, size_t n) {
  if (s.size() != n * 2) return false;
  for (size_t i = 0; i < n; ++i) {
    int v = 0;
    for (int k = 0; k < 2; ++k) {
      const char c = s[i * 2 + k];
      const int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
      if (d < 0) return false;
      v = v * 16 + d;
    }
    out[i] = uint8_t(v);
  }
  return true;
}
bool read_file_entry(const Json& j, UpdateFile& f) {
  if (!j.is_obj() || !j["url"].is_str() || !j["tam"].is_num()) return false;
  f.url = j["url"].as_str();
  // (sólo un nombre de archivo relativo, sin subir de carpeta)
  if (f.url.empty() || f.url.find("..") != std::string::npos || f.url[0] == '/' || f.url.find(':') != std::string::npos) return false;
  f.size = uint64_t(j["tam"].as_num());
  return f.size > 0 && f.size < (200u << 20) && unhex(j["huella"].as_str(), f.hash, 32);
}
bool read_entry(const Json& j, const uint8_t key[32], UpdateEntry& e, std::string& why) {
  e = UpdateEntry();
  if (!j.is_obj()) { why = "no hay versión"; return false; }
  e.serie = int(j["serie"].as_num(0));
  e.build = int(j["build"].as_num(0));
  e.version = j["version"].as_str();
  e.commit = j["commit"].as_str();
  e.fecha = j["fecha"].as_str();
  e.notas = j["notas"].as_str();
  if (e.serie <= 0 || e.build <= 0 || !read_file_entry(j["3dsx"], e.f3dsx) || !read_file_entry(j["cia"], e.cia)) { why = "versión mal escrita"; return false; }
  uint8_t sig[64];
  if (!unhex(j["firma"].as_str(), sig, 64)) { why = "sin firma"; return false; }
  const std::string m = update_message(e);
  if (crypto_eddsa_check(sig, key, reinterpret_cast<const uint8_t*>(m.data()), m.size()) != 0) { why = "la firma no vale"; return false; }
  e.ok = true;
  return true;
}
}  // namespace

std::string update_message(const UpdateEntry& e) {
  char b[160];
  std::snprintf(b, sizeof(b), "pokesurvivor3ds|%d|%d|%s|%s|", e.serie, e.build, e.version.c_str(), e.commit.c_str());
  std::string m = b;
  std::snprintf(b, sizeof(b), "%llu|", (unsigned long long)e.f3dsx.size);
  m += hex(e.f3dsx.hash, 32) + "|" + b;
  std::snprintf(b, sizeof(b), "%llu", (unsigned long long)e.cia.size);
  m += hex(e.cia.hash, 32) + "|" + b;
  return m;
}

bool update_parse(const std::string& json, const uint8_t key[32], UpdateEntry& actual, UpdateEntry& anterior, std::string& why) {
  Json j;
  if (json.size() > 65536 || !j.parse(json) || !j.is_obj()) { why = "no se puede leer"; return false; }
  if (!read_entry(j["actual"], key, actual, why)) return false;
  std::string w2;
  if (j["anterior"].is_obj()) read_entry(j["anterior"], key, anterior, w2);
  else anterior = UpdateEntry();
  return true;
}

bool update_wanted(const UpdateEntry& e, int my_build, int decided_serie) {
  return e.ok && e.build != my_build && e.serie > decided_serie;
}

UpdateHasher::UpdateHasher() { crypto_blake2b_init(reinterpret_cast<crypto_blake2b_ctx*>(ctx), 32); }
void UpdateHasher::add(const void* data, size_t n) { crypto_blake2b_update(reinterpret_cast<crypto_blake2b_ctx*>(ctx), static_cast<const uint8_t*>(data), n); }
void UpdateHasher::done(uint8_t out[32]) { crypto_blake2b_final(reinterpret_cast<crypto_blake2b_ctx*>(ctx), out); }

}  // namespace ps

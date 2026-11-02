// gift.cpp — ver gift.h.
#include "core/gift.h"

#include <cctype>
#include <cstdio>
#include <cstring>

#include "core/gamedata.h"
#include "core/json.h"
#include "core/monocypher.h"
#include "core/save.h"

namespace ps {

namespace {
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
int clampi(double v, int hi) { return v < 0 ? 0 : v > hi ? hi : int(v); }
bool same_name(const std::string& a, const char* b) {
  if (!b || !*b || a.empty() || a.size() != std::strlen(b)) return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
  return true;
}
}  // namespace

std::string gift_message(const Gift& g) {
  char b[256];
  std::snprintf(b, sizeof(b), "pokesurvivor3ds-regalo|%s|%s|%d|%d|%d|%d|%d|", g.id.c_str(), g.para.c_str(), g.coins, g.t1, g.t10, g.candy, g.medal);
  std::string m = b;
  for (int i = 0; i < g.nshiny; ++i) { std::snprintf(b, sizeof(b), "%s%d", i ? "," : "", g.shiny[i]); m += b; }
  return m;
}

int gift_parse(const std::string& json, const uint8_t key[32], Gift* out, int max) {
  Json j;
  if (json.size() > 65536 || !j.parse(json) || !j.is_obj() || !j["regalos"].is_arr()) return 0;
  int n = 0;
  const Json& list = j["regalos"];
  for (size_t i = 0; i < list.size() && n < max; ++i) {
    const Json& e = list[i];
    if (!e.is_obj() || !e["id"].is_str() || !e["para"].is_str()) continue;
    Gift g;
    g.id = e["id"].as_str();
    g.para = e["para"].as_str();
    g.nota = e["nota"].as_str();
    g.nota_en = e["nota_en"].as_str();
    g.msg = e["mensaje"].as_str();
    g.msg_en = e["mensaje_en"].as_str();   // (los textos no van firmados: sólo lo que se da)
    if (g.id.empty() || g.id.size() >= 32 || g.para.empty()) continue;
    g.coins = clampi(e["monedas"].as_num(0), 1000000);
    g.t1 = clampi(e["tickets"].as_num(0), 100000);
    g.t10 = clampi(e["tickets10"].as_num(0), 100000);
    g.candy = clampi(e["caramelos"].as_num(0), 100000);
    g.medal = clampi(e["medalla"].as_num(0), kMaxDex);
    if (e["shinies"].is_arr())
      for (size_t k = 0; k < e["shinies"].size() && g.nshiny < 8; ++k) {
        const int d = clampi(e["shinies"][k].as_num(0), kMaxDex);
        if (d >= 1) g.shiny[g.nshiny++] = d;
      }
    uint8_t sig[64];
    if (!unhex(e["firma"].as_str(), sig, 64)) continue;
    const std::string m = gift_message(g);
    if (crypto_eddsa_check(sig, key, reinterpret_cast<const uint8_t*>(m.data()), m.size()) != 0) continue;
    out[n++] = g;
  }
  return n;
}

bool gift_for(const Gift& g, const char* save_name, const char* account_name) {
  return g.para == "*" || same_name(g.para, save_name) || same_name(g.para, account_name);
}

bool gift_received(const Save& s, const std::string& id) {
  for (int i = 0; i < s.ngifts && i < Save::kMaxGifts; ++i) if (id == s.gift_id[i]) return true;
  return false;
}

bool gift_apply(Save& s, const Gift& g, int64_t now_ms) {
  if (gift_received(s, g.id) || g.id.empty() || g.id.size() >= 32) return false;
  const int64_t now = now_ms > 0 ? now_ms : 1;
  s.coins += g.coins;
  s.t1 += g.t1;
  s.t10 += g.t10;
  if (g.candy > 0) { s.candy += g.candy; s.candy_known = true; }
  if (g.medal >= 1 && g.medal <= kMaxDex && s.arceus_win[g.medal] <= 0) s.arceus_win[g.medal] = now;
  for (int i = 0; i < g.nshiny; ++i) save_grant_shiny_dex(s, g.shiny[i], FROM_OTHER, now);
  // (lleno: se olvida el más viejo; los regalos son pocos y caducan en el servidor)
  if (s.ngifts >= Save::kMaxGifts) {
    for (int i = 1; i < Save::kMaxGifts; ++i) { std::memcpy(s.gift_id[i - 1], s.gift_id[i], sizeof(s.gift_id[i])); s.gift_at[i - 1] = s.gift_at[i]; }
    s.ngifts = Save::kMaxGifts - 1;
  }
  std::snprintf(s.gift_id[s.ngifts], sizeof(s.gift_id[s.ngifts]), "%s", g.id.c_str());
  s.gift_at[s.ngifts] = now;
  s.ngifts++;
  return true;
}

}  // namespace ps

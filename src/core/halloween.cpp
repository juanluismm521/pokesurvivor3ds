// halloween.cpp — ver halloween.h.
#include "core/halloween.h"

#include <cmath>
#include <cstring>

#include "core/game.h"
#include "core/save.h"

namespace ps {

namespace {
// Días desde 1970-01-01 de una fecha civil, y al revés (H. Hinnant).
int64_t days_from_civil(int y, int m, int d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const int64_t yoe = y - era * 400;
  const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}
void civil_from_days(int64_t z, int& y, int& m, int& d) {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const int64_t doe = z - era * 146097;
  const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int64_t mp = (5 * doy + 2) / 153;
  d = int(doy - (153 * mp + 2) / 5 + 1);
  m = int(mp < 10 ? mp + 3 : mp - 9);
  y = int(yoe + era * 400 + (m <= 2));
}
constexpr int64_t kDay = 86400000;
// Último domingo de un mes (día del mes).
int last_sunday(int y, int m) {
  const int dim = m == 3 || m == 10 ? 31 : 30;
  const int64_t z = days_from_civil(y, m, dim);
  const int wd = int(((z % 7) + 11) % 7);   // 0 = domingo (1970-01-01 fue jueves)
  return dim - wd;
}
// Lo que va España por delante de UTC en ese instante (CET / CEST).
int64_t madrid_offset(int64_t utc_ms) {
  int y, m, d;
  civil_from_days(utc_ms >= 0 ? utc_ms / kDay : (utc_ms - kDay + 1) / kDay, y, m, d);
  const int64_t start = days_from_civil(y, 3, last_sunday(y, 3)) * kDay + 3600000;    // 01:00 UTC
  const int64_t end = days_from_civil(y, 10, last_sunday(y, 10)) * kDay + 3600000;
  return utc_ms >= start && utc_ms < end ? 2 * 3600000 : 3600000;
}
}  // namespace

bool halloween_date(int month, int day) { return (month == 10 && day >= 30) || (month == 11 && day <= 2); }

void madrid_date(int64_t utc_ms, int& year, int& month, int& day) {
  const int64_t local = utc_ms + madrid_offset(utc_ms);
  civil_from_days(local >= 0 ? local / kDay : (local - kDay + 1) / kDay, year, month, day);
}

bool halloween_at(int64_t utc_ms) {
  int y, m, d;
  madrid_date(utc_ms, y, m, d);
  return halloween_date(m, d);
}

int64_t madrid_midnight(int year, int month, int day) {
  const int64_t guess = days_from_civil(year, month, day) * kDay;
  return guess - madrid_offset(guess - 3600000);   // (con el horario que toque esa noche)
}

bool halloween_countdown(int64_t utc_ms, bool& on, int64_t& ms) {
  int y, m, d;
  madrid_date(utc_ms, y, m, d);
  constexpr int64_t kSoon = 14 * kDay;
  for (int yy : {y - 1, y}) {
    const int64_t start = madrid_midnight(yy, 10, 30), end = madrid_midnight(yy, 11, 3);
    if (utc_ms >= start && utc_ms < end) { on = true; ms = end - utc_ms; return true; }
    if (utc_ms < start && start - utc_ms <= kSoon) { on = false; ms = start - utc_ms; return true; }
  }
  return false;
}

bool candy_take(Game& g, float t, int need) {
  if (!g.hw) return false;
  if (int(std::floor(t / 60 * kCandyRate)) - g.candy_dropped < need) return false;
  g.candy_dropped++;
  return true;
}

int skin_index(const char* id) {
  for (int k = 0; k < kSkinCount; ++k) if (id && !std::strcmp(kSkins[k].id, id)) return k;
  return -1;
}

int skin_worn(const Save& s, int dex) {
  if (dex < 0 || dex > kMaxDex) return -1;
  const int k = s.pskin[dex];
  return k >= 0 && k < kSkinCount && kSkins[k].dex == dex && s.skin_at[k] > 0 ? k : -1;
}

SkinBuy skin_buy(Save& s, int k, bool event_on, int64_t now_ms) {
  if (k < 0 || k >= kSkinCount || !kSkins[k].sell) return SKIN_BAD;
  if (!event_on) return SKIN_NOT_EVENT;
  if (s.skin_at[k] > 0) return SKIN_OWNED;
  if (s.candy < kSkinPrice) return SKIN_NO_CANDY;
  s.candy -= kSkinPrice;
  s.candy_known = true;
  s.skin_at[k] = now_ms > 0 ? now_ms : 1;
  // Si ya tienes ese Pokémon, se la pone (como la web).
  if (s.owns(kSkins[k].dex)) skin_wear(s, kSkins[k].dex, k, now_ms);
  return SKIN_OK;
}

void skin_wear(Save& s, int dex, int k, int64_t now_ms) {
  if (dex < 0 || dex > kMaxDex) return;
  s.pskin[dex] = int8_t(k >= 0 && k < kSkinCount && kSkins[k].dex == dex ? k : -1);
  s.pskin_at[dex] = now_ms > 0 ? now_ms : 1;
}

}  // namespace ps

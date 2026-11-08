// mons.cpp — ver mons.h. Port de systems/mons.js y data/shop.js (web, d667c216).
#include "core/mons.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/halloween.h"
#include "core/i18n.h"
#include "core/moves.h"

namespace ps {

namespace {
constexpr double kHiddenRate = 1.0 / 20;
inline bool dex_ok(int dex) { return dex > 0 && dex <= kMaxDex && mon_by_dex(dex); }
inline uint32_t imul(uint32_t a, uint32_t b) { return a * b; }   // Math.imul (los mismos 32 bits)

// La semilla de cada Pokémon: «fecha de creación:dex:sal» (como roll de mons.js).
double roll(int64_t created, int dex, const char* salt) {
  char b[64];
  std::snprintf(b, sizeof(b), "%lld:%d:%s", (long long)created, dex, salt);
  return mons_hash01(b);
}

// rng(seed) de shop.js: FNV-1a y luego mulberry32.
struct ShopRng {
  uint32_t h = 2166136261u;
  explicit ShopRng(const char* s) { for (; *s; ++s) { h ^= uint8_t(*s); h = imul(h, 16777619u); } }
  double next() {
    h += 0x6D2B79F5u;
    uint32_t t = h;
    t = imul(t ^ (t >> 15), t | 1u);
    t ^= t + imul(t ^ (t >> 7), t | 61u);
    return double(t ^ (t >> 14)) / 4294967296.0;
  }
};
}  // namespace

double mons_hash01(const char* s) {
  uint32_t h = 2166136261u;
  for (; *s; ++s) { h ^= uint8_t(*s); h = imul(h, 16777619u); }
  h ^= h >> 13;
  h = imul(h, 0x5bd1e995u);
  h ^= h >> 15;
  return double(h) / 4294967296.0;
}

// ---------------- naturaleza ----------------

int nature_rolled(int64_t created, int dex) {
  const int i = int(std::floor(roll(created, dex, "nat") * kNatureCount));
  return i < 0 ? 0 : i >= kNatureCount ? kNatureCount - 1 : i;
}

int nature_of(const Save& s, int dex) {
  if (dex >= 0 && dex <= kMaxDex && s.pnat[dex] >= 0 && s.pnat[dex] < kNatureCount) return s.pnat[dex];
  return nature_rolled(s.created, dex);
}

void nature_set(Save& s, int dex, int nat, int64_t now_ms) {
  if (!dex_ok(dex) || nat < 0 || nat >= kNatureCount) return;
  s.pnat[dex] = int8_t(nat);
  s.pnat_at[dex] = now_ms;
}

const char* nature_name(int nat) {
  if (nat < 0 || nat >= kNatureCount) return "";
  return i18n_lang() == LANG_EN ? kNatures[nat].name_en : kNatures[nat].name;
}

void mon_with_nature(const Mon& base, int nat, Mon& out) {
  out = base;
  if (nat < 0 || nat >= kNatureCount) return;
  const NatureDef& n = kNatures[nat];
  auto field = [&](int st, double k) {
    switch (st) {
      case NS_PA: out.pa = float(double(base.pa) * k); break;
      case NS_SA: out.sa = float(double(base.sa) * k); break;
      case NS_DF: out.df = float(double(base.df) * k); break;
      case NS_SD: out.sd = float(double(base.sd) * k); break;
      case NS_SPD: out.spd = int16_t(std::lround(double(base.spd) * k)); break;   // (la velocidad exacta: nature_spd)
      default: break;
    }
  };
  if (n.up >= 0) field(n.up, 1.1);
  if (n.down >= 0) field(n.down, 0.9);
  out.atk = out.pa > out.sa ? out.pa : out.sa;
}

// ---------------- habilidad ----------------

const int16_t* abil_slots(int dex) {
  static const int16_t none[3] = {-1, -1, -1};
  return dex >= 0 && dex <= kMaxDex ? kDexAbil[dex] : none;
}

int abil_rolled_slot(int64_t created, int dex) {
  const int16_t* l = abil_slots(dex);
  if (l[2] >= 0 && roll(created, dex, "hid") < kHiddenRate) return 2;
  return l[1] >= 0 && roll(created, dex, "abil") < 0.5 ? 1 : 0;
}

AbilPick abil_of(const Save& s, int dex) {
  const int16_t* l = abil_slots(dex);
  const int p = dex >= 0 && dex <= kMaxDex ? s.pabil[dex] : -1;
  int slot = p >= 0 && p <= 2 && l[p] >= 0 ? p : abil_rolled_slot(s.created, dex);
  if (l[slot] < 0) slot = 0;
  AbilPick a;
  a.id = l[slot];
  a.slot = slot;
  return a;
}

int abil_choices(const Save& s, int dex, bool hidden, int out[3]) {
  const int16_t* l = abil_slots(dex);
  const int cur = abil_of(s, dex).slot;
  int n = 0;
  for (int i = 0; i < 3; ++i)
    if (l[i] >= 0 && i != cur && (i < 2 || hidden) && !(i == 1 && l[1] == l[0])) out[n++] = i;
  return n;
}

void abil_set(Save& s, int dex, int slot, int64_t now_ms) {
  if (!dex_ok(dex) || slot < 0 || slot > 2 || abil_slots(dex)[slot] < 0) return;
  s.pabil[dex] = int8_t(slot);
  s.pabil_at[dex] = now_ms;
}

const char* abil_name(int id) {
  if (id < 0 || id >= kAbilCount) return "";
  return i18n_lang() == LANG_EN ? kAbils[id].name_en : kAbils[id].name;
}

// ---------------- MT ----------------

int tm_index_of_move(int move) {
  for (int i = 0; i < kTmCount; ++i) if (kTms[i].move == move) return i;
  return -1;
}

// MT que ya sabe de serie (los de sus tipos y, del port, los extra de su Pokémon:
// Darkrai). Se calcula una vez por Pokémon (la tienda cuenta quién puede aprender cada una).
uint64_t tm_natural_bits(int dex) {
  static uint64_t cache[kMaxDex + 1];
  static uint8_t done[kMaxDex + 1];
  if (dex < 0 || dex > kMaxDex) return 0;
  if (done[dex]) return cache[dex];
  uint64_t b = 0;
  if (const Mon* m = mon_by_dex(dex)) {
    int16_t pool[kMaxMoves];
    const int n = move_pool_for_dex(dex, m->types, pool, kMaxMoves);
    for (int t = 0; t < kTmCount; ++t)
      for (int i = 0; i < n; ++i) if (pool[i] == kTms[t].move) { b |= uint64_t(1) << t; break; }
  }
  cache[dex] = b;
  done[dex] = 1;
  return b;
}

bool tm_natural(int dex, int tm) { return tm >= 0 && tm < kTmCount && ((tm_natural_bits(dex) >> tm) & 1u); }

bool tm_taught(const Save& s, int dex, int tm) {
  return dex >= 0 && dex <= kMaxDex && tm >= 0 && tm < kTmCount && ((s.ptm[dex] >> tm) & 1u);
}

uint64_t tm_taught_bits(const Save& s, int dex) { return dex >= 0 && dex <= kMaxDex ? s.ptm[dex] : 0; }

bool tm_can_learn(const Save& s, int dex, int tm) {
  if (!dex_ok(dex) || tm < 0 || tm >= kTmCount) return false;
  return ((kDexTms[dex] >> tm) & 1u) && !tm_natural(dex, tm) && !tm_taught(s, dex, tm);
}

int tm_learnable(const Save& s, int dex, int* out) {
  int n = 0;
  for (int i = 0; i < kTmCount; ++i) if (tm_can_learn(s, dex, i)) out[n++] = i;
  return n;
}

bool tm_take(Save& s, int tm) {
  if (tm < 0 || tm >= kTmCount || s.tms[tm] <= 0) return false;
  s.tms[tm]--;
  return true;
}

bool tm_teach(Save& s, int dex, int tm) {
  if (!tm_can_learn(s, dex, tm) || !tm_take(s, tm)) return false;
  s.ptm[dex] |= uint64_t(1) << tm;
  return true;
}

// ---------------- Mochila y tienda ----------------

const ShopItemDef kShopItems[BAG_COUNT] = {
    {"mint", "Menta", "Cambia la naturaleza de un Pokémon por la que tú elijas.", "mint", 1500},
    {"cap", "Cápsula Habilidad", "Cambia la habilidad de un Pokémon por otra de las suyas (no la oculta).", "capsule", 1500},
    {"caph", "Cápsula Habilidad Oculta", "Cambia la habilidad de un Pokémon por cualquiera de las suyas, incluida su habilidad oculta.", "capsuleh", 5000},
};

bool bag_take(Save& s, int item) {
  if (item < 0 || item >= BAG_COUNT || s.bag[item] <= 0) return false;
  s.bag[item]--;
  return true;
}

void shop_day(int64_t utc_ms, char out[9]) {
  int y, m, d;
  madrid_date(utc_ms, y, m, d);
  std::snprintf(out, 9, "%04d%02d%02d", (y % 10000 + 10000) % 10000, (m % 100 + 100) % 100 % 13, (d % 100 + 100) % 100 % 32);
}

int shop_daily(const Save& s, const char* day, int out[kShopDaily]) {
  int all[kMaxTms];
  for (int i = 0; i < kTmCount; ++i) all[i] = i;
  char seed[48];
  std::snprintf(seed, sizeof(seed), "kecleon:%s", day);
  ShopRng r(seed);
  for (int i = kTmCount - 1; i > 0; --i) {
    const int j = int(std::floor(r.next() * (i + 1)));
    const int t = all[i]; all[i] = all[j]; all[j] = t;
  }
  const int n = kTmCount < kShopDaily ? kTmCount : kShopDaily;
  for (int i = 0; i < n; ++i) out[i] = all[i];
  // Si ninguna le sirve a tu compañero, la última se cambia por una que sí aprenda.
  const int dex = s.partner;
  if (dex > 0 && n == kShopDaily) {
    bool any = false;
    for (int i = 0; i < n; ++i) if (tm_can_learn(s, dex, out[i])) any = true;
    if (!any) {
      int mine[kMaxTms];
      const int nm = tm_learnable(s, dex, mine);   // (ya por orden alfabético: kTms lo está)
      if (nm) {
        std::snprintf(seed, sizeof(seed), "kecleon:%s:%d", day, dex);
        ShopRng r2(seed);
        out[kShopDaily - 1] = mine[int(std::floor(r2.next() * nm))];
      }
    }
  }
  return n;
}

int tm_price(int tm) { return tm >= 0 && tm < kTmCount && kTms[tm].price > 0 ? kTms[tm].price : 400; }

ShopBuy shop_buy_item(Save& s, int item) {
  if (item < 0 || item >= BAG_COUNT) return BUY_BAD;
  const int price = kShopItems[item].price;
  if (s.coins < price) return BUY_NO_COINS;
  if (s.bag[item] >= 999) return BUY_FULL;
  s.coins -= price;
  s.bag[item]++;
  s.bag_known |= uint8_t(1u << item);
  return BUY_OK;
}

ShopBuy shop_buy_tm(Save& s, int tm, const char* day) {
  if (tm < 0 || tm >= kTmCount) return BUY_BAD;
  int today[kShopDaily];
  const int n = shop_daily(s, day, today);
  bool sold = false;
  for (int i = 0; i < n; ++i) if (today[i] == tm) sold = true;
  if (!sold) return BUY_GONE;
  const int price = tm_price(tm);
  if (s.coins < price) return BUY_NO_COINS;
  if (s.tms[tm] >= 999) return BUY_FULL;
  s.coins -= price;
  s.tms[tm]++;
  s.tms_known |= uint64_t(1) << tm;
  return BUY_OK;
}

}  // namespace ps

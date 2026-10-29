// gacha.cpp — ver gacha.h.
#include "core/gacha.h"

#include <algorithm>
#include <cstring>
#include <initializer_list>

#include "core/anim.h"

namespace ps {

int gacha_force_shiny = 0;

bool gacha_cost(const Save& s, int n, bool use_t10, int& t1, int& t10) {
  t1 = t10 = 0;
  if (n == 100 && use_t10) { if (s.t10 >= 10) { t10 = 10; return true; } return false; }
  if (n == 10 && use_t10) { if (s.t10 >= 1) { t10 = 1; return true; } return false; }
  if (s.t1 >= n) { t1 = n; return true; }
  return false;
}

int gacha_pool(int rarity, int gen, int16_t* out) {
  int n = 0;
  for (int i = 0; i < kDexCount; ++i) if ((gen == kGachaAll || kDex[i].gen == gen) && kDex[i].rarity == rarity) out[n++] = kDex[i].dex;
  return n;
}

const BannerDef& gacha_banner(int gen) {
  static const BannerDef kAll = {kGachaAll, "Todas", {0x2b1055, 0x7597de, 0xf5c542}};
  return gen >= 1 && gen <= 9 ? kBanners[gen - 1] : kAll;
}

int gacha_featured(int gen, int16_t* out) {
  // Elegidos a mano (el dueño, 2026-10-30): en Kanto Dragonite en vez de Articuno;
  // en Paldea, Koraidon, Miraidon y Gholdengo; en «Todas», tres de los más famosos.
  static const int16_t kAllF[3] = {150, 384, 249}, kPaldea[3] = {1007, 1008, 1000};
  // (los que no estén en la Pokédex del juego, p. ej. Miraidon, se cambian por el
  // mejor legendario que quede)
  const int16_t* pick = gen == kGachaAll ? kAllF : gen == 9 ? kPaldea : nullptr;
  int n0 = 0;
  if (pick) for (int i = 0; i < 3; ++i) if (mon_by_dex(pick[i])) out[n0++] = pick[i];
  int16_t p[1100];
  int n = gacha_pool(5, gen, p);
  // (del port: primero los legendarios de verdad; la web pone los 3 de 5★ con más
  // estadísticas y colaba a Dragonite, Tyranitar, Slaking… en el cartel)
  std::stable_sort(p, p + n, [](int16_t a, int16_t b) {
    const Mon *ma = mon_by_dex(a), *mb = mon_by_dex(b);
    if (ma->leg != mb->leg) return ma->leg > mb->leg;
    return ma->bst > mb->bst;
  });
  for (int i = 0; i < n && n0 < 3; ++i) {
    bool dup = false;
    for (int k = 0; k < n0; ++k) dup = dup || out[k] == p[i];
    if (!dup) out[n0++] = p[i];
  }
  n = n0;
  if (gen == 1) for (int i = 0; i < n; ++i) if (out[i] == 144 && mon_by_dex(149)) out[i] = 149;   // (Articuno → Dragonite)
  return n;
}

namespace {
// Pool no vacío más cercano (algunas generaciones tienen rarezas casi vacías).
int pool_near(int rarity, int gen, int16_t* out, int& got) {
  for (int d : {0, -1, 1, -2, 2, -3, 3, -4, 4}) {
    const int r = rarity + d;
    if (r < 1 || r > 5) continue;
    const int n = gacha_pool(r, gen, out);
    if (n) { got = r; return n; }
  }
  got = rarity;
  int n = 0;
  for (int i = 0; i < kDexCount; ++i) if (gen == kGachaAll || kDex[i].gen == gen) out[n++] = kDex[i].dex;
  return n;
}

int roll_rarity(Rng& rng, int min = 1) {
  float total = 0;
  for (int r = min; r <= 5; ++r) total += kGachaRates[r - 1];
  float x = rng.next() * total;
  for (int r = min; r <= 5; ++r) { x -= kGachaRates[r - 1]; if (x <= 0) return r; }
  return 5;
}
}  // namespace

void gacha_dex_of(const Save& s, int gen, int& have, int& total) {
  have = total = 0;
  for (int i = 0; i < kDexCount; ++i) if (gen == kGachaAll || kDex[i].gen == gen) { total++; if (s.owns(kDex[i].dex)) have++; }
}
bool gacha_has_iris(const Save& s, int gen) {
  int have, total;
  gacha_dex_of(s, gen, have, total);
  return total > 0 && have >= total;
}
float gacha_shiny_rate(const Save& s, int gen) { return gacha_has_iris(s, gen) ? kShinyRateIris : kShinyRate; }

int gacha_pull(Save& s, int n, int gen, bool use_t10, Rng& rng, int64_t now_ms, PullResult* out) {
  if (n == 100) {   // diez ×10 seguidas
    int c1, c10;
    if (!gacha_cost(s, 100, use_t10, c1, c10)) return 0;
    int got = 0;
    for (int k = 0; k < 10; ++k) got += gacha_pull(s, 10, gen, use_t10, rng, now_ms, out + got);
    return got;
  }
  int c1, c10;
  if (!gacha_cost(s, n, use_t10, c1, c10)) return 0;
  const float rate = gacha_shiny_rate(s, gen);   // (con el Amuleto Iris de esta región, al doble)
  s.t1 -= c1; s.t10 -= c10;
  const int64_t now = now_ms ? now_ms : 1;
  int16_t list[1100];
  for (int i = 0; i < n; ++i) {
    s.pity[gen]++;
    int want;
    bool any4 = false;
    for (int k = 0; k < i; ++k) if (out[k].rarity >= 4) any4 = true;
    if (s.pity[gen] >= kPity5) want = 5;
    else if (n == 10 && i == n - 1 && !any4) want = roll_rarity(rng, 4);
    else want = roll_rarity(rng);
    int rarity;
    const int nl = pool_near(want, gen, list, rarity);
    if (rarity == 5) s.pity[gen] = 0;
    int dex = list[int(rng.next() * nl)];
    bool shiny = has_shiny(dex) && (gacha_force_shiny > 0 ? (gacha_force_shiny--, true) : rng.next() < rate);
    // Los shiny no se repiten: si ya lo tienes, otro de la misma rareza cuyo shiny te falte.
    if (shiny && s.shiny_at[dex]) {
      int16_t miss[1100]; int nm = 0;
      for (int k = 0; k < nl; ++k) if (has_shiny(list[k]) && !s.shiny_at[list[k]]) miss[nm++] = list[k];
      if (nm) dex = miss[int(rng.next() * nm)];
      else shiny = false;
    }
    PullResult& r = out[i];
    r.dex = int16_t(dex); r.rarity = int8_t(rarity); r.shiny = shiny; r.refund = 0;
    if (shiny) {
      r.is_new = !s.shiny_at[dex];
      if (r.is_new) { s.shiny_at[dex] = now; s.shiny_from[dex] = FROM_GACHA; }
      else r.refund = kGachaRefund[rarity] * 3;
      if (!s.owned_at[dex]) { s.owned_at[dex] = now; s.owned_from[dex] = FROM_GACHA; }
    } else {
      r.is_new = !s.owned_at[dex];
      if (r.is_new) { s.owned_at[dex] = now; s.owned_from[dex] = FROM_GACHA; }
      else r.refund = kGachaRefund[rarity];
    }
    s.coins += r.refund;
  }
  s.stats.pulls += n;
  return n;
}

void TraitScores::add(int trait, int p) {
  for (int i = 0; i < n; ++i) if (order[i] == trait) { pts[i] = int16_t(pts[i] + p); return; }
  if (n < 32) { order[n] = int8_t(trait); pts[n] = int16_t(p); n++; }
}

void personality_result(const TraitScores& s, Rng& rng, int& trait, int& dex) {
  // Rasgo principal: el de más puntos (a igualdad, el primero que se sumó).
  int best = 0;
  for (int i = 1; i < s.n; ++i) if (s.pts[i] > s.pts[best]) best = i;
  trait = s.n ? s.order[best] : 0;
  auto score = [&](const char* id) {
    for (int i = 0; i < s.n; ++i) if (!std::strcmp(kTraits[s.order[i]].id, id)) return int(s.pts[i]);
    return 0;
  };
  int cand[64], nc = 0, bs = -1;
  for (int i = 0; i < kStarterCount; ++i) {
    // El rasgo principal pesa el doble.
    const int v = score(kStarters[i].traits[0]) * 2 + score(kStarters[i].traits[1]);
    if (v > bs) { bs = v; nc = 0; cand[nc++] = i; }
    else if (v == bs) cand[nc++] = i;
  }
  dex = kStarters[cand[int(rng.next() * nc)]].dex;
}

}  // namespace ps

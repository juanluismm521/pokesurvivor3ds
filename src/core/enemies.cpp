// enemies.cpp — Pokémon salvajes de todas las generaciones (enemies.js).
// Cada tramo recoge a todos los Pokémon cuyo total de stats base cae en su
// rango, y al empezar una run se elige al azar un ELENCO de cada tramo.
// Jefes: Pokémon fuertes NO legendarios, al azar por franjas de poder.
#include <algorithm>
#include <cmath>
#include <initializer_list>

#include "core/weekly.h"
#include "core/game.h"

namespace ps {

namespace {
float base_hp(const Mon& p) { return (p.hp - 70) / 0.45f; }
float base_spd(const Mon& p) { return (p.spd - 78) / 0.42f; }

bool in_list(const int16_t* l, int n, int dex) { for (int i = 0; i < n; ++i) if (l[i] == dex) return true; return false; }
int summon_of(int dex) { for (int i = 0; i < kSummonCount; ++i) if (kSummon[i].dex == dex) return kSummon[i].minion; return 0; }
bool has_meta(int dex) { return mon_by_dex(dex) != nullptr; }
bool has_t(const Mon& p, int t) { return p.types[0] == t || p.types[1] == t; }

bool is_plain(int b) { return b == BH_CHASE || b == BH_CHARGER || b == BH_RANGED || b == BH_TANK; }
bool is_support(int b) { return b == BH_HEALER || b == BH_SHIELDER || b == BH_SUMMONER; }

// Candidatos de un tramo (sin legendarios ni míticos), en el orden de la Pokédex.
int candidates(int ti, const Mon** out, int mono = -1) {
  const Tier& t = kTiers[ti];
  int n = 0;
  for (int i = 0; i < kDexCount; ++i) {
    const Mon& p = kDex[i];
    if (p.rarity < 5 && p.bst >= t.bst0 && p.bst <= t.bst1) out[n++] = &p;
  }
  if (mono < 0) return n;
  // Desafío semanal «Todos de tipo X»: sólo los de ese tipo (si hay menos de 3, con
  // 90 de margen en los stats, como la web)
  int m = 0;
  for (int i = 0; i < n; ++i) if (out[i]->types[0] == mono || out[i]->types[1] == mono) out[m++] = out[i];
  if (m >= 3) return m;
  m = 0;
  for (int i = 0; i < kDexCount; ++i) {
    const Mon& p = kDex[i];
    if (p.rarity < 5 && (p.types[0] == mono || p.types[1] == mono) && p.bst >= t.bst0 - 90 && p.bst <= t.bst1 + 90) out[m++] = &p;
  }
  return m;
}
static int mono_of(const Game& g) { return g.cfg.weekly && g.cfg.wk_cond == WC_MONO ? g.cfg.wk_type : -1; }

// U.pickN: n elementos distintos al azar (sacándolos de una copia).
int pick_n(Game& g, const Mon** src, int nsrc, int n, const Mon** out) {
  static const Mon* c[kMaxDex + 1];
  for (int i = 0; i < nsrc; ++i) c[i] = src[i];
  int nc = nsrc, k = 0;
  while (k < n && nc) {
    const int j = int(g.rng.next() * nc);
    out[k++] = c[j];
    for (int q = j; q < nc - 1; ++q) c[q] = c[q + 1];
    nc--;
  }
  return k;
}
}  // namespace

int behavior_of(const Mon& p) {
  const double h = hash(form_web_id(p.dex), 7, 11);   // (las formas regionales, con su id de la web)
  if (in_list(kHealDex, kHealCount, p.dex)) return BH_HEALER;
  if (in_list(kShieldDex, kShieldCount, p.dex)) return BH_SHIELDER;
  if (in_list(kBombDex, kBombCount, p.dex)) return BH_BOMBER;
  const int sm = summon_of(p.dex);
  if (sm && has_meta(sm)) return BH_SUMMONER;
  // Enjambres: algunos Bicho llaman a más de los suyos (desde el principio).
  if (has_t(p, T_BUG) && h >= 0.6 && h < 0.78) return BH_SUMMONER;
  if ((has_t(p, T_ELECTRIC) || has_t(p, T_PSYCHIC) || has_t(p, T_DRAGON) || has_t(p, T_ICE)) && h < 0.18) return BH_BEAMER;
  if ((has_t(p, T_FIRE) || has_t(p, T_GROUND) || has_t(p, T_POISON)) && h < 0.18) return BH_ZONER;
  if ((has_t(p, T_WATER) || has_t(p, T_GRASS) || has_t(p, T_BUG) || has_t(p, T_FAIRY)) && h >= 0.18 && h < 0.32) return BH_FAN;
  if ((has_t(p, T_FIGHTING) || has_t(p, T_GROUND) || has_t(p, T_NORMAL) || has_t(p, T_ROCK)) && h >= 0.42 && h < 0.56) return BH_JUMPER;
  if ((has_t(p, T_PSYCHIC) || has_t(p, T_ELECTRIC) || has_t(p, T_GHOST) || has_t(p, T_FAIRY)) && h < 0.4) return BH_RANGED;
  if ((has_t(p, T_FIRE) || has_t(p, T_WATER) || has_t(p, T_POISON) || has_t(p, T_ICE)) && h < 0.22) return BH_RANGED;
  if ((has_t(p, T_FIGHTING) || has_t(p, T_NORMAL) || has_t(p, T_GROUND) || has_t(p, T_ROCK) || has_t(p, T_DRAGON) || has_t(p, T_STEEL) || has_t(p, T_DARK)) && h < 0.4) return BH_CHARGER;
  if (base_hp(p) > 95 && base_spd(p) < 50) return BH_TANK;
  return BH_CHASE;
}

EnemyDef enemy_def_for(const Mon& p, int ti) {
  const Tier& tier = kTiers[ti];
  const float hp_k = clamp(base_hp(p) / 65, 0.7f, 1.6f);
  const float spd_k = clamp(0.75f + base_spd(p) / 260, 0.75f, 1.3f);
  const int b = behavior_of(p);
  EnemyDef d;
  d.dex = p.dex;
  d.behavior = int8_t(b);
  d.ti = int8_t(ti);
  d.hp_f = hp_k * (b == BH_TANK ? 1.5f : 1);
  d.dmg_f = b == BH_TANK ? 1.2f : 1;
  d.xp_f = b == BH_TANK ? 1.3f : 1;
  d.hp = float(jsround(tier.hp * hp_k * (b == BH_TANK ? 1.5f : 1)));
  d.spd = float(jsround(tier.spd * spd_k * (b == BH_TANK ? 0.7f : 1)));
  d.dmg = float(jsround(tier.dmg * (b == BH_TANK ? 1.2f : 1)));
  d.xp = std::fmax(3.0f, float(jsround(tier.xp * (b == BH_TANK ? 1.3f : 1))));
  // Los que pelean de lejos: daño del disparo, recarga y distancia.
  float cd = 0;
  switch (b) {
    case BH_RANGED: cd = 2.2f - ti * 0.15f; break;
    case BH_FAN: cd = 3.2f - ti * 0.15f; break;
    case BH_BEAMER: cd = 4.4f - ti * 0.2f; break;
    case BH_ZONER: cd = 4.6f - ti * 0.2f; break;
    case BH_HEALER: cd = 3.5f; break;
    case BH_SUMMONER: cd = 7; break;
    default: break;
  }
  if (cd > 0) {
    d.shot_dmg = float(jsround(tier.dmg * (b == BH_FAN ? 0.7f : 0.8f)));
    d.shot_cd = float(jsround(cd * 100) / 100);   // +toFixed(2)
    d.range = (b == BH_BEAMER ? 260.0f : b == BH_HEALER || b == BH_SUMMONER ? 230.0f : 190.0f) + ti * 6;
  }
  if (b == BH_SUMMONER) d.minion = int16_t(summon_of(p.dex));
  if (b == BH_FAN) d.fan_n = ti < 2 ? 3 : 5;               // abanico más ancho más adelante
  if (b == BH_ZONER) d.zones = ti < 2 ? 1 : ti < 4 ? 2 : 3; // más zonas más adelante
  if (b == BH_BOMBER) { d.spd = float(jsround(d.spd * 1.25f)); d.hp = float(jsround(d.hp * 0.8f)); d.hp_f *= 0.8f; }
  return d;
}

// Elenco de un tramo: siempre un "apoyo" (curandero, escudo o invocador) y,
// desde el segundo tramo, también uno de los que atacan con aviso. En el
// primero no hay más personalidades (para ir aprendiendo).
static int cast_for(Game& g, int ti, const Mon** out) {
  static const Mon* all[kMaxDex + 1];
  const int nall = candidates(ti, all, mono_of(g));
  int kinds[3], nk = 0;
  for (int k : {BH_HEALER, BH_SHIELDER, BH_SUMMONER}) {
    for (int i = 0; i < nall; ++i) if (behavior_of(*all[i]) == k) { kinds[nk++] = k; break; }
  }
  int n = 0;
  if (nk) {
    const int sk = kinds[int(g.rng.next() * nk)];
    static const Mon* f[kMaxDex + 1]; int nf = 0;
    for (int i = 0; i < nall; ++i) if (behavior_of(*all[i]) == sk) f[nf++] = all[i];
    n += pick_n(g, f, nf, 1, out + n);
  }
  const Tier& tier = kTiers[ti];
  if (tier.from > 0) {
    static const Mon* f[kMaxDex + 1]; int nf = 0;
    for (int i = 0; i < nall; ++i) { const int b = behavior_of(*all[i]); if (!is_plain(b) && !is_support(b)) f[nf++] = all[i]; }
    n += pick_n(g, f, nf, 1, out + n);
  }
  auto picked = [&](const Mon* p) { for (int i = 0; i < n; ++i) if (out[i] == p) return true; return false; };
  static const Mon* f[kMaxDex + 1]; int nf = 0;
  for (int i = 0; i < nall; ++i) {
    if (picked(all[i])) continue;
    if (tier.from == 0 && !is_plain(behavior_of(*all[i]))) continue;
    f[nf++] = all[i];
  }
  n += pick_n(g, f, nf, tier.cast - n, out + n);
  return n;
}

void enemies_roll_run(Game& g) {
  g.ncast = kTierCount;
  for (int ti = 0; ti < kTierCount; ++ti) {
    const Mon* list[16];
    const int n = cast_for(g, ti, list);
    Game::CastTier& ct = g.cast[ti];
    ct.from = kTiers[ti].from;
    ct.n = std::min(n, 8);
    for (int i = 0; i < ct.n; ++i) ct.list[i] = enemy_def_for(*list[i], ti);
    // Halloween (core/halloween.h): en cada tramo, dos Pokémon Fantasma más de su nivel.
    if (g.hw && mono_of(g) < 0) {
      static const Mon* all[kMaxDex + 1];
      static const Mon* ghosts[kMaxDex + 1];
      const int nall = candidates(ti, all);
      int ng = 0;
      for (int i = 0; i < nall; ++i) {
        const Mon& p = *all[i];
        if (p.types[0] != T_GHOST && p.types[1] != T_GHOST) continue;
        bool have = false;
        for (int k = 0; k < ct.n; ++k) have = have || ct.list[k].dex == p.dex;
        if (!have) ghosts[ng++] = all[i];
      }
      const Mon* pick[2];
      const int np = pick_n(g, ghosts, ng, 2, pick);
      for (int i = 0; i < np && ct.n < 10; ++i) ct.list[ct.n++] = enemy_def_for(*pick[i], ti);
    }
  }
  // Uno por tramo: el más rápido de los "normales" pasa a corredor.
  for (int ti = 0; ti < g.ncast; ++ti) {
    Game::CastTier& ct = g.cast[ti];
    EnemyDef* r = nullptr;
    for (int i = 0; i < ct.n; ++i) {
      EnemyDef& d = ct.list[i];
      if (d.behavior != BH_CHASE && d.behavior != BH_CHARGER && d.behavior != BH_TANK) continue;
      if (!r || d.spd > r->spd) r = &d;
    }
    if (r) { r->behavior = BH_RUNNER; r->spd = float(jsround(r->spd * 1.6f)); r->hp = float(jsround(r->hp * 0.7f)); r->hp_f *= 0.7f; }
  }
  // Jefes.
  int16_t used[8];
  int nused = 0;
  g.nbosses = kBossSlotCount;
  for (int s = 0; s < kBossSlotCount; ++s) {
    const BossSlot& slot = kBossSlots[s];
    const int mono = mono_of(g);
    auto ok = [&](const Mon& p) {
      if (p.leg) return false;
      if (mono >= 0 && p.types[0] != mono && p.types[1] != mono) return false;
      for (int i = 0; i < nused; ++i) if (used[i] == p.dex) return false;
      return true;
    };
    static const Mon* pool[kMaxDex + 1]; int np = 0;
    for (int i = 0; i < kDexCount; ++i) if (ok(kDex[i]) && kDex[i].bst >= slot.bst0 && kDex[i].bst <= slot.bst1) pool[np++] = &kDex[i];
    if (!np) for (int i = 0; i < kDexCount; ++i) if (ok(kDex[i]) && kDex[i].bst >= 580) pool[np++] = &kDex[i];
    if (!np && mono >= 0) {   // (los 3 más fuertes de ese tipo)
      for (int i = 0; i < kDexCount; ++i) if (ok(kDex[i])) pool[np++] = &kDex[i];
      std::stable_sort(pool, pool + np, [](const Mon* x, const Mon* y) { return x->bst > y->bst; });
      np = std::min(np, 3);
    }
    if (!np) for (int i = 0; i < kDexCount; ++i) if (!kDex[i].leg && kDex[i].bst >= 580) {
      bool u = false;
      for (int k = 0; k < nused; ++k) u = u || used[k] == kDex[i].dex;
      if (!u) pool[np++] = &kDex[i];
    }
    const Mon& p = *pool[int(g.rng.next() * np)];
    used[nused++] = p.dex;
    EnemyDef& b = g.bosses[s];
    b = EnemyDef();
    b.at = slot.at; b.dex = p.dex; b.idx = int8_t(s);
    // (Desde 2026-10-10 más duros: ×1,35 de vida y ×1,2 de daño.)
    b.hp = float(jsround(slot.hp * 1.35));
    b.spd = float(jsround(clamp(40 + base_spd(p) * 0.5f, 40.0f, 100.0f)));
    b.dmg = float(jsround(slot.dmg * 1.2f));
    b.xp = slot.xp;
    b.behavior = behavior_of(p) == BH_RANGED ? BH_RANGED : (base_spd(p) > 90 ? BH_CHARGER : BH_CHASE);
    b.shot_dmg = float(jsround(slot.dmg * 0.55f)); b.shot_cd = 1.1f; b.range = 260;
  }
}

// Vida y daño de un enemigo en el segundo t (Enemies.scale): ya no salen de su tramo
// sino de las curvas de core/diff.h; los de un tramo viejo aguantan y pegan menos.
Scale3 enemies_scale(const Game& g, float t, const EnemyDef* d) {
  const bool old = d && d->ti >= 0 && d->ti < enemies_tier_at(t);
  const double hp = diff_mob_hp(g.cfg.diff, t, g.team_size()) * (d ? d->hp_f : 1) * (old ? 0.6 : 1);
  const double dmg = diff_mob_dmg(g.cfg.diff, t) * (d ? d->dmg_f : 1) * (old ? 0.75 : 1);
  return {float(d && d->hp > 0 ? hp / d->hp : hp), float(d && d->dmg > 0 ? dmg / d->dmg : dmg), 1 + std::fmin(0.35f, t / 1400)};
}
Scale3 enemies_boss_scale(const Game& g, float t, const EnemyDef& b) {
  return {float(diff_boss_hp(g.cfg.diff, b.idx, t, g.team_size()) / b.hp), float(diff_boss_dmg(g.cfg.diff, t) / b.dmg), 1};
}
float enemies_xp_of(const Game& g, float t, const EnemyDef* d) {
  (void)g;
  return float(std::fmax(1.0, double(jsround(diff_mob_xp(t) * (d ? d->xp_f : 1) * (d ? d->hp_f : 1)))));
}

int enemies_tier_at(float t) {
  int ti = 0;
  for (int i = 0; i < kTierCount; ++i) if (t >= kTiers[i].from) ti = i;
  return ti;
}

static bool no_horde(int b) { return b == BH_BOMBER || b == BH_HEALER || b == BH_SHIELDER || b == BH_SUMMONER; }

// Tipos con bastantes Pokémon en el tramo actual para una horda (events.cpp).
int enemies_horde_types(const Game& g, float t, int8_t* out) {
  static const Mon* all[kMaxDex + 1];
  const int nall = candidates(enemies_tier_at(t), all, mono_of(g));
  int n = 0;
  for (int ty = 0; ty < T_COUNT; ++ty) {   // (orden de TYPE_COLOR = orden de los tipos)
    int c = 0;
    for (int i = 0; i < nall; ++i) if (!no_horde(behavior_of(*all[i])) && has_t(*all[i], ty)) c++;
    if (c >= 3) out[n++] = int8_t(ty);
  }
  return n;
}

// Hasta 4 Pokémon de un tipo, con los stats del tramo actual.
int enemies_horde_defs(Game& g, int type, float t, EnemyDef* out) {
  const int ti = enemies_tier_at(t);
  static const Mon* all[kMaxDex + 1];
  const int nall = candidates(ti, all, mono_of(g));
  static const Mon* pool[kMaxDex + 1]; int np = 0;
  for (int i = 0; i < nall; ++i) if (has_t(*all[i], type) && !no_horde(behavior_of(*all[i]))) pool[np++] = all[i];
  const Mon* pick[4];
  const int n = pick_n(g, pool, np, 4, pick);
  for (int i = 0; i < n; ++i) out[i] = enemy_def_for(*pick[i], ti);
  return n;
}

// Bolsa de enemigos del segundo `t` (con pesos: los tramos viejos pierden peso).
// Devuelve las definiciones y su peso por separado (pesos en out_w).
int enemies_bag_at(Game& g, float t, const EnemyDef** out, int max) {
  int n = 0;
  if (g.events.cur == EVN_HORDE && g.events.nhorde) {
    for (int i = 0; i < g.events.nhorde && n < max; ++i) out[n++] = &g.events.horde[i];
    return n;
  }
  if (!g.ncast) enemies_roll_run(g);
  for (int k = 0; k < g.ncast; ++k) {
    const Game::CastTier& tier = g.cast[k];
    if (t < tier.from) continue;
    const int w = t - tier.from > 260 ? 3 : 10;
    for (int i = 0; i < tier.n; ++i) for (int j = 0; j < w && n < max; ++j) out[n++] = &tier.list[i];
  }
  if (!n) for (int i = 0; i < g.cast[0].n && n < max; ++i) out[n++] = &g.cast[0].list[i];
  return n;
}

}  // namespace ps

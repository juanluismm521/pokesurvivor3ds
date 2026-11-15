// levelup.cpp — las cartas de subida de nivel (levelup.js de la web).
// Del port (el dueño, 2026-10-30): con 8 movimientos (dos vínculos) o desde el
// nivel kFourLevel salen 4 cartas, y al menos 2 son pasivas (estadísticas); desde
// kMorePassiveLevel, al menos 3 (llega un momento en que ya no caben más movimientos).
//   new   aprender un movimiento nuevo (sólo si quedan huecos de los 4)
//   up    mejorar un movimiento que ya tienes
//   evo   evolucionar un movimiento al máximo con su condición cumplida (dorada)
//   stat  mejora permanente de la run
#include <cmath>

#include "core/game.h"
#include "core/mega.h"
#include "core/stats.h"

namespace ps {

namespace {
struct Cand { Card c; int key; };

int card_key(const Card& c) {
  // new:id · evo:id · up:id · stat:id (claves distintas por tipo)
  switch (c.type) {
    case CT_NEW: return 1000 + c.move;
    case CT_EVO: return 2000 + c.move;
    case CT_UP: return 3000 + c.move;
    default: return 4000 + c.stat;
  }
}

int candidates(Game& g, Player& p, Cand* out) {
  int n = 0;
  // --- movimientos nuevos ---
  if (p.nslots < p.max_moves) {
    // Con 2 potenciadores ya no se ofrecen más (no hacen daño).
    int buffs = 0;
    for (int i = 0; i < p.nmoves; ++i) if (kind_is_status(p.moves[i].kind())) buffs++;   // (Moves.isStatus)
    int16_t all[kMaxMoves];
    const int na = move_pool_for_dex(p.dex, p.mon->types, all, kMaxMoves, p.taught);   // (con sus MT: poolFor de la web)
    int16_t pool[kMaxMoves];
    int np = 0;
    for (int i = 0; i < na; ++i) {
      const int id = all[i];
      if (player_has_move(p, id)) continue;
      if (p.banned.has(id)) continue;
      if (buffs >= 2 && kind_is_status(kMoves[id].kind)) continue;
      pool[np++] = int16_t(id);
    }
    // pickWeighted(pool, 6, weight): los de su lado bueno (y los empatados y
    // los potenciadores) salen el doble.
    float w[kMaxMoves];
    for (int i = 0; i < np; ++i) w[i] = stats_fit(p.mon, kMoves[pool[i]].cat) >= 0.999f ? 2.0f : 1.0f;
    for (int k = 0; k < 6 && np; ++k) {
      float sum = 0;
      for (int i = 0; i < np; ++i) sum += w[i];
      float r = g.rng.next() * sum;
      int j = 0;
      while (j < np - 1 && (r -= w[j]) > 0) j++;
      Cand& c = out[n++];
      c.c = Card(); c.c.type = CT_NEW; c.c.move = pool[j]; c.c.w = 10;
      for (int q = j; q < np - 1; ++q) { pool[q] = pool[q + 1]; w[q] = w[q + 1]; }
      np--;
    }
  }

  // --- su ataque exclusivo (web, 13-11-2026): carta especial desde el nivel kOnlyLvl ---
  if (p.nslots < p.max_moves && p.level >= kOnlyLvl) {
    int16_t ex[8];
    const int ne = move_exclusives_for(p.dex, ex, 8);
    for (int i = 0; i < ne; ++i) {
      if (player_has_move(p, ex[i]) || p.banned.has(ex[i])) continue;
      Cand& c = out[n++];
      c.c = Card(); c.c.type = CT_NEW; c.c.move = ex[i]; c.c.w = 30; c.c.excl = true;
    }
  }

  // --- evoluciones: movimiento al máximo + su condición cumplida ---
  int8_t waiting[LS_COUNT];
  for (auto& w : waiting) w = -1;
  for (int i = 0; i < p.nmoves; ++i) {
    EvoInfo ev;
    if (!move_evo_info(p.moves[i], p.stats_taken, ev)) continue;
    if (!ev.ready) { waiting[ev.need] = int8_t(i); continue; }
    Cand& c = out[n++];
    c.c = Card(); c.c.type = CT_EVO; c.c.move = int16_t(i); c.c.w = 40;
  }

  // --- mejoras de movimientos que ya tienes ---
  for (int i = 0; i < p.nmoves; ++i) {
    if (!move_next_up_text(p.moves[i])) continue;
    Cand& c = out[n++];
    c.c = Card(); c.c.type = CT_UP; c.c.move = int16_t(i); c.c.w = 8;
  }

  // --- stats ---
  // Cada mejora tiene un tope de veces (`max`). Las que piden una evolución salen siempre.
  int8_t avail[LS_COUNT];
  int na = 0;
  for (int s = 0; s < LS_COUNT; ++s) {
    if (waiting[s] >= 0) continue;
    if (kLevelStats[s].max && p.stats_taken[s] >= kLevelStats[s].max) continue;
    // Poder rápido: sólo si llevas una megapiedra (en la web, si tienes poder de Eternatus)
    if (s == LS_PWR && !is_mega_stone(p.item) && p.epower < 0) continue;
    avail[na++] = int8_t(s);
  }
  int8_t picks[LS_COUNT];
  int npk = 0;
  for (int k = 0; k < 5 && na; ++k) {
    const int j = int(g.rng.next() * na);
    picks[npk++] = avail[j];
    for (int q = j; q < na - 1; ++q) avail[q] = avail[q + 1];
    na--;
  }
  for (int s = 0; s < LS_COUNT; ++s) if (waiting[s] >= 0) picks[npk++] = int8_t(s);
  for (int k = 0; k < npk; ++k) {
    const int s = picks[k];
    Cand& c = out[n++];
    c.c = Card(); c.c.type = CT_STAT; c.c.stat = int8_t(s); c.c.evo_for = waiting[s];
    c.c.w = (waiting[s] >= 0 ? 14.0f : p.nslots < p.max_moves ? 4.0f : 7.0f) * (waiting[s] < 0 && s == LS_LUCK ? 0.4f : 1.0f);   // (wk)
  }
  for (int i = 0; i < n; ++i) out[i].key = card_key(out[i].c);
  return n;
}
}  // namespace

bool levelup_four(const Player& p) { return p.four_up && (p.nmoves >= kFourMoves || p.level >= kFourLevel); }
int levelup_min_passive(const Player& p) { return !levelup_four(p) ? 0 : p.level >= kMorePassiveLevel ? 3 : 2; }

// 3 cartas distintas (o 4: levelup_four), elegidas por peso. Si hay una evolución
// lista, sale siempre. Con 4, primero las pasivas que tocan y luego el resto.
void levelup_offer(Game& g, Player& p) {
  Cand pool[kMaxMoves + 32];   // movimientos nuevos + mejoras + estadísticas
  int n = candidates(g, p, pool);
  p.ncards = 0;
  const int want = levelup_four(p) ? 4 : 3;
  int need_stat = levelup_min_passive(p);
  int used[4], nu = 0;
  for (int i = 0; i < n; ++i) {
    if (pool[i].c.type != CT_EVO) continue;
    p.cards[p.ncards++] = pool[i].c;
    used[nu++] = pool[i].key;
    for (int q = i; q < n - 1; ++q) pool[q] = pool[q + 1];
    n--;
    break;
  }
  while (p.ncards < want && n) {
    // (mientras falten pasivas, sólo entre las pasivas; si no quedan, entre todas)
    bool only_stat = false;
    if (need_stat > 0) for (int i = 0; i < n; ++i) if (pool[i].c.type == CT_STAT) { only_stat = true; break; }
    float total = 0;
    for (int i = 0; i < n; ++i) if (!only_stat || pool[i].c.type == CT_STAT) total += pool[i].c.w;
    float r = g.rng.next() * total;
    int idx = -1, first = -1;
    for (int i = 0; i < n; ++i) {
      if (only_stat && pool[i].c.type != CT_STAT) continue;
      if (first < 0) first = i;
      r -= pool[i].c.w;
      if (r <= 0) { idx = i; break; }
    }
    if (idx < 0) idx = first < 0 ? 0 : first;   // (como la web: si el redondeo deja r > 0, el primero)
    const Cand c = pool[idx];
    for (int q = idx; q < n - 1; ++q) pool[q] = pool[q + 1];
    n--;
    bool dup = false;
    for (int k = 0; k < nu; ++k) if (used[k] == c.key) dup = true;
    if (dup) continue;
    used[nu++] = c.key;
    p.cards[p.ncards++] = c.c;
    if (c.c.type == CT_STAT && need_stat > 0) need_stat--;
  }
  // La primera vez, se explica (sólo a quien juega en esta consola).
  if (want == 4 && !p.four_told) {
    p.four_told = true;
    if (&p == &g.players[g.local]) g.say("¡Ahora eliges entre 4 cartas! Al menos 2 son pasivas: poder, velocidad, defensa, recarga…", 5);
  }
}

// Las mejoras que multiplican ya no se acumulan una sobre otra (web, 13-11-2026): la
// n-ésima deja el total en 1 + k·n (sumar), no en (1 + k)^n.
static void stack(float& field, const Player& p, int s, float k) {
  const int n = p.stats_taken[s];
  field *= (1 + k * (n + 1)) / (1 + k * n);
}

static void apply_stat(Game& g, Player& p, int s) {
  switch (s) {
    case LS_HP: player_add_max_hp(p, 20); break;
    case LS_ATK: stack(p.atk_mul, p, s, 0.08f); break;
    case LS_SPD: stack(p.spd_mul, p, s, 0.05f); break;
    case LS_CD: stack(p.cd_mul, p, s, -0.05f); break;
    case LS_DEF: {   // (el tope es sin la mega, que se quita al acabar)
      const float mega = p.mega >= 0 ? mega_boost(p.mega).armor : 0;
      p.dmg_reduce = std::fmin(0.5f, p.dmg_reduce - mega + 0.05f) + mega;
      break;
    }
    case LS_RGN: p.regen_flat += 0.6f; break;
    case LS_MAG: stack(p.magnet, p, s, 0.25f); break;
    case LS_XP: stack(p.xp_mul, p, s, 0.08f); break;
    case LS_HEAL: player_heal(g, p, p.max_hp * 0.45f); break;
    case LS_CRIT: p.crit += 0.02f; break;                       // Ojo Crítico
    case LS_CRITD: p.crit_dmg += 0.05f; break;                  // Golpe Brutal
    case LS_HUNT: stack(p.boss_mul, p, s, 0.06f); break;        // Cazador
    case LS_DODGE: p.dodge = std::fmin(0.15f, p.dodge + 0.03f); break;
    case LS_PWR: stack(p.power_cd_k, p, s, -0.08f); break;      // Poder rápido (la mega y el poder de Eternatus)
    case LS_LUCK: stack(p.coin_mul, p, s, 0.1f); break;         // Racha de suerte
    case LS_COIN: p.coins += 60; break;                         // Bolsa de monedas
    default: break;
  }
}

void levelup_apply(Game& g, Player& p, const Card& c) {
  switch (c.type) {
    case CT_NEW: player_add_move(g, p, c.move); break;
    case CT_UP: move_level_up(p.moves[c.move]); break;
    case CT_EVO: {
      MoveInst& m = p.moves[c.move];
      move_evolve(m);
      g.fx.ring(p.x, p.y, 8, 120, 0xffd23f, 0.8f, 6);
      g.fx.burst(p.x, p.y - p.body_h * 0.5f, 0xffe14d, 40, 240);
      g.sfx(SFX_LEGEND);
      if (&p == &g.me()) progress_event(g, EV_EVOLVE, m.def);   // (sólo las tuyas)
      break;
    }
    case CT_STAT:
      apply_stat(g, p, c.stat);
      if (p.stats_taken[c.stat] < 255) p.stats_taken[c.stat]++;
      break;
    default: break;
  }
}

}  // namespace ps

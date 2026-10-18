// sim.h — un bot que juega partidas enteras sin pantalla (para las pruebas
// y la herramienta de simulación). Se mueve huyendo de los enemigos y yendo
// a por lo que hay en el suelo, elige cartas al azar y coge los objetos.
#pragma once
#include <cmath>
#include <cstdint>
#include <functional>

#include "core/game.h"

namespace ps {

struct SimStats {
  double seconds = 0;
  int level = 0, kills = 0, bosses = 0, coins = 0;
  int max_enemies = 0, max_proj = 0, max_pickups = 0, max_fields = 0, max_hazards = 0, max_parts = 0, max_corpses = 0;
  int rifts = 0, cards = 0;
  bool judged = false, dead = false, nan = false;
  uint32_t checksum = 0;
};

inline bool finite_all(const Game& g) {
  const Player& p = g.players[0];
  if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.hp) || !std::isfinite(p.max_hp) || !std::isfinite(p.xp)) return false;
  for (const Enemy& e : g.enemies) if (!std::isfinite(e.x) || !std::isfinite(e.y) || !std::isfinite(e.hp) || !std::isfinite(e.kx)) return false;
  for (const Projectile& q : g.projectiles) if (!std::isfinite(q.x) || !std::isfinite(q.y) || !std::isfinite(q.vx)) return false;
  for (const Pickup& q : g.pickups) if (!std::isfinite(q.x) || !std::isfinite(q.y)) return false;
  return std::isfinite(g.camera.x) && std::isfinite(g.camera.y);
}

using SimHook = std::function<void(Game&)>;

// Juega hasta `max_seconds` de partida (o hasta que acabe).
inline SimStats sim_run(Game& g, uint32_t seed, int dex, double max_seconds, bool immortal, SimHook* hook = nullptr) {
  RunConfig cfg;
  cfg.dex = dex;
  cfg.seed = seed;
  cfg.judged = 1;
  g.debug_immortal = immortal;
  g.start_run(cfg);
  Rng bot(seed * 7919u + 13);
  SimStats st;
  int t = 0;
  float wander = bot.next() * kTau;
  while (g.state != ST_OVER && g.time < max_seconds && t < int(max_seconds * 60 * 3)) {
    ++t;
    Input in;
    if (g.state == ST_LEVELUP) {
      // Prefiere evoluciones, mejoras de ataques y ataques nuevos (no potenciadores).
      int pick = int(bot.next() * g.ncards), best_score = -1;
      for (int c = 0; c < g.ncards; ++c) {
        const Card& cd = g.cards[c];
        int score = 1;
        if (cd.type == CT_EVO) score = 9;
        else if (cd.type == CT_UP && g.pl().moves[cd.move].kind() != K_BUFF) score = 6;
        else if (cd.type == CT_NEW && kMoves[cd.move].kind != K_BUFF) score = 5;
        else if (cd.type == CT_STAT && cd.evo_for >= 0) score = 7;
        else if (cd.type == CT_STAT) score = 3;
        score = score * 10 + int(bot.next() * 10);
        if (score > best_score) { best_score = score; pick = c; }
      }
      g.pick_card(pick);
      st.cards++;
      continue;
    }
    if (g.state == ST_CHOICE) {
      if (g.choice == CH_ITEM) g.pick_item(bot.next() < 0.5f);
      else if (g.choice == CH_BOND) {
        int8_t loose[4]; int nl, grow;
        const int mode = player_bond_options(g.pl(), loose, nl, grow);
        int pick[4], n = 0;
        if (mode == 1) pick[n++] = loose[0];
        else if (mode == 2) { pick[n++] = loose[0]; pick[n++] = loose[1]; }
        g.pick_bond(pick, n);
      }
      continue;
    }
    if (g.judg.phase == 1 && bot.next() < 0.01f) g.skip_judgment();
    const Player& p = g.pl();
    // Huye del enemigo más cercano, va a por lo del suelo y deambula.
    float ax = std::cos(wander) * 0.6f, ay = std::sin(wander) * 0.6f;
    if (t % 240 == 0) wander += bot.range(-1.5f, 1.5f);
    float best = 1e30f;
    for (const Enemy& e : g.enemies) {
      const float d = dist2(p.x, p.y, e.x, e.y);
      if (d < best) { best = d; }
      if (d < 160 * 160 && d > 1) { ax -= (e.x - p.x) / std::sqrt(d) * 1.2f; ay -= (e.y - p.y) / std::sqrt(d) * 1.2f; }
    }
    float pb = 300 * 300;
    const Pickup* tgt = nullptr;
    for (const Pickup& q : g.pickups) { const float d = dist2(p.x, p.y, q.x, q.y); if (d < pb) { pb = d; tgt = &q; } }
    if (tgt) { const float d = std::sqrt(pb) + 1; ax += (tgt->x - p.x) / d; ay += (tgt->y - p.y) / d; }
    if (g.rift.open) { const float d = dist(p.x, p.y, g.rift.x, g.rift.y) + 1; ax += (g.rift.x - p.x) / d * 2; ay += (g.rift.y - p.y) / d * 2; }
    const float m = std::hypot(ax, ay);
    if (m > 1) { ax /= m; ay /= m; }
    in.ax = ax; in.ay = ay;
    // Hueco activo: el ataque de más nivel (los potenciadores no hacen daño).
    if (t % 120 == 0) {
      int bs = -1, bl = -1;
      for (int k = 0; k < p.nslots; ++k) {
        const MoveInst& mm = p.moves[p.slots[k][0]];
        const int score = (mm.kind() == K_BUFF ? 0 : 10) + mm.lvl + (p.slot_n[k] > 1 ? 20 : 0);
        if (score > bl) { bl = score; bs = k; }
      }
      if (bs >= 0 && bs != p.active_slot) in.select = int8_t(bs);
    }
    const bool was_arena = g.rift.arena;
    g.step(&in);
    if (hook) (*hook)(g);
    if (!was_arena && g.rift.arena) st.rifts++;
    st.max_enemies = std::max(st.max_enemies, g.enemies.n);
    st.max_proj = std::max(st.max_proj, g.projectiles.n);
    st.max_pickups = std::max(st.max_pickups, g.pickups.n);
    st.max_fields = std::max(st.max_fields, g.fields.n);
    st.max_hazards = std::max(st.max_hazards, g.hazards.n);
    st.max_parts = std::max(st.max_parts, g.fx.parts.n);
    st.max_corpses = std::max(st.max_corpses, g.corpses.n);
    if (g.judg.phase) st.judged = true;
    if ((t & 63) == 0 && !finite_all(g)) { st.nan = true; break; }
  }
  const Player& p = g.pl();
  st.seconds = g.time;
  st.level = p.level; st.kills = p.kills; st.bosses = p.bosses; st.coins = p.coins;
  st.dead = g.state == ST_OVER && g.summary.dead;
  // Huella del estado final (para comprobar que es determinista).
  uint32_t h = 2166136261u;
  auto mix = [&](uint32_t v) { h = (h ^ v) * 16777619u; };
  mix(uint32_t(p.level)); mix(uint32_t(p.kills)); mix(uint32_t(int(p.x * 16))); mix(uint32_t(int(p.y * 16)));
  mix(uint32_t(g.enemies.n)); mix(uint32_t(g.pickups.n)); mix(uint32_t(int(g.time * 60)));
  for (const Enemy& e : g.enemies) { mix(uint32_t(e.dex)); mix(uint32_t(int(e.x))); }
  st.checksum = h;
  return st;
}

}  // namespace ps

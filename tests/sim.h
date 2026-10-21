// sim.h — partidas enteras sin pantalla con el bot (pruebas y ps_sim).
#pragma once
#include <cmath>
#include <cstdint>
#include <functional>

#include "core/bot.h"
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
// `base`: la configuración de la partida (mejoras…); si no, la de serie.
inline SimStats sim_run(Game& g, uint32_t seed, int dex, double max_seconds, bool immortal, SimHook* hook = nullptr,
                        const RunConfig* base = nullptr) {
  RunConfig cfg = base ? *base : RunConfig();
  cfg.dex = dex;
  cfg.seed = seed;
  cfg.judged = 1;
  g.debug_immortal = immortal;
  g.start_run(cfg);
  Bot bot;
  bot.init(seed);
  SimStats st;
  int guard = 0;
  while (g.state != ST_OVER && g.time < max_seconds && guard++ < int(max_seconds * 60 * 3)) {
    Input in;
    if (!bot.decide(g, in)) continue;
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
    if ((guard & 63) == 0 && !finite_all(g)) { st.nan = true; break; }
  }
  st.cards = bot.cards;
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

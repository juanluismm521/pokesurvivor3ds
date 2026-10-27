// spawner.cpp — oleadas, jefes y shinies (spawner.js de la web).
// Los enemigos aparecen en un anillo justo fuera de la vista. Cada ~45 s hay
// una "marea": una avalancha desde un lado concreto. Cada enemigo tiene 1/4096
// de salir shiny. Al llegar al tope (64), los que siguen saliendo son "élite".
#include <cmath>
#include <cstdio>

#include "core/game.h"
#include "core/orbs.h"

namespace ps {

namespace {
constexpr float ELITE_FROM = 0.85f;
// Menos Pokémon pero más fuertes: salen ~24% de los de antes, cada uno con más
// vida, daño y experiencia.
constexpr float DENSITY = 0.24f, DMG_K = 2.4f, XP_K = 5.6f;
constexpr float SHINY_RATE = 1.0f / 4096;
// La resistencia extra crece con la run: ×3,3 al principio y hasta ×5 hacia el minuto 6.
float hp_k(float t) { return 3.3f + std::fmin(1.7f, t / 210); }

void ring_point(Game& g, const Player& p, float angle, float& x, float& y) {
  const float d = g.camera.outer_radius() + g.rng.range(10, 70);
  x = p.x + std::cos(angle) * d;
  y = p.y + std::sin(angle) * d * 0.9f;
}

// ¿Lo vería algún jugador? (sólo en grupo)
bool seen_by_anyone(Game& g, float x, float y) {
  const float r = g.camera.outer_radius() - 30;
  for (int i = 0; i < g.nplayers; ++i) if (g.players[i].active && dist2(g.players[i].x, g.players[i].y, x, y) < r * r) return true;
  return false;
}

// Punto del anillo donde quepa: no dentro de paredes ni en el agua.
bool free_point(Game& g, const Player& p, float angle, bool flying, float& ox, float& oy) {
  for (int i = 0; i < 8; ++i) {
    const float a = angle + (i == 0 ? 0 : g.rng.range(-0.9f, 0.9f));
    float x, y;
    ring_point(g, p, a, x, y);
    if (g.nplayers > 1 && seen_by_anyone(g, x, y)) continue;
    if (flying || g.world.is_free(x, y, 14)) { ox = x; oy = y; return true; }
  }
  return false;
}

// Jugador alrededor del que sale el siguiente enemigo.
Player& anchor(Game& g) {
  Player* alive[kMaxPlayers];
  int n = 0;
  // (nunca uno que aún vuelve de la grieta: rift_left_behind)
  for (int i = 0; i < g.nplayers; ++i) if (g.players[i].active && !g.players[i].dead && !rift_left_behind(g, g.players[i])) alive[n++] = &g.players[i];
  return n ? *alive[int(g.rng.next() * n)] : g.players[0];
}

bool roll_shiny(Game& g, int dex) {
  if (!has_shiny(dex)) return false;
  if (g.spawner.force_shiny > 0) { g.spawner.force_shiny--; return true; }
  return g.rng.next() < SHINY_RATE;
}

// Como mucho 3 curanderos, 3 escuderos y 3 invocadores vivos a la vez
// (18-10-2026: juntos dejaban a la horda, y a los jefes, casi inmortales).
// Si ya hay 3 de uno, sale otro del elenco.
constexpr int SUPPORT_MAX = 3;
bool is_support(int b) { return b == BH_HEALER || b == BH_SHIELDER || b == BH_SUMMONER; }
// Tipos de apoyo que ya tienen 3 vivos (bit por Behavior).
uint32_t support_full(Game& g) {
  int n[BH_COUNT] = {};
  uint32_t full = 0;
  for (const Enemy& e : g.enemies)
    if (!e.dead && is_support(e.behavior) && ++n[e.behavior] >= SUPPORT_MAX) full |= 1u << e.behavior;
  return full;
}

Enemy* spawn_one(Game& g, Player& p, float t, float angle, bool has_angle) {
  static const EnemyDef* bag[512];
  const int nb = enemies_bag_at(g, t, bag, 512);
  if (!nb) return nullptr;
  EnemyDef def = *bag[int(g.rng.next() * nb)];
  if (is_support(def.behavior)) {
    const uint32_t full = support_full(g);
    if ((full >> def.behavior) & 1u) {
      static const EnemyDef* rest[512];
      int nr = 0;
      for (int i = 0; i < nb; ++i) if (!((full >> bag[i]->behavior) & 1u)) rest[nr++] = bag[i];
      if (!nr) return nullptr;
      def = *rest[int(g.rng.next() * nr)];
    }
  }
  float x, y;
  if (!free_point(g, p, has_angle ? angle : g.rng.next() * kTau, hovers(def.dex), x, y)) return nullptr;
  const bool shiny = roll_shiny(g, def.dex);
  Scale3 sc = enemies_scale(t);
  sc.hp *= coop_scale_hp(g.team_size()) * hp_k(t);
  Enemy* e = enemy_spawn(g, def, x, y, sc);
  if (!e) return nullptr;
  e->dmg *= DMG_K;
  e->xp = float(jsround(e->xp * XP_K));
  if (g.enemies.n - 1 >= kEnemyCap * ELITE_FROM) {
    e->max_hp = e->hp = float(jsround(e->max_hp * 1.6f));
    e->dmg *= 1.25f;
    e->xp = float(jsround(e->xp * 1.6f));
  }
  if (shiny) enemy_make_shiny(g, *e);
  if (events_golden(g)) enemy_make_golden(*e);
  if (shiny) {
    char b[96];
    std::snprintf(b, sizeof(b), "¡Un %s shiny anda cerca!", mon_by_dex(e->dex)->name);
    g.say(b, 3.4f);
    g.sfx(SFX_SHINY);
  }
  return e;
}

void spawn_boss(Game& g, Player& p, float t, const EnemyDef& b) {
  float x, y;
  ring_point(g, p, g.rng.next() * kTau, x, y);
  Scale3 sc = enemies_boss_scale(t);
  sc.hp *= coop_scale_hp(g.team_size()) * (g.team_size() > 1 ? 1.4f : 1);
  Enemy* e = enemy_spawn(g, b, x, y, sc, true);
  if (!e) return;
  if (roll_shiny(g, b.dex)) enemy_make_shiny(g, *e);
  char s[96];
  std::snprintf(s, sizeof(s), "¡%s%s salvaje apareció!", mon_by_dex(e->dex)->name, e->shiny ? " shiny" : "");
  g.say(s, 3.2f);
  g.camera.kick(1.1f);
  g.sfx(SFX_BOSS);
}
}  // namespace

float spawner_rate(float t) { return (1.6f + t * 0.028f + std::pow(t / 60, 1.6f) * 0.5f) * DENSITY; }

void spawner_reset(Game& g) {
  g.spawner = Game::SpawnerState();
  enemies_roll_run(g);
}

void spawner_update(Game& g, float dt) {
  // En la arena de la grieta no sale nada más (y el reloj de jefes espera).
  // En la Sentencia de Arceus, sólo sus legendarios.
  if (g.rift.arena || judgment_active(g)) return;
  const float t = float(g.time);

  // --- jefes ---
  if (g.spawner.boss_idx < g.nbosses) {
    const EnemyDef& b = g.bosses[g.spawner.boss_idx];
    if (t >= b.at) { spawn_boss(g, anchor(g), t, b); g.spawner.boss_idx++; }
  }

  // Combate de jefe: las oleadas bajan al 25% y no hay mareas.
  const bool boss_fight = g.boss_alive();

  // --- goteo normal ---
  if (g.enemies.n < kEnemyCap) {
    g.spawner.acc += spawner_rate(t) * coop_scale_rate(g.team_size()) * (boss_fight ? 0.25f : 1) * dt;
    int n = int(std::floor(g.spawner.acc));
    g.spawner.acc -= n;
    while (n-- > 0 && g.enemies.n < kEnemyCap) spawn_one(g, anchor(g), t, 0, false);
  }

  // --- marea ---
  if (!boss_fight) g.spawner.surge_t -= dt;
  if (g.spawner.surge_t <= 0) {
    g.spawner.surge_t = g.rng.range(38, 56);
    const float base = g.rng.next() * kTau;
    Player& target = anchor(g);
    const int n = std::min(kEnemyCap - g.enemies.n, int(jsround((14 + std::floor(t / 22)) * coop_scale_rate(g.team_size()) * 0.4f)));
    for (int i = 0; i < n; ++i) spawn_one(g, target, t, base + g.rng.range(-0.55f, 0.55f), true);
    g.say("¡Marea de Pokémon!", 2.2f);
    g.sfx(SFX_SURGE);
  }
}

// Unos cuantos de golpe desde un lado (al empezar una horda).
void spawner_burst(Game& g, int n) {
  const float base = g.rng.next() * kTau;
  Player& target = anchor(g);
  for (int i = 0; i < n && g.enemies.n < kEnemyCap + 10; ++i) spawn_one(g, target, float(g.time), base + g.rng.range(-0.6f, 0.6f), true);
}

// Un rezagado vuelve a salir por delante del jugador (hacia donde corre),
// fuera de la vista. false si no hay sitio (entonces se quita).
bool spawner_ahead(Game& g, Enemy& e, Player& p) {
  const float sp = std::hypot(p.vx_, p.vy_);
  const float a0 = sp > 20 ? std::atan2(p.vy_, p.vx_) : g.rng.next() * kTau;
  float x, y;
  if (!free_point(g, p, a0 + g.rng.range(-0.9f, 0.9f), e.flying, x, y)) return false;
  e.x = x; e.y = y; e.kx = e.ky = 0;
  e.phase = PH_WALK; e.pt = 0; e.jump.on = false; e.jump_lift = 0;
  return true;
}

}  // namespace ps

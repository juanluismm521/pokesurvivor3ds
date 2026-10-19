// rift.cpp — grietas y arena del legendario (rift.js de la web).
// De vez en cuando se abre una GRIETA cerca del jugador. Dura 45 s; si entras,
// te lleva a una arena cerrada contra un Pokémon LEGENDARIO.
//   ganas    tickets y monedas, y un VÍNCULO (fusiona movimientos)
//   pierdes  vuelves sin premio, con algo de vida; la partida sigue
// Mientras dura la arena, los enemigos del mapa se guardan y no salen nuevos.
#include <cmath>
#include <initializer_list>
#include <cstdio>

#include "core/game.h"

namespace ps {

namespace {
// Cada vez que no sale, la siguiente es más probable (20 %, 40 %...): como
// mucho 2,5 min sin grieta. Desde 2 min antes de la Sentencia ya no se abren.
constexpr float CHECK = 30, CHANCE = 0.2f, TTL = 45;
constexpr float ARENA_R = 11, ARENA_TIME = 120, FROM = 75;
constexpr float TS = kTS;

void stash(Game& g) {
  g.stashed.clear();
  for (const Enemy& e : g.enemies) if (!e.dead && !g.stashed.full()) *g.stashed.push() = e;
  g.enemies.clear();
  g.corpses.clear();
  g.has_stash = true;
}

void restore(Game& g) {
  g.enemies.clear();
  g.corpses.clear();
  for (const Enemy& e : g.stashed) { Enemy* n = g.enemies.push(); if (n) *n = e; }
  g.stashed.clear();
  g.has_stash = false;
}

void show(Game& g, float x, float y, float ttl) {
  Game::RiftState& r = g.rift;
  r.open = true; r.x = x; r.y = y; r.ttl = ttl; r.t = 0;
  r.asked = false;
  g.sfx(SFX_RIFT);
  g.fx.ring(x, y, 6, 80, 0xb48aff, 0.7f, 5);
}

void open_rift(Game& g) {
  Player* alive[kMaxPlayers];
  int n = 0;
  for (int i = 0; i < g.nplayers; ++i) if (g.players[i].active && !g.players[i].dead) alive[n++] = &g.players[i];
  Player& near = n ? *alive[int(g.rng.next() * n)] : g.pl();
  for (int i = 0; i < 24; ++i) {
    const float a = g.rng.next() * kTau, d = g.rng.range(380, 560);
    const float x = near.x + std::cos(a) * d, y = near.y + std::sin(a) * d * 0.85f;
    if (!g.world.is_free(x, y, 22)) continue;
    show(g, x, y, TTL);
    return;
  }
}

void begin(Game& g) {
  Game::RiftState& r = g.rift;
  Player& pl = g.pl();
  r.ax = r.tx * TS + TS / 2; r.ay = r.ty * TS + TS / 2;
  r.back_x = pl.x; r.back_y = pl.y; r.back_biome = g.world.biome();
  r.at = 0; r.phase = 0; r.end_t = 0; r.boss_id = 0;
  r.arena = true;
  r.open = false; r.asked = false; r.down_t = 0;
  stash(g);
  g.projectiles.clear();
  combat_clear_fields(g);
  g.hazards.clear();
  const int th = arena_theme_index(kTypeId[r.type]);
  g.world.set_arena(true, r.tx, r.ty, ARENA_R, th >= 0 ? th : 0);
  g.world.set_biome(kArenaBiome0 + (th >= 0 ? th : 0), true);
  // Cada arena trae su clima (el volcán, sol; el glaciar, nieve...).
  if (th >= 0 && kArenaThemes[th].weather >= 0) weather_force(g, kArenaThemes[th].weather, ARENA_TIME + 15);
  // Todos al sur de la arena, en fila; el legendario aparece al norte.
  for (int i = 0; i < g.nplayers; ++i) {
    Player& p = g.players[i];
    p.x = r.ax + (i - (g.nplayers - 1) / 2.0f) * 34; p.y = r.ay + TS * 3;
  }
  g.views_snap();
  g.flash(0xc9a6ff, 0.9f);
  g.sfx(SFX_RIFT);
  g.say("Has entrado en la grieta…", 2.4f);
}

bool is_one_of(const Mon& m, std::initializer_list<int> ts) {
  for (int t : ts) if (m.types[0] == t || m.types[1] == t) return true;
  return false;
}

void spawn_legend(Game& g) {
  Game::RiftState& r = g.rift;
  const Mon& mon = *mon_by_dex(r.dex);
  const float t = float(g.time);
  EnemyDef def;
  def.dex = mon.dex;
  def.hp = float(jsround((3200 + t * 9) * 1 * (g.nplayers > 1 ? 1.4f : 1)));
  def.spd = float(jsround(clamp(48 + (mon.spd - 78) * 0.6f, 48.0f, 105.0f)));
  def.dmg = float(jsround(36 + t / 20));
  def.xp = float(jsround(500 + t * 1.2f));
  def.behavior = is_one_of(mon, {T_PSYCHIC, T_ELECTRIC, T_GHOST, T_FAIRY, T_FIRE, T_WATER, T_ICE, T_DRAGON}) ? BH_RANGED : BH_CHARGER;
  def.shot_dmg = float(jsround(18 + t / 40)); def.shot_cd = 0.9f; def.range = 300;
  Enemy* e = enemy_spawn(g, def, r.ax, r.ay - TS * 3.5f, Scale3(), true);
  if (!e) return;
  e->legend = true;
  // Choca con las paredes y columnas como tú (si volara, tus disparos no le alcanzarían).
  e->flying = false;
  if (r.shiny) enemy_make_shiny(g, *e);
  r.boss_id = e->id;
  char b[96];
  std::snprintf(b, sizeof(b), "¡%s%s te esperaba en la grieta!", mon.name, e->shiny ? " shiny" : "");
  g.say(b, 3.2f);
  g.camera.kick(1.1f);
  g.sfx(SFX_BOSS);
}

// Elige legendario y lugar.
void start_arena(Game& g) {
  if (g.rift.arena) return;
  static int16_t pool[128];
  int np = 0;
  for (int i = 0; i < kDexCount; ++i) if (kDex[i].leg && np < 128) pool[np++] = kDex[i].dex;
  // Los legendarios que aún no has vencido salen 4 veces más (menos repetidos).
  float ws[128], total = 0;
  for (int i = 0; i < np; ++i) {
    const int d = pool[i];
    const bool beaten = (g.cfg.legends_beaten[d >> 3] >> (d & 7)) & 1;
    ws[i] = beaten ? 1 : 4;
    total += ws[i];
  }
  float r = g.rng.next() * total;
  int mi = 0;
  while (mi < np - 1 && (r -= ws[mi]) > 0) mi++;
  const Mon& mon = *mon_by_dex(pool[mi]);
  Game::RiftState& R = g.rift;
  R.tx = 5000 + int32_t(std::floor(g.rng.next() * 3000));
  R.ty = 5000 + int32_t(std::floor(g.rng.next() * 3000));
  R.dex = mon.dex;
  R.shiny = has_shiny(mon.dex) && g.rng.next() < 1.0f / 512;
  // La arena es la de uno de sus tipos, al azar.
  const int nt = mon.types[1] >= 0 ? 2 : 1;
  R.type = mon.types[int(g.rng.next() * nt)];
  begin(g);
  spawn_legend(g);
}

void enter(Game& g) {
  if (g.rift.asked) return;
  g.rift.asked = true;
  start_arena(g);
}

void keep_inside(Game& g) {
  const Game::RiftState& r = g.rift;
  const float max = (ARENA_R - 1.4f) * TS;
  for (Enemy& e : g.enemies) {
    const float dx = e.x - r.ax, dy = e.y - r.ay, d = std::hypot(dx, dy);
    if (d > max) { e.x = r.ax + dx / d * max; e.y = r.ay + dy / d * max; }
  }
}

void won(Game& g) {
  Game::RiftState& r = g.rift;
  if (!r.arena || r.phase != 0) return;
  r.phase = 1; r.end_t = 5;
  progress_event(g, EV_RIFT, r.type);
  pickups_pull_all(g);
  char b[96];
  std::snprintf(b, sizeof(b), "¡Has vencido a %s! Recoge el premio…", mon_by_dex(r.dex)->name);
  g.say(b, 3.4f);
  g.sfx(SFX_LEGEND);
  Player& pl = g.pl();
  pl.bonds++;
  g.fx.dmg_text(pl.x, pl.y - pl.body_h - 10, "¡Vínculo!", 0xff8ad8, true);
  g.fx.ring(pl.x, pl.y, 8, 90, 0xff8ad8, 0.7f, 5);
}

void lost(Game& g) {
  Game::RiftState& r = g.rift;
  if (!r.arena || r.phase != 0) return;
  r.phase = 2; r.end_t = 1.2f;
  g.say("La grieta os ha expulsado…", 2.6f);
  g.sfx(SFX_BACK);
}

void finish(Game& g) {
  Game::RiftState& r = g.rift;
  Player& pl = g.pl();
  r.arena = false;
  g.world.set_arena(false);
  g.world.set_biome(r.back_biome, true);
  weather_clear(g);
  restore(g);
  g.projectiles.clear();
  combat_clear_fields(g);
  g.hazards.clear();
  for (int i = 0; i < g.nplayers; ++i) { g.players[i].x = r.back_x; g.players[i].y = r.back_y; }
  if (pl.dead) player_revive(g, pl, 0.3f);
  g.views_snap();
  g.flash(0xc9a6ff, 0.8f);
  // (Si hay vínculos, game.cpp ofrece la fusión en el siguiente paso.)
}

void arena_tick(Game& g, float dt) {
  Game::RiftState& r = g.rift;
  r.at += dt;
  if (r.phase == 0) {
    keep_inside(g);
    Enemy* boss = g.enemy_by_id(r.boss_id);
    if (!boss || boss->dead) { won(g); return; }
    bool all_down = true;
    for (int i = 0; i < g.nplayers; ++i) if (g.players[i].active && !g.players[i].dead) all_down = false;
    r.down_t = all_down ? r.down_t + dt : 0;
    if (r.down_t > 1.4f || r.at > ARENA_TIME) lost(g);
    return;
  }
  r.end_t -= dt;
  if (r.end_t <= 0) finish(g);
}
}  // namespace

void rift_reset(Game& g) { g.rift = Game::RiftState(); }

float rift_time_left(const Game& g) { return g.rift.arena ? std::fmax(0.0f, ARENA_TIME - g.rift.at) : 0; }

void rift_update(Game& g, float dt) {
  Game::RiftState& r = g.rift;
  Player& pl = g.pl();
  if (r.open) {
    r.t += dt;
    r.ttl -= dt;
    if (r.ttl <= 0) { r.open = false; return; }
    if (!r.arena && !pl.dead && dist2(pl.x, pl.y, r.x, r.y - 10) < 30 * 30) enter(g);
  } else if (!r.arena && g.time > FROM && g.time < kJudgmentAt - 120) {
    r.check_t -= dt;
    if (r.check_t <= 0) {
      r.check_t = CHECK;
      if (g.boss_alive()) return;           // con un jefe vivo no cuenta como intento
      if (g.rng.next() < CHANCE * (1 + r.misses)) { r.misses = 0; open_rift(g); }
      else r.misses++;
    }
  }
  if (r.arena) arena_tick(g, dt);
}

}  // namespace ps

// Cooperativo: reglas de coop.js y determinismo entre consolas (lockstep).
#include <cmath>
#include <memory>

#include "core/bot.h"
#include "core/game.h"
#include "core/halloween.h"
#include "core/save.h"
#include <doctest/doctest.h>
#include "sim.h"
#include <cstdio>

using namespace ps;

namespace {

RunConfig coop_cfg(uint32_t seed, int n) {
  static const int DEX[4] = {25, 4, 7, 133};
  RunConfig c;
  c.seed = seed;
  c.judged = 1;
  c.nplayers = n;
  for (int i = 0; i < n; ++i) {
    c.members[i].dex = DEX[i];
    std::snprintf(c.members[i].name, sizeof(c.members[i].name), "J%d", i + 1);
  }
  c.members[1].upgrades[0] = 3;   // mejoras distintas por jugador
  return c;
}

bool all_finite(const Game& g) {
  for (int i = 0; i < g.nplayers; ++i) {
    const Player& p = g.players[i];
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.hp) || !std::isfinite(p.xp)) return false;
  }
  return finite_all(g);
}

// Lo que "dibuja" cada consola: toca los chunks de alrededor de su jugador y,
// la segunda, además, chunks lejanos al azar (para que su caché descarte otros).
void touch_view(Game& g, int who, Rng* far) {
  const Player& p = g.players[who];
  const int32_t tx = World::tile_of(p.x), ty = World::tile_of(p.y);
  for (int dy = -8; dy <= 8; dy += 4)
    for (int dx = -14; dx <= 14; dx += 4) g.world.terrain_at(tx + dx, ty + dy);
  if (far)
    for (int k = 0; k < 3; ++k) g.world.terrain_at(int32_t(far->range(-4000, 4000)), int32_t(far->range(-4000, 4000)));
}

// Juega `secs` con n bots en dos consolas a la vez; devuelve el paso en que
// se separan (-1 si nunca).
int lockstep_pair(uint32_t seed, int n, double secs, bool immortal) {
  std::unique_ptr<Game> a(new Game()), b(new Game());
  const RunConfig cfg = coop_cfg(seed, n);
  a->local = 0; b->local = n - 1;
  a->fx.quality = 2; b->fx.quality = 0;   // New 3DS y vieja: los efectos no cuentan
  a->debug_immortal = b->debug_immortal = immortal;
  a->start_run(cfg);
  b->start_run(cfg);
  Bot bots[4];
  for (int i = 0; i < n; ++i) bots[i].init(seed + 31 * i);
  Rng far(seed ^ 77);
  for (int step = 0; a->state != ST_OVER && a->time < secs && step < int(secs * 60 * 3); ++step) {
    Input in[4];
    for (int i = 0; i < n; ++i) bots[i].decide(*a, in[i], i);
    a->step(in);
    b->step(in);
    touch_view(*a, 0, nullptr);
    touch_view(*b, n - 1, &far);
    if (step % 60 == 0 && a->state_hash() != b->state_hash()) return step;
  }
  return a->state_hash() == b->state_hash() ? -1 : 999999;
}

}  // namespace

TEST_CASE("cooperativo: escalado de dificultad y experiencia (coop.js)") {
  CHECK(coop_scale_rate(1) == 1.0f);
  CHECK(coop_scale_hp(1) == 1.0f);
  CHECK(coop_scale_rate(2) == doctest::Approx(1.75f));
  CHECK(coop_scale_hp(3) == doctest::Approx(1.6f));
  std::unique_ptr<Game> g(new Game());
  g->start_run(coop_cfg(5, 2));
  REQUIRE(g->nplayers == 2);
  CHECK(g->team_size() == 2);
  // Dos jugadores con el mismo bonus de XP: cada orbe da 1/1,6.
  g->players[1].xp_mul = g->players[0].xp_mul;
  CHECK(game_xp_factor(*g) == doctest::Approx(1 / 1.6f));
  // En fila, separados 34.
  CHECK(g->players[1].x - g->players[0].x == doctest::Approx(34));
}

TEST_CASE("cooperativo: cartas para todos y la partida sigue cuando eligen todos") {
  std::unique_ptr<Game> g(new Game());
  g->start_run(coop_cfg(9, 2));
  g->pending_levels = 1;
  Input in[2];
  g->step(in);
  REQUIRE(g->state == ST_LEVELUP);
  CHECK(g->players[0].ncards > 0);
  CHECK(g->players[1].ncards > 0);
  in[0].cmd = CMD_CARD; in[0].arg = 0;
  g->step(in);
  CHECK(g->state == ST_LEVELUP);   // falta el segundo
  CHECK(g->players[0].picked);
  in[0] = Input();
  in[1].cmd = CMD_CARD; in[1].arg = 0;
  g->step(in);
  CHECK(g->state == ST_PLAYING);
}

TEST_CASE("cooperativo: levantar al caído, fin al caer todos e irse") {
  std::unique_ptr<Game> g(new Game());
  g->start_run(coop_cfg(11, 2));
  Input in[2];
  for (int i = 0; i < 10; ++i) g->step(in);
  // El 2 cae; el 1 está al lado (a 34) y le levanta en 3 s.
  g->players[1].hp = 0; g->players[1].dead = true;
  g->players[1].x = g->players[0].x + 20; g->players[1].y = g->players[0].y;
  int t = 0;
  while (g->players[1].dead && t++ < 60 * 5) {
    g->players[1].x = g->players[0].x + 20; g->players[1].y = g->players[0].y;
    g->step(in);
  }
  CHECK(!g->players[1].dead);
  CHECK(t > 60 * 2.5);
  CHECK(g->state != ST_OVER);
  // Caen los dos: se acaba.
  for (int i = 0; i < 2; ++i) { g->players[i].hp = 0; g->players[i].dead = true; }
  g->players[1].x = g->players[0].x + 500;
  for (int i = 0; i < 60 * 2 && g->state != ST_OVER; ++i) g->step(in);
  CHECK(g->state == ST_OVER);
  // Se va un invitado: la partida sigue; se va el anfitrión: se acaba.
  g->start_run(coop_cfg(12, 3));
  Input in3[3];
  in3[2].cmd = CMD_QUIT;
  g->step(in3);
  CHECK(!g->players[2].active);
  CHECK(g->team_size() == 2);
  CHECK(g->state == ST_PLAYING);
  in3[2] = Input();
  in3[0].cmd = CMD_QUIT;
  g->step(in3);
  CHECK(g->state == ST_OVER);
}

TEST_CASE("cooperativo: dos consolas con las mismas entradas no se separan") {
  for (uint32_t seed : {3u, 21u}) {
    CAPTURE(seed);
    CHECK(lockstep_pair(seed, 2, 8 * 60, true) == -1);   // 8 min, inmortales
  }
  CHECK(lockstep_pair(5, 4, 6 * 60, false) == -1);        // 4 jugadores que pueden caer
}

TEST_CASE("cooperativo: 31 min con 3 bots, hasta la Sentencia, sin errores ni topes") {
  std::unique_ptr<Game> g(new Game());
  g->debug_immortal = true;
  g->start_run(coop_cfg(44, 3));
  Bot bots[3];
  for (int i = 0; i < 3; ++i) bots[i].init(44 + i);
  int max_en = 0, levels = 0, last_level = 1;
  bool judged = false;
  for (int step = 0; g->state != ST_OVER && g->time < 31 * 60 && step < 31 * 60 * 60 * 3; ++step) {
    Input in[3];
    for (int i = 0; i < 3; ++i) bots[i].decide(*g, in[i], i);
    g->step(in);
    max_en = std::max(max_en, g->enemies.n);
    if (g->judg.phase) judged = true;
    if (g->pl().level != last_level) { levels++; last_level = g->pl().level; }
    if ((step & 255) == 0) REQUIRE(all_finite(*g));
  }
  CHECK(max_en <= kMaxEnemies);
  CHECK(judged);
  CHECK(levels > 5);
  // Todos al mismo nivel (es del equipo).
  CHECK(g->players[1].level == g->players[0].level);
  CHECK(g->players[2].level == g->players[0].level);
}

// Modo rápido en grupo (01-11-2026, el dueño y Reddit: «a los invitados se les sale»):
// dos consolas, cada una con su jugador y SU partida guardada, en Halloween, hasta ganar
// al gran Gengar: no se separan nunca y las dos acaban en victoria.
TEST_CASE("cooperativo: modo rápido hasta la victoria, las consolas no se separan") {
  for (int hw = 0; hw < 2; ++hw) {
    CAPTURE(hw);
    std::unique_ptr<Game> a(new Game()), b(new Game());
    std::unique_ptr<Save> sa(new Save()), sb(new Save());
    RunConfig cfg = coop_cfg(9 + hw, 2);
    cfg.mode = MODE_QUICK; cfg.hw = uint8_t(hw); cfg.demo_time = kJudgmentAtQuick - 60;
    a->local = 0; b->local = 1;
    a->fx.quality = 2; b->fx.quality = 0;
    a->debug_immortal = b->debug_immortal = true;
    a->start_run(cfg, sa.get());
    b->start_run(cfg, sb.get());
    a->now_ms = 1000; b->now_ms = 2000;
    Bot bots[2];
    for (int i = 0; i < 2; ++i) bots[i].init(70 + i);
    int split = -1, step = 0;
    auto play = [&](int steps, auto until) {
      for (int k = 0; k < steps && !until(); ++k, ++step) {
        Input in[2];
        for (int i = 0; i < 2; ++i) bots[i].decide(*a, in[i], i);
        a->step(in); b->step(in);
        touch_view(*a, 0, nullptr); touch_view(*b, 1, nullptr);
        if (split < 0 && a->state_hash() != b->state_hash()) split = step;
      }
    };
    play(60 * 120, [&] { return a->judg.phase == 2; });
    REQUIRE(a->judg.phase == 2);
    play(60 * 60, [&] { return a->judg.boss_n >= kQuickLegends; });
    // Los legendarios caen (igual en las dos) y Gengar/Arceus baja la guardia.
    for (Game* g : {a.get(), b.get()})
      for (Enemy& e : g->enemies) if (e.boss && !e.arceus) { e.hp = 1; enemy_hurt(*g, e, 99999, 0xffffff); }
    play(60 * 20, [&] { return a->judg.open; });
    REQUIRE(a->judg.open);
    for (Game* g : {a.get(), b.get()}) { g->judg.ahp = 10; for (Enemy& e : g->enemies) if (e.arceus) enemy_hurt(*g, e, 50, 0xffffff); }
    play(60 * 30, [&] { return a->state == ST_OVER && b->state == ST_OVER; });
    CHECK(split == -1);
    REQUIRE(a->state == ST_OVER);
    REQUIRE(b->state == ST_OVER);
    CHECK(a->summary.victory); CHECK(b->summary.victory);
    // Los caramelos son del equipo: el invitado ve los mismos (antes, 0 en su pantalla).
    if (hw) { CHECK(a->players[0].candy >= kCandyJudge); CHECK(b->players[1].candy == a->players[0].candy); CHECK(a->summary.candy == b->summary.candy); }
    CHECK(sa->arceus_win[a->players[0].dex] > 0);
    CHECK(sb->arceus_win[b->players[1].dex] > 0);
  }
}

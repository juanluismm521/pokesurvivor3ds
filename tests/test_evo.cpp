// Evolución en plena partida a los niveles de los juegos (core/evo.h).
#include <doctest/doctest.h>

#include <memory>

#include "core/evo.h"
#include "core/game.h"

using namespace ps;

namespace {
std::unique_ptr<Game> run(int dex) {
  std::unique_ptr<Game> g(new Game());
  RunConfig c; c.dex = dex; c.seed = 5; c.judged = 1;
  g->start_run(c);
  return g;
}
}  // namespace

TEST_CASE("evolución: los niveles de los juegos (Charmander al 16, Charmeleon al 36)") {
  CHECK(evo_level(4, 5) == 16);
  CHECK(evo_level(5, 6) == 36);
  CHECK(evo_level(133, 134) == 0);   // (Vaporeon: piedra)
  CHECK(evo_prev(6) == 5);
  CHECK(evo_prev(4) == 0);
  int16_t nx[8];
  CHECK(evo_next(133, nx, 8) >= 8);  // Eevee
}

TEST_CASE("evolución: Charmander elige al 16 y al 36; si evoluciona, cambia de aspecto y de tipos") {
  auto g = run(4);
  Player& p = g->players[0];
  CHECK(evo_due_level(p) == 16);
  const float hp0 = p.max_hp;
  p.level = 15; evo_update(*g, p);
  CHECK(p.choice == CH_NONE);
  p.level = 16; evo_update(*g, p);
  REQUIRE(p.choice == CH_EVO);
  REQUIRE(p.evo_nopts == 1);
  CHECK(p.evo_opts[0] == 5);
  evo_choose(*g, p, 0);
  CHECK(p.dex == 5);
  CHECK(p.anim.dex == 5);
  CHECK(p.start_dex == 4);
  CHECK(p.max_hp > hp0);
  CHECK(g->state == ST_PLAYING);
  CHECK(evo_due_level(p) == 36);
  p.level = 36; evo_update(*g, p);
  REQUIRE(p.choice == CH_EVO);
  evo_choose(*g, p, 1);              // mantener su aspecto: sigue siendo Charmeleon, con sus stats
  CHECK(p.dex == 5);
  CHECK(p.mon->hp == mon_by_dex(5)->hp);
  CHECK(p.mon->types[1] == mon_by_dex(5)->types[1]);
  CHECK(evo_due_level(p) == 0);
}

TEST_CASE("evolución: Charizard empieza con la fuerza de Charmander y la gana sola") {
  auto g = run(6);
  Player& p = g->players[0];
  CHECK(p.mon->hp == mon_by_dex(4)->hp);
  CHECK(p.dex == 6);
  p.level = 16; evo_update(*g, p);
  CHECK(p.choice == CH_NONE);
  CHECK(p.mon->hp == mon_by_dex(5)->hp);
  p.level = 36; evo_update(*g, p);
  CHECK(p.mon->hp == mon_by_dex(6)->hp);
  CHECK(p.dex == 6);
}

TEST_CASE("evolución: Eevee, 3 al azar; uno que no evoluciona, 65 % → 82 % → 100 % (20 y 40)") {
  auto g = run(133);
  Player& p = g->players[0];
  CHECK(evo_due_level(p) == kEvoLv1);
  p.level = 20; evo_update(*g, p);
  REQUIRE(p.choice == CH_EVO);
  CHECK(p.evo_nopts == 3);
  evo_choose(*g, p, 2);
  CHECK(p.dex != 133);
  auto h = run(128);   // Tauros
  Player& t = h->players[0];
  CHECK(t.mon->hp == std::lround(mon_by_dex(128)->hp * 0.65));
  t.level = 40; evo_update(*h, t); evo_update(*h, t);
  CHECK(t.mon->hp == mon_by_dex(128)->hp);
}

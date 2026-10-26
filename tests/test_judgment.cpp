// La Sentencia de Arceus (web, 23-10-2026): se le puede pegar, lleva una Tabla
// y los legendarios crecen un 25 % (no el doble); nadie sale de la arena.
#include <doctest/doctest.h>

#include <cmath>
#include <memory>

#include "core/game.h"

using namespace ps;

namespace {
// Una partida que llega a la pelea de la Sentencia (reloj adelantado).
std::unique_ptr<Game> to_fight(uint32_t seed, int dex = 6) {
  std::unique_ptr<Game> g(new Game());
  g->debug_immortal = true;
  RunConfig c; c.dex = dex; c.seed = seed; c.judged = 1; c.demo_time = 1795;
  g->start_run(c);
  Input in{};
  for (int i = 0; i < 60 * 30 && g->judg.phase != 2; ++i) { in = Input{}; g->step(&in); }
  return g;
}
Enemy* arceus_target(Game& g) {
  for (Enemy& e : g.enemies) if (e.arceus) return &e;
  return nullptr;
}
}  // namespace

TEST_CASE("sentencia: Arceus lleva una Tabla y un blanco que recibe una parte del daño") {
  int plates[T_COUNT] = {};
  for (uint32_t seed = 1; seed <= 6; ++seed) {
    auto g = to_fight(seed);
    REQUIRE(g->judg.phase == 2);
    const int pl = g->judg.plate;
    CHECK(pl > T_NORMAL);
    CHECK(pl < T_COUNT);
    plates[pl]++;
    Enemy* a = arceus_target(*g);
    REQUIRE(a);
    CHECK(a->types[0] == pl);
    CHECK(a->dmg == 0);
    CHECK(!a->boss);
    // Va donde está Arceus.
    Input in{};
    g->step(&in);
    a = arceus_target(*g);
    CHECK(a->x == doctest::Approx(g->judg.ax));
    CHECK(a->y == doctest::Approx(g->judg.ay));
    // Un golpe: suma a la cuenta, no le baja la vida ni cae; los estados no le afectan.
    const double before = g->judg.dealt;
    const float hp = a->hp;
    enemy_hurt(*g, *a, 1000, 0xffffff);
    CHECK(g->judg.dealt == doctest::Approx(before + 1000));
    CHECK(a->hp == hp);
    CHECK(!a->dead);
    enemy_apply_burn(*a, 3); enemy_apply_poison(*a, 3); enemy_apply_stun(*a, 2); enemy_apply_slow(*a, 0.5f, 2);
    CHECK(a->burn == 0); CHECK(a->poison == 0); CHECK(a->stun_t == 0); CHECK(a->slow == 0);
  }
  int distinct = 0;
  for (int t = 0; t < T_COUNT; ++t) distinct += plates[t] > 0;
  CHECK(distinct >= 2);   // (no siempre la misma)
  // Recibe el 5 % y cada vez menos.
  auto g = to_fight(3);
  CHECK(judgment_dmg_k(*g) == doctest::Approx(0.05f).epsilon(0.05));
  g->judg.arena_t = 600;
  CHECK(judgment_dmg_k(*g) == doctest::Approx(0.01f));
}

TEST_CASE("sentencia: con legendarios vivos no se le apunta; sin ellos, sí") {
  auto g = to_fight(4);
  REQUIRE(g->judg.phase == 2);
  // Quitar a los legendarios que hubiera: sólo queda el blanco.
  for (int i = g->enemies.n - 1; i >= 0; --i) if (!g->enemies[i].arceus) g->enemies.remove_ordered(i);
  Input in{};
  g->judg.boss_t = 100;   // (que no acuda otro aún)
  g->step(&in);
  CHECK(!judgment_shielded(*g));
  Enemy* t = nearest_enemy(*g, g->judg.ax + 10, g->judg.ay, 400);
  REQUIRE(t);
  CHECK(t->arceus);
  // Acude un legendario: le protege.
  g->judg.boss_t = 0;
  g->step(&in);
  bool boss = false;
  for (const Enemy& e : g->enemies) boss = boss || (e.boss && !e.arceus);
  REQUIRE(boss);
  g->step(&in);   // (se mira al principio de cada paso, como en la web)
  CHECK(judgment_shielded(*g));
  Enemy* t2 = nearest_enemy(*g, g->judg.ax + 10, g->judg.ay, 400);
  CHECK((t2 == nullptr || !t2->arceus));
}

TEST_CASE("sentencia: cada legendario, +25 % de vida y +15 % de daño; nadie sale de la arena") {
  auto g = to_fight(5);
  REQUIRE(g->judg.phase == 2);
  Input in{};
  float hp[3] = {}, dm[3] = {};
  int got = 0;
  for (int i = 0; i < 60 * 20 && got < 3; ++i) {
    const int before = g->judg.boss_n;
    g->step(&in);
    if (g->judg.boss_n != before) {
      for (int k = g->enemies.n - 1; k >= 0; --k)
        if (g->enemies[k].boss && !g->enemies[k].arceus) { hp[got] = g->enemies[k].def.hp; dm[got] = g->enemies[k].def.dmg; got++; break; }
    }
  }
  REQUIRE(got == 3);
  CHECK(hp[1] / hp[0] == doctest::Approx(1.25f).epsilon(0.01));
  CHECK(hp[2] / hp[0] == doctest::Approx(1.5625f).epsilon(0.01));
  CHECK(dm[2] > dm[0]);
  CHECK(dm[2] < dm[0] * 1.5f);   // (antes, ×4)
  // Muy lejos del centro: vuelve dentro.
  Player& p = g->players[0];
  p.x = g->judg.cx + 5000; p.y = g->judg.cy;
  g->step(&in);
  CHECK(dist(p.x, p.y, g->judg.cx, g->judg.cy) < 24 * kTS);
}

TEST_CASE("sentencia: el daño a Arceus sale en el resumen y la partida es determinista") {
  auto a = to_fight(8), b = to_fight(8);
  REQUIRE(a->judg.phase == 2);
  CHECK(a->judg.plate == b->judg.plate);
  Input in{};
  for (int i = 0; i < 60 * 15; ++i) { Input i1{}, i2{}; a->step(&i1); b->step(&i2); }
  CHECK(a->state_hash() == b->state_hash());
  CHECK(a->judg.dealt == b->judg.dealt);
  a->quit_run();
  CHECK(a->summary.judge_dmg == doctest::Approx(a->judg.dealt));
  CHECK(a->summary.judge_plate == b->judg.plate);
  (void)in;
}

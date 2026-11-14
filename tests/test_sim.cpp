// test_sim.cpp — simulación sin pantalla: el bot juega partidas enteras con
// distintas semillas (con AddressSanitizer y UBSan). No puede haber cuelgues,
// NaN ni fugas, ni pasarse de los topes.
//   PS_SIM_SEEDS=20 PS_SIM_SECONDS=1800 ./ps_tests -tc="sim*"   (la larga)
#include <doctest/doctest.h>

#include <cstdio>
#include <cstdlib>
#include <memory>

#include "sim.h"

using namespace ps;

namespace {
int env_int(const char* k, int def) { const char* v = std::getenv(k); return v ? std::atoi(v) : def; }
}  // namespace

TEST_CASE("sim: misma semilla, misma partida") {
  std::unique_ptr<Game> a(new Game()), b(new Game());
  const SimStats sa = sim_run(*a, 1234, 4, 120, false);
  const SimStats sb = sim_run(*b, 1234, 4, 120, false);
  CHECK(sa.checksum == sb.checksum);
  CHECK(sa.kills == sb.kills);
  std::unique_ptr<Game> c(new Game());
  const SimStats sc = sim_run(*c, 4321, 4, 120, false);
  CHECK(sc.checksum != sa.checksum);
}

TEST_CASE("sim: partidas enteras sin errores ni topes superados") {
  const int seeds = env_int("PS_SIM_SEEDS", 3);
  const int secs = env_int("PS_SIM_SECONDS", 420);
  std::unique_ptr<Game> g(new Game());
  const int dexes[] = {4, 7, 1, 25, 133, 147, 92, 66, 74, 129, 63, 246, 443, 371, 610, 633, 152, 255, 393, 650};
  for (int i = 0; i < seeds; ++i) {
    const uint32_t seed = 1000 + i * 7777;
    const int dex = dexes[i % 20];
    // La mitad, inmortales (llegan a la Sentencia si dura lo bastante).
    const bool immortal = (i % 2) == 1;
    const SimStats s = sim_run(*g, seed, dex, secs, immortal);
    std::printf("[sim] semilla %u dex %d %s: %.0f s, nv %d, %d derrotados, %d jefes, %d grietas, enemigos máx %d, "
                "proyectiles máx %d, suelo máx %d, campos máx %d, avisos máx %d, partículas máx %d%s%s\n",
                seed, dex, immortal ? "inmortal" : "normal", s.seconds, s.level, s.kills, s.bosses, s.rifts, s.max_enemies,
                s.max_proj, s.max_pickups, s.max_fields, s.max_hazards, s.max_parts, s.judged ? ", Sentencia" : "", s.dead ? ", cae" : "");
    CHECK_FALSE(s.nan);
    CHECK(s.max_enemies <= kMaxEnemies);
    CHECK(s.max_proj < kMaxProjectiles);
    CHECK(s.max_pickups <= kMaxPickups);
    CHECK(s.max_fields < kMaxFields);
    CHECK(s.max_hazards < kMaxHazards);
    CHECK(s.max_corpses <= kMaxCorpses);
    CHECK(s.kills > 0);
    if (immortal) CHECK(s.seconds >= secs - 1);
    if (immortal && secs >= 1820) CHECK(s.judged);
  }
}

// Cada movimiento de la web (también las familias nuevas: persiguen, rebotan,
// cargan, giran, terremoto, remolino, empujón, aturdir) se usa un rato, al
// nivel 5 y evolucionado si puede: sin NaN ni topes superados, y los que
// hacen daño, lo hacen.
TEST_CASE("sim: cada movimiento de la web se usa sin errores") {
  std::unique_ptr<Game> g(new Game());
  int sin_dano = 0;
  for (int id = 0; id < kMoveCount; ++id) {
    bool armed = false;
    SimHook hook = [&](Game& gg) {
      Player& p = gg.pl();
      if (armed) { p.active_slot = 0; return; }
      armed = true;
      p.moves[0] = move_instance(id, 5);
      move_evolve(p.moves[0]);
      p.moves[0].since = float(gg.time);
      p.nmoves = 1; p.nslots = 1; p.slot_n[0] = 1; p.slots[0][0] = 0; p.active_slot = 0;
      p.max_moves = 1;   // (las cartas sólo mejoran: no aprende otros)
      proj_clear_orbs(gg, 0);
    };
    const SimStats s = sim_run(*g, 900 + uint32_t(id), 25, 35, true, &hook);
    CHECK_MESSAGE(!s.nan, kMoves[id].id);
    CHECK(s.max_proj < kMaxProjectiles);
    CHECK(s.max_fields < kMaxFields);
    if (!kind_is_status(kMoves[id].kind) && g->pl().moves[0].dealt <= 0) {
      std::printf("[sim] %s (%s) no ha hecho daño en 35 s\n", kMoves[id].id, kKindId[kMoves[id].kind]);
      sin_dano++;
    }
  }
  CHECK(sin_dano == 0);
}

// Mejora "Movimiento inicial" (starters.js: startChoices / startMoveOf).
TEST_CASE("movimiento inicial: el elegido con la mejora, y si no el de serie") {
  const int wp = move_index("whirlpool"), wg = move_index("water-gun");
  REQUIRE(wp >= 0);
  CHECK(move_base_start_for(130) == move_start_for(130));
  CHECK(move_start_for(130, wp, true) == wp);
  CHECK(move_start_for(130, wp, false) == move_base_start_for(130));   // sin la mejora comprada
  CHECK(move_start_for(25, wp, true) == move_base_start_for(25));      // no es de los suyos
  int16_t ch[kMaxMoves];
  const int n = move_start_choices(54, ch, kMaxMoves);                   // Psyduck: Confusión de serie aunque sea Agua
  bool base = false, buff = false;
  for (int i = 0; i < n; ++i) { if (ch[i] == move_base_start_for(54)) base = true; if (kMoves[ch[i]].kind == K_BUFF) buff = true; }
  CHECK(base);
  CHECK(!buff);
  std::unique_ptr<Game> g(new Game());
  RunConfig cfg; cfg.dex = 130; cfg.seed = 3; cfg.pstart = int16_t(wp); cfg.upgrades[UP_START] = 1;
  g->start_run(cfg);
  CHECK(g->pl().moves[0].def == wp);
  (void)wg;
}

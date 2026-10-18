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

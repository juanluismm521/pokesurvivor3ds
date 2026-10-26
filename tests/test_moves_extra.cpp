// Movimientos extra por Pokémon (del port): Darkrai aprende los de sus MT/tutor.
#include <doctest/doctest.h>

#include <memory>

#include "core/game.h"
#include "core/moves.h"

using namespace ps;

TEST_CASE("movimientos extra: Darkrai aprende más que su tipo; los demás, igual") {
  const Mon* dk = mon_by_dex(491);
  REQUIRE(dk);
  int16_t a[kMaxMoves], b[kMaxMoves];
  const int nt = move_pool_for(dk->types, a, kMaxMoves);
  const int nd = move_pool_for_dex(491, dk->types, b, kMaxMoves);
  CHECK(nt == 16);
  CHECK(nd == 24);
  auto has = [&](const char* id) { for (int i = 0; i < nd; ++i) if (b[i] == move_index(id)) return true; return false; };
  for (const char* id : {"shadow-ball", "ominous-wind", "psychic", "ice-beam", "thunderbolt", "thunder-wave", "sludge", "toxic", "dark-pulse"})
    CHECK_MESSAGE(has(id), id);
  // Absol (también sólo Siniestro) sigue como en la web.
  const Mon* ab = mon_by_dex(359);
  CHECK(move_pool_for_dex(359, ab->types, b, kMaxMoves) == move_pool_for(ab->types, a, kMaxMoves));
  // Repertorio: se puede elegir uno extra de segundo, y sale de segundo.
  const int st = move_start_for(491);
  const int n2 = move_second_choices(491, st, a, kMaxMoves);
  bool sb = false;
  for (int i = 0; i < n2; ++i) sb = sb || a[i] == move_index("shadow-ball");
  CHECK(sb);
  std::unique_ptr<Game> g(new Game());
  RunConfig c; c.dex = 491; c.seed = 4; c.judged = 1; c.upgrades[UP_MOVES] = 1; c.pmove2 = int8_t(move_index("shadow-ball"));
  g->start_run(c);
  CHECK(g->players[0].moves[1].def == move_index("shadow-ball"));
  // El inicial elegible sigue siendo el de la web (para la partida sincronizada).
  const int ns = move_start_choices(491, a, kMaxMoves);
  for (int i = 0; i < ns; ++i) CHECK(a[i] != move_index("shadow-ball"));
}

TEST_CASE("movimientos extra: a Darkrai le salen en las cartas de nivel") {
  std::unique_ptr<Game> g(new Game());
  int extra = 0;
  for (uint32_t seed = 1; seed <= 40 && !extra; ++seed) {
    RunConfig c; c.dex = 491; c.seed = seed; c.judged = 1;
    g->start_run(c);
    Player& p = g->players[0];
    for (int lv = 0; lv < 6; ++lv) {
      levelup_offer(*g, p);
      for (int k = 0; k < p.ncards; ++k) {
        const Card& cd = p.cards[k];
        if (cd.type != CT_NEW) continue;
        int16_t ex[16];
        const int ne = move_extras_for(491, ex, 16);
        for (int i = 0; i < ne; ++i) extra += cd.move == ex[i];
      }
    }
  }
  CHECK(extra > 0);
}

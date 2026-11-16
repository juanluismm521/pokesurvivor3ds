// El reclamo de cada partida para pokesurvivor.com (core/runclaim.h) y su cola: desde el 15-11-2026 el
// servidor sólo suma lo ganado si le llega así (si no, la sincronización lo quitaba).
#include <doctest/doctest.h>

#include <memory>

#include "core/game.h"
#include "core/runclaim.h"
#include "core/save.h"

using namespace ps;

TEST_CASE("reclamo de partida: lo ganado, los Pokémon conseguidos y la cola") {
  std::unique_ptr<Save> s(new Save());
  s->reset();
  RunConfig c; c.dex = 1030; c.seed = 5; c.judged = 1;   // (Sandslash de Alola: el número de la web)
  std::unique_ptr<Game> g(new Game());
  g->start_run(c, s.get());
  Player& p = g->pl();
  g->time = 420; p.kills = 500; p.bosses = 1; p.coins = 60; p.t1 = 2; p.level = 14;
  g->on_rescue(152);
  g->on_shiny_caught(1036);
  game_end_run(*g, true);
  const Json cl = run_claim(*g);
  CHECK(cl["time"].as_num() == doctest::Approx(420));
  CHECK(cl["kills"].as_num() == 500);
  CHECK(cl["coins"].as_num() == g->summary.coins);
  CHECK(cl["t1"].as_num() == 2);
  CHECK(cl["sdex"].as_num() == form_web_id(1030));
  CHECK(cl["sdex"].as_num() > 500000);
  CHECK(cl["dead"].as_bool());
  REQUIRE(cl["got"].is_obj());
  CHECK(cl["got"]["r"][0].as_num() == 152);
  CHECK(cl["got"]["w"][0].as_num() == form_web_id(1036));
  // La cola: al final, con su hora; como mucho kRunQueueMax (las más viejas fuera)
  Json q = Json::array();
  for (int i = 0; i < kRunQueueMax + 5; ++i) runq_push(q, cl, 1000 + i);
  CHECK(q.size() == size_t(kRunQueueMax));
  CHECK(q[0]["end"].as_num() == 1005);
  CHECK(q[q.size() - 1]["claim"]["kills"].as_num() == 500);
}

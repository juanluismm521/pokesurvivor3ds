// Maestría Pokémon (web, 13-11-2026: mastery.js): puntos por partida, premios, estadísticas
// por Pokémon y sincronización (el mayor de los dos, como db.js).
#include <doctest/doctest.h>

#include <memory>

#include "core/game.h"
#include "core/json.h"
#include "core/mastery.h"
#include "core/save.h"
#include "core/save_io.h"
#include "core/websave.h"

using namespace ps;

TEST_CASE("maestría: puntos de una partida y premios (mastery.js)") {
  CHECK(mastery_run_points(0, 0) == 0);
  CHECK(mastery_run_points(60, 600) == 10 + 250);      // 1 por cada 6 derrotados y 25 por minuto
  CHECK(mastery_run_points(5, 59) == 25);              // (0,83 + 24,58: se redondea hacia abajo, todo junto)
  CHECK(mastery_run_points(0, 5000) == 40 * 25);       // como mucho 40 minutos
  CHECK(mastery_run_points(-3, -10) == 0);
  CHECK(mastery_level(9999) == 0);
  CHECK(mastery_level(10000) == 1);
  CHECK(mastery_level(60000) == 3);
  CHECK(mastery_level(100000) == 4);
  CHECK(mastery_next(0) == MT_PHOTO);
  CHECK(mastery_next(30000) == MT_FRAME);
  CHECK(mastery_next(100000) == -1);
  CHECK(mastery_has(100000, MT_AURA));
  CHECK_FALSE(mastery_has(99999, MT_AURA));
  // Estadísticas: suman, salvo el mejor tiempo y el daño a Arceus (el mayor); con su tope
  PStat p;
  pstat_add(p, {1, 600, 300, 0, 600, 0, 2, 1, 0});
  pstat_add(p, {1, 300, 100, 1, 300, 5000, 1, 0, 1});
  CHECK(p.v[PS_R] == 2);
  CHECK(p.v[PS_S] == 900);
  CHECK(p.v[PS_K] == 400);
  CHECK(p.v[PS_BT] == 600);
  CHECK(p.v[PS_AD] == 5000);
  CHECK(p.v[PS_E] == 1);
  pstat_add(p, {0, 0, 0, 0, 1e9, 0, 0, 0, 0});
  CHECK(p.v[PS_BT] == 86400);
}

TEST_CASE("maestría: al acabar la partida suma a su Pokémon (y el desafío semanal no cuenta)") {
  std::unique_ptr<Save> s(new Save());
  s->reset();
  s->mastery[25] = 9900;
  RunConfig c; c.dex = 25; c.seed = 7; c.judged = 1;
  std::unique_ptr<Game> g(new Game());
  g->start_run(c, s.get());
  g->time = 600; g->pl().kills = 120; g->pl().bosses = 3; g->rifts_won = 1;
  game_end_run(*g, true);
  CHECK(g->summary.m_dex == 25);
  CHECK(g->summary.m_gained == 20 + 250);
  CHECK(s->mastery[25] == 9900 + 270);
  CHECK(g->summary.m_got == (1u << MT_PHOTO));   // ¡la foto!
  CHECK(s->pstat[25].v[PS_R] == 1);
  CHECK(s->pstat[25].v[PS_K] == 120);
  CHECK(s->pstat[25].v[PS_L] == 1);
  CHECK(s->pstat[25].v[PS_B] == 2);
  // El desafío semanal: nada
  c.weekly = 1;
  std::unique_ptr<Game> w(new Game());
  w->start_run(c, s.get());
  w->time = 600; w->pl().kills = 120;
  game_end_run(*w, true);
  CHECK(s->mastery[25] == 9900 + 270);
  CHECK(s->pstat[25].v[PS_R] == 1);
  CHECK(w->summary.m_dex == -1);
}

TEST_CASE("maestría: se guarda, se lee y al sincronizar gana el mayor de cada uno") {
  std::unique_ptr<Save> s(new Save());
  s->reset();
  s->mastery[25] = 500;
  s->mastery[1030] = 77;   // (una forma regional: va con el número de la web)
  s->pstat[25].v[PS_R] = 4; s->pstat[25].v[PS_BT] = 900;
  const Json j = save_full_json(*s);
  CHECK(j["mastery"]["25"].as_num() == 500);
  CHECK(j["pstat"]["25"]["r"].as_num() == 4);
  CHECK(j["pstat"]["25"]["bt"].as_num() == 900);
  CHECK_FALSE(j["mastery"].has("1030"));
  std::unique_ptr<Save> b(new Save());
  REQUIRE(save_from_json(*b, j.dump()));
  CHECK(b->mastery[25] == 500);
  CHECK(b->mastery[1030] == 77);
  CHECK(b->pstat[25].v[PS_BT] == 900);
  // La nube con más en uno y menos en otro: el mayor de cada uno
  Json nube = j;
  nube.ref("mastery").set("25", Json::num(800));
  nube.ref("pstat").ref("25").set("r", Json::num(2));
  nube.ref("pstat").ref("25").set("k", Json::num(50));
  const int64_t now = 1794100000000LL;
  Json local = web::migrate(j, "", now, false);
  const Json m = web::merge_safe(local, web::migrate(nube, "", now, false), now);
  CHECK(m["mastery"]["25"].as_num() == 800);
  CHECK(m["pstat"]["25"]["r"].as_num() == 4);
  CHECK(m["pstat"]["25"]["k"].as_num() == 50);
  CHECK(m["pstat"]["25"]["bt"].as_num() == 900);
  // Lo que no vale se va al sanear
  Json raro = j;
  raro.ref("pstat").ref("25").set("zz", Json::num(3));
  raro.ref("pstat").ref("25").set("k", Json::num(-5));
  const Json r = web::migrate(raro, "", now, true);
  CHECK_FALSE(r["pstat"]["25"].has("zz"));
  CHECK_FALSE(r["pstat"]["25"].has("k"));
}

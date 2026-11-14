// La partida de la web del 13-11-2026 («la Gran Actualización») al sincronizar: lo que
// la 3DS no conoce (maestría, huevos, estelas, fotos, marcos, dificultad, vistos…) no se
// pisa con la copia vieja de la consola, las MT borradas con la «MT vacía» no vuelven y
// lo que retira el servidor (rvk) se retira. Reglas: la fusión de la web de ese día.
#include <doctest/doctest.h>

#include <memory>
#include <string>

#include "core/json.h"
#include "core/save.h"
#include "core/save_io.h"
#include "core/websave.h"

using namespace ps;

namespace {
// Una cuenta de la web nueva, como la da GET /save (recortada).
Json gran_save() {
  Json s;
  REQUIRE(s.parse(R"({
    "v": 3, "name": "Ana", "created": 1794000000000, "updated": 1794000100000, "coins": 600,
    "owned": {"25": {"at": 1794000000100, "from": "starter"}, "150": {"at": 1794000000200, "from": "gacha"}, "10100": {"at": 1794000000300, "from": "gacha"}},
    "shiny": {}, "partner": 25, "partnerShiny": false, "starter": 25, "personality": null,
    "upgrades": {}, "pupg": {}, "pupgDone": true, "dfav": {}, "pused": {"25": 3},
    "mastery": {"25": 120}, "pstat": {"25": {"r": 3, "k": 400}}, "pstart": {}, "pstart2": {}, "pitem": {"25": "leftovers"},
    "look": {"25": "cosplay"}, "wear": {"25": {"at": 1794000000400, "fx": "aura"}}, "mode": "normal", "modeWins": {"normal": 1},
    "bag": {}, "tms": {}, "ptm": {"150": ["psychic", "shadow-ball"]}, "ptmOn": {"150": {"psychic": 1794000000500, "shadow-ball": 1794000000600}}, "ptmOff": {},
    "pnat": {}, "pabil": {}, "skins": {}, "pskin": {}, "trails": {"fuego": 1794000000700}, "fotos": {"25:feliz": 1}, "marcos": {},
    "ptrail": {"25": ["fuego", 1794000000800]},
    "tickets": {"t1": 0, "t10": 0}, "pity": {}, "legends": {}, "arenas": {}, "itemsFound": {}, "evoMoves": {}, "ach": {},
    "eggs": [{"id": "e1", "done": 2}, {"id": "e2", "done": 0}], "eggsHatched": {}, "eggGacha": {"1": 3}, "seen": "AQID", "beat": "AQ",
    "missions": {}, "profile": {"favs": [], "title": "", "photo": "25:feliz", "frame": ""},
    "stats": {"runs": 3, "bestTime": 300, "playSecs": 900},
    "led": {"base": {"c": [500, 0], "t1": [0, 0], "t10": [0, 0]}, "dweb": {"c": [100, 0]}}
  })"));
  return s;
}

// La consola lee la partida de la nube (como al acabar una sincronización).
std::unique_ptr<Save> consola_con(const Json& nube) {
  std::unique_ptr<Save> s(new Save());
  REQUIRE(save_from_json(*s, nube.dump()));
  save_set_raw(nube, *s);
  return s;
}
// Lo que sube la consola al sincronizar (como cloud::sync).
Json sincronizar(const Save& s, const Json& nube, int64_t now, std::string* lost = nullptr) {
  Json local = web::migrate(save_full_json(s), "", now, false);
  web::fold(local, "ctrprueba");
  const Json rem = web::migrate(nube, "", now, false);
  web::mark_taught(local, rem, now);
  const Json m = web::merge_safe(local, rem, now);
  std::string what;
  const bool pierde = web::loses_progress(rem, m, what) || web::loses_progress(local, m, what);
  if (lost) *lost = what;
  else CHECK_MESSAGE(!pierde, what);
  return m;
}
}  // namespace

TEST_CASE("gran actualización: sin cambios en ningún lado, la partida de la web queda idéntica") {
  // (migrada: con lo que la web rellena sola al leerla, como las estadísticas a 0)
  const Json nube = web::migrate(gran_save(), "", 1794000200000LL, false);
  auto s = consola_con(nube);
  const Json m = sincronizar(*s, nube, 1794000200000LL);
  for (const auto& kv : nube.items()) CHECK_MESSAGE(web::same(m[kv.first.c_str()], kv.second), kv.first << ": " << m[kv.first.c_str()].dump() << " / " << kv.second.dump());
  for (const auto& kv : m.items()) CHECK_MESSAGE(nube.has(kv.first.c_str()), kv.first << " (de más): " << kv.second.dump());
  save_raw() = Json();
}

TEST_CASE("gran actualización: lo que avanza en la web no lo pisa la copia vieja de la consola") {
  Json nube = gran_save();
  auto s = consola_con(nube);
  // La consola juega después (su copia parece la más nueva)…
  s->coins += 140;
  s->updated = 1794000900000LL;
  std::snprintf(s->title, sizeof(s->title), "%s", "Campeona");
  // …pero antes, en la web: maestría, un huevo abierto y otro nuevo, estela y foto compradas,
  // foto y marco del perfil, dificultad, vistos y otro aspecto.
  nube.set("updated", Json::num(1794000500000.0));
  nube.ref("mastery").set("25", Json::num(480));
  nube.ref("mastery").set("150", Json::num(60));
  { Json e; REQUIRE(e.parse(R"([{"id": "e2", "done": 5}, {"id": "e3", "done": 0}])")); nube.set("eggs", e); }
  nube.ref("eggsHatched").set("e1", Json::num(1794000400000.0));
  nube.ref("eggGacha").set("1", Json::num(7));
  nube.ref("trails").set("hielo", Json::num(1794000400000.0));
  nube.ref("marcos").set("oro", Json::num(1));
  nube.ref("profile").set("photo", Json::str("150:serio"));
  nube.ref("profile").set("frame", Json::str("oro"));
  nube.set("mode", Json::str("hard"));
  nube.ref("modeWins").set("normal", Json::num(2));
  nube.set("seen", Json::str("AQIDBAU"));
  nube.ref("look").set("25", Json::str("normal"));
  nube.ref("pstat").ref("25").set("k", Json::num(900));
  { Json w; REQUIRE(w.parse(R"({"at": 1794000450000, "fx": "chispas"})")); nube.ref("wear").set("25", w); }
  nube.ref("stats").set("playSecs", Json::num(2400));

  const Json m = sincronizar(*s, nube, 1794001000000LL);
  CHECK(m["mastery"]["25"].as_num() == 480);
  CHECK(m["mastery"]["150"].as_num() == 60);
  CHECK(m["eggs"].size() == 2);
  CHECK(m["eggs"][0]["id"].as_str() == "e2");
  CHECK(m["eggs"][0]["done"].as_num() == 5);
  CHECK(m["eggs"][1]["id"].as_str() == "e3");        // el huevo abierto (e1) no vuelve
  CHECK(m["eggsHatched"].has("e1"));
  CHECK(m["eggGacha"]["1"].as_num() == 7);
  CHECK(m["trails"].has("hielo"));
  CHECK(m["trails"].has("fuego"));
  CHECK(m["marcos"].has("oro"));
  CHECK(m["profile"]["photo"].as_str() == "150:serio");
  CHECK(m["profile"]["frame"].as_str() == "oro");
  CHECK(m["profile"]["title"].as_str() == "Campeona");   // lo de la consola, de la consola
  CHECK(m["mode"].as_str() == "hard");
  CHECK(m["modeWins"]["normal"].as_num() == 2);
  CHECK(m["seen"].as_str() == "AQIDBAU");
  CHECK(m["look"]["25"].as_str() == "normal");
  CHECK(m["pstat"]["25"]["k"].as_num() == 900);
  CHECK(m["wear"]["25"]["fx"].as_str() == "chispas");
  CHECK(m["stats"]["playSecs"].as_num() == 2400);
  CHECK(m["coins"].as_num() == 740);                 // y lo ganado en la consola, sumado
  CHECK(m["owned"].has("10100"));                    // una forma que la 3DS no conoce, sigue
  save_raw() = Json();
}

TEST_CASE("gran actualización: una MT borrada en la web con la MT vacía no vuelve desde la consola") {
  Json nube = gran_save();
  auto s = consola_con(nube);
  s->coins += 10;
  s->updated = 1794000900000LL;
  // En la web se borra Bola Sombra de Mewtwo.
  nube.set("updated", Json::num(1794000500000.0));
  { Json l = Json::array(); l.push(Json::str("psychic")); nube.ref("ptm").set("150", l); }
  nube.ref("ptmOff").ref("150") = Json::object();
  nube.ref("ptmOff").ref("150").set("shadow-ball", Json::num(1794000450000.0));
  const Json m = sincronizar(*s, nube, 1794001000000LL);
  REQUIRE(m["ptm"]["150"].size() == 1);
  CHECK(m["ptm"]["150"][0].as_str() == "psychic");
  // La consola la lee ya sin ella.
  auto s2 = consola_con(m);
  const Json j2 = save_full_json(*s2);
  CHECK(j2["ptm"]["150"].size() == 1);
  // Si después se la vuelve a enseñar en la consola, se queda (y se apunta cuándo).
  Json local = j2;
  { Json l = Json::array(); l.push(Json::str("psychic")); l.push(Json::str("shadow-ball")); local.ref("ptm").set("150", l); }
  local.set("updated", Json::num(1794002000000.0));
  web::mark_taught(local, m, 1794002000000LL);
  CHECK(local["ptmOn"]["150"]["shadow-ball"].as_num() == 1794002000000.0);
  const Json m2 = web::merge_safe(local, m, 1794002000000LL);
  CHECK(m2["ptm"]["150"].size() == 2);
  // Y una MT que se le enseña a otro por primera vez también lleva su apunte.
  Json local3 = j2;
  { Json l = Json::array(); l.push(Json::str("thunderbolt")); local3.ref("ptm").set("25", l); }
  web::mark_taught(local3, m, 1794002000000LL);
  CHECK(local3["ptmOn"]["25"]["thunderbolt"].as_num() == 1794002000000.0);
  CHECK(web::merge_safe(local3, m, 1794002000000LL)["ptm"]["25"].size() == 1);
  save_raw() = Json();
}

TEST_CASE("gran actualización: lo que retira el servidor (rvk) se retira también en la consola") {
  Json nube = gran_save();
  auto s = consola_con(nube);
  s->coins += 10;
  s->updated = 1794000900000LL;
  // El servidor retira a Mewtwo (conseguido en 1794000000200).
  nube.set("updated", Json::num(1794000500000.0));
  nube.ref("owned").remove("150");
  { Json r; REQUIRE(r.parse(R"({"owned": {"150": 1794000400000}})")); nube.set("rvk", r); }
  const Json m = sincronizar(*s, nube, 1794001000000LL);   // (y la red de seguridad no lo toma por una pérdida)
  CHECK(!m["owned"].has("150"));
  CHECK(m["owned"].has("25"));
  CHECK(m["rvk"]["owned"]["150"].as_num() == 1794000400000.0);
  // Si se vuelve a conseguir DESPUÉS, se queda.
  Json local = m;
  { Json e; REQUIRE(e.parse(R"({"at": 1794000950000, "from": "gacha"})")); local.ref("owned").set("150", e); }
  local.set("updated", Json::num(1794001500000.0));
  CHECK(web::merge_safe(local, m, 1794002000000LL)["owned"].has("150"));
  // Sin rvk, perder un Pokémon sigue siendo una pérdida.
  Json sin = m;
  sin.ref("owned").remove("25");
  std::string what;
  CHECK(web::loses_progress(m, sin, what));
  save_raw() = Json();
}

TEST_CASE("gran actualización: una consola recién estrenada no borra las misiones ni el nombre de la cuenta") {
  Json nube = gran_save();
  { Json m; REQUIRE(m.parse(R"({"day": "d20261114", "week": "w2026-46", "rerolled": true,
      "daily": [{"id": "survive", "goal": 7, "prog": 4, "told": false, "param": null, "claimed": false}],
      "weekly": [{"id": "wrift", "goal": 2, "prog": 1, "told": false, "param": null, "claimed": false}]})")); nube.set("missions", m); }
  // Partida nueva en la consola, creada DESPUÉS del último guardado de la web.
  std::unique_ptr<Save> s(new Save());
  new_save(*s, "Jugador", 1794000900000LL);
  save_raw() = Json();
  const Json local = save_full_json(*s);
  CHECK(local["name"].as_str() == "Jugador");
  const Json m = sincronizar(*s, nube, 1794001000000LL);
  CHECK(m["name"].as_str() == "Ana");
  CHECK(m["missions"]["day"].as_str() == "d20261114");
  REQUIRE(m["missions"]["daily"].size() == 1);
  CHECK(m["missions"]["daily"][0]["prog"].as_num() == 4);
  CHECK(m["missions"]["weekly"][0]["prog"].as_num() == 1);
  CHECK(m["missions"]["rerolled"].as_bool());
  CHECK(m["mastery"]["25"].as_num() == 120);
  CHECK(m["owned"].has("150"));

  // Una consola con las misiones de AYER guardada más tarde: se quedan las de hoy de la web,
  // y la semana (la misma) junta el progreso.
  Json vieja = nube;
  { Json mi; REQUIRE(mi.parse(R"({"day": "d20261113", "week": "w2026-46",
      "daily": [{"id": "coins", "goal": 300, "prog": 300, "told": true, "param": null, "claimed": true}],
      "weekly": [{"id": "wrift", "goal": 2, "prog": 2, "told": false, "param": null, "claimed": false}]})")); vieja.set("missions", mi); }
  vieja.set("updated", Json::num(1794000950000.0));
  const Json m2 = web::merge_safe(vieja, nube, 1794001000000LL);
  CHECK(m2["missions"]["day"].as_str() == "d20261114");
  CHECK(m2["missions"]["daily"][0]["id"].as_str() == "survive");
  CHECK(m2["missions"]["weekly"][0]["prog"].as_num() == 2);
  // Y al revés: si la consola ya tiene las de mañana, son las suyas.
  Json manana = vieja;
  manana.ref("missions").set("day", Json::str("d20261115"));
  CHECK(web::merge_safe(manana, nube, 1794001000000LL)["missions"]["daily"][0]["id"].as_str() == "coins");
  save_raw() = Json();
}

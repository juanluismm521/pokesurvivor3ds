// Mejora «Repertorio» (empiezas con 2 movimientos): el segundo se puede elegir (del port).
#include <doctest/doctest.h>

#include <memory>

#include "core/game.h"
#include "core/json.h"
#include "core/lockstep.h"
#include "core/moves.h"
#include "core/save.h"
#include "core/save_io.h"
#include "core/websave.h"

using namespace ps;

namespace {
bool has(const Player& p, int id) {
  for (int i = 0; i < p.nmoves; ++i) if (p.moves[i].def == id) return true;
  return false;
}
}  // namespace

TEST_CASE("repertorio: el segundo movimiento elegido, o al azar si no vale") {
  const int dex = 6;   // Charizard
  int16_t opc[kMaxMoves];
  const int st = move_start_for(dex);
  const int n = move_second_choices(dex, st, opc, kMaxMoves);
  REQUIRE(n >= 2);
  for (int i = 0; i < n; ++i) { CHECK(opc[i] != st); CHECK(kMoves[opc[i]].kind != K_BUFF); }
  std::unique_ptr<Game> g(new Game());
  for (int k = 0; k < n; ++k) {   // cada candidato, elegido, sale de segundo
    RunConfig c; c.dex = dex; c.seed = 11; c.judged = 1; c.upgrades[UP_MOVES] = 1; c.pmove2 = int8_t(opc[k]);
    g->start_run(c);
    const Player& p = g->players[0];
    CHECK(p.nmoves == 2);
    CHECK(p.moves[0].def == st);
    CHECK(p.moves[1].def == opc[k]);
  }
  // Sin elegir (o con uno que no vale: un potenciador, otro tipo, vetado): al azar, como en la web.
  int otro = -1;
  for (int i = 0; i < kMoveCount && otro < 0; ++i) {
    bool en = i == st;
    for (int k = 0; k < n; ++k) if (opc[k] == i) en = true;
    if (!en && kMoves[i].kind != K_BUFF) otro = i;
  }
  REQUIRE(otro >= 0);
  RunConfig base; base.dex = dex; base.seed = 11; base.judged = 1; base.upgrades[UP_MOVES] = 1;
  g->start_run(base);
  const int azar = g->players[0].moves[1].def;
  const uint32_t h = g->state_hash();
  for (int malo : {otro, st}) {
    RunConfig c = base; c.pmove2 = int8_t(malo);
    g->start_run(c);
    CHECK(g->players[0].moves[1].def == azar);   // la misma tirada que sin elegir
    CHECK(g->state_hash() == h);
  }
  RunConfig vet = base; vet.pmove2 = int8_t(opc[0]); vet.upgrades[UP_BAN] = 1; vet.pban[0] = int8_t(opc[0]);
  g->start_run(vet);
  CHECK(!has(g->players[0], opc[0]));
  // Sin la mejora, aunque haya uno elegido: sólo el inicial.
  RunConfig sin = base; sin.upgrades[UP_MOVES] = 0; sin.pmove2 = int8_t(opc[0]);
  g->start_run(sin);
  CHECK(g->players[0].nmoves == 1);
}

TEST_CASE("repertorio: se guarda, se sincroniza y viaja con la sala") {
  int16_t opc[kMaxMoves];
  REQUIRE(move_second_choices(6, move_start_for(6), opc, kMaxMoves) > 0);
  Save a;
  save_from_json(a, "{}");
  a.pmove2[6] = int8_t(opc[0]);
  Save b;
  REQUIRE(save_from_json(b, save_to_json(a)));
  CHECK(b.pmove2[6] == opc[0]);
  CHECK(b.pmove2[25] == -1);
  Save vacia; save_from_json(vacia, "{}");
  CHECK(save_to_json(vacia).find("pmove2") == std::string::npos);   // (sin elegir, la partida de la web queda igual)
  // La web no lo conoce: aunque su copia (más nueva) no lo traiga, no se pierde.
  Json mine, web;
  REQUIRE(mine.parse(save_to_json(a)));
  REQUIRE(web.parse(save_to_json(vacia)));
  web.set("updated", Json::num(mine["updated"].as_num() + 1000));
  const Json m = web::merge_safe(mine, web, int64_t(mine["updated"].as_num()) + 2000);
  CHECK(m["pmove2"]["6"].as_str() == std::string(kMoves[opc[0]].id));
  // En grupo: va con cada jugador.
  RunConfig c; c.seed = 5; c.nplayers = 2; c.members[0].dex = 6; c.members[1].dex = 25;
  c.members[0].pmove2 = int8_t(opc[0]); c.members[0].upgrades[UP_MOVES] = 1;
  uint8_t buf[1024];
  const int len = net::write_config(c, buf, sizeof(buf));
  REQUIRE(len > 0);
  RunConfig d;
  REQUIRE(net::read_config(buf, len, d));
  CHECK(d.members[0].pmove2 == opc[0]);
  CHECK(d.members[1].pmove2 == -1);
  std::unique_ptr<Game> g(new Game());
  g->start_run(d);
  CHECK(g->players[0].moves[1].def == opc[0]);
}

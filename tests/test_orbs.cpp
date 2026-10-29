// Esferas de Mundo Misterioso (orbs.js de la web, 23-10-2026; en la 3DS, A/B y tocar).
#include <doctest/doctest.h>

#include <cmath>
#include <cstdio>
#include <memory>

#include "core/game.h"
#include "core/orbs.h"
#include "sim.h"

using namespace ps;

namespace {
std::unique_ptr<Game> solo(uint32_t seed = 3, int dex = 25) {
  std::unique_ptr<Game> g(new Game());
  RunConfig c; c.dex = dex; c.seed = seed; c.judged = 1;
  g->start_run(c);
  g->enemies.clear();
  return g;
}
Enemy* put_enemy(Game& g, float x, float y, bool boss = false) {
  EnemyDef d; d.dex = 19; d.hp = 9999; d.spd = 60; d.dmg = 10; d.xp = 1; d.behavior = BH_CHASE;
  return enemy_spawn(g, d, x, y, Scale3(), boss);
}
void use(Game& g, int slot) {
  Input in{}; in.cmd = CMD_ORB; in.arg = int8_t(slot);
  g.step(&in);
}
void wait(Game& g, float secs) {
  for (int i = 0; i < int(secs * 60); ++i) { Input in{}; g.step(&in); }
}
}  // namespace

TEST_CASE("esferas: 4 huecos, hasta 3 de cada; si no cabe, no se coge") {
  auto g = solo();
  Player& p = g->players[0];
  for (int k = 0; k < 3; ++k) CHECK(orb_add(p, ORB_FOCUS));
  CHECK_FALSE(orb_add(p, ORB_FOCUS));          // (3 de la misma)
  CHECK(orb_add(p, ORB_SLOW));
  CHECK(orb_add(p, ORB_QUICK));
  CHECK(orb_add(p, ORB_SLUMBER));
  CHECK_FALSE(orb_can_take(p, ORB_BLOWBACK));  // (4 huecos llenos)
  CHECK(orb_can_take(p, ORB_SLOW));
  CHECK(orb_count(p) == 6);
  // Una esfera en el suelo no va hacia quien no la puede coger.
  Pickup* pk = g->pickups.push();
  *pk = Pickup(); pk->kind = PK_SPHERE; pk->value = ORB_BLOWBACK; pk->x = p.x + 30; pk->y = p.y;
  wait(*g, 1);
  bool still = false;
  for (const Pickup& q : g->pickups) still = still || (q.kind == PK_SPHERE && q.value == ORB_BLOWBACK);
  CHECK(still);
  // Gastar una deja el hueco; coger otra de la misma, vuelve a su hueco.
  const int slot = 0;
  REQUIRE(p.orb_id[slot] == ORB_FOCUS);
  use(*g, slot);
  CHECK(p.orb_n[slot] == 2);
}

TEST_CASE("esferas: Soporesfera duerme a los de la pantalla (a los jefes, 1 s) y no hacen daño") {
  auto g = solo();
  Player& p = g->players[0];
  Enemy* a = put_enemy(*g, p.x + 30, p.y);
  Enemy* far = put_enemy(*g, p.x + 2000, p.y);
  Enemy* b = put_enemy(*g, p.x - 200, p.y, true);
  const int32_t ida = a->id, idf = far->id, idb = b->id;
  orb_add(p, ORB_SLUMBER);
  use(*g, 0);
  CHECK(g->enemy_by_id(ida)->sleep_t > 3.5f);
  CHECK(g->enemy_by_id(idf)->sleep_t == 0);
  CHECK(g->enemy_by_id(idb)->sleep_t == doctest::Approx(1).epsilon(0.05));
  // Dormido encima de ti: no te hace daño.
  g->enemy_by_id(ida)->x = p.x; g->enemy_by_id(ida)->y = p.y;
  const float hp = p.hp;
  p.invuln = 0;
  wait(*g, 1);
  CHECK(p.hp == doctest::Approx(hp));
  wait(*g, 3.5f);
  CHECK(g->enemy_by_id(ida)->sleep_t <= 0);
}

TEST_CASE("esferas: Ralentosfera y Rafagaesfera") {
  auto g = solo();
  Player& p = g->players[0];
  Enemy* a = put_enemy(*g, p.x + 60, p.y);
  Enemy* b = put_enemy(*g, p.x - 60, p.y, true);
  const int32_t ida = a->id, idb = b->id;
  orb_add(p, ORB_SLOW);
  use(*g, 0);
  CHECK(g->enemy_by_id(ida)->slow == doctest::Approx(0.5f));
  CHECK(g->enemy_by_id(idb)->slow == doctest::Approx(0.2f));
  wait(*g, 1.1f);
  orb_add(p, ORB_BLOWBACK);
  const float d0 = dist(p.x, p.y, g->enemy_by_id(ida)->x, g->enemy_by_id(ida)->y);
  use(*g, 0);
  wait(*g, 0.5f);
  CHECK(dist(p.x, p.y, g->enemy_by_id(ida)->x, g->enemy_by_id(ida)->y) > d0 + 150);
}

TEST_CASE("esferas: las mejoras con tiempo, para ti y los compañeros cercanos; recarga de 1 s") {
  std::unique_ptr<Game> g(new Game());
  RunConfig c; c.seed = 9; c.judged = 1; c.nplayers = 3;
  c.members[0].dex = 25; c.members[1].dex = 4; c.members[2].dex = 7;
  g->start_run(c);
  g->enemies.clear();
  Player &a = g->players[0], &b = g->players[1], &cc = g->players[2];
  b.x = a.x + 100; b.y = a.y;
  cc.x = a.x + 2000; cc.y = a.y;
  const float ca = a.crit, cb = b.crit, cc0 = cc.crit;
  orb_add(a, ORB_FOCUS);
  orb_add(a, ORB_QUICK);
  Input in[3] = {};
  in[0].cmd = CMD_ORB; in[0].arg = 0;
  g->step(in);
  CHECK(a.crit == doctest::Approx(ca + 0.25f));
  CHECK(b.crit == doctest::Approx(cb + 0.25f));   // (cerca)
  CHECK(cc.crit == doctest::Approx(cc0));         // (lejos)
  // Recarga: otra en menos de 1 s no sale.
  in[0] = Input{}; in[0].cmd = CMD_ORB; in[0].arg = 1;
  g->step(in);
  CHECK(a.orb_id[1] == ORB_QUICK);
  for (int i = 0; i < 70; ++i) { Input z[3] = {}; g->step(z); }
  const float sm = a.spd_mul;
  in[0] = Input{}; in[0].cmd = CMD_ORB; in[0].arg = 1;
  g->step(in);
  CHECK(a.spd_mul == doctest::Approx(sm * 1.4f));
  // Se acaban solas y todo vuelve.
  for (int i = 0; i < 16 * 60; ++i) { Input z[3] = {}; g->step(z); }
  CHECK(a.crit == doctest::Approx(ca));
  CHECK(b.crit == doctest::Approx(cb));
  CHECK(a.spd_mul == doctest::Approx(sm));
}

TEST_CASE("esferas: la Luminosfera hace brillar un tesoro; al llegar, tickets y monedas") {
  auto g = solo(12);
  Player& p = g->players[0];
  orb_add(p, ORB_LUMINOUS);
  use(*g, 0);
  REQUIRE(g->treasure.on);
  const float d = dist(p.x, p.y, g->treasure.x, g->treasure.y);
  CHECK(d > 300);                  // (lejos: hay que ir a buscarlo)
  CHECK(d < 1100);
  CHECK(g->world.is_free(g->treasure.x, g->treasure.y, 16));
  // Ir hasta él: suelta tickets y monedas (aquí se recogen al momento) y se apaga.
  const int t1 = p.t1 + p.t10, c0 = p.coins;
  p.x = g->treasure.x; p.y = g->treasure.y;
  wait(*g, 1);
  CHECK_FALSE(g->treasure.on);
  CHECK(p.t1 + p.t10 >= t1 + 2);
  CHECK(p.coins >= c0 + 24);
  // Si nadie lo coge, a los 2 min se apaga.
  orb_add(p, ORB_LUMINOUS);
  orb_place_treasure(*g, p);
  REQUIRE(g->treasure.on);
  p.x += 3000;
  g->treasure.t = 119;
  treasure_update(*g, 2);
  CHECK_FALSE(g->treasure.on);
}

TEST_CASE("esferas: salen en partidas de verdad (jefes, cofres, a veces enemigos) y la partida es determinista") {
  std::unique_ptr<Game> g(new Game()), h(new Game());
  int seen = 0, used = 0;
  SimHook hook = [&](Game& gg) {
    static int last = 0;
    const int n = orb_count(gg.players[0]);
    if (n > last) seen += n - last;
    if (n < last) used += last - n;
    last = n;
  };
  const SimStats a = sim_run(*g, 77, 6, 900, true, &hook);
  const SimStats b = sim_run(*h, 77, 6, 900, true);
  MESSAGE("esferas cogidas " << seen << " · usadas " << used << " en " << a.seconds << " s");
  CHECK(seen > 0);
  CHECK(used > 0);
  CHECK(a.checksum == b.checksum);
  CHECK(g->state_hash() == h->state_hash());
}

TEST_CASE("esferas: la Curaesfera cura a todo el equipo, esté donde esté") {
  std::unique_ptr<Game> g(new Game());
  RunConfig c; c.seed = 4; c.judged = 1; c.nplayers = 2;
  c.members[0].dex = 25; c.members[1].dex = 4;
  g->start_run(c);
  g->enemies.clear();
  Player &a = g->players[0], &b = g->players[1];
  b.x = a.x + 5000;   // (muy lejos)
  a.hp = a.max_hp * 0.2f; b.hp = b.max_hp * 0.1f;
  orb_add(a, ORB_HEAL);
  Input in[2] = {};
  in[0].cmd = CMD_ORB; in[0].arg = 0;
  g->step(in);
  CHECK(a.hp >= a.max_hp * 0.59f);
  CHECK(b.hp >= b.max_hp * 0.49f);
  CHECK(orb_count(a) == 0);
}

TEST_CASE("esferas: el tesoro de la Luminosfera trae lo de un cofre grande y un objeto") {
  auto g = solo(21);
  Player& p = g->players[0];
  orb_add(p, ORB_LUMINOUS);
  use(*g, 0);
  REQUIRE(g->treasure.on);
  const float tx = g->treasure.x, ty = g->treasure.y;
  p.x = tx + 300; p.y = ty;   // (aún no lo coge)
  wait(*g, 0.1f);
  g->pickups.clear();
  // Llega: suelta objetos, esferas, tickets y monedas (se miran en el suelo).
  treasure_update(*g, 0);
  p.x = tx; p.y = ty;
  const int before = g->pickups.n;
  g->treasure.on = true;
  treasure_update(*g, 0.01f);
  int items = 0, spheres = 0, tickets = 0, coins = 0;
  for (const Pickup& q : g->pickups) {
    items += q.kind == PK_ITEM; spheres += q.kind == PK_SPHERE; coins += q.kind == PK_COIN;
    tickets += q.kind == PK_TICKET || q.kind == PK_TICKET10;
  }
  CHECK(g->pickups.n > before);
  CHECK(items >= 1);
  CHECK(spheres >= 1);
  CHECK(tickets >= 3);
  CHECK(coins >= 15);
}

TEST_CASE("vínculo: al unir dos movimientos sigues con el ataque que tenías elegido") {
  auto g = solo(5);
  Player& p = g->players[0];
  while (p.nmoves < 3) REQUIRE(player_add_move(*g, p, (p.nmoves * 7 + 3) % kMoveCount));
  // Elegido el 1.º; se unen el 2.º y el 3.º: no cambia.
  p.active_slot = 0;
  const int8_t sel = p.slots[0][0];
  int idx[2] = {p.slots[1][0], p.slots[2][0]};
  player_fuse(*g, p, idx, 2, -1);
  CHECK(p.slots[p.active_slot][0] == sel);
  // Elegido uno de los que se unen: sigues en él (el grupo que lo lleva).
  auto h = solo(6);
  Player& q = h->players[0];
  while (q.nmoves < 3) REQUIRE(player_add_move(*h, q, (q.nmoves * 5 + 11) % kMoveCount));
  q.active_slot = 2;
  const int8_t joined = q.slots[2][0];
  int idx2[2] = {q.slots[1][0], q.slots[2][0]};
  player_fuse(*h, q, idx2, 2, -1);
  bool in_group = false;
  for (int j = 0; j < q.slot_n[q.active_slot]; ++j) in_group = in_group || q.slots[q.active_slot][j] == joined;
  CHECK(in_group);
  CHECK(q.slot_n[q.active_slot] == 2);
}

// Esferas de Mundo Misterioso (orbs.js de la web, 23-10-2026; en la 3DS, A/B y tocar).
#include <doctest/doctest.h>

#include <cmath>
#include <cstdio>
#include <memory>

#include "core/game.h"
#include "core/orbs.h"
#include "sim.h"
#include "treasure_sim.h"

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
  CHECK(kTreasureTime == doctest::Approx(40));   // (el dueño, 2026-10-30)
  auto g = solo(12);
  Player& p = g->players[0];
  orb_add(p, ORB_LUMINOUS);
  use(*g, 0);
  REQUIRE(g->treasure.on);
  const float d = dist(p.x, p.y, g->treasure.x, g->treasure.y);
  CHECK(d > kTreasureMin * 0.6f - 1);   // (lejos: hay que correr a por él)
  CHECK(d < kTreasureMax + 1);
  CHECK(g->world.is_free(g->treasure.x, g->treasure.y, 16));
  // Ir hasta él: suelta tickets y monedas (aquí se recogen al momento) y se apaga.
  const int t1 = p.t1 + p.t10, c0 = p.coins;
  p.x = g->treasure.x; p.y = g->treasure.y;
  wait(*g, 1);
  CHECK_FALSE(g->treasure.on);
  CHECK(p.t1 + p.t10 >= t1 + 1);    // (los del cofre grande: 1-3)
  CHECK(p.coins >= c0 + 40);
  // Si nadie lo coge, a los 40 s se apaga (y no antes).
  orb_add(p, ORB_LUMINOUS);
  orb_place_treasure(*g, p);
  REQUIRE(g->treasure.on);
  p.x += 3000;
  g->treasure.t = kTreasureTime - 5;
  treasure_update(*g, 2);
  CHECK(g->treasure.on);
  g->treasure.t = kTreasureTime - 1;
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
  CHECK(a.hp >= a.max_hp * 0.49f);
  CHECK(b.hp >= b.max_hp * 0.39f);
  CHECK(b.hp < b.max_hp * 0.45f);   // (un 30 %, no más)
  CHECK(orb_count(a) == 0);
}

TEST_CASE("esferas: el tesoro de la Luminosfera trae lo de un cofre grande y monedas extra (sin pasarse)") {
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
  CHECK(items <= 1);                // (como mucho el del cofre grande, la mitad de las veces)
  CHECK(spheres >= 1);
  CHECK(tickets >= 1);
  CHECK(tickets <= 4);
  CHECK(coins >= 19);               // (9 del cofre grande y 10 extra)
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

// ---------------- cofres (del port, 2026-10-30) ----------------
namespace {
// El cofre más cercano de ese tipo (posición), buscando por el mapa.
bool find_prop(Game& g, int type, float& x, float& y, float& r) {
  const Player& p = g.players[0];
  float best = 1e30f;
  g.world.for_props_in_rect(p.x - 2500, p.y - 2500, p.x + 2500, p.y + 2500, [&](Prop& q) {
    if (q.type != type || q.gone) return;
    const float d = dist2(p.x, p.y, q.x, q.y);
    if (d < best) { best = d; x = q.x; y = q.y; r = q.r; }
  });
  return best < 1e30f;
}
bool prop_alive(Game& g, int type, float x, float y) {
  bool alive = false;
  g.world.for_props_in_rect(x - 4, y - 4, x + 4, y + 4, [&](Prop& q) { if (q.type == type && !q.gone && q.x == x && q.y == y) alive = true; });
  return alive;
}
}  // namespace

TEST_CASE("cofres: el pequeño no se abre a golpes, se abre al pasar; no frena los disparos") {
  for (uint32_t seed : {3u, 8u, 15u}) {
    auto g = solo(seed);
    Player& p = g->players[0];
    float cx, cy, cr;
    REQUIRE(find_prop(*g, PR_CHEST, cx, cy, cr));
    // A golpes no: ni con mucho daño.
    Prop* hit[8];
    const int nh = g->world.props_in_circle(cx, cy, 2, hit, 8);
    for (int i = 0; i < nh; ++i) if (hit[i]->type == PR_CHEST) g->world.damage_prop(hit[i], 1e6f, 0xffffff);
    CHECK(prop_alive(*g, PR_CHEST, cx, cy));
    // Lejos: sigue cerrado.
    p.x = cx + 200; p.y = cy;
    wait(*g, 0.2f);
    CHECK(prop_alive(*g, PR_CHEST, cx, cy));
    // Al tocarlo: se abre y suelta monedas.
    const int before = g->pickups.n;
    p.x = cx - (p.r + cr + 2); p.y = cy;
    wait(*g, 0.1f);
    CHECK_FALSE(prop_alive(*g, PR_CHEST, cx, cy));
    CHECK(g->pickups.n > before);
  }
}

TEST_CASE("cofres: el grande se abre a tu lado (un poco más lejos que en la web) y no se lleva los golpes") {
  auto g = solo(4);
  Player& p = g->players[0];
  float cx, cy, cr;
  REQUIRE(find_prop(*g, PR_BIGCHEST, cx, cy, cr));
  // No es un blanco: los disparos y golpes no lo tocan.
  Prop* hit[8];
  int n = g->world.props_in_circle(cx, cy, 4, hit, 8), bigs = 0;
  for (int i = 0; i < n; ++i) bigs += hit[i]->type == PR_BIGCHEST;
  CHECK(bigs == 0);
  // A la distancia que antes no llegaba (radio viejo de la web + 4): ahora sí, en 3,5 s.
  const float old_r = std::fmax(46.0f, cr + p.r + 24);
  p.x = cx - (old_r + 4); p.y = cy;
  g->debug_immortal = true;
  for (int i = 0; i < 4 * 60 && prop_alive(*g, PR_BIGCHEST, cx, cy); ++i) { p.x = cx - (old_r + 4); p.y = cy; Input in{}; g->step(&in); }
  CHECK_FALSE(prop_alive(*g, PR_BIGCHEST, cx, cy));
  // Pero no desde lejos (sin pasarse): a 90 no se abre.
  auto h = solo(4);
  Player& q = h->players[0];
  REQUIRE(find_prop(*h, PR_BIGCHEST, cx, cy, cr));
  h->debug_immortal = true;
  for (int i = 0; i < 5 * 60; ++i) { q.x = cx - 90; q.y = cy; Input in{}; h->step(&in); }
  CHECK(prop_alive(*h, PR_BIGCHEST, cx, cy));
}

TEST_CASE("esferas: el tesoro de la Luminosfera cae casi siempre lejos (desafiante, pero a tiempo)") {
  // En 40 tiradas: casi todas a kTreasureMin-kTreasureMax y, con el Pokémon más lento
  // (velocidad 80), siempre se llega en línea recta antes de los 40 s.
  auto g = solo(31);
  Player& p = g->players[0];
  int far = 0;
  for (int i = 0; i < 40; ++i) {
    orb_place_treasure(*g, p);
    const float d = dist(p.x, p.y, g->treasure.x, g->treasure.y);
    if (d >= kTreasureMin - 1) far++;
    CHECK(d / 80 < kTreasureTime);
    CHECK(g->world.is_free(g->treasure.x, g->treasure.y, 16));
  }
  CHECK(far >= 30);
}

TEST_CASE("esferas: el tesoro, equilibrado: en partidas de verdad se llega, pero no sobra tiempo") {
  // El bot juega hasta el minuto 4, usa la Luminosfera y va al tesoro rodeando
  // muros (treasure_sim.h; build-pc/ps_tesoro lo mide en más casos). Un Pokémon
  // normal (Hypno, velocidad 106) llega siempre y tarda más de 12 s (no es un
  // paseo); el más lento (Shuckle, 80), también llega.
  float sum = 0;
  int n = 0;
  for (uint32_t seed : {1097u, 1114u, 1131u}) {
    const TreasureTry r = treasure_try(seed, 97, 4);
    if (!r.placed) continue;
    CHECK(r.path);
    CHECK(r.reached);
    CHECK(r.secs > 12);
    sum += r.secs; n++;
  }
  REQUIRE(n >= 2);
  CHECK(sum / n < kTreasureTime * 0.65f);   // (le sobra margen para los enemigos)
  const TreasureTry slow = treasure_try(1213u, 213, 4);
  if (slow.placed) CHECK(slow.reached);
}

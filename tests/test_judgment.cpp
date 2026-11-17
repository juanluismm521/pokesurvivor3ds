// La Sentencia de Arceus (web, 23-10-2026): se le puede pegar, lleva una Tabla
// y los legendarios crecen un 25 % (no el doble); nadie sale de la arena.
#include <doctest/doctest.h>

#include <cmath>
#include <memory>

#include "core/game.h"
#include "core/bot.h"
#include "core/lockstep.h"
#include "core/save.h"
#include "core/save_io.h"

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

TEST_CASE("modo rápido: la Sentencia a los 20 minutos (y el modo va con la sala)") {
  for (int mode : {0, 1}) {
    std::unique_ptr<Game> g(new Game());
    g->debug_immortal = true;
    RunConfig c; c.dex = 25; c.seed = 2; c.judged = 1; c.demo_time = 1190; c.mode = uint8_t(mode);
    g->start_run(c);
    Bot bot; bot.init(3);
    for (int i = 0; i < 60 * 60 && g->time < 1215; ++i) { Input in{}; if (bot.decide(*g, in)) g->step(&in); }
    if (mode == 1) CHECK(g->judg.phase != 0);
    else CHECK(g->judg.phase == 0);
    CHECK(judgment_at(*g) == (mode == 1 ? 1200.0f : 1800.0f));
  }
  RunConfig c; c.seed = 5; c.nplayers = 2; c.members[0].dex = 6; c.members[1].dex = 25; c.mode = MODE_QUICK;
  uint8_t buf[1024];
  const int len = net::write_config(c, buf, sizeof(buf));
  RunConfig d;
  REQUIRE(net::read_config(buf, len, d));
  CHECK(d.mode == MODE_QUICK);
}

TEST_CASE("modo rápido: sus legendarios, Arceus baja la guardia y se le puede derrotar (medalla y premio)") {
  std::unique_ptr<Save> sv(new Save());
  std::unique_ptr<Game> g(new Game());
  g->debug_immortal = true;
  RunConfig c; c.dex = 6; c.seed = 21; c.judged = 1; c.demo_time = 1195; c.mode = MODE_QUICK;
  g->start_run(c);
  g->save = sv.get();
  g->now_ms = 123456;
  Bot bot; bot.init(5);
  for (int i = 0; i < 60 * 40 && g->judg.phase != 2; ++i) { Input in{}; if (bot.decide(*g, in)) g->step(&in); }
  REQUIRE(g->judg.phase == 2);
  CHECK(g->judg.can_win);
  // Acuden sólo kQuickLegends (aunque pase el tiempo) y mientras queden, no baja la guardia.
  for (int i = 0; i < 60 * 60; ++i) { Input in{}; if (bot.decide(*g, in)) g->step(&in); }
  CHECK(g->judg.boss_n == kQuickLegends);
  // Tumbados todos: baja la guardia, con vida de verdad.
  for (Enemy& e : g->enemies) if (e.boss && !e.arceus) { e.hp = 1; enemy_hurt(*g, e, 99999, 0xffffff); }
  for (int i = 0; i < 600 && !g->judg.open; ++i) { Input in{}; if (bot.decide(*g, in)) g->step(&in); }
  REQUIRE(g->judg.open);
  CHECK(g->judg.ahp_max > g->judg.base * 30);
  CHECK(judgment_dmg_k(*g) == 1.0f);   // (ahora recibe el daño entero)
  Enemy* a = nullptr;
  for (Enemy& e : g->enemies) if (e.arceus) a = &e;
  REQUIRE(a);
  const float hp0 = g->judg.ahp;
  enemy_hurt(*g, *a, 1000, 0xffffff);
  CHECK(g->judg.ahp == doctest::Approx(hp0 - 1000));
  // El golpe final: cae y, a los 4 s, victoria.
  g->judg.ahp = 10;
  enemy_hurt(*g, *a, 50, 0xffffff);
  CHECK(g->judg.won);
  for (int i = 0; i < 60 * 30 && g->state != ST_OVER; ++i) { Input in{}; if (bot.decide(*g, in)) g->step(&in); }
  REQUIRE(g->state == ST_OVER);
  CHECK(g->summary.victory);
  CHECK(!g->summary.dead);
  CHECK(g->summary.win_t10 == kWinT10);
  CHECK(sv->arceus_win[6] == 123456);
  CHECK(sv->arceus_medals() == 1);
  // La medalla se guarda y viaja por la nube sin perderse.
  Save b;
  REQUIRE(save_from_json(b, save_to_json(*sv)));
  CHECK(b.arceus_win[6] == 123456);
}

TEST_CASE("modo normal: Arceus sigue invencible (legendarios sin fin, sin guardia baja)") {
  std::unique_ptr<Game> g(new Game());
  g->debug_immortal = true;
  RunConfig c; c.dex = 6; c.seed = 21; c.judged = 1; c.demo_time = 1795;
  g->start_run(c);
  Bot bot; bot.init(5);
  for (int i = 0; i < 60 * 40 && g->judg.phase != 2; ++i) { Input in{}; if (bot.decide(*g, in)) g->step(&in); }
  REQUIRE(g->judg.phase == 2);
  CHECK_FALSE(g->judg.can_win);
  for (int i = 0; i < 60 * 70; ++i) { Input in{}; if (bot.decide(*g, in)) g->step(&in); }
  for (Enemy& e : g->enemies) if (e.boss && !e.arceus) enemy_hurt(*g, e, 1e9f, 0xffffff);
  for (int i = 0; i < 30; ++i) { Input in{}; if (bot.decide(*g, in)) g->step(&in); }
  CHECK(g->judg.boss_n > kQuickLegends);
  CHECK_FALSE(g->judg.open);
  CHECK(judgment_dmg_k(*g) < 0.06f);
}

TEST_CASE("modo rápido: en la Sentencia la velocidad baja menos que en el normal (Reddit, 01-11-2026)") {
  // Lo de la web en el modo normal, igual que siempre.
  CHECK(judgment_cap_spd(120) == doctest::Approx(120));
  CHECK(judgment_cap_spd(310) == doctest::Approx(175));
  // Modo rápido: hasta 170 nada; lo que pase, la mitad. Un Pokémon rápido con
  // todo (310) se queda en 240 (-23 %) en vez de en 175 (-44 %).
  CHECK(judgment_cap_spd_quick(160) == doctest::Approx(160));
  CHECK(judgment_cap_spd_quick(310) == doctest::Approx(240));
  for (float s = 0; s < 600; s += 7) CHECK(judgment_cap_spd_quick(s) >= judgment_cap_spd(s));
  // Y es lo que usa la partida.
  for (int mode = 0; mode < 2; ++mode) {
    std::unique_ptr<Game> g(new Game());
    g->debug_immortal = true;
    RunConfig c; c.dex = 6; c.seed = 4; c.judged = 1; c.mode = PlayMode(mode);
    c.demo_time = (mode == MODE_QUICK ? kJudgmentAtQuick : kJudgmentAt) - 2;
    g->start_run(c);
    for (int i = 0; i < 60 * 30 && g->judg.phase != 2; ++i) { Input in{}; g->step(&in); }
    REQUIRE(g->judg.phase == 2);
    Player& p = g->players[0];
    p.spd_mul = 310.0f / p.base_spd;
    CHECK(player_spd(*g, p) == doctest::Approx(mode == MODE_QUICK ? 240 : 175));
  }
}

// El fallo de la 0.35.0 (01-11-2026, fallo.log de un invitado y Reddit: «al ganar en el
// rápido se cierra»): el golpe final de un DISPARO llamaba a judgment_hit_arceus desde
// dentro de combat_resolve, que vaciaba la lista de disparos mientras se recorría; luego
// se quitaba «su» disparo y la lista quedaba con -1. Al dibujar: data abort.
TEST_CASE("modo rápido: ganar con el golpe final de un disparo no rompe las listas") {
  for (int hw = 0; hw < 2; ++hw)
    for (int extra = 0; extra < 3; ++extra) {   // (y con otros disparos en la lista, delante y detrás)
      CAPTURE(hw); CAPTURE(extra);
      std::unique_ptr<Game> g(new Game());
      g->debug_immortal = true;
      RunConfig c; c.dex = 25; c.seed = 21 + extra; c.judged = 1; c.demo_time = 1195; c.mode = MODE_QUICK; c.hw = uint8_t(hw);
      g->start_run(c);
      Bot bot; bot.init(5);
      for (int i = 0; i < 60 * 40 && g->judg.phase != 2; ++i) { Input in{}; if (bot.decide(*g, in)) g->step(&in); }
      REQUIRE(g->judg.phase == 2);
      for (int i = 0; i < 60 * 60 && g->judg.boss_n < kQuickLegends; ++i) { Input in{}; if (bot.decide(*g, in)) g->step(&in); }
      for (Enemy& e : g->enemies) if (e.boss && !e.arceus) { e.hp = 1; enemy_hurt(*g, e, 99999, 0xffffff); }
      for (int i = 0; i < 600 && !g->judg.open; ++i) { Input in{}; if (bot.decide(*g, in)) g->step(&in); }
      REQUIRE(g->judg.open);
      // Cuando la partida está en marcha y no le protege nadie: disparos (el que remata,
      // encima de su blanco, y otros lejos) y un paso.
      bool shot = false;
      for (int tries = 0; tries < 60 * 20 && !shot; ++tries) {
        Input in{};
        for (Enemy& e : g->enemies) if (e.boss && !e.arceus) { e.hp = 1; enemy_hurt(*g, e, 99999, 0xffffff); }
        Enemy* a = nullptr;
        for (Enemy& e : g->enemies) if (e.arceus) a = &e;
        REQUIRE(a);
        if (g->state != ST_PLAYING || judgment_shielded(*g)) { if (bot.decide(*g, in)) g->step(&in); continue; }
        g->judg.ahp = 5;
        g->projectiles.clear();
        for (int k = 0; k <= extra; ++k) {
          Projectile* p = proj_spawn(*g);
          REQUIRE(p);
          p->friendly = true; p->owner = 0; p->dmg = 9999; p->r = 10; p->life = 5; p->vx = p->vy = 0;
          p->x = k == 0 ? g->judg.ax : g->judg.ax + 3000; p->y = k == 0 ? g->judg.ay : g->judg.ay + 3000;
        }
        g->step(&in);
        shot = true;
      }
      REQUIRE(shot);
      CHECK(g->judg.won);
      CHECK(g->projectiles.n >= 0);
      CHECK(g->projectiles.n == 0);   // (al final del paso, sin disparos ni enemigos salvo él)
      CHECK(g->hazards.n >= 0);
      for (const Enemy& e : g->enemies) CHECK(e.arceus);
      for (int i = 0; i < 60 * 10 && g->state != ST_OVER; ++i) { Input q{}; if (bot.decide(*g, q)) g->step(&q); CHECK(g->projectiles.n >= 0); }
      CHECK(g->state == ST_OVER);
      CHECK(g->summary.victory);
    }
}

TEST_CASE("listas: quitar un índice que ya no está no hace nada (nunca n < 0)") {
  FixedList<int, 4> l;
  *l.push() = 1;
  l.clear();
  l.remove_ordered(0);
  CHECK(l.n == 0);
  l.remove_swap(3);
  CHECK(l.n == 0);
  *l.push() = 7; *l.push() = 8;
  l.remove_ordered(5);
  CHECK(l.n == 2);
  l.remove_ordered(0);
  CHECK(l.n == 1);
  CHECK(l[0] == 8);
}

// Modo rápido (17-11-2026, «con Mewtwo al máximo no se puede ganar»): Arceus te alcanzaba
// corriendo (hasta el 122 % de tu velocidad, ×1,15 enfurecido) y su toque del 40 % se repetía
// cada 0,9 s: tres toques en tres segundos. Ahora no corre más que tú, toca el 25 % y retrocede.
TEST_CASE("modo rápido: Arceus no te alcanza corriendo y su toque no se encadena") {
  std::unique_ptr<Game> g(new Game());
  RunConfig c; c.dex = 150; c.seed = 33; c.judged = 1; c.demo_time = 1195; c.mode = MODE_QUICK;
  for (int u = 0; u < UP_COUNT; ++u) c.upgrades[u] = kUpgrades[u].max;
  g->debug_immortal = true;
  g->start_run(c);
  for (int i = 0; i < 60 * 40 && g->judg.phase != 2; ++i) { Input in{}; g->step(&in); }
  REQUIRE(g->judg.phase == 2);
  REQUIRE(g->judg.can_win);
  Player& p = g->players[0];
  // Velocidad: el jugador quieto lejos; lo que avanza Arceus por segundo, nunca más que tú
  // (a los 60 s de Sentencia y enfurecido, cuando más corría).
  for (int pass = 0; pass < 2; ++pass) {
    g->judg.arena_t = 60 + pass * 200;
    g->judg.open = pass == 1;
    g->judg.acast = -1; g->judg.acd = 99; g->judg.touch_cd = 99;
    p.x = g->judg.cx + 300; p.y = g->judg.cy; g->judg.ax = g->judg.cx - 300; g->judg.ay = g->judg.cy;
    const float x0 = g->judg.ax;
    const float mine = player_spd(*g, p);
    for (int i = 0; i < 30; ++i) { g->judg.acd = 99; g->judg.acast = -1; p.x = g->judg.cx + 300; p.y = g->judg.cy; Input in{}; g->step(&in); }
    const float per_s = (g->judg.ax - x0) * 2;
    CHECK(per_s <= mine * 0.96f);
    CHECK(per_s > mine * 0.5f);   // (pero te persigue)
  }
  // Toque: el 25 % y no se repite enseguida (retrocede).
  g->judg.open = false; g->judg.arena_t = 60; g->judg.touch_cd = 0; g->judg.acd = 99; g->judg.acast = -1;
  g->debug_immortal = false;
  p.hp = p.max_hp; p.invuln = 0; p.guard_t = 0;
  g->judg.ax = p.x; g->judg.ay = p.y;
  int touches = 0; float last = p.hp;
  for (int i = 0; i < 60; ++i) {   // (1 s pegado a él: antes, dos toques)
    g->judg.acd = 99; g->judg.acast = -1;
    Input in{}; g->step(&in);
    if (p.hp < last - p.max_hp * 0.2f) { touches++; CHECK((last - p.hp) / p.max_hp == doctest::Approx(0.25f).epsilon(0.05)); }
    last = p.hp;
    p.invuln = 0;
  }
  CHECK(touches == 1);
}

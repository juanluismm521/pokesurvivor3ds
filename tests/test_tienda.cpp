// Tienda Kecleon (web, 20-10-2026): Mentas, Cápsulas, MT, naturalezas y habilidades.
// Lo que se compara con la web va en test_golden.cpp (sorteos, MT del día, fusión).
#include <doctest/doctest.h>

#include <cmath>
#include <cstring>
#include <memory>
#include <string>

#include "core/abilities.h"
#include "core/game.h"
#include "core/json.h"
#include "core/lockstep.h"
#include "core/mons.h"
#include "core/moves.h"
#include "core/save.h"
#include "core/save_io.h"
#include "core/websave.h"
#include "sim.h"

using namespace ps;

namespace {
int abil_index(const char* id) {
  for (int i = 0; i < kAbilCount; ++i) if (!std::strcmp(kAbils[i].id, id)) return i;
  return -1;
}
int nature_index(const char* id) {
  for (int i = 0; i < kNatureCount; ++i) if (!std::strcmp(kNatures[i].id, id)) return i;
  return -1;
}
// Un Pokémon que tenga esa habilidad (en cualquiera de sus huecos).
int dex_with(int abil) {
  for (int d = 1; d <= kMaxDex; ++d) {
    if (!mon_by_dex(d)) continue;
    for (int k = 0; k < 3; ++k) if (kDexAbil[d][k] == abil) return d;
  }
  return 0;
}
int tm_of(const char* move) { return tm_index_of_move(move_index(move)); }

// Una partida con ese Pokémon y habilidad (y naturaleza), lista para tocarla.
std::unique_ptr<Game> run_with(int dex, int abil, int nature = -1, uint32_t seed = 5) {
  std::unique_ptr<Game> g(new Game());
  RunConfig cfg;
  cfg.dex = dex; cfg.seed = seed; cfg.judged = 1;
  cfg.abil = int16_t(abil); cfg.nature = int8_t(nature);
  g->start_run(cfg);
  return g;
}
}  // namespace

TEST_CASE("tienda: comprar Mentas, Cápsulas y las MT de hoy, y gastarlas") {
  std::unique_ptr<Save> s(new Save());
  s->created = 1700000000000LL;
  s->owned_at[25] = 1; s->partner = 25;
  s->coins = 5000;
  CHECK(shop_buy_item(*s, BAG_MINT) == BUY_OK);
  CHECK(s->coins == 3500);
  CHECK(s->bag[BAG_MINT] == 1);
  CHECK(shop_buy_item(*s, BAG_CAPH) == BUY_NO_COINS);   // (5000 > 3500)
  CHECK(s->coins == 3500);
  int today[kShopDaily];
  REQUIRE(shop_daily(*s, "20261107", today) == kShopDaily);
  // Una MT que no está hoy no se vende.
  int other = -1;
  for (int t = 0; t < kTmCount && other < 0; ++t) {
    bool in = false;
    for (int k = 0; k < kShopDaily; ++k) if (today[k] == t) in = true;
    if (!in) other = t;
  }
  CHECK(shop_buy_tm(*s, other, "20261107") == BUY_GONE);
  const int before = s->coins;
  REQUIRE(shop_buy_tm(*s, today[0], "20261107") == BUY_OK);
  CHECK(s->coins == before - tm_price(today[0]));
  CHECK(s->tms[today[0]] == 1);
  CHECK(((s->tms_known >> today[0]) & 1u) == 1);
  // Gastar: se acaba.
  CHECK(bag_take(*s, BAG_MINT));
  CHECK(!bag_take(*s, BAG_MINT));
  CHECK(((s->bag_known >> BAG_MINT) & 1u) == 1);   // (y su 0 se escribe)
  // Siempre hay una para tu compañero, si puede aprender alguna.
  bool fits = false;
  for (int k = 0; k < kShopDaily; ++k) fits = fits || tm_can_learn(*s, 25, today[k]);
  CHECK(fits);
}

TEST_CASE("tienda: enseñar una MT la pone en sus cartas, su inicial y Repertorio") {
  std::unique_ptr<Save> s(new Save());
  s->created = 1700000000000LL;
  s->owned_at[150] = 1;
  const int tm = tm_of("shadow-ball");
  REQUIRE(tm >= 0);
  REQUIRE(tm_can_learn(*s, 150, tm));                 // Mewtwo, Bola Sombra
  CHECK(!tm_can_learn(*s, 54, tm_of("flamethrower")));   // Psyduck no aprende Lanzallamas
  CHECK(!tm_teach(*s, 150, tm));                      // sin la MT, no
  s->tms[tm] = 1;
  REQUIRE(tm_teach(*s, 150, tm));
  CHECK(s->tms[tm] == 0);
  CHECK(tm_taught(*s, 150, tm));
  CHECK(!tm_can_learn(*s, 150, tm));                  // ya la sabe
  const Mon* m = mon_by_dex(150);
  int16_t pool[kMaxMoves];
  const int sb = move_index("shadow-ball");
  auto has = [&](int n) { for (int i = 0; i < n; ++i) if (pool[i] == sb) return true; return false; };
  CHECK(!has(move_pool_for_dex(150, m->types, pool, kMaxMoves)));
  CHECK(has(move_pool_for_dex(150, m->types, pool, kMaxMoves, s->ptm[150])));
  CHECK(has(move_start_choices(150, pool, kMaxMoves, s->ptm[150])));
  CHECK(move_start_for(150, sb, true, s->ptm[150]) == sb);
  CHECK(move_start_for(150, sb, true) != sb);         // sin sus MT, no vale
  CHECK(has(move_second_choices(150, move_start_for(150), pool, kMaxMoves, s->ptm[150])));
  // En la partida: sale en sus cartas de nivel (lo aprende al subir).
  std::unique_ptr<Game> g(new Game());
  RunConfig cfg;
  cfg.dex = 150; cfg.seed = 9; cfg.judged = 1; cfg.taught = s->ptm[150];
  g->start_run(cfg);
  CHECK(g->players[0].taught == s->ptm[150]);
}

TEST_CASE("tienda: naturaleza y habilidad elegidas, y lo que cambian") {
  std::unique_ptr<Save> s(new Save());
  s->created = 1792506454241LL;
  const int timid = nature_index("timid"), adamant = nature_index("adamant");
  REQUIRE(timid >= 0);
  nature_set(*s, 25, timid, 1000);
  CHECK(nature_of(*s, 25) == timid);
  Mon m;
  mon_with_nature(*mon_by_dex(25), timid, m);
  CHECK(m.pa == doctest::Approx(mon_by_dex(25)->pa * 0.9));
  CHECK(m.atk == doctest::Approx(std::max(m.pa, m.sa)));
  // En la partida: la velocidad exacta y el ataque con su naturaleza.
  auto g1 = run_with(25, -1, timid), g2 = run_with(25, -1, -1);
  CHECK(g1->players[0].base_spd == doctest::Approx(mon_by_dex(25)->spd * 1.1));
  CHECK(g2->players[0].base_spd == doctest::Approx(float(mon_by_dex(25)->spd)));
  const float pa68 = mon_by_dex(68)->pa;
  auto g3 = run_with(68, -1, adamant);   // Machamp, Firme: más ataque físico
  CHECK(g3->players[0].mon->pa == doctest::Approx(pa68 * 1.1));
  CHECK(g3->players[0].mon != mon_by_dex(68));   // (su copia: kDex no se toca)
  CHECK(mon_by_dex(68)->pa == pa68);
  // Habilidad: Cápsula a otra de las suyas; la oculta, con la Oculta.
  const int16_t* l = abil_slots(6);   // Charizard: Mar Llamas / — / Poder Solar
  int ch[3];
  const int n1 = abil_choices(*s, 6, false, ch), n2 = abil_choices(*s, 6, true, ch);
  CHECK(n2 >= n1);
  abil_set(*s, 6, 2, 2000);
  CHECK(abil_of(*s, 6).slot == 2);
  CHECK(abil_of(*s, 6).id == l[2]);
  abil_set(*s, 6, 1, 3000);            // (no tiene segunda: no cambia)
  CHECK(abil_of(*s, 6).slot == 2);
}

TEST_CASE("tienda: la Mochila, las MT, naturalezas y habilidades se guardan (y un 0 pisa lo de la web)") {
  std::unique_ptr<Save> s(new Save());
  s->created = 1700000000000LL;
  s->owned_at[25] = 1; s->owned_at[150] = 2; s->partner = 25;
  s->bag[BAG_MINT] = 2; s->bag[BAG_CAP] = 1; s->bag_known = 3;
  const int sb = tm_of("shadow-ball"), tb = tm_of("thunderbolt");
  s->tms[sb] = 3; s->tms_known |= uint64_t(1) << sb;
  s->ptm[150] |= uint64_t(1) << sb;
  s->pnat[25] = int8_t(nature_index("bold")); s->pnat_at[25] = 77;
  s->pabil[150] = 1; s->pabil_at[150] = 88;
  const std::string txt = save_to_json(*s);
  std::unique_ptr<Save> r(new Save());
  REQUIRE(save_from_json(*r, txt));
  CHECK(r->bag[BAG_MINT] == 2);
  CHECK(r->bag[BAG_CAP] == 1);
  CHECK(r->tms[sb] == 3);
  CHECK(r->tms[tb] == 0);
  CHECK(tm_taught(*r, 150, sb));
  CHECK(r->pnat[25] == nature_index("bold"));
  CHECK(r->pnat_at[25] == 77);
  CHECK(r->pabil[150] == 1);
  CHECK(r->pabil_at[150] == 88);
  Json j;
  REQUIRE(j.parse(txt));
  CHECK(j["ptm"]["150"][0].as_str() == "shadow-ball");
  CHECK(j["pnat"]["25"][0].as_str() == "bold");
  // Con la copia de la web debajo: gastar la última Menta escribe el 0 (no reaparece).
  save_set_raw(j, *r);
  r->bag[BAG_MINT] = 0;
  r->tms[sb] = 0;
  const Json full = save_full_json(*r);
  CHECK(full["bag"]["mint"].as_num() == 0);
  CHECK(full["tms"]["shadow-ball"].as_num() == 0);
  CHECK(full["bag"]["cap"].as_num() == 1);
  save_raw() = Json();
}

TEST_CASE("tienda: al sincronizar, la Mochila y las MT van en la libreta (nada se duplica ni se pierde)") {
  // La web gastó la Menta que había; la 3DS compró otra con una copia vieja.
  Json base;
  REQUIRE(base.parse(R"({"v":3,"name":"Ana","created":1,"updated":100,"coins":2000,"owned":{"25":{"at":1}},"partner":25,
    "bag":{"mint":1},"tms":{"shadow-ball":1},"led":{"base":{"c":[500,0]},"dweb":{"c":[3000,1500],"b:mint":[1,0],"m:shadow-ball":[1,0]}},
    "ptm":{"150":["psychic"]},"pnat":{"25":["bold",10]}})"));
  Json web = base, ds = base;
  web.set("updated", Json::num(300));
  web.ref("bag").set("mint", Json::num(0));
  {
    Json p = Json::array(); p.push(Json::num(1)); p.push(Json::num(1));
    web.ref("led").ref("dweb").set("b:mint", p);
  }
  { Json p = Json::array(); p.push(Json::str("modest")); p.push(Json::num(30)); web.ref("pnat").set("25", p); }
  ds.set("updated", Json::num(200));
  ds.ref("bag").set("mint", Json::num(2));
  ds.set("coins", Json::num(500));
  { Json l = Json::array(); l.push(Json::str("psychic")); l.push(Json::str("shadow-ball")); ds.ref("ptm").set("150", l); }
  ds.ref("tms").set("shadow-ball", Json::num(0));
  web::fold(ds, "ctrprueba");
  const Json m = web::merge_safe(ds, web, 400);
  CHECK(m["bag"]["mint"].as_num() == 1);              // 1 comprada en la 3DS (la de la web, gastada allí)
  CHECK(!m["tms"].has("shadow-ball"));                // gastada en la 3DS
  CHECK(m["coins"].as_num() == 500);
  CHECK(m["ptm"]["150"].size() == 2);                 // las aprendidas, unión
  CHECK(m["pnat"]["25"][0].as_str() == "modest");     // la elegida más tarde
  std::string what;
  CHECK(!web::loses_progress(ds, m, what));
  CHECK(!web::loses_progress(web, m, what));
}

TEST_CASE("habilidades: todas las de ability-fx.js existen y tienen su efecto") {
  int missing = 0;
  for (int r = 0; r < abil_fx_rows(); ++r) {
    const int a = abil_index(abil_fx_id(r));
    if (a < 0 || abil_fx(a) != r) { missing++; MESSAGE("sin efecto: " << abil_fx_id(r)); }
  }
  CHECK(missing == 0);
  CHECK(abil_fx_rows() > 150);
}

TEST_CASE("habilidades: cada gancho hace lo que dice") {
  // Absorbe Agua: el Agua le cura en vez de dañar.
  {
    const int a = abil_index("water-absorb"), d = dex_with(a);
    REQUIRE(d > 0);
    auto g = run_with(d, a);
    Player& p = g->players[0];
    p.hp = p.max_hp * 0.5f;
    const float hp0 = p.hp;
    player_hurt(*g, p, 30, T_WATER, false, CAT_SPECIAL);
    CHECK(p.hp > hp0);
    p.invuln = 0;
    player_hurt(*g, p, 30, T_FIRE, false, CAT_SPECIAL);
    CHECK(p.hp < hp0 + p.max_hp * 0.031f);
  }
  // Robustez: un golpe que le dejaría KO le deja con 1 (una vez cada 90 s).
  {
    const int a = abil_index("sturdy"), d = dex_with(a);
    REQUIRE(d > 0);
    auto g = run_with(d, a);
    Player& p = g->players[0];
    p.hp = 10;
    player_hurt(*g, p, 9999, T_NONE, true);
    CHECK(p.hp == 1);
    CHECK(!p.dead);
    p.invuln = 0;
    player_hurt(*g, p, 9999, T_NONE, true);
    CHECK(p.dead);
  }
  // Recogida: más monedas; Afortunado: más críticos; Banco: más vida.
  {
    auto base = run_with(25, -1);
    const int a = abil_index("pickup"), d = dex_with(a);
    auto g = run_with(d, a), g0 = run_with(d, -1);
    CHECK(g->players[0].coin_mul == doctest::Approx(g0->players[0].coin_mul * 1.15f));
    const int sl = abil_index("super-luck"), ds = dex_with(sl);
    auto gs = run_with(ds, sl), gs0 = run_with(ds, -1);
    CHECK(gs->players[0].crit == doctest::Approx(gs0->players[0].crit + 0.1f));
    const int sc = abil_index("schooling"), dw = dex_with(sc);
    if (dw) { auto gw = run_with(dw, sc), gw0 = run_with(dw, -1); CHECK(gw->players[0].max_hp == doctest::Approx(std::round(gw0->players[0].max_hp * 1.25f))); }
  }
  // La pasiva genérica: +8 % con los de su tipo principal; Potencia Bruta quita los estados.
  {
    const int a = abil_index("run-away"), d = 19;   // Rattata (Fuga: tiene efecto propio)
    auto g = run_with(d, a);
    CHECK(abil_speed(*g, g->players[0]) == doctest::Approx(1.08f));
    int gen = -1;   // una sin efecto propio
    for (int i = 0; i < kAbilCount && gen < 0; ++i) if (abil_fx(i) < 0) gen = i;
    REQUIRE(gen >= 0);
    auto gg = run_with(25, gen);
    const MoveInst m = move_instance(move_index("thunder-shock"));
    CHECK(abil_out(*gg, gg->players[0], &m, nullptr) == doctest::Approx(1.08f));
    const int sf = abil_index("sheer-force");
    auto gs = run_with(dex_with(sf), sf);
    CHECK(abil_status_k(gs->players[0]) == 0.0f);
    auto g0 = run_with(25, -1);
    CHECK(abil_out(*g0, g0->players[0], &m, nullptr) == 1.0f);   // (sin habilidad: como antes)
  }
  // Autoestima: cada caído +0,5 % de daño, hasta +15 %.
  {
    const int a = abil_index("moxie"), d = dex_with(a);
    auto g = run_with(d, a);
    for (int i = 0; i < 100; ++i) abil_kill(g->players[0]);
    CHECK(g->players[0].ab_moxie == doctest::Approx(0.15f));
  }
  // Sequía: pone sol al empezar.
  {
    const int a = abil_index("drought"), d = dex_with(a);
    auto g = run_with(d, a);
    Input in;
    g->step(&in);
    CHECK(g->weather.kind == WX_SUN);
  }
  // Nado Rápido: +50 % de velocidad con lluvia.
  {
    const int a = abil_index("swift-swim"), d = dex_with(a);
    auto g = run_with(d, a);
    const float v0 = player_spd(*g, g->players[0]);
    weather_force(*g, WX_RAIN, 60);
    const float k = weather_speed_for(*g, g->players[0].mon->types);
    CHECK(player_spd(*g, g->players[0]) == doctest::Approx(v0 * 1.5f * k));
  }
  // Humedad: la explosión de un kamikaze no le daña.
  {
    for (int damp = 0; damp < 2; ++damp) {
      const int a = abil_index("damp"), d = dex_with(a);
      auto g = run_with(d, damp ? a : -1);
      Player& p = g->players[0];
      Hazard* h = g->hazards.push();
      *h = Hazard();
      h->x = p.x; h->y = p.y; h->r = 60; h->delay = 0; h->dmg = 20; h->boom = true;
      const float hp0 = p.hp;
      p.invuln = 0;
      hazards_update(*g, 1.0f / 60);
      if (damp) CHECK(p.hp == hp0); else CHECK(p.hp < hp0);
    }
  }
  // Levitación: la Tierra le hace muy poco.
  {
    const int a = abil_index("levitate"), d = dex_with(a);
    auto g = run_with(d, a);
    CHECK(abil_in_mul(*g, g->players[0], T_GROUND, CAT_PHYSICAL) == doctest::Approx(0.3f));
    CHECK(abil_levitate(g->players[0]));
  }
}

TEST_CASE("habilidades: todas, en partidas enteras con el bot, sin errores y deterministas") {
  // Cada habilidad que tiene efecto propio, con un Pokémon que la tiene, un rato de
  // partida (mortal, para que entren Robustez, Detonación…); con naturaleza al azar.
  int runs = 0;
  for (int a = 0; a < kAbilCount; ++a) {
    if (abil_fx(a) < 0 && a % 9) continue;   // (las genéricas, una de cada 9)
    const int d = dex_with(a);
    if (!d) continue;
    RunConfig base;
    base.abil = int16_t(a);
    base.nature = int8_t(a % kNatureCount);
    base.taught = kDexTms[d] & ~tm_natural_bits(d);   // (todas sus MT)
    std::unique_ptr<Game> g(new Game());
    const SimStats st = sim_run(*g, uint32_t(1000 + a), d, 150, false, nullptr, &base);
    CHECK_MESSAGE(!st.nan, kAbils[a].id);
    CHECK_MESSAGE(finite_all(*g), kAbils[a].id);
    runs++;
  }
  MESSAGE("partidas con habilidad: " << runs);
  CHECK(runs > 100);
  // La misma partida dos veces da lo mismo (lo aleatorio de las habilidades va con la semilla).
  for (const char* id : {"hustle", "moody", "effect-spore", "pickpocket", "sand-veil"}) {
    const int a = abil_index(id), d = dex_with(a);
    RunConfig base;
    base.abil = int16_t(a);
    std::unique_ptr<Game> g1(new Game()), g2(new Game());
    const SimStats s1 = sim_run(*g1, 77, d, 400, true, nullptr, &base), s2 = sim_run(*g2, 77, d, 400, true, nullptr, &base);
    CHECK_MESSAGE(s1.checksum == s2.checksum, id);
    CHECK(g1->state_hash() == g2->state_hash());
  }
}

TEST_CASE("grupo: naturaleza, habilidad y MT viajan con cada jugador (y se limitan)") {
  RunConfig::Member m;
  m.dex = 150;
  m.nature = int8_t(nature_index("modest"));
  m.abil = int16_t(abil_index("pressure"));
  m.taught = (uint64_t(1) << tm_of("shadow-ball")) | (uint64_t(1) << 63);   // (la 63 no la aprende: fuera)
  uint8_t buf[256];
  const int n = net::write_member(m, buf, sizeof(buf));
  REQUIRE(n > 0);
  RunConfig::Member r;
  REQUIRE(net::read_member(buf, n, r));
  CHECK(r.nature == m.nature);
  CHECK(r.abil == m.abil);
  CHECK(r.taught == (uint64_t(1) << tm_of("shadow-ball")));
  // Basura: fuera de rango, se queda en nada.
  m.nature = 99; m.abil = 9999;
  REQUIRE(net::read_member(buf, net::write_member(m, buf, sizeof(buf)), r));
  CHECK(r.nature == -1);
  CHECK(r.abil == -1);
}

TEST_CASE("ataques: los de un solo disparo se abren en abanico con «+1 proyectil» (web, 28-10)") {
  for (const char* id : {"seed-bomb", "energy-ball", "electro-ball", "mist-ball", "acid-spray", "signal-beam", "power-gem", "ice-shard",
                         "shadow-sneak", "draining-kiss", "mirror-shot", "sucker-punch"}) {
    const MoveInst m = move_instance(move_index(id));
    CHECK_MESSAGE(m.get(MS_SPREAD) == doctest::Approx(0.12f), id);
  }
  // Los que ya tenían la suya, igual.
  const int f = move_index("flamethrower");
  if (kMoves[f].has >> MS_SPREAD & 1u) CHECK(move_instance(f).get(MS_SPREAD) == kMoves[f].v[MS_SPREAD]);
}

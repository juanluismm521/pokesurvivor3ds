// Halloween (systems/halloween.js de la web, 25-28-10-2026; core/halloween.h):
// fechas en hora de España, mapa de noche con calabazas, Fantasma de más, caramelos,
// el gran Gengar, disfraces (y su guardado y su fusión con la web), Noche Espectral.
// Y las 4 cartas con 2 pasivas (del port, 2026-10-30).
#include <doctest/doctest.h>

#include <memory>
#include <set>
#include <string>

#include "core/gacha.h"
#include "core/game.h"
#include "core/halloween.h"
#include "core/json.h"
#include "core/lockstep.h"
#include "core/save.h"
#include "core/save_io.h"
#include "core/websave.h"

using namespace ps;

namespace {
// ms UTC de una fecha y hora UTC.
int64_t utc(int y, int mo, int d, int h = 0, int mi = 0) {
  // (madrid_midnight sabe la fecha civil; aquí, días desde 1970 a mano)
  auto days = [](int y_, int m_, int d_) {
    y_ -= m_ <= 2;
    const int64_t era = (y_ >= 0 ? y_ : y_ - 399) / 400, yoe = y_ - era * 400;
    const int64_t doy = (153 * (m_ + (m_ > 2 ? -3 : 9)) + 2) / 5 + d_ - 1, doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
  };
  return days(y, mo, d) * 86400000LL + h * 3600000LL + mi * 60000LL;
}
std::unique_ptr<Game> run(bool hw, uint32_t seed = 7, int dex = 94) {
  std::unique_ptr<Game> g(new Game());
  RunConfig c; c.dex = dex; c.seed = seed; c.judged = 1; c.hw = hw ? 1 : 0;
  g->start_run(c);
  return g;
}
int count_props(Game& g, int type, float R = 3000) {
  int n = 0;
  const Player& p = g.players[0];
  g.world.for_props_in_rect(p.x - R, p.y - R, p.x + R, p.y + R, [&](Prop& q) { n += q.type == type && !q.gone; });
  return n;
}
}  // namespace

TEST_CASE("halloween: del 30-10 al 2-11, en hora de España") {
  CHECK(halloween_date(10, 30));
  CHECK(halloween_date(10, 31));
  CHECK(halloween_date(11, 1));
  CHECK(halloween_date(11, 2));
  CHECK_FALSE(halloween_date(10, 29));
  CHECK_FALSE(halloween_date(11, 3));
  CHECK_FALSE(halloween_date(12, 31));
  // 2026: el horario de verano acaba el 25-10, así que España va a UTC+1.
  CHECK_FALSE(halloween_at(utc(2026, 10, 29, 22, 59)));   // 23:59 del 29 en España
  CHECK(halloween_at(utc(2026, 10, 29, 23, 0)));          // 00:00 del 30
  CHECK(halloween_at(utc(2026, 11, 2, 22, 59)));          // 23:59 del 2-11
  CHECK_FALSE(halloween_at(utc(2026, 11, 2, 23, 0)));     // 00:00 del 3-11
  // Un año en que el 30-10 aún es verano (2022: el cambio fue el 30-10 a las 01:00 UTC).
  CHECK(halloween_at(utc(2022, 10, 29, 22, 0)));          // 00:00 del 30 en CEST
  int y, m, d;
  madrid_date(utc(2026, 7, 1, 22, 30), y, m, d);          // verano: 00:30 del 2-7
  CHECK((y == 2026 && m == 7 && d == 2));
  madrid_date(utc(2026, 1, 1, 22, 30), y, m, d);          // invierno: 23:30 del 1-1
  CHECK((m == 1 && d == 1));
  CHECK(madrid_midnight(2026, 11, 3) == utc(2026, 11, 2, 23, 0));
}

TEST_CASE("halloween: cuenta atrás (dos semanas antes, y lo que queda)") {
  bool on; int64_t ms;
  REQUIRE(halloween_countdown(utc(2026, 10, 20, 12, 0), on, ms));
  CHECK_FALSE(on);
  CHECK(ms == utc(2026, 10, 29, 23, 0) - utc(2026, 10, 20, 12, 0));
  REQUIRE(halloween_countdown(utc(2026, 10, 30, 13, 0), on, ms));
  CHECK(on);
  CHECK(ms == utc(2026, 11, 2, 23, 0) - utc(2026, 10, 30, 13, 0));
  CHECK_FALSE(halloween_countdown(utc(2026, 9, 1), on, ms));    // (el resto del año, nada)
  CHECK_FALSE(halloween_countdown(utc(2026, 11, 10), on, ms));
}

TEST_CASE("halloween: mapa de noche con calabazas, sólo en una partida de Halloween") {
  auto g = run(true), n = run(false);
  CHECK(g->hw);
  CHECK(g->world.biome() >= kBiomeHw);
  CHECK(g->world.biome() < kBiomeHw + 3);
  CHECK(n->world.biome() < kBiomeHw);
  for (int i = 0; i < 9; ++i) { CHECK(g->run_biome(i) == kBiomeHw + i % 3); CHECK(n->run_biome(i) == i % kBiomeHw); }
  CHECK(count_props(*g, PR_PUMPKIN) > 20);
  CHECK(count_props(*n, PR_PUMPKIN) == 0);
}

TEST_CASE("halloween: dos Fantasma más en cada tramo de enemigos") {
  auto g = run(true, 11), n = run(false, 11);
  for (int ti = 0; ti < g->ncast; ++ti) {
    int ghosts = 0, ng = 0;
    for (int i = 0; i < g->cast[ti].n; ++i) {
      const Mon* m = mon_by_dex(g->cast[ti].list[i].dex);
      ghosts += m->types[0] == T_GHOST || m->types[1] == T_GHOST;
    }
    for (int i = 0; i < n->cast[ti].n; ++i) {
      const Mon* m = mon_by_dex(n->cast[ti].list[i].dex);
      ng += m->types[0] == T_GHOST || m->types[1] == T_GHOST;
    }
    CHECK(g->cast[ti].n >= n->cast[ti].n);
    CHECK(ghosts >= 1);
    CHECK(ghosts >= ng);
  }
}

TEST_CASE("halloween: caramelos al ritmo del tiempo aguantado (los Fantasma antes)") {
  auto g = run(true);
  CHECK_FALSE(candy_take(*g, 30));           // (antes del primer minuto, nada)
  CHECK(candy_take(*g, 60));                 // 1,25 al minuto: uno
  CHECK_FALSE(candy_take(*g, 60));
  CHECK(candy_take(*g, 300, 3));             // a los 5 min: 6 en total, ya va 1: quedan 5 >= 3 → sí
  auto h = run(true);
  CHECK(candy_take(*h, 300, 3));             // (6 acumulados: un no-Fantasma puede soltar)
  auto n = run(false);
  CHECK_FALSE(candy_take(*n, 600));          // (fuera de Halloween, nunca)
  // Una calabaza rota: experiencia y, si toca, un caramelo (y se coge: a la Mochila).
  std::unique_ptr<Save> s(new Save());
  auto k = run(true);
  k->save = s.get();
  k->time = 120;
  Prop pk{}; pk.type = PR_PUMPKIN; pk.x = k->players[0].x + 20; pk.y = k->players[0].y;
  k->pickups.clear();
  k->on_prop_destroyed(pk);
  int candy = 0, xp = 0;
  for (const Pickup& p : k->pickups) { candy += p.kind == PK_CANDY; xp += p.kind == PK_XP || p.kind == PK_XPBIG; }
  CHECK(candy == 1);
  CHECK(xp >= 1);
  for (int i = 0; i < 120; ++i) { Input in{}; k->step(&in); }
  CHECK(k->players[0].candy >= 1);
  CHECK(s->candy >= 1);
}

TEST_CASE("halloween: en partidas enteras salen caramelos, la partida es determinista y no se rompe nada") {
  for (uint32_t seed : {3u, 9u}) {
    std::unique_ptr<Game> a(new Game()), b(new Game());
    RunConfig c; c.dex = 94; c.seed = seed; c.judged = 1; c.hw = 1;
    a->start_run(c); b->start_run(c);
    a->debug_immortal = b->debug_immortal = true;
    for (int i = 0; i < 60 * 240; ++i) {
      Input in{}; in.ax = std::sin(i / 200.0f); in.ay = std::cos(i / 170.0f);
      Input in2 = in;
      a->step(&in); b->step(&in2);
      if (a->state == ST_LEVELUP) { Input p{}; p.cmd = CMD_CARD; p.arg = 0; a->step(&p); Input p2 = p; b->step(&p2); }
    }
    MESSAGE("semilla " << seed << ": caramelos " << a->players[0].candy << " · soltados " << a->candy_dropped);
    CHECK(a->candy_dropped >= 1);
    CHECK(a->candy_dropped <= int(4 * kCandyRate) + 1);   // (como mucho 1,25 por minuto)
    CHECK(a->state_hash() == b->state_hash());
  }
}

TEST_CASE("halloween: en la Sentencia, el gran Gengar bruja (Fantasma) y un puñado de caramelos") {
  std::unique_ptr<Save> s(new Save());
  std::unique_ptr<Game> g(new Game());
  g->debug_immortal = true;
  RunConfig c; c.dex = 6; c.seed = 4; c.judged = 1; c.demo_time = 1795; c.hw = 1;
  g->start_run(c, s.get());
  for (int i = 0; i < 60 * 30 && g->judg.phase != 2; ++i) { Input in{}; g->step(&in); }
  REQUIRE(g->judg.phase == 2);
  CHECK(g->judg.gengar);
  CHECK(g->judg.plate == T_GHOST);
  CHECK(g->judg.aanim.dex == kSkins[0].sprite);   // (su disfraz de bruja: kSkins «bruja-gengar»)
  bool target = false;
  for (const Enemy& e : g->enemies) if (e.arceus) { target = true; CHECK(e.dex == 94); CHECK(e.types[0] == T_GHOST); }
  CHECK(target);
  CHECK(s->candy >= kCandyJudge);
  CHECK(g->players[0].candy >= kCandyJudge);
  // Fuera de Halloween, Arceus de siempre.
  std::unique_ptr<Game> n(new Game());
  n->debug_immortal = true;
  c.hw = 0;
  n->start_run(c);
  for (int i = 0; i < 60 * 30 && n->judg.phase != 2; ++i) { Input in{}; n->step(&in); }
  REQUIRE(n->judg.phase == 2);
  CHECK_FALSE(n->judg.gengar);
  CHECK(n->judg.aanim.dex == 493);
}

TEST_CASE("halloween: disfraces de bruja por caramelos (sólo esos días; para siempre)") {
  std::unique_ptr<Save> s(new Save());
  const int k = skin_index("bruja-pikachu");
  REQUIRE(k >= 0);
  CHECK(kSkins[k].dex == 25);
  CHECK(skin_index("hat") >= 0);
  CHECK_FALSE(kSkins[skin_index("hat")].sell);
  CHECK(skin_buy(*s, k, false, 5) == SKIN_NOT_EVENT);
  s->candy = kSkinPrice - 1;
  CHECK(skin_buy(*s, k, true, 5) == SKIN_NO_CANDY);
  s->candy = kSkinPrice + 20;
  s->owned_at[25] = 1;
  CHECK(skin_buy(*s, k, true, 7) == SKIN_OK);
  CHECK(s->candy == 20);
  CHECK(s->skin_at[k] == 7);
  CHECK(skin_worn(*s, 25) == k);                 // (tienes a Pikachu: se lo pone)
  CHECK(skin_buy(*s, k, true, 8) == SKIN_OWNED);
  skin_wear(*s, 25, -1, 9);
  CHECK(skin_worn(*s, 25) == -1);
  skin_wear(*s, 25, k, 10);
  CHECK(skin_worn(*s, 25) == k);
  skin_wear(*s, 94, k, 11);                      // (el de Pikachu no es de Gengar)
  CHECK(skin_worn(*s, 94) == -1);
  // En la partida: con su disfraz (también en grupo, va con la sala).
  RunConfig c; c.dex = 25; c.seed = 3; c.skin = int8_t(k);
  std::unique_ptr<Game> g(new Game());
  g->start_run(c);
  CHECK(g->players[0].skin == k);
  c.dex = 4;
  g->start_run(c);
  CHECK(g->players[0].skin == -1);               // (no es suyo)
  RunConfig cc; cc.nplayers = 2; cc.seed = 9; cc.hw = 1;
  cc.members[0].dex = 25; cc.members[0].skin = int8_t(k); cc.members[1].dex = 197; cc.members[1].skin = int8_t(skin_index("bruja-umbreon"));
  uint8_t buf[2048];
  const int len = net::write_config(cc, buf, sizeof(buf));
  REQUIRE(len > 0);
  RunConfig rd;
  REQUIRE(net::read_config(buf, len, rd));
  CHECK(rd.hw == 1);
  CHECK(rd.members[0].skin == k);
  CHECK(rd.members[1].skin == skin_index("bruja-umbreon"));
}

TEST_CASE("halloween: caramelos y disfraces en la partida guardada (formato de la web)") {
  std::unique_ptr<Save> s(new Save()), t(new Save());
  s->candy = 345;
  s->candy_known = true;
  s->owned_at[94] = 1;
  const int k = skin_index("bruja-gengar");
  s->skin_at[k] = 1761800000000LL;
  skin_wear(*s, 94, k, 1761800000500LL);
  s->pity[kGachaSpooky] = 12;
  const std::string j = save_to_json(*s);
  Json o;
  REQUIRE(o.parse(j));
  CHECK(o["bag"]["candy"].as_num() == 345);
  CHECK(o["skins"]["bruja-gengar"].as_num() == doctest::Approx(1761800000000.0));
  CHECK(o["pskin"]["94"][0].as_str() == "bruja-gengar");
  CHECK(o["pity"]["10"].as_num() == 12);
  REQUIRE(save_from_json(*t, j));
  CHECK(t->candy == 345);
  CHECK(t->skin_at[k] == s->skin_at[k]);
  CHECK(skin_worn(*t, 94) == k);
  CHECK(t->pity[kGachaSpooky] == 12);
  // Sin caramelos nunca, no se escribe la Mochila (la partida de la web queda igual).
  std::unique_ptr<Save> e(new Save());
  Json oe;
  REQUIRE(oe.parse(save_to_json(*e)));
  CHECK_FALSE(oe.has("bag"));
  // Gastados todos: el 0 sí (si no, la web te los devolvería).
  t->candy = 0;
  Json oz;
  REQUIRE(oz.parse(save_to_json(*t)));
  CHECK(oz["bag"]["candy"].as_num() == 0);
}

TEST_CASE("halloween: al juntar con la web, los caramelos van por la libreta y los disfraces se suman") {
  // La web: 100 caramelos ganados allí (libreta «b:candy»), el disfraz de Pikachu.
  Json web;
  REQUIRE(web.parse(R"({"v":3,"name":"x","created":1,"updated":100,"coins":500,"tickets":{"t1":0,"t10":0},
    "bag":{"candy":100,"mint":2},"led":{"base":{"c":[500,0]},"dweb":{"b:candy":[100,0],"b:mint":[2,0]}},
    "skins":{"bruja-pikachu":50},"pskin":{"25":["bruja-pikachu",60]},"owned":{"25":{"at":1}}})"));
  // La 3DS: 40 caramelos ganados aquí, el de Gengar, y Pikachu sin disfraz desde más tarde.
  Json ds;
  REQUIRE(ds.parse(R"({"v":3,"name":"x","created":1,"updated":200,"coins":500,"tickets":{"t1":0,"t10":0},
    "bag":{"candy":40},"led":{"base":{"c":[500,0]}},
    "skins":{"bruja-gengar":70},"pskin":{"25":["",80]},"owned":{"25":{"at":1},"94":{"at":1}}})"));
  web::fold(ds, "d3ds");
  const Json m = web::merge_safe(web::migrate(ds, "", 300, false), web::migrate(web, "", 300, false), 300);
  CHECK(m["bag"]["candy"].as_num() == 140);       // (lo de los dos sitios)
  CHECK(m["bag"]["mint"].as_num() == 2);          // (lo de la web que la 3DS no usa, intacto)
  CHECK(m["skins"].has("bruja-pikachu"));
  CHECK(m["skins"].has("bruja-gengar"));
  CHECK(m["pskin"]["25"][0].as_str() == "");      // (el cambio más reciente: quitárselo en la 3DS)
  std::string lost;
  CHECK_FALSE(web::loses_progress(web, m, lost));
}

TEST_CASE("halloween: banner Noche Espectral (Fantasma de todas las regiones, shiny al 2 %, sin Amuleto Iris)") {
  int16_t pool[1100];
  for (int r = 1; r <= 5; ++r) {
    const int n = gacha_pool(r, kGachaSpooky, pool);
    for (int i = 0; i < n; ++i) { const Mon* m = mon_by_dex(pool[i]); CHECK((m->types[0] == T_GHOST || m->types[1] == T_GHOST)); }
  }
  std::unique_ptr<Save> s(new Save());
  CHECK(gacha_shiny_rate(*s, kGachaSpooky) == doctest::Approx(kShinyRateIris));
  CHECK_FALSE(gacha_has_iris(*s, kGachaSpooky));
  CHECK(std::string(gacha_banner(kGachaSpooky).name) == "Noche Espectral");
  CHECK(gacha_open(kGachaSpooky, true));
  CHECK_FALSE(gacha_open(kGachaSpooky, false));
  CHECK(gacha_next(kGachaAll, true) == kGachaSpooky);
  CHECK(gacha_next(kGachaSpooky, true) == 1);
  CHECK(gacha_next(kGachaAll, false) == 1);
  CHECK(gacha_prev(1, true) == kGachaSpooky);
  s->t1 = 100;
  Rng rng(3);
  PullResult out[100];
  REQUIRE(gacha_pull(*s, 100, kGachaSpooky, false, rng, 1, out) == 100);
  std::set<int> gens;
  for (const PullResult& r : out) { const Mon* m = mon_by_dex(r.dex); CHECK((m->types[0] == T_GHOST || m->types[1] == T_GHOST)); gens.insert(m->gen); }
  CHECK(gens.size() >= 4);
  CHECK(s->pity[kGachaSpooky] < kPity5);
}

TEST_CASE("4 cartas: con 8 movimientos o desde el nivel 25, y al menos 2 pasivas (3 desde el 40)") {
  for (uint32_t seed = 1; seed <= 30; ++seed) {
    auto g = run(false, seed, 25);
    Player& p = g->players[0];
    levelup_offer(*g, p);
    CHECK(p.ncards == 3);                         // (al principio, 3 como siempre)
    p.level = kFourLevel;
    levelup_offer(*g, p);
    REQUIRE(p.ncards == 4);
    int st = 0;
    std::set<int> keys;
    for (int i = 0; i < p.ncards; ++i) {
      st += p.cards[i].type == CT_STAT;
      keys.insert(p.cards[i].type * 1000 + (p.cards[i].type == CT_STAT ? p.cards[i].stat : p.cards[i].move));
    }
    CHECK(st >= 2);
    CHECK(keys.size() == 4);                      // (sin repetidas)
    p.level = kMorePassiveLevel;
    levelup_offer(*g, p);
    st = 0;
    for (int i = 0; i < p.ncards; ++i) st += p.cards[i].type == CT_STAT;
    CHECK(p.ncards == 4);
    CHECK(st >= 3);
  }
  // Con 8 movimientos, aunque el nivel sea bajo.
  auto g = run(false, 5, 25);
  Player& p = g->players[0];
  CHECK_FALSE(levelup_four(p));
  p.nmoves = kFourMoves;
  CHECK(levelup_four(p));
  CHECK(levelup_min_passive(p) == 2);
  // Y se puede elegir la 4.ª.
  p.nmoves = 1;
  p.level = 30;
  g->state = ST_LEVELUP; p.picked = false;
  levelup_offer(*g, p);
  REQUIRE(p.ncards == 4);
  const Card c4 = p.cards[3];
  g->pick_card(p, 3);
  CHECK(p.picked);
  (void)c4;
}

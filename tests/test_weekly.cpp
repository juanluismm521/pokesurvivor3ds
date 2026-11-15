// Desafío semanal (core/weekly.h; web: challenge.js): condiciones en la partida, premio
// de 3 minutos una vez por semana, sin maestría y el intento sincronizado.
#include <doctest/doctest.h>

#include <cstring>
#include <memory>

#include "core/bot.h"
#include "core/game.h"
#include "core/json.h"
#include "core/save.h"
#include "core/save_io.h"
#include "core/websave.h"
#include "core/weekly.h"

using namespace ps;
namespace ps { void game_offer_item_for_test(Game& g, Player& p, int id); }

namespace {
RunConfig weekly_cfg(int cond, bool items = false, int type = -1) {
  const Weekly w = weekly_of("w2026-47");
  RunConfig c;
  c.dex = w.dex; c.seed = w.seed; c.judged = 1;
  c.weekly = 1; c.wk_cond = int8_t(cond); c.wk_items = items; c.wk_type = int8_t(type);
  std::snprintf(c.wk_week, sizeof(c.wk_week), "%s", w.week);
  return c;
}
void play(Game& g, float secs) {
  Bot bot;
  for (int k = 0; k < int(secs * 60) && g.state != ST_OVER; ++k) {
    Input in;
    bot.decide(g, in, 0);
    g.step(&in);
    if (g.state == ST_LEVELUP || g.state == ST_CHOICE) { /* el bot elige */ }
  }
}
}  // namespace

TEST_CASE("desafío semanal: la misma semana da lo mismo; días y fin de semana") {
  const Weekly a = weekly_of("w2026-47"), b = weekly_of("w2026-47"), c = weekly_of("w2026-48");
  CHECK(a.dex == b.dex);
  CHECK(a.seed == b.seed);
  CHECK(a.cond == b.cond);
  CHECK((a.dex != c.dex || a.seed != c.seed));
  const Mon* m = mon_by_dex(a.dex);
  REQUIRE(m);
  CHECK_FALSE(m->leg);
  CHECK(m->rarity >= 2);
  CHECK(m->rarity <= 4);
  // Lunes a las 00:00 → quedan 7 días; domingo a las 23:00 → 1 hora
  const int64_t monday = 1794787200000LL;   // 2026-11-16 00:00 (lunes)
  CHECK(weekly_ends_in(monday) == 7 * 86400000LL);
  CHECK(weekly_ends_in(monday + 6 * 86400000LL + 23 * 3600000LL) == 3600000LL);
  char lb[96];
  Weekly w; w.cond = WC_MONO; w.type = T_FIRE; w.no_items = true;
  weekly_label(w, lb, sizeof(lb));
  CHECK(std::string(lb) == "Todos de tipo Fuego · Sin objetos");
}

TEST_CASE("desafío semanal: cristal, veloces, todos de un tipo, sin curas y sin objetos") {
  std::unique_ptr<Game> n(new Game()), g(new Game());
  RunConfig base = weekly_cfg(WC_RAIN);
  base.weekly = 0;
  n->start_run(base);
  g->start_run(weekly_cfg(WC_GLASS));
  CHECK(g->pl().max_hp == doctest::Approx(std::round(n->pl().max_hp * 0.5f)));
  CHECK(g->pl().atk_mul == doctest::Approx(n->pl().atk_mul * 1.3f));
  // Lluvia: toda la partida
  g->start_run(weekly_cfg(WC_RAIN));
  play(*g, 30);
  CHECK(g->weather.kind == WX_RAIN);
  // Todos de tipo Fuego: los elencos y los jefes
  g->start_run(weekly_cfg(WC_MONO, false, T_FIRE));
  for (int t = 0; t < g->ncast; ++t)
    for (int i = 0; i < g->cast[t].n; ++i) {
      const Mon* m = mon_by_dex(g->cast[t].list[i].dex);
      CHECK((m->types[0] == T_FIRE || m->types[1] == T_FIRE));
    }
  for (int s = 0; s < g->nbosses; ++s) {
    const Mon* m = mon_by_dex(g->bosses[s].dex);
    CHECK((m->types[0] == T_FIRE || m->types[1] == T_FIRE));
  }
  // Sin curaciones: la vida no sube
  g->start_run(weekly_cfg(WC_NOHEAL));
  g->debug_immortal = true;
  Player& p = g->pl();
  p.hp = p.max_hp * 0.5f; p.nh_hp = p.hp;
  player_heal(*g, p, 50);
  p.regen_flat = 20;
  play(*g, 20);
  CHECK(p.hp <= p.max_hp * 0.5f + 0.001f);
  // Sin objetos: monedas
  g->start_run(weekly_cfg(WC_FAST, true));
  CHECK(weekly_noitems(*g));
  const int c0 = g->pl().coins;
  game_offer_item_for_test(*g, g->pl(), 0);
  CHECK(g->pl().coins == c0 + 60);
  CHECK(g->pl().item < 0);
}

TEST_CASE("desafío semanal: ticket ×10 a los 3 minutos (una vez por semana) y sin maestría") {
  std::unique_ptr<Save> s(new Save());
  s->reset();
  const int t10 = s->t10;
  RunConfig c = weekly_cfg(WC_SUN);
  std::unique_ptr<Game> g(new Game());
  g->start_run(c, s.get());
  g->time = 170;
  game_end_run(*g, true);
  CHECK(s->t10 == t10);
  CHECK_FALSE(g->summary.wk_prize);
  g->start_run(c, s.get());
  g->time = 200; g->pl().kills = 300;
  game_end_run(*g, true);
  CHECK(s->t10 == t10 + 1);
  CHECK(g->summary.wk_prize);
  CHECK(std::string(s->wch) == "w2026-47");
  CHECK(s->mastery[c.dex] == 0);
  g->start_run(c, s.get());
  g->time = 400;
  game_end_run(*g, true);
  CHECK(s->t10 == t10 + 1);   // (una vez por semana)
  // Se guarda y, al sincronizar, gana la semana más reciente
  std::snprintf(s->wtry, sizeof(s->wtry), "w2026-47");
  const Json j = save_full_json(*s);
  CHECK(j["wtry"].as_str() == "w2026-47");
  CHECK(j["wch"].as_str() == "w2026-47");
  Json nube = j;
  nube.set("wtry", Json::str("w2026-48"));
  nube.set("wch", Json::str("w2026-45"));
  const int64_t now = 1794800000000LL;
  const Json m = web::merge_safe(web::migrate(j, "", now, false), web::migrate(nube, "", now, false), now);
  CHECK(m["wtry"].as_str() == "w2026-48");
  CHECK(m["wch"].as_str() == "w2026-47");
  std::unique_ptr<Save> b(new Save());
  REQUIRE(save_from_json(*b, m.dump()));
  CHECK(std::string(b->wtry) == "w2026-48");
}

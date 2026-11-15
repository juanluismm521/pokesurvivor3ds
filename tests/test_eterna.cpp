// Eternatus Eternamax (core/eterna.cpp) y los poderes (core/power.h).
#include <doctest/doctest.h>

#include <memory>

#include "core/bot.h"
#include "core/game.h"
#include "core/mega.h"
#include "core/power.h"
#include "core/save.h"

using namespace ps;

namespace {
// Juega con el bot desde el segundo `from` hasta que se cumpla `until` (o `secs` de partida).
template <class F>
void play(Game& g, Bot& bot, float secs, F until) {
  for (int k = 0; k < int(secs * 60) && !until(); ++k) {
    Input in;
    bot.decide(g, in, 0);
    g.step(&in);
  }
}
}  // namespace

TEST_CASE("Eternatus: llega en el minuto 20 (13 en el rápido), sus tres fases y el poder al ganarle") {
  for (int mode = 0; mode < 2; ++mode) {
    CAPTURE(mode);
    std::unique_ptr<Game> a(new Game()), b(new Game());
    RunConfig c; c.dex = 6; c.seed = 21; c.judged = 1; c.mode = uint8_t(mode);
    for (Game* g : {a.get(), b.get()}) {
      g->start_run(c);
      g->debug_immortal = true;
      g->debug_win = true;   // (todo cae deprisa)
      g->time = (mode == MODE_QUICK ? kEternaAtQuick : kEternaAt) - 2;
    }
    Bot ba, bb; ba.init(3); bb.init(3);
    Game& g = *a;
    play(g, ba, 1, [&] { return false; });
    CHECK(g.eterna.st == 0);
    play(g, ba, 5, [&] { return g.eterna.st != 0; });
    REQUIRE(g.eterna.st != 0);
    CHECK(g.time < (mode == MODE_QUICK ? kEternaAtQuick : kEternaAt) + 1);
    int max_phase = 0;
    bool immune = false, ult = false, cores = false;
    play(g, ba, 240, [&] {
      max_phase = std::max(max_phase, int(g.eterna.phase));
      if (const Enemy* e = g.enemy_by_id(g.eterna.boss_id)) immune = immune || e->immune;
      ult = ult || g.eterna.ult > 0;
      for (const Enemy& e : g.enemies) cores = cores || e.ecore;
      return g.eterna.st == 0 && g.eterna.happened;
    });
    CHECK(max_phase == 3);
    CHECK(immune);
    CHECK(cores);
    CHECK(ult);
    CHECK(g.eterna.st == 0);
    CHECK(g.eterna_wins == 1);
    // El bot elige un poder y lo usa: dura y se recarga.
    play(g, ba, 30, [&] { return g.pl().power_on; });
    CHECK(g.pl().epower >= 0);
    CHECK(g.pl().power_on);
    play(g, ba, 60, [&] { return !g.pl().power_on; });
    CHECK(!g.pl().power_on);
    CHECK(g.pl().power_cd > 0);
  }
}

TEST_CASE("Eternatus: la partida es la misma en dos consolas (determinista)") {
  std::unique_ptr<Game> a(new Game()), b(new Game());
  RunConfig c; c.dex = 445; c.seed = 33; c.judged = 1;
  for (Game* g : {a.get(), b.get()}) { g->start_run(c); g->debug_immortal = true; g->debug_win = true; g->time = kEternaAt - 1; }
  Bot ba, bb; ba.init(8); bb.init(8);
  for (int k = 0; k < 60 * 90; ++k) {
    Input i1, i2;
    ba.decide(*a, i1, 0); bb.decide(*b, i2, 0);
    a->step(&i1); b->step(&i2);
    if ((k & 127) == 0) REQUIRE(a->state_hash() == b->state_hash());
  }
  CHECK(a->eterna.happened);
}

TEST_CASE("dificultad: más vida y daño de los enemigos; premio ×1,5 y desbloqueo al vencer a Eternatus") {
  std::unique_ptr<Game> g(new Game());
  RunConfig c; c.dex = 25; c.seed = 4; c.judged = 1;
  g->start_run(c);
  const Scale3 n = enemies_scale(*g, 300);
  c.diff = DF_HARD;
  g->start_run(c);
  const Scale3 h = enemies_scale(*g, 300);
  CHECK(h.hp == doctest::Approx(n.hp * 1.4f));
  CHECK(h.dmg == doctest::Approx(n.dmg * 1.25f));
  // Premio: ×1,5 en Difícil
  std::unique_ptr<Save> s(new Save());
  s->reset();
  s->mode_wins[0] = 1;
  std::unique_ptr<Game> w(new Game());
  w->start_run(c, s.get());
  w->time = 600; w->pl().kills = 400; w->pl().t1 = 4;
  w->eterna_wins = 1;
  game_end_run(*w, false);
  CHECK(w->summary.coins == int(jsround((600 / 5 + 100) * w->pl().coin_mul * 1.5f)));
  CHECK(w->summary.diff_t1 == 2);
  CHECK(s->mode_wins[1] == 1);
  CHECK(s->mode_unlocked(2));
  CHECK(w->summary.unlocked == DF_NIGHTMARE);
  CHECK(s->stats.eterna_wins == 1);
}

TEST_CASE("poder de Eternatus: con la megapiedra puesta, Megaevolución + (o cambiarla por otro poder)") {
  std::unique_ptr<Game> g(new Game());
  RunConfig c; c.dex = 6; c.seed = 3; c.judged = 1;
  g->start_run(c);
  Player& p = g->pl();
  power_offer(*g, p);   // sin piedra: la Megaevolución de Eternatus
  REQUIRE(p.choice == CH_POWER);
  CHECK(p.power_opts[0] == PWK_MEGA);
  p.choice = CH_NONE; g->state = ST_PLAYING;
  player_equip(*g, p, IT_MEGA_A);
  power_offer(*g, p);   // con su piedra: Megaevolución +
  REQUIRE(p.choice == CH_POWER);
  CHECK(p.power_opts[0] == PWK_KEEP);
  for (int i = 1; i < p.npower_opts; ++i) CHECK(p.power_opts[i] != PWK_MEGA);
  power_choose(*g, p, 0);
  CHECK(p.epower == PWK_NONE);
  CHECK(p.mega_plus);
  CHECK(p.item == IT_MEGA_A);
  const float atk0 = p.atk_mul;
  mega_start(*g, p);
  REQUIRE(p.mega >= 0);
  CHECK(p.mega_t == doctest::Approx(kMegaTime + g_mp_time));
  CHECK(p.atk_mul == doctest::Approx(atk0 * kMegaAtk * g_mp_atk));
  mega_end(*g, p);
  CHECK(p.atk_mul == doctest::Approx(atk0));
  CHECK(p.mega_cd == doctest::Approx(kMegaCooldown * p.power_cd_k - g_mp_cd));
  // O cambiarla por otro poder (Dinamax): la mega de siempre y el poder, los dos con Y
  std::unique_ptr<Game> h(new Game());
  h->start_run(c);
  Player& q = h->pl();
  player_equip(*h, q, IT_MEGA_A);
  power_offer(*h, q);
  power_choose(*h, q, 1);
  CHECK(q.epower == PWK_DYNA);
  CHECK_FALSE(q.mega_plus);
}

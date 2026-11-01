// Shinies salvajes: si lo derrotas, va a tu colección (Reddit, 01-11-2026: «encontró
// un shiny, lo derrotó y no se lo dio»). Se prueba con cualquier forma de caer.
#include <doctest/doctest.h>

#include <memory>

#include <algorithm>

#include "core/game.h"
#include "core/moves.h"
#include "core/save.h"
#include "core/save_io.h"
#include "core/websave.h"

using namespace ps;

namespace {
// Juega hasta que salga un shiny (forzado) y devuelve su índice, o -1.
int find_shiny(Game& g, int max_steps) {
  for (int i = 0; i < max_steps; ++i) {
    Input in{};
    g.step(&in);
    for (int k = 0; k < g.enemies.n; ++k) if (g.enemies[k].shiny && !g.enemies[k].dead) return k;
  }
  return -1;
}
}  // namespace

TEST_CASE("shiny salvaje: al derrotarlo va a la colección (y a la partida guardada)") {
  for (int hw = 0; hw < 2; ++hw)
    for (int mode = 0; mode < 2; ++mode) {
      std::unique_ptr<Save> s(new Save());
      std::unique_ptr<Game> g(new Game());
      g->debug_immortal = true;
      RunConfig c; c.dex = 25; c.seed = 7 + hw * 2 + mode; c.hw = uint8_t(hw); c.mode = PlayMode(mode); c.demo_time = 120;
      g->start_run(c, s.get());
      g->spawner.force_shiny = 1;
      const int k = find_shiny(*g, 60 * 60);
      REQUIRE(k >= 0);
      const int dex = g->enemies[k].dex;
      CHECK_FALSE(s->shiny_at[dex]);
      enemy_hurt(*g, g->enemies[k], 1e9f, 0xffffff);
      for (int i = 0; i < 5; ++i) { Input in{}; g->step(&in); }
      INFO("hw " << hw << " modo " << mode << " dex " << dex);
      CHECK(s->shiny_at[dex] != 0);
      CHECK(s->owned_at[dex] != 0);
      CHECK(g->dirty);
    }
}

// La causa (01-11-2026): la sincronización que empieza al acabar una partida se
// pide con la partida de ese momento. Si vuelves a jugar en seguida y termina
// mientras, espera al final de la partida nueva; entonces lo de esa partida
// (un shiny...) estaba en memoria SIN guardar (updated igual) y el resultado de la
// nube lo pisaba. Ahora también se vuelve a juntar si hay cambios sin guardar.
TEST_CASE("sincronizar: lo de una partida sin guardar no se pierde al llegar la de la nube") {
  const int64_t T0 = 1793500000000LL;
  std::unique_ptr<Save> s(new Save());
  new_save(*s, "Ana", T0);
  s->coins = 500;
  // 1. Acaba una partida: se guarda y se pide sincronizar con la partida de entonces.
  const int64_t t0_updated = s->updated;
  Json pedido = web::migrate(save_full_json(*s), "", T0, false);
  web::fold(pedido, "3ds");
  Json nube = pedido;                       // (la nube no tenía nada más)
  const Json resultado = web::merge_safe(pedido, nube, T0 + 1000);
  // 2. Otra partida mientras: un shiny salvaje (en memoria, sin guardar aún).
  std::unique_ptr<Game> g(new Game());
  g->debug_immortal = true;
  RunConfig c; c.dex = 25; c.seed = 3; c.demo_time = 120;
  g->start_run(c, s.get());
  g->spawner.force_shiny = 1;
  const int k = find_shiny(*g, 60 * 60);
  REQUIRE(k >= 0);
  const int dex = g->enemies[k].dex;
  enemy_hurt(*g, g->enemies[k], 1e9f, 0xffffff);
  for (int i = 0; i < 5; ++i) { Input in{}; g->step(&in); }
  REQUIRE(s->shiny_at[dex] != 0);
  REQUIRE(s->updated == t0_updated);        // (aún sin guardar)
  REQUIRE(g->dirty);
  // 3. Llega el resultado: lo que hace App::cloud_tick.
  auto aplicar = [&](bool cambiado) {
    Json merged = resultado;
    if (cambiado) {
      Json mine = web::migrate(save_full_json(*s), "", T0 + 2000, false);
      web::fold(mine, "3ds");
      merged = web::merge_safe(mine, merged, T0 + 2000);
    }
    std::unique_ptr<Save> out(new Save());
    REQUIRE(save_from_json(*out, merged.dump()));
    return out->shiny_at[dex] != 0;
  };
  CHECK(web::sync_changed_since(s->updated, t0_updated, g->dirty));
  CHECK(aplicar(web::sync_changed_since(s->updated, t0_updated, g->dirty)));
  CHECK_FALSE(aplicar(s->updated != t0_updated));   // (antes: el shiny se perdía)
}

// Bola Neblina (Reddit, 01-11-2026): «la mejora de dos proyectiles no va, sólo
// dispara uno». Ni ella ni los otros 11 ataques sin `count` (Bola Voltio...).
TEST_CASE("mejoras: «+1 proyectil» dispara uno más también en los ataques sin count") {
  for (const char* id : {"mist-ball", "electro-ball", "acid-spray", "energy-ball", "shadow-sneak"}) {
    const int def = move_index(id);
    REQUIRE(def >= 0);
    INFO(id);
    CHECK(std::max(1, move_instance(def, 1).geti(MS_COUNT)) == 1);
    // homing: nv2 «+1 proyectil» y nv5 «+1 proyectil y +40 % daño»; bounce: sólo nv5
    const bool homing = kMoves[def].kind == K_HOMING;
    CHECK(std::max(1, move_instance(def, 2).geti(MS_COUNT)) == (homing ? 2 : 1));
    CHECK(std::max(1, move_instance(def, 5).geti(MS_COUNT)) == (homing ? 3 : 2));
  }
  // Los que sí dicen count, como en la web.
  const int lb = move_index("aura-sphere");
  if (lb >= 0 && (kMoves[lb].has & (1u << MS_COUNT))) CHECK(move_instance(lb, 2).geti(MS_COUNT) == int(kMoves[lb].v[MS_COUNT]) + 1);
}

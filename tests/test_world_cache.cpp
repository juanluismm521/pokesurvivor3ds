// Caché de chunks del mundo: los props cambiados no la atascan y no se pierden.
// (Antes un chunk con algo cambiado no se podía descartar: tras unos minutos
// rompiendo cosas por el mapa, la caché se llenaba y el juego regeneraba
// chunks sin parar en cada fotograma; en la consola, de 60 a 4 FPS.)
#include <doctest/doctest.h>

#include <memory>
#include <vector>

#include "core/game.h"

using namespace ps;

namespace {
constexpr float kCWorld = kChunkTiles * kTS;

// Chunk k de una espiral alrededor del origen.
void spiral(int k, int& x, int& y) {
  x = 0; y = 0;
  int dx = 0, dy = -1;
  for (int i = 0; i < k; ++i) {
    if (x == y || (x < 0 && x == -y) || (x > 0 && x == 1 - y)) { const int t = dx; dx = -dy; dy = t; }
    x += dx; y += dy;
  }
}

// Golpea (sin romper) un prop destructible de cada chunk de la espiral, hasta `n`.
// Devuelve cuántos ha golpeado; en `first` deja la casilla del primero.
int hit_rocks(World& w, int n, int32_t& ftx, int32_t& fty) {
  int done = 0;
  ftx = fty = 0;
  for (int k = 0; done < n && k < 6000; ++k) {
    int x, y;
    spiral(k, x, y);
    const float cx = (x + 0.5f) * kCWorld, cy = (y + 0.5f) * kCWorld;
    bool hit = false;
    w.for_props_in_rect(cx - kCWorld / 2 + 1, cy - kCWorld / 2 + 1, cx + kCWorld / 2 - 1, cy + kCWorld / 2 - 1, [&](Prop& p) {
      if (hit || !p.destructible || p.hp < p.max_hp) return;
      p.hp = p.max_hp * 0.5f;
      hit = true;
      if (done == 0) { ftx = p.tx; fty = p.ty; }
    });
    if (hit) done++;
  }
  return done;
}

float prop_hp_at(World& w, int32_t tx, int32_t ty) {
  float hp = -1;
  const float x = (tx + 0.5f) * kTS, y = (ty + 0.5f) * kTS;
  w.for_props_in_rect(x - 1, y - 1, x + 1, y + 1, [&](Prop& p) { if (p.tx == tx && p.ty == ty) hp = p.hp; });
  return hp;
}
}  // namespace

TEST_CASE("mundo: con cientos de chunks cambiados, la caché no se atasca") {
  std::unique_ptr<Game> g(new Game());
  RunConfig c; c.dex = 6; c.seed = 3; c.judged = 1;
  g->start_run(c);
  int32_t tx, ty;
  REQUIRE(hit_rocks(g->world, 400, tx, ty) == 400);   // (más que los 160 de la caché)
  // Andar por una zona: sólo se generan los chunks nuevos que se pisan.
  const int gen0 = g->world.chunks_generated();
  for (int s = 0; s < 600; ++s) {
    const float px = 100 + s * 6.0f, py = 50;
    for (int j = -10; j < 10; ++j)
      for (int i = -14; i < 14; ++i) g->world.terrain_at(World::tile_of(px) + i, World::tile_of(py) + j);
  }
  const int gen = g->world.chunks_generated() - gen0;
  CHECK(gen < 80);   // (antes: más de 30 000)
}

TEST_CASE("mundo: un prop cambiado conserva su estado al salir de la caché y volver") {
  std::unique_ptr<Game> g(new Game());
  RunConfig c; c.dex = 6; c.seed = 3; c.judged = 1;
  g->start_run(c);
  int32_t tx, ty;
  REQUIRE(hit_rocks(g->world, 1, tx, ty) == 1);
  const float hp = prop_hp_at(g->world, tx, ty);
  REQUIRE(hp > 0);
  // Recorrer muchos chunks lejos (el del prop sale de la caché).
  const int gen0 = g->world.chunks_generated();
  for (int k = 0; k < 400; ++k) g->world.terrain_at((200 + k) * kChunkTiles, 300 * kChunkTiles);
  REQUIRE(g->world.chunks_generated() - gen0 >= 400);
  CHECK(prop_hp_at(g->world, tx, ty) == doctest::Approx(hp));   // (vuelve golpeado, no como nuevo)
  // Y si se recupera del todo, al volver a salir ya no se le aplica lo viejo.
  const float x = (tx + 0.5f) * kTS, y = (ty + 0.5f) * kTS;
  g->world.for_props_in_rect(x - 1, y - 1, x + 1, y + 1, [&](Prop& p) { if (p.tx == tx && p.ty == ty) p.hp = p.max_hp; });
  for (int k = 0; k < 400; ++k) g->world.terrain_at((800 + k) * kChunkTiles, 300 * kChunkTiles);
  float maxhp = 0;
  g->world.for_props_in_rect(x - 1, y - 1, x + 1, y + 1, [&](Prop& p) { if (p.tx == tx && p.ty == ty) maxhp = p.max_hp; });
  CHECK(prop_hp_at(g->world, tx, ty) == doctest::Approx(maxhp));
}

TEST_CASE("mundo: mirar otras zonas (cada consola del grupo dibuja la suya) no cambia la partida") {
  std::unique_ptr<Game> a(new Game()), b(new Game());
  RunConfig c; c.dex = 6; c.seed = 9; c.judged = 1;
  a->debug_immortal = b->debug_immortal = true;
  a->start_run(c);
  b->start_run(c);
  int32_t tx, ty;
  hit_rocks(a->world, 250, tx, ty);
  hit_rocks(b->world, 250, tx, ty);
  for (int s = 0; s < 60 * 40; ++s) {
    Input in{};
    in.ax = (s / 300) % 2 ? 1.0f : -0.6f;
    in.ay = (s / 170) % 2 ? 0.7f : -1.0f;
    a->step(&in);
    Input in2 = in;
    b->step(&in2);
    // b, además, «dibuja» zonas lejanas cada poco (como una consola que mira a otro jugador)
    if (s % 7 == 0)
      for (int k = 0; k < 40; ++k) b->world.terrain_at((s / 7 % 50 + k) * kChunkTiles * 3, -(k * 5) * kChunkTiles);
  }
  CHECK(a->state_hash() == b->state_hash());
  // Los props de la zona golpeada, iguales en las dos.
  int same = 0, diff = 0;
  for (int k = 0; k < 250; ++k) {
    int x, y;
    spiral(k, x, y);
    std::vector<float> ha, hb;
    const float cx = (x + 0.5f) * kCWorld, cy = (y + 0.5f) * kCWorld;
    a->world.for_props_in_rect(cx - 200, cy - 200, cx + 200, cy + 200, [&](Prop& p) { ha.push_back(p.hp + p.charge * 7 + p.open * 13 + p.used); });
    b->world.for_props_in_rect(cx - 200, cy - 200, cx + 200, cy + 200, [&](Prop& p) { hb.push_back(p.hp + p.charge * 7 + p.open * 13 + p.used); });
    if (ha == hb) same++; else diff++;
  }
  CHECK(diff == 0);
  CHECK(same == 250);
}

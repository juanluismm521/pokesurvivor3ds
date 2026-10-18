// pathing.cpp — los enemigos rodean rocas y lagos (pathing.js de la web).
// Un "campo de distancias" por jugador: cada 0,35 s un recorrido en anchura
// (BFS) por las casillas de alrededor (57×57) apunta cuántos pasos faltan
// hasta el jugador; un enemigo sólo tiene que ir a la vecina con menos pasos.
//   8 vecinos, sin cortar esquinas · bloquean paredes, objetos sólidos y
//   lava/veneno/descarga (el agua se puede cruzar) · los que flotan no lo usan.
#include <cmath>

#include "core/game.h"

namespace ps {

namespace {
constexpr int R = Game::kPathR, N = Game::kPathN;
constexpr float EVERY = 0.35f;
constexpr int16_t UNSEEN = 0x7fff;
const int D8[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
int32_t queue_[N * N];

void build(Game& g, Game::PathField& f, const Player& pl) {
  const int32_t px = int32_t(std::floor(pl.x / kTS)), py = int32_t(std::floor(pl.y / kTS));
  f.on = true;
  f.ox = px - R; f.oy = py - R;
  for (auto& d : f.dist) d = UNSEEN;
  for (int j = 0; j < N; ++j) for (int i = 0; i < N; ++i) f.open[j * N + i] = g.world.walkable(f.ox + i, f.oy + j) ? 1 : 0;
  const int start = R * N + R;
  f.open[start] = 1;                 // donde está el jugador siempre cuenta
  f.dist[start] = 0;
  int head = 0, tail = 0;
  queue_[tail++] = start;
  while (head < tail) {
    const int c = queue_[head++];
    const int cx = c % N, cy = c / N;
    const int16_t d = int16_t(f.dist[c] + 1);
    for (int k = 0; k < 8; ++k) {
      const int nx = cx + D8[k][0], ny = cy + D8[k][1];
      if (nx < 0 || ny < 0 || nx >= N || ny >= N) continue;
      const int n = ny * N + nx;
      if (!f.open[n] || f.dist[n] != UNSEEN) continue;
      // Diagonal: las dos casillas de al lado tienen que estar libres.
      if (k >= 4 && (!f.open[cy * N + nx] || !f.open[ny * N + cx])) continue;
      f.dist[n] = d;
      queue_[tail++] = n;
    }
  }
}

// ¿Cabe un cuerpo de radio r en línea recta de (ax,ay) a (bx,by)?
bool clear_line(const Game::PathField& f, float ax, float ay, float bx, float by, float r) {
  const float len = std::hypot(bx - ax, by - ay);
  if (len < 1) return true;
  const float ux = (bx - ax) / len, uy = (by - ay) / len;
  const float w = std::fmin(r * 0.9f, kTS * 0.45f);
  for (float d = 0; d <= len; d += kTS * 0.4f) {
    const float cx = ax + ux * d, cy = ay + uy * d;
    const float ss[3] = {-w, 0, w};
    for (float s : ss) {
      const int x = int(std::floor((cx - uy * s) / kTS)) - f.ox, y = int(std::floor((cy + ux * s) / kTS)) - f.oy;
      if (x < 0 || y < 0 || x >= N || y >= N) return false;
      if (!f.open[y * N + x]) return false;
    }
  }
  return true;
}
}  // namespace

void path_clear(Game& g) {
  for (auto& f : g.paths) f.on = false;
  g.path_timer = 0;
}

void path_update(Game& g, float dt) {
  g.path_timer -= dt;
  if (g.path_timer > 0) return;
  g.path_timer = EVERY;
  for (int i = 0; i < kMaxPlayers; ++i) {
    const Player& p = g.players[i];
    if (i >= g.nplayers || !p.active) { g.paths[i].on = false; continue; }
    if (!p.dead) build(g, g.paths[i], p);
  }
}

// Dirección para que `e` llegue hasta el jugador rodeando obstáculos, o false
// si no hace falta (cerca, fuera del campo o sin camino).
bool path_dir_for(Game& g, Enemy& e, int pi, float& ox, float& oy) {
  const Game::PathField& f = g.paths[pi];
  if (!f.on) return false;
  const Player& pl = g.players[pi];
  const int tx = int(std::floor(e.x / kTS)) - f.ox, ty = int(std::floor(e.y / kTS)) - f.oy;
  if (tx < 0 || ty < 0 || tx >= N || ty >= N) return false;
  const int16_t here = f.dist[ty * N + tx];
  if (here <= 1 || here == UNSEEN) return false;   // pegado a ti, o sin camino: directo
  // Con vista despejada hasta el jugador, en línea recta. Se mira cada 0,2 s por enemigo.
  const float now = float(g.time);
  if (e.los_at < 0 || now - e.los_at > 0.2f) { e.los_at = now; e.los = clear_line(f, e.x, e.y, pl.x, pl.y, e.r); }
  if (e.los) return false;
  // Siguiente casilla del camino (la vecina con menos pasos, sin cortar esquinas).
  int bx = tx, by = ty;
  int16_t best = here;
  for (int k = 0; k < 8; ++k) {
    const int x = tx + D8[k][0], y = ty + D8[k][1];
    if (x < 0 || y < 0 || x >= N || y >= N) continue;
    const int16_t v = f.dist[y * N + x];
    if (v >= best) continue;
    if (k >= 4 && (!f.open[ty * N + x] || !f.open[y * N + tx])) continue;
    best = v; bx = x; by = y;
  }
  if (bx == tx && by == ty) return false;
  float gx = (f.ox + bx + 0.5f) * kTS, gy = (f.oy + by + 0.5f) * kTS;
  // Si va descentrado y el atajo hasta esa casilla roza un obstáculo, primero
  // al centro de la suya (así no se engancha en las esquinas).
  if (!clear_line(f, e.x, e.y, gx, gy, e.r)) { gx = (f.ox + tx + 0.5f) * kTS; gy = (f.oy + ty + 0.5f) * kTS; }
  norm(gx - e.x, gy - e.y, ox, oy);
  return true;
}

}  // namespace ps

// treasure_sim.h — ¿cuesta llegar al tesoro de la Luminosfera? El bot juega una
// partida de verdad hasta el minuto que se pida, usa la Luminosfera y luego va
// derecho al tesoro (rodeando árboles y rocas si se atasca), con los enemigos que
// haya. Lo usan tests/test_orbs.cpp y la herramienta ps_tesoro.
#pragma once
#include <cmath>
#include <algorithm>
#include <memory>
#include <queue>
#include <vector>

#include "core/orbs.h"
#include "core/util.h"
#include "sim.h"

namespace ps {

struct TreasureTry {
  bool placed = false;     // (no se pudo: arena de grieta, Sentencia, partida acabada)
  bool reached = false;
  float secs = 0;          // lo que tardó (o kTreasureTime si no llegó)
  float dist = 0;          // a qué distancia salió
  float hp_lost = 0;       // vida perdida por el camino (fracción de la máxima)
  float left = 0;          // si no llegó: lo que le faltaba
  bool path = false;       // había camino andando (si no, está en una isla o tras un muro)
  float walk = 0;          // lo largo de ese camino
};

// Camino por casillas (Dijkstra; el agua cuesta más) de la casilla de (x0, y0) a la
// de (x1, y1), dentro de una caja alrededor. Devuelve los puntos de paso (en mundo)
// o vacío si no hay camino: como un jugador que mira el minimapa y rodea.
inline std::vector<std::pair<float, float>> treasure_path(World& w, float x0, float y0, float x1, float y1, float r) {
  const int ax = int(std::floor(x0 / kTS)), ay = int(std::floor(y0 / kTS));
  const int bx = int(std::floor(x1 / kTS)), by = int(std::floor(y1 / kTS));
  const int pad = 24, lx = std::min(ax, bx) - pad, ly = std::min(ay, by) - pad;
  const int W = std::abs(ax - bx) + 2 * pad + 1, H = std::abs(ay - by) + 2 * pad + 1;
  std::vector<float> cost(size_t(W * H), 1e30f);
  std::vector<int> from(size_t(W * H), -1);
  std::vector<int8_t> pass(size_t(W * H), -1);
  auto ok = [&](int i) -> int {   // 0 no, 1 suelo, 3 agua
    if (pass[size_t(i)] < 0) {
      const float cx = (lx + i % W + 0.5f) * kTS, cy = (ly + i / W + 0.5f) * kTS;
      pass[size_t(i)] = int8_t(!w.is_free(cx, cy, r * 0.75f, true) ? 0 : w.is_liquid(cx, cy) ? 3 : 1);
    }
    return pass[size_t(i)];
  };
  using Q = std::pair<float, int>;
  std::priority_queue<Q, std::vector<Q>, std::greater<Q>> q;
  const int s0 = (ay - ly) * W + (ax - lx), goal = (by - ly) * W + (bx - lx);
  cost[size_t(s0)] = 0;
  q.push({0, s0});
  while (!q.empty()) {
    const Q c = q.top(); q.pop();
    if (c.first > cost[size_t(c.second)]) continue;
    if (c.second == goal) break;
    const int cx = c.second % W, cy = c.second / W;
    for (int d = 0; d < 8; ++d) {
      static const int DX[] = {1, -1, 0, 0, 1, 1, -1, -1}, DY[] = {0, 0, 1, -1, 1, -1, 1, -1};
      const int nx = cx + DX[d], ny = cy + DY[d];
      if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
      const int ni = ny * W + nx;
      const int k = ok(ni);
      if (!k) continue;
      if (d >= 4 && (!ok(cy * W + nx) || !ok(ny * W + cx))) continue;   // (sin cortar esquinas)
      const float nc = c.first + k * (d >= 4 ? 1.414f : 1.0f);
      if (nc < cost[size_t(ni)]) { cost[size_t(ni)] = nc; from[size_t(ni)] = c.second; q.push({nc, ni}); }
    }
  }
  std::vector<std::pair<float, float>> out;
  if (from[size_t(goal)] < 0 && goal != s0) return out;
  for (int i = goal; i != s0 && i >= 0; i = from[size_t(i)]) out.push_back({(lx + i % W + 0.5f) * kTS, (ly + i / W + 0.5f) * kTS});
  std::reverse(out.begin(), out.end());
  return out;
}

inline TreasureTry treasure_try(uint32_t seed, int dex, double minute, float ups = 0.5f) {
  std::unique_ptr<Game> g(new Game());
  RunConfig base;
  for (int u = 0; u < UP_COUNT; ++u) base.upgrades[u] = uint8_t(kUpgrades[u].max * ups + 0.5f);
  TreasureTry r;
  // (no muere: lo que se mide es el camino; la vida perdida se apunta aparte)
  sim_run(*g, seed, dex, minute * 60, true, nullptr, &base);
  Game& G = *g;
  Player& p = G.players[0];
  if (G.state == ST_OVER || G.rift.arena || G.judg.phase) return r;
  orb_place_treasure(G, p);
  r.placed = true;
  r.dist = dist(p.x, p.y, G.treasure.x, G.treasure.y);
  Bot bot;
  bot.init(seed ^ 0x7e50);
  std::vector<std::pair<float, float>> path = treasure_path(G.world, p.x, p.y, G.treasure.x, G.treasure.y, p.r);
  r.path = !path.empty();
  for (size_t i = 1; i < path.size(); ++i) r.walk += dist(path[i - 1].first, path[i - 1].second, path[i].first, path[i].second);
  float hp0 = p.hp, lost = 0, check_t = 0, side_t = 0, cx = p.x, cy = p.y;
  int side = 1;
  size_t wp = 0;
  const float t0 = float(G.time);
  while (G.treasure.on && G.state != ST_OVER && float(G.time) - t0 < kTreasureTime + 2) {
    Input in;
    if (!bot.decide(G, in)) continue;
    if (G.state == ST_PLAYING) {
      // Al siguiente punto del camino que no esté ya cerca (o derecho, si no hay camino).
      while (wp < path.size() && dist(p.x, p.y, path[wp].first, path[wp].second) < kTS * 1.2f) wp++;
      const float tx = wp < path.size() ? path[wp].first : G.treasure.x, ty = wp < path.size() ? path[wp].second : G.treasure.y;
      float dx = tx - p.x, dy = ty - p.y;
      const float l = std::sqrt(dx * dx + dy * dy);
      if (l > 0) { dx /= l; dy /= l; }
      if (side_t > 0) {   // atascado (enemigos, una roca): de lado un rato
        const float c = std::cos(1.22f), s = std::sin(1.22f) * float(side);
        const float rx = dx * c - dy * s, ry = dx * s + dy * c;
        dx = rx; dy = ry;
      }
      in.ax = dx; in.ay = dy;
    }
    G.step(&in);
    const float dt = 1.0f / 60;
    if (side_t > 0) side_t -= dt;
    if ((check_t += dt) >= 0.5f) {   // ¿se ha atascado? (menos de 15 px en medio segundo)
      if (dist(p.x, p.y, cx, cy) < 15 && side_t <= 0) { side_t = 0.8f; side = -side; }
      check_t = 0; cx = p.x; cy = p.y;
    }
    if (p.hp < hp0) lost += hp0 - p.hp;
    hp0 = p.hp;
  }
  r.secs = float(G.time) - t0;
  r.reached = !G.treasure.on && r.secs <= kTreasureTime;
  r.reached = r.reached && dist(p.x, p.y, G.treasure.x, G.treasure.y) < 60;   // (no por apagarse en una grieta)
  if (!r.reached) { r.secs = kTreasureTime; r.left = dist(p.x, p.y, G.treasure.x, G.treasure.y); }
  r.hp_lost = lost / std::fmax(1.0f, p.max_hp);
  return r;
}

}  // namespace ps

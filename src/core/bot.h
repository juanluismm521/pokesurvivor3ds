// bot.h — un bot que juega solo (pruebas en PC, autotest de la consola y
// herramienta ps_sim). Huye de los enemigos, va a por lo que hay en el suelo y
// a las grietas, mantiene activo su mejor ataque y elige cartas con criterio.
#pragma once
#include <cmath>

#include "core/game.h"

namespace ps {

struct Bot {
  Rng rng;
  int t = 0;
  float wander = 0;
  int cards = 0;

  void init(uint32_t seed) { rng.reseed(seed * 7919u + 13); t = 0; wander = rng.next() * kTau; cards = 0; }

  // Decide lo de este paso. Devuelve false si ha resuelto una pantalla de
  // elección (cartas, objeto, vínculo) y no hay que avanzar la partida.
  bool decide(Game& g, Input& in) {
    ++t;
    in = Input();
    if (g.state == ST_LEVELUP) {
      // Prefiere evoluciones, mejoras de ataques y ataques nuevos (no potenciadores).
      int pick = int(rng.next() * g.ncards), best_score = -1;
      for (int c = 0; c < g.ncards; ++c) {
        const Card& cd = g.cards[c];
        int score = 1;
        if (cd.type == CT_EVO) score = 9;
        else if (cd.type == CT_UP && g.pl().moves[cd.move].kind() != K_BUFF) score = 6;
        else if (cd.type == CT_NEW && kMoves[cd.move].kind != K_BUFF) score = 5;
        else if (cd.type == CT_STAT && cd.evo_for >= 0) score = 7;
        else if (cd.type == CT_STAT) score = 3;
        score = score * 10 + int(rng.next() * 10);
        if (score > best_score) { best_score = score; pick = c; }
      }
      g.pick_card(pick);
      cards++;
      return false;
    }
    if (g.state == ST_CHOICE) {
      if (g.choice == CH_ITEM) g.pick_item(rng.next() < 0.5f);
      else if (g.choice == CH_BOND) {
        int8_t loose[4]; int nl, grow;
        const int mode = player_bond_options(g.pl(), loose, nl, grow);
        int pick[4], n = 0;
        if (mode == 1) pick[n++] = loose[0];
        else if (mode == 2) { pick[n++] = loose[0]; pick[n++] = loose[1]; }
        g.pick_bond(pick, n);
      }
      return false;
    }
    if (g.state == ST_PAUSED) { g.toggle_pause(); return false; }
    if (g.judg.phase == 1 && rng.next() < 0.01f) g.skip_judgment();
    const Player& p = g.pl();
    float ax = std::cos(wander) * 0.6f, ay = std::sin(wander) * 0.6f;
    if (t % 240 == 0) wander += rng.range(-1.5f, 1.5f);
    for (const Enemy& e : g.enemies) {
      const float d = dist2(p.x, p.y, e.x, e.y);
      if (d < 160 * 160 && d > 1) { ax -= (e.x - p.x) / std::sqrt(d) * 1.2f; ay -= (e.y - p.y) / std::sqrt(d) * 1.2f; }
    }
    float pb = 300 * 300;
    const Pickup* tgt = nullptr;
    for (const Pickup& q : g.pickups) { const float d = dist2(p.x, p.y, q.x, q.y); if (d < pb) { pb = d; tgt = &q; } }
    if (tgt) { const float d = std::sqrt(pb) + 1; ax += (tgt->x - p.x) / d; ay += (tgt->y - p.y) / d; }
    if (g.rift.open) { const float d = dist(p.x, p.y, g.rift.x, g.rift.y) + 1; ax += (g.rift.x - p.x) / d * 2; ay += (g.rift.y - p.y) / d * 2; }
    const float m = std::hypot(ax, ay);
    if (m > 1) { ax /= m; ay /= m; }
    in.ax = ax; in.ay = ay;
    // Hueco activo: el ataque de más nivel (los potenciadores no hacen daño).
    if (t % 120 == 0) {
      int bs = -1, bl = -1;
      for (int k = 0; k < p.nslots; ++k) {
        const MoveInst& mm = p.moves[p.slots[k][0]];
        const int score = (mm.kind() == K_BUFF ? 0 : 10) + mm.lvl + (p.slot_n[k] > 1 ? 20 : 0);
        if (score > bl) { bl = score; bs = k; }
      }
      if (bs >= 0 && bs != p.active_slot) in.select = int8_t(bs);
    }
    return true;
  }
};

}  // namespace ps

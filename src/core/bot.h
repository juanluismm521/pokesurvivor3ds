// bot.h — un bot que juega solo (pruebas en PC, autotest de la consola y
// herramienta ps_sim). Huye de los enemigos, va a por lo que hay en el suelo y
// a las grietas, mantiene activo su mejor ataque y elige cartas con criterio.
#pragma once
#include <cmath>

#include "core/game.h"
#include "core/mega.h"
#include "core/power.h"

namespace ps {

struct Bot {
  Rng rng;
  int t = 0;
  float wander = 0;
  int cards = 0;
  bool suicidal = false;   // (autotest) va a por los enemigos sin huir ni curarse

  void init(uint32_t seed) { rng.reseed(seed * 7919u + 13); t = 0; wander = rng.next() * kTau; cards = 0; }

  // Decide lo de este paso para el jugador `who`: movimiento y, si hay una
  // pantalla de elección (cartas, objeto, vínculo), la orden que la resuelve.
  // (Devuelve siempre true: la orden la aplica el paso, como con un jugador.)
  bool decide(Game& g, Input& in, int who = 0) {
    ++t;
    in = Input();
    const Player& me = g.players[who];
    if (g.state == ST_LEVELUP) {
      if (me.picked) return true;   // (esperando a los demás)
      // Prefiere evoluciones, mejoras de ataques y ataques nuevos (no potenciadores).
      int pick = int(rng.next() * me.ncards), best_score = -1;
      for (int c = 0; c < me.ncards; ++c) {
        const Card& cd = me.cards[c];
        int score = 1;
        if (cd.type == CT_EVO) score = 9;
        else if (cd.type == CT_UP && !kind_is_status(me.moves[cd.move].kind())) score = 6;
        else if (cd.type == CT_NEW && !kind_is_status(kMoves[cd.move].kind)) score = 5;
        else if (cd.type == CT_STAT && cd.evo_for >= 0) score = 7;
        else if (cd.type == CT_STAT) score = 3;
        score = score * 10 + int(rng.next() * 10);
        if (score > best_score) { best_score = score; pick = c; }
      }
      in.cmd = CMD_CARD; in.arg = int8_t(pick);
      cards++;
      move(g, in, me);   // (tras elegir, el paso sigue: que se mueva ya)
      return true;
    }
    if (g.state == ST_CHOICE) {
      if (me.choice == CH_ITEM) { in.cmd = CMD_ITEM; in.arg = rng.next() < 0.5f ? 1 : 0; }
      if (me.choice == CH_POWER) { in.cmd = CMD_POWER; in.arg = int8_t(rng.next() * me.npower_opts); }
      if (me.choice == CH_EVO) { in.cmd = CMD_EVO; in.arg = int8_t(rng.next() * (me.evo_nopts + 1)); }
      else if (me.choice == CH_BOND) {
        int8_t loose[4]; int nl, grow;
        const int mode = player_bond_options(me, loose, nl, grow);
        in.cmd = CMD_BOND;
        if (mode == 1) { in.moves[0] = loose[0]; in.arg = 1; }
        else if (mode == 2) { in.moves[0] = loose[0]; in.moves[1] = loose[1]; in.arg = 2; }
      }
      if (me.choice != CH_NONE) move(g, in, me);
      return true;
    }
    if (g.state == ST_PAUSED) { in.cmd = CMD_PAUSE; return true; }
    if (g.state == ST_PLAYING && (mega_ready(me) || power_ready(me))) in.cmd = CMD_MEGA;   // (megaevoluciona en cuanto puede)
    else if (g.state == ST_PLAYING && me.orb_cd <= 0 && rng.next() < 0.004f) {   // (una esfera de vez en cuando)
      for (int k = 0; k < 4; ++k) if (me.orb_id[k] >= 0) { in.cmd = CMD_ORB; in.arg = int8_t(k); break; }
    }
    if (g.judg.phase == 1 && rng.next() < 0.01f) in.cmd = CMD_SKIP;
    move(g, in, me);
    return true;
  }

  // Movimiento: huye de los enemigos, va a lo del suelo, a la grieta y a
  // levantar al compañero caído; cada 2 s, el hueco con el mejor ataque.
  void move(const Game& g, Input& in, const Player& me) {
    const int who = int(&me - g.players);
    if (suicidal) {
      // Directo al enemigo más cercano.
      float bd = 1e30f, tx = me.x, ty = me.y;
      for (const Enemy& e : g.enemies) { const float d = dist2(me.x, me.y, e.x, e.y); if (!e.dead && d < bd) { bd = d; tx = e.x; ty = e.y; } }
      const float d = std::sqrt(bd) + 1;
      if (bd < 1e29f) { in.ax = (tx - me.x) / d; in.ay = (ty - me.y) / d; }
      return;
    }
    const Player& p = me;
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
    // En grupo: a levantar al compañero caído.
    for (int k = 0; k < g.nplayers; ++k) {
      const Player& o = g.players[k];
      if (k == who || !o.active || !o.dead) continue;
      const float d = dist(p.x, p.y, o.x, o.y) + 1;
      if (d > 30) { ax += (o.x - p.x) / d * 3; ay += (o.y - p.y) / d * 3; }
    }
    const float m = std::hypot(ax, ay);
    if (m > 1) { ax /= m; ay /= m; }
    in.ax = ax; in.ay = ay;
    // Hueco activo: el ataque de más nivel (los potenciadores no hacen daño).
    if (t % 120 == 0) {
      int bs = -1, bl = -1;
      for (int k = 0; k < p.nslots; ++k) {
        const MoveInst& mm = p.moves[p.slots[k][0]];
        const int score = (kind_is_status(mm.kind()) ? 0 : 10) + mm.lvl + (p.slot_n[k] > 1 ? 20 : 0);
        if (score > bl) { bl = score; bs = k; }
      }
      if (bs >= 0 && bs != p.active_slot) in.select = int8_t(bs);
    }
  }
};

}  // namespace ps

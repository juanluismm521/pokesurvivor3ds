// pickup.cpp — orbes de experiencia y objetos (pickup.js de la web).
// Los orbes se quedan quietos hasta que entras en el radio del imán; entonces
// aceleran hacia ti. Al pasar de MAX se fusionan los más antiguos.
#include <cmath>

#include "core/game.h"
#include "core/orbs.h"

namespace ps {

namespace {
constexpr int MAX = 420;
constexpr float BOSS_T10 = 1.0f / 3;     // ticket ×10 de un jefe normal
constexpr float LEGEND_T10 = 1.0f / 2;   // ticket ×10 del legendario de la grieta
constexpr float TICKET_RATE = 1.0f / 400;

void push(Game& g, int kind, float x, float y, float value) {
  Pickup* o = g.pickups.push();
  if (!o) return;
  const float a = g.rng.next() * kTau;
  o->id = g.next_pickup_id++;
  o->kind = int8_t(kind); o->x = x; o->y = y; o->value = value;
  const float s1 = g.rng.range(10, 46);
  o->vx = std::cos(a) * s1;
  const float s2 = g.rng.range(10, 46);
  o->vy = std::sin(a) * s2;
  o->t = g.rng.next() * 6.28f;
  o->pulled = false;
  if (g.pickups.n > MAX) {
    // Fusiona los dos más antiguos de XP para no crecer sin límite.
    int i = -1;
    for (int k = 0; k < g.pickups.n; ++k) if (g.pickups[k].kind == PK_XP || g.pickups[k].kind == PK_XPBIG) { i = k; break; }
    if (i >= 0) {
      const Pickup a2 = g.pickups[i];
      g.pickups.remove_ordered(i);
      for (int k = 0; k < g.pickups.n; ++k)
        if (g.pickups[k].kind == PK_XP || g.pickups[k].kind == PK_XPBIG) { g.pickups[k].value += a2.value; g.pickups[k].kind = PK_XPBIG; break; }
    } else {
      g.pickups.remove_ordered(0);
    }
  }
}
}  // namespace

void pickups_pull_all(Game& g) { for (Pickup& o : g.pickups) o.pulled = true; }

// plain: sólo la experiencia (los legendarios de la Sentencia de Arceus)
void pickup_drop_xp(Game& g, float x, float y, float value, bool boss, bool legend, bool plain) {
  if (boss) {
    // Los jefes revientan en un montón de orbes grandes + recompensas.
    for (int i = 0; i < 14; ++i) {
      const float a = g.rng.next() * kTau, d = g.rng.range(10, 70);
      push(g, PK_XPBIG, x + std::cos(a) * d, y + std::sin(a) * d, std::ceil(value / 14));
    }
    if (plain) return;
    push(g, PK_HEAL, x + 24, y, 0);
    push(g, PK_MAGNET, x - 24, y, 0);
    if (g.rng.next() < (legend ? LEGEND_T10 : BOSS_T10)) push(g, PK_TICKET10, x, y - 20, 1);
    // Esferas de Mundo Misterioso (web, 23-10): una de cada jefe y dos del legendario.
    push(g, PK_SPHERE, x - 20, y - 16, float(orb_roll(g)));
    if (legend) push(g, PK_SPHERE, x + 20, y - 16, float(orb_roll(g)));
    return;
  }
  push(g, value >= 20 ? PK_XPBIG : PK_XP, x, y, value);
  if (g.rng.next() < TICKET_RATE) push(g, PK_TICKET, x + g.rng.range(-6, 6), y, 1);
  if (g.rng.next() < kOrbSphereRate) push(g, PK_SPHERE, x + g.rng.range(-6, 6), y, float(orb_roll(g)));
  // Drops ocasionales.
  const float r = g.rng.next();
  if (r < 0.012f) push(g, PK_HEAL, x + g.rng.range(-8, 8), y, 0);
  else if (r < 0.020f) push(g, PK_MAGNET, x, y, 0);
  else if (r < 0.026f) push(g, PK_BOMB, x, y, 0);
}

void pickup_drop(Game& g, int kind, float x, float y, float value) { push(g, kind, x, y, value); }

// Lo de recoger cada cosa (onCollect de game.js) está en game.cpp.
void game_on_collect(Game& g, const Pickup& p, Player& who);

void pickups_update(Game& g, float dt) {
  for (int i = g.pickups.n - 1; i >= 0; --i) {
    Pickup& p = g.pickups[i];
    p.t += dt * 4;
    // El jugador vivo más cercano (en solitario, siempre tú).
    Player* pl = nullptr;
    float d2 = 1e30f;
    for (int k = 0; k < g.nplayers; ++k) {
      Player& o = g.players[k];
      if (!o.active || (o.dead && g.nplayers > 1)) continue;
      if (p.kind == PK_SPHERE && !orb_can_take(o, int(p.value))) continue;   // (una esfera, sólo a quien le quepa)
      const float dd = dist2(o.x, o.y, p.x, p.y);
      if (dd < d2) { d2 = dd; pl = &o; }
    }
    if (!pl) { p.pulled = false; p.vx *= std::pow(0.02f, dt); p.vy *= std::pow(0.02f, dt); p.x += p.vx * dt; p.y += p.vy * dt; continue; }
    const float dx = pl->x - p.x, dy = pl->y - p.y;
    const float mag2 = pl->magnet * pl->magnet;
    if (p.pulled || d2 < mag2) {
      p.pulled = true;
      const float d = d2 > 0 ? std::sqrt(d2) : 1;
      const float pull = 240 + 1400 / d;
      p.vx = damp(p.vx, (dx / d) * pull, 7, dt);
      p.vy = damp(p.vy, (dy / d) * pull, 7, dt);
    } else {
      p.vx *= std::pow(0.02f, dt);
      p.vy *= std::pow(0.02f, dt);
    }
    p.x += p.vx * dt;
    p.y += p.vy * dt;
    if (d2 < 15 * 15) {
      const Pickup got = p;
      g.pickups.remove_ordered(i);
      game_on_collect(g, got, *pl);
    }
  }
}

}  // namespace ps

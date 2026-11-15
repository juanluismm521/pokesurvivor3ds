// hazards.cpp — ataques con aviso de los enemigos (hazards.js de la web).
// Un peligro se dibuja primero en el suelo como AVISO (se va llenando) y, al
// cumplirse el tiempo, golpea a quien esté dentro:
//   zona  círculo (zonas de Tierra/Fuego/Veneno, explosiones, caídas de salto)
//   rayo  recto desde el enemigo (Eléctrico, Psíquico, Dragón, Hielo)
// Los de Arceus quitan un % de la vida (pct) sin mirar tipos ni defensa (pure)
// y la Sentencia cae como un pilar de luz.
#include <cmath>

#include "core/abilities.h"
#include "core/game.h"

namespace ps {

Hazard* hazard_add(Game& g) {
  Hazard* h = g.hazards.push();
  if (!h) return nullptr;
  *h = Hazard();
  return h;
}

bool hazard_inside(const Hazard& h, float x, float y, float r) {
  if (h.safe) {   // todo menos los círculos
    for (int i = 0; i < h.nspots; ++i) if (dist2(x, y, h.sx[i], h.sy[i]) < h.sr[i] * h.sr[i]) return false;
    return dist2(x, y, h.x, h.y) < (h.r + r) * (h.r + r);
  }
  if (!h.beam) return dist2(x, y, h.x, h.y) < (h.r + r * 0.6f) * (h.r + r * 0.6f);
  const float ca = std::cos(h.angle), sa = std::sin(h.angle);
  const float px = x - h.x, py = y - h.y;
  const float along = px * ca + py * sa;
  if (along < -r || along > h.len + r) return false;
  return std::fabs(-px * sa + py * ca) <= h.w / 2 + r * 0.6f;
}

static void detonate(Game& g, const Hazard& h) {
  const bool near = g.camera.sees(h.x, h.y, 120);
  if (h.safe) {
    g.flash(h.color, 0.45f);
    for (int i = 0; i < h.nspots; ++i) g.fx.ring(h.sx[i], h.sy[i], 4, h.sr[i], 0xffffff, 0.4f, 4);
    if (near || true) { g.sfx(SFX_BOSS); g.camera.kick(0.9f); }
  } else if (h.pillar) {
    // Pilar de luz que cae del cielo.
    g.fx.beam(h.x, h.y, -kPi / 2, 420, h.r * 1.1f, h.color, T_ELECTRIC, SH_NONE);
    g.fx.ring(h.x, h.y, 4, h.r * 1.3f, 0xfff3c8, 0.4f, 5);
    g.fx.burst(h.x, h.y - 6, h.color, 10, 180);
    if (near) { g.sfx(SFX_JUDGE); g.camera.kick(0.2f); }
  } else if (!h.beam) {
    g.fx.ring(h.x, h.y, 4, h.r, h.color, 0.35f, 5);
    g.fx.burst(h.x, h.y - 6, h.color, int(jsround(h.r / 3)), 160);
    if (near) { g.sfx(SFX_BREAK); if (h.r > 60) g.camera.kick(0.35f); }
  } else {
    const int t = h.type >= 0 ? int(h.type) : int(h.pure ? T_ELECTRIC : T_NORMAL);
    g.fx.beam(h.x, h.y - kLift, h.angle, h.len, h.w, h.color, t, SH_NONE);   // (forType no trae forma de rayo)
    if (near) g.sfx(SFX_SHOT, K_BEAM, h.type);
  }
}

void hazards_update(Game& g, float dt) {
  for (int i = g.hazards.n - 1; i >= 0; --i) {
    Hazard& h = g.hazards[i];
    h.t += dt;
    if (!h.hit && h.t >= h.delay) {
      h.hit = true;
      detonate(g, h);
      for (int k = 0; k < g.nplayers; ++k) {   // (cada consola, al suyo: aquí, a todos)
        Player& q = g.players[k];
        // (Humedad: la explosión de un kamikaze no le daña, ver abilities.h.)
        if (q.active && !q.dead && hazard_inside(h, q.x, q.y, q.r) && !(h.boom && abil_damp(q))) {
          q.invuln = 0;
          player_hurt(g, q, h.pct > 0 ? std::fmax(h.dmg, q.max_hp * h.pct) : h.dmg, h.type, h.pure, h.cat);
        }
      }
    }
    if (h.hit && h.t >= h.delay + h.end) g.hazards.remove_ordered(i);
  }
}

}  // namespace ps

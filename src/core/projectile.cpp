// projectile.cpp — proyectiles y orbes (projectile.js de la web).
// Una sola lista para todo lo que vuela: disparos del jugador, disparos
// enemigos y los orbes que giran alrededor del Pokémon (proyectiles con `orb`,
// anclados al jugador y que vuelven a golpear cada `rehit` segundos).
#include <cmath>

#include "core/game.h"

namespace ps {

Projectile* proj_spawn(Game& g) {
  Projectile* p = g.projectiles.push();
  if (!p) return nullptr;
  *p = Projectile();
  p->id = g.next_proj_id++;
  return p;
}

// Orbe anclado a un jugador: vive mientras el movimiento esté activo.
Projectile* proj_spawn_orb(Game& g) {
  Projectile* p = proj_spawn(g);
  if (!p) return nullptr;
  p->orb = true;
  p->life = 1e30f;
  p->pierce = 1 << 30;
  return p;
}

// Borra los orbes de un jugador (o todos con owner < 0) al cambiar de movimiento.
void proj_clear_orbs(Game& g, int owner) {
  for (int i = g.projectiles.n - 1; i >= 0; --i) {
    const Projectile& p = g.projectiles[i];
    if (p.orb && (owner < 0 || p.owner == owner)) g.projectiles.remove_ordered(i);
  }
}

Enemy* nearest_enemy(Game& g, float x, float y, float max_dist) {
  Enemy* best = nullptr;
  float bd = max_dist * max_dist;
  for (Enemy& e : g.enemies) {
    if (e.dead) continue;
    const float d = dist2(x, y, e.x, e.y);
    if (d < bd) { bd = d; best = &e; }
  }
  return best;
}

void projectiles_update(Game& g, float dt) {
  for (int i = g.projectiles.n - 1; i >= 0; --i) {
    Projectile& p = g.projectiles[i];
    p.t += dt;
    const Player& o = g.players[p.owner];

    if (p.orb) {
      // Gira alrededor de su jugador.
      p.oa += p.ospeed * dt;
      p.x = o.x + std::cos(p.oa) * p.orad;
      p.y = o.y + std::sin(p.oa) * p.orad * 0.7f;
      continue;
    }

    if (p.t >= p.life) { g.projectiles.remove_ordered(i); continue; }

    // Bumerán: a los `boom` segundos se da la vuelta (y puede volver a golpear
    // a los mismos) y vuelve a su dueño; al llegar, desaparece.
    if (p.boom > 0) {
      if (!p.back && p.t >= p.boom) { p.back = true; p.nhits = 0; }
      if (p.back) {
        float dx, dy;
        norm(o.x - p.x, o.y - p.y, dx, dy);
        p.vx = damp(p.vx, dx * p.bsp, 7, dt);
        p.vy = damp(p.vy, dy * p.bsp, 7, dt);
        if (dist2(p.x, p.y, o.x, o.y) < (o.r + 8) * (o.r + 8)) { g.projectiles.remove_ordered(i); continue; }
      }
    }

    // Teledirigido: curva la velocidad hacia el enemigo más cercano.
    if (p.homing > 0 && p.friendly) {
      if (Enemy* tg = nearest_enemy(g, p.x, p.y, 340)) {
        float dx, dy;
        norm(tg->x - p.x, tg->y - p.y, dx, dy);
        const float sp = std::hypot(p.vx, p.vy);
        p.vx = damp(p.vx, dx * sp, p.homing, dt);
        p.vy = damp(p.vy, dy * sp, p.homing, dt);
      }
    }

    p.px = p.x; p.py = p.y; p.has_prev = true;   // para la colisión barrida (combat)
    if (p.has_vis && p.vis_trail != TR_NONE && g.camera.sees(p.x, p.y, 20)) {
      p.trail_t -= dt;
      if (p.trail_t <= 0) { p.trail_t = 0.04f; g.fx.trail(p.vis_trail, p.vis_pal, p.x, p.y - kLift, p.vx, p.vy); }
    }
    p.x += p.vx * dt;
    p.y += p.vy * dt;

    // Fuera de la vista con margen generoso → se descarta.
    if (!g.camera.sees(p.x, p.y, 260)) { g.projectiles.remove_ordered(i); continue; }
  }
}

}  // namespace ps

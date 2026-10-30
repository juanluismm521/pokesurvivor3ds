// interact.cpp — altares, manantiales, cofres con candado y trampas (interact.js).
//   altar     da un poder de 25 s (Furia, Rayo, Imán o Prisa) y se apaga
//   spring    cura mientras estás encima; se agota y se recarga sola
//   chest     (del port, 2026-10-30) se abre al pasar por encima, no a golpes
//   bigchest  quédate a su lado 3,5 s y se abre (desde un poco más lejos que en la web)
//   trap      salta al pisarla (tú o un enemigo): veneno, pegajosa o explosiva
#include <cmath>
#include <cstdio>

#include "core/game.h"

namespace ps {

namespace {
constexpr float CHEST_TIME = 3.5f;
constexpr float CHEST_TOUCH = 8;             // cofre pequeño: se abre al tocarlo
constexpr float BIG_R_MIN = 54, BIG_R_PAD = 30;   // cofre grande: radio (la web, 46 y 24)
constexpr float POWER_TIME = 25;
constexpr float TRAP_R = 74;

void spring(Game& g, Player& pl, Prop& p, float d2, float dt) {
  if (p.charge < 1) p.charge = std::fmin(1.0f, p.charge + dt / 45);
  if (pl.dead || d2 > 26 * 26 || p.charge <= 0.02f || pl.hp >= pl.max_hp) return;
  const float before = pl.hp;
  pl.hp = std::fmin(pl.max_hp, pl.hp + pl.max_hp * 0.14f * dt);
  g.heal_acc += pl.hp - before;
  p.charge = std::fmax(0.0f, p.charge - dt * 0.09f);
  if (g.vrng.next() < dt * 14) {
    PxOpts o; o.vy = -50; o.life = 0.5f;
    g.fx.px(pl.x + g.vrng.range(-pl.r, pl.r), pl.y - g.vrng.range(0, pl.body_h), 0x7dffb0, o);
  }
  g.heal_shown -= dt;
  if (g.heal_shown <= 0 && g.heal_acc >= 1) {
    g.heal_shown = 0.6f;
    char b[16];
    std::snprintf(b, sizeof(b), "+%d", int(jsround(g.heal_acc)));
    g.fx.dmg_text(pl.x, pl.y - pl.body_h - 6, b, 0x5fe08a, false, true);
    g.heal_acc = 0;
    g.sfx(SFX_XP);
  }
  if (p.charge <= 0.02f) g.say("El manantial se ha secado (se recarga solo)", 2.2f);
}

void use_power(Game& g, Player& pl, Prop& p) {
  p.used = true;
  const PowerDef& pw = kPowers[p.kind];
  player_add_power(pl, p.kind, POWER_TIME);
  g.fx.ring(p.x, p.y - 20, 8, 90, pw.color, 0.6f, 5);
  g.fx.burst(pl.x, pl.y - pl.body_h * 0.5f, pw.color, 24, 200);
  g.sfx(SFX_LEGEND);
  char b[96];
  std::snprintf(b, sizeof(b), "¡Poder %s! %s durante %d s", pw.name, pw.desc, int(POWER_TIME));
  g.say(b, 3);
}
}  // namespace

void interact_update(Game& g, Player& pl, float dt) {
  // Cofres pequeños: se abren al tocarlos.
  if (!pl.dead) {
    Prop* near[8];
    const int nc = g.world.props_in_circle(pl.x, pl.y, pl.r + CHEST_TOUCH, near, 8);
    for (int i = 0; i < nc; ++i) if (near[i]->type == PR_CHEST && !near[i]->gone) g.world.break_prop(near[i]);
  }
  Prop* list[64];
  const int n = g.world.specials_near(pl.x, pl.y, 720, list, 64);
  for (int i = 0; i < n; ++i) {
    Prop& p = *list[i];
    if (p.gone) continue;
    const float d2 = dist2(pl.x, pl.y, p.x, (p.type == PR_SPRING || p.type == PR_TRAP) ? p.y - 8 : p.y);
    switch (p.type) {
      case PR_SPRING: spring(g, pl, p, d2, dt); break;
      case PR_ALTAR: {
        // Contando el tamaño de tu Pokémon (uno grande chocaba antes de llegar).
        const float rr = std::fmax(30.0f, p.r + pl.r + 14);
        if (!p.used && !pl.dead && d2 < rr * rr) use_power(g, pl, p);
        break;
      }
      case PR_BIGCHEST: {
        const float rr = std::fmax(BIG_R_MIN, p.r + pl.r + BIG_R_PAD);
        if (!pl.dead && d2 < rr * rr) {
          if (p.open == 0) g.sfx(SFX_SELECT);
          p.open += dt;
          if (p.open >= CHEST_TIME) { p.open = 0; g.world.break_prop(&p); }
        } else if (p.open > 0) p.open = std::fmax(0.0f, p.open - dt * 1.5f);
        break;
      }
      case PR_TRAP: {
        if (!pl.dead && d2 < 13 * 13) { g.world.break_prop(&p); break; }
        Enemy* es[kMaxEnemies];
        const int ne = g.query_circle(p.x, p.y - 8, 3, es, kMaxEnemies);
        bool any = false;
        for (int k = 0; k < ne; ++k) if (!es[k]->flying) any = true;
        if (any) g.world.break_prop(&p);
        break;
      }
      default: break;
    }
  }
}

// Una trampa salta: daña a los enemigos de alrededor y, si estás dentro, a ti.
void interact_trap(Game& g, const Prop& p) {
  const float x = p.x, y = p.y - 8;
  const int k = p.kind;
  const uint32_t cols[3] = {0xc86bdc, 0x8bd94a, 0xff7b3d};
  const int pals[3] = {T_POISON, T_GRASS, T_FIRE};
  const uint32_t col = cols[k];
  g.fx.ring(x, y, 6, TRAP_R, col, 0.5f, k == 2 ? 7 : 4);
  g.fx.burst(x, y - 6, col, k == 2 ? 40 : 22, k == 2 ? 260 : 140);
  for (int i = 0; i < (k == 2 ? 10 : 7); ++i) {
    const float a = g.vrng.next() * kTau, r = g.vrng.range(8, TRAP_R * 0.8f);
    SpOpts o; o.vy = k == 2 ? -30 : -45; o.life = g.vrng.range(0.4f, 0.8f);
    g.fx.sp(k == 2 ? SH_IMPACT : SH_BLOB, pals[k], x + std::cos(a) * r, y + std::sin(a) * r * 0.7f - 6, o);
  }
  g.sfx(k == 2 ? SFX_BOSS : SFX_SURGE);
  if (k == 2 && dist2(x, y, g.camera.x, g.camera.y) < 500 * 500) g.camera.kick(0.7f);

  Player& pl = g.pl();
  const int lvl = pl.active ? pl.level : 1;
  Enemy* es[kMaxEnemies];
  const int ne = g.query_circle(x, y, TRAP_R, es, kMaxEnemies);
  int32_t ids[kMaxEnemies];
  for (int i = 0; i < ne; ++i) ids[i] = es[i]->id;
  for (int i = 0; i < ne; ++i) {
    Enemy* e = g.enemy_by_id(ids[i]);
    if (!e) continue;
    if (k == 0) { enemy_apply_poison(*e, 5); enemy_hurt(g, *e, 14 + lvl * 3.0f, 0xd47cf0); }
    else if (k == 1) { enemy_apply_slow(*e, 0.65f, 4.5f); enemy_hurt(g, *e, 4, 0x8bd94a); }
    else enemy_hurt(g, *e, 90 + lvl * 18.0f, 0xff7b3d, e->x - x, e->y - y, 220);
  }
  if (pl.active && !pl.dead && dist2(x, y, pl.x, pl.y) < (TRAP_R * 0.7f) * (TRAP_R * 0.7f)) {
    pl.invuln = 0;
    if (k == 0) player_hurt(g, pl, pl.max_hp * 0.1f);
    else if (k == 1) { pl.slow_t = 3.5f; g.fx.dmg_text(pl.x, pl.y - pl.body_h - 6, "¡Pegajoso!", 0x8bd94a); }
    else player_hurt(g, pl, pl.max_hp * 0.2f);
  }
}

}  // namespace ps

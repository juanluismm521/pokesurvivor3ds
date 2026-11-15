// eterna.cpp — Eternatus Eternamax (eterna.js de la web, Gran Actualización 13-11-2026).
// El jefe del minuto 20 (en el modo rápido, el 13: game_teq). El cielo se tiñe de rojo y te
// lleva a una arena; la pelea tiene tres fases:
//   1 DESPERTAR          Rayo Infinito (barrido de rayos), Bomba Lodo (charcos), Energía Dinamax
//                        (anillos de disparos) y, de cerca, Pulso Dragón.
//   2 ESCUDO DINAMAX     (a 2/3 de vida) es inmune hasta que rompes 4 núcleos que giran unidos
//                        por una barrera de láseres que se apaga a ratos; las nubes rojas cierran
//                        la arena; suma el Cañón Dinamax.
//   3 ETERNAMAX FURIOSO  (a 1/3) Espiral Infinita, Noche Oscura (sólo te salvas en los círculos),
//                        Remolino Infinito (te arrastra) y, al 10 %, el RAYO ETERNAMAX: rompe el
//                        Núcleo Eterno en 8 s y queda aturdido (+50 % de daño); si no, a los círculos.
// A los 5 min se enfurece (más rápido y más daño). Si caéis todos, se acaba la partida; si
// ganas: ticket ×10, 600 monedas, la XP de un gran jefe y un poder para el resto de la partida.
// Sus golpes quitan un % de tu vida (como los de Arceus), según la dificultad.
#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/game.h"
#include "core/power.h"

namespace ps {

namespace {
constexpr float TS = kTS;
constexpr int ARENA_R = 15;
constexpr uint32_t RED = 0xff2a4a, DARK = 0xb0153a, PINK = 0xe04aff, PALE = 0xffd6e0;
constexpr float INTRO = 3.2f;
constexpr float kEternaScale = 0.85f;
// Barrera de láseres (fase 2): enciende `on` s de cada `cycle`, avisa `warn` s antes.
constexpr float W_WARM = 2, W_CYCLE = 5, W_ON = 3.4f, W_WARN = 0.7f, W_HIT = 0.08f, W_CD = 0.6f, W_W = 8;
constexpr float CORE_SPIN = kTau / 26;
// Charcos de Bomba Lodo
constexpr float POOL_R = 50, POOL_LIFE = 7, POOL_HIT = 0.045f, POOL_TICK = 0.5f;
// Remolino Infinito
constexpr float PULL_DELAY = 0.35f, PULL_DUR = 3.1f, PULL_K = 0.55f;
// Rayo Eternamax
constexpr float ULT_AT = 0.1f, ULT_TIME = 8, ULT_CORE = 0.025f, ULT_STUN = 6, ULT_VULN = 0.5f;
enum Cast : int8_t { C_NONE = -1, C_SWEEP, C_SWEEP2, C_SLUDGE, C_ORBS, C_NOVA, C_CANNON, C_SPIRAL, C_NIGHT, C_PULL };
const char* const kCastName[] = {"Rayo Infinito", "Rayo Infinito", "Bomba Lodo", "Energía Dinamax", "Pulso Dragón", "Cañón Dinamax", "Espiral Infinita", "Noche Oscura", "Remolino Infinito"};

Game::EternaState& E(Game& g) { return g.eterna; }

void banner(Game& g, const char* title, const char* sub, const char* info) {
  auto& s = E(g);
  std::snprintf(s.b_title, sizeof(s.b_title), "%s", title);
  std::snprintf(s.b_sub, sizeof(s.b_sub), "%s", sub);
  std::snprintf(s.b_info, sizeof(s.b_info), "%s", info);
  s.b_t = 0;
  char b[200];
  std::snprintf(b, sizeof(b), "%s · %s", title, sub);
  g.say(b, 3.2f);
}

Enemy* boss(Game& g) { Enemy* e = g.enemy_by_id(E(g).boss_id); return e && !e->dead ? e : nullptr; }

Player* nearest(Game& g, float x, float y) {
  Player* best = nullptr; float bd = 1e30f;
  for (int i = 0; i < g.nplayers; ++i) {
    Player& p = g.players[i];
    if (!p.active || p.dead) continue;
    const float d = dist2(p.x, p.y, x, y);
    if (d < bd) { bd = d; best = &p; }
  }
  return best;
}

// % de la vida que quita cada golpe (enfurecido, ×1,3; la dificultad lo multiplica).
float pct(Game& g, float k) { return std::fmin(0.62f, k * (E(g).enraged ? 1.3f : 1) * kDiffModes[g.cfg.diff].dmg); }

float avg_max_hp(Game& g) {
  float s = 0; int n = 0;
  for (int i = 0; i < g.nplayers; ++i) if (g.players[i].active && !g.players[i].dead) { s += g.players[i].max_hp; n++; }
  return n ? s / n : 200;
}

Hazard* hz(Game& g) {
  Hazard* h = hazard_add(g);
  if (!h) return nullptr;
  h->type = T_DRAGON; h->pure = true; h->dmg = 1; h->color = RED;
  return h;
}

// Radio de la arena con las nubes rojas (se cierra en 20 s desde la fase 2): 0 = sin nubes.
float cloud_r(const Game& g) {
  const auto& s = g.eterna;
  if (s.wall_t < 0) return 0;
  return (13.5f - clamp(s.wall_t / 20, 0.0f, 1.0f) * 4.5f) * TS;
}

void core_pos(const Game& g, int slot, float& x, float& y) {
  const auto& s = g.eterna;
  const float a = kPi / 4 + slot * kPi / 2 + std::fmax(0.0f, s.wall_t) * CORE_SPIN, r = TS * 7.5f;
  x = s.cx + std::cos(a) * r; y = s.cy + std::sin(a) * r * 0.8f;
}

// 1 encendida, -1 avisando, 0 apagada
int barrier(const Game& g) {
  const auto& s = g.eterna;
  if (s.phase != 2 || s.wall_t < W_WARM) return 0;
  const float k = std::fmod(s.wall_t - W_WARM, W_CYCLE);
  return k < W_ON ? 1 : k > W_CYCLE - W_WARN ? -1 : 0;
}

void shot(Game& g, Enemy& e, float a, float sp, float k = 1) {
  Projectile* s = proj_spawn(g);
  if (!s) return;
  s->x = e.x; s->y = e.y - e.body_h * 0.35f; s->vx = std::cos(a) * sp; s->vy = std::sin(a) * sp;
  s->dmg = avg_max_hp(g) * 0.11f * k * (E(g).enraged ? 1.3f : 1) * kDiffModes[g.cfg.diff].dmg;
  s->r = 9; s->life = 4; s->friendly = false; s->color = RED;
  s->mtype = T_DRAGON; s->mcat = CAT_SPECIAL;
  s->has_vis = true; s->vis_proj = kTypeShape[T_DRAGON]; s->vis_pal = T_DRAGON;
}

void add_pool(Game& g, float x, float y, float wait) {
  auto& s = E(g);
  if (s.npools >= 12) { for (int i = 0; i < 11; ++i) s.pools[i] = s.pools[i + 1]; s.npools = 11; }
  s.pools[s.npools++] = {x, y, wait, 0};
}

// Zona que lo cubre todo menos unos círculos (Noche Oscura, Rayo Eternamax).
void safe_zone(Game& g, int spots, float sr, float minsep, float rmin, float rmax, float delay, float k) {
  auto& s = E(g);
  Hazard* h = hz(g);
  if (!h) return;
  h->safe = true; h->x = s.cx; h->y = s.cy; h->r = ARENA_R * TS; h->delay = delay; h->pct = pct(g, k);
  const float R = cloud_r(g) > 0 ? cloud_r(g) : TS * 13.5f;
  for (int t = 0; t < 60 && h->nspots < spots; ++t) {
    const float a = g.rng.next() * kTau, d = (rmin + g.rng.next() * (rmax - rmin)) * R;
    const float x = s.cx + std::cos(a) * d, y = s.cy + std::sin(a) * d;
    bool ok = true;
    for (int i = 0; i < h->nspots; ++i) ok = ok && dist2(h->sx[i], h->sy[i], x, y) > minsep * minsep;
    if (ok) { h->sx[h->nspots] = x; h->sy[h->nspots] = y; h->sr[h->nspots] = sr; h->nspots++; }
  }
}

void cast(Game& g, Enemy& e, int id, const Player& tg) {
  auto& s = E(g);
  s.last = int8_t(id);
  s.cast = int8_t(id); s.ct = 0; s.wind = 1; s.emit = 0; s.cn = 0; s.cdone = false;
  const float aim = std::atan2(tg.y - e.y, tg.x - e.x);
  switch (id) {
    case C_SWEEP: case C_SWEEP2: {
      const float dir = g.rng.next() < 0.5f ? 1.0f : -1.0f;
      constexpr int N = 18; constexpr float ARC = 2.6f;
      for (float off : {0.0f, kPi}) {
        if (off > 0 && id == C_SWEEP) break;
        for (int i = 0; i < N; ++i) {
          Hazard* h = hz(g);
          if (!h) break;
          h->beam = true; h->x = e.x; h->y = e.y; h->angle = aim + off - dir * ARC / 2 + dir * i * ARC / N;
          h->len = 1500; h->w = 44; h->delay = 1.05f + i * 0.065f; h->pct = pct(g, 0.28f);
        }
      }
      s.wind = 1.05f + N * 0.065f;
      e.anim.play(A_CHARGE, 0.8f);
      break;
    }
    case C_SLUDGE:
      for (int k = 0; k < g.nplayers; ++k) {
        const Player& p = g.players[k];
        if (!p.active || p.dead) continue;
        const int pool2 = 1 + int(g.rng.next() * 6);
        for (int i = 0; i < 7; ++i) {
          const float a = g.rng.next() * kTau, d = i ? g.rng.range(50, 170) : 0;
          const float x = p.x + std::cos(a) * d, y = p.y + std::sin(a) * d * 0.8f;
          Hazard* h = hz(g);
          if (h) { h->x = x; h->y = y; h->r = 54; h->delay = 1.1f + i * 0.08f; h->pct = pct(g, 0.2f); h->color = 0xb04fe0; h->type = T_POISON; }
          if (i == 0 || i == pool2) add_pool(g, x, y, 1.1f + i * 0.08f);
        }
      }
      s.wind = 0.9f;
      e.anim.play(A_SHOOT, 0.9f);
      break;
    case C_ORBS: s.wind = 1.1f; e.anim.play(A_SHOOT, 0.8f); break;
    case C_NOVA: {
      Hazard* h = hz(g);
      if (h) { h->x = e.x; h->y = e.y; h->r = 240; h->delay = 0.9f; h->pct = pct(g, 0.3f); h->color = PINK; }
      s.wind = 0.9f;
      e.anim.play(A_CHARGE, 1.2f);
      break;
    }
    case C_CANNON: {
      Hazard* h = hz(g);
      if (h) { h->beam = true; h->x = e.x; h->y = e.y; h->angle = aim; h->len = 1800; h->w = 120; h->delay = 1.75f; h->pct = pct(g, 0.42f); }
      s.wind = 1.75f;
      e.anim.play(A_CHARGE, 0.55f);
      break;
    }
    case C_SPIRAL: s.wind = 3.4f; s.cdir = g.rng.next() < 0.5f ? 1.0f : -1.0f; e.anim.play(A_SHOOT, 0.6f); break;
    case C_NIGHT:
      safe_zone(g, 3, 78, 220, 0, 0.7f, 2.5f, 0.5f);
      s.wind = 2.5f;
      e.anim.play(A_CHARGE, 0.5f);
      break;
    case C_PULL: {
      Hazard* h = hz(g);
      if (h) { h->x = e.x; h->y = e.y; h->r = 250; h->delay = 3.6f; h->pct = pct(g, 0.34f); h->color = PINK; }
      s.wind = 3.6f;
      s.pull_t = PULL_DUR; s.pull_delay = PULL_DELAY;
      e.anim.play(A_CHARGE, 0.5f);
      break;
    }
    default: break;
  }
}

int pick_cast(Game& g, float dist) {
  auto& s = E(g);
  if (dist < 230 && s.last != C_NOVA && g.rng.next() < 0.55f) return C_NOVA;
  static const int8_t P1[] = {C_SWEEP, C_SLUDGE, C_ORBS}, P2[] = {C_CANNON, C_SWEEP, C_SLUDGE, C_ORBS},
                      P3[] = {C_SPIRAL, C_NIGHT, C_CANNON, C_SWEEP2, C_SLUDGE, C_PULL};
  const int8_t* l = s.phase == 1 ? P1 : s.phase == 2 ? P2 : P3;
  const int n = s.phase == 1 ? 3 : s.phase == 2 ? 4 : 6;
  int8_t c[6]; int nc = 0;
  for (int i = 0; i < n; ++i) if (l[i] != s.last) c[nc++] = l[i];
  return c[int(g.rng.next() * nc)];
}

void tick_cast(Game& g, Enemy& e, float dt, const Player* tg) {
  auto& s = E(g);
  s.ct += dt;
  if (tg) e.anim.dir = int8_t(dir_from_angle(std::atan2(tg->y - e.y, tg->x - e.x)));
  if (s.cast == C_ORBS) {
    static const float AT[2] = {0.6f, 1};
    for (int k = 0; k < 2; ++k) if (s.cn == k && s.ct >= AT[k]) {
      s.cn++;
      const int n = 16 + s.phase * 2;
      for (int i = 0; i < n; ++i) shot(g, e, (i + k * 0.5f) * kTau / n, 150.0f + s.phase * 15);
      g.sfx(SFX_SHOT, K_PROJECTILE, T_DRAGON);
    }
  } else if (s.cast == C_PULL) {
    static const float AT[2] = {1, 2.2f};
    for (int k = 0; k < 2; ++k) if (s.cn == k && s.ct >= AT[k]) {
      s.cn++;
      for (int i = 0; i < 12; ++i) shot(g, e, (i + k * 0.5f) * kTau / 12, 125, 0.8f);
    }
  } else if (s.cast == C_SPIRAL && s.ct > 0.3f && s.ct < s.wind - 0.2f && (s.emit -= dt) <= 0) {
    s.emit = s.enraged ? 0.08f : 0.11f;
    s.spin += s.cdir * 0.27f;
    for (int i = 0; i < 4; ++i) shot(g, e, s.spin + i * kPi / 2, 165, 0.8f);
  }
  if (s.ct >= s.wind && !s.cdone) {
    s.cdone = true;
    e.anim.play(s.cast == C_NOVA || s.cast == C_NIGHT ? A_ATTACK : A_SHOOT, 1.1f);
    if (s.cast == C_CANNON || s.cast == C_NIGHT) g.camera.kick(0.9f);
  }
  if (s.ct >= s.wind + 0.4f) {
    s.cast = C_NONE;
    s.cd = (s.phase == 1 ? 2.2f : s.phase == 2 ? 2.3f : 1.55f) + g.rng.next() * 0.5f;
  }
}

void set_phase(Game& g, Enemy& e, int ph) {
  auto& s = E(g);
  const int was = s.phase;
  s.phase = int8_t(ph);
  if (ph == 2 && was < 2) {
    s.wall_t = 0;
    banner(g, "FASE 2", "ESCUDO DINAMAX", "Rompe los 4 núcleos · la barrera se apaga a ratos: crúzala entonces · las nubes rojas cierran la arena");
    g.camera.kick(0.8f);
    g.flash(PINK, 0.4f);
    e.immune = true;
    for (int i = 0; i < 4; ++i) {
      EnemyDef d; d.dex = 890; d.hp = float(jsround(e.max_hp * 0.055f)); d.xp = 200; d.behavior = BH_CHASE;
      float x, y; core_pos(g, i, x, y);
      Enemy* c = enemy_spawn(g, d, x, y, Scale3(), false);
      if (!c) continue;
      c->ecore = true; c->slot = int8_t(i); c->flying = true; c->r = 24; c->body_h = 64;
      c->types[0] = T_DRAGON; c->types[1] = T_NONE; c->atk_type = T_DRAGON;
      s.core_ids[i] = c->id;
    }
    s.cast = C_NONE; s.cd = 1.4f;
  }
  if (ph == 3 && was < 3) {
    banner(g, "FASE 3", "ETERNAMAX FURIOSO", "Cuidado con el Remolino Infinito: corre en contra");
    g.flash(RED, 0.6f);
    g.camera.kick(1.2f);
    s.cast = C_NONE; s.cd = 1.2f;
  }
}

void start_ult(Game& g, Enemy& e) {
  auto& s = E(g);
  s.ult = 1; s.ult_t = 0;
  e.immune = true;
  s.cast = C_NONE; s.cd = 99;
  EnemyDef d; d.dex = 890; d.hp = std::fmax(1.0f, float(jsround(e.max_hp * ULT_CORE))); d.xp = 300; d.behavior = BH_CHASE;
  Enemy* c = enemy_spawn(g, d, s.cx, s.cy + TS * 2.5f, Scale3(), false);
  if (c) {
    c->ecore = true; c->ult = true; c->slot = -1; c->flying = true; c->r = 24; c->body_h = 64;
    c->types[0] = T_DRAGON; c->types[1] = T_NONE;
    c->home_x = c->x; c->home_y = c->y;
    s.ucore_id = c->id;
  }
  banner(g, "GOLPE FINAL", "¡RAYO ETERNAMAX!", "Rompe el Núcleo Eterno antes de que dispare");
  g.camera.kick(0.8f);
  g.flash(RED, 0.5f);
}

void end_ult(Game& g, Enemy& e, bool broke) {
  auto& s = E(g);
  s.ult = 2;
  e.immune = false;
  if (broke) {
    s.stun_t = ULT_STUN;
    e.vuln_t = ULT_STUN;
    banner(g, "¡NÚCLEO ETERNO ROTO!", "¡ETERNATUS ATURDIDO!", "¡Ahora! Recibe un 50 % más de daño unos segundos");
    g.camera.kick(1);
    g.flash(0xffffff, 0.5f);
    s.cast = C_NONE; s.cd = ULT_STUN + 0.5f;
    return;
  }
  safe_zone(g, 2, 82, 260, 0.3f, 0.75f, 2.2f, 0.6f);
  banner(g, "GOLPE FINAL", "¡RAYO ETERNAMAX!", "¡Ponte en uno de los círculos!");
  g.camera.kick(0.6f);
  s.cast = C_NONE; s.cd = 3.6f;
}

void keep_in(float cx, float cy, float R, float& x, float& y) {
  const float dx = x - cx, dy = y - cy, d = std::hypot(dx, dy);
  if (d > R) { x = cx + dx / d * R; y = cy + dy / d * R; }
}

void stash(Game& g) {
  g.stashed.clear();
  for (const Enemy& e : g.enemies) if (!e.dead && !g.stashed.full()) *g.stashed.push() = e;
  g.enemies.clear(); g.corpses.clear();
  g.has_stash = true;
}
void restore(Game& g) {
  g.enemies.clear(); g.corpses.clear();
  for (const Enemy& e : g.stashed) { Enemy* n = g.enemies.push(); if (n) *n = e; }
  g.stashed.clear();
  g.has_stash = false;
}

void begin(Game& g) {
  auto& s = E(g);
  s = Game::EternaState();
  s.st = 1; s.happened = true;
  const Player& pl = g.pl();
  s.back_x = pl.x; s.back_y = pl.y; s.back_biome = g.world.biome();
  s.tx = 9000 + int32_t(std::floor(g.rng.next() * 2000));
  s.ty = -9000 - int32_t(std::floor(g.rng.next() * 2000));
  s.cx = s.tx * TS + TS / 2; s.cy = s.ty * TS + TS / 2;
  g.rift.open = false;
  stash(g);
  g.projectiles.clear(); combat_clear_fields(g); g.hazards.clear(); g.pickups.clear();
  weather_clear(g);
  const int th = arena_theme_index("dragon");
  g.world.set_arena(true, s.tx, s.ty, ARENA_R, th >= 0 ? th : 0);
  g.world.set_biome(kArenaBiome0 + (th >= 0 ? th : 0), true);
  for (int i = 0; i < g.nplayers; ++i) {
    Player& p = g.players[i];
    p.x = s.cx + (i - (g.nplayers - 1) / 2.0f) * 34; p.y = s.cy + TS * 5;
    p.invuln = std::fmax(p.invuln, INTRO + 1);
  }
  g.views_snap();
  // Eternatus Eternamax (vida: difficulty.js eternaHp)
  EnemyDef d;
  d.dex = 890;
  d.hp = float(jsround(diff_eterna_hp(g.cfg.diff, g.team_size())));
  d.spd = 0; d.dmg = 0; d.xp = float(jsround(1500 + game_teq(g) * 1.5f)); d.behavior = BH_CHASE;
  Enemy* e = enemy_spawn(g, d, s.cx, s.cy - TS * 3, Scale3(), true);
  if (e) {
    // (en la web, a ×2 con la cámara alejada; en la pantalla de la 3DS, más pequeño)
    e->eterna = true; e->flying = true; e->scale = kEternaScale;
    e->anim.init(kSpriteEternamax, kEternaScale);
    e->r = body_radius(kSpriteEternamax, kEternaScale) * 0.75f;
    e->body_h = body_height(kSpriteEternamax, kEternaScale);
    e->floor_k = 2.0f / 3;
    e->anim.play(A_ATTACK, 0.7f);
    s.boss_id = e->id;
  }
  s.cd = INTRO + 2.2f;
  s.phase = 1;
  g.flash(RED, 0.9f);
  g.camera.kick(1.4f);
  g.sfx(SFX_BOSS);
  banner(g, "¡ETERNATUS ETERNAMAX!", "El cielo se tiñe de rojo…", "Derrótalo y elige un poder · si caes, se acaba la partida");
}

void won(Game& g) {
  auto& s = E(g);
  if (s.st != 2) return;
  s.st = 3; s.t = 0;
  g.hazards.clear();
  for (int i = g.projectiles.n - 1; i >= 0; --i) if (!g.projectiles[i].friendly) g.projectiles.remove_ordered(i);
  for (Enemy& e : g.enemies) if (e.ecore && !e.dead) enemy_die(g, e);
  s.npools = 0; s.pull_t = 0; s.ult = 0; s.stun_t = 0;
  pickups_pull_all(g);
  g.flash(0xffffff, 0.8f);
  g.camera.kick(1.2f);
  g.sfx(SFX_LEGEND);
  banner(g, "¡VICTORIA!", "ETERNATUS ETERNAMAX", "¡Has vencido a Eternatus Eternamax!");
  Player& team = g.pl();
  team.t10 += 1;
  team.coins += 600;
  g.eterna_wins++;
}

void finish(Game& g) {
  auto& s = E(g);
  s.st = 0;
  g.world.set_arena(false);
  g.world.set_biome(s.back_biome, true);
  weather_clear(g);
  restore(g);
  g.projectiles.clear(); combat_clear_fields(g); g.hazards.clear();
  for (int i = 0; i < g.nplayers; ++i) { g.players[i].x = s.back_x; g.players[i].y = s.back_y; }
  for (int i = 0; i < g.nplayers; ++i) if (g.players[i].active && g.players[i].dead) player_revive(g, g.players[i], 0.3f);
  g.views_snap();
  g.flash(RED, 0.8f);
  // El poder para el resto de la partida (core/power.h): cada uno elige el suyo.
  for (int i = 0; i < g.nplayers; ++i) if (g.players[i].active) power_offer(g, g.players[i]);
}

void hurt_pct(Game& g, Player& p, float k, int type) {
  p.invuln = 0;
  player_hurt(g, p, p.max_hp * k * kDiffModes[g.cfg.diff].dmg, type, true);
}

void fight(Game& g, float dt) {
  auto& s = E(g);
  s.ft += dt;
  if (s.wall_t >= 0) s.wall_t += dt;
  if (s.stun_t > 0) s.stun_t -= dt;
  if (s.ult == 1) s.ult_t += dt;
  if (!s.enraged && s.ft >= 300) { s.enraged = true; g.say("¡Eternatus se ha enfurecido del todo! Acaba con él ya", 3.4f); }
  Enemy* e = boss(g);
  // Todos dentro de la arena (y de las nubes)
  const float R = TS * 13.6f;
  for (Enemy& q : g.enemies) if (!q.dead) keep_in(s.cx, s.cy, R, q.x, q.y);
  for (int i = 0; i < g.nplayers; ++i) if (g.players[i].active) keep_in(s.cx, s.cy, R, g.players[i].x, g.players[i].y);
  if (!e) { if (s.ft > 1) won(g); return; }
  // (pruebas y capturas: debug_win, «ganar» en el autotest: todo cae deprisa)
  if (g.debug_win && s.ft > 3) {
    if (!e->immune) enemy_hurt(g, *e, e->max_hp * 0.025f * dt, 0xffffff, 0, 0, 0, EFF_NONE);
    for (Enemy& c : g.enemies) if (c.ecore && !c.dead) enemy_hurt(g, c, c.max_hp * 0.2f * dt, 0xffffff, 0, 0, 0, EFF_NONE);
    if (!(e = boss(g))) return;
  }
  // Fases (la vida no baja del suelo de la fase hasta que cambia)
  e->floor_k = s.phase == 1 ? 2.0f / 3 : s.phase == 2 ? 1.0f / 3 : s.ult == 2 ? 0 : ULT_AT;
  if (s.phase == 1 && e->hp <= e->max_hp * (2.0f / 3) + 1) set_phase(g, *e, 2);
  else if (s.phase == 2 && !e->immune && e->hp <= e->max_hp * (1.0f / 3) + 1) set_phase(g, *e, 3);
  if (s.phase == 2 && e->immune) {
    bool any = false;
    for (int i = 0; i < 4; ++i) { const Enemy* c = g.enemy_by_id(s.core_ids[i]); any = any || (c && !c->dead); }
    if (!any) { e->immune = false; g.say("¡Escudo roto! Eternatus ya no es inmune", 3); }
  }
  if (s.phase == 3 && !s.ult && e->hp <= e->max_hp * ULT_AT + 1) start_ult(g, *e);
  if (s.ult == 1) {
    const Enemy* c = g.enemy_by_id(s.ucore_id);
    if (!c || c->dead) end_ult(g, *e, true);
    else if (s.ult_t >= ULT_TIME) {
      if (Enemy* cc = g.enemy_by_id(s.ucore_id)) { cc->dropped = true; enemy_die(g, *cc); }
      end_ult(g, *e, false);
    }
  }
  // Tocarle: un 18 % (una vez por segundo)
  s.touch_cd -= dt;
  for (int i = 0; i < g.nplayers && s.touch_cd <= 0; ++i) {
    Player& p = g.players[i];
    if (!p.active || p.dead) continue;
    const float rr = e->r + p.r;
    if (dist2(e->x, e->y, p.x, p.y) > rr * rr) continue;
    s.touch_cd = 1;
    hurt_pct(g, p, 0.18f * (s.enraged ? 1.3f : 1), T_DRAGON);
    g.camera.kick(0.6f);
  }
  // Nubes rojas: fuera de su círculo, un 7 % cada medio segundo
  const float cr = cloud_r(g);
  s.cloud_cd -= dt;
  if (cr > 0 && s.cloud_cd <= 0)
    for (int i = 0; i < g.nplayers; ++i) {
      Player& p = g.players[i];
      if (!p.active || p.dead || dist(p.x, p.y, s.cx, s.cy) <= cr) continue;
      s.cloud_cd = 0.5f;
      hurt_pct(g, p, 0.07f, T_DRAGON);
    }
  // Barrera de láseres entre núcleos vecinos
  s.laser_cd -= dt;
  if (s.laser_cd <= 0 && barrier(g) == 1) {
    for (int i = 0; i < g.nplayers && s.laser_cd <= 0; ++i) {
      Player& p = g.players[i];
      if (!p.active || p.dead) continue;
      const float py = p.y - std::fmin(30.0f, p.body_h * 0.35f);
      for (int k = 0; k < 4; ++k) {
        const Enemy* a = g.enemy_by_id(s.core_ids[k]);
        const Enemy* b = g.enemy_by_id(s.core_ids[(k + 1) & 3]);
        if (!a || a->dead || !b || b->dead) continue;
        const float dx = b->x - a->x, dy = b->y - a->y, l2 = dx * dx + dy * dy;
        const float tt = l2 > 0 ? clamp(((p.x - a->x) * dx + (py - (a->y - 26)) * dy) / l2, 0.0f, 1.0f) : 0;
        const float qx = a->x + dx * tt - p.x, qy = a->y - 26 + dy * tt - py;
        if (qx * qx + qy * qy > (p.r + W_W) * (p.r + W_W)) continue;
        s.laser_cd = W_CD;
        hurt_pct(g, p, W_HIT, T_DRAGON);
        break;
      }
    }
  }
  // Charcos de Tóxico
  for (int i = s.npools - 1; i >= 0; --i) {
    s.pools[i].t += dt;
    if (s.pools[i].t >= s.pools[i].wait + POOL_LIFE) { for (int k = i; k < s.npools - 1; ++k) s.pools[k] = s.pools[k + 1]; s.npools--; }
  }
  s.pool_cd -= dt;
  if (s.pool_cd <= 0)
    for (int i = 0; i < g.nplayers; ++i) {
      Player& p = g.players[i];
      if (!p.active || p.dead) continue;
      const float pr = POOL_R * 0.9f;
      bool in = false;
      for (int k = 0; k < s.npools; ++k) {
        const auto& q = s.pools[k];
        const float ax = (p.x - q.x) / pr, ay = (p.y - q.y) / (pr * 0.75f);
        in = in || (q.t >= q.wait && ax * ax + ay * ay < 1);
      }
      if (!in) continue;
      s.pool_cd = POOL_TICK;
      hurt_pct(g, p, POOL_HIT, T_POISON);
      break;
    }
  // Remolino Infinito: te arrastra hacia él
  if (s.pull_t > 0) {
    if (s.pull_delay > 0) s.pull_delay -= dt;
    else {
      s.pull_t -= dt;
      for (int i = 0; i < g.nplayers; ++i) {
        Player& p = g.players[i];
        if (!p.active || p.dead) continue;
        const float d = dist(p.x, p.y, e->x, e->y) - e->r - p.r;
        if (d <= 0) continue;
        float nx, ny; norm(e->x - p.x, e->y - p.y, nx, ny);
        const float k = std::fmin(d, player_spd(g, p) * PULL_K * dt);
        p.x += nx * k; p.y += ny * k;
      }
      if (g.vrng.next() < dt * 60) {
        const float a = g.vrng.next() * kTau, sp = g.vrng.range(260, 480);
        PxOpts o; o.vx = -std::cos(a) * sp * 1.1f; o.vy = -std::sin(a) * sp * 0.8f; o.life = 0.8f; o.drag = 1; o.size = 2;
        g.fx.px(e->x + std::cos(a) * 300, e->y + std::sin(a) * 220, PINK, o);
      }
    }
  }
}
}  // namespace

bool eterna_on(const Game& g) { return g.eterna.st != 0; }
bool eterna_intro(const Game& g) { return g.eterna.st == 1; }
int eterna_barrier(const Game& g) { return barrier(g); }
float eterna_cloud_r(const Game& g) { return cloud_r(g); }
void eterna_reset(Game& g) { g.eterna = Game::EternaState(); }
const char* eterna_cast_name(const Game& g) { return g.eterna.cast >= 0 ? kCastName[g.eterna.cast] : nullptr; }

void eterna_update(Game& g, float dt) {
  auto& s = E(g);
  if (s.b_t >= 0) s.b_t += dt;
  if (!s.st) {
    // (no si ya casi toca la Sentencia: una partida adelantada para pruebas o para grabar)
    if (!s.happened && game_teq(g) >= kEternaAt && game_teq(g) < kJudgmentAt - 30 && !g.rift.arena && !judgment_active(g)) begin(g);
    return;
  }
  s.t += dt;
  if (s.st == 1) { if (s.t >= INTRO) { s.st = 2; s.t = 0; banner(g, "FASE 1", "DESPERTAR", "¡Derrota a Eternatus Eternamax!"); } return; }
  if (s.st == 3) { if (s.t >= 4.4f) finish(g); return; }
  fight(g, dt);
}

// La IA de Eternatus y de los núcleos (enemy_update la llama para ellos).
void eterna_enemy_update(Game& g, Enemy& e, float dt) {
  auto& s = E(g);
  e.anim.update(dt);
  if (e.flash > 0) e.flash -= dt;
  if (e.vuln_t > 0) e.vuln_t -= dt;
  if (e.dead) {
    if (e.ecore && !e.dropped) { e.dropped = true; pickup_drop(g, PK_HEAL, e.x, e.y, 0); }
    e.fade -= dt;
    return;
  }
  e.bob += dt * (e.ecore ? 3 : 6);
  e.kx = e.ky = 0;
  if (e.ecore) {
    if (e.slot >= 0 && s.phase == 2) core_pos(g, e.slot, e.x, e.y);
    else if (e.ult) { e.x = e.home_x; e.y = e.home_y; }
    return;
  }
  if (s.st != 2) { e.anim.loop(A_IDLE); return; }
  if (g.vrng.next() < dt * 22) {
    SpOpts o; o.vy = -60; o.life = 0.8f;
    PxOpts po; po.vy = -60; po.life = 0.8f;
    g.fx.px(e.x + g.vrng.range(-e.r, e.r), e.y - g.vrng.range(0, e.body_h), RED, po);
  }
  if (s.ult == 1) {   // cargando el Rayo Eternamax: al centro
    e.x += (s.cx - e.x) * std::fmin(1.0f, 2.5f * dt);
    e.y += (s.cy - TS * 1.5f - e.y) * std::fmin(1.0f, 2.5f * dt);
    e.anim.dir = 0;
    e.anim.loop(A_CHARGE, 0.6f);
    return;
  }
  if (s.stun_t > 0) { e.anim.loop(A_IDLE, 0.35f); return; }
  Player* tg = nearest(g, e.x, e.y);
  if (s.cast >= 0) { tick_cast(g, e, dt, tg); return; }
  if (!tg) { e.anim.loop(A_IDLE); return; }
  // Se coloca encima (o a un lado) de su objetivo, sin salirse de la arena.
  static const float OFF[6][2] = {{0, -5}, {5.5f, -2.5f}, {-5.5f, -2.5f}, {6.5f, 0}, {-6.5f, 0}, {0, 5}};
  const float lim = TS * 12.5f, wob = std::sin(s.ft * 0.45f) * 2.5f * TS;
  auto ok = [&](int k) { return dist(tg->x + (k == 0 || k == 5 ? wob : OFF[k][0] * TS), tg->y + OFF[k][1] * TS, s.cx, s.cy) <= lim; };
  if (s.side < 0 || !ok(s.side) || (s.side > 0 && ok(0))) { s.side = 0; while (s.side < 5 && !ok(s.side)) s.side++; }
  const float gx = tg->x + (s.side == 0 || s.side == 5 ? wob : OFF[s.side][0] * TS), gy = tg->y + OFF[s.side][1] * TS;
  const float dx = gx - e.x, dy = gy - e.y, d = std::hypot(dx, dy);
  const float sp = (70.0f + s.phase * 20) * (s.enraged ? 1.5f : 1);
  if (d > 3) { const float k = std::fmin(d, sp * dt) / d; e.x += dx * k; e.y += dy * k; }
  e.anim.dir = int8_t(dir_from_angle(std::atan2(tg->y - e.y, tg->x - e.x)));
  e.anim.loop(A_IDLE, 1 + s.phase * 0.15f);
  s.cd -= dt * (s.enraged ? 1.7f : 1);
  if (s.cd <= 0) cast(g, e, pick_cast(g, dist(e.x, e.y, tg->x, tg->y)), *tg);
}

}  // namespace ps

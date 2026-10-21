// enemy.cpp — Pokémon salvajes y su gestor (enemy.js de la web).
// (x, y) son los PIES. Los que flotan y los jefes ignoran el terreno; el resto
// choca con paredes y objetos, va más lento en el agua y se quema en la lava.
// La rejilla espacial se reconstruye cada paso (separación y colisiones).
#include <cmath>
#include <initializer_list>
#include <cstdio>

#include "core/game.h"
#include "core/mega.h"
#include "core/stats.h"

namespace ps {

const char* const kBehaviorId[BH_COUNT] = {
  "chase", "charger", "ranged", "tank", "healer", "shielder", "summoner", "bomber",
  "beamer", "zoner", "fan", "jumper", "runner", "thief"
};

namespace {
constexpr float CELL = 52;

// Los jefes se agrandan hasta ~110 de alto, según lo grande que ya sea su
// sprite (en múltiplos de 0,5: cada píxel de arte, un número entero de píxeles).
float boss_scale(int dex) {
  const AnimMeta* m = anim_meta(dex, A_IDLE);
  const float h = (m ? m->ay : 30) * kPX;
  return clamp(float(jsround(110 / h * 2) / 2), 1.0f, 2.0f);
}

bool keeps_distance(int b) {
  return b == BH_RANGED || b == BH_FAN || b == BH_BEAMER || b == BH_ZONER || b == BH_HEALER || b == BH_SUMMONER;
}

float shot_dmg(const Enemy& e) { return (e.def.shot_dmg > 0 ? e.def.shot_dmg : 10) * (e.dmg / e.def.dmg); }

uint32_t type_col(int t) { return t >= 0 ? kTypeColor[t] : 0xff7a9e; }
}  // namespace

/*
 * Curas y escudos sobre un jefe (18-10-2026): van con el daño que le estás
 * haciendo, no con su vida. rd es el daño recibido con un olvido de RD_TAU s,
 * así que rd / RD_TAU ≈ daño por segundo de ahora.
 */
constexpr float RD_TAU = 6, BOSS_HEAL_S = 0.4f, BOSS_SHIELD_S = 1.2f;
static float boss_dps(const Enemy& e) { return e.rd / RD_TAU; }

/*
 * Veneno (18-10-2026): con muchísima vida (los legendarios de la Sentencia) el
 * % se aplica a una vida "efectiva" que crece con la raíz: hasta 60.000 PV
 * igual que antes; con 240.000, como si tuviera 120.000.
 */
constexpr float PCT_REF = 60000;
static float pct_hp(const Enemy& e) { return e.max_hp <= PCT_REF ? e.max_hp : float(PCT_REF * std::sqrt(double(e.max_hp) / PCT_REF)); }

Enemy* enemy_spawn(Game& g, const EnemyDef& def, float x, float y, Scale3 sc, bool boss) {
  Enemy* e = g.enemies.push();
  if (!e) return nullptr;
  *e = Enemy();
  e->id = g.next_enemy_id++;
  e->def = def;
  e->dex = def.dex;
  e->x = x; e->y = y;
  e->boss = boss;
  e->scale = boss ? boss_scale(def.dex) : 1;
  e->anim.init(def.dex, e->scale);
  e->r = body_radius(def.dex, e->scale);
  e->body_h = body_height(def.dex, e->scale);
  e->max_hp = float(jsround(def.hp * sc.hp));
  e->hp = e->max_hp;
  e->spd = def.spd * sc.spd;
  e->dmg = def.dmg * sc.dmg;
  e->xp = def.xp;
  e->behavior = def.behavior;
  const Mon* mon = mon_by_dex(def.dex);
  if (mon && mon->types[0] >= 0) { e->types[0] = mon->types[0]; e->types[1] = mon->types[1]; }
  e->atk_type = e->types[0];
  e->mon = mon;                        // sus Defensas (stats.h)
  e->cat = int8_t(stats_side_of(mon)); // pega con su lado fuerte
  // Vuelan por encima de rocas y agua sólo los que FLOTAN en su sprite. Los jefes, también.
  e->flying = boss || hovers(def.dex);
  e->shot_cd = def.shot_cd > 0 ? g.rng.range(0.3f, def.shot_cd) : 0;
  e->bob = g.rng.next() * 6.28f;
  e->sc = sc;
  e->skill_cd = g.rng.range(1, 4);
  return e;
}

// Lo convierte en shiny: otro sprite, más aguante y mucha más XP.
void enemy_make_shiny(Game& g, Enemy& e) {
  (void)g;
  e.shiny = true;
  e.anim.init(e.dex, e.scale, true);
  e.max_hp *= 2; e.hp = e.max_hp;
  e.xp *= 5;
}

// Hora dorada: doble de experiencia y a veces monedas.
void enemy_make_golden(Enemy& e) {
  if (e.golden || e.boss || e.behavior == BH_THIEF) return;
  e.golden = true;
  e.xp *= 2;
}

static void show_hurt(Game& g, Enemy& e, int d, uint32_t color, float dir_x, float dir_y, int eff) {
  e.flash = 0.09f;
  // Muy eficaz: número amarillo y grande. Poco eficaz: gris. Crítico: naranja y grande.
  const uint32_t c = eff == EFF_CRIT ? 0xff9a3c : eff == EFF_SUPER ? 0xffe14d : eff == EFF_WEAK ? 0xa3abb8 : color;
  g.fx.dmg_num(e.x, e.y - e.body_h - 4, d, c, e.boss || eff == EFF_SUPER || eff == EFF_CRIT);
  g.fx.spark(e.x, e.y - e.body_h * 0.5f, color, dir_x, dir_y, 3);
  if (!e.anim.busy() && e.phase != PH_DASH) e.anim.play(A_HURT);
  g.sfx(SFX_HIT);
}

void enemy_hurt(Game& g, Enemy& e, float amount, uint32_t color, float dir_x, float dir_y, float knock, int eff) {
  if (e.dead) return;
  const int d = std::max(1, int(jsround(amount)));
  if (e.boss) e.rd += float(d);
  // Escudo: absorbe primero.
  if (e.shield > 0) {
    const float a = std::fmin(e.shield, float(d));
    e.shield -= a;
    if (a >= d) { g.fx.dmg_num(e.x, e.y - e.body_h - 4, d, 0x9fd8ff); e.flash = 0.05f; return; }
    amount = d - a;
  }
  e.hp -= std::max(1.0f, float(jsround(amount)));
  judgment_note_dmg(g, amount, e);
  show_hurt(g, e, d, color, dir_x, dir_y, eff);
  if (knock > 0 && !e.boss) {
    float nx, ny;
    norm(dir_x, dir_y, nx, ny);
    e.kx += nx * knock;
    e.ky += ny * knock;
  }
  if (e.hp <= 0) enemy_die(g, e);
}

// Aturdido: se queda quieto y no ataca `dur` s (Onda Trueno, Tumba Rocas...).
void enemy_apply_stun(Enemy& e, float dur) { e.stun_t = std::fmax(e.stun_t, dur); }

void enemy_apply_slow(Enemy& e, float factor, float dur) {
  e.slow = std::fmax(e.slow, factor);
  e.slow_t = std::fmax(e.slow_t, dur);
}
void enemy_apply_burn(Enemy& e, float stacks) { e.burn = std::fmin(8.0f, e.burn + stacks); e.burn_t = 3; }
void enemy_apply_poison(Enemy& e, float stacks) { e.poison = std::fmin(8.0f, e.poison + stacks); e.poison_t = 4; }

void enemy_die(Game& g, Enemy& e) {
  if (e.dead) return;
  e.dead = true;
  e.fade = 0.9f;
  e.anim.play(A_FAINT, 1, true);
  const uint32_t col = e.boss ? 0xffd95e : 0xffffff;
  const float cy = e.y - e.body_h * 0.5f;
  g.fx.burst(e.x, cy, col, e.boss ? 34 : 9, e.boss ? 260 : 120);
  g.fx.ring(e.x, e.y, e.r, e.r * (e.boss ? 6 : 2.6f), col, e.boss ? 0.6f : 0.3f, e.boss ? 6 : 3);
  // (Los legendarios de la Sentencia de Arceus sólo dan experiencia.)
  if (e.boss && !e.judge) {
    g.camera.kick(0.9f);
    for (int i = 0; i < 10; ++i) pickup_drop(g, PK_COIN, e.x + g.rng.range(-40, 40), e.y + g.rng.range(-30, 30), 5);
    // Los jefes siempre sueltan un objeto equipable.
    pickup_drop(g, PK_ITEM, e.x - 26, e.y - 10, float(roll_item(g)));
  }
  pickup_drop_xp(g, e.x, e.y, e.xp, e.boss, e.legend, e.judge);
  if (e.golden && g.rng.next() < 0.35f) pickup_drop(g, PK_COIN, e.x + 6, e.y - 4, 3);
  // Duende del tesoro: Meowth suelta monedas; Gholdengo, un ticket ×10.
  if (e.behavior == BH_THIEF) {
    const int n = e.def.loot_t10 ? 6 : 20;
    for (int i = 0; i < n; ++i) pickup_drop(g, PK_COIN, e.x + g.rng.range(-36, 36), e.y + g.rng.range(-26, 26), 5);
    if (e.def.loot_t10) pickup_drop(g, PK_TICKET10, e.x, e.y - 12, 1);
    g.fx.ring(e.x, e.y, e.r, e.r * 5, 0xffd23f, 0.6f, 4);
  }
  if (e.legend) {
    // El legendario de la grieta: tickets sueltos y monedas.
    for (int i = 0; i < 3; ++i) pickup_drop(g, PK_TICKET, e.x + g.rng.range(-30, 30), e.y + g.rng.range(-20, 20), 1);
    for (int i = 0; i < 10; ++i) pickup_drop(g, PK_COIN, e.x + g.rng.range(-50, 50), e.y + g.rng.range(-35, 35), 8);
  }
  g.sfx(e.boss ? SFX_BOSSDOWN : SFX_FAINT);
  if (e.shiny) {
    g.fx.burst(e.x, cy, 0xffe9a0, 30, 220);
    g.fx.ring(e.x, e.y, e.r, e.r * 7, 0x9ae6ff, 0.7f, 5);
    g.on_shiny_caught(e.dex);
  }
}

// ---------------- jefes ----------------
// Cada jefe (y cada legendario de las grietas) tiene 4 ataques: 2 de cerca
// (Pisotón siempre + Embestida o Salto) y 2 de lejos según su tipo. Alterna
// entre ir a por ti y pelear a distancia. Todo avisa en el suelo. Por debajo
// de la mitad de vida se enfurece.

static bool has_type(const Enemy& e, int t) { return e.types[0] == t || e.types[1] == t; }

static void boss_init(Game& g, Enemy& e) {
  BossAI& ai = e.ai;
  ai.init = true;
  ai.melee[0] = BA_SLAM;
  ai.melee[1] = hash(e.dex, 3, 7) < 0.5 ? BA_DASH : BA_LEAP;
  int pref;
  if (has_type(e, T_FIRE) || has_type(e, T_GROUND) || has_type(e, T_POISON) || has_type(e, T_ROCK) || has_type(e, T_GRASS)) pref = BA_RAIN;
  else if (has_type(e, T_ELECTRIC) || has_type(e, T_PSYCHIC) || has_type(e, T_DRAGON) || has_type(e, T_ICE) || has_type(e, T_STEEL)) pref = BA_BEAM;
  else if (has_type(e, T_FAIRY) || has_type(e, T_GHOST) || has_type(e, T_WATER) || has_type(e, T_FLYING)) pref = BA_NOVA;
  else pref = BA_VOLLEY;
  int others[3], no = 0;
  for (int x : {BA_RAIN, BA_BEAM, BA_NOVA, BA_VOLLEY}) if (x != pref) others[no++] = x;
  ai.ranged[0] = int8_t(pref);
  ai.ranged[1] = int8_t(others[int(std::floor(hash(e.dex, 5, 9) * 3)) % 3]);
  const bool far = e.behavior == BH_RANGED;
  ai.far_like = far ? 0.65f : 0.35f;
  ai.far = far;
  ai.mode_t = g.rng.range(6, 8);
  ai.cd = 1.6f;
  ai.cast = BA_NONE;
  ai.fury = false;
  ai.side = g.rng.next() < 0.5f ? -1 : 1;
}

static void enrage(Game& g, Enemy& e) {
  e.ai.fury = true;
  e.spd *= 1.15f;
  if (!e.judge) {
    char b[96];
    std::snprintf(b, sizeof(b), "¡%s se ha enfurecido!", mon_by_dex(e.dex)->name);
    g.say(b, 2.4f);
  }
  g.fx.ring(e.x, e.y, e.r, e.r * 5, 0xff4a4a, 0.6f, 5);
  g.camera.kick(0.5f);
  g.sfx(SFX_BOSS);
}

// Zona o rayo con aviso (hazard), con la categoría de sus golpes (tu defensa que la para).
static Hazard* zone(Game& g, const Enemy& e, float x, float y, float r, float delay, float dmg, int type, uint32_t color) {
  Hazard* h = hazard_add(g);
  if (!h) return nullptr;
  h->beam = false; h->x = x; h->y = y; h->r = r; h->delay = delay; h->dmg = dmg; h->type = int8_t(type); h->color = color;
  h->cat = e.cat;
  return h;
}
static Hazard* hbeam(Game& g, const Enemy& e, float x, float y, float angle, float len, float w, float delay, float dmg, int type, uint32_t color) {
  Hazard* h = hazard_add(g);
  if (!h) return nullptr;
  h->beam = true; h->x = x; h->y = y; h->angle = angle; h->len = len; h->w = w; h->delay = delay; h->dmg = dmg;
  h->type = int8_t(type); h->color = color;
  h->cat = e.cat;
  return h;
}

// Disparo normal de enemigo (uno, o varios en abanico).
static void shoot(Game& g, Enemy& e, float ax, float ay, float spread = 0, int n = 1, float k = 1) {
  const float sp = 190;
  const int type = e.atk_type;
  const float base = std::atan2(ay, ax);
  for (int i = 0; i < n; ++i) {
    const float a = base + (n == 1 ? 0 : (float(i) / (n - 1) - 0.5f) * spread);
    Projectile* s = proj_spawn(g);
    if (!s) break;
    s->x = e.x; s->y = e.y; s->vx = std::cos(a) * sp; s->vy = std::sin(a) * sp;
    s->dmg = shot_dmg(e) * k; s->r = 7; s->life = 2.4f; s->friendly = false; s->color = 0xff7a9e;
    s->mtype = int8_t(type);
    s->mcat = e.cat;
    s->has_vis = true; s->vis_proj = kTypeShape[type >= 0 ? type : T_NORMAL]; s->vis_pal = int8_t(type >= 0 ? type : T_NORMAL);
  }
  e.anim.play(A_SHOOT);
  g.fx.ring(e.x, e.y - kLift, 4, 14, 0xff7a9e, 0.2f, 2);
}

static void boss_start(Game& g, Enemy& e, int id, const Player& pl) {
  BossAI& ai = e.ai;
  const float f = ai.fury ? 0.8f : 1;
  const int type = e.atk_type;
  const uint32_t col = type_col(type);
  const float ang = std::atan2(pl.y - e.y, pl.x - e.x);
  ai.cast = int8_t(id); ai.ct = 0; ai.wind = 0.6f * f; ai.after = 0.35f; ai.ang = ang; ai.go = false; ai.fired = false;
  switch (id) {
    case BA_SLAM:     // pisotón: onda alrededor
      ai.wind = 0.85f * f;
      zone(g, e, e.x, e.y, 96 + e.r, ai.wind, e.dmg * 1.3f, type, col);
      e.anim.play(A_CHARGE, 1.2f);
      break;
    case BA_DASH:     // embestida en línea recta
      ai.wind = 0.75f * f; ai.len = 330; ai.dur = 0.32f;
      hbeam(g, e, e.x, e.y, ang, ai.len + e.r, e.r * 2 + 16, ai.wind + ai.dur, e.dmg * 1.2f, type, col);
      e.anim.play(A_CHARGE, 1.3f);
      break;
    case BA_LEAP:     // salto y caída sobre ti
      e.jump = {true, e.x, e.y, pl.x, pl.y, -0.5f * f, 0.65f};
      zone(g, e, pl.x, pl.y, 82, 0.5f * f + 0.65f, e.dmg * 1.25f, type, col);
      e.anim.play(A_CHARGE, 1.4f);
      ai.cast = BA_NONE; ai.cd = g.rng.range(1.4f, 2) + 0.6f;
      break;
    case BA_RAIN: {   // zonas que caen alrededor de ti
      const int n = ai.fury ? 6 : 4;
      for (int i = 0; i < n; ++i) {
        const float a = i ? g.rng.next() * kTau : 0, d = i ? g.rng.range(40, 120) : 0;
        zone(g, e, pl.x + std::cos(a) * d, pl.y + std::sin(a) * d * 0.8f, 50, 1.05f + i * 0.12f, shot_dmg(e) * 1.15f, type, col);
      }
      ai.wind = 0.5f; e.anim.play(A_SHOOT);
      break;
    }
    case BA_BEAM: {   // rayos en abanico
      const int n = ai.fury ? 5 : 3;
      const float sp = ai.fury ? 0.42f : 0.5f;
      for (int i = 0; i < n; ++i)
        hbeam(g, e, e.x, e.y, ang + (i - (n - 1) / 2.0f) * sp, 400, 26, 1.0f, shot_dmg(e) * 1.2f, type, col);
      ai.wind = 1.0f; ai.after = 0.25f; e.anim.play(A_CHARGE, 1.1f);
      break;
    }
    case BA_VOLLEY:   // abanico de disparos
    case BA_NOVA:     // anillo de disparos
      ai.wind = (id == BA_NOVA ? 0.7f : 0.5f) * f;
      e.anim.play(A_CHARGE, 1.4f);
      g.fx.ring(e.x, e.y, e.r, e.r * 2.4f, col, ai.wind, 2);
      break;
    default: break;
  }
}

struct Vel { float vx, vy; int anim; };

static Vel boss_end(Game& g, Enemy& e) {
  BossAI& ai = e.ai;
  ai.cast = BA_NONE;
  ai.cd = g.rng.range(1.3f, 2.0f);
  if (ai.fury && g.rng.next() < 0.35f) ai.cd = 0.3f;    // encadena otro
  return {0, 0, A_IDLE};
}

static Vel boss_cast(Game& g, Enemy& e, float dt, const Player& pl) {
  BossAI& ai = e.ai;
  ai.ct += dt;
  if (ai.cast == BA_DASH && ai.ct >= ai.wind) {
    if (!ai.go) { ai.go = true; e.anim.play(A_ATTACK, 1.6f); }
    if (ai.ct >= ai.wind + ai.dur) return boss_end(g, e);
    const float sp = ai.len / ai.dur;
    return {std::cos(ai.ang) * sp, std::sin(ai.ang) * sp, A_WALK};
  }
  if (ai.ct >= ai.wind && !ai.fired) {
    ai.fired = true;
    float ax, ay;
    norm(pl.x - e.x, pl.y - e.y, ax, ay);
    if (ai.cast == BA_VOLLEY) shoot(g, e, ax, ay, 1.1f, ai.fury ? 9 : 7, 0.8f);
    else if (ai.cast == BA_NOVA) { const int n = ai.fury ? 18 : 14; shoot(g, e, ax, ay, kTau * (n - 1) / n, n, 0.7f); }
    else if (ai.cast == BA_SLAM) e.anim.play(A_ATTACK, 1.4f);
  }
  if (ai.ct >= ai.wind + ai.after) return boss_end(g, e);
  return {0, 0, A_IDLE};
}

static Vel boss_move(Game& g, Enemy& e, float dt, const Player& pl, float d, float dx, float dy, float spd) {
  if (!e.ai.init) boss_init(g, e);
  BossAI& ai = e.ai;
  if (!ai.fury && e.hp < e.max_hp * 0.5f) enrage(g, e);
  const float k = ai.fury ? 1.25f : 1;
  if (ai.cast != BA_NONE) return boss_cast(g, e, dt, pl);
  ai.mode_t -= dt; ai.cd -= dt * k;
  // Alterna siempre: en su distancia favorita está más rato (7-9 s) que en la otra (4-5 s).
  if (ai.mode_t <= 0) {
    ai.far = !ai.far;
    ai.mode_t = (ai.far == (ai.far_like > 0.5f)) ? g.rng.range(7, 9) : g.rng.range(4, 5);
    ai.side = -ai.side;
  }
  float vx = dx * spd * k * 1.1f, vy = dy * spd * k * 1.1f;
  float ddx, ddy;
  norm(pl.x - e.x, pl.y - e.y, ddx, ddy);
  if (ai.far) {
    if (d < 170) { vx = -ddx * spd; vy = -ddy * spd; }
    else if (d < 300) { vx = -ddy * spd * 0.55f * ai.side; vy = ddx * spd * 0.55f * ai.side; }   // te rodea
  }
  if (ai.cd <= 0) {
    int id = BA_NONE;
    if (!ai.far) {
      int ok[2], nok = 0;
      for (int i = 0; i < 2; ++i) {
        const int m = ai.melee[i];
        if (m == BA_SLAM ? d < 125 : d < 320) ok[nok++] = m;
      }
      // De vez en cuando, aunque esté cerca, también dispara.
      if (nok && g.rng.next() < 0.8f) id = ok[int(g.rng.next() * nok)];
      else if (d < 480) id = ai.ranged[int(g.rng.next() * 2)];
    } else {
      id = d < 115 ? int(BA_SLAM) : d < 430 ? int(ai.ranged[int(g.rng.next() * 2)]) : int(BA_NONE);
    }
    if (id != BA_NONE) boss_start(g, e, id, pl);
  }
  return {vx, vy, A_WALK};
}

// ---------------- personalidades ----------------

static void heal_around(Game& g, Enemy& e) {
  // 7 % de la vida, y más cuanto más fuerte pegas, pero cada vez menos (tope ×2,5).
  Player* ps[kMaxPlayers];
  const int np = g.everyone(ps);
  float pow_ = 1;
  for (int i = 0; i < np; ++i) {
    const float a = player_atk(*ps[i]);
    pow_ = std::fmax(pow_, (a > 0 && ps[i]->base_atk > 0) ? a / ps[i]->base_atk : 1);
  }
  const float k = 0.07f * std::fmin(2.5f, std::pow(pow_, 0.35f));
  int n = 0;
  Enemy* near[kMaxEnemies];
  const int nn = g.query_circle(e.x, e.y, 150, near, kMaxEnemies);
  for (int i = 0; i < nn; ++i) {
    Enemy* o = near[i];
    if (o->dead || o->hp >= o->max_hp) continue;
    // A un jefe, como mucho lo que le quitas en 0,4 s: así nunca se vuelve inmatable.
    const float heal = o->max_hp * k;
    o->hp = std::fmin(o->max_hp, o->hp + (o->boss ? std::fmin(heal, boss_dps(*o) * BOSS_HEAL_S) : heal));
    g.fx.burst(o->x, o->y - o->body_h * 0.6f, 0x7dffb0, 5, 60);
    n++;
  }
  e.anim.play(A_CHARGE);
  g.fx.ring(e.x, e.y, 8, 150, 0x5fe08a, 0.5f, 3);
  if (n && g.camera.sees(e.x, e.y, 0)) g.sfx(SFX_HEAL);
}

static void cast_shields(Game& g, Enemy& e) {
  Enemy* near[kMaxEnemies];
  int nn = g.query_circle(e.x, e.y, 160, near, kMaxEnemies);
  // Los 6 más cercanos (orden estable por distancia).
  for (int a = 1; a < nn; ++a) {
    Enemy* v = near[a];
    const float dv = dist2(v->x, v->y, e.x, e.y);
    int b = a - 1;
    while (b >= 0 && dist2(near[b]->x, near[b]->y, e.x, e.y) > dv) { near[b + 1] = near[b]; --b; }
    near[b + 1] = v;
  }
  if (nn > 6) nn = 6;
  // A un jefe, el escudo es lo que le quitas en 1,2 s (no un 35 % de su vida
  // enorme), y uno cada 6 s aunque haya varios escuderos (shield_t empieza en 8).
  for (int i = 0; i < nn; ++i) {
    Enemy& o = *near[i];
    if (o.dead) continue;
    if (o.boss && o.shield_t > 2) continue;
    const float sh = o.boss ? std::fmin(o.max_hp * 0.35f, boss_dps(o) * BOSS_SHIELD_S) : o.max_hp * 0.35f;
    o.shield = std::fmax(o.shield, sh);
    o.shield_t = 8;
  }
  e.anim.play(A_CHARGE);
  g.fx.ring(e.x, e.y, 8, 160, 0x7fc8ff, 0.5f, 3);
}

static void summon(Game& g, Enemy& e) {
  int live = 0;
  for (int i = 0; i < e.nminions; ++i) {
    Enemy* m = g.enemy_by_id(e.minions[i]);
    if (m && !m->dead) e.minions[live++] = e.minions[i];
  }
  e.nminions = live;
  // Con el tope general de enemigos, con un pequeño margen.
  if (e.nminions >= 4 || g.enemies.n > kEnemyCap + 15) return;
  EnemyDef def;
  def.dex = e.def.minion ? e.def.minion : e.dex;
  def.hp = float(jsround(e.def.hp * 0.35f));
  def.spd = float(jsround(e.def.spd * 1.1f));
  def.dmg = float(jsround(e.def.dmg * 0.6f));
  def.xp = std::fmax(1.0f, float(jsround(e.def.xp * 0.25f)));
  def.behavior = BH_CHASE;
  const float ex = e.x, ey = e.y;
  const Scale3 sc = e.sc;
  const int32_t eid = e.id;
  for (int i = 0; i < 2; ++i) {
    const float a = g.rng.next() * kTau;
    Enemy* m = enemy_spawn(g, def, ex + std::cos(a) * 30, ey + std::sin(a) * 22, sc);
    if (!m) break;
    Enemy* self = g.enemy_by_id(eid);   // (el array no se mueve al añadir, pero por si acaso)
    if (self && self->nminions < 8) self->minions[self->nminions++] = m->id;
    g.fx.burst(m->x, m->y - 8, 0xc47bff, 8, 90);
  }
  Enemy* self = g.enemy_by_id(eid);
  if (!self) return;
  self->anim.play(A_CHARGE);
  g.fx.ring(self->x, self->y, 8, 50, 0xc47bff, 0.4f, 3);
}

static void special(Game& g, Enemy& e, const Player& pl, float dx, float dy) {
  const uint32_t col = type_col(e.atk_type);
  switch (e.behavior) {
    case BH_FAN: { const int n = e.def.fan_n ? e.def.fan_n : 5; shoot(g, e, dx, dy, n == 3 ? 0.6f : 0.9f, n, 0.65f); break; }
    case BH_BEAMER:
      // Rayo con aviso: primero se ve la franja en el suelo.
      hbeam(g, e, e.x, e.y, std::atan2(dy, dx), 360, 22, 1.0f, shot_dmg(e) * 1.05f, e.atk_type, col);
      e.anim.play(A_CHARGE, 1.2f);
      break;
    case BH_ZONER: {
      const int n = g.rng.range_int(1, e.def.zones ? e.def.zones : 3);
      for (int i = 0; i < n; ++i) {
        const float ox = i ? g.rng.range(-70, 70) : 0;
        const float oy = i ? g.rng.range(-55, 55) : 0;
        zone(g, e, pl.x + ox, pl.y + oy, 44, 1.2f, shot_dmg(e) * 0.95f, e.atk_type, col);
      }
      e.anim.play(A_SHOOT);
      break;
    }
    case BH_HEALER: heal_around(g, e); break;
    case BH_SUMMONER: summon(g, e); break;
    default: shoot(g, e, dx, dy); break;
  }
}

static void start_fuse(Game& g, Enemy& e) {
  e.phase = PH_FUSE; e.pt = 1.0f;
  zone(g, e, e.x, e.y, 62, 1.0f, e.dmg * 1.4f, e.atk_type, 0xff7b3d);
}

static void start_jump(Game& g, Enemy& e, const Player& pl) {
  const float tx = pl.x, ty = pl.y;
  e.jump = {true, e.x, e.y, tx, ty, -0.45f, 0.6f};
  e.anim.play(A_CHARGE, 1.4f);
  zone(g, e, tx, ty, 50, 1.05f, e.dmg * 1.2f, e.atk_type, type_col(e.atk_type));
}

// Salto: se agacha, vuela en arco y cae donde estabas.
static void update_jump(Game& g, Enemy& e, float dt) {
  Jump& j = e.jump;
  j.t += dt;
  if (j.t < 0) { e.anim.loop(A_IDLE); return; }
  const float k = std::fmin(1.0f, j.t / j.dur);
  e.x = j.sx + (j.tx - j.sx) * k;
  e.y = j.sy + (j.ty - j.sy) * k;
  e.jump_lift = std::sin(k * kPi) * 46;
  e.anim.dir = int8_t(dir_from_angle(std::atan2(j.ty - j.sy, j.tx - j.sx)));
  if (k >= 1) {
    j.on = false; e.jump_lift = 0;
    if (!e.flying) g.world.collide(e.x, e.y, e.r);
    e.anim.play(A_ATTACK, 1.4f);
  }
}

void enemy_strike(Enemy& e, const Player& pl) {
  if (!e.anim.busy()) {
    e.anim.dir = int8_t(dir_from_angle(std::atan2(pl.y - e.y, pl.x - e.x)));
    e.anim.play(A_ATTACK, 1.5f);
  }
}

static void enemy_update(Game& g, Enemy& e, float dt, const Player& pl) {
  // Fuera de pantalla no hace falta animar (sí moverse y pensar).
  const bool seen = g.seen_by_any(e.x, e.y, 120);
  if (seen || e.dead) e.anim.update(dt);
  if (e.flash > 0) e.flash -= dt;
  if (e.dead) { e.fade -= dt; return; }
  if (e.shiny && seen) {
    e.spark_t -= dt;
    if (e.spark_t <= 0) {
      e.spark_t = 0.22f;
      g.fx.twinkle(e.x + g.vrng.range(-e.r, e.r) * 1.2f, e.y - g.vrng.range(4, e.body_h * 1.1f));
    }
  }
  if (e.touch_cd > 0) e.touch_cd -= dt;
  if (e.rd > 0) e.rd *= float(std::exp(-double(dt) / RD_TAU));
  e.bob += dt * 6;

  // --- estados ---
  if (e.slow_t > 0) { e.slow_t -= dt; if (e.slow_t <= 0) e.slow = 0; }
  if (e.burn > 0) {
    e.burn_t -= dt; e.burn_tick -= dt;
    if (e.burn_tick <= 0) { e.burn_tick = 0.5f; enemy_hurt(g, e, 1.6f * e.burn * (1 + pl.level * 0.06f), 0xff9a4d); }
    if (e.burn_t <= 0) e.burn = 0;
  }
  if (e.poison > 0) {
    e.poison_t -= dt; e.poison_tick -= dt;
    if (e.poison_tick <= 0) { e.poison_tick = 0.6f; enemy_hurt(g, e, pct_hp(e) * 0.012f * e.poison + 1, 0xd47cf0); }
    if (e.poison_t <= 0) e.poison = 0;
  }
  if (e.dead) return;

  // --- terreno ---
  float terrain_mul = 1;
  // (El legendario está en su casa: su arena no le afecta. Los de Arceus, tampoco.)
  if (!e.flying && !e.legend && !e.judge && g.world.is_liquid(e.x, e.y)) {
    const LiquidFx le = g.world.liquid_effect(e.types);
    if (le.dmg > 0) {
      e.lava_tick -= dt;
      if (e.lava_tick <= 0) { e.lava_tick = le.every; enemy_hurt(g, e, 6 + e.max_hp * 0.04f, le.color); }
      if (e.dead) return;
    } else if (le.slow > 0) terrain_mul = le.slow < 0.5f ? 0.45f : 0.55f;
  }

  if (e.stun_t > 0) { e.stun_t -= dt; e.anim.loop(A_IDLE); return; }

  const float spd = e.spd * (1 - e.slow) * terrain_mul * weather_speed_for(g, e.types);
  float dx, dy;
  norm(pl.x - e.x, pl.y - e.y, dx, dy);
  const float d = dist(e.x, e.y, pl.x, pl.y);
  // Duende del tesoro: huye de ti (en zigzag) en cuanto te acercas.
  if (e.behavior == BH_THIEF) {
    e.zig += dt;
    const bool near = d < 300;
    const float a = std::atan2(-dy, -dx) + std::sin(e.zig * 2.2f) * 0.7f;
    const float k = near ? 1 : 0.35f;
    const float vx0 = std::cos(a) * spd * k, vy0 = std::sin(a) * spd * k;
    const float ox = e.x, oy = e.y;
    e.x += (vx0 + e.kx) * dt; e.y += (vy0 + e.ky) * dt;
    e.kx *= std::pow(0.0015f, dt); e.ky *= std::pow(0.0015f, dt);
    if (g.world.collide(e.x, e.y, e.r) && std::hypot(e.x - ox, e.y - oy) < spd * k * dt * 0.3f) e.zig += 1.4f;
    if (g.vrng.next() < dt * 6 && g.camera.sees(e.x, e.y, 0)) g.fx.twinkle(e.x + g.vrng.range(-e.r, e.r), e.y - g.vrng.range(4, e.body_h));
    e.anim.dir = int8_t(dir_from_angle(a));
    e.anim.loop(A_WALK, 1.4f);
    return;
  }
  // Corredor: va a donde VAS a estar (te corta el paso si sólo huyes).
  if (e.behavior == BH_RUNNER && pl.has_v) {
    const float lead = clamp(d / std::fmax(60.0f, spd), 0.0f, 1.4f);
    norm(pl.x + pl.vx_ * lead - e.x, pl.y + pl.vy_ * lead - e.y, dx, dy);
    if (g.camera.sees(e.x, e.y, 0) && g.vrng.next() < dt * 10) {
      PxOpts o; o.vy = -12; o.life = 0.35f;
      g.fx.px(e.x - dx * 8 + g.vrng.range(-4, 4), e.y, 0xe8dcc0, o);
    }
  }

  // --- personalidad (escudos, saltos, explosión) ---
  if (e.shield_t > 0 && (e.shield_t -= dt) <= 0) e.shield = 0;
  if (e.jump.on) { update_jump(g, e, dt); return; }
  if (e.behavior == BH_BOMBER && e.phase == PH_FUSE) {
    e.pt -= dt;
    e.flash = int(std::floor(e.pt * 10)) % 2 ? 0.05f : 0;
    if (e.pt <= 0) { e.hp = 0; enemy_die(g, e); }
    e.anim.loop(A_IDLE);
    return;
  }
  if (e.behavior == BH_BOMBER && d < e.r + pl.r + 26) { start_fuse(g, e); return; }
  if (e.behavior == BH_SHIELDER || e.behavior == BH_JUMPER) {
    e.skill_cd -= dt;
    if (e.skill_cd <= 0) {
      if (e.behavior == BH_SHIELDER) { e.skill_cd = 6; cast_shields(g, e); }
      else if (d < 250) { e.skill_cd = g.rng.range(5, 7); start_jump(g, e, pl); return; }
      else e.skill_cd = 0.5f;
    }
  }

  // Si lleva rato atascado contra algo, rodea por un lado.
  if (e.side_t > 0) {
    e.side_t -= dt;
    const float a = std::atan2(dy, dx) + e.side * 1.25f;
    dx = std::cos(a); dy = std::sin(a);
  }

  // Camino alrededor de los obstáculos (los que no vuelan).
  if (!e.flying && e.phase != PH_DASH && e.side_t <= 0) {
    float px, py;
    const int pi = int(&pl - g.players);
    if (path_dir_for(g, e, pi, px, py) && !(keeps_distance(e.behavior) && d < (e.def.range > 0 ? e.def.range : 200) * 0.82f)) { dx = px; dy = py; }
  }

  float vx = dx * spd, vy = dy * spd;
  int anim_base = A_WALK;

  if (e.boss) {
    const Vel v = boss_move(g, e, dt, pl, d, dx, dy, spd);
    vx = v.vx; vy = v.vy; anim_base = v.anim;
  } else if (e.behavior == BH_CHARGER) {
    e.pt -= dt;
    if (e.phase == PH_WALK) {
      if (d < 200 && e.pt <= 0) {
        e.phase = PH_AIM; e.pt = 0.55f;
        e.anim.play(A_CHARGE, 1.6f);
      }
    } else if (e.phase == PH_AIM) {
      vx = vy = 0; anim_base = A_IDLE;
      if (e.pt <= 0) {
        e.phase = PH_DASH; e.pt = 0.42f;
        e.dx = dx; e.dy = dy;
        e.anim.play(A_ATTACK, 1.4f);
        g.fx.ring(e.x, e.y, e.r, e.r * 2.4f, 0xff6b6b, 0.25f, 2);
      }
    } else if (e.phase == PH_DASH) {
      vx = e.dx * spd * 3.4f; vy = e.dy * spd * 3.4f;
      if (e.pt <= 0) { e.phase = PH_WALK; e.pt = 1.1f; }
    }
  } else if (keeps_distance(e.behavior)) {
    const float range = e.def.range > 0 ? e.def.range : 200;
    const float keep = range * 0.82f;
    // Recta hacia ti (no el camino) para alejarse.
    float ddx, ddy;
    norm(pl.x - e.x, pl.y - e.y, ddx, ddy);
    if (d < keep * 0.75f) { vx = -ddx * spd * 0.8f; vy = -ddy * spd * 0.8f; }
    else if (d < keep) { vx *= 0.1f; vy *= 0.1f; anim_base = A_IDLE; }
    e.shot_cd -= dt;
    if (e.shot_cd <= 0 && d < range) {
      e.shot_cd = e.def.shot_cd > 0 ? e.def.shot_cd : 2;
      const int32_t id = e.id;
      special(g, e, pl, ddx, ddy);
      // (summon puede añadir enemigos; e sigue siendo válido: no se mueven al añadir)
      (void)id;
    }
  }

  // --- integración ---
  const float ox = e.x, oy = e.y;
  e.x += e.kx * dt; e.y += e.ky * dt;
  e.kx *= std::pow(0.0015f, dt); e.ky *= std::pow(0.0015f, dt);
  e.x += vx * dt; e.y += vy * dt;

  if (!e.flying && g.world.collide(e.x, e.y, e.r)) {
    // ¿Ha avanzado bastante menos de lo que quería?
    const float want = std::hypot(vx, vy) * dt;
    const float got = std::hypot(e.x - ox, e.y - oy);
    if (want > 0.5f && got < want * 0.35f) {
      e.stuck_t += dt;
      if (e.stuck_t > 0.35f && e.side_t <= 0) {
        e.side_t = 0.7f; e.stuck_t = 0;
        e.side = g.rng.next() < 0.5f ? -1 : 1;
      }
    } else e.stuck_t = 0;
  }

  // --- animación ---
  const bool moving = std::fabs(vx) + std::fabs(vy) > 2;
  const float face_a = e.phase == PH_DASH ? std::atan2(e.dy, e.dx) : moving ? std::atan2(vy, vx) : std::atan2(pl.y - e.y, pl.x - e.x);
  e.anim.dir = int8_t(dir_from_angle(face_a));
  e.anim.loop(moving ? anim_base : A_IDLE, moving ? std::fmax(0.6f, spd / 70) : 1);
}

// ---------------- gestor ----------------

static inline int cell_of(float v) { return int(std::floor(v / CELL)); }
static inline int grid_slot(int cx, int cy) {
  const int m = Game::kGridSize;
  return (((cy % m) + m) % m) * m + (((cx % m) + m) % m);
}

void Game::rebuild_grid() {
  for (auto& h : grid_head) h = -1;
  for (int i = 0; i < enemies.n; ++i) {
    const Enemy& e = enemies[i];
    if (e.dead) { grid_next[i] = -1; continue; }
    const int s = grid_slot(cell_of(e.x), cell_of(e.y));
    grid_next[i] = grid_head[s];
    grid_head[s] = int16_t(i);
  }
}

int Game::query_circle(float x, float y, float r, Enemy** out, int max) {
  int n = 0;
  const int c0x = cell_of(x - r) - 1, c1x = cell_of(x + r) + 1;
  const int c0y = cell_of(y - r) - 1, c1y = cell_of(y + r) + 1;
  // La rejilla da la vuelta (64×64 celdas): si el círculo es muy grande, se
  // recorre la lista entera (mismo resultado, sin repetir).
  if (c1x - c0x >= kGridSize || c1y - c0y >= kGridSize) {
    for (int i = 0; i < enemies.n && n < max; ++i) {
      Enemy& e = enemies[i];
      if (e.dead) continue;
      const float rr = r + e.r;
      if (dist2(x, y, e.x, e.y) <= rr * rr) out[n++] = &e;
    }
    return n;
  }
  for (int cy = c0y; cy <= c1y; ++cy)
    for (int cx = c0x; cx <= c1x; ++cx)
      for (int i = grid_head[grid_slot(cx, cy)]; i >= 0; i = grid_next[i]) {
        Enemy& e = enemies[i];
        if (e.dead || cell_of(e.x) != cx || cell_of(e.y) != cy) continue;   // (otra celda con el mismo hueco)
        const float rr = r + e.r;
        if (dist2(x, y, e.x, e.y) <= rr * rr && n < max) out[n++] = &e;
      }
  return n;
}

static void separate(Game& g, float dt) {
  for (int idx = 0; idx < g.enemies.n; ++idx) {
    Enemy& e = g.enemies[idx];
    if (e.dead) continue;
    const int cx = cell_of(e.x), cy = cell_of(e.y);
    float px = 0, py = 0;
    int n = 0;
    for (int j = -1; j <= 1; ++j)
      for (int i = -1; i <= 1; ++i) {
        for (int k = g.grid_head[grid_slot(cx + i, cy + j)]; k >= 0; k = g.grid_next[k]) {
          Enemy& o = g.enemies[k];
          if (&o == &e || o.dead) continue;
          if (cell_of(o.x) != cx + i || cell_of(o.y) != cy + j) continue;
          const float min = (e.r + o.r) * 0.82f;
          const float ddx = e.x - o.x, ddy = e.y - o.y;
          const float d2 = ddx * ddx + ddy * ddy;
          if (d2 > min * min || d2 < 1e-4f) continue;
          const float d = std::sqrt(d2);
          const float f = (min - d) / min;
          px += (ddx / d) * f; py += (ddy / d) * f;
          if (++n > 6) break;
        }
      }
    if (n) {
      e.x += px * 42 * dt;
      e.y += py * 42 * dt;
      if (!e.flying) g.world.collide(e.x, e.y, e.r);
    }
  }
}

static Player* nearest_player(Game& g, float x, float y) {
  Player* best = nullptr;
  float bd = 1e30f;
  for (int i = 0; i < g.nplayers; ++i) {
    Player& p = g.players[i];
    if (!p.active || p.dead) continue;
    const float d = dist2(x, y, p.x, p.y);
    if (d < bd) { bd = d; best = &p; }
  }
  return best;
}

// Segundos que un rezagado tiene que llevar fuera de la vista antes de volver por delante.
static constexpr float LAG_T = 2.5f;
static constexpr float BOSS_FAR = 2500;   // jefe más lejos que esto de todos: se ha perdido

void enemies_update(Game& g, float dt) {
  Player& pl = g.pl();
  // Velocidad suavizada de cada jugador.
  for (int i = 0; i < g.nplayers; ++i) {
    Player& p = g.players[i];
    if (p.has_v && dt > 0) {
      const float k = std::fmin(1.0f, dt * 5);
      p.vx_ += ((p.x - p.px_) / dt - p.vx_) * k; p.vy_ += ((p.y - p.py_) / dt - p.vy_) * k;
      if (std::fabs(p.x - p.px_) > 200 || std::fabs(p.y - p.py_) > 200) p.vx_ = p.vy_ = 0;   // teletransporte
    } else { p.vx_ = 0; p.vy_ = 0; p.has_v = true; }
    p.px_ = p.x; p.py_ = p.y;
  }
  g.rebuild_grid();
  for (int i = 0; i < g.enemies.n; ++i) {
    Enemy& e = g.enemies[i];
    Player* near = nearest_player(g, e.x, e.y);
    enemy_update(g, e, dt, near ? *near : pl);
  }
  separate(g, dt);
  for (int i = g.enemies.n - 1; i >= 0; --i) {
    Enemy& e = g.enemies[i];
    if (e.dead) {
      pl.kills++;
      if (e.boss && !e.judge) pl.bosses++;
      progress_kill(g, e);
      if (g.corpses.full()) g.corpses.remove_ordered(0);
      *g.corpses.push() = e;
      g.enemies.remove_ordered(i);
      continue;
    }
    Player* nearp = nearest_player(g, e.x, e.y);
    Player& near = nearp ? *nearp : pl;
    // El duende del tesoro se escapa si no lo atrapas a tiempo.
    if (e.behavior == BH_THIEF && (e.life -= dt) <= 0) {
      g.fx.burst(e.x, e.y - 10, 0xffd23f, 16, 140);
      char b[96];
      std::snprintf(b, sizeof(b), "¡%s se ha escapado!", mon_by_dex(e.dex)->name);
      g.say(b, 2.4f);
      g.enemies.remove_ordered(i);
      continue;
    }
    // Jefe lejísimos de todos (salió junto a un compañero que aún volvía de la
    // grieta, o se quedó atrás): a los 4 s vuelve a salir cerca, fuera de la vista.
    if (e.boss && !e.judge && !g.rift.arena) {
      e.lag_t = dist2(e.x, e.y, near.x, near.y) > BOSS_FAR * BOSS_FAR ? e.lag_t + dt : 0;
      if (e.lag_t > 4) { e.lag_t = 0; spawner_ahead(g, e, near); }
    }
    if (e.boss || e.shiny || g.rift.arena) continue;
    // Rezagados: los que se quedan atrás mientras corres y llevan un rato FUERA
    // DE LA VISTA vuelven a salir por delante de ti.
    const float d2 = dist2(e.x, e.y, near.x, near.y);
    const float R = g.camera.outer_radius() + 40;
    const bool unseen = d2 > R * R && !g.seen_by_any(e.x, e.y, 60);
    const float sp = std::hypot(near.vx_, near.vy_);
    const bool behind = sp > 40 && ((e.x - near.x) * near.vx_ + (e.y - near.y) * near.vy_) < -0.2f * std::sqrt(d2) * sp;
    if (unseen && behind) e.lag_t += dt;
    else if (!unseen) e.lag_t = 0;
    if ((unseen && e.lag_t > LAG_T) || d2 > 1400 * 1400) {
      e.lag_t = 0;
      if (!spawner_ahead(g, e, near)) g.enemies.remove_ordered(i);
    }
  }
  for (int i = g.corpses.n - 1; i >= 0; --i) {
    enemy_update(g, g.corpses[i], dt, pl);
    if (g.corpses[i].fade <= 0) g.corpses.remove_ordered(i);
  }
}

Enemy* Game::enemy_by_id(int32_t id) {
  for (auto& e : enemies) if (e.id == id) return &e;
  return nullptr;
}

bool Game::boss_alive() const {
  for (const auto& e : enemies) if (e.boss && !e.dead) return true;
  return false;
}

}  // namespace ps

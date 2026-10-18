// combat.cpp — ejecución de movimientos y colisiones (combat.js de la web).
// Sólo se ejecuta el hueco ACTIVO del jugador (con un vínculo, todos sus
// movimientos a la vez). Todos los ataques dañan también los objetos
// destructibles del escenario. Lo que se queda en el mapa un rato (minas,
// meteoros, torretas, charcos) vive en `fields`.
#include <cmath>

#include "core/game.h"

namespace ps {

namespace {
constexpr float CRIT_MUL = 1.5f;

int pl_index(Game& g, const Player& p) { return int(&p - g.players); }

const MoveVis& vis_of(const MoveInst& m) { return kMoveVis[m.def]; }

bool needs_target(int kind) {
  switch (kind) {
    case K_MELEE: case K_BEAM: case K_PROJECTILE: case K_BOOMERANG: case K_CHAIN: case K_METEOR: case K_DASH: case K_CONE: return true;
    default: return false;
  }
}

float reach(const MoveInst& m) {
  switch (m.kind()) {
    case K_MELEE: return m.get(MS_RADIUS) + 30;
    case K_BEAM: return m.get(MS_RADIUS) + 20;
    case K_BOOMERANG: return m.get(MS_RADIUS) + 40;
    case K_CHAIN: return 280;
    case K_METEOR: return 340;
    case K_DASH: return m.get(MS_RADIUS) + 60;
    case K_CONE: return m.get(MS_SPEED) * m.get(MS_LIFE) + 30;
    default: return 520;
  }
}

// Tabla de tipos × clima para un ataque de tipo `type` contra `e`.
float type_k(Game& g, int type, const Enemy& e) { return type_mult(type, e.types) * weather_mult(g, type); }

bool is_crit(Game& g, const Player& p) { return p.crit > 0 && g.rng.next() < p.crit; }

void on_dealt(Player& p, float dmg) {
  // Cascabel Concha: un poco de vida por el daño que haces (tope 4/s).
  if (p.item == IT_SHELLBELL) p.bell_heal = std::fmin(4.0f, p.bell_heal + dmg * 0.04f);
}

void hit(Game& g, Player& p, Enemy& e, float dmg, uint32_t col, float dx, float dy, float knock, const MoveInst& m) {
  const float k = type_k(g, m.type(), e);
  const bool crit = is_crit(g, p);
  dmg *= k * (crit ? CRIT_MUL : 1);
  enemy_hurt(g, e, dmg, col, dx, dy, knock, crit ? EFF_CRIT : type_label(type_mult(m.type(), e.types)));
  on_dealt(p, dmg);
  p.dmg_dealt += dmg;
  if (m.get(MS_BURN) > 0) enemy_apply_burn(e, m.get(MS_BURN));
  if (m.get(MS_POISON) > 0) enemy_apply_poison(e, m.get(MS_POISON));
  if (m.get(MS_SLOW) > 0) enemy_apply_slow(e, m.get(MS_SLOW));
}

void fill_payload(Projectile& pr, const MoveInst& m) {
  pr.burn = m.get(MS_BURN); pr.poison = m.get(MS_POISON); pr.slow = m.get(MS_SLOW); pr.knock = m.get(MS_KNOCK);
  pr.mtype = int8_t(m.type());
  const MoveVis& v = vis_of(m);
  pr.has_vis = true; pr.vis_proj = v.proj; pr.vis_pal = v.pal; pr.vis_trail = v.trail;
}

// Distancia² del punto (cx, cy) al segmento (ax, ay)-(bx, by).
float seg_dist2(float ax, float ay, float bx, float by, float cx, float cy) {
  const float dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy;
  float t = l2 > 0 ? ((cx - ax) * dx + (cy - ay) * dy) / l2 : 0;
  t = t < 0 ? 0 : t > 1 ? 1 : t;
  const float qx = ax + dx * t - cx, qy = ay + dy * t - cy;
  return qx * qx + qy * qy;
}

bool in_arc(float px, float py, float x, float y, float aim, float half) {
  const float a = std::atan2(y - py, x - px);
  return std::fabs(std::fmod(a - aim + kPi * 3, kTau) - kPi) <= half;
}

// ---------------- familias ----------------

void fire_projectiles(Game& g, Player& p, const MoveInst& m) {
  const uint32_t col = kTypeColor[m.type()];
  const int n = std::max(1, m.geti(MS_COUNT));
  const float spread = m.get(MS_SPREAD);
  for (int i = 0; i < n; ++i) {
    const float off = n == 1 ? 0 : (float(i) / (n - 1) - 0.5f) * spread * n;
    const float a = p.aim + off + g.rng.range(-0.03f, 0.03f);
    Projectile* pr = proj_spawn(g);
    if (!pr) break;
    pr->x = p.x + std::cos(a) * 3; pr->y = p.y + std::sin(a) * 3;
    pr->vx = std::cos(a) * m.get(MS_SPEED); pr->vy = std::sin(a) * m.get(MS_SPEED);
    pr->dmg = m.get(MS_DMG) * player_atk(p); pr->r = m.get(MS_SIZE); pr->life = m.get(MS_LIFE);
    pr->pierce = m.geti(MS_PIERCE); pr->color = col; pr->homing = m.get(MS_HOMING);
    pr->owner = int8_t(pl_index(g, p));
    fill_payload(*pr, m);
  }
  g.fx.spark(p.x + std::cos(p.aim) * p.r, p.y - kLift + std::sin(p.aim) * p.r, col, std::cos(p.aim), std::sin(p.aim), 3);
}

void nova(Game& g, Player& p, const MoveInst& m) {
  const uint32_t col = kTypeColor[m.type()];
  const int n = std::max(3, m.geti(MS_COUNT));
  for (int i = 0; i < n; ++i) {
    const float a = (float(i) / n) * kTau + g.rng.range(-0.06f, 0.06f);
    Projectile* pr = proj_spawn(g);
    if (!pr) break;
    pr->x = p.x + std::cos(a) * 3; pr->y = p.y + std::sin(a) * 3;
    pr->vx = std::cos(a) * m.get(MS_SPEED); pr->vy = std::sin(a) * m.get(MS_SPEED);
    pr->dmg = m.get(MS_DMG) * player_atk(p); pr->r = m.get(MS_SIZE); pr->life = m.get(MS_LIFE);
    pr->pierce = m.geti(MS_PIERCE); pr->color = col; pr->homing = m.get(MS_HOMING);
    pr->owner = int8_t(pl_index(g, p));
    fill_payload(*pr, m);
  }
  g.fx.ring(p.x, p.y, p.r, p.r * 4, col, 0.3f, 3);
}

void melee(Game& g, Player& p, const MoveInst& m) {
  const uint32_t col = kTypeColor[m.type()];
  const float radius = m.get(MS_RADIUS), arc = m.get(MS_ARC);
  const float half = std::fmin(arc, kTau) / 2;
  g.fx.slash(p.x, p.y - kLift * 0.5f, p.aim, radius, arc, col, &vis_of(m));
  Enemy* list[kMaxEnemies];
  const int n = g.query_circle(p.x, p.y, radius, list, kMaxEnemies);
  const int32_t ids_n = n;
  int32_t ids[kMaxEnemies];
  for (int i = 0; i < ids_n; ++i) ids[i] = list[i]->id;
  for (int i = 0; i < ids_n; ++i) {
    Enemy* e = g.enemy_by_id(ids[i]);
    if (!e || !in_arc(p.x, p.y, e->x, e->y, p.aim, half)) continue;
    hit(g, p, *e, m.get(MS_DMG) * player_atk(p), col, e->x - p.x, e->y - p.y, m.get(MS_KNOCK), m);
  }
  Prop* props[64];
  const int np = g.world.props_in_circle(p.x, p.y, radius, props, 64);
  for (int i = 0; i < np; ++i)
    if (!props[i]->gone && in_arc(p.x, p.y, props[i]->x, props[i]->y, p.aim, half)) g.world.damage_prop(props[i], m.get(MS_DMG) * player_atk(p), col);
}

void beam(Game& g, Player& p, const MoveInst& m) {
  const uint32_t col = kTypeColor[m.type()];
  const float w = m.get(MS_WIDTH);
  const float ca = std::cos(p.aim), sa = std::sin(p.aim);
  const float radius = m.get(MS_RADIUS);
  // El rayo se corta en la primera pared.
  float len = radius;
  for (float d = 8; d < radius; d += 8) {
    if (g.world.is_wall(p.x + ca * d, p.y + sa * d)) { len = d; break; }
  }
  const MoveVis& v = vis_of(m);
  g.fx.beam(p.x, p.y - kLift, p.aim, len, w, col, v.pal, v.beam);
  g.camera.kick(0.08f);
  const float mx = p.x + ca * len / 2, my = p.y + sa * len / 2;
  auto on_beam = [&](float x, float y, float r) {
    const float px = x - p.x, py = y - p.y;
    const float along = px * ca + py * sa;
    if (along < -r || along > len + r) return false;
    return std::fabs(-px * sa + py * ca) <= w / 2 + r;
  };
  Enemy* list[kMaxEnemies];
  const int n = g.query_circle(mx, my, len / 2 + w, list, kMaxEnemies);
  int32_t ids[kMaxEnemies];
  for (int i = 0; i < n; ++i) ids[i] = list[i]->id;
  for (int i = 0; i < n; ++i) {
    Enemy* e = g.enemy_by_id(ids[i]);
    if (e && on_beam(e->x, e->y, e->r)) hit(g, p, *e, m.get(MS_DMG) * player_atk(p), col, ca, sa, 0, m);
  }
  Prop* props[64];
  const int np = g.world.props_in_circle(mx, my, len / 2 + w, props, 64);
  for (int i = 0; i < np; ++i)
    if (!props[i]->gone && on_beam(props[i]->x, props[i]->y, props[i]->r)) g.world.damage_prop(props[i], m.get(MS_DMG) * player_atk(p), col);
}

void aura(Game& g, Player& p, const MoveInst& m) {
  const uint32_t col = kTypeColor[m.type()];
  const float radius = m.get(MS_RADIUS);
  g.fx.ring(p.x, p.y, radius * 0.5f, radius, col, 0.28f, 2);
  const MoveVis& v = vis_of(m);
  if (v.aura >= 0) {
    for (int i = 0; i < 5; ++i) {
      const float a = g.vrng.next() * kTau, r = radius * g.vrng.range(0.45f, 1);
      const float x = p.x + std::cos(a) * r, y = p.y + std::sin(a) * r * 0.8f - kLift * 0.5f;
      // Hierba Lazo tira hacia ti; veneno y sombras suben; el resto gira.
      SpOpts o;
      if (v.inward) { o.vx = -std::cos(a) * r * 2.2f; o.vy = -std::sin(a) * r * 1.8f; }
      else if (v.rise) { o.vx = g.vrng.range(-10, 10); o.vy = -55; }
      else { o.vx = -std::sin(a) * 70; o.vy = std::cos(a) * 55; }
      o.life = 0.42f;
      o.spin = kShapeInfo[v.aura].spin ? g.vrng.range(-6, 6) : 0;
      g.fx.sp(v.aura, v.pal, x, y, o);
    }
  }
  Enemy* list[kMaxEnemies];
  const int n = g.query_circle(p.x, p.y, radius, list, kMaxEnemies);
  int32_t ids[kMaxEnemies];
  for (int i = 0; i < n; ++i) ids[i] = list[i]->id;
  float healed = 0;
  for (int i = 0; i < n; ++i) {
    Enemy* e = g.enemy_by_id(ids[i]);
    if (!e) continue;
    hit(g, p, *e, m.get(MS_DMG) * player_atk(p), col, e->x - p.x, e->y - p.y, 0, m);
    if (m.get(MS_DRAIN) > 0) healed += m.get(MS_DRAIN);
  }
  Prop* props[64];
  const int np = g.world.props_in_circle(p.x, p.y, radius, props, 64);
  for (int i = 0; i < np; ++i) if (!props[i]->gone) g.world.damage_prop(props[i], m.get(MS_DMG) * player_atk(p), col);
  if (healed > 0) player_heal(g, p, std::fmin(healed, 6.0f));
}

void buff(Game& g, Player& p, const MoveInst& m) {
  player_add_buff(p, m.d().buff, m.get(MS_AMOUNT), m.get(MS_DUR), m.geti(MS_STACKS));
  const uint32_t col = kTypeColor[m.type()];
  g.fx.ring(p.x, p.y, p.r * 2.4f, p.r * 1.1f, col, 0.35f, 2.5f);
  const MoveVis& v = vis_of(m);
  for (int i = 0; i < 3; ++i) {
    SpOpts o; o.vy = -70; o.life = 0.6f;
    const float x = p.x + g.vrng.range(-p.r * 1.4f, p.r * 1.4f);
    const float y = p.y - g.vrng.range(0, p.body_h * 0.8f);
    g.fx.sp(v.buff >= 0 ? int(v.buff) : int(SH_ARROW), v.pal, x, y, o);
  }
}

// Bumerán: sale hacia el enemigo, se para y vuelve a ti golpeando a la ida y a la vuelta.
void boomerang(Game& g, Player& p, const MoveInst& m) {
  const uint32_t col = kTypeColor[m.type()];
  const int n = std::max(1, m.geti(MS_COUNT));
  const float spread = m.get(MS_SPREAD) > 0 ? m.get(MS_SPREAD) : 0.5f;
  const float out = m.get(MS_RADIUS) / m.get(MS_SPEED);   // segundos de ida
  for (int i = 0; i < n; ++i) {
    const float a = p.aim + (n == 1 ? 0 : (float(i) / (n - 1) - 0.5f) * spread * std::fmin(float(n), 3.0f));
    Projectile* pr = proj_spawn(g);
    if (!pr) break;
    pr->x = p.x; pr->y = p.y; pr->vx = std::cos(a) * m.get(MS_SPEED); pr->vy = std::sin(a) * m.get(MS_SPEED);
    pr->dmg = m.get(MS_DMG) * player_atk(p); pr->r = m.get(MS_SIZE); pr->life = out * 4 + 1.5f; pr->pierce = 999; pr->color = col;
    pr->owner = int8_t(pl_index(g, p)); pr->boom = out; pr->bsp = m.get(MS_SPEED) * 1.1f;
    fill_payload(*pr, m);
  }
}

// Cadena: golpea a un enemigo y salta a los siguientes más cercanos (cada salto, algo menos).
void chain(Game& g, Player& p, const MoveInst& m) {
  const uint32_t col = kTypeColor[m.type()];
  const MoveVis& v = vis_of(m);
  Enemy* cur = p.target_enemy ? g.enemy_by_id(p.target_enemy) : nullptr;
  if (cur && cur->dead) cur = nullptr;
  if (!cur) cur = nearest_enemy(g, p.x, p.y, reach(m));
  if (!cur) return;
  float fx = p.x, fy = p.y - kLift, k = 1;
  int32_t done[16];
  int ndone = 0;
  const int n = std::max(1, m.geti(MS_COUNT));
  const float radius = m.get(MS_RADIUS);
  for (int i = 0; i < n && cur; ++i) {
    if (ndone < 16) done[ndone++] = cur->id;
    const float tx = cur->x, ty = cur->y - cur->body_h * 0.5f;
    g.fx.beam(fx, fy, std::atan2(ty - fy, tx - fx), std::hypot(tx - fx, ty - fy), 10, col, v.pal, v.beam);
    // Infortunio: más daño a quien ya sufre un estado.
    const float hex = m.get(MS_HEX) > 0 && (cur->burn > 0 || cur->poison > 0 || cur->slow > 0) ? m.get(MS_HEX) : 1;
    const float cx = cur->x, cy = cur->y;
    hit(g, p, *cur, m.get(MS_DMG) * player_atk(p) * k * hex, col, tx - fx, ty - fy, 0, m);
    k *= 0.85f;
    fx = tx; fy = ty;
    Enemy* best = nullptr;
    float bd = radius * radius;
    Enemy* list[kMaxEnemies];
    const int nn = g.query_circle(cx, cy, radius, list, kMaxEnemies);
    for (int j = 0; j < nn; ++j) {
      bool seen = false;
      for (int q = 0; q < ndone; ++q) if (done[q] == list[j]->id) seen = true;
      if (seen) continue;
      const float d = dist2(cx, cy, list[j]->x, list[j]->y);
      if (d < bd) { bd = d; best = list[j]; }
    }
    cur = best;
  }
  g.camera.kick(0.05f);
}

int count_fields(Game& g, int kind, int owner, bool skip_gone) {
  int c = 0;
  for (const Field& f : g.fields) if (f.kind == kind && f.owner == owner && !(skip_gone && f.gone)) c++;
  return c;
}

// Las más viejas del dueño se van si hay más de `keep`.
void trim_fields(Game& g, int kind, int owner, int keep, bool skip_gone) {
  int c = count_fields(g, kind, owner, skip_gone);
  for (Field& f : g.fields) {
    if (c <= keep) break;
    if (f.kind == kind && f.owner == owner && !(skip_gone && f.gone)) { f.gone = true; c--; }
  }
}

Field* new_field(Game& g, int kind, Player& p, const MoveInst& m, int move_index) {
  Field* f = g.fields.push();
  if (!f) return nullptr;
  *f = Field();
  f->kind = int8_t(kind);
  f->owner = int8_t(pl_index(g, p));
  f->pl_move = int8_t(move_index);
  f->col = kTypeColor[m.type()];
  const MoveVis& v = vis_of(m);
  f->vis_proj = v.proj; f->vis_pal = v.pal; f->vis_aura = v.aura;
  f->dmg = m.get(MS_DMG) * player_atk(p);
  return f;
}

// Minas: se quedan en el suelo y estallan cuando un enemigo las pisa.
void mines(Game& g, Player& p, const MoveInst& m, int mi) {
  const int n = std::max(1, m.geti(MS_COUNT));
  for (int i = 0; i < n; ++i) {
    const float a = g.rng.next() * kTau, d = n == 1 ? 0 : g.rng.range(18, 46);
    Field* f = new_field(g, FK_MINE, p, m, mi);
    if (!f) break;
    f->x = p.x + std::cos(a) * d; f->y = p.y + std::sin(a) * d * 0.8f;
    f->arm = 0.35f; f->life = m.get(MS_DUR) > 0 ? m.get(MS_DUR) : 7; f->r = m.get(MS_RADIUS);
  }
  // Como mucho unas pocas por jugador: las más viejas se van.
  trim_fields(g, FK_MINE, pl_index(g, p), 4 + n * 2, false);
}

// Meteoros: caen del cielo sobre enemigos cercanos (la sombra avisa dónde).
void meteors(Game& g, Player& p, const MoveInst& m, int mi) {
  const int n = std::max(1, m.geti(MS_COUNT));
  Enemy* near[kMaxEnemies];
  int nn = g.query_circle(p.x, p.y, reach(m), near, kMaxEnemies);
  // Barajado (sort con random en la web).
  for (int i = nn - 1; i > 0; --i) { const int j = int(g.rng.next() * (i + 1)); Enemy* t = near[i]; near[i] = near[j]; near[j] = t; }
  for (int i = 0; i < n; ++i) {
    Enemy* e = i < nn ? near[i] : nullptr;
    const float x = e ? e->x + g.rng.range(-8, 8) : p.x + g.rng.range(-140, 140);
    const float y = e ? e->y + g.rng.range(-6, 6) : p.y + g.rng.range(-110, 110);
    Field* f = new_field(g, FK_METEOR, p, m, mi);
    if (!f) break;
    f->x = x; f->y = y; f->t = -i * 0.09f; f->fall = 0.5f; f->r = m.get(MS_RADIUS);
  }
}

// Torreta: deja algo en el suelo que dispara solo un rato.
void turret(Game& g, Player& p, const MoveInst& m, int mi) {
  const float a = p.aim + kPi + g.rng.range(-0.6f, 0.6f);
  Field* f = new_field(g, FK_TURRET, p, m, mi);
  if (f) {
    f->x = p.x + std::cos(a) * 26; f->y = p.y + std::sin(a) * 20;
    f->life = m.get(MS_DUR) > 0 ? m.get(MS_DUR) : 6; f->shot_t = 0.3f;
  }
  trim_fields(g, FK_TURRET, pl_index(g, p), std::max(1, m.geti(MS_COUNT)), true);
  g.fx.ring(p.x, p.y, 4, 30, kTypeColor[m.type()], 0.3f, 2);
}

// Embestida: cruzas como un rayo hacia el enemigo y golpeas a todo lo del camino.
void dash(Game& g, Player& p, const MoveInst& m) {
  const uint32_t col = kTypeColor[m.type()];
  const MoveVis& v = vis_of(m);
  const float ca = std::cos(p.aim), sa = std::sin(p.aim);
  const float sx = p.x, sy = p.y;
  float len = 0;
  for (float d = 8; d <= m.get(MS_RADIUS); d += 8) {
    if (!g.world.is_free(sx + ca * d, sy + sa * d, p.r * 0.8f, true)) break;
    len = d;
  }
  // Estela: varias copias del golpe a lo largo del recorrido.
  for (float d = 0; d <= len; d += 22) {
    SpOpts o; o.life = 0.22f; o.angle = p.aim;
    g.fx.sp(v.proj >= 0 ? int(v.proj) : int(SH_STREAK), v.pal, sx + ca * d, sy + sa * d - kLift * 0.6f, o);
  }
  g.fx.slash(sx + ca * len, sy + sa * len - kLift * 0.5f, p.aim, 34, 2.2f, col, &v);
  p.x = sx + ca * len; p.y = sy + sa * len;
  g.world.collide(p.x, p.y, p.r);
  p.invuln = std::fmax(p.invuln, 0.3f);
  const float w = m.get(MS_WIDTH) > 0 ? m.get(MS_WIDTH) : 34;
  const float mx = sx + ca * len / 2, my = sy + sa * len / 2;
  Enemy* list[kMaxEnemies];
  const int n = g.query_circle(mx, my, len / 2 + w, list, kMaxEnemies);
  int32_t ids[kMaxEnemies];
  for (int i = 0; i < n; ++i) ids[i] = list[i]->id;
  for (int i = 0; i < n; ++i) {
    Enemy* e = g.enemy_by_id(ids[i]);
    if (e && seg_dist2(sx, sy, p.x, p.y, e->x, e->y) <= (w / 2 + e->r) * (w / 2 + e->r))
      hit(g, p, *e, m.get(MS_DMG) * player_atk(p), col, ca, sa, m.get(MS_KNOCK), m);
  }
  Prop* props[64];
  const int np = g.world.props_in_circle(mx, my, len / 2 + w, props, 64);
  for (int i = 0; i < np; ++i)
    if (!props[i]->gone && seg_dist2(sx, sy, p.x, p.y, props[i]->x, props[i]->y) <= (w / 2 + props[i]->r) * (w / 2 + props[i]->r))
      g.world.damage_prop(props[i], m.get(MS_DMG) * player_atk(p), col);
  g.camera.kick(0.12f);
}

// Rastro: vas dejando charcos que dañan a lo que pasa por encima.
void trail_patch(Game& g, Player& p, const MoveInst& m, int mi) {
  const float radius = m.get(MS_RADIUS);
  // Parado no deja uno nuevo encima del anterior (sólo de vez en cuando).
  if (p.last_patch >= 0) {
    // ¿Sigue vivo el último charco y es reciente?
    for (const Field& f : g.fields) {
      if (f.kind == FK_PATCH && f.owner == pl_index(g, p) && f.x == p.last_patch_x && f.y == p.last_patch_y) {
        if (dist2(f.x, f.y, p.x, p.y) < (radius * 0.9f) * (radius * 0.9f) && f.t < 1) return;
        break;
      }
    }
  }
  Field* f = new_field(g, FK_PATCH, p, m, mi);
  if (!f) return;
  f->x = p.x; f->y = p.y; f->life = m.get(MS_DUR) > 0 ? m.get(MS_DUR) : 3; f->r = radius; f->tick = 0;
  p.last_patch = 0; p.last_patch_x = f->x; p.last_patch_y = f->y;
  trim_fields(g, FK_PATCH, pl_index(g, p), 18, true);
}

// Abanico corto: muchos disparos rápidos que no llegan lejos (aliento, chorro...).
void cone(Game& g, Player& p, const MoveInst& m) {
  const uint32_t col = kTypeColor[m.type()];
  const int n = std::max(2, m.geti(MS_COUNT));
  const float arc = m.get(MS_ARC) > 0 ? m.get(MS_ARC) : 0.9f;
  for (int i = 0; i < n; ++i) {
    const float a = p.aim + (float(i) / (n - 1) - 0.5f) * arc + g.rng.range(-0.05f, 0.05f);
    const float sp = m.get(MS_SPEED) * g.rng.range(0.85f, 1.1f);
    Projectile* pr = proj_spawn(g);
    if (!pr) break;
    pr->x = p.x + std::cos(a) * 4; pr->y = p.y + std::sin(a) * 4; pr->vx = std::cos(a) * sp; pr->vy = std::sin(a) * sp;
    pr->dmg = m.get(MS_DMG) * player_atk(p); pr->r = m.get(MS_SIZE); pr->life = m.get(MS_LIFE) * g.rng.range(0.85f, 1.05f);
    pr->pierce = m.geti(MS_PIERCE); pr->color = col;
    pr->owner = int8_t(pl_index(g, p));
    fill_payload(*pr, m);
  }
  g.fx.spark(p.x + std::cos(p.aim) * p.r, p.y - kLift + std::sin(p.aim) * p.r, col, std::cos(p.aim), std::sin(p.aim), 5);
}

void orbit(Game& g, Player& p, MoveInst& m, float dt, float now) {
  const uint32_t col = kTypeColor[m.type()];
  const int want = std::max(1, m.geti(MS_COUNT));
  const int owner = pl_index(g, p);
  auto mine = [&](const Projectile& q) { return q.orb && q.owner == owner && q.mid == m.def; };
  int have = 0;
  float base = now * m.get(MS_SPEED);
  bool first = true;
  for (const Projectile& q : g.projectiles) if (mine(q)) { if (first) { base = q.oa; first = false; } have++; }
  // Si cambia el número de orbes, se reparten por igual desde donde va el primero.
  const bool relayout = have != want;
  if (have > want) {
    int seen = 0;
    for (int i = 0; i < g.projectiles.n; ++i) {
      if (!mine(g.projectiles[i])) continue;
      if (++seen > want) { g.projectiles.remove_ordered(i); --i; }
    }
  }
  for (int i = have; i < want; ++i) {
    Projectile* pr = proj_spawn_orb(g);
    if (!pr) break;
    pr->owner = int8_t(owner); pr->mid = m.def;
    pr->x = p.x; pr->y = p.y; pr->r = m.get(MS_SIZE); pr->color = col;
    pr->dmg = m.get(MS_DMG) * player_atk(p);
    pr->oa = (float(i) / want) * kTau; pr->orad = m.get(MS_RADIUS); pr->ospeed = m.get(MS_SPEED);
    pr->rehit = std::fmax(0.12f, m.get(MS_CD));
    fill_payload(*pr, m);
  }
  int live = 0;
  for (const Projectile& q : g.projectiles) if (mine(q)) live++;
  int k = 0;
  for (Projectile& q : g.projectiles) {
    if (!mine(q)) continue;
    q.dmg = m.get(MS_DMG) * player_atk(p);
    q.orad = m.get(MS_RADIUS);
    q.ospeed = m.get(MS_SPEED);
    q.r = m.get(MS_SIZE);
    if (relayout) q.oa = base + (float(k) / live) * kTau;
    k++;
  }
  // Animación de concentración de vez en cuando, para que se note vivo.
  p.orb_cast_t -= dt;
  if (p.orb_cast_t <= 0) { p.orb_cast_t = 1.6f; player_cast(g, p, nullptr, K_ORBIT, 1.6f); }
}

void run(Game& g, Player& p, MoveInst& m, int mi) {
  switch (m.kind()) {
    case K_PROJECTILE: fire_projectiles(g, p, m); break;
    case K_MELEE: melee(g, p, m); break;
    case K_BEAM: beam(g, p, m); break;
    case K_NOVA: nova(g, p, m); break;
    case K_AURA: aura(g, p, m); break;
    case K_BUFF: buff(g, p, m); break;
    case K_BOOMERANG: boomerang(g, p, m); break;
    case K_CHAIN: chain(g, p, m); break;
    case K_MINE: mines(g, p, m, mi); break;
    case K_METEOR: meteors(g, p, m, mi); break;
    case K_TURRET: turret(g, p, m, mi); break;
    case K_DASH: dash(g, p, m); break;
    case K_TRAIL: trail_patch(g, p, m, mi); break;
    case K_CONE: cone(g, p, m); break;
    default: break;
  }
}

void tick_move(Game& g, Player& p, int mi, float dt) {
  MoveInst& m = p.moves[mi];
  if (m.kind() == K_ORBIT) { orbit(g, p, m, dt, float(g.time)); return; }
  m.t -= dt;
  if (m.t > 0) return;
  // Los ataques que necesitan blanco esperan a tenerlo (no gastes animación al aire).
  if (needs_target(m.kind()) && !(p.has_target && dist(p.x, p.y, p.target_x, p.target_y) <= reach(m))) {
    m.t = 0.05f;
    return;
  }
  m.t = std::fmax(0.05f, m.get(MS_CD) * p.cd_mul);
  player_cast(g, p, &m, m.kind(), m.get(MS_CD));
  m.cast_at = float(g.time);
  run(g, p, m, mi);
}
}  // namespace

void combat_tick(Game& g, Player& p, float dt) {
  if (p.dead) return;
  // Vínculos: todos los movimientos del hueco activo, cada uno con su recarga.
  const int s = p.active_slot;
  for (int j = 0; j < p.slot_n[s]; ++j) tick_move(g, p, p.slots[s][j], dt);
}

// Daño de un campo (mina, meteoro, charco) a todo lo que hay en su círculo.
static void blast(Game& g, Field& f, float r, float k = 1) {
  if (f.ghost) return;
  Player& p = g.players[f.owner];
  const MoveInst& m = p.moves[f.pl_move];
  Enemy* list[kMaxEnemies];
  const int n = g.query_circle(f.x, f.y, r, list, kMaxEnemies);
  int32_t ids[kMaxEnemies];
  for (int i = 0; i < n; ++i) ids[i] = list[i]->id;
  for (int i = 0; i < n; ++i) {
    Enemy* e = g.enemy_by_id(ids[i]);
    if (e) hit(g, p, *e, f.dmg * k, f.col, e->x - f.x, e->y - f.y, m.get(MS_KNOCK), m);
  }
  Prop* props[64];
  const int np = g.world.props_in_circle(f.x, f.y, r, props, 64);
  for (int i = 0; i < np; ++i) if (!props[i]->gone) g.world.damage_prop(props[i], f.dmg * k, f.col);
}

void combat_update_fields(Game& g, float dt) {
  for (int i = g.fields.n - 1; i >= 0; --i) {
    Field& f = g.fields[i];
    f.t += dt;
    if (f.gone || !g.players[f.owner].active || (f.life > 0 && f.t >= f.life && f.kind != FK_MINE)) { g.fields.remove_ordered(i); continue; }
    if (f.kind == FK_MINE) {
      const bool armed = f.t >= f.arm;
      Enemy* tmp[1];
      const bool trig = armed && g.query_circle(f.x, f.y, 14, tmp, 1) > 0;
      if (trig || f.t >= f.life) {
        g.fx.ring(f.x, f.y, 4, f.r, f.col, 0.3f, 4);
        g.fx.burst(f.x, f.y - 6, f.col, 10, 150);
        if (g.camera.sees(f.x, f.y, 40)) g.sfx(SFX_BREAK);
        Field copy = f;
        blast(g, copy, f.r, trig ? 1 : 0.5f);
        g.fields.remove_ordered(i);
      }
    } else if (f.kind == FK_METEOR) {
      if (f.t >= f.fall) {
        g.fx.ring(f.x, f.y, 4, f.r, f.col, 0.35f, 4);
        g.fx.burst(f.x, f.y - 6, f.col, 12, 170);
        if (g.camera.sees(f.x, f.y, 40)) { g.sfx(SFX_BREAK); g.camera.kick(0.12f); }
        Field copy = f;
        blast(g, copy, f.r);
        g.fields.remove_ordered(i);
      }
    } else if (f.kind == FK_TURRET) {
      f.shot_t -= dt;
      if (f.shot_t <= 0) {
        const Player& p = g.players[f.owner];
        const MoveInst& m = p.moves[f.pl_move];
        Enemy* tg = nearest_enemy(g, f.x, f.y, m.get(MS_RANGE) > 0 ? m.get(MS_RANGE) : 300);
        f.shot_t = tg ? (m.get(MS_RATE) > 0 ? m.get(MS_RATE) : 0.6f) : 0.15f;
        if (tg) {
          const float a = std::atan2(tg->y - f.y, tg->x - f.x);
          const float sp = m.get(MS_SPEED) > 0 ? m.get(MS_SPEED) : 300;
          Projectile* pr = proj_spawn(g);
          if (pr) {
            pr->x = f.x; pr->y = f.y; pr->vx = std::cos(a) * sp; pr->vy = std::sin(a) * sp;
            pr->dmg = f.dmg; pr->r = m.get(MS_SIZE) > 0 ? m.get(MS_SIZE) : 6; pr->life = 1.4f; pr->pierce = m.geti(MS_PIERCE);
            pr->color = f.col; pr->ghost = f.ghost; pr->owner = f.owner;
            fill_payload(*pr, m);
          }
          f.kick = 0.12f;
        }
      }
      if (f.kick > 0) f.kick -= dt;
    } else if (f.kind == FK_PATCH) {
      f.tick -= dt;
      if (f.tick <= 0) { f.tick = 0.5f; Field copy = f; blast(g, copy, f.r); }
      Field& ff = g.fields[i];
      if (ff.vis_aura >= 0 && g.vrng.next() < dt * 3 && g.camera.sees(ff.x, ff.y, 20)) {
        SpOpts o; o.vy = -40; o.life = 0.4f;
        g.fx.sp(ff.vis_aura, ff.vis_pal, ff.x + g.vrng.range(-ff.r, ff.r) * 0.6f, ff.y - 4, o);
      }
    }
  }
}

void combat_clear_fields(Game& g, int owner) {
  if (owner < 0) { g.fields.clear(); return; }
  for (int i = g.fields.n - 1; i >= 0; --i) if (g.fields[i].owner == owner) g.fields.remove_ordered(i);
}

static bool has_id(const int32_t* a, int n, int32_t id) {
  for (int i = 0; i < n; ++i) if (a[i] == id) return true;
  return false;
}

void combat_resolve(Game& g, Player& pl, float dt) {
  (void)dt;
  const float now = float(g.time);
  for (int i = g.projectiles.n - 1; i >= 0; --i) {
    Projectile* p = &g.projectiles[i];

    // --- disparos enemigos: paredes y objetos sólidos dan cobertura ---
    if (!p->friendly) {
      if (g.world.projectile_block(p->x, p->y, p->r, nullptr)) {
        g.fx.burst(p->x, p->y - kLift, p->color, 5, 70);
        g.projectiles.remove_ordered(i);
        continue;
      }
      if (dist2(p->x, p->y, pl.x, pl.y) < (p->r + pl.r) * (p->r + pl.r)) {
        player_hurt(g, pl, p->dmg, p->mtype);
        g.fx.burst(p->x, p->y - kLift, p->color, 7, 110);
        g.projectiles.remove_ordered(i);
      }
      continue;
    }

    // --- disparos de jugadores ---
    if (p->orb && p->ghost) continue;
    if (!p->orb) {
      Prop* bp = nullptr;
      const int block = g.world.projectile_block(p->x, p->y, p->r, &bp);
      if (block == 1 || (block == 2 && !bp->destructible)) {
        g.fx.burst(p->x, p->y - kLift, p->color, 5, 80);
        g.projectiles.remove_ordered(i);
        continue;
      }
    }

    // Objetos del escenario (rocas, cofres, hierba).
    bool consumed = false;
    Prop* props[32];
    const int np = g.world.props_in_circle(p->x, p->y, p->r, props, 32);
    for (int k = 0; k < np; ++k) {
      Prop* prop = props[k];
      if (prop->gone) continue;
      if (p->orb) {
        // (los orbes llevan el registro de props con id negativo)
        const int32_t key = -prop->id;
        float last = -99;
        int li = -1;
        for (int q = 0; q < p->nlog; ++q) if (p->log[q].id == key) { last = p->log[q].t; li = q; }
        if (now - last < p->rehit) continue;
        if (li >= 0) p->log[li].t = now;
        else if (p->nlog < Projectile::kHits) p->log[p->nlog++] = {key, now};
        else { p->log[0] = {key, now}; }
        g.world.damage_prop(prop, p->dmg, p->color);
        continue;
      }
      if (has_id(p->phits, p->nphits, prop->id)) continue;
      if (p->nphits < 16) p->phits[p->nphits++] = prop->id;
      const bool solid = prop->solid;
      if (!p->ghost) g.world.damage_prop(prop, p->dmg, p->color);
      // La hierba no frena; lo sólido sí gasta perforación.
      if (solid) {
        if (p->pierce > 0) p->pierce--;
        else { consumed = true; break; }
      }
    }
    if (consumed) {
      g.fx.burst(p->x, p->y - kLift, p->color, 5, 80);
      g.projectiles.remove_ordered(i);
      continue;
    }

    // Enemigos. Colisión BARRIDA: todo el tramo recorrido en este paso.
    const float sx = p->orb || !p->has_prev ? p->x : p->px, sy = p->orb || !p->has_prev ? p->y : p->py;
    const float half = std::hypot(p->x - sx, p->y - sy) / 2;
    Enemy* list[kMaxEnemies];
    const int n = g.query_circle((sx + p->x) / 2, (sy + p->y) / 2, half + p->r, list, kMaxEnemies);
    int32_t ids[kMaxEnemies];
    for (int k = 0; k < n; ++k) ids[k] = list[k]->id;
    const int32_t pid = p->id;
    bool removed = false;
    for (int k = 0; k < n; ++k) {
      Enemy* e = g.enemy_by_id(ids[k]);
      if (!e || e->dead) continue;
      // (enemy_hurt puede soltar cosas pero no mueve los proyectiles)
      p = &g.projectiles[i];
      if (p->id != pid) { removed = true; break; }
      if (seg_dist2(sx, sy, p->x, p->y, e->x, e->y) > (p->r + e->r) * (p->r + e->r)) continue;
      if (p->orb) {
        float last = -99;
        int li = -1;
        for (int q = 0; q < p->nlog; ++q) if (p->log[q].id == e->id) { last = p->log[q].t; li = q; }
        if (now - last < p->rehit) continue;
        if (li >= 0) p->log[li].t = now;
        else {
          // Las órbitas duran toda la partida: se olvidan los golpes viejos.
          if (p->nlog >= Projectile::kHits) {
            int w = 0;
            for (int q = 0; q < p->nlog; ++q) if (now - p->log[q].t <= p->rehit) p->log[w++] = p->log[q];
            p->nlog = w;
            if (p->nlog >= Projectile::kHits) p->nlog = Projectile::kHits - 1;
          }
          p->log[p->nlog++] = {e->id, now};
        }
      } else {
        if (has_id(p->hits, p->nhits, e->id)) continue;
        if (p->nhits < Projectile::kHits) p->hits[p->nhits++] = e->id;
      }

      // Disparo fantasma de un compañero: choca y se ve, pero no daña.
      if (p->ghost) {
        SpOpts o; o.life = 0.12f; o.scale = 1;
        g.fx.sp(SH_IMPACT, p->has_vis ? int(p->vis_pal) : int(T_NORMAL), e->x, e->y - e->body_h * 0.5f, o);
        if (p->pierce > 0) { p->pierce--; continue; }
        g.fx.burst(p->x, p->y - kLift, p->color, 5, 80);
        g.projectiles.remove_ordered(i);
        removed = true;
        break;
      }

      const bool crit = is_crit(g, pl);
      const float kk = (p->mtype >= 0 ? type_k(g, p->mtype, *e) : 1) * (crit ? CRIT_MUL : 1);
      const float dvx = p->vx != 0 ? p->vx : (e->x - pl.x), dvy = p->vy != 0 ? p->vy : (e->y - pl.y);
      const int eff = crit ? EFF_CRIT : p->mtype >= 0 ? type_label(type_mult(p->mtype, e->types)) : EFF_NONE;
      const float dmg = p->dmg * kk;
      const float burn = p->burn, poison = p->poison, slow = p->slow;
      const float hx = p->x, hy = p->y;
      const int8_t pal = p->has_vis ? int(p->vis_pal) : int(T_NORMAL);
      const uint32_t pcol = p->color;
      enemy_hurt(g, *e, dmg, pcol, dvx, dvy, p->knock, eff);
      if (g.vrng.next() < 0.5f) {
        SpOpts o; o.life = 0.12f; o.scale = 1;
        g.fx.sp(SH_IMPACT, pal, e->x, e->y - e->body_h * 0.5f, o);
      }
      pl.dmg_dealt += dmg;
      on_dealt(pl, dmg);
      if (burn > 0) enemy_apply_burn(*e, burn);
      if (poison > 0) enemy_apply_poison(*e, poison);
      if (slow > 0) enemy_apply_slow(*e, slow);

      p = &g.projectiles[i];
      if (!p->orb) {
        if (p->pierce > 0) { p->pierce--; }
        else {
          g.fx.burst(hx, hy - kLift, pcol, 5, 80);
          g.projectiles.remove_ordered(i);
          removed = true;
          break;
        }
      }
    }
    (void)removed;
  }

  // --- contacto enemigo ↔ jugador ---
  if (pl.dead) return;
  Enemy* list[kMaxEnemies];
  const int n = g.query_circle(pl.x, pl.y, pl.r + 40, list, kMaxEnemies);
  int32_t ids[kMaxEnemies];
  for (int k = 0; k < n; ++k) ids[k] = list[k]->id;
  for (int k = 0; k < n; ++k) {
    Enemy* e = g.enemy_by_id(ids[k]);
    if (!e || e->dead) continue;
    if (dist2(e->x, e->y, pl.x, pl.y) > (e->r + pl.r) * (e->r + pl.r)) continue;
    if (e->touch_cd > 0 || e->behavior == BH_THIEF) continue;
    e->touch_cd = 0.55f;
    enemy_strike(*e, pl);
    player_hurt(g, pl, e->dmg, e->atk_type);
    if (pl.item == IT_ROCKYHELMET) enemy_hurt(g, *e, 15 + pl.level * 4.0f, 0x9aa6b5, e->x - pl.x, e->y - pl.y, 80);
    float nx, ny;
    norm(e->x - pl.x, e->y - pl.y, nx, ny);
    e->kx += nx * 150; e->ky += ny * 150;
  }
}

}  // namespace ps

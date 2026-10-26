// player.cpp — el Pokémon que controlas (player.js de la web).
// (x, y) son los PIES. Sólo UN hueco está activo a la vez: se dispara solo y
// se cambia con los botones. Cada disparo reproduce la animación de ataque de
// Mundo Misterioso que le toca (Attack, Shoot o Charge).
#include <cmath>
#include <cstdio>

#include "core/game.h"
#include "core/orbs.h"
#include "core/mega.h"
#include "core/stats.h"
#include "core/upgrades.h"

namespace ps {

float xp_for(int level) {
  // (×2 y luego ×4 el 2026-10-10: se subía de nivel demasiado rápido.)
  return float(std::floor((8 + level * 6 + std::pow(double(level), 1.75) * 1.6) * 4));
}

void player_init(Game& g, Player& p, int dex, bool shiny, const RunConfig& cfg) {
  (void)g;
  p = Player();
  p.active = true;
  p.dex = int16_t(dex);
  p.mon = mon_by_dex(dex);
  p.shiny = shiny && has_shiny(dex);
  // Los grandes, algo más pequeños y con el choque de uno normal (anim.h).
  p.scale = player_scale(dex);
  p.anim.init(dex, p.scale, p.shiny);
  p.r = player_radius(dex);
  p.body_h = body_height(dex, p.scale);
  const Perks k = upgrade_perks(cfg.upgrades);
  p.atk_mul = k.atk_mul; p.spd_mul = k.spd_mul; p.cd_mul = k.cd_mul;
  p.dmg_reduce = k.dmg_reduce; p.regen_flat = k.regen;
  p.magnet = 78 * k.magnet_mul; p.xp_mul = k.xp_mul; p.coin_mul = k.coin_mul;
  p.max_hp = p.mon->hp + k.hp;
  p.hp = p.max_hp;
  p.base_spd = p.mon->spd;
  p.base_atk = p.mon->atk;
  p.xp_next = xp_for(1);
  p.reroll_bonus = k.rerolls;
  p.crit = k.crit;
  p.mega_up = cfg.mega_up;
  for (auto& b : p.bind) b = -1;
  for (int s = 0; s < 4; ++s) for (int j = 0; j < 4; ++j) p.slots[s][j] = -1;
}

// ---------------- stats efectivos ----------------
float player_atk(const Player& p) { return p.base_atk * p.atk_mul * (1 + p.buffs[BS_ATK].n * p.buffs[BS_ATK].amount); }

// Monotipo (19-10-2026): los de un solo tipo pegan un +20 % con los ataques de su tipo.
constexpr float kMonoK = 1.2f;
bool player_mono_type(const Player& p, int type) { return p.mon->types[1] < 0 && type == p.mon->types[0]; }

// Ataque con un movimiento (atkFor): el físico o el especial de tu Pokémon
// (stats.h), con las danzas que sean de su lado.
float player_atk_for(const Player& p, const MoveInst& m) {
  const int cat = m.cat();
  const Buff* b = cat == CAT_PHYSICAL ? &p.buffs[BS_PATK] : cat == CAT_SPECIAL ? &p.buffs[BS_SATK] : nullptr;
  return player_atk(p) * stats_fit(p.mon, cat) * (b ? 1 + b->n * b->amount : 1) * (player_mono_type(p, m.type()) ? kMonoK : 1);
}

float player_spd(Game& g, const Player& p) {
  const float s = p.base_spd * p.spd_mul * (1 + p.buffs[BS_SPD].n * p.buffs[BS_SPD].amount) * (p.slow_t > 0 ? 0.5f : 1);
  const float w = weather_speed_for(g, p.mon->types);
  float slow = 1;
  if (p.in_liquid) { const LiquidFx le = g.world.liquid_effect(p.mon->types); if (le.slow > 0) slow = le.slow; }
  // En la Sentencia, la velocidad tiene tope suave (contra la huida sin fin).
  return judgment_active(g) ? judgment_cap_spd(s * slow * w) : s * slow * w;
}

float player_reduction(const Player& p) { return std::fmin(0.8f, p.dmg_reduce + p.buffs[BS_DEF].n * p.buffs[BS_DEF].amount); }
float player_regen(const Player& p) { return p.regen_flat + p.buffs[BS_REGEN].n * p.buffs[BS_REGEN].amount; }

// ---------------- movimientos ----------------
bool player_has_move(const Player& p, int def) {
  for (int i = 0; i < p.nmoves; ++i) if (p.moves[i].def == def) return true;
  return false;
}

static int want_slot(const Player& p, int s) {
  int k = 99;
  for (int j = 0; j < p.slot_n[s]; ++j) {
    const int b = p.bind[p.moves[p.slots[s][j]].def];
    if (b >= 0 && b < k) k = b;
  }
  return k;
}

// Huecos fijos (Mejoras → Atajos): cada movimiento con hueco elegido se pone
// en ese número. Mientras no tengas tantos, va al más cercano; los demás
// rellenan los huecos libres en el orden en que los aprendiste.
static void arrange(Player& p) {
  const int n = p.nslots;
  if (n < 2) return;
  int8_t cur_first = p.slot_n[p.active_slot] ? p.slots[p.active_slot][0] : -1;
  int out[4] = {-1, -1, -1, -1};   // hueco nuevo → hueco viejo
  // Los que tienen hueco, por orden de lo que piden (estable).
  int order[4], no = 0;
  for (int s = 0; s < n; ++s) if (want_slot(p, s) < 99) order[no++] = s;
  for (int a = 1; a < no; ++a) {
    const int v = order[a];
    int b = a - 1;
    while (b >= 0 && want_slot(p, order[b]) > want_slot(p, v)) { order[b + 1] = order[b]; --b; }
    order[b + 1] = v;
  }
  for (int k = 0; k < no; ++k) {
    int q = std::min(want_slot(p, order[k]), n - 1);
    while (q >= 0 && out[q] >= 0) q--;                 // ocupado: el de antes
    if (q < 0) { q = 0; while (out[q] >= 0) q++; }     // o el primero libre
    out[q] = order[k];
  }
  for (int s = 0; s < n; ++s) {
    bool used = false;
    for (int k = 0; k < n; ++k) if (out[k] == s) used = true;
    if (used) continue;
    for (int k = 0; k < n; ++k) if (out[k] < 0) { out[k] = s; break; }
  }
  int8_t ns[4][4], nn[4];
  for (int k = 0; k < n; ++k) {
    nn[k] = p.slot_n[out[k]];
    for (int j = 0; j < 4; ++j) ns[k][j] = p.slots[out[k]][j];
  }
  for (int k = 0; k < n; ++k) {
    p.slot_n[k] = nn[k];
    for (int j = 0; j < 4; ++j) p.slots[k][j] = ns[k][j];
  }
  if (cur_first >= 0)
    for (int k = 0; k < n; ++k) if (p.slot_n[k] && p.slots[k][0] == cur_first) p.active_slot = k;
}

bool player_add_move(Game& g, Player& p, int def) {
  if (p.nslots >= p.max_moves || player_has_move(p, def) || p.nmoves >= 8) return false;
  const int mi = p.nmoves++;
  p.moves[mi] = move_instance(def, 1);
  const int s = p.nslots++;
  p.slots[s][0] = int8_t(mi);
  p.slot_n[s] = 1;
  p.moves[mi].since = float(g.time);   // desde cuándo lo tienes (resumen de la pausa)
  arrange(p);
  // Te quedas con el que llevabas (18-10-2026: antes se cambiaba solo al
  // nuevo). Sólo si no tenías ninguno, el nuevo.
  if (p.active_slot >= p.nslots || !p.slot_n[p.active_slot]) {
    for (int k = 0; k < p.nslots; ++k)
      if (p.slot_n[k] == 1 && p.slots[k][0] == mi) { p.active_slot = k; break; }
  }
  return true;
}

void player_set_active(Game& g, Player& p, int i) {
  if (i < 0 || i >= p.nslots || i == p.active_slot) return;
  // Cinta Elección: no se puede cambiar de movimiento.
  if (p.item == IT_CHOICEBAND && p.slot_n[p.active_slot] > 0) {
    g.fx.dmg_text(p.x, p.y - p.body_h - 8, "Cinta Elección", 0xff8ad8);
    return;
  }
  p.active_slot = i;
  proj_clear_orbs(g, int(&p - g.players));
  g.sfx(SFX_SELECT);
  for (int j = 0; j < p.slot_n[i]; ++j) {
    MoveInst& m = p.moves[p.slots[i][j]];
    m.t = std::fmin(m.t, m.get(MS_CD) * 0.35f);
  }
  const MoveInst& m = p.moves[p.slots[i][0]];
  g.fx.ring(p.x, p.y - kLift * 0.5f, 6, p.r * 3.4f, kTypeColor[m.type()], 0.3f, 3);
}

void player_cycle(Game& g, Player& p, int dir) {
  if (!p.nslots) return;
  player_set_active(g, p, (p.active_slot + dir + p.nslots) % p.nslots);
}

// ---------------- vínculos (fusiones) ----------------
// Cada legendario vencido da un vínculo: se juntan movimientos en un solo hueco
// y se lanzan a la vez. Sólo crece un grupo (hasta 4); si no hay ninguno a
// medias, el vínculo une dos movimientos sueltos.
// Devuelve 0 (nada que hacer), 1 (añadir al grupo grow_slot) o 2 (unir dos sueltos).
int player_bond_options(const Player& p, int8_t* loose, int& nloose, int& grow_slot) {
  nloose = 0; grow_slot = -1;
  for (int s = 0; s < p.nslots; ++s) if (p.slot_n[s] == 1) loose[nloose++] = p.slots[s][0];
  for (int s = 0; s < p.nslots; ++s) if (p.slot_n[s] > 1 && p.slot_n[s] < 4) { grow_slot = s; break; }
  if (grow_slot >= 0) return nloose ? 1 : 0;
  return nloose >= 2 ? 2 : 0;
}

void player_fuse(Game& g, Player& p, const int* move_idx, int n, int grow_slot) {
  if (n <= 0) return;
  const int8_t cur_first = p.slot_n[p.active_slot] ? p.slots[p.active_slot][0] : -1;
  // Grupo destino: el que crece o el hueco del primero.
  int8_t target_first = -1;
  if (grow_slot >= 0) target_first = p.slots[grow_slot][0];
  else target_first = int8_t(move_idx[0]);
  for (int k = 0; k < n; ++k) {
    const int m = move_idx[k];
    int ts = -1;
    for (int s = 0; s < p.nslots; ++s) if (p.slot_n[s] && p.slots[s][0] == target_first) ts = s;
    if (ts < 0) break;
    bool in = false;
    for (int j = 0; j < p.slot_n[ts]; ++j) if (p.slots[ts][j] == m) in = true;
    if (in) continue;
    int si = -1;
    for (int s = 0; s < p.nslots; ++s) if (p.slot_n[s] == 1 && p.slots[s][0] == m) si = s;
    if (si < 0 || p.slot_n[ts] >= 4) continue;
    // Quita el hueco suelto (desplazando los demás) y añade al grupo.
    for (int s = si; s < p.nslots - 1; ++s) {
      p.slot_n[s] = p.slot_n[s + 1];
      for (int j = 0; j < 4; ++j) p.slots[s][j] = p.slots[s + 1][j];
    }
    p.nslots--;
    for (int s = 0; s < p.nslots; ++s)
      if (p.slot_n[s] && p.slots[s][0] == target_first) { p.slots[s][p.slot_n[s]++] = int8_t(m); break; }
  }
  // Mantiene el hueco activo apuntando a lo que había (o al grupo si se ha fundido).
  p.active_slot = 0;
  for (int s = 0; s < p.nslots; ++s) if (p.slot_n[s] && p.slots[s][0] == target_first) p.active_slot = s;
  for (int s = 0; s < p.nslots; ++s) if (p.slot_n[s] && p.slots[s][0] == cur_first) p.active_slot = s;
  arrange(p);
  int a = -1, t = 0;
  for (int s = 0; s < p.nslots; ++s) {
    if (p.slot_n[s] && p.slots[s][0] == cur_first) a = s;
    if (p.slot_n[s] && p.slots[s][0] == target_first) t = s;
  }
  // Se pasa al grupo, salvo con la Cinta Elección (sigues en el hueco que tenías, si queda).
  p.active_slot = (p.item == IT_CHOICEBAND && a >= 0) ? a : t;
  p.bonds = std::max(0, p.bonds - 1);
  proj_clear_orbs(g, int(&p - g.players));
}

// Lo llama combat al lanzar un movimiento. Elige la animación de PMD y la
// acelera si la recarga es más corta que la animación, para que encadene.
void player_cast(Game& g, Player& p, const MoveInst* m, int kind, float cd) {
  // Vidasfera: cada ataque cuesta un poco de vida (nunca te deja KO).
  if (p.item == IT_LIFEORB && kind != K_ORBIT && kind != K_TRAIL) p.hp = std::fmax(1.0f, p.hp - std::fmax(1.0f, p.max_hp * 0.01f));
  int anim = A_ATTACK;
  switch (kind) {
    case K_MELEE: case K_NOVA: case K_DASH: anim = A_ATTACK; break;
    case K_BEAM: case K_PROJECTILE: case K_BOOMERANG: case K_CHAIN: case K_CONE: anim = A_SHOOT; break;
    case K_AURA: case K_BUFF: case K_ORBIT: case K_MINE: case K_METEOR: case K_TURRET: anim = A_CHARGE; break;
    case K_TRAIL: anim = A_WALK; break;
    default: break;
  }
  // Las familias nuevas suenan como la parecida de las de siempre.
  int snd = kind;
  switch (kind) {
    case K_BOOMERANG: snd = K_PROJECTILE; break;
    case K_CHAIN: snd = K_BEAM; break;
    case K_MINE: case K_METEOR: snd = K_NOVA; break;
    case K_TURRET: snd = K_BUFF; break;
    case K_DASH: snd = K_MELEE; break;
    case K_TRAIL: snd = K_AURA; break;
    case K_CONE: snd = K_PROJECTILE; break;
    default: break;
  }
  // El rastro no suena a cada charco (sería un zumbido constante).
  if (kind != K_TRAIL) g.sfx(SFX_SHOT, snd, m ? m->type() : -1);
  if (kind == K_TRAIL) return;
  // No reinicies una animación que va por la mitad: se vería a tirones.
  if (p.anim.busy() && p.anim.progress() < 0.65f) return;
  const AnimMeta* meta = anim_meta(p.dex, anim);
  float len = 0.4f;
  if (meta) { float s = 0; for (int i = 0; i < meta->n; ++i) s += kAnimDur[meta->dur + i]; len = s / 60; }
  const float c = std::fmax(0.12f, cd * p.cd_mul);
  p.anim.play(anim, std::fmax(1.0f, len / (c * 0.9f)));
  p.attack_face = 0.35f;
}

// ---------------- daño y curación ----------------
// cat: 'physical' | 'special', con qué defensa tuya se para (la del enemigo que pega).
void player_hurt(Game& g, Player& p, float amount, int type, bool pure, int cat) {
  if (p.invuln > 0 || p.dead) return;
  if (!pure) amount *= stats_taken(p.mon, cat);
  int eff = EFF_NONE;
  if (type >= 0) {
    const float k = type_mult(type, p.mon->types);
    eff = type_label(k);
    amount *= k * weather_mult(g, type);
  }
  // Con un ataque cuerpo a cuerpo activo se recibe un 25 % menos.
  bool close = false;
  for (int j = 0; j < p.slot_n[p.active_slot]; ++j) if (p.moves[p.slots[p.active_slot][j]].kind() == K_MELEE) close = true;
  const float d = pure ? std::fmax(1.0f, amount) : std::fmax(1.0f, amount * (1 - player_reduction(p)) * (close ? 0.75f : 1));
  p.hp -= d;
  p.invuln = 0.45f;
  p.flash = 0.14f;
  g.sfx(SFX_HURT);
  g.camera.kick(0.28f);
  char b[16];
  std::snprintf(b, sizeof(b), "-%d", int(jsround(d)));
  g.fx.dmg_text(p.x, p.y - p.body_h - 4, b, eff == EFF_SUPER ? 0xff3048 : eff == EFF_WEAK ? 0xc9a0a8 : 0xff6b7a, eff == EFF_SUPER, true);
  g.fx.burst(p.x, p.y - p.body_h * 0.5f, 0xff5f6d, 6, 90);
  // Banda Focus: aguanta el golpe con 1 de vida (una vez cada 60 s).
  if (p.hp <= 0 && p.item == IT_FOCUSSASH && g.time - p.sash_t > 60) {
    p.sash_t = float(g.time);
    p.hp = 1; p.invuln = 1.2f;
    g.fx.dmg_text(p.x, p.y - p.body_h - 16, "¡Banda Focus!", 0xff9a3d, true);
    g.fx.ring(p.x, p.y, 6, 70, 0xff9a3d, 0.5f, 4);
  }
  if ((g.debug_immortal || g.cfg.members[int(&p - g.players) & 3].immortal) && p.hp < 1) p.hp = 1;
  if (p.hp <= 0) {
    p.hp = 0; p.dead = true;
    p.anim.play(A_FAINT, 1, true);
    proj_clear_orbs(g, int(&p - g.players));
    p.revive_t = 0;
  }
}

void player_heal(Game& g, Player& p, float amount) {
  const float before = p.hp;
  p.hp = std::fmin(p.max_hp, p.hp + amount);
  const int got = int(jsround(p.hp - before));
  if (got >= 1) {
    char b[16];
    std::snprintf(b, sizeof(b), "+%d", got);
    g.fx.dmg_text(p.x, p.y - p.body_h - 6, b, 0x5fe08a, false, true);
  }
}

void player_add_max_hp(Player& p, float amount) { p.max_hp += amount; p.hp += amount; }

int player_gain_xp(Player& p, float amount) {
  p.xp += amount * p.xp_mul;
  int leveled = 0;
  while (p.xp >= p.xp_next) {
    p.xp -= p.xp_next;
    p.level++;
    p.xp_next = xp_for(p.level);
    leveled++;
  }
  return leveled;
}

// ---------------- objeto equipado ----------------
static void item_on(Player& p, int id) {
  switch (id) {
    case IT_LEFTOVERS: p.regen_flat += p.max_hp * 0.015f; p.item_regen = p.max_hp * 0.015f; break;
    case IT_CHOICEBAND: p.atk_mul *= 1.4f; break;
    case IT_QUICKCLAW: p.cd_mul *= 0.8f; break;
    case IT_AMULETCOIN: p.coin_mul *= 1.5f; break;
    case IT_LUCKYEGG: p.xp_mul *= 1.3f; break;
    case IT_LIFEORB: p.atk_mul *= 1.3f; break;
    case IT_CHOICESCARF: p.spd_mul *= 1.35f; break;
    case IT_ASSAULTVEST: p.dmg_reduce += 0.2f; break;
    default: break;
  }
}
static void item_off(Player& p, int id) {
  switch (id) {
    case IT_LEFTOVERS: p.regen_flat -= p.item_regen; break;
    case IT_CHOICEBAND: p.atk_mul /= 1.4f; break;
    case IT_QUICKCLAW: p.cd_mul /= 0.8f; break;
    case IT_AMULETCOIN: p.coin_mul /= 1.5f; break;
    case IT_LUCKYEGG: p.xp_mul /= 1.3f; break;
    case IT_LIFEORB: p.atk_mul /= 1.3f; break;
    case IT_CHOICESCARF: p.spd_mul /= 1.35f; break;
    case IT_ASSAULTVEST: p.dmg_reduce -= 0.2f; break;
    default: break;
  }
}

void player_equip(Game& g, Player& p, int id) {
  (void)g;
  if (p.item >= 0) item_off(p, p.item);
  p.item = int8_t(id);
  if (id >= 0) item_on(p, id);
}

// ---------------- poderes de altar ----------------
static void power_on(Player& p, int id) {
  switch (id) {
    case 0: p.atk_mul *= 2; break;
    case 1: p.spd_mul *= 1.45f; break;
    case 2: p.magnet *= 4; break;
    case 3: p.cd_mul *= 0.6f; break;
    default: break;
  }
}
static void power_off(Player& p, int id) {
  switch (id) {
    case 0: p.atk_mul /= 2; break;
    case 1: p.spd_mul /= 1.45f; break;
    case 2: p.magnet /= 4; break;
    case 3: p.cd_mul /= 0.6f; break;
    default: break;
  }
}

void player_add_power(Player& p, int def, float dur) {
  if (p.power.def >= 0) power_off(p, p.power.def);
  power_on(p, def);
  p.power.def = int8_t(def); p.power.t = dur; p.power.dur = dur;
}

void player_add_buff(Player& p, int stat, float amount, float dur, int max_stacks) {
  if (stat < 0 || stat >= BS_COUNT) return;
  Buff& b = p.buffs[stat];
  b.amount = amount;
  b.n = std::min(max_stacks, b.n + 1);
  b.t = dur;
}

// ---------------- ciclo ----------------
void player_update(Game& g, Player& p, const Input& in, float dt) {
  p.anim.update(dt);
  mega_update(g, p, dt);   // (también caído: la mega se acaba al caer)
  orbs_update(g, p, dt);
  if (p.dead) return;
  if (p.invuln > 0) p.invuln -= dt;
  if (p.flash > 0) p.flash -= dt;
  if (p.attack_face > 0) p.attack_face -= dt;

  for (auto& b : p.buffs) if (b.n > 0) { b.t -= dt; if (b.t <= 0) b.n = 0; }
  if (p.slow_t > 0) p.slow_t -= dt;
  if (p.bell_heal > 0) { const float h = std::fmin(p.bell_heal, 4 * dt); p.hp = std::fmin(p.max_hp, p.hp + h); p.bell_heal -= h; }
  if (p.power.def >= 0 && (p.power.t -= dt) <= 0) { power_off(p, p.power.def); p.power.def = -1; }
  const float rg = player_regen(p);
  if (rg > 0) p.hp = std::fmin(p.max_hp, p.hp + rg * dt);

  // --- movimiento con colisiones ---
  const float ax = p.frozen ? 0 : in.ax, ay = p.frozen ? 0 : in.ay;
  p.walking = ax != 0 || ay != 0;
  const float s = player_spd(g, p);
  // Hielo: resbalas (la velocidad cambia despacio).
  const bool slide = p.in_liquid && g.world.liquid_effect(p.mon->types).slide;
  p.svx = slide ? damp(p.svx, ax * s * 1.15f, 1.6f, dt) : ax * s;
  p.svy = slide ? damp(p.svy, ay * s * 1.15f, 1.6f, dt) : ay * s;
  p.x += p.svx * dt;
  p.y += p.svy * dt;
  g.world.collide(p.x, p.y, p.r);
  if (p.walking) p.move_angle = std::atan2(ay, ax);

  // --- terreno líquido ---
  p.in_liquid = g.world.is_liquid(p.x, p.y);
  if (p.in_liquid) {
    const LiquidFx le = g.world.liquid_effect(p.mon->types);
    if (le.dmg > 0) {
      p.lava_tick -= dt;
      if (p.lava_tick <= 0) {
        p.lava_tick = le.every; p.invuln = 0;
        player_hurt(g, p, std::fmax(le.flat, p.max_hp * le.dmg * 0.5f));
        g.fx.burst(p.x, p.y - 4, le.color, 5, 70);
      }
    } else if (p.walking && g.vrng.next() < dt * 8) {
      g.fx.burst(p.x, p.y, le.slide ? 0xffffff : 0xb3defa, 2, 40);
    }
  }

  // --- apuntado: enemigo más cercano; si no hay, hacia donde caminas ---
  //   a distancia (proyectil, rayo): el jefe si está a tiro > lo que te toca > el más cercano
  //   cuerpo a cuerpo y el resto:   lo que te toca > el jefe > el más cercano
  // Sin enemigos, apunta a la roca o cofre más cercano.
  const MoveInst* m = p.slot_n[p.active_slot] ? &p.moves[p.slots[p.active_slot][0]] : nullptr;
  const bool ranged = m && (m->kind() == K_PROJECTILE || m->kind() == K_BEAM);
  Enemy* threat = nearest_enemy(g, p.x, p.y, p.r + 24);
  Enemy* boss = nullptr;
  for (Enemy& e : g.enemies) if (e.boss && !e.dead && dist(p.x, p.y, e.x, e.y) < 330) { boss = &e; break; }
  Enemy* tg = ranged ? (boss ? boss : threat) : (threat ? threat : boss);
  if (!tg) tg = nearest_enemy(g, p.x, p.y, 520);
  p.target_enemy = 0; p.target_prop = nullptr; p.has_target = false;
  if (tg) {
    p.target_enemy = tg->id; p.has_target = true; p.target_x = tg->x; p.target_y = tg->y;
  } else if (Prop* pr = g.world.nearest_breakable(p.x, p.y, 200)) {
    p.target_prop = pr; p.has_target = true; p.target_x = pr->x; p.target_y = pr->y;
  }
  if (p.has_target) p.aim = std::atan2(p.target_y - p.y, p.target_x - p.x);
  else if (p.walking) p.aim = p.move_angle;

  // --- animación: mira al objetivo al atacar, si no a donde camina ---
  const float face = p.attack_face > 0 || !p.walking ? p.aim : p.move_angle;
  p.anim.dir = int8_t(dir_from_angle(face));
  p.anim.loop(p.walking ? A_WALK : A_IDLE, p.walking ? s / 110 : 1);
}

void player_revive(Game& g, Player& p, float frac) {
  p.dead = false;
  p.hp = std::fmax(1.0f, p.max_hp * frac);
  p.invuln = 2;
  p.revive_t = 0;
  p.anim.start(A_IDLE, false);
  g.fx.ring(p.x, p.y, 6, 70, 0x5fe08a, 0.5f, 4);
  g.fx.burst(p.x, p.y - p.body_h * 0.5f, 0x9dffb8, 18, 150);
  g.sfx(SFX_HEAL);
}

}  // namespace ps

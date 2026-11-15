// power.cpp — ver power.h.
#include "core/power.h"

#include <cmath>
#include <cstdio>

#include "core/game.h"
#include "core/mega.h"

namespace ps {

const PowerInfo kPowerInfo[PWK_COUNT] = {
    {"Megaevolución", "40 s con su mega: sus tipos y mucha más fuerza, sin megapiedra. Recarga 90 s.", 40, 90, 0xff8ad8},
    {"Dinamax", "12 s gigante: doble de vida, +40 % de daño, cada golpe salpica a los de alrededor y aplastas lo que pisas. Recarga 80 s.", 12, 80, 0xff3a5a},
    {"Pulsera Z", "Onda expansiva al usarlo y, durante 15 s, una gran Z de tu tipo rueda hacia los enemigos. Al acabar, el movimiento Z arrasa la pantalla. Recarga 60 s.", 15, 60, 0xffd23f},
    {"Teracristal", "30 s con corona de cristal: tus ataques de tu tipo pegan un 80 % más. Recarga 75 s.", 30, 75, 0x7fe8ff},
    {"Megaevolución +", "Ya tienes tu megapiedra: su megaevolución dura 10 s más, se recarga 5 s antes y pega un 10 % más.", 0, 0, 0xff8ad8},
};
// (medido con build-bal/ps_powers el 15-11-2026, 40 partidas por opción, daño por segundo con la piedra:
// sólo la piedra 1,019; + Megaevolución + 1,145; + Dinamax 1,103; + Z 1,204; + Teracristal 1,157)
float g_mp_time = 10, g_mp_cd = 5, g_mp_atk = 1.10f;

namespace {
void say_to(Game& g, const Player& p, const char* t, float s) { if (&p == &g.players[g.local]) g.say(t, s); }

// Daño de un golpe de poder: como un buen ataque de tu Pokémon a estas alturas.
float power_dmg(const Player& p, float k) { return 60 * player_atk(p) * k * (1 + p.level * 0.04f); }

void wave(Game& g, Player& p, float r, float k, uint32_t col) {
  g.fx.ring(p.x, p.y, 8, r, col, 0.5f, 6);
  g.fx.burst(p.x, p.y - p.body_h * 0.5f, col, 30, 260);
  g.camera.kick(0.8f);
  Enemy* list[kMaxEnemies];
  const int n = g.query_circle(p.x, p.y, r, list, kMaxEnemies);
  int32_t ids[kMaxEnemies];
  for (int i = 0; i < n; ++i) ids[i] = list[i]->id;
  const int type = p.mon->types[0];
  for (int i = 0; i < n; ++i) {
    Enemy* e = g.enemy_by_id(ids[i]);
    if (!e || e->dead) continue;
    const float d = power_dmg(p, k) * type_mult(type, e->types);
    enemy_hurt(g, *e, d, col, e->x - p.x, e->y - p.y, 200, type_label(type_mult(type, e->types)));
    p.dmg_dealt += d;
  }
}

void z_roll(Game& g, Player& p) {
  const int type = p.mon->types[0];
  Enemy* t = nearest_enemy(g, p.x, p.y, 520);
  const float a = t ? std::atan2(t->y - p.y, t->x - p.x) : p.aim;
  Projectile* pr = proj_spawn(g);
  if (!pr) return;
  pr->x = p.x; pr->y = p.y; pr->vx = std::cos(a) * 260; pr->vy = std::sin(a) * 260;
  pr->dmg = power_dmg(p, 0.6f); pr->r = 18; pr->life = 2.4f; pr->pierce = 999;
  pr->color = kTypeColor[type]; pr->owner = int8_t(&p - g.players); pr->mtype = int8_t(type); pr->mcat = CAT_NONE;
  pr->has_vis = true; pr->vis_proj = SH_ZROLL; pr->vis_pal = int8_t(type);
}

void stop(Game& g, Player& p) {
  const int k = p.epower;
  p.power_on = false;
  p.power_cd = kPowerInfo[k].cd * p.power_cd_k;
  if (k == PWK_DYNA) {
    p.max_hp -= p.power_hp; p.hp = std::fmin(p.hp, p.max_hp);
    if (!p.dead && p.hp < 1) p.hp = 1;
    p.power_hp = 0;
    p.anim.scale = p.scale; p.body_h = body_height(p.anim.dex, p.scale); p.r = player_radius(p.dex);
  } else if (k == PWK_Z) {
    // El movimiento Z: arrasa la pantalla
    wave(g, p, 420, 3.2f, kPowerInfo[PWK_Z].color);
    g.flash(0xffe680, 0.5f);
    char b[96];
    std::snprintf(b, sizeof(b), "¡%s!", kZMoveName[p.mon->types[0] >= 0 ? p.mon->types[0] : 0]);
    say_to(g, p, b, 2.6f);
  }
}
}  // namespace

const char* const kZMoveName[T_COUNT] = {
    "Carrera Arrolladora", "Hecatombe Pírica", "Hidrovórtice Abisal", "Megatón Floral", "Gigavoltio Destructor", "Crioaliento Despiadado",
    "Ráfaga Demoledora", "Diluvio Corrosivo", "Barrena Telúrica", "Picado Supersónico", "Disruptor Psíquico", "Guadaña Sedosa",
    "Aplastamiento Gigalítico", "Presa Espectral", "Dracoaliento Devastador", "Agujero Negro Aniquilador", "Hélice Trepanadora", "Arrumaco Sideral"};

void power_offer(Game& g, Player& p) {
  if (p.epower >= 0) return;   // (ya tiene uno: en una partida sólo hay un Eternatus)
  p.npower_opts = 0;
  // La Megaevolución, si tu Pokémon tiene mega y aún no has encontrado su megapiedra; si ya la
  // llevas, puedes quedártela (su mega recarga 5 s antes) o cambiarla por otro poder.
  const bool stone = is_mega_stone(p.item) && mega_index(p.dex, p.item - IT_MEGA_A) >= 0;
  if (stone) p.power_opts[p.npower_opts++] = PWK_KEEP;
  else if (mega_index(p.dex, 0) >= 0) p.power_opts[p.npower_opts++] = PWK_MEGA;
  p.power_opts[p.npower_opts++] = PWK_DYNA;
  p.power_opts[p.npower_opts++] = PWK_Z;
  p.power_opts[p.npower_opts++] = PWK_TERA;
  p.choice = CH_POWER;
  if (g.state == ST_PLAYING) g.state = ST_CHOICE;
}

void power_choose(Game& g, Player& p, int pick) {
  if (p.choice != CH_POWER) return;
  if (pick < 0 || pick >= p.npower_opts) pick = 0;
  char b[120];
  if (p.power_opts[pick] == PWK_KEEP) {
    p.mega_plus = true;
    p.mega_cd = std::fmax(0.0f, p.mega_cd - g_mp_cd);
    say_to(g, p, "¡Megaevolución +! Tu mega dura más, recarga antes y pega más", 3.4f);
    p.choice = CH_NONE;
    if (g.state == ST_CHOICE && !g.anyone_choosing()) g.state = ST_PLAYING;
    return;
  }
  p.epower = p.power_opts[pick];
  p.power_on = false; p.power_cd = 0;
  std::snprintf(b, sizeof(b), "¡Poder: %s! Pulsa Y para usarlo", kPowerInfo[p.epower].name);
  say_to(g, p, b, 3.4f);
  p.choice = CH_NONE;
  if (g.state == ST_CHOICE && !g.anyone_choosing()) g.state = ST_PLAYING;
}

bool power_ready(const Player& p) { return p.epower >= 0 && !p.power_on && p.power_cd <= 0 && !p.dead && p.mega < 0; }

void power_start(Game& g, Player& p) {
  if (!power_ready(p) || (g.state != ST_PLAYING && g.state != ST_CHOICE)) return;
  const int k = p.epower;
  if (k == PWK_MEGA) {   // la mega de siempre (core/mega), con los tiempos del poder
    p.power_mega = true;
    mega_start(g, p);
    p.power_mega = false;
    if (p.mega >= 0) { p.mega_t = kPowerInfo[PWK_MEGA].dur; p.power_on = true; p.power_t = p.mega_t; }
    return;
  }
  p.power_on = true;
  p.power_t = kPowerInfo[k].dur;
  const uint32_t col = kPowerInfo[k].color;
  g.fx.ring(p.x, p.y - p.body_h * 0.5f, 120, 8, col, 0.6f, 5);
  g.sfx(SFX_SURGE);
  char b[96];
  if (k == PWK_DYNA) {
    p.power_hp = p.max_hp;
    p.max_hp += p.power_hp; p.hp += p.power_hp;
    p.anim.scale = p.scale * 1.8f; p.body_h = body_height(p.anim.dex, p.anim.scale);
    std::snprintf(b, sizeof(b), "¡%s se ha dinamaxizado!", mon_by_dex(p.dex)->name);
  } else if (k == PWK_Z) {
    wave(g, p, 220, 1, col);
    p.z_t = 0.3f;
    std::snprintf(b, sizeof(b), "¡PODER Z! %s", kZMoveName[p.mon->types[0] >= 0 ? p.mon->types[0] : 0]);
  } else {
    std::snprintf(b, sizeof(b), "¡%s se ha teracristalizado! Tipo %s", mon_by_dex(p.dex)->name, kTypeName[p.mon->types[0] >= 0 ? p.mon->types[0] : 0]);
  }
  say_to(g, p, b, 2.6f);
}

void power_update(Game& g, Player& p, float dt) {
  if (p.epower < 0) return;
  if (p.epower == PWK_MEGA) {   // (la mega se acaba sola: entonces empieza su recarga)
    if (p.power_on && p.mega < 0) { p.power_on = false; p.power_cd = kPowerInfo[PWK_MEGA].cd * p.power_cd_k; p.mega_cd = 0; }
    else if (!p.power_on && p.power_cd > 0) p.power_cd -= dt;
    return;
  }
  if (!p.power_on) { if (p.power_cd > 0) p.power_cd -= dt; return; }
  p.power_t -= dt;
  if (p.dead) { p.power_t = 0; }
  if (p.epower == PWK_DYNA) {
    // Aplasta a los Pokémon normales que pisa y destroza lo que toca.
    const float R = p.r * 2.2f;
    Enemy* list[kMaxEnemies];
    const int n = g.query_circle(p.x, p.y, R, list, kMaxEnemies);
    for (int i = 0; i < n; ++i) {
      Enemy& e = *list[i];
      if (e.dead || e.boss || e.legend || e.eterna || e.ecore || e.arceus || e.judge || e.crush_t > 0) continue;
      e.crush_t = 0.5f;
      enemy_hurt(g, e, power_dmg(p, 0.8f), kPowerInfo[PWK_DYNA].color, e.x - p.x, e.y - p.y, 300, EFF_NONE);
    }
    Prop* props[32];
    const int np = g.world.props_in_circle(p.x, p.y, R, props, 32);
    for (int i = 0; i < np; ++i) if (!props[i]->gone) g.world.damage_prop(props[i], 9999, 0xff3a5a);
  } else if (p.epower == PWK_Z) {
    if ((p.z_t -= dt) <= 0) { p.z_t = 1.2f; z_roll(g, p); }
  }
  if (p.power_t <= 0) stop(g, p);
}

float power_out_k(const Player& p, const MoveInst* m) {
  if (!p.power_on) return 1;
  if (p.epower == PWK_DYNA) return 1.4f;
  if (p.epower == PWK_TERA && m && (m->type() == p.mon->types[0] || m->type() == p.mon->types[1])) return 1.8f;
  return 1;
}

void power_after_hit(Game& g, Player& p, Enemy& e, float dmg) {
  if (!p.power_on || p.epower != PWK_DYNA || p.splashing) return;
  p.splashing = true;
  Enemy* list[kMaxEnemies];
  const int n = g.query_circle(e.x, e.y, 70, list, kMaxEnemies);
  int32_t ids[kMaxEnemies];
  for (int i = 0; i < n; ++i) ids[i] = list[i]->id;
  const int32_t me = e.id;
  for (int i = 0; i < n && i < 6; ++i) {
    Enemy* o = g.enemy_by_id(ids[i]);
    if (o && o->id != me && !o->dead) enemy_hurt(g, *o, dmg * 0.4f, 0xff3a5a, o->x - e.x, o->y - e.y, 0, EFF_NONE);
  }
  p.splashing = false;
}

}  // namespace ps

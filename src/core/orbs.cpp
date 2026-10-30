// orbs.cpp — esferas de Mundo Misterioso (orbs.js de la web; ver orbs.h).
#include "core/orbs.h"

#include <cmath>
#include <cstdio>

#include "core/game.h"
#include "core/mega.h"

namespace ps {

const OrbDef kOrbs[ORB_COUNT] = {
    {"slumber", "Soporesfera", true, 0, 0xb39bff, "Duerme a los enemigos de la pantalla 4 s (a los jefes, 1 s)."},
    {"slow", "Ralentosfera", true, 0, 0x5fd0ff, "Los enemigos de la pantalla van un 50 % más lentos durante 8 s (los jefes, un 20 %)."},
    {"blowback", "Rafagaesfera", true, 0, 0x9ff0c0, "Lanza lejos a los enemigos que tienes cerca (a los jefes, la mitad)."},
    {"focus", "Focosfera", false, 15, 0xff6f7d, "+25 % de crítico durante 15 s, para ti y los compañeros cercanos."},
    {"luminous", "Luminosfera", false, 0, 0xffe680, "Hace brillar un tesoro lejos: tienes 40 s para llegar siguiendo la flecha dorada. Trae lo de un cofre grande y monedas extra."},
    {"quick", "Rapidosfera", false, 10, 0xffb24d, "+40 % de velocidad durante 10 s, para ti y los compañeros cercanos."},
    {"heal", "Curaesfera", false, 0, 0x7dffa0, "Cura un 30 % de la vida a todo el equipo, esté donde esté."},
};

namespace {
constexpr float PUSH = 1700, PUSH_R = 240;   // la Rafagaesfera los lleva a ~1/3 de pantalla

int slot_of(const Player& p, int id) {
  for (int i = 0; i < kOrbSlots; ++i) if (p.orb_id[i] == id) return i;
  return -1;
}

void ring(Game& g, int id, float x, float y, float r = 260) { g.fx.ring(x, y, 10, r, kOrbs[id].col, 0.6f, 5); }

void buff_on(Player& p, int id) {
  if (id == ORB_FOCUS) p.crit += 0.25f;
  else if (id == ORB_QUICK) p.spd_mul *= 1.4f;
}
void buff_off(Player& p, int id) {
  if (id == ORB_FOCUS) p.crit -= 0.25f;
  else if (id == ORB_QUICK) p.spd_mul /= 1.4f;
}

void buff(Game& g, Player& p, int id, const Player& user) {
  if (!(p.orb_buff[id] > 0)) buff_on(p, id);
  p.orb_buff[id] = kOrbs[id].dur;
  g.fx.dmg_text(p.x, p.y - p.body_h - 8, kOrbs[id].name, kOrbs[id].col, true);
  // Lo que te da, en grande (a quien la recibe en esta consola).
  if (&p == &g.players[g.local]) {
    char b[120];
    if (&p == &user) std::snprintf(b, sizeof(b), "¡%s! %s durante %d s", kOrbs[id].name, orb_effect(id), int(kOrbs[id].dur));
    else std::snprintf(b, sizeof(b), "¡%s de un compañero! %s durante %d s", kOrbs[id].name, orb_effect(id), int(kOrbs[id].dur));
    g.say(b, 3.2f);
  }
}

// A los enemigos que ve quien la usa.
void foe(Game& g, const Player& user, int id) {
  ring(g, id, user.x, user.y);
  if (&user == &g.players[g.local]) {
    char b[120];
    std::snprintf(b, sizeof(b), "¡%s! %s", kOrbs[id].name, orb_effect(id));
    g.say(b, 3);
  }
  const float hw = g.camera.w / 2 + 60, hh = g.camera.h / 2 + 60;
  for (Enemy& e : g.enemies) {
    if (e.dead || e.arceus) continue;
    const float dx = e.x - user.x, dy = e.y - user.y;
    const bool big = e.boss || e.legend || e.judge;
    if (id == ORB_BLOWBACK) {
      if (dx * dx + dy * dy > PUSH_R * PUSH_R) continue;
      float nx, ny;
      norm(dx != 0 ? dx : 1, dy, nx, ny);
      e.kx += nx * PUSH * (big ? 0.5f : 1); e.ky += ny * PUSH * (big ? 0.5f : 1);
      continue;
    }
    if (std::fabs(dx) > hw || std::fabs(dy) > hh) continue;
    if (id == ORB_SLUMBER) e.sleep_t = std::fmax(e.sleep_t, big ? 1.0f : 4.0f);
    else if (id == ORB_SLOW) enemy_apply_slow(e, big ? 0.2f : 0.5f, 8);
  }
}
}  // namespace

int orb_roll(Game& g) { return int(g.rng.next() * ORB_COUNT) % ORB_COUNT; }

bool orb_can_take(const Player& p, int id) {
  const int s = slot_of(p, id);
  if (s >= 0) return p.orb_n[s] < kOrbStack;
  return slot_of(p, -1) >= 0;
}

bool orb_add(Player& p, int id) {
  if (id < 0 || id >= ORB_COUNT || !orb_can_take(p, id)) return false;
  int s = slot_of(p, id);
  if (s >= 0) p.orb_n[s]++;
  else { s = slot_of(p, -1); p.orb_id[s] = int8_t(id); p.orb_n[s] = 1; }
  p.orb_new = int8_t(s); p.orb_new_t = 3;
  return true;
}

void orb_got(Game& g, Player& p, int id) {
  if (!orb_add(p, id)) return;
  const OrbDef& o = kOrbs[id];
  g.fx.dmg_text(p.x, p.y - p.body_h - 8, o.name, o.col, true);
  g.fx.ring(p.x, p.y, 6, 50, o.col, 0.4f, 3);
  // La primera vez en la partida: qué hace y cómo se usa (sólo a quien la coge).
  if (!(p.orb_seen & (1u << id))) {
    p.orb_seen |= uint8_t(1u << id);
    if (&p == &g.players[g.local]) {
      char b[200];
      std::snprintf(b, sizeof(b), "%s (A para usarla, B cambia de esfera): %s", o.name, o.desc);
      g.say(b, 5);
    }
  }
}

bool orb_use(Game& g, Player& p, int slot) {
  if (slot < 0 || slot >= kOrbSlots || p.orb_id[slot] < 0 || p.dead) return false;
  if (p.orb_cd > 0 || (g.state != ST_PLAYING)) return false;
  const int id = p.orb_id[slot];
  p.orb_cd = kOrbCd;
  if (--p.orb_n[slot] <= 0) { p.orb_id[slot] = -1; p.orb_n[slot] = 0; }
  g.sfx(SFX_CONFIRM);
  if (id == ORB_LUMINOUS) {
    orb_place_treasure(g, p);
  } else if (id == ORB_HEAL) {
    // A todo el equipo (vivo), esté donde esté.
    for (int i = 0; i < g.nplayers; ++i) {
      Player& o = g.players[i];
      if (!o.active || o.dead) continue;
      player_heal(g, o, o.max_hp * kOrbHeal);
      g.fx.ring(o.x, o.y, 8, 70, kOrbs[id].col, 0.5f, 4);
      g.fx.dmg_text(o.x, o.y - o.body_h - 8, kOrbs[id].name, kOrbs[id].col, true);
      if (&o == &g.players[g.local]) {
        char b[120];
        if (&o == &p) std::snprintf(b, sizeof(b), "¡%s! %s", kOrbs[id].name, orb_effect(id));
        else std::snprintf(b, sizeof(b), "¡%s de un compañero! %s", kOrbs[id].name, orb_effect(id));
        g.say(b, 3);
      }
    }
  } else if (kOrbs[id].foe) {
    foe(g, p, id);
  } else {
    // A ti y a los compañeros a menos de media pantalla.
    ring(g, id, p.x, p.y, kOrbNear);
    for (int i = 0; i < g.nplayers; ++i) {
      Player& o = g.players[i];
      if (!o.active || o.dead) continue;
      if (&o != &p && dist2(o.x, o.y, p.x, p.y) >= kOrbNear * kOrbNear) continue;
      buff(g, o, id, p);
    }
  }
  return true;
}

void orbs_update(Game& g, Player& p, float dt) {
  (void)g;
  if (p.orb_cd > 0) p.orb_cd -= dt;
  if (p.orb_new_t > 0) p.orb_new_t -= dt;
  for (int id = 0; id < ORB_COUNT; ++id)
    if (p.orb_buff[id] > 0 && (p.orb_buff[id] -= dt) <= 0) { p.orb_buff[id] = 0; buff_off(p, id); }
}

// ---------------- el tesoro de la Luminosfera ----------------

void orb_place_treasure(Game& g, const Player& p) {
  // Lejos (kTreasureMin-kTreasureMax), en suelo por el que se pueda andar; si no hay
  // sitio, más cerca.
  float x = p.x, y = p.y;
  bool ok = false;
  for (int i = 0; i < 24 && !ok; ++i) {
    const float a = g.rng.next() * kTau, d = g.rng.range(kTreasureMin, kTreasureMax) * (i < 16 ? 1.0f : 0.6f);
    x = p.x + std::cos(a) * d; y = p.y + std::sin(a) * d;
    ok = g.world.is_free(x, y, 16);
  }
  if (!ok) { x = p.x + 60; y = p.y; }
  g.treasure.on = true; g.treasure.x = x; g.treasure.y = y; g.treasure.t = 0;
  g.fx.ring(p.x, p.y, 10, kOrbNear, kOrbs[ORB_LUMINOUS].col, 0.6f, 5);
  g.sfx(SFX_SHINY);
  if (&p == &g.players[g.local]) g.say("¡Luminosfera! Brilla un tesoro lejos: tienes 40 s, sigue la flecha dorada", 4);
}

void treasure_update(Game& g, float dt) {
  Game::Treasure& t = g.treasure;
  if (!t.on) return;
  t.t += dt;
  if (g.rift.arena || judgment_active(g)) { t.on = false; return; }   // (en una arena no)
  if (t.t > kTreasureTime) {   // (si no se recoge a tiempo, se apaga)
    t.on = false;
    g.say("El tesoro de la Luminosfera se ha apagado", 2.5f);
    return;
  }
  for (int i = 0; i < g.nplayers; ++i) {
    const Player& p = g.players[i];
    if (!p.active || p.dead || dist2(p.x, p.y, t.x, t.y) > 34 * 34) continue;
    // ¡Encontrado! Lo de un cofre grande (tickets, a veces un objeto, esferas,
    // experiencia…) y, además, monedas extra (el dueño, 2026-10-30: con un objeto
    // seguro y tickets de más daba demasiado). En Halloween, también un caramelo.
    t.on = false;
    {
      Prop chest{};
      chest.type = PR_BIGCHEST; chest.x = t.x; chest.y = t.y;
      g.on_prop_destroyed(chest);
    }
    for (int k = 0; k < 10; ++k) pickup_drop(g, PK_COIN, t.x + g.rng.range(-24, 24), t.y + g.rng.range(-16, 16), 4);
    if (g.hw) pickup_drop(g, PK_CANDY, t.x + 12, t.y - 12, 1);
    g.fx.burst(t.x, t.y - 10, 0xffe680, 30, 220);
    g.fx.ring(t.x, t.y, 6, 70, 0xffd23f, 0.5f, 4);
    g.sfx(SFX_CHEST);
    g.say("¡Tesoro encontrado!", 2.4f);
    return;
  }
}



const char* orb_effect(int id) {
  switch (id) {
    case ORB_SLUMBER: return "Los enemigos de la pantalla se duermen";
    case ORB_SLOW: return "Los enemigos de la pantalla van más lentos";
    case ORB_BLOWBACK: return "Los enemigos cercanos salen volando";
    case ORB_FOCUS: return "+25 % de crítico";
    case ORB_LUMINOUS: return "Un tesoro brilla lejos: 40 s para llegar";
    case ORB_QUICK: return "+40 % de velocidad";
    case ORB_HEAL: return "Todo el equipo recupera un 30 % de vida";
    default: return "";
  }
}

int orb_count(const Player& p) {
  int n = 0;
  for (int i = 0; i < kOrbSlots; ++i) if (p.orb_id[i] >= 0) n += p.orb_n[i];
  return n;
}

}  // namespace ps

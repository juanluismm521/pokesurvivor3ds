// orbs.cpp — esferas de Mundo Misterioso (orbs.js de la web; ver orbs.h).
#include "core/orbs.h"

#include <cmath>
#include <cstdio>

#include "core/game.h"

namespace ps {

const OrbDef kOrbs[ORB_COUNT] = {
    {"slumber", "Soporesfera", true, 0, 0xb39bff, "Duerme a los enemigos de la pantalla 4 s (a los jefes, 1 s)."},
    {"slow", "Ralentosfera", true, 0, 0x5fd0ff, "Los enemigos de la pantalla van un 50 % más lentos durante 8 s (los jefes, un 20 %)."},
    {"blowback", "Rafagaesfera", true, 0, 0x9ff0c0, "Lanza lejos a los enemigos que tienes cerca (a los jefes, la mitad)."},
    {"focus", "Focosfera", false, 15, 0xff6f7d, "+25 % de crítico durante 15 s, para ti y los compañeros cercanos."},
    {"luminous", "Luminosfera", false, 15, 0xffe680, "Ves más campo (la cámara se aleja) durante 15 s, tú y los compañeros cercanos."},
    {"quick", "Rapidosfera", false, 10, 0xffb24d, "+40 % de velocidad durante 10 s, para ti y los compañeros cercanos."},
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
  // (ORB_LUMINOUS: lo ve el dibujo y el que hace salir a los enemigos, orb_zoom)
}
void buff_off(Player& p, int id) {
  if (id == ORB_FOCUS) p.crit -= 0.25f;
  else if (id == ORB_QUICK) p.spd_mul /= 1.4f;
}

void buff(Game& g, Player& p, int id) {
  if (!(p.orb_buff[id] > 0)) buff_on(p, id);
  p.orb_buff[id] = kOrbs[id].dur;
  g.fx.dmg_text(p.x, p.y - p.body_h - 8, kOrbs[id].name, kOrbs[id].col, true);
}

// A los enemigos que ve quien la usa.
void foe(Game& g, const Player& user, int id) {
  ring(g, id, user.x, user.y);
  const float z = orb_zoom(user);
  const float hw = g.camera.w / 2 * z + 60, hh = g.camera.h / 2 * z + 60;
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
  if (kOrbs[id].foe) {
    foe(g, p, id);
  } else {
    // A ti y a los compañeros a menos de media pantalla.
    ring(g, id, p.x, p.y, kOrbNear);
    for (int i = 0; i < g.nplayers; ++i) {
      Player& o = g.players[i];
      if (!o.active || o.dead) continue;
      if (&o != &p && dist2(o.x, o.y, p.x, p.y) >= kOrbNear * kOrbNear) continue;
      buff(g, o, id);
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

float orb_zoom(const Player& p) { return p.orb_buff[ORB_LUMINOUS] > 0 ? kLuminousZoom : 1.0f; }

int orb_count(const Player& p) {
  int n = 0;
  for (int i = 0; i < kOrbSlots; ++i) if (p.orb_id[i] >= 0) n += p.orb_n[i];
  return n;
}

}  // namespace ps

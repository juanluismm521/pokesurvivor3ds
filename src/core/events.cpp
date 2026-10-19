// events.cpp — eventos a mitad de partida y duendes del tesoro (events.js).
// Cada 4-5 minutos (nunca con un jefe vivo ni en la grieta):
//   horde  30 s en los que todos los que salen son de un mismo tipo
//   gold   30 s de enemigos dorados (doble de experiencia, a veces monedas)
//   lost   Pokémon perdido: lo tocas, te sigue y lo llevas hasta la marca
// Duendes del tesoro (1-2 por partida): un Meowth cargado de monedas (o, 1 de
// cada 4, un Gholdengo con un ticket ×10) que huye y se escapa a los 15 s.
#include <cmath>
#include <cstdio>

#include "core/game.h"

namespace ps {

namespace {
constexpr float EVERY0 = 240, EVERY1 = 300, DUR = 30, LOST_TIME = 120;
constexpr float THIEF_FIRST0 = 120, THIEF_FIRST1 = 240, THIEF_NEXT0 = 240, THIEF_NEXT1 = 360, THIEF_LIFE = 15;

const char* mon_name(int dex) { const Mon* m = mon_by_dex(dex); return m ? m->name : ""; }

Player& pick_alive(Game& g) {
  Player* alive[kMaxPlayers];
  int n = 0;
  for (int i = 0; i < g.nplayers; ++i) if (g.players[i].active && !g.players[i].dead) alive[n++] = &g.players[i];
  if (!n) { for (int i = 0; i < g.nplayers; ++i) alive[n++] = &g.players[i]; }
  return *alive[int(g.rng.next() * n)];
}

void end_event(Game& g) { g.events.cur = EVN_NONE; }

void announce(Game& g) {
  char b[128];
  if (g.events.cur == EVN_HORDE) { std::snprintf(b, sizeof(b), "¡Horda de tipo %s!", kTypeName[g.events.horde_type]); g.say(b, 3); }
  else if (g.events.cur == EVN_GOLD) g.say("¡Hora dorada! Doble de experiencia", 3);
  else if (g.lost.on) { std::snprintf(b, sizeof(b), "¡Un %s se ha perdido! Búscalo y llévalo a la marca", mon_name(g.lost.dex)); g.say(b, 3.4f); }
  g.sfx(g.events.cur == EVN_GOLD ? SFX_SHINY : SFX_SURGE);
}

void spawn_thief(Game& g) {
  const bool gold = g.rng.next() < 0.25f;
  const int dex = gold ? 1000 : 52;
  const Mon* mon = mon_by_dex(dex);
  if (!mon) return;
  Player& pl = pick_alive(g);
  EnemyDef def;
  def.dex = int16_t(dex); def.behavior = BH_THIEF;
  def.hp = float(jsround(60 * (1 + g.time / 95) * (gold ? 1.5 : 1)));
  def.spd = 92; def.dmg = 0; def.xp = 10; def.loot_t10 = gold;
  for (int i = 0; i < 12; ++i) {
    const float a = g.rng.next() * kTau, d = g.rng.range(230, 300);
    const float x = pl.x + std::cos(a) * d, y = pl.y + std::sin(a) * d * 0.8f;
    if (!g.world.is_free(x, y, 14)) continue;
    Enemy* e = enemy_spawn(g, def, x, y, Scale3());
    if (!e) return;
    e->life = THIEF_LIFE;
    g.fx.ring(x, y, 4, 40, 0xffd23f, 0.5f, 3);
    char b[128];
    std::snprintf(b, sizeof(b), gold ? "¡Un %s con un ticket ×10! ¡Atrápalo antes de que huya!" : "¡Un %s cargado de monedas! ¡Atrápalo antes de que huya!", mon->name);
    g.say(b, 3.2f);
    g.sfx(SFX_SHINY);
    return;
  }
}

bool spawn_lost(Game& g) {
  Player& pl = pick_alive(g);
  static int16_t pool[1024];
  int np = 0;
  for (int i = 0; i < kDexCount; ++i) {
    const Mon& p = kDex[i];
    if (p.rarity <= 3 && !p.leg && !hovers(p.dex)) pool[np++] = p.dex;
  }
  const int dex = pool[int(g.rng.next() * np)];
  bool at = false, to = false;
  float ax = 0, ay = 0, tx = 0, ty = 0;
  for (int i = 0; i < 20 && !at; ++i) {
    const float a = g.rng.next() * kTau, d = g.rng.range(220, 300);
    const float x = pl.x + std::cos(a) * d, y = pl.y + std::sin(a) * d * 0.8f;
    if (g.world.is_free(x, y, 16)) { at = true; ax = x; ay = y; }
  }
  for (int i = 0; i < 30 && at && !to; ++i) {
    const float a = g.rng.next() * kTau, d = g.rng.range(700, 850);
    const float x = ax + std::cos(a) * d, y = ay + std::sin(a) * d * 0.85f;
    if (g.world.is_free(x, y, 30)) { to = true; tx = x; ty = y; }
  }
  if (!at || !to) return false;
  Lost& L = g.lost;
  L = Lost();
  L.on = true; L.dex = int16_t(dex); L.x = ax; L.y = ay; L.tx = tx; L.ty = ty;
  L.follow = false; L.hp = 100; L.max_hp = 100;
  L.grant = g.rng.next() < 0.2f;
  L.anim.init(dex, 1);
  L.r = body_radius(dex);
  return true;
}

void start(Game& g, int id) {
  g.events.last = int8_t(id);
  if (id == EVN_HORDE) {
    int8_t types[T_COUNT];
    const int nt = enemies_horde_types(float(g.time), types);
    if (!nt) { start(g, EVN_GOLD); return; }
    const int type = types[int(g.rng.next() * nt)];
    g.events.nhorde = enemies_horde_defs(g, type, float(g.time), g.events.horde);
    g.events.cur = EVN_HORDE; g.events.cur_t = 0; g.events.cur_dur = DUR; g.events.horde_type = int8_t(type);
    spawner_burst(g, int(jsround(6 * coop_scale_rate(g.team_size()))));
  } else if (id == EVN_GOLD) {
    g.events.cur = EVN_GOLD; g.events.cur_t = 0; g.events.cur_dur = DUR;
    for (Enemy& e : g.enemies) enemy_make_golden(e);
  } else {
    if (!spawn_lost(g)) return;
    g.events.cur = EVN_LOST; g.events.cur_t = 0; g.events.cur_dur = 1e30f;
  }
  announce(g);
}

void reward(Game& g, int dex, bool grant) {
  Player& pl = g.pl();
  // (en la web, cada consola cura al suyo: aquí, a todos)
  for (int i = 0; i < g.nplayers; ++i) {
    Player& q = g.players[i];
    if (q.active && !q.dead) { player_heal(g, q, q.max_hp * 0.5f); g.fx.ring(q.x, q.y, 6, 70, 0x5fe08a, 0.5f, 3); }
  }
  g.fx.burst(g.lost.on ? g.lost.tx : pl.x, g.lost.on ? g.lost.ty - 10 : pl.y, 0x7dffb0, 24, 180);
  g.sfx(SFX_LEGEND);
  if (grant) g.on_rescue(dex);
  else {
    char b[128];
    std::snprintf(b, sizeof(b), "¡Has llevado a %s a casa! Te lo agradece", mon_name(dex));
    g.say(b, 3.2f);
  }
}

void rescued(Game& g) {
  Lost& L = g.lost;
  for (int i = 0; i < 12; ++i) pickup_drop(g, PK_COIN, L.tx + g.rng.range(-40, 40), L.ty + g.rng.range(-30, 30), 5);
  pickup_drop(g, PK_TICKET, L.tx, L.ty - 12, 1);
  reward(g, L.dex, L.grant);
  L.on = false;
  end_event(g);
}

void gone(Game& g, const char* msg) {
  g.say(msg, 3);
  g.lost.on = false;
  end_event(g);
}

void lost_tick(Game& g, float dt) {
  Lost& L = g.lost;
  L.t += dt;
  L.anim.update(dt);
  if (!L.follow) {
    for (int i = 0; i < g.nplayers; ++i) {
      Player& o = g.players[i];
      if (!o.active || o.dead) continue;
      const float rr = o.r + L.r + 18;
      if (dist2(o.x, o.y, L.x, L.y) < rr * rr) {
        L.follow = true; L.follow_pl = int8_t(i);
        char b[128];
        std::snprintf(b, sizeof(b), "¡%s te sigue! Llévalo a la marca verde", mon_name(L.dex));
        g.say(b, 3);
        g.sfx(SFX_CONFIRM);
        break;
      }
    }
    L.moving = false;
  } else {
    Player* p = &g.players[L.follow_pl];
    if (p->dead) for (int i = 0; i < g.nplayers; ++i) if (g.players[i].active && !g.players[i].dead) { p = &g.players[i]; break; }
    const float d = dist(L.x, L.y, p->x, p->y), keep = p->r + L.r + 14;
    L.moving = d > keep;
    if (L.moving) {
      const float sp = std::fmin(d - keep, std::fmax(120.0f, player_spd(g, *p) * 1.15f) * dt);
      float nx, ny;
      norm(p->x - L.x, p->y - L.y, nx, ny);
      L.x += nx * sp; L.y += ny * sp;
      g.world.collide(L.x, L.y, L.r);
      L.anim.dir = int8_t(dir_from_angle(std::atan2(ny, nx)));
    }
    if (dist2(L.x, L.y, L.tx, L.ty) < 56 * 56) { rescued(g); return; }
  }
  L.anim.loop(L.moving ? A_WALK : A_IDLE);

  // Los enemigos que lo tocan le hacen daño.
  if ((L.hit_cd -= dt) <= 0) {
    Enemy* es[kMaxEnemies];
    const int ne = g.query_circle(L.x, L.y, L.r, es, kMaxEnemies);
    float sum = 0;
    int hits = 0;
    for (int i = 0; i < ne; ++i) if (!es[i]->dead && es[i]->behavior != BH_THIEF) { sum += 4 + es[i]->dmg * 0.12f; hits++; }
    if (hits) {
      L.hit_cd = 0.5f;
      L.hp -= std::fmin(30.0f, sum);
      L.flash = 0.1f;
      g.fx.burst(L.x, L.y - 10, 0xff5f6d, 4, 60);
      if (L.hp <= 0) {
        char b[128];
        std::snprintf(b, sizeof(b), "¡%s se ha asustado y ha huido…!", mon_name(L.dex));
        gone(g, b);
        return;
      }
    }
  }
  if (L.flash > 0) L.flash -= dt;
  if (L.t > LOST_TIME) {
    char b[128];
    std::snprintf(b, sizeof(b), "%s se ha cansado de esperar y se ha ido…", mon_name(L.dex));
    gone(g, b);
  }
}
}  // namespace

void events_reset(Game& g) {
  g.events = Game::EventsState();
  g.events.next_t = g.rng.range(EVERY0, EVERY1);
  g.lost = Lost();
  g.events.thief_t = g.rng.range(THIEF_FIRST0, THIEF_FIRST1);
}

bool events_golden(const Game& g) { return g.events.cur == EVN_GOLD; }

void events_update(Game& g, float dt) {
  Game::EventsState& ev = g.events;
  if (ev.cur != EVN_NONE) {
    ev.cur_t += dt;
    if (ev.cur != EVN_LOST && ev.cur_t >= ev.cur_dur) end_event(g);
  }
  if (g.lost.on) lost_tick(g, dt);
  if (g.rift.arena || judgment_active(g)) return;

  if (ev.thieves < 2) {
    ev.thief_t -= dt;
    if (ev.thief_t <= 0) {
      if (g.boss_alive()) ev.thief_t = 8;
      else { spawn_thief(g); ev.thieves++; ev.thief_t = g.rng.range(THIEF_NEXT0, THIEF_NEXT1); }
    }
  }

  if (ev.cur != EVN_NONE) return;
  ev.next_t -= dt;
  if (ev.next_t > 0) return;
  if (g.boss_alive()) { ev.next_t = 10; return; }
  ev.next_t = g.rng.range(EVERY0, EVERY1);
  int ids[3], n = 0;
  for (int id : {EVN_HORDE, EVN_GOLD, EVN_LOST}) if (id != ev.last) ids[n++] = id;
  start(g, ids[int(g.rng.next() * n)]);
}

}  // namespace ps

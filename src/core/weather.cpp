// weather.cpp — climas (weather.js de la web). Desde el minuto 2, ~1 de cada
// 6 minutos llega un clima que dura alrededor de un minuto y cambia el daño
// según el tipo del ataque, para ti y para los enemigos.
#include <cmath>
#include <cstdio>

#include "core/game.h"

namespace ps {

namespace {
constexpr float FROM = 120, CHECK = 60, CHANCE = 0.18f;

bool hurts(const Game& g, const int8_t types[2]) {
  if (g.weather.kind < 0) return false;
  const WeatherDef& d = kWeather[g.weather.kind];
  if (!d.safe) return false;
  for (int i = 0; i < 2; ++i) if (types[i] >= 0 && ((d.safe >> types[i]) & 1u)) return false;
  return true;
}

void start(Game& g, int kind, float dur) {
  g.weather.kind = int8_t(kind); g.weather.t = 0; g.weather.dur = dur;
  const WeatherDef& d = kWeather[kind];
  char b[128];
  std::snprintf(b, sizeof(b), "%s: %s", d.name, d.hint);
  g.say(b, 3.4f);
  g.sfx(SFX_BIOME);
}

int pick(Game& g, int biome) {
  const float* w = kWeatherBiomeW[biome % 3];
  float total = 0;
  for (int k = 0; k < WX_COUNT; ++k) total += w[k];
  float r = g.rng.next() * total;
  // Mismo orden que las claves del objeto de la web para ese bioma.
  static const int8_t ORDER[3][WX_COUNT] = {
    {WX_RAIN, WX_FOG, WX_SUN, WX_SNOW, WX_SAND},
    {WX_FOG, WX_SNOW, WX_RAIN, WX_SAND, WX_SUN},
    {WX_SUN, WX_SAND, WX_FOG, WX_RAIN, WX_SNOW},
  };
  for (int i = 0; i < WX_COUNT; ++i) {
    const int k = ORDER[biome % 3][i];
    if (w[k] <= 0) continue;
    r -= w[k];
    if (r <= 0) return k;
  }
  return WX_RAIN;
}
}  // namespace

void weather_reset(Game& g) { g.weather = Game::WeatherState(); }

// Intensidad 0..1 (entra y sale en 3 s).
float weather_level(const Game& g) {
  if (g.weather.kind < 0) return 0;
  return std::fmin(1.0f, std::fmin(g.weather.t / 3, (g.weather.dur - g.weather.t) / 3));
}

float weather_mult(const Game& g, int type) {
  if (g.weather.kind < 0 || type < 0) return 1;
  const float m = kWeather[g.weather.kind].mult[type];
  return m != 1 ? 1 + (m - 1) * weather_level(g) : 1;
}

float weather_speed_for(const Game& g, const int8_t types[2]) {
  if (g.weather.kind < 0 || !kWeather[g.weather.kind].slow) return 1;
  if (types[0] == T_ICE || types[1] == T_ICE) return 1;
  return 1 - 0.1f * weather_level(g);
}

void weather_force(Game& g, int kind, float dur) { start(g, kind, dur); }
void weather_clear(Game& g) { g.weather.kind = WX_NONE; }

void weather_update(Game& g, float dt) {
  Game::WeatherState& w = g.weather;
  Player& pl = g.pl();
  if (w.kind >= 0) {
    w.t += dt;
    if (w.t >= w.dur) w.kind = WX_NONE;
  } else if (g.time > FROM && !g.rift.arena && !judgment_active(g)) {
    w.check_t -= dt;
    if (w.check_t <= 0) {
      w.check_t = CHECK;
      if (g.rng.next() < CHANCE) {
        const int kind = pick(g, g.world.biome());
        const float dur = float(jsround(g.rng.range(55, 75)));
        start(g, kind, dur);
      }
    }
  }
  // Arena y nieve: te van quitando un poco de vida (nunca te dejan KO).
  w.chip_t -= dt;
  if (w.chip_t <= 0) {
    w.chip_t = 2;
    for (int i = 0; i < g.nplayers; ++i) {   // (cada consola, al suyo: aquí, a todos)
      Player& q = g.players[i];
      if (!q.active || q.dead || !hurts(g, q.mon->types) || weather_level(g) <= 0.5f) continue;
      const float d = std::fmax(1.0f, q.max_hp * 0.012f);
      if (q.hp > d + 1) {
        q.hp -= d;
        char b[16];
        std::snprintf(b, sizeof(b), "-%d", int(jsround(d)));
        g.fx.dmg_text(q.x, q.y - q.body_h - 4, b, kWeather[w.kind].color, false, true);
      }
    }
    (void)pl;
    if (w.kind >= 0 && kWeather[w.kind].safe && weather_level(g) > 0.5f) {
      for (Enemy& e : g.enemies) {
        if (e.dead || e.boss) continue;
        if (!hurts(g, e.types)) continue;
        e.hp -= e.max_hp * 0.02f + 1;
        e.flash = 0.05f;
        if (e.hp <= 0) enemy_die(g, e);
      }
    }
  }
}

}  // namespace ps

// judgment.cpp — La Sentencia de Arceus, minuto 30 (judgment.js de la web).
//   1. Animación: oscuro, "LA SENTENCIA DE ARCEUS", destello y viaje a una arena
//      enorme. Arceus baja por un pilar de luz. La primera vez se ve entera.
//   2. Pelea: Arceus es INVENCIBLE. Te persigue cada vez más deprisa; tocarle
//      quita el 40 % de la vida, y lanza pilares, rayos en cruz y ondas.
//      Desde el 23-10-2026 (web) se le puede pegar: lleva una Tabla al azar
//      (su tipo), sólo recibe una parte (judgment_dmg_k) y los ataques le
//      apuntan cuando no queda ningún legendario. Lo que le hacéis sale al final.
//   3. Cada 5 s acude un legendario, cada uno con un 25 % más de vida y un 15 %
//      más de daño que el anterior (antes el doble: no había forma de tumbarlos).
//      El primero, según el daño que hacías a UN enemigo.
//   (Del 23-10 no se porta, por ahora: el minuto 35 que rompe los objetos.)
//   5. Modo rápido (del port, 2026-10-29): acuden sólo kQuickLegends legendarios.
//      Tumbados todos, Arceus BAJA LA GUARDIA: tiene vida de verdad (ARCEUS_HP veces
//      la del primer legendario), recibe el daño entero y se enfurece (ataca más a
//      menudo y corre más). Si cae, la partida acaba en victoria (medalla y premio).
//      En el modo normal todo sigue como en la web: invencible.
#include <cmath>
#include <initializer_list>
#include <cstdio>

#include "core/halloween.h"
#include "core/save.h"
#include "core/game.h"

namespace ps {

namespace {
constexpr float ARENA_R = 24;
constexpr float BOSS_EVERY = 5;
constexpr float TOUCH = 0.4f;
// Contra la huida sin fin (18-10-2026): velocidad con tope suave (game.h,
// judgment_cap_spd), legendarios al menos al 80 % de vuestra velocidad, Arceus
// acelera sin tope desde los 5 min de Sentencia y desde los 7 su toque quita
// cada vez más, hasta el 100 % a los 10.
float touch_pct(float arena_t) { return std::fmin(1.0f, TOUCH + std::fmax(0.0f, arena_t - 420) / 180 * (1 - TOUCH)); }
constexpr int ARCEUS = 493;
constexpr float SCALE = 1.5f;
// Halloween: el gran Gengar bruja (su disfraz, kSkins «bruja-gengar»), el doble de grande.
constexpr int GENGAR = 94;
constexpr float GENGAR_SCALE = 3;
int boss_dex(const Game& g) { return g.judg.gengar ? GENGAR : ARCEUS; }
float boss_scale(const Game& g) { return g.judg.gengar ? GENGAR_SCALE : SCALE; }
int boss_sprite(const Game& g) {
  if (!g.judg.gengar) return ARCEUS;
  for (int k = 0; k < kSkinCount; ++k) if (kSkins[k].dex == GENGAR) return kSkins[k].sprite;
  return GENGAR;
}
constexpr float T_TITLE = 1.3f, T_FLASH = 4.7f, T_PORT = 5.1f, T_DESC = 5.7f, T_LAND = 7.9f, T_BACK = 9.1f, T_END = 10.1f;
constexpr uint32_t GOLD = 0xffd25e;
constexpr float TS = kTS;
constexpr float HP_GROW = 1.25f, DMG_GROW = 1.15f;   // cada legendario, respecto al anterior
// Modo rápido: vida de Arceus con la guardia baja (× la del primer legendario) y
// cuánto se enfurece. Ajustado con partidas del bot (tests/test_judgment.cpp).
constexpr float ARCEUS_HP = 40;
constexpr float RAGE_CD = 0.65f, RAGE_SPD = 1.15f;
enum { JC_RAIN, JC_CROSS, JC_NOVA };

void sample_dps(Game& g, float dt) {
  Game::JudgmentState& j = g.judg;
  j.hit_t += dt;
  if (j.hit_t < 1) return;
  float best = 0;
  for (int i = 0; i < j.nhit; ++i) if (j.hit_v[i] > best) best = j.hit_v[i];
  if (j.nsamples >= 30) { for (int i = 0; i < 29; ++i) j.samples[i] = j.samples[i + 1]; j.nsamples = 29; }
  j.samples[j.nsamples++] = best / j.hit_t;
  j.nhit = 0; j.hit_t = 0;
}

float single_dps(const Game& g) {
  const Game::JudgmentState& j = g.judg;
  float s[30];
  int n = j.nsamples;
  for (int i = 0; i < n; ++i) s[i] = j.samples[i];
  for (int a = 1; a < n; ++a) { const float v = s[a]; int b = a - 1; while (b >= 0 && s[b] < v) { s[b + 1] = s[b]; --b; } s[b + 1] = v; }
  if (n > 6) n = 6;
  if (!n) return 0;
  float sum = 0;
  for (int i = 0; i < n; ++i) sum += s[i];
  return sum / n;
}

void begin(Game& g) {
  Game::JudgmentState& j = g.judg;
  if (j.phase) return;
  j.phase = 1; j.ct = 0;
  // La primera vez se ve entera.
  j.can_skip = g.cfg.judged > 0;
  g.music = MU_NONE;
}

void start(Game& g) {
  Game::JudgmentState& j = g.judg;
  static int16_t pool[128];
  int np = 0;
  for (int i = 0; i < kDexCount; ++i) if (kDex[i].leg && kDex[i].dex != ARCEUS && np < 128) pool[np++] = kDex[i].dex;
  j.nq = 40;
  for (int i = 0; i < 40; ++i) j.q[i] = pool[int(g.rng.next() * np)];
  j.tx = -6000 - int32_t(std::floor(g.rng.next() * 2000));
  j.ty = -6000 - int32_t(std::floor(g.rng.next() * 2000));
  // Su Tabla: uno de los 17 tipos (todos menos Normal), al azar. (El gran Gengar: Fantasma.)
  int types[T_COUNT], nt = 0;
  for (int t = 0; t < T_COUNT; ++t) if (t != T_NORMAL) types[nt++] = t;
  j.plate = int8_t(types[int(g.rng.next() * nt)]);
  j.gengar = g.hw;
  if (j.gengar) j.plate = T_GHOST;
  begin(g);
}

// Viaje a la arena (a mitad de la animación, tapado por el destello).
void port(Game& g) {
  Game::JudgmentState& j = g.judg;
  j.ported = true;
  rift_reset(g);
  // Los del mapa se guardan (como en una grieta); no vuelven.
  g.stashed.clear();
  for (const Enemy& e : g.enemies) if (!e.dead && !g.stashed.full()) *g.stashed.push() = e;
  g.enemies.clear(); g.corpses.clear();
  g.projectiles.clear();
  combat_clear_fields(g);
  g.hazards.clear();
  // Los caramelos que quedaban en el suelo no se pierden con el viaje: se dan.
  int candy = 0;
  for (const Pickup& o : g.pickups) if (o.kind == PK_CANDY) candy += int(o.value);
  if (candy) {
    g.pl().candy += candy;
    if (g.save) { g.save->candy += candy; g.save->candy_known = true; g.dirty = true; }
  }
  g.pickups.clear();
  events_reset(g);
  weather_clear(g);
  const int th = arena_theme_index("judgment");
  g.world.set_arena(true, j.tx, j.ty, ARENA_R, th);
  g.world.set_biome(kArenaBiome0 + th, true);
  j.cx = j.tx * TS + TS / 2; j.cy = j.ty * TS + TS / 2;
  for (int i = 0; i < g.nplayers; ++i) {
    Player& p = g.players[i];
    p.x = j.cx + (i - (g.nplayers - 1) / 2.0f) * 34; p.y = j.cy + TS * 3;
  }
  g.views_snap();
  j.a_on = true;
  j.ax = j.cx; j.ay = j.cy - TS * 5; j.lift = 900;
  j.aanim.init(boss_sprite(g), boss_scale(g));
  j.ar = body_radius(boss_dex(g), boss_scale(g)) * 0.9f;
  j.abody_h = body_height(boss_dex(g), boss_scale(g));
  j.acd = 2.2f; j.acast = -1; j.touch_cd = 0; j.spin = 0;
  j.aanim.dir = 0;
}

void land(Game& g, bool quiet) {
  Game::JudgmentState& j = g.judg;
  if (j.landed || !j.a_on) return;
  j.landed = true;
  j.lift = 0;
  j.aanim.play(A_ATTACK, 0.8f);
  if (quiet) return;
  g.camera.kick(1.2f);
  g.flash(0xfff3c8, 0.7f);
  g.sfx(SFX_LAND);
  for (int i = 0; i < 4; ++i) g.fx.ring(j.ax, j.ay, 10 + i * 20.0f, 260 + i * 140.0f, i % 2 ? 0xfff3c8 : GOLD, 0.5f + i * 0.25f, 8.0f - i);
  g.fx.burst(j.ax, j.ay - 20, GOLD, 60, 360);
  g.fx.burst(j.ax, j.ay - 10, 0xffffff, 30, 240);
}

void fight(Game& g) {
  Game::JudgmentState& j = g.judg;
  j.phase = 2;
  if (!j.ported) port(g);
  land(g, true);
  j.arena_t = 0; j.boss_t = 1.2f; j.boss_n = 0; j.downs = 0;
  progress_event(g, EV_JUDGE);
  // Vida del primer legendario: unos 3 s de vuestro daño a un enemigo (y un mínimo).
  const float t = float(g.time);
  const float floor_ = (3200 + t * 9) * 0.35f * coop_scale_hp(g.team_size());
  j.base = std::fmax(floor_, single_dps(g) * 3);
  j.dealt = 0;
  j.can_win = g.cfg.mode == MODE_QUICK;
  j.open = j.won = false; j.won_t = 0;
  char b[160];
  if (j.gengar && j.can_win)
    std::snprintf(b, sizeof(b), "¡El gran Gengar te ha arrastrado a su Noche Espectral! Tumba a sus %d legendarios y bajará la guardia: ¡aquí se le puede derrotar!", kQuickLegends);
  else if (j.gengar) std::snprintf(b, sizeof(b), "¡El gran Gengar te ha arrastrado a su Noche Espectral!");
  else if (j.can_win)
    std::snprintf(b, sizeof(b), "¡Arceus lleva la %s (tipo %s)! Tumba a sus %d legendarios y bajará la guardia: ¡aquí se le puede derrotar!",
                  plate_name(j.plate), kTypeName[j.plate], kQuickLegends);
  else std::snprintf(b, sizeof(b), "¡Arceus lleva la %s! Ahora es de tipo %s", plate_name(j.plate), kTypeName[j.plate]);
  g.say(b, j.can_win ? 6 : 4);
  // Halloween: llegar hasta aquí da un buen puñado de caramelos (en grupo, a todos).
  if (j.gengar) {
    g.pl().candy += kCandyJudge;
    if (g.save) { g.save->candy += kCandyJudge; g.save->candy_known = true; g.dirty = true; }
    // Que se vea (antes se sumaban sin decir nada).
    const Player& me = g.me();
    char c[32];
    std::snprintf(c, sizeof(c), "+%d caramelos", kCandyJudge);
    g.fx.dmg_text(me.x, me.y - me.body_h - 8, c, 0xff8a2a, true);
    g.fx.ring(me.x, me.y, 6, 60, 0xff8a2a, 0.5f, 3);
  }
  // El blanco de Arceus: un enemigo invisible que va siempre donde está él.
  // Así los ataques le dan como a cualquiera. No se mueve solo, no hace daño al
  // tocarle y no cae nunca (enemy.cpp).
  EnemyDef def;
  def.dex = int16_t(boss_dex(g)); def.hp = 1e9f; def.spd = 0; def.dmg = 0; def.xp = 0; def.behavior = BH_CHASE;
  if (Enemy* e = enemy_spawn(g, def, j.ax, j.ay, Scale3(), false)) {
    e->arceus = true; e->judge = true;
    e->types[0] = j.plate; e->types[1] = T_NONE; e->atk_type = j.plate;
    e->mon = mon_by_dex(boss_dex(g));
    e->r = body_radius(boss_dex(g), boss_scale(g)) * 0.9f;
    e->body_h = body_height(boss_dex(g), boss_scale(g));
    e->flying = true;
  }
}

Player* target(Game& g) {
  Game::JudgmentState& j = g.judg;
  Player* best = nullptr;
  float bd = 1e30f;
  for (int i = 0; i < g.nplayers; ++i) {
    Player& p = g.players[i];
    if (!p.active || p.dead) continue;
    const float d = dist2(p.x, p.y, j.ax, j.ay);
    if (d < bd) { bd = d; best = &p; }
  }
  return best;
}

Hazard* H(Game& g) {
  Hazard* h = hazard_add(g);
  if (!h) return nullptr;
  h->type = T_NONE; h->pure = true; h->dmg = 1; h->color = GOLD;
  return h;
}

void cast(Game& g, int id, const Player& tg) {
  Game::JudgmentState& j = g.judg;
  j.alast = int8_t(id);
  j.acast = int8_t(id); j.act = 0; j.awind = 0.7f; j.adone = false;
  if (id == JC_RAIN) {
    // La Sentencia: pilares de luz sobre cada uno (más cuanto más dura).
    const int n = 4 + std::min(6, int(std::floor(j.arena_t / 10)));
    for (int k = 0; k < g.nplayers; ++k) {
      const Player& p = g.players[k];
      if (!p.active || p.dead) continue;
      for (int i = 0; i < n; ++i) {
        const float a = g.rng.next() * kTau, d = i ? g.rng.range(40, 150) : 0;
        Hazard* h = H(g);
        if (!h) break;
        h->beam = false; h->x = p.x + std::cos(a) * d; h->y = p.y + std::sin(a) * d * 0.8f; h->r = 44;
        h->delay = 1.25f + i * 0.09f; h->pct = 0.3f; h->pillar = true;
      }
    }
    j.aanim.play(A_SHOOT, 0.9f);
  } else if (id == JC_CROSS) {
    // Rayos en cruz que barren la arena.
    const float a0 = std::atan2(tg.y - j.ay, tg.x - j.ax) + (g.rng.next() < 0.5f ? 0 : kPi / 4);
    const int n = j.arena_t > 30 ? 8 : 4;
    for (int i = 0; i < n; ++i) {
      Hazard* h = H(g);
      if (!h) break;
      h->beam = true; h->x = j.ax; h->y = j.ay; h->angle = a0 + i * kTau / n; h->len = 1700; h->w = 34;
      h->delay = 1.1f; h->pct = 0.35f; h->color = 0xfff3b0;
    }
    j.awind = 1.1f;
    j.aanim.play(A_CHARGE, 0.9f);
  } else {
    // Onda alrededor de él (si estás demasiado cerca).
    Hazard* h = H(g);
    if (h) { h->beam = false; h->x = j.ax; h->y = j.ay; h->r = 170; h->delay = 0.85f; h->pct = 0.4f; }
    j.awind = 0.85f;
    j.aanim.play(A_CHARGE, 1.2f);
  }
  g.fx.ring(j.ax, j.ay, j.ar, j.ar * 3, GOLD, j.awind, 3);
}

void cast_tick(Game& g, float dt, Player* tg) {
  Game::JudgmentState& j = g.judg;
  j.act += dt;
  if (tg) j.aanim.dir = int8_t(dir_from_angle(std::atan2(tg->y - j.ay, tg->x - j.ax)));
  if (j.act >= j.awind && !j.adone) {
    j.adone = true;
    j.aanim.play(j.acast == JC_NOVA ? A_ATTACK : A_SHOOT, 1.2f);
  }
  if (j.act >= j.awind + 0.35f) {
    j.acast = -1;
    j.acd = std::fmax(1.4f, 3.2f - j.arena_t * 0.025f) * (j.open ? RAGE_CD : 1.0f);
  }
}

void arceus_tick(Game& g, float dt) {
  Game::JudgmentState& j = g.judg;
  Player* tg = target(g);
  if (j.acast >= 0) { cast_tick(g, dt, tg); return; }
  if (!tg) { j.aanim.loop(A_IDLE); return; }
  // Más lento que tú al principio; a los ~35 s ya te alcanza.
  const float k = std::fmin(1.22f, 0.74f + j.arena_t * 0.013f) + std::fmax(0.0f, j.arena_t - 300) * 0.0015f;
  const float spd = std::fmax(70.0f, player_spd(g, *tg) * k) * (j.open ? RAGE_SPD : 1.0f);
  float dx, dy;
  norm(tg->x - j.ax, tg->y - j.ay, dx, dy);
  j.ax += dx * spd * dt; j.ay += dy * spd * dt;
  j.aanim.dir = int8_t(dir_from_angle(std::atan2(dy, dx)));
  j.aanim.loop(A_WALK, std::fmax(0.8f, spd / 90));
  if (g.vrng.next() < dt * 14) {
    PxOpts o; o.vy = -30; o.life = 0.6f;
    g.fx.px(j.ax + g.vrng.range(-20, 20), j.ay - g.vrng.range(0, j.abody_h), GOLD, o);
  }
  j.acd -= dt;
  if (j.acd <= 0) {
    const float d = dist(j.ax, j.ay, tg->x, tg->y);
    const int id = d < 210 && g.rng.next() < 0.6f ? JC_NOVA : (j.alast == JC_RAIN ? JC_CROSS : JC_RAIN);
    cast(g, id, *tg);
  }
}

void touch(Game& g, Player& pl) {
  Game::JudgmentState& j = g.judg;
  if (!pl.active || pl.dead || j.touch_cd > 0) return;
  const float rr = j.ar + pl.r;
  if (dist2(j.ax, j.ay, pl.x, pl.y) > rr * rr) return;
  j.touch_cd = 0.9f;
  pl.invuln = 0;
  player_hurt(g, pl, pl.max_hp * touch_pct(j.arena_t), T_NONE, true);
  g.camera.kick(0.6f);
  g.fx.ring(pl.x, pl.y, 6, 70, GOLD, 0.4f, 4);
  j.aanim.play(A_ATTACK, 1.4f);
}

void keep_inside(Game& g) {
  Game::JudgmentState& j = g.judg;
  const float max = (ARENA_R - 1.4f) * TS;
  auto clamp_in = [&](float& x, float& y) {
    const float dx = x - j.cx, dy = y - j.cy, d = std::hypot(dx, dy);
    if (d > max) { x = j.cx + dx / d * max; y = j.cy + dy / d * max; }
  };
  clamp_in(j.ax, j.ay);
  for (Enemy& e : g.enemies) clamp_in(e.x, e.y);
  // Y los jugadores (web, 25-10-2026: yendo muy rápido alguien se salía de la
  // arena y ya no se le podía alcanzar: aguantaba para siempre).
  for (int i = 0; i < g.nplayers; ++i) if (g.players[i].active) clamp_in(g.players[i].x, g.players[i].y);
}

bool is_one_of(const Mon& m, std::initializer_list<int> ts) {
  for (int t : ts) if (m.types[0] == t || m.types[1] == t) return true;
  return false;
}

void spawn_boss(Game& g) {
  Game::JudgmentState& j = g.judg;
  if (j.can_win && j.boss_n >= kQuickLegends) return;   // (modo rápido: sólo esos)
  const int dex = j.q[j.boss_n % j.nq];
  const Mon* mon = mon_by_dex(dex);
  if (!mon) return;
  const float t = float(g.time);
  // Lejos de todos (en el borde de la arena).
  float x = j.cx, y = j.cy;
  for (int i = 0; i < 16; ++i) {
    const float a = g.rng.next() * kTau, d = (ARENA_R - g.rng.range(3, 6)) * TS;
    x = j.cx + std::cos(a) * d; y = j.cy + std::sin(a) * d;
    bool far = true;
    for (int k = 0; k < g.nplayers; ++k) if (g.players[k].active && dist2(g.players[k].x, g.players[k].y, x, y) <= 280 * 280) far = false;
    if (far) break;
  }
  // Cada uno, un 25 % más de vida y un 15 % más de daño que el anterior (tope
  // de seguridad para el float: en una partida nadie llega ni de lejos).
  const float k = std::pow(HP_GROW, float(std::min(j.boss_n, 300)));
  const float kd = std::pow(DMG_GROW, float(std::min(j.boss_n, 300)));
  EnemyDef def;
  def.dex = int16_t(dex);
  def.hp = float(jsround(j.base * k));
  float fast = 0;
  for (int q = 0; q < g.nplayers; ++q) if (g.players[q].active && !g.players[q].dead) fast = std::fmax(fast, player_spd(g, g.players[q]));
  def.spd = float(jsround(std::fmax(clamp(48 + (mon->spd - 78) * 0.6f, 48.0f, 105.0f), fast * 0.8f)));
  def.dmg = float(jsround((36 + t / 20) * kd));
  def.xp = float(jsround(500 + t * 1.2f));
  def.behavior = is_one_of(*mon, {T_PSYCHIC, T_ELECTRIC, T_GHOST, T_FAIRY, T_FIRE, T_WATER, T_ICE, T_DRAGON}) ? BH_RANGED : BH_CHARGER;
  def.shot_dmg = float(jsround((18 + t / 40) * kd)); def.shot_cd = 0.9f; def.range = 300;
  Enemy* e = enemy_spawn(g, def, x, y, Scale3(), true);
  if (!e) return;
  e->judge = true;
  e->flying = false;
  j.boss_n++;
  g.fx.ring(x, y, 8, 120, GOLD, 0.6f, 5);
  g.fx.burst(x, y - 30, 0xfff3c8, 24, 200);
  char b[96];
  std::snprintf(b, sizeof(b), "¡%s acude a la Sentencia!", mon->name);
  g.say(b, 1.8f);
  g.sfx(SFX_RIFT);
}
}  // namespace

void judgment_reset(Game& g) { g.judg = Game::JudgmentState(); }

// Daño a Arceus: recibe el 5 % de lo que le hacéis, y cada vez menos (el 1 % a
// los 10 min de Sentencia), como en la web.
float judgment_dmg_k(const Game& g) { return g.judg.open ? 1.0f : 0.05f / (1 + g.judg.arena_t / 150); }
void judgment_hit_arceus(Game& g, float d) {
  Game::JudgmentState& j = g.judg;
  if (j.phase != 2 || j.won) return;
  j.dealt += d;
  if (!j.open) return;
  j.ahp -= d;
  if (j.ahp > 0) return;
  // ¡Derrotado! Se para todo y, en unos segundos, la partida acaba en victoria.
  j.ahp = 0; j.won = true; j.won_t = 0;
  g.pending_levels = 0;   // (que ninguna carta de nivel tape la victoria)
  j.acast = -1;
  j.aanim.play(A_FAINT, 0.6f);
  g.hazards.clear();
  g.projectiles.clear();
  for (int i = g.enemies.n - 1; i >= 0; --i) if (!g.enemies[i].arceus) g.enemies.remove_ordered(i);
  g.flash(0xffffff, 1.0f);
  g.camera.kick(1.2f);
  for (int i = 0; i < 5; ++i) g.fx.ring(j.ax, j.ay, 10 + i * 24.0f, 300 + i * 160.0f, i % 2 ? 0xfff3c8 : GOLD, 0.6f + i * 0.3f, 8.0f - i);
  g.fx.burst(j.ax, j.ay - 30, GOLD, 80, 420);
  g.fx.burst(j.ax, j.ay - 20, 0xffffff, 40, 300);
  g.sfx(SFX_LEGEND);
  g.say(j.gengar ? "¡HAS DERROTADO AL GRAN GENGAR!" : "¡HAS DERROTADO A ARCEUS!", 5);
}

const char* plate_name(int type) {
  static const char* const N[T_COUNT] = {"Tabla Normal", "Tabla Llama", "Tabla Linfa", "Tabla Pradal", "Tabla Trueno", "Tabla Helada",
                                          "Tabla Fuerte", "Tabla Tóxica", "Tabla Terrax", "Tabla Cielo", "Tabla Mental", "Tabla Bicho",
                                          "Tabla Pétrea", "Tabla Terror", "Tabla Draco", "Tabla Oscura", "Tabla Acero", "Tabla Duende"};
  return type >= 0 && type < T_COUNT ? N[type] : "Tabla";
}
bool judgment_active(const Game& g) { return g.judg.phase != 0; }
bool judgment_cinematic(const Game& g) { return g.judg.phase == 1; }

// Daño hecho a un enemigo (para la vida del primer legendario).
void judgment_note_dmg(Game& g, float d, const Enemy& e) {
  Game::JudgmentState& j = g.judg;
  if (j.phase) return;
  for (int i = 0; i < j.nhit; ++i) if (j.hit_id[i] == e.id) { j.hit_v[i] += d; return; }
  if (j.nhit < 64) { j.hit_id[j.nhit] = e.id; j.hit_v[j.nhit] = d; j.nhit++; }
}

void Game::skip_judgment() {
  Game::JudgmentState& j = judg;
  if (j.phase != 1 || !j.can_skip || j.ct >= T_BACK) return;
  if (!j.ported) port(*this);
  land(*this, true);
  j.ct = T_BACK;
}

// Durante la animación (el resto del juego está parado).
void judgment_cine_tick(Game& g, float dt) {
  Game::JudgmentState& j = g.judg;
  const float prev = j.ct;
  j.ct += dt;
  auto at = [&](float x) { return prev < x && j.ct >= x; };
  Player& pl = g.pl();
  if (at(0.01f)) { g.sfx(SFX_QUAKE); g.camera.kick(0.9f); }
  if (j.ct < T_FLASH && g.vrng.next() < dt * 3) g.camera.kick(0.15f);
  if (at(T_TITLE)) g.sfx(SFX_TOLL);
  if (at(T_TITLE + 1.7f)) g.sfx(SFX_TOLL);
  if (at(T_FLASH)) g.flash(0xffffff, 0.9f);
  if (at(T_PORT) && !j.ported) port(g);
  if (at(T_DESC)) g.sfx(SFX_DESCEND);
  if (j.a_on && !j.landed) {
    // Bajada: lenta al principio y frenando al final (como si se posara).
    const float k = clamp((j.ct - T_DESC) / (T_LAND - T_DESC), 0.0f, 1.0f);
    j.lift = 900 * std::pow(1 - k, 2.2f);
    j.aanim.loop(A_IDLE);
    if (j.ct > T_DESC && g.vrng.next() < dt * 30) {
      PxOpts o; o.vy = g.vrng.range(60, 160); o.life = 0.9f;
      g.fx.px(j.ax + g.vrng.range(-60, 60), j.ay - j.lift - g.vrng.range(0, 200), g.vrng.next() < 0.5f ? GOLD : 0xfff3c8, o);
    }
    if (at(T_LAND)) land(g, false);
  }
  if (j.a_on) { j.aanim.update(dt); j.spin += dt; }
  // Cámara: tu Pokémon; sube despacio hacia Arceus mientras baja y vuelve.
  float tx = pl.x, ty = pl.y;
  if (j.ported && j.a_on && j.ct < T_BACK) { tx = j.ax; ty = j.ay - 60; }
  if (!j.cam_on || at(T_PORT) || (j.ported && !j.cam_ported)) { j.cam_on = true; j.cam_x = g.pl().view_x; j.cam_y = g.pl().view_y; j.cam_ported = j.ported; }
  j.cam_x = damp(j.cam_x, tx, 2.2f, dt); j.cam_y = damp(j.cam_y, ty, 2.2f, dt);
  g.views_follow_point(j.cam_x, j.cam_y, dt);
  if (j.ct >= T_END) { j.cam_on = false; fight(g); }
}

void judgment_update(Game& g, float dt) {
  Game::JudgmentState& j = g.judg;
  if (!j.phase) {
    sample_dps(g, dt);                        // (para la vida del primer legendario)
    if (g.time >= judgment_at(g) && !g.rift.arena) start(g);
    return;
  }
  if (j.phase != 2) return;
  if (j.won) {   // (victoria: Arceus cae; a los 4 s, el resumen)
    j.won_t += dt;
    j.aanim.update(dt);
    if (j.won_t >= 4) { extern void game_end_run(Game & g, bool dead); g.judg.won = true; game_end_run(g, false); }
    return;
  }
  j.arena_t += dt;
  j.touch_cd -= dt; j.spin += dt;
  j.shielded = false;
  for (const Enemy& e : g.enemies) if (e.boss && !e.dead && !e.arceus) { j.shielded = true; break; }
  // Modo rápido: tumbados todos sus legendarios, Arceus baja la guardia.
  if (j.can_win && !j.open && j.boss_n >= kQuickLegends && !j.shielded) {
    j.open = true;
    j.ahp_max = j.ahp = j.base * ARCEUS_HP;
    g.flash(0xfff3c8, 0.6f);
    g.camera.kick(0.8f);
    for (int i = 0; i < 3; ++i) g.fx.ring(j.ax, j.ay, j.ar, 300 + i * 200.0f, i % 2 ? 0xfff3c8 : GOLD, 0.7f + i * 0.2f, 6.0f - i);
    g.sfx(SFX_TOLL);
    g.say(j.gengar ? "¡El gran Gengar baja la guardia! Ahora sí le haces daño… y está furioso"
                   : "¡Arceus baja la guardia! Ahora sí le haces daño… y está furioso", 4);
  }
  // (pruebas: debug_win, para llegar a la victoria en un momento; dentro del paso, así
  // en grupo todas las consolas hacen lo mismo)
  if (g.debug_win) {
    for (Enemy& e : g.enemies) if (e.boss && !e.arceus && e.hp > 50) e.hp = 50;
    if (j.open && j.ahp > j.ahp_max * 0.12f) j.ahp = j.ahp_max * 0.12f;
  }
  arceus_tick(g, dt);
  keep_inside(g);
  j.boss_t -= dt;
  if (j.boss_t <= 0) { j.boss_t = BOSS_EVERY; spawn_boss(g); }
  j.aanim.update(dt);
  for (int i = 0; i < g.nplayers; ++i) touch(g, g.players[i]);
}

}  // namespace ps

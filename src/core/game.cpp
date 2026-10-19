// game.cpp — flujo de la partida y paso principal (game.js de la web).
//   estados: ui (menús), playing, levelup, paused, choice (objeto o vínculo), over
#include "core/game.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/save.h"
#include "core/upgrades.h"

namespace ps {

Game::Game() {
  world.init(this);
  fx.vr = &vrng;
  for (auto& h : grid_head) h = -1;
}

void Game::say(const char* text, float secs) {
  std::snprintf(banner, sizeof(banner), "%s", text);
  banner_t = secs;
}

void Game::sfx(int id, int kind, int type) {
  SfxEvent* e = sfx_queue.push();
  if (e) { e->id = int8_t(id); e->kind = int8_t(kind); e->type = int8_t(type); }
}

int Game::everyone(Player** out) {
  int n = 0;
  for (int i = 0; i < nplayers; ++i) if (players[i].active) out[n++] = &players[i];
  return n;
}

void Game::views_follow(float dt) {
  for (int i = 0; i < nplayers; ++i) {
    Player& p = players[i];
    if (!p.active) continue;
    p.view_x = damp(p.view_x, p.x, 9, dt);
    p.view_y = damp(p.view_y, p.y, 9, dt);
  }
  camera.x = players[local].view_x; camera.y = players[local].view_y;
  camera.update_shake(dt, vrng);
}

void Game::views_follow_point(float x, float y, float dt) {
  for (int i = 0; i < nplayers; ++i) {
    Player& p = players[i];
    p.view_x = damp(p.view_x, x, 9, dt);
    p.view_y = damp(p.view_y, y, 9, dt);
  }
  camera.x = players[local].view_x; camera.y = players[local].view_y;
  camera.update_shake(dt, vrng);
}

void Game::views_set(float x, float y) {
  for (int i = 0; i < nplayers; ++i) { players[i].view_x = x; players[i].view_y = y; }
  camera.x = x; camera.y = y;
}

void Game::views_snap() {
  for (int i = 0; i < nplayers; ++i) { players[i].view_x = players[i].x; players[i].view_y = players[i].y; }
  camera.x = players[local].view_x; camera.y = players[local].view_y;
}

bool Game::seen_by_any(float x, float y, float pad) const {
  for (int i = 0; i < nplayers; ++i)
    if (players[i].active && camera.sees_from(players[i].view_x, players[i].view_y, x, y, pad)) return true;
  return false;
}

float Game::level_xp() {
  const Player& p = pl();
  if (!p.active) return 20;
  const float k = 1 * (p.xp_mul > 0 ? p.xp_mul : 1);
  return std::ceil((p.xp_next - p.xp + 1) / k);
}

// ---------------- partida ----------------

void Game::start_run(const RunConfig& c, Save* s) {
  cfg = c;
  save = s;
  rng.reseed(c.seed);
  vrng.reseed(c.seed ^ 0x5bd1e995u);
  fx.clear();
  projectiles.clear();
  fields.clear();
  enemies.clear();
  corpses.clear();
  pickups.clear();
  hazards.clear();
  stashed.clear();
  has_stash = false;
  next_enemy_id = next_proj_id = next_pickup_id = 1;
  spawner_reset(*this);
  rift_reset(*this);
  judgment_reset(*this);
  events_reset(*this);
  weather_reset(*this);
  path_clear(*this);
  world.reset(int32_t(rng.next() * 1e6f));
  world.set_biome(0, true);
  biome = 0;
  time = 0; pending_levels = 0;
  death_t = 0; flash_t = 0;
  banner[0] = 0; banner_t = 0;
  sfx_queue.clear();
  ncaught = 0; nlater = 0;
  choice = CH_NONE;
  over_ready = false;
  heal_shown = heal_acc = 0;

  nplayers = 1;
  for (auto& p : players) p.active = false;
  Player& p = pl();
  player_init(*this, p, c.dex, c.shiny, c);
  // Atajos: movimientos con hueco fijo, antes de aprender nada.
  for (int i = 0; i < 4; ++i) if (c.pbind[i] >= 0 && c.pbind[i] < 64) p.bind[c.pbind[i]] = int8_t(i);
  player_add_move(*this, p, move_start_for(c.dex));
  // Mejora "Veto": movimientos que no quieres que te salgan.
  const Perks perks = upgrade_perks(c.upgrades);
  for (int i = 0; i < 3 && i < perks.bans; ++i) if (c.pban[i] >= 0) p.banned |= 1ull << c.pban[i];
  // Mejora "Repertorio": un segundo movimiento de ataque al azar.
  if (perks.start_moves >= 2) {
    int16_t all[64], pool[64];
    const int na = move_pool_for(p.mon->types, all, 64);
    int np = 0;
    for (int i = 0; i < na; ++i) {
      const int id = all[i];
      if (player_has_move(p, id) || ((p.banned >> id) & 1ull) || kMoves[id].kind == K_BUFF) continue;
      pool[np++] = int16_t(id);
    }
    if (np) player_add_move(*this, p, pool[int(rng.next() * np)]);
    player_set_active(*this, p, 0);
  }
  views_snap(); camera.shake = 0;
  music = MU_FOREST;
  state = ST_PLAYING;
}

void Game::quit_run() {
  if (state == ST_UI || state == ST_OVER) return;
  // endRun(false)
  extern void game_end_run(Game & g, bool dead);
  game_end_run(*this, false);
}

void Game::toggle_pause() {
  if (state == ST_PLAYING) { state = ST_PAUSED; sfx(SFX_PAUSE); }
  else if (state == ST_PAUSED) state = ST_PLAYING;
}

// Un shiny salvaje cae: te lo quedas (y la versión normal si no la tenías).
void Game::on_shiny_caught(int dex) {
  Player& p = pl();
  const bool is_new = true;   // (sin guardado: se trata como nuevo; ver progress/save)
  extern bool save_grant_shiny(Game & g, int dex);
  const bool fresh = save ? save_grant_shiny(*this, dex) : is_new;
  if (ncaught < 16) { caught_dex[ncaught] = int16_t(dex); caught_new[ncaught] = fresh; ncaught++; }
  char b[96];
  const Mon* m = mon_by_dex(dex);
  if (fresh) std::snprintf(b, sizeof(b), "¡Has conseguido a %s shiny!", m ? m->name : "");
  else std::snprintf(b, sizeof(b), "¡Otro %s shiny! (+300 monedas)", m ? m->name : "");
  say(b, 4);
  if (!fresh) p.coins += 300;
  sfx(SFX_SHINYGET);
}

// Has llevado a casa a un Pokémon perdido y se une a ti.
void Game::on_rescue(int dex) {
  Player& p = pl();
  extern bool save_grant(Game & g, int dex);
  const bool fresh = save ? save_grant(*this, dex) : true;
  if (!fresh) p.coins += 150;
  const Mon* m = mon_by_dex(dex);
  char b[96];
  if (fresh) std::snprintf(b, sizeof(b), "¡%s se une a tu equipo!", m ? m->name : "");
  else std::snprintf(b, sizeof(b), "¡%s te da las gracias! (+150 monedas, ya lo tenías)", m ? m->name : "");
  say(b, 4);
  sfx(SFX_SHINYGET);
}

// Lo que pasa al romperse un objeto del escenario (destroyProp de world.js).
void Game::on_prop_destroyed(const Prop& p) {
  const Biome& B = kBiomes[world.biome()];
  sfx(p.type == PR_GRASS ? SFX_GRASS : p.type == PR_CHEST ? SFX_CHEST : SFX_BREAK);
  switch (p.type) {
    case PR_GRASS: {
      fx.burst(p.x, p.y - 6, B.grass[1], 6, 70);
      const float r = rng.next();
      if (r < 0.05f) pickup_drop(*this, PK_HEAL, p.x, p.y);
      else if (r < 0.11f) pickup_drop(*this, PK_COIN, p.x, p.y, 1);
      break;
    }
    case PR_ROCK: {
      fx.burst(p.x, p.y - 10, B.rock[1], 14, 140);
      fx.ring(p.x, p.y - 8, 4, 26, B.rock[2], 0.3f, 3);
      const float r = rng.next();
      if (r < 0.40f) pickup_drop_xp(*this, p.x, p.y, 6);
      if (r > 0.75f) pickup_drop(*this, PK_COIN, p.x, p.y, 2);
      if (r > 0.94f) pickup_drop(*this, PK_HEAL, p.x + 8, p.y);
      break;
    }
    case PR_CHEST: {
      fx.burst(p.x, p.y - 10, 0xf2c443, 22, 180);
      fx.ring(p.x, p.y - 8, 6, 46, 0xf2c443, 0.45f, 4);
      camera.kick(0.25f);
      const int n = rng.range_int(4, 7);
      for (int i = 0; i < n; ++i) pickup_drop(*this, PK_COIN, p.x + rng.range(-14, 14), p.y + rng.range(-10, 10), 3);
      pickup_drop(*this, rng.next() < 0.5f ? PK_HEAL : PK_MAGNET, p.x, p.y);
      pickup_drop_xp(*this, p.x, p.y, 20);
      if (rng.next() < 0.08f) pickup_drop(*this, PK_ITEM, p.x, p.y - 12, float(rng.range_int(0, IT_COUNT - 1)));
      break;
    }
    case PR_BIGCHEST: {
      // Botín grande: monedas, tickets y experiencia para subir un nivel.
      fx.burst(p.x, p.y - 12, 0xf2c443, 34, 220);
      fx.ring(p.x, p.y - 8, 6, 80, 0xf2c443, 0.6f, 5);
      camera.kick(0.4f);
      for (int i = 0; i < 9; ++i) pickup_drop(*this, PK_COIN, p.x + rng.range(-22, 22), p.y + rng.range(-14, 14), 4);
      const int tk = rng.range_int(1, 3);
      for (int i = 0; i < tk; ++i) pickup_drop(*this, PK_TICKET, p.x + rng.range(-16, 16), p.y + rng.range(-10, 10), 1);
      if (rng.next() < 0.15f) pickup_drop(*this, PK_TICKET10, p.x, p.y - 10, 1);
      if (rng.next() < 0.5f) pickup_drop(*this, PK_ITEM, p.x + 14, p.y - 14, float(rng.range_int(0, IT_COUNT - 1)));
      pickup_drop_xp(*this, p.x, p.y, level_xp());
      progress_event(*this, EV_CHEST);
      break;
    }
    case PR_TRAP: interact_trap(*this, p); break;
    default: break;
  }
}

// ---------------- objetos equipables ----------------

static void after_choice(Game& g);

// Has cogido un objeto: si no llevas ninguno te lo pones; si no, eliges.
static void offer_item(Game& g, int id) {
  Player& p = g.pl();
  if (id < 0 || id >= IT_COUNT) return;
  if (g.state == ST_LEVELUP || g.choice != CH_NONE) { if (g.nlater < 8) g.later_items[g.nlater++] = int8_t(id); return; }
  char b[96];
  if (p.item < 0) {
    player_equip(g, p, id);
    std::snprintf(b, sizeof(b), "¡Llevas %s!", kItems[id].name);
    g.say(b, 2.6f);
    progress_event(g, EV_ITEM, id);
    return;
  }
  if (p.item == id) { std::snprintf(b, sizeof(b), "Ya llevas %s", kItems[id].name); g.say(b, 2); return; }
  // Se para el juego mientras eliges.
  g.item_cur = p.item; g.item_next = int8_t(id);
  g.choice = CH_ITEM;
  if (g.state == ST_PLAYING) g.state = ST_CHOICE;
}

void Game::pick_item(bool take_new) {
  if (choice != CH_ITEM) return;
  Player& p = pl();
  const int got = take_new ? item_next : item_cur;
  if (got != p.item) {
    player_equip(*this, p, got);
    char b[96];
    std::snprintf(b, sizeof(b), "¡Llevas %s!", kItems[got].name);
    say(b, 2.6f);
    progress_event(*this, EV_ITEM, got);
  }
  choice = CH_NONE;
  if (state == ST_CHOICE) state = ST_PLAYING;
  after_choice(*this);
}

static void after_choice(Game& g) {
  if (g.state != ST_PLAYING) return;
  if (g.nlater) {
    const int id = g.later_items[0];
    for (int i = 0; i < g.nlater - 1; ++i) g.later_items[i] = g.later_items[i + 1];
    g.nlater--;
    offer_item(g, id);
  }
}

// ---------------- vínculos ----------------
// Vínculo de un legendario: eliges qué movimientos se fusionan. Si ahora no se
// puede (no hay dos sueltos), se guarda hasta que aprendas otro movimiento.
static void offer_bond(Game& g) {
  Player& p = g.pl();
  if (p.bonds <= 0 || p.dead || g.state != ST_PLAYING || g.rift.arena || g.choice != CH_NONE) return;
  int8_t loose[4];
  int nl, grow;
  if (!player_bond_options(p, loose, nl, grow)) return;
  g.choice = CH_BOND;
  g.state = ST_CHOICE;
  g.sfx(SFX_LEVELUP);
}

void Game::pick_bond(const int* move_idx, int n) {
  if (choice != CH_BOND) return;
  Player& p = pl();
  int8_t loose[4];
  int nl, grow;
  const int mode = player_bond_options(p, loose, nl, grow);
  if (!mode || n <= 0) return;
  player_fuse(*this, p, move_idx, n, mode == 1 ? grow : -1);
  sfx(SFX_LEGEND);
  fx.ring(p.x, p.y, 8, 130, 0xff8ad8, 0.8f, 6);
  fx.burst(p.x, p.y - p.body_h * 0.5f, 0xffb3e6, 40, 240);
  int size = 0;
  for (int s = 0; s < p.nslots; ++s) if (p.slot_n[s] > size) size = p.slot_n[s];
  char b[64];
  std::snprintf(b, sizeof(b), "¡Vínculo! %d movimientos a la vez", size);
  say(b, 2.6f);
  choice = CH_NONE;
  if (state == ST_CHOICE) state = ST_PLAYING;
  after_choice(*this);
}

// ---------------- subida de nivel ----------------
static void maybe_level_up(Game& g) {
  if (g.pending_levels <= 0 || g.state != ST_PLAYING) return;
  Player& p = g.pl();
  // Nunca abras cartas con el Pokémon caído.
  if (p.dead) return;
  g.pending_levels--;
  g.state = ST_LEVELUP;
  g.sfx(SFX_LEVELUP);
  levelup_offer(g, p);
}

void Game::pick_card(int i) {
  if (state != ST_LEVELUP || i < 0 || i >= ncards) return;
  sfx(SFX_CONFIRM);
  levelup_apply(*this, pl(), cards[i]);
  state = ST_PLAYING;
  after_choice(*this);
}

// Cambiar las cartas: 1 gratis por run más los comprados en Mejoras.
bool Game::reroll_cards() {
  Player& p = pl();
  if (state != ST_LEVELUP || p.rerolls >= 1 + p.reroll_bonus) return false;
  p.rerolls++;
  levelup_offer(*this, p);
  sfx(SFX_SELECT);
  return true;
}

// ---------------- recogida ----------------

void game_on_collect(Game& g, const Pickup& p, Player& who) {
  Player& pl = g.pl();
  const bool mine = &who == &pl;
  static const int8_t SND[PK_COUNT] = {SFX_XP, SFX_XP, SFX_HEAL, SFX_CONFIRM, SFX_BREAK, SFX_COIN, SFX_SHINY, SFX_LEGEND, SFX_LEGEND};
  if (mine || p.kind == PK_BOMB) g.sfx(SND[p.kind]);
  char b[32];
  switch (p.kind) {
    case PK_XP: case PK_XPBIG:
      g.pending_levels += player_gain_xp(pl, p.value * 1);
      break;
    case PK_COIN:
      pl.coins += int(p.value);
      std::snprintf(b, sizeof(b), "+%d", int(p.value));
      g.fx.dmg_text(who.x, who.y - who.body_h - 6, b, 0xffd23f);
      break;
    case PK_ITEM:
      if (mine) offer_item(g, int(p.value));
      break;
    case PK_TICKET: case PK_TICKET10:
      if (p.kind == PK_TICKET) pl.t1++; else pl.t10++;
      // Los tickets se guardan en cuanto los coges (como los shinies).
      if (g.save) { if (p.kind == PK_TICKET) g.save->t1++; else g.save->t10++; g.dirty = true; }
      g.fx.dmg_text(who.x, who.y - who.body_h - 8, p.kind == PK_TICKET ? "+1 ticket" : "¡Ticket ×10!", p.kind == PK_TICKET ? 0xffd23f : 0xd9a6ff, true);
      g.fx.ring(who.x, who.y, 6, 50, p.kind == PK_TICKET ? 0xffd23f : 0xc47bff, 0.4f, 3);
      break;
    case PK_HEAL:
      if (mine) player_heal(g, pl, pl.max_hp * 0.3f);
      g.fx.ring(who.x, who.y, 6, 60, 0x5fe08a, 0.4f, 3);
      break;
    case PK_MAGNET:
      pickups_pull_all(g);
      g.fx.ring(who.x, who.y, 10, 420, 0xff9ed8, 0.6f, 4);
      break;
    case PK_BOMB: {
      if (dist2(who.x, who.y, pl.x, pl.y) < 500 * 500) g.camera.kick(0.8f);
      const float R = g.camera.outer_radius();
      g.fx.ring(who.x, who.y, 20, R, 0xff7b3d, 0.5f, 6);
      int32_t ids[kMaxEnemies];
      int n = 0;
      // (lo que ve quien la coge; en solitario, la cámara)
      for (const Enemy& e : g.enemies) if (!e.dead && g.camera.sees_from(who.view_x, who.view_y, e.x, e.y, 0)) ids[n++] = e.id;
      for (int i = 0; i < n; ++i) {
        Enemy* e = g.enemy_by_id(ids[i]);
        if (e) enemy_hurt(g, *e, 60 + pl.level * 14.0f, 0xff7b3d, e->x - who.x, e->y - who.y, 160);
      }
      break;
    }
    default: break;
  }
}

// ---------------- fin de la partida ----------------

void game_end_run(Game& g, bool dead) {
  if (g.state == ST_OVER) return;
  Player& pl = g.pl();
  g.state = ST_OVER;
  g.music = MU_NONE;
  g.sfx(dead ? SFX_GAMEOVER : SFX_BACK);
  RunSummary& s = g.summary;
  s = RunSummary();
  s.dead = dead;
  s.time = float(g.time);
  s.parts[0] = int(std::floor(g.time / 5));
  s.parts[1] = int(std::floor(pl.kills * 0.25));
  s.parts[2] = pl.bosses * 80;
  s.parts[3] = pl.coins;
  s.coin_mul = pl.coin_mul;
  s.coins = int(jsround((s.parts[0] + s.parts[1] + s.parts[2] + s.parts[3]) * pl.coin_mul));
  s.level = pl.level; s.kills = pl.kills; s.bosses = pl.bosses; s.t1 = pl.t1; s.t10 = pl.t10;
  if (g.judg.phase == 2) { s.judge_time = g.judg.arena_t; s.judge_bosses = g.judg.downs; }
  s.ncaught = g.ncaught;
  for (int i = 0; i < g.ncaught; ++i) { s.caught_dex[i] = g.caught_dex[i]; s.caught_new[i] = g.caught_new[i]; }
  extern void save_end_run(Game & g);
  if (g.save) save_end_run(g);
  g.over_ready = true;
}

// ---------------- paso ----------------

void Game::step(const Input* inputs) {
  const float dt = kDt;
  sfx_queue.clear();
  if (banner_t > 0) banner_t -= dt;
  if (state != ST_PLAYING) {
    // Con las cartas abiertas o en pausa, los efectos van a cámara lenta.
    if (state != ST_UI && state != ST_OVER) { fx.update(dt * 0.25f); pl().anim.update(dt * 0.25f); }
    return;
  }
  Player& p = pl();
  const Input& in = inputs ? inputs[0] : Input();
  if (in.select >= 0) player_set_active(*this, p, in.select);
  if (in.cycle) player_cycle(*this, p, in.cycle);

  if (flash_t > 0) flash_t -= dt;
  // La Sentencia de Arceus: mientras dura la animación, todo lo demás se para.
  if (judgment_cinematic(*this)) {
    judgment_cine_tick(*this, dt);
    p.anim.update(dt);
    world.update(dt, camera.left(), camera.top(), camera.w, camera.h);
    fx.update(dt);
    music = (judg.phase == 1 && judg.ct < 7.9f) ? MU_NONE : MU_JUDGMENT;
    return;
  }
  // Tras caer, deja que se vea la animación de Faint antes del resumen.
  // (En la arena de la grieta no: allí caer sólo te echa de vuelta.)
  if (p.dead && !rift.arena) {
    death_t += dt;
    player_update(*this, p, in, dt);
    enemies_update(*this, dt * 0.3f);
    fx.update(dt);
    views_follow(dt);
    if (death_t > 1.3f) { death_t = 0; game_end_run(*this, true); }
    return;
  }
  time += dt;

  const int b = int(std::floor(time / 240));
  if (b != biome && !rift.arena && !judgment_active(*this)) {
    biome = b;
    world.set_biome(b);
    sfx(SFX_BIOME);
    char s[96];
    std::snprintf(s, sizeof(s), "— %s —", world.biome_name());
    say(s, 3);
    fx.ring(p.x, p.y, 10, camera.outer_radius(), 0xffffff, 0.7f, 6);
  }

  p.frozen = false;
  player_update(*this, p, in, dt);
  views_follow(dt);
  world.update(dt, camera.left(), camera.top(), camera.w, camera.h);

  path_update(*this, dt);
  spawner_update(*this, dt);
  enemies_update(*this, dt);
  rebuild_grid();
  combat_tick(*this, p, dt);
  projectiles_update(*this, dt);
  combat_resolve(*this, p, dt);
  combat_update_fields(*this, dt);
  pickups_update(*this, dt);
  hazards_update(*this, dt);
  interact_update(*this, p, dt);
  rift_update(*this, dt);
  judgment_update(*this, dt);
  events_update(*this, dt);
  weather_update(*this, dt);
  fx.update(dt);

  if (state != ST_PLAYING && state != ST_CHOICE) return;
  music = int8_t(judgment_active(*this) ? int(MU_JUDGMENT) : rift.arena ? int(MU_RIFT) : boss_alive() ? int(MU_BOSS) : int(MU_FOREST) + biome % 3);
  maybe_level_up(*this);
  if (p.bonds > 0) offer_bond(*this);
}

}  // namespace ps

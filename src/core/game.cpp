// game.cpp — flujo de la partida y paso principal (game.js de la web).
//   estados: ui (menús), playing, levelup, paused, choice (objeto o vínculo), over
#include "core/game.h"
#include "core/evo.h"
#include "core/power.h"
#include "core/halloween.h"
#include "core/orbs.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/save.h"
#include "core/mega.h"
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
  const float k = game_xp_factor(*this) * (p.xp_mul > 0 ? p.xp_mul : 1);
  return std::ceil((p.xp_next - p.xp + 1) / k);
}

// ---------------- partida ----------------

static bool in_type_pool(const Player& p, int id) {
  int16_t all[kMaxMoves];
  const int na = move_pool_for_dex(p.dex, p.mon->types, all, kMaxMoves, p.taught);
  for (int i = 0; i < na; ++i) if (all[i] == id) return true;
  return false;
}

// Un jugador al empezar: Pokémon, atajos, vetos y mejora "Repertorio".
static void init_player(Game& g, Player& p, const RunConfig& c) {
  player_init(g, p, c.dex, c.shiny, c);
  p.skin = int8_t(c.skin >= 0 && c.skin < kSkinCount && kSkins[c.skin].dex == c.dex ? c.skin : -1);   // (su disfraz, si es suyo)
  // Atajos: movimientos con hueco fijo, antes de aprender nada.
  for (int i = 0; i < 4; ++i) if (c.pbind[i] >= 0 && c.pbind[i] < kMaxMoves) p.bind[c.pbind[i]] = int8_t(i);
  player_add_move(g, p, move_start_for(c.dex, c.pstart, c.upgrades[UP_START] > 0, p.taught));
  // Mejora "Veto": movimientos que no quieres que te salgan.
  const Perks perks = upgrade_perks(c.upgrades);
  for (int i = 0; i < 3 && i < perks.bans; ++i) if (c.pban[i] >= 0) p.banned.set(c.pban[i]);
  // Mejora "Repertorio": un segundo movimiento de ataque, el elegido (del port:
  // Mejoras → Repertorio) si sigue valiendo, o al azar como en la web.
  if (perks.start_moves >= 2 && c.pmove2 >= 0 && c.pmove2 < kMoveCount && !player_has_move(p, c.pmove2) &&
      !p.banned.has(c.pmove2) && !kind_is_status(kMoves[c.pmove2].kind) && in_type_pool(p, c.pmove2)) {
    player_add_move(g, p, c.pmove2);
    player_set_active(g, p, 0);
  } else if (perks.start_moves >= 2) {
    int16_t all[kMaxMoves], pool[kMaxMoves];
    const int na = move_pool_for_dex(p.dex, p.mon->types, all, kMaxMoves, p.taught);
    int np = 0;
    for (int i = 0; i < na; ++i) {
      const int id = all[i];
      if (player_has_move(p, id) || p.banned.has(id) || kind_is_status(kMoves[id].kind)) continue;
      pool[np++] = int16_t(id);
    }
    if (np) player_add_move(g, p, pool[int(g.rng.next() * np)]);
    player_set_active(g, p, 0);
  }
  p.ncards = 0; p.picked = true;
  p.choice = CH_NONE; p.nlater = 0; p.paused = false;
}

void Game::start_run(const RunConfig& c, Save* s) {
  cfg = c;
  save = s;
  hw = c.hw != 0;          // Halloween: antes de sortear enemigos y colocar el mapa
  candy_dropped = 0;
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
  eterna_reset(*this);
  eterna_wins = 0;
  judgment_reset(*this);
  treasure = Treasure();
  events_reset(*this);
  weather_reset(*this);
  path_clear(*this);
  world.reset(int32_t(rng.next() * 1e6f));
  world.set_halloween(hw);
  world.set_biome(run_biome(0), true);
  biome = 0;
  time = 0; pending_levels = 0;
  death_t = 0; flash_t = 0;
  banner[0] = 0; banner_t = 0;
  sfx_queue.clear();
  ncaught = 0;
  over_ready = false;
  heal_shown = heal_acc = 0;

  for (auto& p : players) p.active = false;
  nplayers = c.nplayers > 1 ? std::min(c.nplayers, kMaxPlayers) : 1;
  local = std::min(local, nplayers - 1);
  for (int i = 0; i < nplayers; ++i) {
    // Cada uno con su Pokémon y sus mejoras (en solitario, los de cfg).
    RunConfig pc = c;
    if (nplayers > 1) {
      const RunConfig::Member& m = c.members[i];
      pc.dex = m.dex; pc.shiny = m.shiny;
      std::memcpy(pc.upgrades, m.upgrades, sizeof(pc.upgrades));
      pc.mega_up = m.mega_up;
      pc.four_up = m.four_up;
      std::memcpy(pc.pbind, m.pbind, sizeof(pc.pbind));
      std::memcpy(pc.pban, m.pban, sizeof(pc.pban));
      pc.pstart = m.pstart;
      pc.pmove2 = m.pmove2;
      pc.skin = m.skin;
      pc.nature = m.nature;
      pc.abil = m.abil;
      pc.taught = m.taught;
    }
    init_player(*this, players[i], pc);
    if (nplayers > 1 && c.members[i].test_stone) player_equip(*this, players[i], IT_MEGA_A);   // (pruebas)
    // En grupo, en fila (coop.js begin).
    if (nplayers > 1) { players[i].x = (i - (nplayers - 1) / 2.0f) * 34; players[i].y = 0; }
  }
  views_snap(); camera.shake = 0;
  music = MU_FOREST;
  state = ST_PLAYING;
  if (c.demo_time) time = c.demo_time;
  // (con el reloj adelantado más allá de Eternatus, no sale: grabaciones y pruebas de la Sentencia)
  if (c.demo_time && game_teq(*this) >= kEternaAt) eterna.happened = true;
  if (c.demo_level) {
    Player& team = pl();
    while (team.level < c.demo_level) pending_levels += player_gain_xp(team, (team.xp_next - team.xp) / std::fmax(0.01f, team.xp_mul) + 0.01f);
  }
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
  // En grupo las monedas de la partida son del equipo (iguales en todas las
  // consolas): lo que depende de TU colección va directo a tu partida guardada.
  if (!fresh) { if (nplayers > 1) { if (save) { save->coins += 300; dirty = true; } } else p.coins += 300; }
  sfx(SFX_SHINYGET);
}

// Has llevado a casa a un Pokémon perdido y se une a ti.
void Game::on_rescue(int dex) {
  Player& p = pl();
  extern bool save_grant(Game & g, int dex);
  const bool fresh = save ? save_grant(*this, dex) : true;
  if (!fresh) { if (nplayers > 1) { if (save) { save->coins += 150; dirty = true; } } else p.coins += 150; }
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
  sfx(p.type == PR_GRASS ? SFX_GRASS : p.type == PR_CHEST ? SFX_CHEST : SFX_BREAK);   // (calabaza: como una roca)
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
      if (rng.next() < 0.08f) pickup_drop(*this, PK_ITEM, p.x, p.y - 12, float(roll_item(*this)));
      if (rng.next() < 0.35f) pickup_drop(*this, PK_SPHERE, p.x - 12, p.y - 12, float(orb_roll(*this)));   // (web, 23-10)
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
      if (rng.next() < 0.5f) pickup_drop(*this, PK_ITEM, p.x + 14, p.y - 14, float(roll_item(*this)));
      pickup_drop(*this, PK_SPHERE, p.x - 14, p.y - 14, float(orb_roll(*this)));
      if (rng.next() < 0.5f) pickup_drop(*this, PK_SPHERE, p.x, p.y - 20, float(orb_roll(*this)));
      pickup_drop_xp(*this, p.x, p.y, level_xp());
      progress_event(*this, EV_CHEST);
      break;
    }
    case PR_TRAP: interact_trap(*this, p); break;
    case PR_PUMPKIN: {
      // Halloween: experiencia, monedas y, si toca (candy_take), un caramelo.
      fx.burst(p.x, p.y - 10, 0xff8a2a, 18, 160);
      fx.burst(p.x, p.y - 12, 0xffd23f, 8, 120);
      fx.ring(p.x, p.y - 8, 4, 30, 0xff8a2a, 0.35f, 3);
      pickup_drop_xp(*this, p.x, p.y, 8);
      if (rng.next() < 0.5f) pickup_drop(*this, PK_COIN, p.x + 6, p.y, 2);
      if (candy_take(*this, time)) pickup_drop(*this, PK_CANDY, p.x - 6, p.y - 4, 1);
      break;
    }
    default: break;
  }
}

// ---------------- objetos equipables ----------------

static void after_choice(Game& g, Player& p);

namespace {
struct Fnv {
  uint32_t h = 2166136261u;
  void bytes(const void* p, size_t n) { const uint8_t* b = static_cast<const uint8_t*>(p); for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 16777619u; } }
  template <class T> void v(const T& x) { bytes(&x, sizeof(x)); }
};
}  // namespace

uint32_t Game::state_hash() const {
  Fnv f;
  f.bytes(rng.state(), 16);
  f.v(time); f.v(state); f.v(pending_levels); f.v(biome); f.v(hw); f.v(candy_dropped);
  f.v(next_enemy_id); f.v(next_proj_id); f.v(next_pickup_id);
  for (int i = 0; i < nplayers; ++i) {
    const Player& p = players[i];
    f.v(p.active); f.v(p.dead); f.v(p.x); f.v(p.y); f.v(p.hp); f.v(p.max_hp);
    f.v(p.level); f.v(p.xp); f.v(p.kills); f.v(p.coins); f.v(p.candy); f.v(p.skin); f.v(p.nmoves); f.v(p.active_slot);
    f.v(p.choice); f.v(p.picked); f.v(p.item); f.v(p.bonds);
    f.v(p.mega); f.v(p.mega_t); f.v(p.mega_cd);
    for (int k = 0; k < 4; ++k) { f.v(p.orb_id[k]); f.v(p.orb_n[k]); }
  }
  f.v(treasure.on); f.v(treasure.x); f.v(treasure.y);
  f.v(enemies.n);
  for (const Enemy& e : enemies) { f.v(e.id); f.v(e.x); f.v(e.y); f.v(e.hp); }
  f.v(projectiles.n);
  for (const Projectile& q : projectiles) { f.v(q.x); f.v(q.y); }
  f.v(pickups.n);
  for (const Pickup& q : pickups) { f.v(q.x); f.v(q.y); f.v(q.kind); }
  f.v(fields.n); f.v(hazards.n);
  f.v(rift.open); f.v(rift.arena); f.v(judg.phase);
  return f.h;
}

int Game::team_size() const {
  int n = 0;
  for (int i = 0; i < nplayers; ++i) if (players[i].active) n++;
  return n;
}

bool Game::anyone_choosing() const {
  for (int i = 0; i < nplayers; ++i) if (players[i].active && players[i].choice != CH_NONE) return true;
  return false;
}

// Un aviso que sólo es para un jugador sale sólo en su consola.
static void say_to(Game& g, const Player& p, const char* text, float secs) {
  if (&p == &g.players[g.local]) g.say(text, secs);
}

// Has cogido un objeto: eliges si te lo quedas (aunque no lleves ninguno:
// lo pidió el dueño del port; la web se lo pone sin preguntar). Si ya llevas
// otro, con cuál te quedas.
static void offer_item(Game& g, Player& p, int id) {
  if (id < 0 || id >= kItemTotal) return;
  id = mega_stone_for(g, p, id);   // (la megapiedra: la de tu Pokémon)
  if (g.state == ST_LEVELUP || p.choice != CH_NONE) { if (p.nlater < 8) p.later_items[p.nlater++] = int8_t(id); return; }
  char b[96];
  if (p.item == id) { std::snprintf(b, sizeof(b), "Ya llevas %s", item_name(id, p.dex)); say_to(g, p, b, 2); return; }
  // Se para el juego (para todos) mientras eliges. (item_cur -1: «dejarlo»)
  p.item_cur = p.item; p.item_next = int8_t(id);
  p.choice = CH_ITEM;
  if (g.state == ST_PLAYING) g.state = ST_CHOICE;
}

void Game::pick_item(Player& p, bool take_new) {
  if (p.choice != CH_ITEM) return;
  const int got = take_new ? p.item_next : p.item_cur;
  if (got < 0) say_to(*this, p, "Lo dejas en el suelo", 2);
  else if (got != p.item) {
    player_equip(*this, p, got);
    char b[96];
    std::snprintf(b, sizeof(b), "¡Llevas %s!", item_name(got, p.dex));
    say_to(*this, p, b, 2.6f);
    if (&p == &players[local] && got < IT_COUNT) progress_event(*this, EV_ITEM, got);
    if (is_mega_stone(got) && &p == &players[local]) say_to(*this, p, "¡Megapiedra! Pulsa Y para megaevolucionar", 3.2f);
  }
  // Sin la piedra (o con la otra: de la X a la Y), la mega de ahora se acaba.
  if (p.mega >= 0 && !p.power_on && (!is_mega_stone(p.item) || kMegas[p.mega].slot != p.item - IT_MEGA_A)) mega_end(*this, p);
  p.choice = CH_NONE;
  if (state == ST_CHOICE && !anyone_choosing()) state = ST_PLAYING;
  after_choice(*this, p);
}

static void after_choice(Game& g, Player& p) {
  if (g.state != ST_PLAYING && g.state != ST_CHOICE) return;
  if (p.nlater && p.choice == CH_NONE) {
    const int id = p.later_items[0];
    for (int i = 0; i < p.nlater - 1; ++i) p.later_items[i] = p.later_items[i + 1];
    p.nlater--;
    offer_item(g, p, id);
  }
}

// ---------------- vínculos ----------------
// Vínculo de un legendario: eliges qué movimientos se fusionan. Si ahora no se
// puede (no hay dos sueltos), se guarda hasta que aprendas otro movimiento.
static void offer_bond(Game& g, Player& p) {
  if (p.bonds <= 0 || p.dead || (g.state != ST_PLAYING && g.state != ST_CHOICE) || in_arena(g) || p.choice != CH_NONE) return;
  int8_t loose[4];
  int nl, grow;
  if (!player_bond_options(p, loose, nl, grow)) return;
  p.choice = CH_BOND;
  g.state = ST_CHOICE;
  if (&p == &g.players[g.local]) g.sfx(SFX_LEVELUP);
}

void Game::pick_bond(Player& p, const int* move_idx, int n) {
  if (p.choice != CH_BOND) return;
  int8_t loose[4];
  int nl, grow;
  const int mode = player_bond_options(p, loose, nl, grow);
  if (!mode || n <= 0) return;
  for (int i = 0; i < n; ++i) if (move_idx[i] < 0 || move_idx[i] >= p.nmoves) return;
  player_fuse(*this, p, move_idx, n, mode == 1 ? grow : -1);
  sfx(SFX_LEGEND);
  fx.ring(p.x, p.y, 8, 130, 0xff8ad8, 0.8f, 6);
  fx.burst(p.x, p.y - p.body_h * 0.5f, 0xffb3e6, 40, 240);
  int size = 0;
  for (int s = 0; s < p.nslots; ++s) if (p.slot_n[s] > size) size = p.slot_n[s];
  char b[64];
  std::snprintf(b, sizeof(b), "¡Vínculo! %d movimientos a la vez", size);
  say_to(*this, p, b, 2.6f);
  p.choice = CH_NONE;
  if (state == ST_CHOICE && !anyone_choosing()) state = ST_PLAYING;
  after_choice(*this, p);
}

// ---------------- subida de nivel ----------------
// En grupo, las cartas salen a la vez para todos y se espera a que elijan.
static void maybe_level_up(Game& g) {
  if (g.pending_levels <= 0 || g.state != ST_PLAYING) return;
  // Nunca abras cartas con el Pokémon caído (en grupo: con todos caídos).
  bool alive = false;
  for (int i = 0; i < g.nplayers; ++i) if (g.players[i].active && !g.players[i].dead) alive = true;
  if (!alive) return;
  g.pending_levels--;
  g.state = ST_LEVELUP;
  g.sfx(SFX_LEVELUP);
  for (int i = 0; i < g.nplayers; ++i) {
    Player& p = g.players[i];
    if (!p.active) continue;
    p.picked = false;
    levelup_offer(g, p);
  }
}

void Game::pick_card(Player& p, int i) {
  if (state != ST_LEVELUP || p.picked || i < 0 || i >= p.ncards) return;
  if (&p == &players[local]) sfx(SFX_CONFIRM);
  levelup_apply(*this, p, p.cards[i]);
  p.picked = true;
  for (int k = 0; k < nplayers; ++k) if (players[k].active && !players[k].picked) return;
  state = ST_PLAYING;
  for (int k = 0; k < nplayers; ++k) if (players[k].active) after_choice(*this, players[k]);
}

// Cambiar las cartas: 1 gratis por run más los comprados en Mejoras.
bool Game::reroll_cards(Player& p) {
  if (state != ST_LEVELUP || p.picked || p.rerolls >= 1 + p.reroll_bonus) return false;
  p.rerolls++;
  levelup_offer(*this, p);
  if (&p == &players[local]) sfx(SFX_SELECT);
  return true;
}

// ---------------- jugadores que se van ----------------

void Game::leave(int who) {
  if (who < 0 || who >= nplayers || !players[who].active) return;
  if (nplayers == 1 || who == 0) { quit_run(); return; }   // (si se va el anfitrión, se acaba)
  Player& p = players[who];
  p.active = false;
  p.choice = CH_NONE; p.picked = true;
  // Sus órbitas desaparecen (Projectiles.clearOrbs).
  for (int i = projectiles.n - 1; i >= 0; --i) if (projectiles[i].orb && projectiles[i].owner == who) projectiles.remove_ordered(i);
  char b[64];
  std::snprintf(b, sizeof(b), "%s ha dejado la partida", cfg.members[who].name[0] ? cfg.members[who].name : "Un compañero");
  say(b, 2.6f);
  if (state == ST_CHOICE && !anyone_choosing()) state = ST_PLAYING;
  if (state == ST_LEVELUP) {
    bool all = true;
    for (int k = 0; k < nplayers; ++k) if (players[k].active && !players[k].picked) all = false;
    if (all) state = ST_PLAYING;
  }
}

// ---------------- experiencia del equipo ----------------
// coop.js xpFactor: la media de los bonus de cada uno, repartida entre los que
// sois (en grupo se sube más despacio; si no, el triple de rápido).
static float xp_factor(const Game& g) {
  // (modo rápido: la experiencia llega antes, en la misma proporción que el reloj: quick_xp_k)
  if (g.nplayers <= 1) return quick_xp_k(g);
  const Player& p0 = g.players[0];
  float s = 0;
  int n = 0;
  for (int i = 0; i < g.nplayers; ++i) if (g.players[i].active) { s += g.players[i].xp_mul > 0 ? g.players[i].xp_mul : 1; n++; }
  const float own = p0.xp_mul > 0 ? p0.xp_mul : 1;
  return (s / n) / own / (1 + 0.6f * (n - 1)) * quick_xp_k(g);
}

float game_xp_factor(const Game& g) { return xp_factor(g); }

// Nivel, experiencia y contadores son del equipo (los lleva el jugador 0).
static void sync_team(Game& g) {
  const Player& t = g.players[0];
  for (int i = 1; i < g.nplayers; ++i) {
    Player& p = g.players[i];
    p.level = t.level; p.xp = t.xp; p.xp_next = t.xp_next;
    p.kills = t.kills; p.bosses = t.bosses; p.coins = t.coins; p.t1 = t.t1; p.t10 = t.t10; p.candy = t.candy;
  }
}

// ---------------- recogida ----------------

void game_on_collect(Game& g, const Pickup& p, Player& who) {
  Player& pl = g.pl();   // el equipo
  const bool mine = &who == &g.players[g.local];
  static const int8_t SND[PK_COUNT] = {SFX_XP, SFX_XP, SFX_HEAL, SFX_CONFIRM, SFX_BREAK, SFX_COIN, SFX_SHINY, SFX_LEGEND, SFX_LEGEND, SFX_CONFIRM, SFX_COIN};
  if (mine || p.kind == PK_BOMB || p.kind == PK_XP || p.kind == PK_XPBIG) g.sfx(SND[p.kind]);
  char b[32];
  switch (p.kind) {
    case PK_XP: case PK_XPBIG:
      g.pending_levels += player_gain_xp(pl, p.value * xp_factor(g));
      break;
    case PK_COIN:
      pl.coins += int(p.value);
      std::snprintf(b, sizeof(b), "+%d", int(p.value));
      g.fx.dmg_text(who.x, who.y - who.body_h - 6, b, 0xffd23f);
      break;
    case PK_ITEM:
      offer_item(g, who, int(p.value));
      break;
    case PK_SPHERE:
      orb_got(g, who, int(p.value));
      break;
    case PK_CANDY:
      // Caramelos de Halloween: a la Mochila en cuanto se cogen (como los tickets; en grupo, todos).
      pl.candy += int(p.value);
      if (g.save) { g.save->candy += int(p.value); g.save->candy_known = true; g.dirty = true; }
      if (int(p.value) == 1) std::snprintf(b, sizeof(b), "+1 caramelo");
      else std::snprintf(b, sizeof(b), "+%d caramelos", int(p.value));
      g.fx.dmg_text(who.x, who.y - who.body_h - 8, b, 0xff8a2a, true);
      g.fx.ring(who.x, who.y, 6, 44, 0xff8a2a, 0.4f, 3);
      break;
    case PK_TICKET: case PK_TICKET10:
      if (p.kind == PK_TICKET) pl.t1++; else pl.t10++;
      // Los tickets se guardan en cuanto los coges (como los shinies; en grupo, todos).
      if (g.save) { if (p.kind == PK_TICKET) g.save->t1++; else g.save->t10++; g.dirty = true; }
      g.fx.dmg_text(who.x, who.y - who.body_h - 8, p.kind == PK_TICKET ? "+1 ticket" : "¡Ticket ×10!", p.kind == PK_TICKET ? 0xffd23f : 0xd9a6ff, true);
      g.fx.ring(who.x, who.y, 6, 50, p.kind == PK_TICKET ? 0xffd23f : 0xc47bff, 0.4f, 3);
      break;
    case PK_HEAL:
      player_heal(g, who, who.max_hp * 0.3f);
      g.fx.ring(who.x, who.y, 6, 60, 0x5fe08a, 0.4f, 3);
      break;
    case PK_MAGNET:
      pickups_pull_all(g);
      g.fx.ring(who.x, who.y, 10, 420, 0xff9ed8, 0.6f, 4);
      break;
    case PK_BOMB: {
      const Player& me = g.players[g.local];
      if (dist2(who.x, who.y, me.x, me.y) < 500 * 500) g.camera.kick(0.8f);
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
  Player& pl = g.pl();                       // el equipo
  const Player& me = g.players[g.local];     // las mejoras de esta consola
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
  s.coin_mul = me.coin_mul;
  s.coins = int(jsround((s.parts[0] + s.parts[1] + s.parts[2] + s.parts[3]) * me.coin_mul));
  s.level = pl.level; s.kills = pl.kills; s.bosses = pl.bosses; s.t1 = pl.t1; s.t10 = pl.t10;
  if (g.judg.phase == 2) { s.judge_time = g.judg.arena_t; s.judge_bosses = g.judg.downs; s.judge_dmg = g.judg.dealt; s.judge_plate = g.judg.plate; }
  s.candy = pl.candy; s.hw = g.hw;
  if (g.judg.won) {   // ¡Arceus derrotado! (modo rápido): premio
    s.victory = true; s.dead = false;
    s.win_coins = kWinCoins; s.win_t10 = kWinT10;
    s.coins += kWinCoins;
  }
  s.ncaught = g.ncaught;
  for (int i = 0; i < g.ncaught; ++i) { s.caught_dex[i] = g.caught_dex[i]; s.caught_new[i] = g.caught_new[i]; }
  extern void save_end_run(Game & g);
  if (g.save) save_end_run(g);
  g.over_ready = true;
}

// ---------------- órdenes de cada jugador ----------------

static void apply_cmd(Game& g, int i, const Input& in) {
  Player& p = g.players[i];
  if (!p.active) return;
  switch (in.cmd) {
    case CMD_CARD: g.pick_card(p, in.arg); break;
    case CMD_REROLL: g.reroll_cards(p); break;
    case CMD_ITEM: g.pick_item(p, in.arg != 0); break;
    case CMD_BOND: {
      int idx[4], n = 0;
      for (int k = 0; k < 4 && k < in.arg; ++k) if (in.moves[k] >= 0) idx[n++] = in.moves[k];
      g.pick_bond(p, idx, n);
      break;
    }
    case CMD_PAUSE:
      // Solo: se para la partida. En grupo no: sólo te quedas quieto.
      if (g.nplayers == 1) g.toggle_pause();
      else p.paused = !p.paused;
      break;
    case CMD_SKIP: if (i == 0) g.skip_judgment(); break;   // (en grupo, el anfitrión)
    case CMD_QUIT: g.leave(i); break;
    case CMD_MEGA: if (power_ready(p)) power_start(g, p); else mega_start(g, p); break;
    case CMD_POWER: power_choose(g, p, in.arg); break;
    case CMD_ORB: orb_use(g, p, in.arg); break;
    case CMD_EVO: evo_choose(g, p, in.arg); break;
    default: break;
  }
}

// ---------------- en grupo: levantar al caído y fin ----------------

static void coop_tick(Game& g, float dt) {
  // Caído: un compañero a su lado le va levantando (game.js reviveTick).
  bool all_down = true;
  for (int i = 0; i < g.nplayers; ++i) {
    Player& p = g.players[i];
    if (!p.active) continue;
    if (!p.dead) { all_down = false; continue; }
    bool helper = false;
    for (int k = 0; k < g.nplayers; ++k) {
      const Player& o = g.players[k];
      if (k != i && o.active && !o.dead && dist2(o.x, o.y, p.x, p.y) < 56 * 56) helper = true;
    }
    p.revive_t = helper ? p.revive_t + dt : std::fmax(0.0f, p.revive_t - dt * 0.5f);
    if (p.revive_t >= 3) player_revive(g, p, 0.5f);
  }
  // Si caéis todos, se acaba (tras ver caer al último). En la arena no.
  if (g.rift.arena || !all_down) { g.death_t = 0; return; }
  g.death_t += dt;
  if (g.death_t > 1.6f) { g.death_t = 0; game_end_run(g, true); }
}

// ---------------- paso ----------------

void Game::step(const Input* inputs) {
  const float dt = kDt;
  static const Input none;
  auto in_of = [&](int i) -> const Input& { return inputs ? inputs[i] : none; };
  sfx_queue.clear();
  if (banner_t > 0) banner_t -= dt;
  if (state == ST_UI || state == ST_OVER) return;
  // Órdenes (cartas, objeto, vínculo, pausa...), siempre en el mismo orden.
  for (int i = 0; i < nplayers; ++i) if (in_of(i).cmd != CMD_NONE) apply_cmd(*this, i, in_of(i));
  if (state == ST_UI || state == ST_OVER) return;
  if (state != ST_PLAYING) {
    // Con las cartas abiertas o en pausa, los efectos van a cámara lenta.
    fx.update(dt * 0.25f);
    for (int i = 0; i < nplayers; ++i) if (players[i].active) players[i].anim.update(dt * 0.25f);
    return;
  }
  const bool coop = nplayers > 1;
  Player& p = pl();
  for (int i = 0; i < nplayers; ++i) {
    Player& q = players[i];
    if (!q.active) continue;
    const Input& in = in_of(i);
    if (in.select >= 0) player_set_active(*this, q, in.select);
    if (in.cycle) player_cycle(*this, q, in.cycle);
  }

  if (flash_t > 0) flash_t -= dt;
  // La Sentencia de Arceus: mientras dura la animación, todo lo demás se para.
  if (judgment_cinematic(*this)) {
    judgment_cine_tick(*this, dt);
    for (int i = 0; i < nplayers; ++i) if (players[i].active) players[i].anim.update(dt);
    world.update(dt, camera.left(), camera.top(), camera.w, camera.h);
    fx.update(dt);
    music = (judg.phase == 1 && judg.ct < 7.9f) ? MU_NONE : MU_JUDGMENT;
    return;
  }
  // Tras caer, deja que se vea la animación de Faint antes del resumen.
  // (En la arena de la grieta no: allí caer sólo te echa de vuelta. En grupo
  // tampoco: un compañero te puede levantar.)
  if (!coop && p.dead && !rift.arena) {
    death_t += dt;
    player_update(*this, p, in_of(0), dt);
    enemies_update(*this, dt * 0.3f);
    fx.update(dt);
    views_follow(dt);
    if (death_t > 1.3f) { death_t = 0; game_end_run(*this, true); }
    return;
  }
  time += dt;

  const int b = int(std::floor(time / 240));
  if (b != biome && !in_arena(*this) && !judgment_active(*this)) {
    biome = b;
    world.set_biome(run_biome(b));
    sfx(SFX_BIOME);
    char s[96];
    std::snprintf(s, sizeof(s), "— %s —", world.biome_name());
    say(s, 3);
    fx.ring(me().x, me().y, 10, camera.outer_radius(), 0xffffff, 0.7f, 6);
  }

  for (int i = 0; i < nplayers; ++i) {
    Player& q = players[i];
    if (!q.active) continue;
    q.frozen = coop && q.paused;
    player_update(*this, q, in_of(i), dt);
  }
  views_follow(dt);
  world.update(dt, camera.left(), camera.top(), camera.w, camera.h);

  path_update(*this, dt);
  spawner_update(*this, dt);
  enemies_update(*this, dt);
  rebuild_grid();
  for (int i = 0; i < nplayers; ++i) if (players[i].active) combat_tick(*this, players[i], dt);
  projectiles_update(*this, dt);
  combat_resolve(*this, dt);
  combat_update_fields(*this, dt);
  pickups_update(*this, dt);
  hazards_update(*this, dt);
  for (int i = 0; i < nplayers; ++i) if (players[i].active) interact_update(*this, players[i], dt);
  interact_bigchests(*this, dt);
  rift_update(*this, dt);
  eterna_update(*this, dt);
  treasure_update(*this, dt);
  judgment_update(*this, dt);
  events_update(*this, dt);
  weather_update(*this, dt);
  fx.update(dt);
  if (coop) { coop_tick(*this, dt); sync_team(*this); }

  if (state != ST_PLAYING && state != ST_CHOICE) return;
  music = int8_t(judgment_active(*this) ? int(MU_JUDGMENT) : eterna_on(*this) ? int(MU_JUDGMENT) : rift.arena ? int(MU_RIFT) : boss_alive() ? int(MU_BOSS) : int(MU_FOREST) + biome % 3);
  maybe_level_up(*this);
  // Evolución en plena partida (core/evo.h): cuando ya no quedan cartas por elegir.
  if (state == ST_PLAYING && pending_levels <= 0 && !in_arena(*this) && !judgment_cinematic(*this))
    for (int i = 0; i < nplayers; ++i) if (players[i].active) evo_update(*this, players[i]);
  for (int i = 0; i < nplayers; ++i) if (players[i].active && players[i].bonds > 0) offer_bond(*this, players[i]);
}

}  // namespace ps

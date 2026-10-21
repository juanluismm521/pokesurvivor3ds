// game.h — la partida entera (game.js y todos los sistemas de la web).
//
// Es el equivalente al objeto global G de la web: Game tiene el mundo, los
// jugadores, los enemigos, los proyectiles... y cada sistema (combat.cpp,
// spawner.cpp...) trabaja sobre él. NO dibuja, no suena y no lee botones:
// recibe Input por jugador y deja un estado que la plataforma dibuja, más
// colas de sonidos y avisos.
//
//   paso fijo de 60 Hz · aleatoriedad con semilla (rng: partida, vrng: efectos)
//   sin memoria dinámica en la partida: todo en grupos de tamaño fijo
//
// Preparado para el cooperativo futuro: hay hasta kMaxPlayers jugadores
// (de momento siempre 1) y los sistemas trabajan con everyone().
#pragma once
#include <cstdint>

#include "core/anim.h"
#include "core/camera.h"
#include "core/fx.h"
#include "core/gamedata.h"
#include "core/moves.h"
#include "core/pool.h"
#include "core/rng.h"
#include "core/types.h"
#include "core/world.h"

namespace ps {

constexpr float kDt = 1.0f / 60.0f;
constexpr int kMaxPlayers = 4;
// Escalado de dificultad del cooperativo (coop.js): n = jugadores en la partida.
inline float coop_scale_rate(int n) { return 1 + 0.75f * float(n > 1 ? n - 1 : 0); }
inline float coop_scale_hp(int n) { return 1 + 0.3f * float(n > 1 ? n - 1 : 0); }
constexpr int kEnemyCap = 64;          // Spawner.CAP
constexpr int kMaxEnemies = 112;       // vivos (tope + invocaciones + margen)
constexpr int kMaxCorpses = 120;
constexpr int kMaxProjectiles = 600;
constexpr int kMaxPickups = 424;       // Pickups.MAX = 420 (+ margen al fusionar)
constexpr int kMaxFields = 96;
constexpr int kMaxHazards = 160;

// ---------------- entradas ----------------
// Órdenes de la interfaz. Todo lo que cambia la partida entra como orden en
// la entrada del jugador (y no llamando al juego desde la interfaz): así, en
// el cooperativo, basta con mandar las entradas de cada paso.
enum Cmd : int8_t { CMD_NONE, CMD_CARD, CMD_REROLL, CMD_ITEM, CMD_BOND, CMD_PAUSE, CMD_SKIP, CMD_QUIT, CMD_MEGA };

struct Input {
  float ax = 0, ay = 0;     // movimiento (-1..1), ya normalizado si hace falta
  int8_t select = -1;       // elegir hueco 0..3
  int8_t cycle = 0;         // -1 / +1
  int8_t cmd = CMD_NONE;
  int8_t arg = 0;           // CARD: carta · ITEM: 1 = el nuevo, 0 = el que llevas · BOND: nº de movimientos
  int8_t moves[4] = {-1, -1, -1, -1};   // BOND: índices en p.moves
};

// ---------------- sonidos y avisos (para la plataforma) ----------------
enum Sfx : int8_t {
  SFX_NONE, SFX_SHOT, SFX_HIT, SFX_HURT, SFX_FAINT, SFX_BOSSDOWN, SFX_XP, SFX_COIN, SFX_HEAL,
  SFX_BREAK, SFX_CONFIRM, SFX_SHINY, SFX_LEGEND, SFX_LEVELUP, SFX_SELECT, SFX_PAUSE, SFX_BACK,
  SFX_GAMEOVER, SFX_BIOME, SFX_SURGE, SFX_BOSS, SFX_SHINYGET, SFX_GRASS, SFX_CHEST, SFX_RIFT,
  SFX_JUDGE, SFX_QUAKE, SFX_TOLL, SFX_DESCEND, SFX_LAND, SFX_COUNT
};
struct SfxEvent { int8_t id; int8_t kind; int8_t type; };   // kind/type: para 'shot'
enum Music : int8_t { MU_NONE = -1, MU_FOREST, MU_CAVE, MU_VOLCANO, MU_BOSS, MU_RIFT, MU_JUDGMENT, MU_VILLAGE, MU_COUNT };

// ---------------- jugador ----------------
struct PowerState { int8_t def = -1; float t = 0, dur = 0; };
struct Buff { int n = 0; float t = 0, amount = 0; };

// ---------------- cartas de subir de nivel ----------------
enum CardType : int8_t { CT_NEW, CT_UP, CT_EVO, CT_STAT };
struct Card {
  int8_t type = CT_NEW;
  int16_t move = -1;            // CT_NEW: kMoves · CT_UP/CT_EVO: índice en pl.moves
  int8_t stat = -1;             // CT_STAT
  int8_t evo_for = -1;          // CT_STAT: movimiento que espera esta mejora (índice en pl.moves)
  float w = 0;
};

// Elección que tiene abierta un jugador (la partida se para para todos).
enum Choice : int8_t { CH_NONE, CH_ITEM, CH_BOND };

struct Player {
  bool active = false;
  int16_t dex = 0;
  const Mon* mon = nullptr;
  bool shiny = false;
  Animator anim;
  float x = 0, y = 0, r = 0, body_h = 0;

  float atk_mul = 1, spd_mul = 1, cd_mul = 1, dmg_reduce = 0, regen_flat = 0;
  float magnet = 78, xp_mul = 1, coin_mul = 1;
  float max_hp = 100, hp = 100, base_spd = 100, base_atk = 1;
  float scale = 1;              // los grandes se dibujan algo más pequeños (player_scale)

  MoveInst moves[8];
  int nmoves = 0;
  int8_t slots[4][4];       // huecos: índices en moves (se lanzan a la vez)
  int8_t slot_n[4] = {};
  int nslots = 0;
  int active_slot = 0;
  int max_moves = 4;
  int bonds = 0;
  Buff buffs[BS_COUNT];

  int level = 1;
  float xp = 0, xp_next = 0;
  int kills = 0, bosses = 0;
  int rerolls = 0, reroll_bonus = 0;
  float crit = 0;
  MoveBits banned;              // bit por movimiento (Veto)
  int8_t bind[kMaxMoves];       // movimiento → hueco fijo (Atajos) o -1
  int coins = 0, t1 = 0, t10 = 0;
  double dmg_dealt = 0;

  float invuln = 0, flash = 0;
  // Megaevolución (core/mega): índice en kMegas mientras dura (-1 si no), lo
  // que queda, la recarga, la transformación y lo que hay que deshacer.
  int8_t mega = -1;
  uint8_t mega_up = 0;          // nivel de la mejora «Megapiedra» (probabilidad)
  float mega_t = 0, mega_cd = 0, mega_tf = 0;
  float mega_dhp = 0;           // vida máxima que da la mega (se quita al acabar)
  const Mon* mega_prev = nullptr;
  float aim = kPi / 2, move_angle = kPi / 2;
  bool walking = false, in_liquid = false;
  float lava_tick = 0, attack_face = 0;
  bool dead = false;
  PowerState power;
  float slow_t = 0;
  int8_t item = IT_NONE;
  float sash_t = -999;          // (segundos de partida)
  float bell_heal = 0;
  float item_regen = 0;
  uint8_t stats_taken[LS_COUNT] = {};
  float svx = 0, svy = 0;
  float orb_cast_t = 0;
  bool frozen = false;
  float revive_t = 0;
  // Vista de este jugador (la cámara que le seguiría, sin sacudidas). Las
  // comprobaciones de juego («fuera de la vista») usan las vistas de todos,
  // nunca la cámara de la consola: en el cooperativo cada una sigue a uno.
  float view_x = 0, view_y = 0;
  // Cartas de subir de nivel ofrecidas a este jugador (state == ST_LEVELUP) y
  // si ya ha elegido (en grupo, la partida sigue cuando han elegido todos).
  Card cards[3];
  int ncards = 0;
  bool picked = true;
  // Elección de objeto (choice == CH_ITEM): el que llevas y el nuevo; los que
  // llegan mientras eliges esperan su turno.
  Choice choice = CH_NONE;
  int8_t item_cur = -1, item_next = -1;
  int8_t later_items[8];
  int nlater = 0;
  bool paused = false;          // en grupo, la pausa no para la partida: te quedas quieto
  // velocidad suavizada (EnemyMgr.update: p._vx)
  bool has_v = false;
  float px_ = 0, py_ = 0, vx_ = 0, vy_ = 0;
  // blanco actual: enemigo (id) o prop
  int32_t target_enemy = 0;
  Prop* target_prop = nullptr;
  float target_x = 0, target_y = 0;
  bool has_target = false;
  // último charco del rastro
  int16_t last_patch = -1;
  float last_patch_x = 0, last_patch_y = 0;
};

// ---------------- enemigos ----------------
enum Behavior : int8_t {
  BH_CHASE, BH_CHARGER, BH_RANGED, BH_TANK, BH_HEALER, BH_SHIELDER, BH_SUMMONER, BH_BOMBER,
  BH_BEAMER, BH_ZONER, BH_FAN, BH_JUMPER, BH_RUNNER, BH_THIEF, BH_COUNT
};
extern const char* const kBehaviorId[BH_COUNT];

struct EnemyDef {
  int16_t dex = 0;
  int8_t behavior = BH_CHASE;
  float hp = 0, spd = 0, dmg = 0, xp = 0;
  float shot_dmg = 0, shot_cd = 0, range = 0;
  int16_t minion = 0;
  int8_t fan_n = 0, zones = 0;
  int16_t at = 0;               // jefes: segundo en que salen
  bool loot_t10 = false;        // ladrón: Gholdengo (ticket ×10)
};

struct Scale3 { float hp = 1, dmg = 1, spd = 1; };

enum Phase : int8_t { PH_WALK, PH_AIM, PH_DASH, PH_FUSE };
enum BossAtk : int8_t { BA_NONE = -1, BA_SLAM, BA_DASH, BA_LEAP, BA_RAIN, BA_BEAM, BA_NOVA, BA_VOLLEY };

struct BossAI {
  bool init = false;
  int8_t melee[2], ranged[2];
  float far_like = 0.35f;
  bool far = false;
  float mode_t = 0, cd = 1.6f;
  bool fury = false;
  float side = 1;
  // ataque en curso
  int8_t cast = BA_NONE;
  float ct = 0, wind = 0, after = 0, ang = 0, len = 0, dur = 0;
  bool go = false, fired = false;
};

struct Jump { bool on = false; float sx, sy, tx, ty, t, dur; };

struct Enemy {
  int32_t id = 0;
  EnemyDef def;
  int16_t dex = 0;
  float x = 0, y = 0;
  bool boss = false, legend = false, judge = false, shiny = false, golden = false;
  float scale = 1;
  Animator anim;
  float r = 0, body_h = 0;
  float max_hp = 0, hp = 0, spd = 0, dmg = 0, xp = 0;
  int8_t behavior = BH_CHASE;
  int8_t types[2] = {T_NORMAL, T_NONE};
  int8_t atk_type = T_NORMAL;
  const Mon* mon = nullptr;     // sus Defensas (stats.h)
  int8_t cat = CAT_PHYSICAL;    // pega con su lado fuerte: físico o especial
  bool flying = false;
  float flash = 0;
  float slow = 0, slow_t = 0;
  float burn = 0, burn_t = 0, burn_tick = 0;
  float poison = 0, poison_t = 0, poison_tick = 0;
  float lava_tick = 0;
  float kx = 0, ky = 0;
  float touch_cd = 0;
  bool dead = false;
  float fade = 0;
  int8_t phase = PH_WALK;
  float pt = 0, dx = 0, dy = 0;
  float shot_cd = 0;
  float stuck_t = 0, side_t = 0, side = 1;
  float bob = 0;
  float spark_t = 0;
  Scale3 sc;
  float shield = 0, shield_t = 0;
  float rd = 0;                 // jefes: daño recibido hace poco (bossDps = rd / 6)
  float stun_t = 0;             // aturdido: quieto y sin atacar
  float skill_cd = 0;
  int32_t minions[8];
  int nminions = 0;
  Jump jump;
  float jump_lift = 0;
  float lag_t = 0;
  float life = 0;               // ladrón: segundos antes de escapar
  float zig = 0;
  BossAI ai;
  // línea de visión (pathing): se mira cada 0,2 s
  float los_at = -1;
  bool los = false;
};

// ---------------- proyectiles ----------------
struct Projectile {
  int32_t id = 0;
  float x = 0, y = 0, vx = 0, vy = 0;
  float px = 0, py = 0;
  bool has_prev = false;
  float dmg = 10, r = 6, life = 1.4f, t = 0;
  int pierce = 0;
  bool friendly = true;
  uint32_t color = 0xffffff;
  float homing = 0, slow = 0, burn = 0, poison = 0, knock = 0;
  int8_t mtype = T_NONE;
  int8_t mcat = CAT_NONE;       // físico / especial (la defensa que lo para)
  int8_t bounce = 0;            // rebotes que le quedan (K_BOUNCE)
  float drain = 0;              // vida que cura al dueño al dar (Beso Drenaje)
  int8_t src = -1;              // movimiento del dueño (índice en moves): daño para el resumen
  int8_t vis_proj = SH_BALL, vis_pal = T_NORMAL, vis_trail = TR_NONE;
  bool has_vis = false;
  int8_t owner = 0;
  bool ghost = false;
  // órbitas
  bool orb = false;
  int16_t mid = -1;             // movimiento (kMoves) de la órbita
  float oa = 0, orad = 0, ospeed = 0, rehit = 0.34f;
  // bumerán
  float boom = 0, bsp = 0;
  bool back = false;
  float trail_t = 0;
  // golpes ya dados
  static constexpr int kHits = 48;
  int32_t hits[kHits];
  int nhits = 0;
  int32_t phits[16];
  int nphits = 0;
  struct HitLog { int32_t id; float t; };
  HitLog log[kHits];
  int nlog = 0;
};

// ---------------- objetos del suelo ----------------
enum PickKind : int8_t { PK_XP, PK_XPBIG, PK_HEAL, PK_MAGNET, PK_BOMB, PK_COIN, PK_TICKET, PK_TICKET10, PK_ITEM, PK_COUNT };
struct Pickup {
  int32_t id = 0;
  int8_t kind = PK_XP;
  float x = 0, y = 0, value = 0, vx = 0, vy = 0, t = 0;
  bool pulled = false;
};

// ---------------- campos (minas, meteoros, torretas, charcos) ----------------
enum FieldKind : int8_t { FK_MINE, FK_METEOR, FK_TURRET, FK_PATCH,
                          // familias nuevas (2026-10-19)
                          FK_CHARGE, FK_SPIN, FK_QUAKE, FK_VORTEX, FK_STUNZONE };
struct Field {
  int8_t kind = FK_MINE;
  int8_t owner = 0;
  bool ghost = false, gone = false;
  int8_t pl_move = 0;           // índice en players[owner].moves
  uint32_t col = 0;
  int8_t vis_proj = SH_BALL, vis_pal = T_NORMAL, vis_aura = SH_NONE;
  float x = 0, y = 0, t = 0;
  float arm = 0, life = 0, r = 0, dmg = 0;
  float fall = 0, shot_t = 0, kick = 0, tick = 0;
  // familias nuevas: carga (at), giro y terremoto (n, next, a / i, ca, sa),
  // remolino (pull) y zona de aturdir (delay)
  float at = 0, next = 0, a = 0, ca = 0, sa = 0, pull = 0, delay = 0;
  int8_t n = 0, i = 0;
};

// ---------------- peligros con aviso ----------------
struct Hazard {
  bool beam = false;            // false = zona
  bool pillar = false;          // la Sentencia (pilar de luz)
  float x = 0, y = 0, r = 0, angle = 0, len = 0, w = 0;
  float delay = 0, dmg = 0, pct = 0, t = 0, end = 0.3f;
  int8_t type = T_NONE;
  int8_t cat = CAT_NONE;        // la defensa tuya que lo para (la del enemigo que lo lanza)
  bool pure = false, hit = false;
  uint32_t color = 0;
};

// ---------------- eventos de partida ----------------
enum EventId : int8_t { EVN_NONE = -1, EVN_HORDE, EVN_GOLD, EVN_LOST };
struct Lost {
  bool on = false;
  int16_t dex = 0;
  float x = 0, y = 0, tx = 0, ty = 0;
  float hp = 100, max_hp = 100, t = 0, hit_cd = 0, flash = 0;
  bool follow = false, moving = false, grant = false;
  int8_t follow_pl = 0;
  float r = 0;
  Animator anim;
};

enum State : int8_t { ST_UI, ST_PLAYING, ST_LEVELUP, ST_PAUSED, ST_CHOICE, ST_OVER };

// Resumen al acabar (pantalla de fin de partida).
struct RunSummary {
  bool dead = false;
  float time = 0;
  int level = 1, kills = 0, bosses = 0, coins = 0, t1 = 0, t10 = 0;
  bool record = false;
  int parts[4] = {};            // tiempo, derrotados, jefes, recogidas
  float coin_mul = 1;
  float judge_time = -1;
  int judge_bosses = 0;
  int ncaught = 0;
  int16_t caught_dex[16];
  bool caught_new[16];
};

struct Save;

// Datos de partida que el juego necesita de la partida guardada.
struct RunConfig {
  int dex = 1;
  bool shiny = false;
  uint8_t upgrades[UP_COUNT] = {};
  uint8_t mega_up = 0;                   // mejora «Megapiedra» (del port: Save::pmega)
  int8_t pbind[4] = {-1, -1, -1, -1};   // movimientos con hueco fijo
  int8_t pban[3] = {-1, -1, -1};
  int8_t pstart = -1;                    // Movimiento inicial elegido (-1: el de serie)
  uint32_t seed = 1;
  int judged = 0;                        // veces que ha visto la Sentencia (saltar animación)
  uint8_t legends_beaten[(kMaxDex + 8) / 8] = {};   // bit por dex: legendarios ya vencidos (rift.cpp)
  // Cooperativo: cada jugador con su Pokémon y sus mejoras (el 0 es el
  // anfitrión). Con nplayers == 1 se usan los campos de arriba.
  struct Member {
    int dex = 1;
    bool shiny = false;
    uint8_t upgrades[UP_COUNT] = {};
  uint8_t mega_up = 0;                   // mejora «Megapiedra» (del port: Save::pmega)
    int8_t pbind[4] = {-1, -1, -1, -1};
    int8_t pban[3] = {-1, -1, -1};
    int8_t pstart = -1;
    char name[16] = {};
    // Pruebas: la vida de ESTE jugador nunca baja de 1. Va con los datos de la
    // sala para que todas las consolas lo simulen igual (debug_immortal es de
    // la partida entera).
    bool immortal = false;
    bool test_stone = false;   // pruebas: empieza con su megapiedra puesta
  };
  int nplayers = 1;
  Member members[4];
};

struct Game {
  Game();
  // --- control ---
  void start_run(const RunConfig& cfg, Save* save = nullptr);
  void step(const Input* inputs);        // un paso de 1/60 s
  void toggle_pause();
  void quit_run();
  // Elecciones (las aplica step() al leer las órdenes de cada jugador).
  void pick_card(Player& p, int i);
  bool reroll_cards(Player& p);
  void pick_item(Player& p, bool take_new);
  void pick_bond(Player& p, const int* move_idx, int n);
  void skip_judgment();
  void leave(int who);          // un jugador se va (el 0, el anfitrión: se acaba para todos)
  bool anyone_choosing() const;
  int team_size() const;        // jugadores que siguen en la partida
  // Huella de lo que es de juego (no de lo que sólo se ve): en el cooperativo,
  // si dos consolas dan una distinta en el mismo paso, se han desincronizado.
  uint32_t state_hash() const;

  // --- estado ---
  State state = ST_UI;
  double time = 0;
  int biome = 0;
  int pending_levels = 0;
  Save* save = nullptr;
  int64_t now_ms = 0;           // reloj de la consola (lo pone la plataforma)
  bool dirty = false;           // hay que guardar
  bool debug_immortal = false;  // pruebas: la vida nunca baja de 1 (para llegar a la Sentencia)
  RunConfig cfg;
  Rng rng, vrng;
  World world;
  Camera camera;
  Fx fx;
  Player players[kMaxPlayers];
  int nplayers = 1;
  int local = 0;                // jugador de esta consola (sólo para la cámara, sonidos y dibujo)
  Player& pl() { return players[0]; }
  Player& me() { return players[local]; }
  // Vistas: siguen a cada jugador (o todas a un punto); la cámara copia la del local.
  void views_follow(float dt);
  void views_follow_point(float x, float y, float dt);
  void views_set(float x, float y);
  void views_snap();            // cada vista, encima de su jugador (al teletransportar)
  bool seen_by_any(float x, float y, float pad) const;

  FixedList<Enemy, kMaxEnemies> enemies;
  FixedList<Enemy, kMaxCorpses> corpses;
  FixedList<Projectile, kMaxProjectiles> projectiles;
  FixedList<Pickup, kMaxPickups> pickups;
  FixedList<Field, kMaxFields> fields;
  FixedList<Hazard, kMaxHazards> hazards;
  int32_t next_enemy_id = 1, next_proj_id = 1, next_pickup_id = 1;


  RunSummary summary;
  bool over_ready = false;      // el resumen está listo para enseñarse
  float death_t = 0;
  float flash_t = 0, flash_dur = 1;
  uint32_t flash_col = 0xffffff;
  int ncaught = 0;
  int16_t caught_dex[16];
  bool caught_new[16];

  // Aviso grande en pantalla (Spawner.say).
  char banner[96] = {};
  float banner_t = 0;
  void say(const char* text, float secs = 2.6f);

  // Sonidos de este paso (la plataforma los toca y vacía la cola).
  FixedList<SfxEvent, 48> sfx_queue;
  void sfx(int id, int kind = -1, int type = -1);
  int8_t music = MU_NONE;

  // --- utilidades para los sistemas ---
  int everyone(Player** out);
  Enemy* enemy_by_id(int32_t id);
  bool boss_alive() const;
  int query_circle(float x, float y, float r, Enemy** out, int max);
  void rebuild_grid();
  void on_prop_destroyed(const Prop& p);
  void flash(uint32_t col, float dur) { flash_col = col; flash_dur = dur; flash_t = dur; }
  float level_xp();             // experiencia que falta para subir (cofres con candado)
  void on_shiny_caught(int dex);
  void on_rescue(int dex);

  // --- subsistemas (cada uno en su .cpp) ---
  struct SpawnerState {
    float acc = 0; int boss_idx = 0; float surge_t = 32; int force_shiny = 0;
  } spawner;
  struct CastTier { int16_t from; int n; EnemyDef list[8]; };
  CastTier cast[8];
  int ncast = 0;
  EnemyDef bosses[8];
  int nbosses = 0;

  struct EventsState {
    float next_t = 0; int8_t cur = EVN_NONE; float cur_t = 0, cur_dur = 0; int8_t last = EVN_NONE;
    int8_t horde_type = T_NONE; EnemyDef horde[4]; int nhorde = 0;
    float thief_t = 0; int thieves = 0;
  } events;
  Lost lost;

  struct WeatherState { int8_t kind = WX_NONE; float t = 0, dur = 0, check_t = 60, chip_t = 0; } weather;

  struct RiftState {
    bool open = false; float x = 0, y = 0, ttl = 0, t = 0;
    bool arena = false; int32_t tx = 0, ty = 0; float ax = 0, ay = 0, back_x = 0, back_y = 0;
    int16_t dex = 0; bool shiny = false; int8_t type = 0; int back_biome = 0;
    float at = 0; int8_t phase = 0; float end_t = 0; int32_t boss_id = 0;
    float check_t = 30; bool asked = false; float down_t = 0; int misses = 0;
    bool zone = false; float zone_x = 0, zone_y = 0, zone_r = 0;   // dónde estaba la última arena (leftBehind)
  } rift;

  struct JudgmentState {
    int8_t phase = 0;           // 0 nada, 1 animación, 2 pelea
    float ct = 0;
    int32_t tx = 0, ty = 0;
    int16_t q[40]; int nq = 0;
    bool ported = false, landed = false, can_skip = false;
    float cx = 0, cy = 0;       // centro de la arena
    // Arceus
    bool a_on = false;
    float ax = 0, ay = 0, lift = 0, ar = 0, abody_h = 0, acd = 0, touch_cd = 0, spin = 0;
    Animator aanim;
    int8_t acast = -1; float act = 0, awind = 0; bool adone = false; int8_t alast = -1;
    float arena_t = 0, boss_t = 0; int boss_n = 0; float base = 0; int downs = 0;
    // daño a un solo enemigo por segundo (vida del primer legendario)
    float hit_t = 0; float samples[30]; int nsamples = 0;
    int32_t hit_id[64]; float hit_v[64]; int nhit = 0;
    bool cam_on = false; float cam_x = 0, cam_y = 0; bool cam_ported = false;
  } judg;

  // Campo de distancias del camino (pathing.js), uno por jugador.
  static constexpr int kPathR = 28, kPathN = kPathR * 2 + 1;
  struct PathField { bool on = false; int32_t ox = 0, oy = 0; int16_t dist[kPathN * kPathN]; uint8_t open[kPathN * kPathN]; };
  PathField paths[kMaxPlayers];
  float path_timer = 0;

  // Rejilla espacial de enemigos (EnemyMgr: CELL = 52).
  static constexpr int kGridSize = 64;
  int16_t grid_head[kGridSize * kGridSize];
  int16_t grid_next[kMaxEnemies];
  int32_t grid_cx0 = 0, grid_cy0 = 0;
  // Enemigos guardados mientras dura una arena.
  FixedList<Enemy, kEnemyCap + 32> stashed;
  bool has_stash = false;

  // Interacción (manantial)
  float heal_shown = 0, heal_acc = 0;
};

// ---------------- sistemas (game_*.cpp) ----------------
// player.cpp
void player_init(Game& g, Player& p, int dex, bool shiny, const RunConfig& cfg);
void player_update(Game& g, Player& p, const Input& in, float dt);
bool player_add_move(Game& g, Player& p, int def);
bool player_has_move(const Player& p, int def);
void player_set_active(Game& g, Player& p, int i);
void player_cycle(Game& g, Player& p, int dir);
void player_hurt(Game& g, Player& p, float amount, int type = T_NONE, bool pure = false, int cat = CAT_NONE);
void player_heal(Game& g, Player& p, float amount);
void player_add_max_hp(Player& p, float amount);
int player_gain_xp(Player& p, float amount);
void player_equip(Game& g, Player& p, int item);
void player_add_power(Player& p, int def, float dur);
void player_add_buff(Player& p, int stat, float amount, float dur, int max_stacks);
void player_cast(Game& g, Player& p, const MoveInst* m, int kind, float cd);
void player_revive(Game& g, Player& p, float frac);
float player_atk(const Player& p);
float player_atk_for(const Player& p, const MoveInst& m);   // atkFor
bool player_mono_type(const Player& p, int type);
float player_spd(Game& g, const Player& p);
float player_reduction(const Player& p);
float player_regen(const Player& p);
float xp_for(int level);
int player_bond_options(const Player& p, int8_t* loose, int& nloose, int& grow_slot);
void player_fuse(Game& g, Player& p, const int* move_idx, int n, int grow_slot);
// enemy.cpp
Enemy* enemy_spawn(Game& g, const EnemyDef& def, float x, float y, Scale3 sc, bool boss = false);
void enemy_make_shiny(Game& g, Enemy& e);
void enemy_make_golden(Enemy& e);
void enemy_hurt(Game& g, Enemy& e, float amount, uint32_t color, float dir_x = 0, float dir_y = 0, float knock = 0, int eff = 0);
void enemy_apply_slow(Enemy& e, float factor, float dur = 1.2f);
void enemy_apply_stun(Enemy& e, float dur);
void enemy_apply_burn(Enemy& e, float stacks = 1);
void enemy_apply_poison(Enemy& e, float stacks = 1);
void enemy_die(Game& g, Enemy& e);
void enemy_strike(Enemy& e, const Player& p);
void enemies_update(Game& g, float dt);
// projectile.cpp
Projectile* proj_spawn(Game& g);
Projectile* proj_spawn_orb(Game& g);
void proj_clear_orbs(Game& g, int owner);
void projectiles_update(Game& g, float dt);
Enemy* nearest_enemy(Game& g, float x, float y, float max_dist);
// combat.cpp
void combat_tick(Game& g, Player& p, float dt);
void combat_resolve(Game& g, float dt);
void combat_update_fields(Game& g, float dt);
void combat_clear_fields(Game& g, int owner = -1);
// pickup.cpp
void pickup_drop(Game& g, int kind, float x, float y, float value = 0);
void pickup_drop_xp(Game& g, float x, float y, float value, bool boss = false, bool legend = false, bool plain = false);
void pickups_pull_all(Game& g);
void pickups_update(Game& g, float dt);
// spawner.cpp + enemies.cpp
void spawner_reset(Game& g);
void spawner_update(Game& g, float dt);
void spawner_burst(Game& g, int n);
bool spawner_ahead(Game& g, Enemy& e, Player& p);
float spawner_rate(float t);
void enemies_roll_run(Game& g);
Scale3 enemies_scale(float t);
Scale3 enemies_boss_scale(float t);
int behavior_of(const Mon& p);
EnemyDef enemy_def_for(const Mon& p, int ti);
int enemies_bag_at(Game& g, float t, const EnemyDef** out, int max);
int enemies_tier_at(float t);
int enemies_horde_types(float t, int8_t* out);
int enemies_horde_defs(Game& g, int type, float t, EnemyDef* out);
// hazards.cpp
Hazard* hazard_add(Game& g);
void hazards_update(Game& g, float dt);
bool hazard_inside(const Hazard& h, float x, float y, float r);
// levelup.cpp
// Multiplicador de la experiencia del equipo (coop.js xpFactor; 1 en solitario).
float game_xp_factor(const Game& g);
void levelup_offer(Game& g, Player& p);
void levelup_apply(Game& g, Player& p, const Card& c);
// interact.cpp
void interact_update(Game& g, Player& p, float dt);
void interact_trap(Game& g, const Prop& p);
// events.cpp
void events_reset(Game& g);
void events_update(Game& g, float dt);
bool events_golden(const Game& g);
// weather.cpp
void weather_reset(Game& g);
void weather_update(Game& g, float dt);
float weather_mult(const Game& g, int type);
float weather_speed_for(const Game& g, const int8_t types[2]);
float weather_level(const Game& g);
void weather_force(Game& g, int kind, float dur);
void weather_clear(Game& g);
// rift.cpp
// ¿Sigue este jugador en la arena aunque ya se haya vuelto al mapa? (leftBehind)
inline bool rift_left_behind(const Game& g, const Player& p) {
  return !g.rift.arena && g.rift.zone && dist2(p.x, p.y, g.rift.zone_x, g.rift.zone_y) < g.rift.zone_r * g.rift.zone_r;
}
void rift_reset(Game& g);
void rift_update(Game& g, float dt);
float rift_time_left(const Game& g);
// judgment.cpp
constexpr float kJudgmentAt = 30 * 60;
void judgment_reset(Game& g);
void judgment_update(Game& g, float dt);
void judgment_cine_tick(Game& g, float dt);
void judgment_note_dmg(Game& g, float d, const Enemy& e);
bool judgment_active(const Game& g);
// Velocidad con tope suave en la Sentencia: lo que pase de 130 cuenta un 25 %.
inline float judgment_cap_spd(float s) { return s <= 130 ? s : 130 + (s - 130) * 0.25f; }
bool judgment_cinematic(const Game& g);
// pathing.cpp
void path_update(Game& g, float dt);
bool path_dir_for(Game& g, Enemy& e, int pl_index, float& ox, float& oy);
void path_clear(Game& g);
// progress.cpp (misiones, logros, estadísticas)
struct KillInfo { const int8_t* types; bool boss; int legend; };
void progress_kill(Game& g, const Enemy& e);
void progress_event(Game& g, int ev, int a = 0, int b = 0, float f = 0);

}  // namespace ps

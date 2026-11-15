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
enum Cmd : int8_t { CMD_NONE, CMD_CARD, CMD_REROLL, CMD_ITEM, CMD_BOND, CMD_PAUSE, CMD_SKIP, CMD_QUIT, CMD_MEGA, CMD_ORB, CMD_EVO, CMD_POWER };   // (CMD_ORB: arg = hueco · CMD_EVO: arg = opción)

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
  bool excl = false;            // CT_NEW: es el ataque exclusivo de tu Pokémon (carta dorada)
  float w = 0;
};

// Elección que tiene abierta un jugador (la partida se para para todos).
enum Choice : int8_t { CH_NONE, CH_ITEM, CH_BOND, CH_EVO, CH_POWER };

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
  // Esferas de Mundo Misterioso (orbs.h): 4 huecos (id o -1, cuántas), mejoras
  // con tiempo (segundos que quedan de cada una) y recarga.
  int8_t orb_id[4] = {-1, -1, -1, -1};
  int8_t orb_n[4] = {};
  float orb_buff[8] = {};   // (por esfera: ORB_COUNT ≤ 8)
  float orb_cd = 0;
  uint8_t orb_seen = 0;         // las que ya te han explicado (bit por esfera)
  int8_t orb_new = -1; float orb_new_t = 0;   // (la casilla recién cogida brilla)
  MoveBits banned;              // bit por movimiento (Veto)
  int8_t bind[kMaxMoves];       // movimiento → hueco fijo (Atajos) o -1
  int coins = 0, t1 = 0, t10 = 0;
  bool four_told = false;       // (ya se le ha explicado lo de las 4 cartas)
  int candy = 0;                // caramelos de Halloween de esta partida
  int8_t skin = -1;             // disfraz (índice de kSkins; -1 ninguno)
  bool aura = false;            // aura de maestro (maestría completa con su Pokémon: sólo se ve)
  float nh_hp = 0;              // desafío «Sin curaciones»: la vida nunca sube de aquí
  // Naturaleza, habilidad y MT (core/mons.h, core/abilities.h): `mon` apunta a
  // mon_buf, su Pokémon con la naturaleza aplicada (y a mega_buf durante la mega).
  int8_t nature = -1;
  int16_t abil = -1;            // índice de kAbils (-1: sin habilidad, como en las pruebas)
  int16_t abx = -1;             // su efecto (abilities.cpp; -1: la pasiva genérica de su tipo)
  int8_t ab_type = T_NONE;      // tipo de la pasiva genérica (+8 %): el primero del Pokémon al empezar
  TmBits taught = {};          // MT aprendidas (bit = kTms): salen en sus cartas de nivel
  float ab_moxie = 0, ab_spd = 0, ab_t = 0, ab_sum = 0, ab_sturdy = -999;
  bool ab_angry = false, ab_shield = false, ab_boomed = false;
  int8_t ab_mood = -1;
  Mon mon_buf{}, mega_buf{};
  double dmg_dealt = 0;

  float invuln = 0, flash = 0;
  // Megaevolución (core/mega): índice en kMegas mientras dura (-1 si no), lo
  // que queda, la recarga, la transformación y lo que hay que deshacer.
  int8_t mega = -1;
  uint8_t mega_up = 0;          // nivel de la mejora «Megapiedra» (probabilidad)
  bool four_up = false;         // mejora «Más pasivas a niveles altos» (las 4 cartas: levelup_four)
  float mega_t = 0, mega_cd = 0, mega_tf = 0;
  float mega_dhp = 0;           // vida máxima que da la mega (se quita al acabar)
  float mega_regen = 0;         // regeneración que da la mega (se quita al acabar)
  const Mon* mega_prev = nullptr;
  float aim = kPi / 2, move_angle = kPi / 2;
  bool walking = false, in_liquid = false;
  float lava_tick = 0, attack_face = 0;
  bool dead = false;
  PowerState power;
  float slow_t = 0;
  // Gran Actualización: Protección, Descanso, Reflejo [físico] / Pantalla Luz [especial], y las
  // cartas de nivel nuevas (esquiva, daño de los críticos, daño a jefes, más monedas).
  float guard_t = 0, rest_t = 0, zzz_t = 0;
  float screen_t[2] = {}, screen_k[2] = {};
  float dodge = 0, crit_dmg = 0, boss_mul = 1, power_cd_k = 1;
  // Evolución en plena partida (core/evo.h): con quién empezó, su línea, la etapa, de quién
  // tiene ahora los stats y, mientras eliges (choice == CH_EVO), las opciones.
  int16_t start_dex = 0;
  bool start_shiny = false;
  int16_t evo_line[4] = {};
  int8_t evo_nline = 0, evo_stage = 0;
  int16_t evo_stat[8] = {};
  int8_t evo_nstat = 0;
  bool evo_half = false, evo_solo = false, evo_pending_half = false;
  int16_t evo_lv_prev = 0;
  int16_t evo_opts[3] = {}, evo_all[8] = {};
  int8_t evo_nopts = 0, evo_nall = 0;
  // Poder de Eternatus (core/power.h): cuál (-1 ninguno), si está en marcha, lo que queda,
  // la recarga y, mientras eliges (choice == CH_POWER), las opciones.
  int8_t epower = -1;
  bool power_on = false, power_mega = false, splashing = false;
  float power_t = 0, power_cd = 0, power_hp = 0, z_t = 0;
  int8_t power_opts[4] = {}, npower_opts = 0;
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
  Card cards[4];               // 3 (o 4 con levelup_four: al menos 2 pasivas)
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
  // Gran Actualización (enemies.js): su tramo (-1 = ninguno), lo que multiplica a la vida,
  // el daño y la XP que le tocan por tiempo (core/diff.h), y qué jefe es (0..4).
  int8_t ti = -1, idx = 0;
  float hp_f = 1, dmg_f = 1, xp_f = 1;
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
  bool arceus = false;          // el blanco de Arceus en la Sentencia (invisible, le sigue; nunca cae)
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
  float shred_t = 0;            // Triturar: recibe un 25 % más mientras dura
  float crush_t = 0;            // Dinamax: aplastado hace poco (no se le repite cada fotograma)
  // Eternatus Eternamax y sus núcleos (core/eterna.cpp)
  bool eterna = false, ecore = false, ult = false, immune = false, dropped = false;
  float floor_k = 0;            // su vida no baja de este tanto (fase)
  float vuln_t = 0;             // aturdido tras romper el Núcleo Eterno: +50 % de daño
  int8_t slot = -1;
  float home_x = 0, home_y = 0;
  float sleep_t = 0;            // dormido (Soporesfera): quieto, sin atacar ni hacer daño al tocarte
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
enum PickKind : int8_t { PK_XP, PK_XPBIG, PK_HEAL, PK_MAGNET, PK_BOMB, PK_COIN, PK_TICKET, PK_TICKET10, PK_ITEM, PK_SPHERE, PK_CANDY, PK_COUNT };   // (PK_CANDY: caramelo de Halloween)   // (PK_SPHERE: value = esfera)
struct Pickup {
  int32_t id = 0;
  int8_t kind = PK_XP;
  float x = 0, y = 0, value = 0, vx = 0, vy = 0, t = 0;
  bool pulled = false;
};

// ---------------- campos (minas, meteoros, torretas, charcos) ----------------
enum FieldKind : int8_t { FK_MINE, FK_METEOR, FK_TURRET, FK_PATCH,
                          // familias nuevas (2026-10-19)
                          FK_CHARGE, FK_SPIN, FK_QUAKE, FK_VORTEX, FK_STUNZONE,
                          // Gran Actualización (13-11-2026)
                          FK_TREMOR, FK_WAVE, FK_CROSS, FK_BOLT, FK_PUNCH, FK_BLITZ, FK_LEAP, FK_DECOY };
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
  // Gran Actualización: de dónde salta (leap), lo que lleva y mide la ola (wave), si el rayo
  // ya cayó (bolt) y a quién ha tocado ya (ids de enemigo; negativos, props)
  float sx = 0, sy = 0, d = 0, len = 0, w = 0;
  bool struck = false;
  uint8_t nhits = 0;
  int32_t hits[40];
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
  bool boom = false;            // la explosión de un kamikaze (Humedad la aguanta)
  uint32_t color = 0;
  // Zona que lo cubre todo (radio r) menos unos círculos (Noche Oscura, Rayo Eternamax)
  bool safe = false;
  int8_t nspots = 0;
  float sx[3] = {}, sy[3] = {}, sr[3] = {};
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
  double judge_dmg = 0;         // daño a Arceus
  bool victory = false;         // modo rápido: ¡has derrotado a Arceus!
  int win_coins = 0, win_t10 = 0;   // su recompensa
  int candy = 0;                    // caramelos de Halloween
  // Maestría (core/mastery.h): la de tu Pokémon antes y después, y los premios nuevos (bits de MasteryTier)
  int16_t m_dex = -1;
  int32_t m_before = 0, m_after = 0, m_gained = 0;
  uint8_t m_got = 0;
  bool wk_prize = false;            // desafío semanal: ticket ×10 de esta semana
  int diff_t1 = 0, diff_t10 = 0;    // tickets de más por la dificultad
  int8_t unlocked = -1;             // dificultad desbloqueada al vencer a Eternatus
  bool hw = false;                  // partida de Halloween (el gran Gengar en vez de Arceus)
  int8_t judge_plate = -1;      // su Tabla (tipo)
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
  bool four_up = false;                  // mejora «Más pasivas a niveles altos» (del port: Save::pfour)
  int16_t pbind[4] = {-1, -1, -1, -1};   // movimientos con hueco fijo
  int16_t pban[3] = {-1, -1, -1};
  int16_t pstart = -1;                    // Movimiento inicial elegido (-1: el de serie)
  int16_t pmove2 = -1;                    // segundo movimiento con «Repertorio» (-1: al azar, como la web)
  uint32_t seed = 1;
  int judged = 0;                        // veces que ha visto la Sentencia (saltar animación)
  // Para grabar (tráiler, también en grupo): empezar con el reloj adelantado y
  // el equipo subido a este nivel (las cartas se eligen como siempre).
  uint16_t demo_time = 0;
  // Modo de juego (del port, 2026-10-26): 0 normal (la Sentencia a los 30 min),
  // 1 rápido (a los 20). En grupo lo elige quien crea la sala.
  uint8_t mode = 0;
  // Dificultad (web, 13-11-2026): 0 Normal, 1 Difícil, 2 Pesadilla (core/diff.h). En grupo, la del anfitrión.
  uint8_t diff = 0;
  uint8_t demo_level = 0;
  // Halloween (core/halloween.h): la partida es del evento (mapa de noche, caramelos,
  // el gran Gengar). En grupo lo decide el anfitrión.
  uint8_t hw = 0;
  // Desafío semanal (core/weekly.h): misma semilla y Pokémon para todos esa semana; no suma maestría.
  uint8_t weekly = 0;
  int8_t wk_cond = -1;                   // su condición (WeeklyCond), sin objetos y el tipo de «todos de tipo X»
  bool wk_items = false;
  int8_t wk_type = -1;
  char wk_week[12] = {};                 // la semana («w2026-47»)
  bool aura = false;                     // aura de maestro (maestría 100 000 con su Pokémon)
  int8_t skin = -1;                      // disfraz puesto (índice de kSkins; -1 ninguno)
  // Tienda Kecleon (core/mons.h): naturaleza, habilidad y MT aprendidas de tu Pokémon
  // (-1 / 0: ni naturaleza ni habilidad, como antes; las pruebas lo dejan así).
  int8_t nature = -1;
  int16_t abil = -1;
  TmBits taught = {};
  uint8_t legends_beaten[(kMaxDex + 8) / 8] = {};   // bit por dex: legendarios ya vencidos (rift.cpp)
  // Cooperativo: cada jugador con su Pokémon y sus mejoras (el 0 es el
  // anfitrión). Con nplayers == 1 se usan los campos de arriba.
  struct Member {
    int dex = 1;
    bool shiny = false;
    uint8_t upgrades[UP_COUNT] = {};
  uint8_t mega_up = 0;                   // mejora «Megapiedra» (del port: Save::pmega)
    bool four_up = false;
    int16_t pbind[4] = {-1, -1, -1, -1};
    int16_t pban[3] = {-1, -1, -1};
    int16_t pstart = -1;
    int16_t pmove2 = -1;
    char name[16] = {};
    // Pruebas: la vida de ESTE jugador nunca baja de 1. Va con los datos de la
    // sala para que todas las consolas lo simulen igual (debug_immortal es de
    // la partida entera).
    bool immortal = false;
    bool test_stone = false;   // pruebas: empieza con su megapiedra puesta
    int8_t skin = -1;          // su disfraz de Halloween (índice de kSkins; -1 ninguno)
    bool aura = false;         // su aura de maestro
    int8_t nature = -1;        // su naturaleza (kNatures; -1: neutra)
    int16_t abil = -1;         // su habilidad (kAbils; -1: ninguna)
    TmBits taught = {};       // sus MT aprendidas (bit = kTms)
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
  bool hw = false;              // partida de Halloween (RunConfig::hw)
  // Bioma número i de la partida (runBiome de tiles.js): en Halloween, los 3 de
  // noche; si no, los de siempre (que van turnándose con los de las arenas, como la web).
  int run_biome(int i) const { i = i < 0 ? 0 : i; return hw ? kBiomeHw + i % 3 : i % kBiomeHw; }
  int candy_dropped = 0;        // caramelos soltados (los que van con el tiempo: candy_take)
  int rifts_won = 0;            // legendarios de grieta vencidos en esta partida (maestría)
  int eterna_wins = 0;          // Eternatus vencido en esta partida (stats.eternaWins y la dificultad siguiente)
  int pending_levels = 0;
  Save* save = nullptr;
  int64_t now_ms = 0;           // reloj de la consola (lo pone la plataforma)
  bool dirty = false;           // hay que guardar
  bool debug_win = false;      // pruebas: en la Sentencia del modo rápido, todo cae deprisa (igual en todas las consolas)
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
  char banner[200] = {};   // (los avisos largos: el de Arceus en el modo rápido, el de cada esfera)
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
  struct CastTier { int16_t from; int n; EnemyDef list[10]; };   // (7 y, en Halloween, 2 Fantasma más)
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
  // Tesoro de la Luminosfera (del port, 2026-10-27): brilla a cierta distancia,
  // una flecha dorada lleva hasta él y al llegar suelta tickets y monedas.
  struct Treasure { bool on = false; float x = 0, y = 0, t = 0; } treasure;

  struct WeatherState { int8_t kind = WX_NONE; float t = 0, dur = 0, check_t = 60, chip_t = 0; } weather;

  struct RiftState {
    bool open = false; float x = 0, y = 0, ttl = 0, t = 0;
    bool arena = false; int32_t tx = 0, ty = 0; float ax = 0, ay = 0, back_x = 0, back_y = 0;
    int16_t dex = 0; bool shiny = false; int8_t type = 0; int back_biome = 0;
    float at = 0; int8_t phase = 0; float end_t = 0; int32_t boss_id = 0;
    float check_t = 30; bool asked = false; float down_t = 0; int misses = 0;
    int n = 0;                  // grietas ganadas en la partida
    int8_t next_i = 0, idx = 0; // la siguiente del calendario y la de ahora
    float cal_t0 = 0, floor_hp = 0; bool cal_done = true;   // calibrar su vida a los 6 s (la anterior, de suelo)
    bool zone = false; float zone_x = 0, zone_y = 0, zone_r = 0;   // dónde estaba la última arena (leftBehind)
  } rift;

  // El legendario de la grieta (core/riftboss.cpp)
  struct RiftBossState {
    bool on = false, landed = false, boom = false, cdone = false, ult_done = false;
    int32_t boss_id = 0;
    int8_t type = T_NORMAL, cat = CAT_PHYSICAL, phase = 1, next = 0, state = 0, last = -1, cast = -1, cn = 0, nkit = 0, nbag = 0, nth = 0;
    struct KM { int8_t pat; const char* name; } kit[6] = {};
    int8_t bag[6] = {};
    float dmg = 0, st = 0, cd = 0, ct = 0, wind = 0, k = 1, ang = 0, tx = 0, ty = 0, jx = 0, jy = 0, spin = 0, dir = 1, emit = 0;
    float wall_off = 0, pull_t = 0, touch_cd = 0, side_t = 0, side = 1, ult_t = 0;
    char cast_name[40] = {};
    struct Threat { int8_t kind = 0, ngaps = 0; bool hit = false; float x = 0, y = 0, R = 0, r = 0, a = 0, t = 0, life = 0, cd = 0; float gaps[3] = {}; } th[8];
  } rboss;

  // Eternatus Eternamax (core/eterna.cpp)
  struct EternaState {
    int8_t st = 0;              // 0 nada, 1 llegando, 2 pelea, 3 ganada (o se retira)
    bool happened = false, won = false;
    float t = 0, ft = 0, wall_t = -1;
    int8_t phase = 0; bool enraged = false;
    int32_t tx = 0, ty = 0; float cx = 0, cy = 0, back_x = 0, back_y = 0; int back_biome = 0;
    int32_t boss_id = 0, core_ids[4] = {}, ucore_id = 0;
    int8_t cast = -1, last = -1, cn = 0, side = -1; bool cdone = false;
    float ct = 0, wind = 0, cd = 0, emit = 0, spin = 0, cdir = 1;
    struct Pool { float x, y, wait, t; } pools[12]; int npools = 0;
    float pool_cd = 0, cloud_cd = 0, laser_cd = 0, touch_cd = 0, pull_t = 0, pull_delay = 0;
    int8_t ult = 0; float ult_t = 0, stun_t = 0;
    char b_title[32] = {}, b_sub[48] = {}, b_info[140] = {}; float b_t = -1;
  } eterna;

  struct JudgmentState {
    int8_t phase = 0;           // 0 nada, 1 animación, 2 pelea
    float ct = 0;
    int32_t tx = 0, ty = 0;
    int16_t q[40]; int nq = 0;
    bool ported = false, landed = false, can_skip = false;
    bool wipe = false;   // (victoria: quitar disparos, peligros y enemigos al final del paso)
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
    // Desde el 23-10-2026 (web): se le puede pegar. Lleva una Tabla al azar (su
    // tipo), recibe una parte del daño (judgment_dmg_k) y, con legendarios vivos,
    // los ataques no le apuntan (shielded).
    int8_t plate = T_FIGHTING;
    // Halloween (judgment.js de la web): el gran Gengar bruja en vez de Arceus
    // (tipo Fantasma, sin Tablas; todo lo demás igual). Va con la partida (Game::hw).
    bool gengar = false;
    double dealt = 0;
    bool shielded = false;
    // Modo rápido (del port, 2026-10-29): acuden 8 legendarios; tumbados todos,
    // Arceus baja la guardia (vida de verdad) y se le puede derrotar.
    bool can_win = false, open = false, won = false;
    float ahp = 0, ahp_max = 0, won_t = 0;
    // Gran Actualización (web, 13-11-2026): Velocidad Extrema, Premonición, Gravedad, Hiperrayo,
    // Sentencia de las Tablas y las Aspas Doradas (dos carriles, a los 5 y a los 7 min).
    float grav_t = 0, last_grav = -99, last_plates = -99;
    int8_t ex_n = 0, ex_i = 0; bool ex_dash = false, ex_go = false; float ex_t = 0, ex_a = 0, ex_len = 0, ex_sx = 0, ex_sy = 0;
    float pl_spin = 0, pl_emit = 0; int pl_ti = 0; int8_t rain_n = 0; float rain_t = 0;
    float hist_t = 0; float hist_x[4][12] = {}, hist_y[4][12] = {}; int8_t hist_n = 0;
    bool lane_on[2] = {}; float spoke_safe = 0;
    bool push_on = false; float push_t = 0, push_x0 = 0, push_y0 = 0, push_x1 = 0, push_y1 = 0; int8_t push_pl = 0;
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
float player_group_k(const Player& p, const MoveInst& m);
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
float spawner_rate(const Game& g, float t);
void enemies_roll_run(Game& g);
// Desafío semanal (core/weekly.h): las condiciones de esta partida
inline bool weekly_is(const Game& g, int cond);
inline bool weekly_noheal(const Game& g);
inline bool weekly_noitems(const Game& g);
// (t = tiempo equivalente de la partida: game_teq)
Scale3 enemies_scale(const Game& g, float t, const EnemyDef* def = nullptr);
Scale3 enemies_boss_scale(const Game& g, float t, const EnemyDef& boss);
float enemies_xp_of(const Game& g, float t, const EnemyDef* def);
int behavior_of(const Mon& p);
EnemyDef enemy_def_for(const Mon& p, int ti);
int enemies_bag_at(Game& g, float t, const EnemyDef** out, int max);
int enemies_tier_at(float t);
int enemies_horde_types(const Game& g, float t, int8_t* out);
int enemies_horde_defs(Game& g, int type, float t, EnemyDef* out);
// hazards.cpp
Hazard* hazard_add(Game& g);
void hazards_update(Game& g, float dt);
bool hazard_inside(const Hazard& h, float x, float y, float r);
// levelup.cpp
// Multiplicador de la experiencia del equipo (coop.js xpFactor; 1 en solitario).
float game_xp_factor(const Game& g);
void levelup_offer(Game& g, Player& p);
// Las 4 cartas (del port, 2026-10-30), con la mejora «Más pasivas a niveles altos»
// (Player::four_up): con kFourMoves movimientos (dos vínculos) o desde el nivel kFourLevel; al menos levelup_min_passive de ellas, pasivas.
constexpr int kFourMoves = 8, kFourLevel = 25, kMorePassiveLevel = 40;
constexpr int kFourUpCost = 2500;   // la mejora (una vez por Pokémon, en Mejoras)
bool levelup_four(const Player& p);
int levelup_min_passive(const Player& p);
void levelup_apply(Game& g, Player& p, const Card& c);
// interact.cpp
void interact_update(Game& g, Player& p, float dt);
void interact_bigchests(Game& g, float dt);   // (una vez por paso, con todos los jugadores)
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
// Fin de la partida (game.cpp): el resumen y, con partida guardada, lo que se guarda.
void game_end_run(Game& g, bool dead);
// riftboss.cpp (el legendario de la grieta pelea de verdad: kit por tipo, 3 fases, ataque definitivo)
void riftboss_arm(Game& g, Enemy& e, float dmg);
void riftboss_update(Game& g, Enemy& e, float dt);
void riftboss_reset(Game& g);
bool riftboss_on(const Game& g);
// eterna.cpp (Eternatus Eternamax, el minuto 20 / el 13 en el modo rápido)
void eterna_reset(Game& g);
void eterna_update(Game& g, float dt);
void eterna_enemy_update(Game& g, Enemy& e, float dt);
bool eterna_on(const Game& g);
bool eterna_intro(const Game& g);
int eterna_barrier(const Game& g);          // barrera de láseres: 1 encendida, -1 avisando, 0 apagada
float eterna_cloud_r(const Game& g);        // radio de las nubes rojas (0: sin nubes)
const char* eterna_cast_name(const Game& g);
// En una arena (grieta o Eternatus): no salen enemigos, ni eventos, ni grietas…
inline bool in_arena(const Game& g) { return g.rift.arena || g.eterna.st != 0; }
// judgment.cpp
constexpr float kJudgmentAt = 30 * 60;
constexpr float kJudgmentAtQuick = 20 * 60;   // modo rápido
enum PlayMode : uint8_t { MODE_NORMAL, MODE_QUICK, MODE_COUNT };
inline float judgment_at(const Game& g) { return g.cfg.mode == MODE_QUICK ? kJudgmentAtQuick : kJudgmentAt; }
// Eternatus Eternamax (web, 13-11-2026): minuto 20; en el modo rápido (del port), el 13.
constexpr float kEternaAt = 20 * 60;
constexpr float kEternaAtQuick = 13 * 60;
inline float eterna_at(const Game& g) { return g.cfg.mode == MODE_QUICK ? kEternaAtQuick : kEternaAt; }
// Tiempo EQUIVALENTE (del port): el equilibrio de la web va por el reloj (curvas de
// core/diff.h, tramos de enemigos, jefes). En el modo rápido todo llega antes y en la
// misma proporción: el minuto 13 pesa como el 20 de la web y el 20, como el 30. A
// cambio la experiencia llega más deprisa (quick_xp_k) para subir lo mismo en menos rato.
inline float teq_of(int mode, float t) {
  if (mode != MODE_QUICK) return t;
  return t <= kEternaAtQuick ? t * (kEternaAt / kEternaAtQuick)
                             : kEternaAt + (t - kEternaAtQuick) * ((kJudgmentAt - kEternaAt) / (kJudgmentAtQuick - kEternaAtQuick));
}
inline float game_teq(const Game& g) { return teq_of(g.cfg.mode, float(g.time)); }
inline float quick_xp_k(const Game& g) { return g.cfg.mode == MODE_QUICK ? kEternaAt / kEternaAtQuick : 1.0f; }
// Lo que tarda algo que en la web pasa en el segundo `at` (jefes, grietas…)
inline float at_of(const Game& g, float at) {
  if (g.cfg.mode != MODE_QUICK) return at;
  return at <= kEternaAt ? at * (kEternaAtQuick / kEternaAt) : kEternaAtQuick + (at - kEternaAt) * ((kJudgmentAtQuick - kEternaAtQuick) / (kJudgmentAt - kEternaAt));
}
void judgment_reset(Game& g);
void judgment_update(Game& g, float dt);
void judgment_cine_tick(Game& g, float dt);
void judgment_note_dmg(Game& g, float d, const Enemy& e);
bool judgment_active(const Game& g);
// Velocidad con tope suave en la Sentencia: lo que pase de 130 cuenta un 25 %.
inline float judgment_cap_spd(float s) { return s <= 130 ? s : 130 + (s - 130) * 0.25f; }
// En el modo rápido (del port, 01-11-2026), más suave: lo que pase de 170 cuenta la
// mitad. El tope de la web es contra la huida sin fin de un Arceus invencible; aquí
// se le puede ganar, y con un Pokémon rápido el de la web quitaba un 30-50 %.
inline float judgment_cap_spd_quick(float s) { return s <= 170 ? s : 170 + (s - 170) * 0.5f; }
bool judgment_cinematic(const Game& g);
// Gravedad de Arceus: clavado al suelo (sin saltos ni embestidas) y más lento.
inline bool judgment_pinned(const Game& g) { return g.judg.phase == 2 && g.judg.grav_t > 0; }
// Las Aspas Doradas (para dibujarlas): posiciones de las del carril `lane` (devuelve cuántas).
int judgment_spokes(const Game& g, int lane, float* xs, float* ys, float* alpha);
// Arceus en la pelea: qué parte del daño recibe (el 5 %, y cada vez menos) y
// si le protegen los legendarios (entonces no se le apunta).
float judgment_dmg_k(const Game& g);
inline bool judgment_shielded(const Game& g) { return g.judg.phase == 2 && g.judg.shielded; }
void judgment_hit_arceus(Game& g, float d);
// Modo rápido: cuántos legendarios acuden, y lo que da derrotar a Arceus.
constexpr int kQuickLegends = 8;
constexpr int kWinCoins = 2500, kWinT10 = 3;
const char* plate_name(int type);
// pathing.cpp
void path_update(Game& g, float dt);
bool path_dir_for(Game& g, Enemy& e, int pl_index, float& ox, float& oy);
void path_clear(Game& g);
// progress.cpp (misiones, logros, estadísticas)
struct KillInfo { const int8_t* types; bool boss; int legend; };
void progress_kill(Game& g, const Enemy& e);
void progress_event(Game& g, int ev, int a = 0, int b = 0, float f = 0);

inline bool weekly_is(const Game& g, int cond) { return g.cfg.weekly && g.cfg.wk_cond == cond; }
inline bool weekly_noheal(const Game& g) { return weekly_is(g, 8 /* WC_NOHEAL */); }
inline bool weekly_noitems(const Game& g) { return g.cfg.weekly && (g.cfg.wk_items || g.cfg.wk_cond == 2 /* WC_NOITEMS */); }

}  // namespace ps

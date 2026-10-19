// gamedata.h — tablas del juego. Los datos los genera tools/gen_data.js a
// partir de reference/web/js/data (build/gen/gamedata.cpp): aquí sólo están
// las estructuras. Nada se copia a mano; si la web cambia, se regenera.
#pragma once
#include <cstdint>

namespace ps {

// ---------------- tipos ----------------
// Mismo orden que Object.keys(G.U.TYPE_NAME) en la web.
enum Type : int8_t {
  T_NONE = -1, T_NORMAL, T_FIRE, T_WATER, T_GRASS, T_ELECTRIC, T_ICE, T_FIGHTING,
  T_POISON, T_GROUND, T_FLYING, T_PSYCHIC, T_BUG, T_ROCK, T_GHOST, T_DRAGON,
  T_DARK, T_STEEL, T_FAIRY, T_COUNT
};

extern const char* const kTypeId[T_COUNT];      // "fire"...
extern const char* const kTypeName[T_COUNT];    // "Fuego"... (español)
extern const uint32_t kTypeColor[T_COUNT];      // 0xRRGGBB
// Multiplicador de los juegos (2, 0.5, 0 o 1) de atacante → defensor.
extern const float kTypeChart[T_COUNT][T_COUNT];

// ---------------- Pokédex ----------------
struct Mon {
  int16_t dex;
  const char* name;
  int8_t types[2];   // T_NONE si sólo tiene uno
  int16_t hp, spd;
  float atk;         // el mayor de pa y sa
  float pa, sa;      // daño de sus ataques físicos / especiales (stats.js)
  int16_t df, sd;    // Defensa y Defensa Especial base de los juegos
  int8_t rarity;
  int16_t bst;
  int8_t gen;
  bool starter;
  bool leg;
};
extern const Mon kDex[];
extern const int kDexCount;
constexpr int kMaxDex = 1025;
// Índice en kDex de cada número de Pokédex (-1 si no existe).
extern const int16_t kDexIndex[kMaxDex + 1];
inline const Mon* mon_by_dex(int dex) {
  return (dex < 0 || dex > kMaxDex || kDexIndex[dex] < 0) ? nullptr : &kDex[kDexIndex[dex]];
}

// ---------------- geometría de los sprites (SPRITE_META) ----------------
enum Anim : int8_t { A_IDLE, A_WALK, A_ATTACK, A_SHOOT, A_CHARGE, A_HURT, A_FAINT, A_COUNT };
extern const char* const kAnimName[A_COUNT];

struct AnimMeta {
  uint8_t w, h;       // fotograma
  uint8_t n, r;       // fotogramas, filas (0 = no existe)
  uint16_t dur;       // índice en kAnimDur (duraciones en 1/60 s)
  float ax, ay;       // ancla al suelo dentro del fotograma
};
struct SpriteMeta {
  uint8_t shadow;     // ShadowSize de PMD (0..2)
  bool shiny;         // tiene versión shiny
  bool hover;         // flota (pasa por encima de rocas y agua)
  int16_t shiny_alt;  // índice en kShinyAlt o -1
  AnimMeta a[A_COUNT];
};
extern const uint8_t kAnimDur[];
extern const SpriteMeta kSpriteMeta[];          // mismo orden que kDex
// Geometría shiny que difiere de la normal (n = 0: igual que la normal).
struct ShinyAlt { AnimMeta a[A_COUNT]; };
extern const ShinyAlt kShinyAlt[];

// ---------------- movimientos ----------------
enum Kind : int8_t {
  K_PROJECTILE, K_MELEE, K_BEAM, K_ORBIT, K_NOVA, K_AURA, K_BOOMERANG, K_CHAIN,
  K_MINE, K_METEOR, K_TURRET, K_DASH, K_TRAIL, K_CONE,
  // desde 2026-10-19 (moves.js)
  K_HOMING, K_BOUNCE, K_CHARGE, K_SPIN, K_QUAKE, K_VORTEX, K_SHOVE, K_STUN,
  K_BUFF, K_COUNT
};
extern const char* const kKindId[K_COUNT];

// Números de un movimiento. `has` dice cuáles existen (en la web, los que
// faltan son undefined: los mods multiplicativos no los tocan).
enum MoveStat : int8_t {
  MS_CD, MS_DMG, MS_SPEED, MS_COUNT, MS_SPREAD, MS_PIERCE, MS_LIFE, MS_SIZE,
  MS_RADIUS, MS_ARC, MS_WIDTH, MS_KNOCK, MS_BURN, MS_POISON, MS_SLOW, MS_HOMING,
  MS_AMOUNT, MS_DUR, MS_STACKS, MS_RATE, MS_RANGE, MS_HEX, MS_DRAIN,
  MS_PULL, MS_BOUNCE, MS_STUN, MS_CHARGE, MS_NUM
};
extern const char* const kMoveStatId[MS_NUM];
// Cómo se aplica cada número en las mejoras: 0 = se asigna, 1 = multiplica, 2 = suma.
extern const uint8_t kMoveStatOp[MS_NUM];

// patk / satk: Danza Espada y Danza Dragón (físico) y Maquinación (especial). atk ya no lo usa ninguno.
enum BuffStat : int8_t { BS_NONE = -1, BS_ATK, BS_PATK, BS_SATK, BS_SPD, BS_DEF, BS_REGEN, BS_COUNT };
// Físico o especial (G.Stats); los potenciadores no tienen.
enum MoveCat : int8_t { CAT_NONE = -1, CAT_PHYSICAL, CAT_SPECIAL };

struct MoveDef {
  const char* id;
  const char* name;
  const char* desc;
  int8_t type;
  int8_t kind;
  int8_t buff;               // BuffStat (sólo los potenciadores)
  int8_t cat;                // MoveCat
  uint32_t has;              // bit i = MoveStat i existe
  float v[MS_NUM];
  int8_t evo_need;           // LevelStat que pide para evolucionar (-1 = no evoluciona)
  const char* evo_name;
};
extern const MoveDef kMoves[];
extern const int kMoveCount;
// Tope de movimientos del port (los ids caben en int8_t). gen_data comprueba
// al compilar que la web no lo pasa; si algún día lo pasa, hay que agrandar
// los int8_t de los movimientos (atajos, vetos...).
constexpr int kMaxMoves = 127;
// Un conjunto de movimientos (bit por id): vetos...
struct MoveBits {
  uint64_t w[2] = {};
  void set(int id) { if (id >= 0 && id < 128) w[id >> 6] |= 1ull << (id & 63); }
  bool has(int id) const { return id >= 0 && id < 128 && ((w[id >> 6] >> (id & 63)) & 1ull); }
};
int move_index(const char* id);   // -1 si no existe

struct Mod { int8_t stat; float val; };
struct Ladder { const char* text; int8_t n; Mod m[5]; };
extern const Ladder kLadder[K_COUNT][4];   // mejoras de nivel 1→5
extern const Ladder kEvoMods[K_COUNT];     // al evolucionar
extern const int8_t kStartMove[T_COUNT];   // movimiento inicial por tipo principal

// Aspecto de cada movimiento (vfx.js): formas y paleta.
enum Shape : int8_t {
  SH_NONE = -1, SH_FIRE, SH_DROP, SH_BUBBLE, SH_LEAF, SH_BOLT, SH_SHARD, SH_ROCK, SH_BLOB,
  SH_NEEDLE, SH_STAR, SH_STREAK, SH_WIND, SH_WEB, SH_RING, SH_CRESCENT, SH_NOTE, SH_IMPACT,
  SH_CLAW, SH_JAWS, SH_ARROW, SH_BALL, SH_SHADOW, SH_HEART, SH_COUNT, SH_LIGHTNING = 100
};
extern const char* const kShapeId[SH_COUNT];
struct ShapeInfo { uint8_t w, h, frames; bool dir, spin; };
extern const ShapeInfo kShapeInfo[SH_COUNT];
enum Pal : int8_t { PAL_AURA = T_COUNT, PAL_COUNT };   // 0..17 = tipos
extern const uint32_t kPal[PAL_COUNT][5];              // contorno, oscuro, medio, claro, brillo
enum Trail : int8_t { TR_NONE, TR_EMBER, TR_DRIP, TR_SPARK, TR_BUBBLES, TR_DIRT, TR_WISP };
enum Melee : int8_t { ME_NONE, ME_IMPACT, ME_JAWS, ME_CLAW, ME_VINE, ME_FEATHER };
struct MoveVis {
  int8_t proj, beam, aura, buff;   // Shape (beam puede ser SH_LIGHTNING)
  int8_t pal, trail, melee;
  bool inward, rise;
};
extern const MoveVis kMoveVis[];                 // mismo orden que kMoves
extern const int8_t kTypeShape[T_COUNT];         // disparo por defecto de cada tipo

// ---------------- subir de nivel ----------------
enum LevelStat : int8_t { LS_HP, LS_ATK, LS_SPD, LS_CD, LS_DEF, LS_RGN, LS_MAG, LS_XP, LS_HEAL, LS_COUNT };
struct LevelStatDef { const char* id; const char* icon; const char* title; const char* desc; int16_t max; };
extern const LevelStatDef kLevelStats[LS_COUNT];
extern const char* const kNeedName[LS_COUNT];    // "Vigor"... (condición de evolución)

// ---------------- objetos equipables ----------------
enum ItemId : int8_t {
  IT_NONE = -1, IT_LEFTOVERS, IT_CHOICEBAND, IT_QUICKCLAW, IT_FOCUSSASH, IT_SHELLBELL,
  IT_AMULETCOIN, IT_LUCKYEGG, IT_LIFEORB, IT_CHOICESCARF, IT_ROCKYHELMET, IT_ASSAULTVEST, IT_COUNT
};
struct ItemDef { const char* id; const char* name; const char* desc; };
extern const ItemDef kItems[IT_COUNT];

// ---------------- mejoras permanentes ----------------
enum UpgradeId : int8_t {
  UP_HP, UP_ATK, UP_SPD, UP_CD, UP_DEF, UP_REGEN, UP_MAGNET, UP_XP, UP_COINS, UP_MOVES,
  UP_CRIT, UP_REROLL, UP_BAN, UP_START, UP_COUNT
};
struct UpgradeDef { const char* id; const char* icon; const char* name; const char* unit; int8_t max; int16_t base; float grow; };
extern const UpgradeDef kUpgrades[UP_COUNT];

// ---------------- enemigos ----------------
struct Tier { int16_t from, bst0, bst1; float hp, spd, dmg, xp; int8_t cast; };
extern const Tier kTiers[];
extern const int kTierCount;
struct BossSlot { int16_t at; int32_t hp; int16_t dmg, xp, bst0, bst1; };
extern const BossSlot kBossSlots[];
extern const int kBossSlotCount;
extern const int16_t kHealDex[], kShieldDex[], kBombDex[];
extern const int kHealCount, kShieldCount, kBombCount;
struct SummonPair { int16_t dex, minion; };
extern const SummonPair kSummon[];
extern const int kSummonCount;

// ---------------- mundo: biomas y arenas ----------------
enum Liquid : int8_t { LQ_WATER, LQ_SWAMP, LQ_LAVA, LQ_POISON, LQ_SPARK, LQ_ICE, LQ_COUNT };
enum PropArt : int8_t { PA_TREE, PA_CRYSTAL, PA_DEAD };
struct Biome {
  const char* name;
  int8_t liquid, prop;
  uint32_t floor[3], dirt[3], speck, light, flowers[3];
  uint32_t wallTop[3], wallLight, wallEdge, wallFace[2], wallFaceDark;
  uint32_t liq, liqDeep, foam, shore;
  uint32_t leaf[4], trunk[2], rock[3], grass[3];
};
extern const Biome kBiomes[];
extern const int kBiomeCount;
constexpr int kNormalBiomes = 3;       // los que se turnan en una run
constexpr int kArenaBiome0 = 3;        // a partir de aquí, las arenas

enum WeatherKind : int8_t { WX_NONE = -1, WX_SUN, WX_RAIN, WX_SAND, WX_SNOW, WX_FOG, WX_COUNT };
struct ArenaTheme {
  const char* id;        // "fire"... y "normal", "judgment"
  const char* name;
  int8_t liquid;
  float pools, pillars;
  float tree, rock, grass;   // densidad de objetos
  int8_t weather;
  float clear;               // claro central (Arceus)
};
extern const ArenaTheme kArenaThemes[];
extern const int kArenaThemeCount;
int arena_theme_index(const char* id);

// ---------------- clima ----------------
struct WeatherDef {
  const char* id; const char* name; const char* icon; const char* hint;
  uint32_t color; uint8_t tint[3]; float tint_a;
  float mult[T_COUNT];
  uint32_t safe;     // bit por tipo
  bool slow;
};
extern const WeatherDef kWeather[WX_COUNT];
extern const float kWeatherBiomeW[3][WX_COUNT];

// ---------------- altares ----------------
struct PowerDef { const char* id; const char* name; const char* desc; uint32_t color; };
extern const PowerDef kPowers[4];

// ---------------- gacha ----------------
struct BannerDef { int8_t gen; const char* name; uint32_t color[3]; };
extern const BannerDef kBanners[9];
extern const float kGachaRates[5];          // por rareza 1..5
extern const int16_t kGachaRefund[6];       // índice = rareza
extern const char* const kRarityName[6];
extern const uint32_t kRarityColor[6];

// ---------------- compañeros y test ----------------
struct StarterDef { int16_t dex; int8_t start; const char* traits[2]; };
extern const StarterDef kStarters[];
extern const int kStarterCount;
struct TraitText { const char* id; const char* text; };
extern const TraitText kTraits[];
extern const int kTraitCount;
struct TestAnswer { const char* text; int8_t n; struct { int8_t trait; int8_t pts; } p[3]; };
struct TestQuestion { const char* q; TestAnswer a[4]; };
extern const TestQuestion kTest[];
extern const int kTestCount;

// ---------------- misiones y logros ----------------
enum ProgEv : int8_t { EV_KILL, EV_CHEST, EV_ITEM, EV_EVOLVE, EV_RIFT, EV_RUNEND, EV_JUDGE, EV_JUDGEEND, EV_MISSIONDONE, EV_COUNT };
struct Reward { int16_t t1, t10, coins; };
struct MissionDef {
  const char* id; int8_t ev; bool weekly; bool max_mode; bool type_param;
  int8_t ngoals; int32_t goals[3]; Reward reward;
};
extern const MissionDef kMissions[];
extern const int kMissionCount;
struct AchDef {
  const char* id; const char* cat; int8_t tier; const char* name; const char* desc;
  int32_t goal; Reward reward; const char* title; int16_t dex;
};
extern const AchDef kAch[];
extern const int kAchCount;
struct AchCat { const char* id; const char* name; };
extern const AchCat kAchCats[];
extern const int kAchCatCount;

// ---------------- versión de los datos ----------------
extern const char* const kWebBase;       // commit de la web de reference/web (7 letras)
extern const char* const kWebBaseDate;   // y su fecha (AAAA-MM-DD)
extern const uint32_t kDataHash;         // huella de todas las tablas generadas

// ---------------- idioma ----------------
struct Tr { const char* es; const char* en; };
extern const Tr kLangEn[];
extern const int kLangEnCount;

}  // namespace ps

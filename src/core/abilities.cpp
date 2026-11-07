// abilities.cpp — ver abilities.h. Port de data/ability-fx.js (web, d667c216):
// mismos números y mismas frases (las traducciones, en lang_en_port.cpp).
#include "core/abilities.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/game.h"
#include "core/types.h"

namespace ps {

namespace {

// Lo que hace cada una (las que son iguales comparten efecto: ver kTable).
enum Fx : int8_t {
  FX_PINCH, FX_BOOST, FX_BYNAME, FX_ABSORB, FX_RUSH, FX_SUMMON, FX_STEADY, FX_MOXIE,
  FX_WATER_BUBBLE, FX_TECHNICIAN, FX_HUGE_POWER, FX_HUSTLE, FX_GUTS, FX_ADAPTABILITY, FX_PROTEAN,
  FX_SHEER_FORCE, FX_SERENE_GRACE, FX_TOUGH_CLAWS, FX_RECKLESS, FX_TINTED_LENS, FX_SCRAPPY, FX_SAND_FORCE,
  FX_SOLAR_POWER, FX_SNIPER, FX_CRIT_UP, FX_SPEED_BOOST, FX_RUN_AWAY, FX_RAIN_DISH, FX_ICE_BODY, FX_DRY_SKIN,
  FX_LEVITATE, FX_THICK_FAT, FX_HEATPROOF, FX_MULTISCALE, FX_FILTER, FX_FUR_COAT, FX_FLUFFY, FX_ICE_SCALES,
  FX_ARMOR, FX_INTIMIDATE, FX_STURDY, FX_REGENERATOR, FX_MAGIC_GUARD, FX_STAMINA,
  FX_STATIC, FX_FLAME_BODY, FX_POISON_POINT, FX_CUTE_CHARM, FX_EFFECT_SPORE, FX_ROUGH_SKIN, FX_POISON_TOUCH,
  FX_PICKUP, FX_TRUANT, FX_SLOW_START, FX_DEFEATIST, FX_WONDER_GUARD, FX_SCHOOLING, FX_ZEN_MODE,
  FX_STANCE_CHANGE, FX_IMPOSTER, FX_MAGNET, FX_SAND_VEIL, FX_SNOW_CLOAK, FX_HEAL_UP, FX_UNNERVE,
  FX_INFILTRATOR, FX_TELEPATHY, FX_FOREWARN, FX_WEAK_ARMOR, FX_STEADFAST, FX_RATTLED, FX_HYDRATION,
  FX_OVERCOAT, FX_SHED_SKIN, FX_NATURAL_CURE, FX_LEAF_GUARD, FX_MOLD_BREAKER, FX_DAMP, FX_RIVALRY,
  FX_SYNCHRONIZE, FX_PRANKSTER, FX_UNBURDEN, FX_UNAWARE, FX_DEFIANT, FX_COMPETITIVE, FX_SOUNDPROOF,
  FX_BIG_PECKS, FX_WATER_VEIL, FX_PICKPOCKET, FX_ANALYTIC, FX_KLUTZ, FX_HYPER_CUTTER, FX_CONTRARY,
  FX_QUICK_FEET, FX_BULLETPROOF, FX_MAGIC_BOUNCE, FX_FRIEND_GUARD, FX_STENCH, FX_ANGER_POINT, FX_JUSTIFIED,
  FX_CURSED_BODY, FX_PLUS, FX_NO_GUARD, FX_AFTERMATH, FX_MOODY, FX_TANGLED_FEET, FX_SKILL_LINK,
  FX_PROTOSYNTHESIS, FX_COUNT
};

// Nombres de movimiento que cuentan para cada «byName» (las expresiones de la web).
const char* const kPunch[] = {"punch", nullptr};
const char* const kJaw[] = {"bite", "fang", "crunch", "jaw", nullptr};
const char* const kPulse[] = {"pulse", "aura-sphere", nullptr};
const char* const kCut[] = {"cut", "slash", "blade", "razor", "scissor", "sword", nullptr};
const char* const kSound[] = {"voice", "buzz", "boom", "noise", "sing", "echo", "uproar", "snarl", nullptr};
const char* const* const kNames[] = {kPunch, kJaw, kPulse, kCut, kSound};
const char* const kNamesEs[] = {"los puñetazos", "los mordiscos", "los pulsos y esferas", "los cortes", "los sonidos"};

struct Row { const char* id; Fx fx; int8_t arg; float k; bool plus; };
const Row kTable[] = {
    // --- más daño ---
    {"overgrow", FX_PINCH, T_GRASS, 0, false}, {"blaze", FX_PINCH, T_FIRE, 0, false},
    {"torrent", FX_PINCH, T_WATER, 0, false}, {"swarm", FX_PINCH, T_BUG, 0, false},
    {"steelworker", FX_BOOST, T_STEEL, 0, false}, {"steely-spirit", FX_BOOST, T_STEEL, 0, false},
    {"dragons-maw", FX_BOOST, T_DRAGON, 0, false}, {"transistor", FX_BOOST, T_ELECTRIC, 0, false},
    {"water-bubble", FX_WATER_BUBBLE, 0, 0, false}, {"technician", FX_TECHNICIAN, 0, 0, false},
    {"huge-power", FX_HUGE_POWER, 0, 0, false}, {"pure-power", FX_HUGE_POWER, 0, 0, false},
    {"hustle", FX_HUSTLE, 0, 0, false}, {"guts", FX_GUTS, 0, 0, false},
    {"adaptability", FX_ADAPTABILITY, 0, 0, false}, {"protean", FX_PROTEAN, 0, 0, false}, {"libero", FX_PROTEAN, 0, 0, false},
    {"sheer-force", FX_SHEER_FORCE, 0, 0, false}, {"serene-grace", FX_SERENE_GRACE, 0, 0, false},
    {"tough-claws", FX_TOUGH_CLAWS, 0, 0, false}, {"reckless", FX_RECKLESS, 0, 0, false},
    {"iron-fist", FX_BYNAME, 0, 1.25f, false}, {"strong-jaw", FX_BYNAME, 1, 1.3f, false},
    {"mega-launcher", FX_BYNAME, 2, 1.3f, false}, {"sharpness", FX_BYNAME, 3, 1.3f, false},
    {"punk-rock", FX_BYNAME, 4, 1.3f, false},
    {"tinted-lens", FX_TINTED_LENS, 0, 0, false}, {"scrappy", FX_SCRAPPY, 0, 0, false},
    {"sand-force", FX_SAND_FORCE, 0, 0, false}, {"solar-power", FX_SOLAR_POWER, 0, 0, false},
    {"sniper", FX_SNIPER, 0, 0, false}, {"super-luck", FX_CRIT_UP, 0, 0.1f, false}, {"compound-eyes", FX_CRIT_UP, 0, 0.06f, false},
    {"moxie", FX_MOXIE, 0, 0, false}, {"chilling-neigh", FX_MOXIE, 0, 0, false}, {"grim-neigh", FX_MOXIE, 0, 0, false},
    {"beast-boost", FX_MOXIE, 0, 0, false},
    // --- velocidad ---
    {"speed-boost", FX_SPEED_BOOST, 0, 0, false},
    {"swift-swim", FX_RUSH, WX_RAIN, 0, false}, {"chlorophyll", FX_RUSH, WX_SUN, 0, false},
    {"sand-rush", FX_RUSH, WX_SAND, 0, false}, {"slush-rush", FX_RUSH, WX_SNOW, 0, false},
    {"run-away", FX_RUN_AWAY, 0, 0, false},
    // --- climas ---
    {"drought", FX_SUMMON, WX_SUN, 0, false}, {"orichalcum-pulse", FX_SUMMON, WX_SUN, 0, false},
    {"drizzle", FX_SUMMON, WX_RAIN, 0, false}, {"sand-stream", FX_SUMMON, WX_SAND, 0, false},
    {"snow-warning", FX_SUMMON, WX_SNOW, 0, false},
    {"rain-dish", FX_RAIN_DISH, 0, 0, false}, {"ice-body", FX_ICE_BODY, 0, 0, false}, {"dry-skin", FX_DRY_SKIN, T_WATER, 0, false},
    // --- defensa ---
    {"levitate", FX_LEVITATE, 0, 0, false}, {"thick-fat", FX_THICK_FAT, 0, 0, false}, {"heatproof", FX_HEATPROOF, 0, 0, false},
    {"water-absorb", FX_ABSORB, T_WATER, 0, false}, {"storm-drain", FX_ABSORB, T_WATER, 0, false},
    {"volt-absorb", FX_ABSORB, T_ELECTRIC, 0, false}, {"lightning-rod", FX_ABSORB, T_ELECTRIC, 0, false},
    {"motor-drive", FX_ABSORB, T_ELECTRIC, 0, false}, {"flash-fire", FX_ABSORB, T_FIRE, 0, false},
    {"well-baked-body", FX_ABSORB, T_FIRE, 0, false}, {"sap-sipper", FX_ABSORB, T_GRASS, 0, false},
    {"earth-eater", FX_ABSORB, T_GROUND, 0, false},
    {"multiscale", FX_MULTISCALE, 0, 0, false}, {"shadow-shield", FX_MULTISCALE, 0, 0, false},
    {"filter", FX_FILTER, 0, 0, false}, {"solid-rock", FX_FILTER, 0, 0, false}, {"prism-armor", FX_FILTER, 0, 0, false},
    {"fur-coat", FX_FUR_COAT, 0, 0, false}, {"fluffy", FX_FLUFFY, 0, 0, false}, {"ice-scales", FX_ICE_SCALES, 0, 0, false},
    {"shell-armor", FX_ARMOR, 0, 0, false}, {"battle-armor", FX_ARMOR, 0, 0, false},
    {"intimidate", FX_INTIMIDATE, 0, 0, false}, {"pressure", FX_ARMOR, 0, 0, false},
    {"sturdy", FX_STURDY, 0, 0, false}, {"regenerator", FX_REGENERATOR, 0, 0, false},
    {"magic-guard", FX_MAGIC_GUARD, 0, 0, false}, {"stamina", FX_STAMINA, 0, 0, false},
    {"limber", FX_STEADY, 0, 0, false}, {"own-tempo", FX_STEADY, 0, 0, false}, {"oblivious", FX_STEADY, 0, 0, false},
    {"inner-focus", FX_STEADY, 0, 0, false}, {"clear-body", FX_STEADY, 0, 0, false}, {"white-smoke", FX_STEADY, 0, 0, false},
    {"full-metal-body", FX_STEADY, 0, 0, false}, {"insomnia", FX_STEADY, 0, 0, false}, {"vital-spirit", FX_STEADY, 0, 0, false},
    // --- contraataques (al recibir un golpe cuerpo a cuerpo) ---
    {"static", FX_STATIC, 0, 0, false}, {"flame-body", FX_FLAME_BODY, 0, 0, false},
    {"poison-point", FX_POISON_POINT, 0, 0, false}, {"cute-charm", FX_CUTE_CHARM, 0, 0, false},
    {"effect-spore", FX_EFFECT_SPORE, 0, 0, false}, {"rough-skin", FX_ROUGH_SKIN, 0, 0, false},
    {"iron-barbs", FX_ROUGH_SKIN, 0, 0, false}, {"poison-touch", FX_POISON_TOUCH, 0, 0, false},
    // --- botín ---
    {"pickup", FX_PICKUP, 0, 0, false}, {"honey-gather", FX_PICKUP, 0, 0, false},
    // --- las que en los juegos son un lastre: aquí, un intercambio ---
    {"truant", FX_TRUANT, 0, 0, false}, {"slow-start", FX_SLOW_START, 0, 0, false},
    {"defeatist", FX_DEFEATIST, 0, 0, false}, {"wonder-guard", FX_WONDER_GUARD, 0, 0, false},
    {"schooling", FX_SCHOOLING, 0, 0, false}, {"zen-mode", FX_ZEN_MODE, 0, 0, true},
    {"stance-change", FX_STANCE_CHANGE, 0, 0, false}, {"imposter", FX_IMPOSTER, 0, 0, false},
    // --- las genéricas más comunes (suman también la pasiva de su tipo) ---
    {"keen-eye", FX_CRIT_UP, 0, 0.05f, true},
    {"frisk", FX_MAGNET, 0, 1.2f, true}, {"magnet-pull", FX_MAGNET, 0, 1.2f, true},
    {"sticky-hold", FX_MAGNET, 0, 1.2f, true}, {"illuminate", FX_MAGNET, 0, 1.1f, true},
    {"sand-veil", FX_SAND_VEIL, 0, 0, true}, {"snow-cloak", FX_SNOW_CLOAK, 0, 0, true},
    {"gluttony", FX_HEAL_UP, 0, 1.4f, true}, {"harvest", FX_HEAL_UP, 0, 1.3f, true},
    {"unnerve", FX_UNNERVE, 0, 0, true}, {"infiltrator", FX_INFILTRATOR, 0, 0, true},
    {"telepathy", FX_TELEPATHY, 0, 0, true}, {"anticipation", FX_FOREWARN, 0, 0, true}, {"forewarn", FX_FOREWARN, 0, 0, true},
    {"weak-armor", FX_WEAK_ARMOR, 0, 0, true}, {"steadfast", FX_STEADFAST, 0, 0, true}, {"rattled", FX_RATTLED, 0, 0, true},
    {"hydration", FX_HYDRATION, 0, 0, true}, {"overcoat", FX_OVERCOAT, 0, 0, true}, {"shed-skin", FX_SHED_SKIN, 0, 0, true},
    {"natural-cure", FX_NATURAL_CURE, 0, 0, true}, {"healer", FX_NATURAL_CURE, 0, 0, true},
    {"leaf-guard", FX_LEAF_GUARD, 0, 0, true}, {"mold-breaker", FX_MOLD_BREAKER, 0, 0, true}, {"damp", FX_DAMP, 0, 0, true},
    {"rivalry", FX_RIVALRY, 0, 0, true}, {"synchronize", FX_SYNCHRONIZE, 0, 0, true}, {"prankster", FX_PRANKSTER, 0, 0, true},
    {"early-bird", FX_STEADY, 0, 0, true}, {"shield-dust", FX_STEADY, 0, 0, true}, {"sweet-veil", FX_STEADY, 0, 0, true},
    {"unburden", FX_UNBURDEN, 0, 0, true}, {"unaware", FX_UNAWARE, 0, 0, true},
    {"defiant", FX_DEFIANT, 0, 0, true}, {"competitive", FX_COMPETITIVE, 0, 0, true},
    {"soundproof", FX_SOUNDPROOF, 0, 0, true}, {"big-pecks", FX_BIG_PECKS, 0, 0, true},
    {"water-veil", FX_WATER_VEIL, 0, 0, true}, {"pickpocket", FX_PICKPOCKET, 0, 0, true},
    {"analytic", FX_ANALYTIC, 0, 0, true}, {"klutz", FX_KLUTZ, 0, 0, true}, {"hyper-cutter", FX_HYPER_CUTTER, 0, 0, true},
    {"contrary", FX_CONTRARY, 0, 0, true}, {"quick-feet", FX_QUICK_FEET, 0, 0, true},
    {"bulletproof", FX_BULLETPROOF, 0, 0, true}, {"magic-bounce", FX_MAGIC_BOUNCE, 0, 0, true},
    {"friend-guard", FX_FRIEND_GUARD, 0, 0, true}, {"stench", FX_STENCH, 0, 0, true},
    {"anger-point", FX_ANGER_POINT, 0, 0, false}, {"justified", FX_JUSTIFIED, 0, 0, true},
    {"cursed-body", FX_CURSED_BODY, 0, 0, false}, {"plus", FX_PLUS, 0, 0, false}, {"minus", FX_PLUS, 0, 0, false},
    {"no-guard", FX_NO_GUARD, 0, 0, false}, {"aftermath", FX_AFTERMATH, 0, 0, true}, {"moody", FX_MOODY, 0, 0, true},
    {"tangled-feet", FX_TANGLED_FEET, 0, 0, true}, {"skill-link", FX_SKILL_LINK, 0, 0, true},
    {"protosynthesis", FX_PROTOSYNTHESIS, 0, 0, true},
};
constexpr int kTableN = int(sizeof(kTable) / sizeof(kTable[0]));

// Índice en kTable de cada habilidad de kAbils (-1: la genérica). Se calcula una vez.
int16_t g_row[512];
bool g_rows_ready = false;
const Row* row_of(int abil) {
  if (!g_rows_ready) {
    for (int i = 0; i < kAbilCount && i < 512; ++i) {
      g_row[i] = -1;
      for (int k = 0; k < kTableN; ++k) if (!std::strcmp(kTable[k].id, kAbils[i].id)) { g_row[i] = int16_t(k); break; }
    }
    g_rows_ready = true;
  }
  return abil >= 0 && abil < kAbilCount && abil < 512 && g_row[abil] >= 0 ? &kTable[g_row[abil]] : nullptr;
}
const Row* row(const Player& p) { return p.abx >= 0 && p.abx < kTableN ? &kTable[p.abx] : nullptr; }

inline int wx(const Game& g) { return g.weather.kind; }
inline bool low(const Player& p) { return p.hp < p.max_hp / 3; }
inline bool half(const Player& p) { return p.hp < p.max_hp / 2; }
inline bool has_type(const int8_t t[2], int x) { return x >= 0 && (t[0] == x || t[1] == x); }
inline bool super_eff(const Player& p, int type) { return type >= 0 && type_mult(type, p.mon->types) > 1; }
inline void heal_k(Player& p, float k) { p.hp = std::fmin(p.max_hp, p.hp + p.max_hp * k); }
inline int dmg_back(const Player& p) { return 10 + p.level * 3; }
bool name_has(const char* id, const char* const* list) {
  for (int i = 0; list[i]; ++i) if (std::strstr(id, list[i])) return true;
  return false;
}
inline bool has_item(const Player& p) { return p.item >= 0; }

// × al daño de la pasiva genérica (+8 % con los de su tipo principal).
inline float generic_out(const Player& p, const MoveInst* m) { return m && p.ab_type >= 0 && m->type() == p.ab_type ? 1.08f : 1.0f; }

// × al daño que hace la habilidad (sin la pasiva genérica).
float own_out(Game& g, Player& p, const Row& r, const MoveInst& m, const Enemy* e) {
  const int cat = m.cat(), type = m.type(), kind = m.kind();
  switch (r.fx) {
    case FX_PINCH: return type == r.arg && low(p) ? 1.35f : 1;
    case FX_BOOST: return type == r.arg ? 1.2f : 1;
    case FX_WATER_BUBBLE: return type == T_WATER ? 1.4f : 1;
    case FX_TECHNICIAN: return kMoves[m.def].v[MS_DMG] <= 12 ? 1.35f : 1;
    case FX_HUGE_POWER: return cat == CAT_PHYSICAL ? 1.5f : 1;
    case FX_HUSTLE: return cat != CAT_PHYSICAL ? 1 : g.rng.next() < 1.0 / 6 ? 0.3f : 1.25f;
    case FX_GUTS: return cat == CAT_PHYSICAL && half(p) ? 1.3f : 1;
    case FX_ADAPTABILITY: return has_type(p.mon->types, type) ? 1.25f : 1;
    case FX_PROTEAN: return 1.12f;
    case FX_SHEER_FORCE: return m.get(MS_BURN) > 0 || m.get(MS_POISON) > 0 || m.get(MS_SLOW) > 0 || m.get(MS_KNOCK) > 0 ? 1.25f : 1;
    case FX_TOUGH_CLAWS: return kind == K_MELEE || kind == K_DASH ? 1.25f : 1;
    case FX_RECKLESS: return kind == K_DASH ? 1.25f : 1;
    case FX_BYNAME: return name_has(kMoves[m.def].id, kNames[r.arg]) ? r.k : 1;
    case FX_TINTED_LENS: { const float k = e ? type_mult(type, e->types) : 1; return k < 1 && k > 0 ? 1 / k : 1; }
    case FX_SCRAPPY:
      return (type == T_NORMAL || type == T_FIGHTING) && e && has_type(e->types, T_GHOST) ? 1 / std::fmax(0.3f, type_mult(type, e->types)) : 1;
    case FX_SAND_FORCE: return wx(g) == WX_SAND && (type == T_ROCK || type == T_GROUND || type == T_STEEL) ? 1.3f : 1;
    case FX_SOLAR_POWER: return wx(g) == WX_SUN && cat == CAT_SPECIAL ? 1.4f : 1;
    case FX_MOXIE: return 1 + p.ab_moxie;
    case FX_TRUANT: return 1.35f;
    case FX_SLOW_START: return p.ab_t < 60 ? 0.6f : 1;
    case FX_DEFEATIST: return half(p) ? 0.75f : 1.15f;
    case FX_SCHOOLING: return p.hp > p.max_hp / 4 ? 1.5f : 1;
    case FX_ZEN_MODE: return half(p) && cat == CAT_SPECIAL ? 1.3f : 1;
    case FX_STANCE_CHANGE: return p.ab_shield ? 1 : 1.2f;
    case FX_MOLD_BREAKER: return e && e->boss ? 1.12f : 1;
    case FX_RIVALRY: return e && (has_type(e->types, p.mon->types[0]) || has_type(e->types, p.mon->types[1])) ? 1.15f : 1;
    case FX_UNBURDEN: return 1;
    case FX_DEFIANT: return p.slow_t > 0 && cat == CAT_PHYSICAL ? 1.2f : 1;
    case FX_COMPETITIVE: return p.slow_t > 0 && cat == CAT_SPECIAL ? 1.2f : 1;
    case FX_ANALYTIC: return e && e->hp < e->max_hp * 0.5f ? 1.15f : 1;
    case FX_KLUTZ: return has_item(p) ? 1 : 1.12f;
    case FX_HYPER_CUTTER: return cat == CAT_PHYSICAL ? 1.06f : 1;
    case FX_ANGER_POINT: return p.ab_angry ? 1.25f : 1;
    case FX_JUSTIFIED: return e && has_type(e->types, T_DARK) ? 1.15f : 1;
    case FX_PLUS: return cat == CAT_SPECIAL ? 1.1f : 1;
    case FX_NO_GUARD: return 1.15f;
    case FX_MOODY: return p.ab_mood == (cat == CAT_SPECIAL ? 1 : 0) ? 1.1f : 1;   // (0 ataque, 1 at. esp., 2 velocidad, 3 defensa)
    case FX_SKILL_LINK: return m.get(MS_COUNT) > 2 || m.get(MS_BOUNCE) > 0 ? 1.25f : 1;
    case FX_PROTOSYNTHESIS: return wx(g) == WX_SUN ? 1.2f : 1;
    default: return 1;
  }
}

// Frase de lo que hace (las de ability-fx.js, en español: tr() las traduce).
void own_desc(const Row& r, char* b, int n) {
  const char* TN = r.arg >= 0 && r.arg < T_COUNT ? kTypeName[r.arg] : "";
  static const char* const WXN[] = {"sol", "lluvia", "tormenta de arena", "nieve"};
  static const char* const WXS[] = {"sol", "lluvia", "una tormenta de arena", "nieve"};
  const char* s = "";
  switch (r.fx) {
    case FX_PINCH: std::snprintf(b, size_t(n), "+35 %% de daño de tipo %s con poca vida (menos de 1/3).", TN); return;
    case FX_BOOST: std::snprintf(b, size_t(n), "+20 %% de daño con movimientos de tipo %s.", TN); return;
    case FX_BYNAME: std::snprintf(b, size_t(n), "+%d %% de daño con %s.", int(std::lround((r.k - 1) * 100)), kNamesEs[r.arg]); return;
    case FX_ABSORB: std::snprintf(b, size_t(n), "Los ataques de tipo %s no le hacen daño: le curan un poco.", TN); return;
    case FX_RUSH: std::snprintf(b, size_t(n), "+50 %% de velocidad con %s.", WXN[r.arg]); return;
    case FX_SUMMON: std::snprintf(b, size_t(n), "Pone %s al empezar la partida y cada 3 minutos.", WXS[r.arg]); return;
    case FX_CRIT_UP: std::snprintf(b, size_t(n), "+%d %% de probabilidad de golpe crítico.", int(std::lround(r.k * 100))); return;
    case FX_MAGNET: std::snprintf(b, size_t(n), "Atrae los objetos desde más lejos (+%d %% de imán).", int(std::lround((r.k - 1) * 100))); return;
    case FX_HEAL_UP: std::snprintf(b, size_t(n), "Las curas le curan un %d %% más.", int(std::lround((r.k - 1) * 100))); return;
    case FX_STEADY: s = "No le frenan las trampas pegajosas ni el terreno."; break;
    case FX_MOXIE: s = "Cada enemigo derrotado le da +0,5 % de daño (hasta +15 %)."; break;
    case FX_WATER_BUBBLE: s = "+40 % de daño de tipo Agua y recibe la mitad de daño de Fuego."; break;
    case FX_TECHNICIAN: s = "+35 % de daño con los ataques de golpes flojos."; break;
    case FX_HUGE_POWER: s = "+50 % de daño con movimientos físicos."; break;
    case FX_HUSTLE: s = "+25 % de daño con movimientos físicos, pero 1 de cada 6 golpes físicos sale flojo."; break;
    case FX_GUTS: s = "+30 % de daño físico con menos de la mitad de vida."; break;
    case FX_ADAPTABILITY: s = "+25 % de daño con los movimientos de sus tipos."; break;
    case FX_PROTEAN: s = "+12 % de daño con todos sus movimientos."; break;
    case FX_SHEER_FORCE: s = "+25 % de daño con movimientos que queman, envenenan, frenan o empujan, pero pierden ese efecto."; break;
    case FX_SERENE_GRACE: s = "Las quemaduras, venenos y frenazos de sus movimientos son el doble de fuertes."; break;
    case FX_TOUGH_CLAWS: s = "+25 % de daño con ataques cuerpo a cuerpo y embestidas."; break;
    case FX_RECKLESS: s = "+25 % de daño con las embestidas."; break;
    case FX_TINTED_LENS: s = "Sus movimientos poco eficaces hacen daño normal."; break;
    case FX_SCRAPPY: s = "Sus movimientos Normal y Lucha dañan del todo a los Fantasma."; break;
    case FX_SAND_FORCE: s = "+30 % de daño Roca, Tierra y Acero con tormenta de arena."; break;
    case FX_SOLAR_POWER: s = "+40 % de daño especial con sol, pero el sol le quita algo de vida."; break;
    case FX_SNIPER: s = "Sus golpes críticos hacen ×2 de daño (en vez de ×1,5)."; break;
    case FX_SPEED_BOOST: s = "Cada 10 segundos va un 1,5 % más rápido (hasta +30 %)."; break;
    case FX_RUN_AWAY: s = "+8 % de velocidad."; break;
    case FX_RAIN_DISH: s = "Con lluvia recupera un 1,5 % de vida por segundo."; break;
    case FX_ICE_BODY: s = "Con nieve recupera un 1,5 % de vida por segundo."; break;
    case FX_DRY_SKIN: s = "El Agua le cura y con lluvia recupera vida, pero el Fuego le hace un 25 % más."; break;
    case FX_LEVITATE: s = "Flota: no le afectan el agua, la lava ni las trampas, y la Tierra le hace muy poco."; break;
    case FX_THICK_FAT: s = "Recibe un 40 % menos de daño de Fuego y Hielo."; break;
    case FX_HEATPROOF: s = "Recibe la mitad de daño de Fuego."; break;
    case FX_MULTISCALE: s = "Con la vida casi llena recibe la mitad de daño."; break;
    case FX_FILTER: s = "Los ataques muy eficaces le hacen un 25 % menos."; break;
    case FX_FUR_COAT: s = "Recibe un 40 % menos de daño físico."; break;
    case FX_FLUFFY: s = "Recibe un 30 % menos de daño físico."; break;
    case FX_ICE_SCALES: s = "Recibe un 40 % menos de daño especial."; break;
    case FX_ARMOR: s = "Recibe un 8 % menos de daño."; break;
    case FX_INTIMIDATE: s = "Los enemigos le hacen un 12 % menos de daño físico."; break;
    case FX_STURDY: s = "Un golpe que le dejaría KO le deja con 1 de vida (cada 90 s)."; break;
    case FX_REGENERATOR: s = "Recupera un 0,6 % de su vida por segundo."; break;
    case FX_MAGIC_GUARD: s = "Sólo le hacen daño los ataques: ni la lava ni las trampas."; break;
    case FX_STAMINA: s = "Cada golpe cuerpo a cuerpo que recibe le sube un poco la defensa."; break;
    case FX_STATIC: s = "Quien le golpea cuerpo a cuerpo se queda frenado (paralizado)."; break;
    case FX_FLAME_BODY: s = "Quien le golpea cuerpo a cuerpo se quema."; break;
    case FX_POISON_POINT: s = "Quien le golpea cuerpo a cuerpo se envenena."; break;
    case FX_CUTE_CHARM: s = "Quien le golpea cuerpo a cuerpo se queda embobado (frenado)."; break;
    case FX_EFFECT_SPORE: s = "Quien le golpea cuerpo a cuerpo se frena o se envenena."; break;
    case FX_ROUGH_SKIN: s = "Quien le golpea cuerpo a cuerpo recibe daño."; break;
    case FX_POISON_TOUCH: s = "Sus ataques cuerpo a cuerpo y embestidas envenenan."; break;
    case FX_PICKUP: s = "+15 % de Pokémonedas en la partida."; break;
    case FX_TRUANT: s = "Golpes sueltos y fuertes: tarda un 60 % más en recargar, pero cada golpe hace +35 %."; break;
    case FX_SLOW_START: s = "Arranca lento: el primer minuto hace −40 % de daño y va más despacio; después, a plena potencia."; break;
    case FX_DEFEATIST: s = "Cañón de cristal: +15 % de daño con más de la mitad de vida; con menos, −25 %."; break;
    case FX_WONDER_GUARD: s = "Lo que no es muy eficaz apenas le hace daño (35 %); lo muy eficaz, un 50 % más."; break;
    case FX_SCHOOLING: s = "Forma banco: +25 % de vida y, con más de 1/4 de vida, +50 % de daño."; break;
    case FX_ZEN_MODE: s = "Con menos de la mitad de vida entra en Modo Daruma: +30 % de daño especial."; break;
    case FX_STANCE_CHANGE: s = "Alterna cada 8 s entre filo (+20 % de daño) y escudo (−20 % de daño recibido)."; break;
    case FX_IMPOSTER: s = "Se transforma: pega y aguanta como un Pokémon equilibrado."; break;
    case FX_SAND_VEIL: s = "Con tormenta de arena esquiva el 15 % de los golpes."; break;
    case FX_SNOW_CLOAK: s = "Con nieve esquiva el 15 % de los golpes."; break;
    case FX_UNNERVE: s = "Pone nerviosos a los enemigos: le hacen un 6 % menos de daño."; break;
    case FX_INFILTRATOR: s = "Sus golpes rompen los escudos de los enemigos."; break;
    case FX_TELEPATHY: s = "Lee las intenciones: esquiva el 8 % de los golpes."; break;
    case FX_FOREWARN: s = "Presiente el peligro: esquiva el 6 % de los golpes."; break;
    case FX_WEAK_ARMOR: s = "Al recibir un golpe físico se aligera: +20 % de velocidad un momento."; break;
    case FX_STEADFAST: s = "Al recibir un golpe, +10 % de velocidad un momento (se acumula)."; break;
    case FX_RATTLED: s = "Los golpes de Bicho, Fantasma o Siniestro le asustan: +25 % de velocidad un momento."; break;
    case FX_HYDRATION: s = "Con lluvia recupera un 1 % de vida por segundo y nada le frena."; break;
    case FX_OVERCOAT: s = "Recibe un 5 % menos de daño especial."; break;
    case FX_SHED_SKIN: s = "Cada 5 s muda: se quita los frenazos y recupera un 2 % de vida."; break;
    case FX_NATURAL_CURE: s = "Recupera un 0,3 % de su vida por segundo."; break;
    case FX_LEAF_GUARD: s = "Con sol no le frena nada y recibe un 15 % menos de daño."; break;
    case FX_MOLD_BREAKER: s = "+12 % de daño a los jefes."; break;
    case FX_DAMP: s = "Las explosiones de los Pokémon kamikaze no le dañan."; break;
    case FX_RIVALRY: s = "+15 % de daño a los enemigos que comparten un tipo con él."; break;
    case FX_SYNCHRONIZE: s = "Quien le golpea cuerpo a cuerpo recibe parte del golpe."; break;
    case FX_PRANKSTER: s = "Sus potenciadores se recargan un 30 % antes."; break;
    case FX_UNBURDEN: s = "Sin objeto va un 15 % más rápido."; break;
    case FX_UNAWARE: s = "No se deja impresionar: recibe un 8 % menos de daño."; break;
    case FX_DEFIANT: s = "Si le frenan, se pica: +20 % de daño físico mientras dure."; break;
    case FX_COMPETITIVE: s = "Si le frenan, se pica: +20 % de daño especial mientras dure."; break;
    case FX_SOUNDPROOF: s = "Recibe un 10 % menos de daño especial."; break;
    case FX_BIG_PECKS: s = "Recibe un 8 % menos de daño físico."; break;
    case FX_WATER_VEIL: s = "La lava no le quema y recibe la mitad de daño de Fuego."; break;
    case FX_PICKPOCKET: s = "Al recibir golpes, a veces les quita una moneda."; break;
    case FX_ANALYTIC: s = "+15 % de daño a enemigos con menos de la mitad de vida."; break;
    case FX_KLUTZ: s = "Sin objeto, +12 % de daño."; break;
    case FX_HYPER_CUTTER: s = "+6 % de daño con movimientos físicos."; break;
    case FX_CONTRARY: s = "Al revés: frenado, va más rápido."; break;
    case FX_QUICK_FEET: s = "Frenado, va más rápido (en vez de más lento)."; break;
    case FX_BULLETPROOF: s = "Recibe un 15 % menos de daño especial."; break;
    case FX_MAGIC_BOUNCE: s = "Recibe un 15 % menos de daño especial y esquiva el 10 % de esos golpes."; break;
    case FX_FRIEND_GUARD: s = "Recibe un 6 % menos de daño."; break;
    case FX_STENCH: s = "Sus golpes a veces frenan (1 de cada 10)."; break;
    case FX_ANGER_POINT: s = "La primera vez que baja de la mitad de vida se enfurece: +25 % de daño el resto de la partida."; break;
    case FX_JUSTIFIED: s = "+15 % de daño a los Pokémon de tipo Siniestro."; break;
    case FX_CURSED_BODY: s = "A veces frena a quien le golpea cuerpo a cuerpo."; break;
    case FX_PLUS: s = "+10 % de daño con movimientos especiales."; break;
    case FX_NO_GUARD: s = "Todo acierta: +15 % de daño hecho, pero recibe un 10 % más."; break;
    case FX_AFTERMATH: s = "Una vez por partida, al quedar con poca vida, explota y daña a los de alrededor."; break;
    case FX_MOODY: s = "Cada 10 s, +10 % a algo al azar: ataque, ataque especial, velocidad o defensa."; break;
    case FX_TANGLED_FEET: s = "Frenado, esquiva el 20 % de los golpes."; break;
    case FX_SKILL_LINK: s = "+25 % de daño con los movimientos de varios golpes."; break;
    case FX_PROTOSYNTHESIS: s = "Con sol, +20 % de daño."; break;
    default: break;
  }
  std::snprintf(b, size_t(n), "%s", s);
}

void generic_desc(int type, char* b, int n) {
  std::snprintf(b, size_t(n), "+8 %% de daño con movimientos de tipo %s.", type >= 0 && type < T_COUNT ? kTypeName[type] : "Normal");
}

}  // namespace

int abil_fx(int abil) {
  const Row* r = row_of(abil);
  return r ? int(r - kTable) : -1;
}

int abil_fx_rows() { return kTableN; }
const char* abil_fx_id(int row) { return row >= 0 && row < kTableN ? kTable[row].id : ""; }

bool abil_plus(int abil) {
  const Row* r = row_of(abil);
  return r && r->plus;
}

void abil_desc(int abil, const Mon* mon, char* d1, int n1, char* d2, int n2) {
  if (n1 > 0) d1[0] = 0;
  if (n2 > 0) d2[0] = 0;
  const int type = mon ? int(mon->types[0]) : int(T_NORMAL);
  const Row* r = row_of(abil);
  if (!r) { generic_desc(type, d1, n1); return; }
  own_desc(*r, d1, n1);
  if (r->plus) generic_desc(type, d2, n2);
}

void abil_setup(Game& g, Player& p, int abil) {
  (void)g;
  p.abil = int16_t(abil);
  p.abx = -1;
  p.ab_type = T_NONE;
  if (abil < 0 || abil >= kAbilCount) return;   // (sin habilidad: como antes, para las pruebas)
  p.abx = int16_t(abil_fx(abil));
  p.ab_type = p.mon ? p.mon->types[0] : int8_t(T_NONE);
  const Row* r = row(p);
  if (!r) return;
  switch (r->fx) {
    case FX_TRUANT: p.cd_mul *= 1.6f; break;
    case FX_CRIT_UP: p.crit += r->k; break;
    case FX_REGENERATOR: p.regen_flat += p.max_hp * 0.006f; break;
    case FX_NATURAL_CURE: p.regen_flat += p.max_hp * 0.003f; break;
    case FX_PICKUP: p.coin_mul *= 1.15f; break;
    case FX_MAGNET: p.magnet *= r->k; break;
    case FX_SCHOOLING: p.max_hp = float(std::round(double(p.max_hp) * 1.25)); p.hp = p.max_hp; break;
    case FX_IMPOSTER: {
      // (su copia del Pokémon: kDex no se toca)
      Mon& m = p.mon_buf;
      const float a = std::fmax(m.atk, 1.12f);
      m.pa = m.sa = m.atk = a;
      p.base_atk = a;
      m.df = std::fmax(m.df, 72.0f);
      m.sd = std::fmax(m.sd, 72.0f);
      break;
    }
    default: break;
  }
}

float abil_out(Game& g, Player& p, const MoveInst* m, const Enemy* e) {
  if (p.abil < 0 || !m) return 1;
  const Row* r = row(p);
  if (!r) return generic_out(p, m);
  const float k = own_out(g, p, *r, *m, e);
  return r->plus ? k * generic_out(p, m) : k;
}

void abil_after_hit(Game& g, Player& p, Enemy& e, const MoveInst* m) {
  const Row* r = row(p);
  if (!r || e.dead) return;
  switch (r->fx) {
    case FX_INFILTRATOR: if (e.shield > 0) { e.shield = 0; e.shield_t = 0; } break;
    case FX_STENCH: if (g.rng.next() < 0.1) enemy_apply_slow(e, 0.35f, 1.5f); break;
    case FX_POISON_TOUCH: if (m && (m->kind() == K_MELEE || m->kind() == K_DASH)) enemy_apply_poison(e, 1); break;
    default: break;
  }
}

float abil_crit_mul(const Player& p) { const Row* r = row(p); return r && r->fx == FX_SNIPER ? 2.0f : 1.5f; }

float abil_status_k(const Player& p) {
  const Row* r = row(p);
  return !r ? 1.0f : r->fx == FX_SHEER_FORCE ? 0.0f : r->fx == FX_SERENE_GRACE ? 2.0f : 1.0f;
}

float abil_status_cd(const Player& p) { const Row* r = row(p); return r && r->fx == FX_PRANKSTER ? 0.7f : 1.0f; }

float abil_heal_mul(const Player& p) { const Row* r = row(p); return r && r->fx == FX_HEAL_UP ? r->k : 1.0f; }

bool abil_absorbs(const Player& p, int type) {
  const Row* r = row(p);
  if (!r || type < 0) return false;
  if (r->fx == FX_ABSORB) return type == r->arg;
  if (r->fx == FX_DRY_SKIN) return type == T_WATER;
  return false;
}

float abil_dodge(Game& g, const Player& p, int type, int cat) {
  (void)type;
  const Row* r = row(p);
  if (!r) return 0;
  switch (r->fx) {
    case FX_SAND_VEIL: return wx(g) == WX_SAND ? 0.15f : 0;
    case FX_SNOW_CLOAK: return wx(g) == WX_SNOW ? 0.15f : 0;
    case FX_TELEPATHY: return 0.08f;
    case FX_FOREWARN: return 0.06f;
    case FX_MAGIC_BOUNCE: return cat == CAT_SPECIAL ? 0.1f : 0;
    case FX_TANGLED_FEET: return p.slow_t > 0 ? 0.2f : 0;
    default: return 0;
  }
}

float abil_in_mul(Game& g, const Player& p, int type, int cat) {
  const Row* r = row(p);
  if (!r) return 1;
  switch (r->fx) {
    case FX_WATER_BUBBLE: case FX_HEATPROOF: case FX_WATER_VEIL: return type == T_FIRE ? 0.5f : 1;
    case FX_DRY_SKIN: return type == T_FIRE ? 1.25f : 1;
    case FX_LEVITATE: return type == T_GROUND ? 0.3f : 1;
    case FX_THICK_FAT: return type == T_FIRE || type == T_ICE ? 0.6f : 1;
    case FX_MULTISCALE: return p.hp >= p.max_hp * 0.95f ? 0.5f : 1;
    case FX_FILTER: return super_eff(p, type) ? 0.75f : 1;
    case FX_FUR_COAT: return cat == CAT_PHYSICAL ? 0.6f : 1;
    case FX_FLUFFY: return cat == CAT_PHYSICAL ? 0.7f : 1;
    case FX_ICE_SCALES: return cat == CAT_SPECIAL ? 0.6f : 1;
    case FX_ARMOR: case FX_UNAWARE: return 0.92f;
    case FX_INTIMIDATE: return cat == CAT_PHYSICAL ? 0.88f : 1;
    case FX_WONDER_GUARD: return super_eff(p, type) ? 1.5f : 0.35f;
    case FX_STANCE_CHANGE: return p.ab_shield ? 0.8f : 1;
    case FX_UNNERVE: case FX_FRIEND_GUARD: return 0.94f;
    case FX_OVERCOAT: return cat == CAT_SPECIAL ? 0.95f : 1;
    case FX_LEAF_GUARD: return wx(g) == WX_SUN ? 0.85f : 1;
    case FX_SOUNDPROOF: return cat == CAT_SPECIAL ? 0.9f : 1;
    case FX_BIG_PECKS: return cat == CAT_PHYSICAL ? 0.92f : 1;
    case FX_BULLETPROOF: case FX_MAGIC_BOUNCE: return cat == CAT_SPECIAL ? 0.85f : 1;
    case FX_NO_GUARD: return 1.1f;
    case FX_MOODY: return p.ab_mood == 3 ? 0.9f : 1;
    default: return 1;
  }
}

void abil_hurt(Game& g, Player& p, int type, int cat) {
  const Row* r = row(p);
  if (!r) return;
  switch (r->fx) {
    case FX_WEAK_ARMOR: if (cat == CAT_PHYSICAL) player_add_buff(p, BS_SPD, 0.2f, 3, 1); break;
    case FX_STEADFAST: player_add_buff(p, BS_SPD, 0.1f, 3, 2); break;
    case FX_RATTLED: if (type == T_BUG || type == T_GHOST || type == T_DARK) player_add_buff(p, BS_SPD, 0.25f, 4, 1); break;
    case FX_PICKPOCKET:
      if (g.rng.next() < 0.12) pickup_drop(g, PK_COIN, p.x + g.rng.range(-10, 10), p.y + g.rng.range(-10, 10), 2);
      break;
    default: break;
  }
}

bool abil_sturdy(const Player& p) { const Row* r = row(p); return r && r->fx == FX_STURDY; }

void abil_contact(Game& g, Player& p, Enemy& e) {
  const Row* r = row(p);
  if (!r) return;
  switch (r->fx) {
    case FX_STAMINA: player_add_buff(p, BS_DEF, 0.03f, 6, 5); break;
    case FX_STATIC: enemy_apply_slow(e, 0.5f, 2.5f); break;
    case FX_FLAME_BODY: enemy_apply_burn(e, 2); break;
    case FX_POISON_POINT: enemy_apply_poison(e, 2); break;
    case FX_CUTE_CHARM: enemy_apply_slow(e, 0.4f, 2.5f); break;
    case FX_EFFECT_SPORE: if (g.rng.next() < 0.5) enemy_apply_slow(e, 0.45f, 2.5f); else enemy_apply_poison(e, 2); break;
    case FX_ROUGH_SKIN: enemy_hurt(g, e, float(dmg_back(p)), 0x9aa6b5); break;
    case FX_SYNCHRONIZE: enemy_hurt(g, e, float(6 + p.level * 2), 0xff9fcf); break;
    case FX_CURSED_BODY: if (g.rng.next() < 0.3) enemy_apply_slow(e, 0.5f, 2); break;
    default: break;
  }
}

void abil_tick(Game& g, Player& p, float dt) {
  const Row* r = row(p);
  if (!r) return;
  switch (r->fx) {
    case FX_SOLAR_POWER: if (wx(g) == WX_SUN && p.hp > p.max_hp * 0.1f) p.hp -= p.max_hp * 0.004f * dt; break;
    case FX_SPEED_BOOST:
      p.ab_t += dt;
      if (p.ab_t >= 10) { p.ab_t = 0; p.ab_spd = std::fmin(0.3f, p.ab_spd + 0.015f); }
      break;
    case FX_RAIN_DISH: if (wx(g) == WX_RAIN) heal_k(p, 0.015f * dt); break;
    case FX_ICE_BODY: if (wx(g) == WX_SNOW) heal_k(p, 0.015f * dt); break;
    case FX_DRY_SKIN: if (wx(g) == WX_RAIN) heal_k(p, 0.012f * dt); break;
    case FX_SLOW_START: p.ab_t += dt; break;
    case FX_STANCE_CHANGE:
      p.ab_t += dt;
      if (p.ab_t >= 8) { p.ab_t = 0; p.ab_shield = !p.ab_shield; }
      break;
    case FX_HYDRATION: if (wx(g) == WX_RAIN) { heal_k(p, 0.01f * dt); p.slow_t = 0; } break;
    case FX_SHED_SKIN:
      p.ab_t += dt;
      if (p.ab_t >= 5) { p.ab_t = 0; p.slow_t = 0; heal_k(p, 0.02f); }
      break;
    case FX_LEAF_GUARD: if (wx(g) == WX_SUN) p.slow_t = 0; break;
    case FX_ANGER_POINT:
      if (!p.ab_angry && half(p)) { p.ab_angry = true; g.fx.dmg_text(p.x, p.y - p.body_h - 16, "¡Irascible!", 0xff6b4d, true); }
      break;
    case FX_AFTERMATH:
      if (!p.ab_boomed && p.hp < p.max_hp * 0.2f && !p.dead) {
        p.ab_boomed = true;
        g.fx.ring(p.x, p.y, 10, 140, 0xff7b3d, 0.5f, 6);
        g.camera.kick(0.6f);
        g.sfx(SFX_BREAK);
        Enemy* list[kMaxEnemies];
        const int n = g.query_circle(p.x, p.y, 140, list, kMaxEnemies);
        int32_t ids[kMaxEnemies];
        for (int k = 0; k < n; ++k) ids[k] = list[k]->id;
        for (int k = 0; k < n; ++k) {
          Enemy* e = g.enemy_by_id(ids[k]);
          if (e && !e->dead) enemy_hurt(g, *e, 40.0f + p.level * 10, 0xff7b3d, e->x - p.x, e->y - p.y, 160);
        }
      }
      break;
    case FX_MOODY: {
      p.ab_t -= dt;
      if (p.ab_t > 0) break;
      p.ab_t = 10;
      p.ab_mood = int8_t(std::min(3, int(g.rng.next() * 4)));
      static const uint32_t C[4] = {0xff6b4d, 0xff8ad8, 0x7fe0ff, 0xb0b8c8};
      g.fx.ring(p.x, p.y, p.r * 2.4f, p.r * 1.1f, C[p.ab_mood], 0.35f, 2.5f);
      break;
    }
    default: break;
  }
  // Sequía, Llovizna...: su clima al empezar y cada 3 min, si no hay otro.
  if (r->fx == FX_SUMMON && (p.ab_sum -= dt) <= 0) {
    p.ab_sum = 180;
    if (g.weather.kind < 0) weather_force(g, r->arg, 60);
  }
}

void abil_kill(Player& p) {
  const Row* r = row(p);
  if (r && r->fx == FX_MOXIE) p.ab_moxie = std::fmin(0.15f, p.ab_moxie + 0.005f);
}

float abil_speed(Game& g, const Player& p) {
  const Row* r = row(p);
  if (!r) return 1;
  switch (r->fx) {
    case FX_SPEED_BOOST: return 1 + p.ab_spd;
    case FX_RUSH: return wx(g) == r->arg ? 1.5f : 1;
    case FX_RUN_AWAY: return 1.08f;
    case FX_SLOW_START: return p.ab_t < 60 ? 0.8f : 1;
    case FX_UNBURDEN: return has_item(p) ? 1 : 1.15f;
    case FX_CONTRARY: return p.slow_t > 0 ? 2.4f : 1;
    case FX_QUICK_FEET: return p.slow_t > 0 ? 2.5f : 1;
    case FX_MOODY: return p.ab_mood == 2 ? 1.1f : 1;
    default: return 1;
  }
}

bool abil_steady(const Player& p) { const Row* r = row(p); return r && r->fx == FX_STEADY; }
bool abil_levitate(const Player& p) { const Row* r = row(p); return r && r->fx == FX_LEVITATE; }
bool abil_lava_proof(const Player& p) { const Row* r = row(p); return r && (r->fx == FX_MAGIC_GUARD || r->fx == FX_WATER_VEIL); }
bool abil_trap_proof(const Player& p) { const Row* r = row(p); return r && r->fx == FX_MAGIC_GUARD; }
bool abil_damp(const Player& p) { const Row* r = row(p); return r && r->fx == FX_DAMP; }

}  // namespace ps

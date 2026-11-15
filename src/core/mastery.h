#pragma once
// Maestría Pokémon (web, Gran Actualización 13-11-2026, mastery.js).
//
// Cada partida (sola o en grupo; el desafío semanal no cuenta) suma puntos al
// Pokémon con el que se empezó: uno por cada 6 derrotados y 25 por minuto
// aguantado (como mucho 40 minutos). Con los puntos llegan los premios:
//   10 000 foto de maestría · 25 000 título «Maestro de X» · 50 000 marco ·
//   100 000 aura de maestro (destellos dorados en la partida).
// La foto, el título y el marco son del perfil de la web: en la 3DS se ven en la
// Colección y llegan a la web al sincronizar. El aura sí se ve en la partida.
//
// También guarda, por Pokémon, estadísticas de sus partidas (pstat de la web):
// r partidas, s segundos, k derrotados, a Sentencias, bt mejor tiempo, ad daño a
// Arceus (el mayor), b jefes, l legendarios de grieta, e Eternatus. Las de bt y ad
// se quedan con el mayor; las demás suman. Cada una con su tope (STAT_KEYS).
#include <cstdint>

namespace ps {

enum MasteryTier : int8_t { MT_PHOTO, MT_TITLE, MT_FRAME, MT_AURA, MT_COUNT };
struct MasteryTierDef { const char* id; int32_t at; const char* name; };
extern const MasteryTierDef kMasteryTiers[MT_COUNT];
constexpr int32_t kMasteryMax = 100000;
constexpr int kMasteryKills = 6, kMasteryPerMin = 25, kMasteryRunMaxS = 2400;

// Puntos de una partida
int32_t mastery_run_points(int kills, double secs);
// Premios conseguidos (0..4) con esos puntos
int mastery_level(int32_t pts);
// El siguiente premio (-1: todos)
int mastery_next(int32_t pts);
bool mastery_has(int32_t pts, int tier);

enum PStatKey : int8_t { PS_R, PS_S, PS_K, PS_A, PS_BT, PS_AD, PS_B, PS_L, PS_E, PS_COUNT };
extern const char* const kPStatId[PS_COUNT];
extern const double kPStatMax[PS_COUNT];
inline bool pstat_is_max(int k) { return k == PS_BT || k == PS_AD; }
struct PStat { double v[PS_COUNT] = {}; };
// Suma una partida (addStats)
void pstat_add(PStat& p, const double (&run)[PS_COUNT]);

// Texto del premio para el resumen («Foto de maestría de Pikachu»…)
void mastery_reward_text(int tier, const char* mon_name, char* out, int cap);

}  // namespace ps

// diff.h — dificultad y equilibrio (difficulty.js de la web, Gran Actualización 13-11-2026).
// La web ya no escala a los enemigos con fórmulas sueltas: parte de cuánto daño hace un
// jugador a cada minuto (CROWD: a un grupo; SINGLE: a uno solo) y de cuánta experiencia
// debe ganar por segundo (XP_S), y de ahí salen la vida de cada enemigo, la de los jefes,
// la de los legendarios, la de Eternatus y la de Arceus. Tres dificultades: Normal,
// Difícil (×1,4 vida, ×1,25 daño, ×1,5 premios) y Pesadilla (×2, ×1,5, ×2).
#pragma once
#include <cstdint>

namespace ps {

enum Difficulty : uint8_t { DF_NORMAL, DF_HARD, DF_NIGHTMARE, DF_COUNT };
struct DiffMode { float hp, dmg, reward; };
extern const DiffMode kDiffModes[DF_COUNT];
extern const char* const kDiffId[DF_COUNT];      // "normal", "hard", "nightmare" (save.mode de la web)
extern const char* const kDiffName[DF_COUNT];    // «Normal», «Difícil», «Pesadilla»
extern const uint32_t kDiffColor[DF_COUNT];

// Curvas tiempo (s) → valor, interpoladas en logaritmo: 0 CROWD, 1 SINGLE, 2 XP_S, 3 daño de un enemigo.
enum DiffCurve { DC_CROWD, DC_SINGLE, DC_XP, DC_DMG };
extern const int kDiffCurveN[4];
extern const double kDiffCurve[4][32][2];
extern const float kDiffBossS[5], kDiffLegendS[5], kDiffBossK[5];
extern const float kDiffFieldK, kDiffEternaS, kDiffEternaK;
// Guardia de los jefes (GUARD): un golpe no quita más de `hit` de su vida…
struct DiffGuard { float hit, min_frac, over, window, player; };
extern const DiffGuard kDiffGuard;

double diff_curve(int c, double t);
double diff_spawn_k(double t);                           // RATE: enemigos por segundo (sin el grupo)
inline double diff_coop(int n) { return 1 + (n > 1 ? n - 1 : 0) * 0.5; }
double diff_rate(double t, int players);
double diff_mob_hp(int mode, double t, int players);
double diff_mob_dmg(int mode, double t);
double diff_mob_xp(double t);
inline double diff_coop_xp(int n) { return 1 / diff_coop(n); }
double diff_boss_hp(int mode, int idx, double t, int players);
double diff_boss_dmg(int mode, double t);
// Legendario de la grieta n.º idx: dps = daño por segundo medido al equipo (0 = no se sabe),
// floor_ = vida mínima (la del jefe más duro vencido).
double diff_legend_hp(int mode, int idx, double t, int players, double dps = 0, double floor_ = 0);
double diff_legend_dmg(int mode, double t);
double diff_eterna_hp(int mode, int players);
double diff_judge_hp(int mode, int players);

}  // namespace ps

// types.h — tabla de tipos suavizada (types.js de la web).
//   juegos ×2 → ×1,6 · ×0,5 → ×0,6 · ×0 → ×0,3; con dos tipos se multiplican,
//   con tope en [×0,3, ×2,4].
#pragma once
#include "core/gamedata.h"

namespace ps {

// Multiplicador de un ataque de tipo `atk` contra un Pokémon de tipos `def`.
float type_mult(int atk, const int8_t def[2]);
inline float type_mult1(int atk, int def) { const int8_t d[2] = {int8_t(def), T_NONE}; return type_mult(atk, d); }

enum Eff : int8_t { EFF_NONE, EFF_SUPER, EFF_WEAK, EFF_CRIT };
// Types.label: 'super' | 'weak' | null.
inline Eff type_label(float m) { return m > 1.05f ? EFF_SUPER : m < 0.95f ? EFF_WEAK : EFF_NONE; }

int type_from_id(const char* id);   // "fire" → T_FIRE; T_NONE si no existe

}  // namespace ps

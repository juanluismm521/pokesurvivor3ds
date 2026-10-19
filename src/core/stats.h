// stats.h — ataque físico / especial y defensas (stats.js de la web, 18-10-2026).
// Como en los juegos: cada ataque es físico o especial (MoveDef::cat). Los
// físicos pegan con el Ataque del Pokémon (Mon::pa) y los especiales con su
// Ataque Especial (Mon::sa); `atk` sigue siendo el mayor de los dos.
//
// Defensas (Mon::df / Mon::sd), en suave: con 75 (la media) igual; con mucha,
// hasta un 22 % menos de daño; con poca, hasta un 25 % más.
//   - Los enemigos se defienden de tus ataques físicos con su Defensa y de los
//     especiales con su Defensa Especial.
//   - Cada enemigo pega con su lado fuerte (físico si su Ataque es mayor que
//     su Ataque Especial) y tú te defiendes con la defensa que toque.
// El daño de Arceus (pure) y de las trampas ignora todo esto.
#pragma once
#include <cmath>

#include "core/gamedata.h"

namespace ps {

// Multiplicador del daño que recibe quien tiene esa defensa base (guard).
inline float stats_guard(int def) {
  const double REF = 75, EXP = 0.35, LO = 0.78, HI = 1.25;
  const double d = def ? double(def) : REF;
  const double k = std::pow(REF / (d > 5 ? d : 5), EXP);
  return float(k < LO ? LO : k > HI ? HI : k);
}

// Daño que recibe un Pokémon de un golpe de categoría `cat` (sin categoría o sin datos: ×1).
inline float stats_taken(const Mon* mon, int cat) {
  if (!mon || cat < 0) return 1;
  return stats_guard(cat == CAT_SPECIAL ? mon->sd : mon->df);
}

// Lado fuerte de un Pokémon (empate: físico).
inline int stats_side_of(const Mon* mon) {
  if (!mon) return CAT_PHYSICAL;
  return mon->sa > mon->pa ? CAT_SPECIAL : CAT_PHYSICAL;
}

// Lo que rinde un ataque con este Pokémon respecto a su lado bueno (1 = igual).
inline float stats_fit(const Mon* mon, int cat) {
  if (!mon || cat < 0) return 1;
  return float(double(cat == CAT_SPECIAL ? mon->sa : mon->pa) / double(mon->atk));
}

}  // namespace ps

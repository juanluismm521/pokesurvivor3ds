// upgrades.h — mejoras permanentes entre runs (upgrades.js de la web).
#pragma once
#include <cstdint>

#include "core/gamedata.h"

namespace ps {

struct Perks {
  float hp = 0, atk_mul = 1, spd_mul = 1, cd_mul = 1, dmg_reduce = 0, regen = 0;
  float magnet_mul = 1, xp_mul = 1, coin_mul = 1;
  int start_moves = 1;
  float crit = 0;
  int rerolls = 0, bans = 0;
};

// Precio del nivel `lvl` (0 = el primero) de una mejora.
int upgrade_cost(int id, int lvl);
// Niveles comprados → modificadores para el jugador (nunca por encima del máximo).
Perks upgrade_perks(const uint8_t levels[UP_COUNT]);

}  // namespace ps

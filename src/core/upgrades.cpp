// upgrades.cpp
#include "core/upgrades.h"

#include <cmath>

#include "core/util.h"

namespace ps {

int upgrade_cost(int id, int lvl) {
  const UpgradeDef& u = kUpgrades[id];
  return int(jsround(u.base * std::pow(double(u.grow), lvl) / 10)) * 10;
}

Perks upgrade_perks(const uint8_t levels[UP_COUNT]) {
  auto L = [&](int id) { return clamp(int(levels[id]), 0, int(kUpgrades[id].max)); };
  Perks p;
  p.hp = L(UP_HP) * 8.0f;
  p.atk_mul = 1 + L(UP_ATK) * 0.04f;
  p.spd_mul = 1 + L(UP_SPD) * 0.03f;
  p.cd_mul = 1 - L(UP_CD) * 0.03f;
  p.dmg_reduce = L(UP_DEF) * 0.02f;
  p.regen = L(UP_REGEN) * 0.2f;
  p.magnet_mul = 1 + L(UP_MAGNET) * 0.12f;
  p.xp_mul = 1 + L(UP_XP) * 0.05f;
  p.coin_mul = 1 + L(UP_COINS) * 0.08f;
  p.start_moves = 1 + L(UP_MOVES);
  p.crit = L(UP_CRIT) * 0.03f;
  p.rerolls = L(UP_REROLL);
  p.bans = L(UP_BAN);
  return p;
}

}  // namespace ps

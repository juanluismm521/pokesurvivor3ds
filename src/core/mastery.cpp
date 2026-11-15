// Maestría Pokémon (web: mastery.js). Ver mastery.h.
#include "core/mastery.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ps {

const MasteryTierDef kMasteryTiers[MT_COUNT] = {
    {"photo", 10000, "Foto de maestría"},
    {"title", 25000, "Título de maestro"},
    {"frame", 50000, "Marco de maestría"},
    {"aura", 100000, "Aura de maestro"},
};

const char* const kPStatId[PS_COUNT] = {"r", "s", "k", "a", "bt", "ad", "b", "l", "e"};
const double kPStatMax[PS_COUNT] = {1e6, 1e9, 1e10, 1e6, 86400, 1e15, 1e7, 1e7, 1e6};

int32_t mastery_run_points(int kills, double secs) {
  const double k = std::max(0.0, std::floor(double(kills)));
  const double s = std::min(double(kMasteryRunMaxS), std::max(0.0, std::isfinite(secs) ? secs : 0.0));
  return int32_t(std::floor(k / kMasteryKills + s / 60 * kMasteryPerMin));
}

int mastery_level(int32_t pts) {
  int n = 0;
  for (const auto& t : kMasteryTiers) if (pts >= t.at) n++;
  return n;
}

int mastery_next(int32_t pts) {
  for (int i = 0; i < MT_COUNT; ++i) if (pts < kMasteryTiers[i].at) return i;
  return -1;
}

bool mastery_has(int32_t pts, int tier) { return tier >= 0 && tier < MT_COUNT && pts >= kMasteryTiers[tier].at; }

void pstat_add(PStat& p, const double (&run)[PS_COUNT]) {
  for (int k = 0; k < PS_COUNT; ++k) {
    const double x = std::max(0.0, std::floor(std::isfinite(run[k]) ? run[k] : 0));
    const double cur = std::isfinite(p.v[k]) ? p.v[k] : 0;
    p.v[k] = std::min(kPStatMax[k], pstat_is_max(k) ? std::max(cur, x) : cur + x);
  }
}

void mastery_reward_text(int tier, const char* name, char* out, int cap) {
  switch (tier) {
    case MT_PHOTO: std::snprintf(out, size_t(cap), "Foto de maestría de %s", name); break;
    case MT_TITLE: std::snprintf(out, size_t(cap), "Título «Maestro de %s»", name); break;
    case MT_FRAME: std::snprintf(out, size_t(cap), "Marco de maestría de %s", name); break;
    default: std::snprintf(out, size_t(cap), "Aura de maestro para %s", name); break;
  }
}

}  // namespace ps

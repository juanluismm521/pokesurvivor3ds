// diff.cpp — ver diff.h (difficulty.js de la web).
#include "core/diff.h"

#include <algorithm>
#include <cmath>

namespace ps {

const char* const kDiffId[DF_COUNT] = {"normal", "hard", "nightmare"};
const char* const kDiffName[DF_COUNT] = {"Normal", "Difícil", "Pesadilla"};
const uint32_t kDiffColor[DF_COUNT] = {0x6fdc8c, 0xffb24d, 0xff5a7a};

double diff_curve(int c, double t) {
  const double (*p)[2] = kDiffCurve[c];
  const int n = kDiffCurveN[c];
  if (t <= p[0][0]) return p[0][1];
  for (int i = 1; i < n; ++i) {
    if (t <= p[i][0]) {
      const double k = (t - p[i - 1][0]) / (p[i][0] - p[i - 1][0]);
      return std::exp(std::log(p[i - 1][1]) + (std::log(p[i][1]) - std::log(p[i - 1][1])) * k);
    }
  }
  return p[n - 1][1];
}

static const DiffMode& mk(int mode) { return kDiffModes[mode >= 0 && mode < DF_COUNT ? mode : 0]; }

double diff_spawn_k(double t) { return 1 + std::pow(std::min(1.0, t / 1080), 0.8) * 3; }
double diff_rate(double t, int players) { return diff_spawn_k(t) * diff_coop(players); }
double diff_mob_hp(int mode, double t, int players) {
  return diff_curve(DC_CROWD, t) / double(kDiffFieldK) / diff_spawn_k(t) * (std::max(1, players) / diff_coop(players)) * mk(mode).hp;
}
double diff_mob_dmg(int mode, double t) { return diff_curve(DC_DMG, t) * mk(mode).dmg; }
double diff_mob_xp(double t) { return diff_curve(DC_XP, t) / diff_spawn_k(t); }
double diff_boss_hp(int mode, int idx, double t, int players) {
  const int i = std::min(std::max(idx, 0), 4);
  return diff_curve(DC_SINGLE, t) * double(kDiffBossS[i]) * double(kDiffBossK[i]) * std::max(1, players) * mk(mode).hp;
}
double diff_boss_dmg(int mode, double t) { return diff_curve(DC_DMG, t) * 1.6 * mk(mode).dmg; }
double diff_legend_hp(int mode, int idx, double t, int players, double dps, double floor_) {
  const double one = diff_curve(DC_SINGLE, t) * std::max(1, players);
  const double base = one * double(kDiffLegendS[std::min(std::max(idx, 0), 4)]);
  const double k = dps > 0 ? std::min(4.0, std::max(0.25, dps / one)) : 1;
  return std::max(base * std::pow(k, 0.585) * mk(mode).hp, floor_ * 1.15);
}
double diff_legend_dmg(int mode, double t) { return diff_curve(DC_DMG, t) * 1.1 * mk(mode).dmg; }
double diff_eterna_hp(int mode, int players) { return diff_curve(DC_SINGLE, 1200) * double(kDiffEternaS) * double(kDiffEternaK) * std::max(1, players) * mk(mode).hp; }
double diff_judge_hp(int mode, int players) { return diff_curve(DC_SINGLE, 1800) * 3 * std::max(1, players) * mk(mode).hp; }

}  // namespace ps

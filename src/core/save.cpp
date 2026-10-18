// save.cpp — partida guardada (db.js) y lo que la partida en curso apunta en
// ella al acabar (endRun de game.js) o al pasar algo (shinies, rescates).
#include "core/save.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/game.h"

namespace ps {

Save::Save() { reset(); }

void Save::reset() {
  std::memset(static_cast<void*>(this), 0, sizeof(*this));
  v = kSaveVersion;
  std::snprintf(name, sizeof(name), "Jugador");
  coins = kStartCoins;
  for (auto& m : daily) m = Mission();
  for (auto& m : weekly) m = Mission();
  stats = SaveStats();
  for (auto& b : pbind) for (auto& x : b) x = -1;
  for (auto& b : pban) for (auto& x : b) x = -1;
}

int Save::owned_count() const { int n = 0; for (int d = 0; d <= kMaxDex; ++d) if (owned_at[d] && mon_by_dex(d)) n++; return n; }
int Save::shiny_count() const { int n = 0; for (int d = 0; d <= kMaxDex; ++d) if (shiny_at[d]) n++; return n; }
int Save::legends_count() const { int n = 0; for (int d = 0; d <= kMaxDex; ++d) if (legends[d]) n++; return n; }

bool save_grant_dex(Save& s, int dex, int from, int64_t now) {
  if (dex < 0 || dex > kMaxDex || s.owned_at[dex]) return false;
  s.owned_at[dex] = now ? now : 1;
  s.owned_from[dex] = int8_t(from);
  return true;
}

bool save_grant_shiny_dex(Save& s, int dex, int from, int64_t now) {
  save_grant_dex(s, dex, from, now);
  if (dex < 0 || dex > kMaxDex || s.shiny_at[dex]) return false;
  s.shiny_at[dex] = now ? now : 1;
  s.shiny_from[dex] = int8_t(from);
  return true;
}

// ---------------- enganches con la partida (game.cpp) ----------------
// La hora la pone la plataforma en g.now_ms (reloj de la consola).

bool save_grant_shiny(Game& g, int dex) {
  Save& s = *g.save;
  const bool fresh = save_grant_shiny_dex(s, dex, FROM_WILD, g.now_ms);
  s.stats.shinies++;
  g.dirty = true;
  return fresh;
}

bool save_grant(Game& g, int dex) {
  const bool fresh = save_grant_dex(*g.save, dex, FROM_RESCUE, g.now_ms);
  g.dirty = true;
  return fresh;
}

// Lo que se apunta al acabar la partida (endRun de game.js).
void save_end_run(Game& g) {
  Save& s = *g.save;
  RunSummary& r = g.summary;
  const Player& pl = g.pl();
  r.record = g.time > s.stats.best_time;
  s.coins += r.coins;
  // (los tickets ya se guardaron al cogerlos)
  s.stats.runs++;
  s.stats.total_kills += pl.kills;
  s.stats.best_time = std::max(s.stats.best_time, int32_t(std::floor(g.time)));
  s.stats.best_level = std::max(s.stats.best_level, int32_t(pl.level));
  s.stats.coins_earned += r.coins;
  if (r.judge_time >= 0) progress_event(g, EV_JUDGEEND, r.judge_bosses, 0, r.judge_time);
  progress_event(g, EV_RUNEND, pl.level, pl.coins, float(g.time));
  g.dirty = true;
}

}  // namespace ps

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
  for (auto& x : pstart) x = -1;
  for (auto& x : pskin) x = -1;
  for (auto& x : pnat) x = -1;
  for (auto& x : pabil) x = -1;
  for (auto& x : pmove2) x = -1;
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
  if (pl.start_dex >= 0 && pl.start_dex <= kMaxDex) s.pused[pl.start_dex]++;   // para "más usados" en la colección
  s.stats.total_kills += pl.kills;
  s.stats.best_time = std::max(s.stats.best_time, int32_t(std::floor(g.time)));
  s.stats.best_level = std::max(s.stats.best_level, int32_t(pl.level));
  s.stats.coins_earned += r.coins;
  if (r.judge_time >= 0) progress_event(g, EV_JUDGEEND, r.judge_bosses, 0, r.judge_time);
  // Medalla de Arceus (modo rápido): para el Pokémon con el que le has ganado (en
  // grupo, cada consola la suya) y su premio de tickets ×10.
  if (r.victory) {
    const int dex = g.players[g.local].start_dex;   // (con el que empezó, aunque haya evolucionado)
    if (dex >= 1 && dex <= kMaxDex && s.arceus_win[dex] <= 0) s.arceus_win[dex] = g.now_ms > 0 ? g.now_ms : 1;
    s.t10 += r.win_t10;
  }
  // Eternatus vencido: cuenta para la dificultad (y desbloquea la siguiente)
  if (g.eterna_wins > 0) {
    s.stats.eterna_wins += g.eterna_wins;
    const int d = g.cfg.diff < DF_COUNT ? g.cfg.diff : 0;
    const bool had = s.mode_unlocked(d + 1);
    s.mode_wins[d] += g.eterna_wins;
    for (int k = d - 1; k >= 0; --k) if (s.mode_wins[k] <= 0) s.mode_wins[k] = 1;
    if (d + 1 < DF_COUNT && !had) r.unlocked = int8_t(d + 1);
  }
  // Maestría (web: Mastery.addRun / addStats), para el Pokémon con el que empezó.
  // (El desafío semanal no cuenta.)
  if (!g.cfg.weekly && pl.start_dex >= 1 && pl.start_dex <= kMaxDex) {
    const int d = pl.start_dex;
    const int32_t before = s.mastery[d], gained = mastery_run_points(pl.kills, g.time);
    const int32_t after = int32_t(std::min<int64_t>(2000000000, int64_t(before) + gained));
    if (gained > 0) s.mastery[d] = after;
    r.m_dex = int16_t(d); r.m_before = before; r.m_after = after; r.m_gained = gained;
    for (int t = 0; t < MT_COUNT; ++t) if (before < kMasteryTiers[t].at && after >= kMasteryTiers[t].at) r.m_got |= uint8_t(1u << t);
    const int ew = g.eterna_wins > 0 ? 1 : 0;
    const double run[PS_COUNT] = {1, g.time, double(pl.kills), r.judge_time >= 0 ? 1.0 : 0.0, g.time,
                                  r.judge_time >= 0 ? r.judge_dmg : 0.0, double(std::max(0, pl.bosses - g.rifts_won - ew)),
                                  double(g.rifts_won), double(ew)};
    pstat_add(s.pstat[d], run);
  }
  s.t1 += r.diff_t1; s.t10 += r.diff_t10;   // (los tickets de más de la dificultad)
  progress_event(g, EV_RUNEND, pl.level, pl.coins, float(g.time));
  g.dirty = true;
}

}  // namespace ps

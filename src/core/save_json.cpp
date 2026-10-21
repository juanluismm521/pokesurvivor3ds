// save_json.cpp — la partida guardada en JSON, con las mismas claves que el
// guardado local de la web (db.js), y escritura a prueba de apagones:
//   1. se escribe save.tmp entero
//   2. el save.json anterior pasa a save.bak
//   3. save.tmp pasa a save.json
// Al leer, si save.json está mal (apagón a mitad), se usa save.bak.
#include "core/save_io.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "core/json.h"
#include "core/moves.h"
#include "core/progress.h"
#include "core/types.h"
#include "core/util.h"
#include "core/websave.h"

namespace ps {

namespace {
const char* const kFrom[] = {"", "starter", "gacha", "wild", "rescue", "other"};
int from_id(const std::string& s) {
  for (int i = 1; i < 6; ++i) if (s == kFrom[i]) return i;
  return FROM_OTHER;
}
std::string num_key(int n) { char b[16]; std::snprintf(b, sizeof(b), "%d", n); return b; }

Json dates_by_dex(const int64_t* at, const int8_t* from) {
  Json o = Json::object();
  for (int d = 0; d <= kMaxDex; ++d) {
    if (!at[d]) continue;
    Json e = Json::object();
    e.set("at", Json::num(double(at[d])));
    e.set("from", Json::str(kFrom[from ? clamp_from(from[d]) : 0]));
    o.set(num_key(d), std::move(e));
  }
  return o;
}

void read_dates_by_dex(const Json& o, int64_t* at, int8_t* from) {
  for (const auto& kv : o.items()) {
    const int d = std::atoi(kv.first.c_str());
    if (!mon_by_dex(d)) continue;   // (sanitize: Pokémon que no existen fuera)
    const Json& e = kv.second;
    at[d] = e.is_obj() ? int64_t(e["at"].as_num(1)) : int64_t(e.as_num(1));
    if (at[d] <= 0) at[d] = 1;
    if (from) from[d] = int8_t(e.is_obj() ? from_id(e["from"].as_str()) : FROM_OTHER);
  }
}

Json date_map(const int64_t* arr, int n, const char* const* ids) {
  Json o = Json::object();
  for (int i = 0; i < n; ++i) if (arr[i]) o.set(ids ? std::string(ids[i]) : num_key(i), Json::num(double(arr[i])));
  return o;
}

Json mission_json(const Mission& m) {
  Json o = Json::object();
  o.set("id", Json::str(kMissions[m.def].id));
  o.set("goal", Json::num(m.goal));
  o.set("param", m.param >= 0 ? Json::str(kTypeId[m.param]) : Json());
  o.set("prog", Json::num(m.prog));
  o.set("claimed", Json::boolean(m.claimed));
  o.set("told", Json::boolean(m.told));
  return o;
}

bool mission_read(const Json& o, Mission& m) {
  m = Mission();
  if (!o.is_obj()) return false;
  for (int i = 0; i < kMissionCount; ++i) if (o["id"].as_str() == kMissions[i].id) m.def = int8_t(i);
  if (m.def < 0) return false;
  m.goal = int32_t(o["goal"].as_int(1));
  m.param = o["param"].is_str() ? int8_t(type_from_id(o["param"].as_str().c_str())) : int8_t(T_NONE);
  m.prog = int32_t(o["prog"].as_int(0));
  m.claimed = o["claimed"].as_bool();
  m.told = o["told"].as_bool();
  return true;
}

int n0(const Json& v) {   // números raros o negativos → 0 (sanitize de db.js: +v)
  const double x = v.is_str() ? std::strtod(v.as_str().c_str(), nullptr) : v.as_num(0);
  return std::isfinite(x) && x > 0 ? int(std::floor(x)) : 0;
}
}  // namespace

int clamp_from(int f) { return f < 0 || f > 5 ? 5 : f; }

std::string save_to_json(const Save& s) {
  Json o = Json::object();
  o.set("v", Json::num(kSaveVersion));
  o.set("name", Json::str(s.name));
  o.set("created", Json::num(double(s.created)));
  o.set("updated", Json::num(double(s.updated)));
  o.set("coins", Json::num(s.coins));
  o.set("owned", dates_by_dex(s.owned_at, s.owned_from));
  o.set("shiny", dates_by_dex(s.shiny_at, s.shiny_from));
  o.set("partner", s.partner ? Json::num(s.partner) : Json());
  o.set("partnerShiny", Json::boolean(s.partner_shiny));
  o.set("starter", s.starter ? Json::num(s.starter) : Json());
  if (s.trait[0]) {
    Json p = Json::object();
    p.set("trait", Json::str(s.trait));
    Json sc = Json::object();
    for (int i = 0; i < kTraitCount && i < 32; ++i) if (s.trait_scores[i]) sc.set(kTraits[i].id, Json::num(s.trait_scores[i]));
    p.set("scores", std::move(sc));
    o.set("personality", std::move(p));
  } else {
    o.set("personality", Json());
  }
  o.set("upgrades", Json::object());
  Json pupg = Json::object();
  for (int d = 0; d <= kMaxDex; ++d) {
    Json u = Json::object();
    bool any = false;
    for (int k = 0; k < UP_COUNT; ++k) if (s.pupg[d][k]) { u.set(kUpgrades[k].id, Json::num(s.pupg[d][k])); any = true; }
    if (any) pupg.set(num_key(d), std::move(u));
  }
  o.set("pupg", std::move(pupg));
  o.set("pupgDone", Json::boolean(true));
  Json tk = Json::object();
  tk.set("t1", Json::num(s.t1));
  tk.set("t10", Json::num(s.t10));
  o.set("tickets", std::move(tk));
  Json pity = Json::object();
  for (int g = 1; g <= 9; ++g) if (s.pity[g]) pity.set(num_key(g), Json::num(s.pity[g]));
  o.set("pity", std::move(pity));
  o.set("legends", date_map(s.legends, kMaxDex + 1, nullptr));
  o.set("arenas", date_map(s.arenas, T_COUNT, kTypeId));
  const char* item_ids[IT_COUNT];
  for (int i = 0; i < IT_COUNT; ++i) item_ids[i] = kItems[i].id;
  o.set("itemsFound", date_map(s.items_found, IT_COUNT, item_ids));
  const char* move_ids[kMaxMoves];
  for (int i = 0; i < kMaxMoves; ++i) move_ids[i] = i < kMoveCount ? kMoves[i].id : "";
  o.set("evoMoves", date_map(s.evo_moves, kMoveCount, move_ids));
  const char* ach_ids[256];
  for (int i = 0; i < 256; ++i) ach_ids[i] = i < kAchCount ? kAch[i].id : "";
  o.set("ach", date_map(s.ach, kAchCount < 256 ? kAchCount : 256, ach_ids));
  Json mi = Json::object();
  if (s.m_day[0]) {
    mi.set("day", Json::str(s.m_day));
    Json d = Json::array();
    for (const auto& m : s.daily) if (m.def >= 0) d.push(mission_json(m));
    mi.set("daily", std::move(d));
    mi.set("rerolled", Json::boolean(s.m_rerolled));
  }
  if (s.m_week[0]) {
    mi.set("week", Json::str(s.m_week));
    Json w = Json::array();
    for (const auto& m : s.weekly) if (m.def >= 0) w.push(mission_json(m));
    mi.set("weekly", std::move(w));
  }
  o.set("missions", std::move(mi));
  Json prof = Json::object();
  Json favs = Json::array();
  for (int f : s.favs) if (f) favs.push(Json::num(f));
  prof.set("favs", std::move(favs));
  prof.set("title", Json::str(s.title));
  o.set("profile", std::move(prof));
  Json st = Json::object();
  const SaveStats& S = s.stats;
  st.set("runs", Json::num(S.runs)); st.set("bestTime", Json::num(S.best_time)); st.set("bestLevel", Json::num(S.best_level));
  st.set("totalKills", Json::num(S.total_kills)); st.set("pulls", Json::num(S.pulls)); st.set("coinsEarned", Json::num(S.coins_earned));
  st.set("shinies", Json::num(S.shinies)); st.set("bossesTotal", Json::num(S.bosses_total)); st.set("riftWins", Json::num(S.rift_wins));
  st.set("evolutions", Json::num(S.evolutions)); st.set("missionsDone", Json::num(S.missions_done)); st.set("groupRuns", Json::num(S.group_runs));
  st.set("bestGroupTime", Json::num(S.best_group_time)); st.set("judged", Json::num(S.judged)); st.set("judgeTime", Json::num(S.judge_time));
  st.set("judgeBosses", Json::num(S.judge_bosses));
  o.set("stats", std::move(st));
  Json pb = Json::object(), pn = Json::object();
  for (int d = 0; d <= kMaxDex; ++d) {
    bool any = false;
    for (int k = 0; k < 4; ++k) if (s.pbind[d][k] >= 0) any = true;
    if (any) {
      Json a = Json::array();
      for (int k = 0; k < 4; ++k) a.push(s.pbind[d][k] >= 0 && s.pbind[d][k] < kMoveCount ? Json::str(kMoves[s.pbind[d][k]].id) : Json());
      pb.set(num_key(d), std::move(a));
    }
    Json b = Json::array();
    for (int k = 0; k < 3; ++k) if (s.pban[d][k] >= 0 && s.pban[d][k] < kMoveCount) b.push(Json::str(kMoves[s.pban[d][k]].id));
    if (b.size()) pn.set(num_key(d), std::move(b));
  }
  o.set("pbind", std::move(pb));
  o.set("pban", std::move(pn));
  Json pst = Json::object(), pus = Json::object();
  for (int d = 0; d <= kMaxDex; ++d) {
    if (s.pstart[d] >= 0 && s.pstart[d] < kMoveCount) pst.set(num_key(d), Json::str(kMoves[s.pstart[d]].id));
    if (s.pused[d] > 0) pus.set(num_key(d), Json::num(s.pused[d]));
  }
  o.set("pstart", std::move(pst));
  o.set("pused", std::move(pus));
  // Del port (la web no lo conoce y lo deja tal cual): la mejora «Megapiedra».
  Json pmg = Json::object();
  for (int d = 0; d <= kMaxDex; ++d) if (s.pmega[d] > 0) pmg.set(num_key(d), Json::num(s.pmega[d]));
  if (pmg.size()) o.set("pmega", std::move(pmg));   // (sólo si hay: si no, la partida de la web queda idéntica)
  return o.dump();
}

bool save_from_json(Save& s, const std::string& text) {
  Json o;
  if (!o.parse(text) || !o.is_obj()) return false;
  s.reset();
  if (o["name"].is_str()) std::snprintf(s.name, sizeof(s.name), "%s", o["name"].as_str().c_str());
  s.created = int64_t(o["created"].as_num(0));
  s.updated = int64_t(o["updated"].as_num(0));
  s.coins = n0(o["coins"]);
  read_dates_by_dex(o["owned"], s.owned_at, s.owned_from);
  read_dates_by_dex(o["shiny"], s.shiny_at, s.shiny_from);
  s.partner = int16_t(o["partner"].as_int(0));
  if (!mon_by_dex(s.partner)) s.partner = 0;
  s.partner_shiny = o["partnerShiny"].as_bool();
  s.starter = int16_t(o["starter"].as_int(0));
  const Json& per = o["personality"];
  if (per.is_obj()) {
    std::snprintf(s.trait, sizeof(s.trait), "%s", per["trait"].as_str().c_str());
    for (int i = 0; i < kTraitCount && i < 32; ++i) s.trait_scores[i] = int16_t(per["scores"][kTraits[i].id].as_int(0));
  }
  // v2 → v3: las mejoras eran de la cuenta; pasan al compañero de entonces.
  const Json& up = o["upgrades"];
  if (!o["pupgDone"].as_bool() && s.partner && up.is_obj()) {
    for (int k = 0; k < UP_COUNT; ++k) s.pupg[s.partner][k] = uint8_t(std::min(int(kUpgrades[k].max), n0(up[kUpgrades[k].id])));
  }
  for (const auto& kv : o["pupg"].items()) {
    const int d = std::atoi(kv.first.c_str());
    if (d < 0 || d > kMaxDex || !kv.second.is_obj()) continue;
    for (int k = 0; k < UP_COUNT; ++k) s.pupg[d][k] = uint8_t(std::min(int(kUpgrades[k].max), n0(kv.second[kUpgrades[k].id])));
  }
  s.t1 = n0(o["tickets"]["t1"]);
  s.t10 = n0(o["tickets"]["t10"]);
  if (o["pity"].is_num()) s.pity[1] = n0(o["pity"]);   // v1 → v2: un único banner
  else for (const auto& kv : o["pity"].items()) { const int g = std::atoi(kv.first.c_str()); if (g >= 1 && g <= 9) s.pity[g] = n0(kv.second); }
  for (const auto& kv : o["legends"].items()) { const int d = std::atoi(kv.first.c_str()); if (d > 0 && d <= kMaxDex) s.legends[d] = std::max<int64_t>(1, int64_t(kv.second.as_num(1))); }
  for (int t = 0; t < T_COUNT; ++t) if (o["arenas"].has(kTypeId[t])) s.arenas[t] = std::max<int64_t>(1, int64_t(o["arenas"][kTypeId[t]].as_num(1)));
  for (int i = 0; i < IT_COUNT; ++i) if (o["itemsFound"].has(kItems[i].id)) s.items_found[i] = std::max<int64_t>(1, int64_t(o["itemsFound"][kItems[i].id].as_num(1)));
  for (int i = 0; i < kMoveCount; ++i) if (o["evoMoves"].has(kMoves[i].id)) s.evo_moves[i] = std::max<int64_t>(1, int64_t(o["evoMoves"][kMoves[i].id].as_num(1)));
  for (int i = 0; i < kAchCount && i < 256; ++i) if (o["ach"].has(kAch[i].id)) s.ach[i] = std::max<int64_t>(1, int64_t(o["ach"][kAch[i].id].as_num(1)));
  const Json& mi = o["missions"];
  if (mi.is_obj()) {
    std::snprintf(s.m_day, sizeof(s.m_day), "%s", mi["day"].as_str().c_str());
    std::snprintf(s.m_week, sizeof(s.m_week), "%s", mi["week"].as_str().c_str());
    s.m_rerolled = mi["rerolled"].as_bool();
    for (int i = 0; i < 3; ++i) mission_read(mi["daily"][i], s.daily[i]);
    for (int i = 0; i < 2; ++i) mission_read(mi["weekly"][i], s.weekly[i]);
  }
  const Json& prof = o["profile"];
  for (int i = 0; i < 3; ++i) s.favs[i] = int16_t(prof["favs"][i].as_int(0));
  std::snprintf(s.title, sizeof(s.title), "%s", prof["title"].as_str().c_str());
  const Json& st = o["stats"];
  SaveStats& S = s.stats;
  S.runs = n0(st["runs"]); S.best_time = n0(st["bestTime"]); S.best_level = n0(st["bestLevel"]);
  S.total_kills = n0(st["totalKills"]); S.pulls = n0(st["pulls"]); S.coins_earned = n0(st["coinsEarned"]);
  S.shinies = n0(st["shinies"]); S.bosses_total = n0(st["bossesTotal"]); S.rift_wins = n0(st["riftWins"]);
  S.evolutions = n0(st["evolutions"]); S.missions_done = n0(st["missionsDone"]); S.group_runs = n0(st["groupRuns"]);
  S.best_group_time = n0(st["bestGroupTime"]); S.judged = n0(st["judged"]); S.judge_time = n0(st["judgeTime"]);
  S.judge_bosses = n0(st["judgeBosses"]);
  // Atajos y veto: movimientos que existan, sin repetir.
  for (const auto& kv : o["pbind"].items()) {
    const int d = std::atoi(kv.first.c_str());
    if (d < 0 || d > kMaxDex) continue;
    for (int k = 0; k < 4; ++k) {
      const Json& v = kv.second[k];
      const int mi2 = v.is_str() ? move_index(v.as_str().c_str()) : -1;
      bool dup = false;
      for (int q = 0; q < k; ++q) if (s.pbind[d][q] == mi2 && mi2 >= 0) dup = true;
      s.pbind[d][k] = int8_t(dup ? -1 : mi2);
    }
  }
  for (const auto& kv : o["pban"].items()) {
    const int d = std::atoi(kv.first.c_str());
    if (d < 0 || d > kMaxDex) continue;
    int n = 0;
    for (size_t k = 0; k < kv.second.size() && n < 3; ++k) {
      const Json& v = kv.second[k];
      const int mi2 = v.is_str() ? move_index(v.as_str().c_str()) : -1;
      if (mi2 < 0) continue;
      bool dup = false;
      for (int q = 0; q < n; ++q) if (s.pban[d][q] == mi2) dup = true;
      if (!dup) s.pban[d][n++] = int8_t(mi2);
    }
  }
  // Movimiento inicial y partidas por Pokémon.
  for (const auto& kv : o["pstart"].items()) {
    const int d = std::atoi(kv.first.c_str());
    if (d < 0 || d > kMaxDex || !kv.second.is_str()) continue;
    s.pstart[d] = int8_t(move_index(kv.second.as_str().c_str()));
  }
  for (const auto& kv : o["pused"].items()) {
    const int d = std::atoi(kv.first.c_str());
    if (d >= 0 && d <= kMaxDex) s.pused[d] = n0(kv.second);
  }
  for (const auto& kv : o["pmega"].items()) {
    const int d = std::atoi(kv.first.c_str());
    if (d >= 0 && d <= kMaxDex) s.pmega[d] = uint8_t(std::min(5, std::max(0, int(n0(kv.second)))));
  }
  return true;
}

void new_save(Save& s, const char* name, int64_t now_ms) {
  s.reset();
  std::snprintf(s.name, sizeof(s.name), "%s", name && *name ? name : "Jugador");
  s.created = s.updated = now_ms;
  s.coins = kStartCoins;
}

namespace {
bool read_text(const std::string& path, std::string& out) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  char buf[8192];
  size_t n;
  out.clear();
  while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
  std::fclose(f);
  return true;
}
}  // namespace

Json& save_raw() {
  static Json raw;
  return raw;
}

namespace {
Json& save_was() {   // lo que la 3DS entendía de la partida web al leerla
  static Json was;
  return was;
}
}  // namespace

void save_set_raw(const Json& raw, const Save& s) {
  save_raw() = raw;
  save_was() = Json();
  save_was().parse(save_to_json(s));
}

Json save_full_json(const Save& s) {
  Json mine;
  mine.parse(save_to_json(s));
  return save_raw().is_obj() ? web::overlay_changes(save_raw(), save_was(), mine) : mine;
}

int save_read_file(const char* dir, Save& s) {
  std::string text;
  const std::string base = std::string(dir) + "/save";
  int got = 0;
  if (read_text(base + ".json", text) && save_from_json(s, text)) got = 1;
  else if (read_text(base + ".bak", text) && save_from_json(s, text)) got = 2;   // copia de seguridad
  if (got) { Json raw; raw.parse(text); save_set_raw(raw, s); }
  return got;
}

bool save_write_file(const char* dir, const Save& s) {
  return save_write_text(dir, save_full_json(s).dump());
}

bool save_write_text(const char* dir, const std::string& text) {
  const std::string base = std::string(dir) + "/save";
  const std::string tmp = base + ".tmp", cur = base + ".json", bak = base + ".bak";
  FILE* f = std::fopen(tmp.c_str(), "wb");
  if (!f) return false;
  bool ok = true;
  for (size_t at = 0; ok && at < text.size(); at += 16384) {
    const size_t n = std::min<size_t>(16384, text.size() - at);
    ok = std::fwrite(text.data() + at, 1, n, f) == n;
    std::fflush(f);
  }
  if (std::fclose(f) != 0 || !ok) { std::remove(tmp.c_str()); return false; }
  // El anterior pasa a copia de seguridad; el nuevo ocupa su sitio.
  std::remove(bak.c_str());
  std::rename(cur.c_str(), bak.c_str());
  if (std::rename(tmp.c_str(), cur.c_str()) != 0) { std::rename(bak.c_str(), cur.c_str()); return false; }
  return true;
}

}  // namespace ps

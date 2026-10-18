// progress.cpp — ver progress.h (progress.js de la web).
#include "core/progress.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "core/game.h"

namespace ps {

Notices g_notices;

void Notices::push(const char* title, const char* sub, int ach) {
  if (n >= 8) pop();
  Notice& x = q[n++];
  std::snprintf(x.title, sizeof(x.title), "%s", title);
  std::snprintf(x.sub, sizeof(x.sub), "%s", sub ? sub : "");
  x.ach = int16_t(ach);
}

void Notices::pop() {
  if (!n) return;
  for (int i = 0; i < n - 1; ++i) q[i] = q[i + 1];
  n--;
}

// ---------------- fechas ----------------

namespace {
// Días desde 1970-01-01 → fecha (algoritmo civil de H. Hinnant).
void civil(int64_t z, int& y, int& m, int& d) {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = unsigned(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int64_t yy = int64_t(yoe) + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  d = int(doy - (153 * mp + 2) / 5 + 1);
  m = int(mp < 10 ? mp + 3 : mp - 9);
  y = int(yy + (m <= 2));
}
int64_t days_from_civil(int y, int m, int d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = unsigned(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + int64_t(doe) - 719468;
}
}  // namespace

// now_ms es la hora local de la consola (lo que pone su reloj).
void period_ids(int64_t now_ms, char* day, int nd, char* week, int nw) {
  const int64_t days = (now_ms >= 0 ? now_ms / 86400000 : (now_ms - 86399999) / 86400000);
  int y, m, d;
  civil(days, y, m, d);
  std::snprintf(day, nd, "d%04d%02d%02d", y, m, d);
  // Semana ISO (lunes a domingo), como isoWeek de la web.
  const int wd = int(((days % 7) + 7 + 3) % 7) + 1;   // 1 = lunes (1970-01-01 fue jueves)
  const int64_t th = days + 4 - wd;
  int ty, tm, td;
  civil(th, ty, tm, td);
  const int64_t y0 = days_from_civil(ty, 1, 1);
  const int wk = int(std::ceil((double(th - y0) + 1) / 7));
  std::snprintf(week, nw, "w%04d-%02d", ty, wk);
}

// ---------------- misiones ----------------

namespace {
struct MRng {
  uint32_t h;
  explicit MRng(const char* s) : h(2166136261u) {
    for (; *s; ++s) { h ^= uint8_t(*s); h *= 16777619u; }
  }
  double operator()() {
    h += 0x6D2B79F5u;
    uint32_t t = h;
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return double(t ^ (t >> 14)) / 4294967296.0;
  }
};

Mission make(int def, MRng& r) {
  const MissionDef& d = kMissions[def];
  Mission m;
  m.def = int8_t(def);
  m.goal = d.goals[int(std::floor(r() * d.ngoals))];
  m.param = d.type_param ? int8_t(std::floor(r() * T_COUNT)) : int8_t(T_NONE);
  return m;
}

// Sortea n misiones de las de un tipo (diarias o semanales), sin las de `avoid`.
int pick_set(bool weekly, int n, MRng& r, Mission* out, const Mission* avoid = nullptr, int navoid = 0) {
  int pool[32], np = 0;
  for (int i = 0; i < kMissionCount; ++i) {
    if (kMissions[i].weekly != weekly) continue;
    bool skip = false;
    for (int k = 0; k < navoid; ++k) if (avoid[k].def == i) skip = true;
    if (!skip) pool[np++] = i;
  }
  int k = 0;
  while (k < n && np) {
    const int j = int(std::floor(r() * np));
    out[k++] = make(pool[j], r);
    for (int q = j; q < np - 1; ++q) pool[q] = pool[q + 1];
    np--;
  }
  return k;
}

bool has_type(const int8_t* types, int t) { return types && t >= 0 && (types[0] == t || types[1] == t); }

void advance(Mission* list, int n, const ProgEvent& e) {
  for (int i = 0; i < n; ++i) {
    Mission& m = list[i];
    if (m.def < 0 || m.claimed || mission_done(m)) continue;
    const MissionDef& d = kMissions[m.def];
    if (d.ev != e.ev) continue;
    const char* id = d.id;
    if ((!std::strcmp(id, "killType") || !std::strcmp(id, "playType")) && !has_type(e.types, m.param)) continue;
    if ((!std::strcmp(id, "boss") || !std::strcmp(id, "wboss")) && !e.boss) continue;
    if (!std::strcmp(id, "wdaily") && !e.daily) continue;
    int v = 1;
    if (!std::strcmp(id, "survive") || !std::strcmp(id, "playType") || !std::strcmp(id, "wsurvive")) v = int(std::floor(e.time / 60));
    else if (!std::strcmp(id, "level")) v = e.level;
    else if (!std::strcmp(id, "coins")) v = e.coins;
    m.prog = d.max_mode ? std::max(m.prog, int32_t(v)) : m.prog + v;
    if (mission_done(m) && !m.told) {
      m.told = true;
      char t[96], b[128];
      mission_text(m, t, sizeof(t));
      std::snprintf(b, sizeof(b), "Misión completada: %s", t);
      g_notices.push(b, "Recoge el premio en Misiones", -1);
    }
  }
}

// toLocaleString('es'): en español los números de 4 cifras no llevan punto.
void thousands(int n, char* out, int len) {
  if (n < 10000) { std::snprintf(out, len, "%d", n); return; }
  std::snprintf(out, len, "%d.%03d", n / 1000, n % 1000);
}
}  // namespace

void mission_text(const Mission& m, char* out, int n) {
  if (m.def < 0) { out[0] = 0; return; }
  const char* id = kMissions[m.def].id;
  const int g = m.goal;
  const char* T = m.param >= 0 ? kTypeName[m.param] : "";
  if (!std::strcmp(id, "kill")) std::snprintf(out, n, "Derrota a %d Pokémon", g);
  else if (!std::strcmp(id, "killType")) std::snprintf(out, n, "Derrota a %d Pokémon de tipo %s", g, T);
  else if (!std::strcmp(id, "survive") || !std::strcmp(id, "wsurvive")) std::snprintf(out, n, "Aguanta %d minutos en una run", g);
  else if (!std::strcmp(id, "level")) std::snprintf(out, n, "Llega al nivel %d en una run", g);
  else if (!std::strcmp(id, "boss")) { if (g == 1) std::snprintf(out, n, "Derrota a un jefe"); else std::snprintf(out, n, "Derrota a %d jefes", g); }
  else if (!std::strcmp(id, "chest")) { if (g == 1) std::snprintf(out, n, "Abre un cofre con candado"); else std::snprintf(out, n, "Abre %d cofres con candado", g); }
  else if (!std::strcmp(id, "coins")) std::snprintf(out, n, "Recoge %d monedas en tus runs", g);
  else if (!std::strcmp(id, "playType")) std::snprintf(out, n, "Aguanta %d minutos con un Pokémon de tipo %s", g, T);
  else if (!std::strcmp(id, "item")) std::snprintf(out, n, "Encuentra un objeto equipable");
  else if (!std::strcmp(id, "evolve")) std::snprintf(out, n, "Evoluciona un movimiento");
  else if (!std::strcmp(id, "wkill")) { char b[16]; thousands(g, b, sizeof(b)); std::snprintf(out, n, "Derrota a %s Pokémon", b); }
  else if (!std::strcmp(id, "wrift")) { if (g == 1) std::snprintf(out, n, "Gana una pelea en una grieta"); else std::snprintf(out, n, "Gana %d peleas en grietas", g); }
  else if (!std::strcmp(id, "wboss")) std::snprintf(out, n, "Derrota a %d jefes", g);
  else if (!std::strcmp(id, "wevolve")) std::snprintf(out, n, "Evoluciona %d movimientos", g);
  else if (!std::strcmp(id, "wdaily")) std::snprintf(out, n, "Completa %d misiones diarias", g);
  else std::snprintf(out, n, "%s", id);
}

void progress_ensure(Save& s, int64_t now_ms) {
  char d[16], w[16];
  period_ids(now_ms, d, sizeof(d), w, sizeof(w));
  progress_ensure_ids(s, d, w);
}

void progress_ensure_ids(Save& s, const char* d, const char* w) {
  char key[64];
  if (std::strcmp(s.m_day, d)) {
    std::snprintf(s.m_day, sizeof(s.m_day), "%s", d);
    s.m_rerolled = false;
    std::snprintf(key, sizeof(key), "%s%lld", d, (long long)s.created);
    MRng r(key);
    for (auto& m : s.daily) m = Mission();
    pick_set(false, 3, r, s.daily);
  }
  if (std::strcmp(s.m_week, w)) {
    std::snprintf(s.m_week, sizeof(s.m_week), "%s", w);
    std::snprintf(key, sizeof(key), "%s%lld", w, (long long)s.created);
    MRng r(key);
    for (auto& m : s.weekly) m = Mission();
    pick_set(true, 2, r, s.weekly);
  }
}

void grant_reward(Save& s, const Reward& r) {
  s.coins += r.coins;
  s.t1 += r.t1;
  s.t10 += r.t10;
}

bool progress_claim(Save& s, int i, bool weekly, int64_t now_ms) {
  Mission& m = weekly ? s.weekly[i] : s.daily[i];
  if (m.def < 0 || m.claimed || !mission_done(m)) return false;
  m.claimed = true;
  grant_reward(s, kMissions[m.def].reward);
  s.stats.missions_done++;
  ProgEvent e;
  e.ev = EV_MISSIONDONE;
  e.daily = !weekly;
  progress_emit(s, e, now_ms);
  return true;
}

// Cambia una diaria sin terminar por otra (una vez al día).
bool progress_reroll(Save& s, int i, int64_t now_ms) {
  progress_ensure(s, now_ms);
  Mission& m = s.daily[i];
  if (m.def < 0 || s.m_rerolled || m.claimed) return false;
  char key[64];
  std::snprintf(key, sizeof(key), "%scambio%lld", s.m_day, (long long)now_ms);
  MRng r(key);
  Mission n;
  if (!pick_set(false, 1, r, &n, s.daily, 3)) return false;
  m = n;
  s.m_rerolled = true;
  return true;
}

int progress_pending(Save& s, int64_t now_ms) {
  progress_ensure(s, now_ms);
  int n = 0;
  for (const auto& m : s.daily) if (m.def >= 0 && mission_done(m) && !m.claimed) n++;
  for (const auto& m : s.weekly) if (m.def >= 0 && mission_done(m) && !m.claimed) n++;
  return n;
}

// ---------------- logros ----------------

int ach_value(const Save& s, int i) {
  const AchDef& a = kAch[i];
  const char* id = a.id;
  auto starts = [&](const char* p) { return !std::strncmp(id, p, std::strlen(p)); };
  auto count = [](const int64_t* arr, int n) { int c = 0; for (int k = 0; k < n; ++k) if (arr[k]) c++; return c; };
  if (a.dex) return s.legends[a.dex] ? 1 : 0;
  if (starts("own")) return s.owned_count();
  if (starts("shiny")) return s.shiny_count();
  if (!std::strcmp(id, "pulls100")) return s.stats.pulls;
  if (starts("time")) return s.stats.best_time / 60;
  if (starts("lvl")) return s.stats.best_level;
  if (!std::strcmp(id, "judge")) return s.stats.judged;
  if (!std::strcmp(id, "judge60")) return s.stats.judge_time;
  if (!std::strcmp(id, "judgeBoss")) return s.stats.judge_bosses;
  if (!std::strcmp(id, "runs50")) return s.stats.runs;
  if (starts("kills")) return s.stats.total_kills;
  if (starts("boss")) return s.stats.bosses_total;
  if (!std::strcmp(id, "evoAll")) return count(s.evo_moves, 64);
  if (starts("evo")) return s.stats.evolutions;
  if (starts("items")) return count(s.items_found, IT_COUNT);
  if (starts("rift")) return s.stats.rift_wins;
  if (!std::strcmp(id, "arenas")) return count(s.arenas, T_COUNT);
  if (starts("leg")) return s.legends_count();
  if (starts("mis")) return s.stats.missions_done;
  if (starts("group")) return s.stats.group_runs;
  return 0;
}

void progress_check_ach(Save& s, int64_t now_ms) {
  for (int i = 0; i < kAchCount && i < 256; ++i) {
    if (s.ach[i]) continue;
    if (ach_value(s, i) < kAch[i].goal) continue;
    s.ach[i] = now_ms ? now_ms : 1;
    grant_reward(s, kAch[i].reward);
    char t[96], sub[64] = "";
    std::snprintf(t, sizeof(t), "¡Logro: %s!", kAch[i].name);
    if (kAch[i].title) std::snprintf(sub, sizeof(sub), "Nuevo título: %s", kAch[i].title);
    g_notices.push(t, sub, i);
  }
}

// ---------------- eventos ----------------

void progress_emit(Save& s, const ProgEvent& e, int64_t now_ms, bool check_ach) {
  progress_ensure(s, now_ms);
  SaveStats& S = s.stats;
  const int64_t now = now_ms ? now_ms : 1;
  switch (e.ev) {
    case EV_KILL:
      if (e.boss) S.bosses_total++;
      if (e.legend > 0 && e.legend <= kMaxDex && !s.legends[e.legend]) s.legends[e.legend] = now;
      break;
    case EV_RIFT:
      S.rift_wins++;
      if (e.id >= 0 && e.id < T_COUNT && !s.arenas[e.id]) s.arenas[e.id] = now;
      break;
    case EV_EVOLVE:
      S.evolutions++;
      if (e.id >= 0 && e.id < 64 && !s.evo_moves[e.id]) s.evo_moves[e.id] = now;
      break;
    case EV_ITEM:
      if (e.id >= 0 && e.id < IT_COUNT && !s.items_found[e.id]) s.items_found[e.id] = now;
      break;
    case EV_RUNEND:
      if (e.group) S.group_runs++;
      break;
    case EV_JUDGE:
      S.judged++;
      break;
    case EV_JUDGEEND:
      S.judge_time = std::max(S.judge_time, int32_t(std::floor(e.time)));
      S.judge_bosses = std::max(S.judge_bosses, int32_t(e.bosses));
      break;
    default: break;
  }
  advance(s.daily, 3, e);
  advance(s.weekly, 2, e);
  if (check_ach) progress_check_ach(s, now_ms);
}

// ---------------- enganches con la partida ----------------

void progress_event(Game& g, int ev, int a, int b, float f) {
  if (!g.save) return;
  ProgEvent e;
  e.ev = ev;
  const Player& p = g.pl();
  switch (ev) {
    case EV_EVOLVE: case EV_ITEM: case EV_RIFT: e.id = a; break;
    case EV_JUDGEEND: e.bosses = a; e.time = f; break;
    case EV_RUNEND: e.level = a; e.coins = b; e.time = f; e.types = p.mon ? p.mon->types : nullptr; break;
    case EV_KILL: e.boss = a != 0; e.legend = b; break;
    default: break;
  }
  progress_emit(*g.save, e, g.now_ms);
  g.dirty = true;
}

// Un enemigo ha caído.
void progress_kill(Game& g, const Enemy& e) {
  // Los legendarios de la Sentencia de Arceus no cuentan como jefes: tienen su contador.
  const bool judge_boss = judgment_active(g) && e.boss;
  if (judge_boss && g.judg.phase == 2) g.judg.downs++;
  if (!g.save) return;
  ProgEvent pe;
  pe.ev = EV_KILL;
  pe.types = e.types;
  pe.boss = judge_boss ? false : e.boss;
  pe.legend = judge_boss ? 0 : ((e.legend || (g.rift.arena && e.boss)) ? e.dex : 0);
  // Los logros se miran de vez en cuando (hay cientos de bajas), salvo jefes.
  static int counter = 0;
  const bool check = pe.boss || pe.legend || (++counter % 30) == 0;
  progress_emit(*g.save, pe, g.now_ms, check);
}

}  // namespace ps

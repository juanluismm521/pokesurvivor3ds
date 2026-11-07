// datapack.cpp — ver datapack.h.
#include "core/datapack.h"

#include <cmath>
#include <cstring>
#include <deque>
#include <vector>

#include "core/gamedata.h"
#include "core/i18n.h"
#include "core/json.h"

namespace ps {

namespace {
DataPackInfo g_cur;

// Los textos del paquete viven aquí y no se borran nunca (un paquete al
// arrancar y, como mucho, otro bajado después: unos cientos de KB).
std::deque<std::string>& pool() { static std::deque<std::string> p; return p; }
const char* keep(const std::string& s) { pool().push_back(s); return pool().back().c_str(); }
std::vector<Tr>& tr_table() { static std::vector<Tr> t; return t; }

// Copia de las tablas de fábrica (para datapack_reset), hecha la primera vez.
struct Factory {
  bool saved = false;
  std::vector<Mon> dex;
  std::vector<MoveDef> moves;
  Ladder ladder[K_COUNT][4], evos[K_COUNT];
  float chart[T_COUNT][T_COUNT];
  LevelStatDef lv[LS_COUNT];
  ItemDef items[IT_COUNT];
  UpgradeDef ups[UP_COUNT];
  std::vector<Tier> tiers;
  std::vector<BossSlot> bosses;
  WeatherDef wx[WX_COUNT];
  float rates[5];
  int16_t refund[6];
  std::vector<MissionDef> missions;
  std::vector<AchDef> ach;
  uint32_t hash = 0;
};
Factory& factory() { static Factory f; return f; }
void save_factory() {
  Factory& f = factory();
  if (f.saved) return;
  f.saved = true;
  f.dex.assign(kDex, kDex + kDexCount);
  f.moves.assign(kMoves, kMoves + kMoveCount);
  std::memcpy(f.ladder, kLadder, sizeof(f.ladder));
  std::memcpy(f.evos, kEvoMods, sizeof(f.evos));
  std::memcpy(f.chart, kTypeChart, sizeof(f.chart));
  std::memcpy(f.lv, kLevelStats, sizeof(f.lv));
  std::memcpy(f.items, kItems, sizeof(f.items));
  std::memcpy(f.ups, kUpgrades, sizeof(f.ups));
  f.tiers.assign(kTiers, kTiers + kTierCount);
  f.bosses.assign(kBossSlots, kBossSlots + kBossSlotCount);
  std::memcpy(f.wx, kWeather, sizeof(f.wx));
  std::memcpy(f.rates, kGachaRates, sizeof(f.rates));
  std::memcpy(f.refund, kGachaRefund, sizeof(f.refund));
  f.missions.assign(kMissions, kMissions + kMissionCount);
  f.ach.assign(kAch, kAch + kAchCount);
  f.hash = kDataHash;
}

uint32_t fnv(const char* p, size_t n) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < n; ++i) { h ^= uint8_t(p[i]); h *= 16777619u; }
  return h;
}

// ---------------- comprobaciones ----------------
struct Check {
  std::string why;
  bool ok() const { return why.empty(); }
  void fail(const char* what) { if (why.empty()) why = what; }
  // Un número finito dentro de [lo, hi].
  double num(const Json& v, double lo, double hi, const char* what) {
    const double x = v.as_num(std::nan(""));
    if (!v.is_num() || !std::isfinite(x) || x < lo || x > hi) { fail(what); return lo; }
    return x;
  }
  const Json& arr(const Json& v, size_t n, const char* what) {
    if (!v.is_arr() || (n && v.size() != n)) fail(what);
    return v;
  }
  void str(const Json& v, const char* what, bool null_ok = false) {
    if (null_ok && v.is_null()) return;
    if (!v.is_str() || v.as_str().size() > 600) fail(what);
  }
};

void check_ladder(Check& c, const Json& l) {
  c.arr(l, 2, "escalera");
  c.str(l[0], "texto de escalera");
  const Json& mods = c.arr(l[1], 0, "mejoras de escalera");
  if (mods.size() > 5) c.fail("escalera con más de 5 mejoras");
  for (const Json& m : mods.elems()) {
    c.arr(m, 2, "mejora de escalera");
    c.num(m[0], 0, MS_NUM - 1, "número de escalera");
    c.num(m[1], -1000, 1000, "valor de escalera");
  }
}

// Todo lo de `d` (los datos) con la forma y los rangos que espera este programa.
void check_data(Check& c, const Json& d) {
  const Json& dex = c.arr(d["dex"], size_t(kDexCount), "Pokédex");
  for (const Json& p : dex.elems()) {
    c.arr(p, 10, "Pokémon");
    c.str(p[0], "nombre de Pokémon");
    c.num(p[1], 1, 100000, "vida"); c.num(p[2], 1, 2000, "velocidad");
    c.num(p[3], 0.01, 20, "ataque"); c.num(p[4], 0.01, 20, "ataque físico"); c.num(p[5], 0.01, 20, "ataque especial");
    c.num(p[6], 1, 1000, "defensa"); c.num(p[7], 1, 1000, "defensa especial");
    c.num(p[8], 1, 5, "rareza"); c.num(p[9], 1, 5000, "total base");
  }
  const Json& mv = c.arr(d["movs"], size_t(kMoveCount), "movimientos");
  for (const Json& m : mv.elems()) {
    c.arr(m, 5, "movimiento");
    c.str(m[0], "nombre de movimiento"); c.str(m[1], "descripción de movimiento");
    const Json& v = c.arr(m[2], MS_NUM, "números de movimiento");
    for (const Json& x : v.elems()) if (!x.is_null()) c.num(x, 0, 100000, "número de movimiento");
    if (!v[MS_CD].is_null()) c.num(v[MS_CD], 0.02, 600, "recarga");
    c.num(m[3], -1, LS_COUNT - 1, "condición de evolución");
    c.str(m[4], "nombre de la evolución", true);
  }
  const Json& es = c.arr(d["escaleras"], K_COUNT, "escaleras");
  for (const Json& k : es.elems()) { c.arr(k, 4, "escalera de una familia"); for (const Json& l : k.elems()) check_ladder(c, l); }
  const Json& ev = c.arr(d["evos"], K_COUNT, "evoluciones");
  for (const Json& l : ev.elems()) if (!l.is_null()) check_ladder(c, l);
  const Json& tb = c.arr(d["tabla"], T_COUNT, "tabla de tipos");
  for (const Json& r : tb.elems()) { c.arr(r, T_COUNT, "fila de la tabla de tipos"); for (const Json& x : r.elems()) c.num(x, 0, 10, "tabla de tipos"); }
  const Json& lv = c.arr(d["nivel"], LS_COUNT, "mejoras de nivel");
  for (const Json& x : lv.elems()) { c.arr(x, 3, "mejora de nivel"); c.str(x[0], "título"); c.str(x[1], "descripción"); c.num(x[2], 0, 1000, "máximo"); }
  const Json& it = c.arr(d["objetos"], IT_COUNT, "objetos");
  for (const Json& x : it.elems()) { c.arr(x, 2, "objeto"); c.str(x[0], "objeto"); c.str(x[1], "objeto"); }
  const Json& up = c.arr(d["mejoras"], UP_COUNT, "mejoras");
  for (const Json& x : up.elems()) {
    c.arr(x, 5, "mejora"); c.str(x[0], "mejora"); c.str(x[1], "mejora");
    c.num(x[2], 1, 100, "máximo de mejora"); c.num(x[3], 1, 30000, "precio de mejora"); c.num(x[4], 0.5, 10, "subida de precio");
  }
  const Json& ti = c.arr(d["tiers"], size_t(kTierCount), "tandas de enemigos");
  for (const Json& x : ti.elems()) {
    c.arr(x, 8, "tanda"); c.num(x[0], 0, 100000, "tanda"); c.num(x[1], 0, 2000, "tanda"); c.num(x[2], 0, 2000, "tanda");
    for (int k = 3; k < 7; ++k) c.num(x[k], 0, 1000, "tanda");
    c.num(x[7], 1, 100, "elenco");
  }
  const Json& bo = c.arr(d["jefes"], size_t(kBossSlotCount), "jefes");
  for (const Json& x : bo.elems()) { c.arr(x, 6, "jefe"); for (int k = 0; k < 6; ++k) c.num(x[k], 0, 10000000, "jefe"); }
  const Json& wx = c.arr(d["climas"], WX_COUNT, "climas");
  for (const Json& r : wx.elems()) { c.arr(r, T_COUNT, "clima"); for (const Json& x : r.elems()) c.num(x, 0, 10, "clima"); }
  const Json& ga = c.arr(d["gacha"], 2, "gacha");
  c.arr(ga[0], 5, "probabilidades del gacha");
  double sum = 0;
  for (const Json& x : ga[0].elems()) sum += c.num(x, 0, 100, "probabilidad del gacha");   // (en %)
  if (std::fabs(sum - 100) > 0.5) c.fail("las probabilidades del gacha no suman 100");
  c.arr(ga[1], 6, "devoluciones del gacha");
  for (const Json& x : ga[1].elems()) c.num(x, 0, 30000, "devolución del gacha");
  const Json& mi = c.arr(d["misiones"], size_t(kMissionCount), "misiones");
  for (const Json& x : mi.elems()) {
    c.arr(x, 2, "misión"); c.arr(x[0], 3, "metas"); c.arr(x[1], 3, "premio");
    for (const Json& y : x[0].elems()) c.num(y, 0, 1e9, "meta");
    for (const Json& y : x[1].elems()) c.num(y, 0, 30000, "premio");
  }
  const Json& lo = c.arr(d["logros"], size_t(kAchCount), "logros");
  for (const Json& x : lo.elems()) {
    c.arr(x, 5, "logro"); c.str(x[0], "logro"); c.str(x[1], "logro"); c.num(x[2], 0, 1e9, "meta de logro");
    c.arr(x[3], 3, "premio de logro");
    for (const Json& y : x[3].elems()) c.num(y, 0, 30000, "premio de logro");
    c.str(x[4], "título de logro", true);
  }
  const Json& en = c.arr(d["en"], 0, "traducciones");
  if (en.size() > 20000) c.fail("demasiadas traducciones");
  for (const Json& x : en.elems()) { c.arr(x, 2, "traducción"); c.str(x[0], "traducción"); c.str(x[1], "traducción"); }
}

bool parse_pack(const std::string& text, Json& pack, std::string& why, DataPackInfo& info) {
  if (text.size() > (4u << 20)) { why = "el paquete es demasiado grande"; return false; }
  if (!pack.parse(text) || !pack.is_obj()) { why = "el paquete no se puede leer"; return false; }
  if (pack["formato"].as_int() != 1) { why = "formato de paquete desconocido"; return false; }
  if (pack["esquema"].as_str() != kDataSchema) { why = "el paquete es para otra versión del juego"; return false; }
  // La huella es la de los bytes del objeto "datos", tal cual vienen.
  const size_t at = text.find("\"datos\":");
  if (at == std::string::npos || text.size() < at + 9 || text.back() != '}') { why = "el paquete está incompleto"; return false; }
  const size_t from = at + 8, len = text.size() - 1 - from;
  const uint32_t h = fnv(text.data() + from, len);
  if (double(h) != pack["huella"].as_num(-1)) { why = "el paquete está estropeado (la huella no cuadra)"; return false; }
  Check c;
  check_data(c, pack["datos"]);
  if (!c.ok()) { why = "el paquete trae datos raros: " + c.why; return false; }
  info.on = true;
  info.web = pack["web"].as_str();
  info.fecha = pack["fecha"].as_str();
  info.huella = h;
  return true;
}

void put_ladder(Ladder& l, const Json& j) {
  l.text = keep(j[0].as_str());
  l.n = int8_t(j[1].size());
  for (int i = 0; i < l.n && i < 5; ++i) { l.m[i].stat = int8_t(j[1][i][0].as_int()); l.m[i].val = float(j[1][i][1].as_num()); }
}
Reward reward(const Json& r) { return Reward{int16_t(r[0].as_int()), int16_t(r[1].as_int()), int16_t(r[2].as_int())}; }
}  // namespace

bool datapack_check(const std::string& text, std::string& why, DataPackInfo* info) {
  Json pack;
  DataPackInfo tmp;
  if (!parse_pack(text, pack, why, tmp)) return false;
  if (info) *info = tmp;
  return true;
}

bool datapack_apply(const std::string& text, std::string& why) {
  Json pack;
  DataPackInfo info;
  if (!parse_pack(text, pack, why, info)) return false;
  save_factory();
  const Json& d = pack["datos"];
  for (int i = 0; i < kDexCount; ++i) {
    const Json& p = d["dex"][i];
    Mon& m = kDex[i];
    m.name = keep(p[0].as_str());
    m.hp = int16_t(p[1].as_int()); m.spd = int16_t(p[2].as_int());
    m.atk = float(p[3].as_num()); m.pa = float(p[4].as_num()); m.sa = float(p[5].as_num());
    m.df = float(p[6].as_int()); m.sd = float(p[7].as_int());
    m.rarity = int8_t(p[8].as_int()); m.bst = int16_t(p[9].as_int());
  }
  for (int i = 0; i < kMoveCount; ++i) {
    const Json& j = d["movs"][i];
    MoveDef& m = kMoves[i];
    m.name = keep(j[0].as_str());
    m.desc = keep(j[1].as_str());
    m.has = 0;
    for (int s = 0; s < MS_NUM; ++s) {
      const Json& v = j[2][s];
      if (v.is_null()) { m.v[s] = 0; continue; }
      m.v[s] = float(v.as_num());
      m.has |= 1u << s;
    }
    m.evo_need = int8_t(j[3].as_int());
    m.evo_name = j[4].is_null() ? nullptr : keep(j[4].as_str());
  }
  for (int k = 0; k < K_COUNT; ++k) {
    for (int l = 0; l < 4; ++l) put_ladder(kLadder[k][l], d["escaleras"][k][l]);
    if (d["evos"][k].is_null()) kEvoMods[k] = Ladder{nullptr, 0, {}};
    else put_ladder(kEvoMods[k], d["evos"][k]);
  }
  for (int a = 0; a < T_COUNT; ++a) for (int b = 0; b < T_COUNT; ++b) kTypeChart[a][b] = float(d["tabla"][a][b].as_num());
  for (int i = 0; i < LS_COUNT; ++i) {
    kLevelStats[i].title = keep(d["nivel"][i][0].as_str());
    kLevelStats[i].desc = keep(d["nivel"][i][1].as_str());
    kLevelStats[i].max = int16_t(d["nivel"][i][2].as_int());
  }
  for (int i = 0; i < IT_COUNT; ++i) { kItems[i].name = keep(d["objetos"][i][0].as_str()); kItems[i].desc = keep(d["objetos"][i][1].as_str()); }
  for (int i = 0; i < UP_COUNT; ++i) {
    const Json& u = d["mejoras"][i];
    kUpgrades[i].name = keep(u[0].as_str()); kUpgrades[i].unit = keep(u[1].as_str());
    kUpgrades[i].max = int8_t(u[2].as_int()); kUpgrades[i].base = int16_t(u[3].as_int()); kUpgrades[i].grow = float(u[4].as_num());
  }
  for (int i = 0; i < kTierCount; ++i) {
    const Json& t = d["tiers"][i];
    kTiers[i] = Tier{int16_t(t[0].as_int()), int16_t(t[1].as_int()), int16_t(t[2].as_int()), float(t[3].as_num()), float(t[4].as_num()),
                     float(t[5].as_num()), float(t[6].as_num()), int8_t(t[7].as_int())};
  }
  for (int i = 0; i < kBossSlotCount; ++i) {
    const Json& b = d["jefes"][i];
    kBossSlots[i] = BossSlot{int16_t(b[0].as_int()), int32_t(b[1].as_int()), int16_t(b[2].as_int()), int16_t(b[3].as_int()),
                             int16_t(b[4].as_int()), int16_t(b[5].as_int())};
  }
  for (int w = 0; w < WX_COUNT; ++w) for (int t = 0; t < T_COUNT; ++t) kWeather[w].mult[t] = float(d["climas"][w][t].as_num());
  for (int r = 0; r < 5; ++r) kGachaRates[r] = float(d["gacha"][0][r].as_num());
  for (int r = 0; r < 6; ++r) kGachaRefund[r] = int16_t(d["gacha"][1][r].as_int());
  for (int i = 0; i < kMissionCount; ++i) {
    const Json& m = d["misiones"][i];
    for (int g = 0; g < 3; ++g) kMissions[i].goals[g] = int32_t(m[0][g].as_int());
    kMissions[i].reward = reward(m[1]);
  }
  for (int i = 0; i < kAchCount; ++i) {
    const Json& a = d["logros"][i];
    kAch[i].name = keep(a[0].as_str()); kAch[i].desc = keep(a[1].as_str());
    kAch[i].goal = int32_t(a[2].as_int()); kAch[i].reward = reward(a[3]);
    kAch[i].title = a[4].is_null() ? nullptr : keep(a[4].as_str());
  }
  std::vector<Tr>& t = tr_table();
  t.clear();
  for (const Json& x : d["en"].elems()) t.push_back(Tr{keep(x[0].as_str()), keep(x[1].as_str())});
  i18n_set_table(t.data(), int(t.size()));
  kDataHash = info.huella;
  g_cur = info;
  return true;
}

const DataPackInfo& datapack_current() { return g_cur; }

void datapack_reset() {
  Factory& f = factory();
  if (!f.saved) return;
  std::copy(f.dex.begin(), f.dex.end(), kDex);
  std::copy(f.moves.begin(), f.moves.end(), kMoves);
  std::memcpy(kLadder, f.ladder, sizeof(f.ladder));
  std::memcpy(kEvoMods, f.evos, sizeof(f.evos));
  std::memcpy(kTypeChart, f.chart, sizeof(f.chart));
  std::memcpy(kLevelStats, f.lv, sizeof(f.lv));
  std::memcpy(kItems, f.items, sizeof(f.items));
  std::memcpy(kUpgrades, f.ups, sizeof(f.ups));
  std::copy(f.tiers.begin(), f.tiers.end(), kTiers);
  std::copy(f.bosses.begin(), f.bosses.end(), kBossSlots);
  std::memcpy(kWeather, f.wx, sizeof(f.wx));
  std::memcpy(kGachaRates, f.rates, sizeof(f.rates));
  std::memcpy(kGachaRefund, f.refund, sizeof(f.refund));
  std::copy(f.missions.begin(), f.missions.end(), kMissions);
  std::copy(f.ach.begin(), f.ach.end(), kAch);
  i18n_set_table(nullptr, 0);
  kDataHash = f.hash;
  g_cur = DataPackInfo();
}

}  // namespace ps

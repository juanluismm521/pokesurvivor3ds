// Desafío semanal (web: challenge.js). Ver weekly.h.
#include "core/weekly.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "core/gamedata.h"

namespace ps {

const WeeklyCondDef kWeeklyConds[WC_COUNT] = {
    {"rain", "Siempre llueve", "Llueve toda la partida: el Agua y el Eléctrico se potencian y el Fuego se debilita.", WX_RAIN, 1},
    {"sun", "Sol abrasador", "Hace sol toda la partida: el Fuego se potencia y el Agua se debilita.", WX_SUN, 1},
    {"noitems", "Sin objetos", "Los objetos equipables no salen: en su lugar, monedas.", WX_NONE, 0},
    {"fast", "Enemigos veloces", "Los enemigos van un 30 % más rápido.", WX_NONE, 1},
    {"glass", "Cañón de cristal", "Tienes la mitad de vida, pero haces un 30 % más de daño.", WX_NONE, 1},
    {"fog", "Niebla", "Hay niebla toda la partida: ves menos, y el Fantasma, el Hada y el Psíquico se potencian.", WX_FOG, 1},
    {"snow", "Nevada", "Nieva toda la partida: el Hielo se potencia, y los demás sufren y van más lentos.", WX_SNOW, 1},
    {"sand", "Tormenta de arena", "Hay tormenta de arena toda la partida: Roca, Tierra y Acero se potencian, y los demás sufren.", WX_SAND, 1},
    {"noheal", "Sin curaciones", "No te curas de ninguna forma: ni bayas, ni manantiales, ni regeneración, ni ataques que curan.", WX_NONE, 1},
    {"night", "Noche cerrada", "Toda la partida es de noche: sólo ves lo que tienes cerca.", WX_NONE, 1},
    {"mono", "Todos de tipo %s", "Todos los Pokémon salvajes y los jefes son de tipo %s.", WX_NONE, 0.5f},
};

static_assert(WC_NOITEMS == 2 && WC_NOHEAL == 8, "game.h (weekly_noheal / weekly_noitems) usa estos números");

namespace {
// Las condiciones de antes de la semana 47 de 2026 y las de después (en su orden de la web)
const int8_t kOld[] = {WC_RAIN, WC_SUN, WC_NOITEMS, WC_FAST, WC_GLASS};
const int8_t kNew[] = {WC_RAIN, WC_SUN, WC_FOG, WC_SNOW, WC_SAND, WC_FAST, WC_GLASS, WC_NOHEAL, WC_NIGHT, WC_MONO};
const char* const kMonoTypes[18] = {"normal", "fire", "water", "grass", "electric", "ice", "fighting", "poison", "ground",
                                    "flying", "psychic", "bug", "rock", "ghost", "dragon", "dark", "steel", "fairy"};

struct Mulberry {
  uint32_t a;
  double next() {
    a += 1831565813u;
    uint32_t t = a;
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return double(t ^ (t >> 14)) / 4294967296.0;
  }
};

uint32_t fnv(const char* s) {
  uint32_t h = 2166136261u;
  for (; *s; ++s) { h ^= uint8_t(*s); h *= 16777619u; }
  return h;
}

int week_num(const char* w) {
  int y = 0, k = 0;
  if (std::sscanf(w, "w%4d-%2d", &y, &k) != 2) return 0;
  return y * 100 + k;
}
}  // namespace

Weekly weekly_of(const char* week) {
  Weekly r;
  std::snprintf(r.week, sizeof(r.week), "%s", week);
  char key[48];
  std::snprintf(key, sizeof(key), "pokekatens-desafio-%s", week);
  Mulberry rng{fnv(key)};
  // Los que pueden salir: ni legendarios ni de rareza 1 o 5, por número de la web
  static int16_t pool[kMaxDex + 1];
  int n = 0;
  for (int i = 0; i < kDexCount; ++i) {
    const Mon& m = kDex[i];
    if (!m.leg && m.rarity >= 2 && m.rarity <= 4) pool[n++] = m.dex;
  }
  std::sort(pool, pool + n, [](int16_t a, int16_t b) { return form_web_id(a) < form_web_id(b); });
  r.dex = n ? pool[int(rng.next() * n)] : 25;
  if (week_num(week) < 202647) {
    r.cond = kOld[int(rng.next() * 5)];
    r.seed = uint32_t(rng.next() * 1000000);
    r.no_items = r.cond == WC_NOITEMS;
    return r;
  }
  float total = 0;
  for (int8_t c : kNew) total += kWeeklyConds[c].w;
  double x = rng.next() * total;
  r.cond = kNew[9];
  for (int8_t c : kNew) if ((x -= kWeeklyConds[c].w) < 0) { r.cond = c; break; }
  r.seed = uint32_t(rng.next() * 1000000);
  r.no_items = rng.next() < 1.0 / 3;
  if (r.cond == WC_MONO) {
    const char* id = kMonoTypes[int(rng.next() * 18)];
    for (int t = 0; t < T_COUNT; ++t) if (!std::strcmp(kTypeId[t], id)) r.type = int8_t(t);
  }
  return r;
}

void weekly_label(const Weekly& w, char* out, int cap) {
  if (w.cond == WC_MONO) std::snprintf(out, size_t(cap), kWeeklyConds[WC_MONO].name, w.type >= 0 ? kTypeName[w.type] : "?");
  else std::snprintf(out, size_t(cap), "%s", kWeeklyConds[w.cond].name);
  if (w.no_items && w.cond != WC_NOITEMS) {
    const size_t l = std::strlen(out);
    std::snprintf(out + l, size_t(cap) - l, " · %s", kWeeklyConds[WC_NOITEMS].name);
  }
}

void weekly_desc(const Weekly& w, char* out, int cap) {
  if (w.cond == WC_MONO) std::snprintf(out, size_t(cap), kWeeklyConds[WC_MONO].desc, w.type >= 0 ? kTypeName[w.type] : "?");
  else std::snprintf(out, size_t(cap), "%s", kWeeklyConds[w.cond].desc);
  if (w.no_items && w.cond != WC_NOITEMS) {
    const size_t l = std::strlen(out);
    std::snprintf(out + l, size_t(cap) - l, " %s", "Y no salen objetos equipables: en su lugar, monedas.");
  }
}

int64_t weekly_ends_in(int64_t now_ms) {
  // (now_ms: la hora de la consola, como las misiones de la semana)
  const int64_t day = 86400000;
  const int64_t days = now_ms >= 0 ? now_ms / day : (now_ms - day + 1) / day;
  const int wd = int(((days % 7) + 7 + 3) % 7);   // 0 = lunes
  return (7 - wd) * day - (now_ms - days * day);
}

}  // namespace ps

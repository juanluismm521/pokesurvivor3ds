// i18n_main.cpp — ps_i18n: cómo sale en inglés cada texto del port.
//   ps_i18n textos.tsv [todo | estricto ignorar.txt]   (de tools/i18n/extraer.py)
// Escribe por cada texto que no se traduce entero (frase o patrón): archivo,
// línea, tipo (NADA / TROZOS / MEDIAS: le queda español) y cómo queda. Con
// «todo», también los que sí. Con «estricto», sale con error si queda alguno
// que no esté en ignorar.txt (la prueba «i18n» de ctest).
#include <cstdio>
#include <cstring>
#include <set>
#include <string>

#include "core/abilities.h"
#include "core/mons.h"
#include "core/gamedata.h"
#include "core/i18n.h"

using namespace ps;

// «datos»: los textos de los datos del juego (movimientos, objetos, logros…).
int check_data(bool strict) {
  i18n_set(LANG_EN);
  int bad = 0;
  std::set<std::string> seen;
  auto chk = [&](const char* where, const char* s) {
    if (!s || !*s || !seen.insert(s).second) return;
    bool mon = false;
    for (int i = 0; i < kDexCount && !mon; ++i) mon = !std::strcmp(s, kDex[i].name);
    if (mon) return;
    const TrKind k = tr_kind(s);
    const char* en = tr(s);
    if ((k == TRK_EXACT || k == TRK_PATTERN) && !tr_looks_spanish(en)) return;
    ++bad;
    std::printf("%s\t%s\t%s\t%s\n", k == TRK_NONE ? "NADA" : k == TRK_FRAGS ? "TROZOS" : "MEDIAS", where, s, en);
  };
  for (int i = 0; i < T_COUNT; ++i) chk("tipo", kTypeName[i]);
  for (int i = 0; i < kMoveCount; ++i) { chk("movimiento", kMoves[i].name); chk("movimiento", kMoves[i].desc); }
  for (int i = 0; i < IT_COUNT; ++i) { chk("objeto", kItems[i].name); chk("objeto", kItems[i].desc); }
  for (int i = 0; i < UP_COUNT; ++i) { chk("mejora", kUpgrades[i].name); chk("mejora", kUpgrades[i].unit); }
  for (int i = 0; i < LS_COUNT; ++i) { chk("estadística", kLevelStats[i].title); chk("estadística", kLevelStats[i].desc); chk("estadística", kNeedName[i]); }
  for (int i = 0; i < kBiomeCount; ++i) chk("bioma", kBiomes[i].name);
  for (int i = 0; i < kArenaThemeCount; ++i) chk("arena", kArenaThemes[i].name);
  for (int i = 0; i < WX_COUNT; ++i) { chk("clima", kWeather[i].name); chk("clima", kWeather[i].hint); }
  for (int i = 0; i < 4; ++i) { chk("poder", kPowers[i].name); chk("poder", kPowers[i].desc); }
  for (int i = 0; i < 9; ++i) chk("banner", kBanners[i].name);
  for (int i = 1; i < 6; ++i) chk("rareza", kRarityName[i]);
  for (int i = 0; i < kTraitCount; ++i) { chk("rasgo", kTraits[i].id); chk("rasgo", kTraits[i].text); }
  for (int i = 0; i < kTestCount; ++i) { chk("test", kTest[i].q); for (const auto& a : kTest[i].a) chk("test", a.text); }
  for (int i = 0; i < kAchCount; ++i) { chk("logro", kAch[i].name); chk("logro", kAch[i].desc); chk("logro", kAch[i].title); }
  for (int i = 0; i < kAchCatCount; ++i) chk("logro", kAchCats[i].name);
  for (int i = 0; i < kMegaCount; ++i) { chk("mega", kMegas[i].mon.name); chk("mega", kMegas[i].stone); }
  // Tienda Kecleon: lo que hace cada habilidad (con su tipo dentro: una por cada tipo
  // principal) y los objetos de la tienda. (Los nombres de naturalezas y habilidades
  // van en las dos lenguas en kNatures / kAbils.)
  for (int a = -1; a < kAbilCount; ++a)
    for (int t = 0; t < T_COUNT; ++t) {
      Mon m{};
      m.types[0] = int8_t(t); m.types[1] = T_NONE;
      char d1[256], d2[160];
      abil_desc(a, &m, d1, sizeof(d1), d2, sizeof(d2));
      const std::string a1 = d1, a2 = d2;
      chk("habilidad", a1.c_str());
      chk("habilidad", a2.c_str());
    }
  for (int i = 0; i < BAG_COUNT; ++i) { chk("tienda", kShopItems[i].name); chk("tienda", kShopItems[i].desc); }
  std::fprintf(stderr, "datos: %zu textos · %d no salen enteros en inglés\n", seen.size(), bad);
  return strict && bad ? 1 : 0;
}

int main(int argc, char** argv) {
  if (argc >= 2 && !std::strcmp(argv[1], "datos")) return check_data(argc > 2 && !std::strcmp(argv[2], "estricto"));
  if (argc < 2) { std::fprintf(stderr, "uso: ps_i18n textos.tsv [todo]\n"); return 2; }
  const bool all = argc > 2 && !std::strcmp(argv[2], "todo");
  const bool strict = argc > 3 && !std::strcmp(argv[2], "estricto");
  std::set<std::string> skip;
  if (strict) {
    FILE* g = std::fopen(argv[3], "rb");
    if (!g) { std::fprintf(stderr, "no puedo leer %s\n", argv[3]); return 2; }
    char ln[1024];
    while (std::fgets(ln, sizeof(ln), g)) {
      std::string t(ln);
      while (!t.empty() && (t.back() == '\n' || t.back() == '\r')) t.pop_back();
      if (!t.empty() && t[0] != '#') skip.insert(t);
    }
    std::fclose(g);
  }
  int failed = 0;
  FILE* f = std::fopen(argv[1], "rb");
  if (!f) return 2;
  i18n_set(LANG_EN);
  char line[4096];
  int n = 0, bad = 0, frags = 0;
  while (std::fgets(line, sizeof(line), f)) {
    std::string l(line);
    while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
    const size_t a = l.find('\t'), b = l.find('\t', a + 1);
    if (a == std::string::npos || b == std::string::npos) continue;
    std::string s = l.substr(b + 1);
    if (skip.count(s)) continue;
    bool mon = false;
    for (int i = 0; i < kDexCount && !mon; ++i) mon = s == kDex[i].name;
    if (mon) continue;
    for (size_t p; (p = s.find("\\n")) != std::string::npos;) s.replace(p, 2, "\n");
    // Los {x} de printf: se prueba con un número (lo que suele ir).
    std::string probe = s;
    for (size_t p; (p = probe.find("{x}")) != std::string::npos;) probe.replace(p, 3, "7");
    ++n;
    const TrKind k = tr_kind(probe.c_str());
    const bool es = tr_looks_spanish(tr(probe.c_str()));
    if ((k == TRK_EXACT || k == TRK_PATTERN) && es) {
      ++frags; ++failed;
      std::printf("MEDIAS\t%s\t%s\t%s\n", l.substr(0, b).c_str(), s.c_str(), tr(probe.c_str()));
      continue;
    }
    if (k == TRK_EXACT || k == TRK_PATTERN) { if (all) std::printf("OK\t%s\t%s\t%s\n", l.substr(0, b).c_str(), s.c_str(), tr(probe.c_str())); continue; }
    if (k == TRK_FRAGS) ++frags; else ++bad;
    ++failed;
    std::printf("%s\t%s\t%s\t%s\n", k == TRK_FRAGS ? "TROZOS" : "NADA", l.substr(0, b).c_str(), s.c_str(), tr(probe.c_str()));
  }
  std::fclose(f);
  std::fprintf(stderr, "%d textos · %d sin traducir · %d a trozos o a medias · %d bien\n", n, bad, frags, n - bad - frags);
  if (strict && failed) std::fprintf(stderr, "i18n: %d textos del port no salen enteros en inglés (añádelos a src/core/lang_en_port.cpp)\n", failed);
  return strict && failed ? 1 : 0;
}

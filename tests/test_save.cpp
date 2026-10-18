// test_save.cpp — partida guardada, misiones, logros y gacha.
#include <doctest/doctest.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "core/gacha.h"
#include "core/json.h"
#include "core/progress.h"
#include "core/save_io.h"

using namespace ps;

namespace {
std::string read_file(const std::string& path) {
  std::string s;
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return s;
  char buf[65536];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
  std::fclose(f);
  return s;
}
// La partida ocupa ~50 KB: siempre en el montón.
std::unique_ptr<Save> fresh() { std::unique_ptr<Save> s(new Save()); return s; }
}  // namespace

TEST_CASE("misiones: mismo sorteo y textos que la web") {
  Json g;
  REQUIRE(g.parse(read_file(std::string(PS_GOLDEN_DIR) + "/golden.json")));
  const Json& ms = g["missions"];
  for (size_t i = 0; i < ms.size(); ++i) {
    auto s = fresh();
    s->created = int64_t(ms[i]["created"].as_num());
    progress_ensure_ids(*s, ms[i]["d"].as_str().c_str(), ms[i]["w"].as_str().c_str());
    for (int k = 0; k < 3; ++k) {
      const Json& e = ms[i]["daily"][k];
      REQUIRE(s->daily[k].def >= 0);
      CHECK(std::string(kMissions[s->daily[k].def].id) == e[0].as_str());
      CHECK(s->daily[k].goal == e[1].as_int());
      CHECK(std::string(s->daily[k].param >= 0 ? kTypeId[s->daily[k].param] : "") == (e[2].is_str() ? e[2].as_str() : std::string()));
    }
    for (int k = 0; k < 2; ++k) {
      const Json& e = ms[i]["weekly"][k];
      CHECK(std::string(kMissions[s->weekly[k].def].id) == e[0].as_str());
      CHECK(s->weekly[k].goal == e[1].as_int());
    }
    for (int k = 0; k < 5; ++k) {
      char t[128];
      mission_text(k < 3 ? s->daily[k] : s->weekly[k - 3], t, sizeof(t));
      CHECK(std::string(t) == ms[i]["texts"][k].as_str());
    }
  }
  const Json& ach = g["ach"];
  REQUIRE(int(ach.size()) == kAchCount);
  for (int i = 0; i < kAchCount; ++i) CHECK(kAch[i].goal == ach[i][1].as_int());
}

TEST_CASE("misiones: claves de día y semana ISO con el reloj de la consola") {
  char d[16], w[16];
  auto at = [](int y, int m, int dd) {   // ms de esa fecha a las 12:00
    struct { int y, m, d; } t{y, m, dd};
    // días desde 1970 (algoritmo civil)
    int yy = t.y - (t.m <= 2);
    const long era = (yy >= 0 ? yy : yy - 399) / 400;
    const unsigned yoe = unsigned(yy - era * 400);
    const unsigned doy = (153 * (t.m + (t.m > 2 ? -3 : 9)) + 2) / 5 + t.d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (int64_t(era) * 146097 + int64_t(doe) - 719468) * 86400000LL + 12 * 3600000LL;
  };
  period_ids(at(2026, 10, 18), d, 16, w, 16);
  CHECK(std::string(d) == "d20261018");
  CHECK(std::string(w) == "w2026-42");
  period_ids(at(2027, 1, 1), d, 16, w, 16);
  CHECK(std::string(w) == "w2026-53");
  period_ids(at(2024, 12, 30), d, 16, w, 16);
  CHECK(std::string(w) == "w2025-01");
  period_ids(at(2026, 1, 5), d, 16, w, 16);
  CHECK(std::string(w) == "w2026-02");
}

TEST_CASE("misiones y logros avanzan con los eventos") {
  auto s = fresh();
  s->created = 77;
  progress_ensure(*s, 1);
  // Fuerza una diaria de derrotar Pokémon y comprueba que avanza y se recoge.
  for (int i = 0; i < kMissionCount; ++i) if (!std::strcmp(kMissions[i].id, "kill")) { s->daily[0] = Mission(); s->daily[0].def = int8_t(i); s->daily[0].goal = 5; }
  const int8_t types[2] = {T_FIRE, T_NONE};
  for (int k = 0; k < 5; ++k) { ProgEvent e; e.types = types; progress_emit(*s, e, 1); }
  CHECK(mission_done(s->daily[0]));
  const int coins = s->coins;
  CHECK(progress_claim(*s, 0, false, 1));
  CHECK(s->coins > coins);
  CHECK(s->stats.missions_done == 1);
  CHECK_FALSE(progress_claim(*s, 0, false, 1));
  // Logro "Primer compañero": tener 1 Pokémon.
  save_grant_dex(*s, 4, FROM_STARTER, 5);
  progress_check_ach(*s, 5);
  int own1 = -1;
  for (int i = 0; i < kAchCount; ++i) if (!std::strcmp(kAch[i].id, "own1")) own1 = i;
  REQUIRE(own1 >= 0);
  CHECK(s->ach[own1] == 5);
}

TEST_CASE("guardado: ida y vuelta por JSON y archivo con copia de seguridad") {
  auto s = fresh();
  new_save(*s, "Ash", 1760000000000LL);
  save_grant_dex(*s, 25, FROM_STARTER, 1760000000001LL);
  save_grant_shiny_dex(*s, 6, FROM_GACHA, 1760000000002LL);
  s->partner = 25; s->partner_shiny = false; s->starter = 25;
  s->pupg[25][UP_ATK] = 3; s->pupg[25][UP_HP] = 99;   // 99 > máximo: se recorta al leer
  s->pbind[25][0] = int8_t(move_index("thunder-shock"));
  s->pban[25][0] = int8_t(move_index("tackle"));
  s->t1 = 12; s->t10 = 2; s->pity[1] = 33;
  s->legends[150] = 9; s->arenas[T_PSYCHIC] = 9; s->items_found[IT_LIFEORB] = 9; s->evo_moves[3] = 9; s->ach[0] = 9;
  s->stats.runs = 7; s->stats.best_time = 1234; s->stats.judged = 1;
  std::snprintf(s->trait, sizeof(s->trait), "valiente");
  s->trait_scores[0] = 4;
  progress_ensure_ids(*s, "d20261018", "w2026-42");
  const std::string j = save_to_json(*s);
  auto r = fresh();
  REQUIRE(save_from_json(*r, j));
  CHECK(std::string(r->name) == "Ash");
  CHECK(r->coins == kStartCoins);
  CHECK(r->owns(25));
  CHECK(r->owns(6));
  CHECK(r->owns_shiny(6));
  CHECK(r->owned_from[25] == FROM_STARTER);
  CHECK(r->partner == 25);
  CHECK(r->pupg[25][UP_ATK] == 3);
  CHECK(r->pupg[25][UP_HP] == kUpgrades[UP_HP].max);
  CHECK(r->pbind[25][0] == move_index("thunder-shock"));
  CHECK(r->pban[25][0] == move_index("tackle"));
  CHECK(r->t1 == 12); CHECK(r->t10 == 2); CHECK(r->pity[1] == 33);
  CHECK(r->legends[150] == 9); CHECK(r->arenas[T_PSYCHIC] == 9); CHECK(r->items_found[IT_LIFEORB] == 9);
  CHECK(r->stats.runs == 7); CHECK(r->stats.best_time == 1234);
  CHECK(std::string(r->trait) == "valiente");
  CHECK(std::string(r->m_day) == "d20261018");
  CHECK(r->daily[0].def == s->daily[0].def);

  // Archivo: escribir dos veces deja save.json y save.bak; un save.json roto usa la copia.
  const char* dir = "test_save_dir";
  std::remove("test_save_dir/save.json"); std::remove("test_save_dir/save.bak");
  if (std::system("mkdir -p test_save_dir") != 0) FAIL("no se pudo crear la carpeta");
  REQUIRE(save_write_file(dir, *s));
  s->coins = 999;
  REQUIRE(save_write_file(dir, *s));
  auto a = fresh();
  CHECK(save_read_file(dir, *a) == 1);
  CHECK(a->coins == 999);
  FILE* f = std::fopen("test_save_dir/save.json", "wb");
  std::fputs("{\"v\":3,\"name\":\"Ash\",\"coi", f);   // apagón a mitad
  std::fclose(f);
  CHECK(save_read_file(dir, *a) == 2);
  CHECK(a->coins == kStartCoins);
}

TEST_CASE("guardado: lee una partida de la web (claves y versiones viejas)") {
  const char* web = R"({"v":2,"name":"Misty","created":1700000000000,"coins":"1500","owned":{"7":{"at":1,"from":"starter"},"9999":{"at":1}},
    "shiny":{},"partner":7,"upgrades":{"atk":4},"tickets":{"t1":-3},"pity":12,"stats":{"runs":3,"bestTime":95.7},
    "pbind":{"7":["water-gun","water-gun","no-existe"]},"pban":{"7":["tackle","tackle","bite","harden","agility"]}})";
  auto s = fresh();
  REQUIRE(save_from_json(*s, web));
  CHECK(s->coins == 1500);
  CHECK(s->owns(7));
  CHECK(s->owned_count() == 1);              // el 9999 no existe: fuera
  CHECK(s->pupg[7][UP_ATK] == 4);            // v2 → v3: mejoras al compañero
  CHECK(s->t1 == 0);                         // negativos → 0
  CHECK(s->pity[1] == 12);                   // v1: pity numérico
  CHECK(s->stats.best_time == 95);
  CHECK(s->pbind[7][0] == move_index("water-gun"));
  CHECK(s->pbind[7][1] == -1);               // repetido
  CHECK(s->pbind[7][2] == -1);               // no existe
  CHECK(s->pban[7][2] == move_index("harden"));   // sin repetir, como mucho 3
}

TEST_CASE("gacha: probabilidades, garantías y repetidos") {
  auto s = fresh();
  Rng rng(42);
  int by[6] = {};
  PullResult out[10];
  int pulls = 0, max_pity = 0;
  s->t1 = 1 << 30;
  for (int i = 0; i < 20000; ++i) {
    const int n = gacha_pull(*s, 10, 1 + i % 9, false, rng, 1, out);
    REQUIRE(n == 10);
    bool any4 = false;
    for (int k = 0; k < n; ++k) { by[out[k].rarity]++; if (out[k].rarity >= 4) any4 = true; }
    CHECK(any4);   // la ×10 trae al menos un 4★
    pulls += n;
    for (int gidx = 1; gidx <= 9; ++gidx) max_pity = std::max(max_pity, int(s->pity[gidx]));
  }
  CHECK(max_pity < kPity5);
  // Sin contar la garantía de la ×10 las tasas son 45/30/17/6,5/1,5: aquí, cerca.
  CHECK(by[1] / double(pulls) == doctest::Approx(0.45).epsilon(0.08));
  CHECK(by[5] / double(pulls) > 0.015);
  CHECK(s->stats.pulls == pulls);
  // Sin tickets no se tira.
  auto e = fresh();
  e->t1 = 0;
  CHECK(gacha_pull(*e, 1, 1, false, rng, 1, out) == 0);
  // Con un ticket ×10.
  e->t10 = 1;
  CHECK(gacha_pull(*e, 10, 1, true, rng, 1, out) == 10);
  CHECK(e->t10 == 0);
}

#include "core/i18n.h"

TEST_CASE("idioma: las traducciones al inglés dan lo mismo que la web") {
  Json g;
  REQUIRE(g.parse(read_file(std::string(PS_GOLDEN_DIR) + "/golden.json")));
  const Json& t = g["i18n"];
  REQUIRE(t.size() > 100);
  i18n_set(LANG_EN);
  int bad = 0;
  for (size_t i = 0; i < t.size(); ++i) {
    const std::string got = tr(t[i][0].as_str().c_str());
    if (got != t[i][1].as_str()) { bad++; MESSAGE(t[i][0].as_str() << " → " << got << " (web: " << t[i][1].as_str() << ")"); }
  }
  CHECK(bad == 0);
  i18n_set(LANG_ES);
  CHECK(std::string(tr("Volver")) == "Volver");
}

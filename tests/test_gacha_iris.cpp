// Gacha ×100 y Amuletos Iris (web, 22 y 23-10-2026).
#include <doctest/doctest.h>

#include <set>
#include <string>

#include <memory>

#include "core/gacha.h"
#include "core/save_io.h"

using namespace ps;

TEST_CASE("gacha ×100: 100 tickets o 10 ×10; diez ×10 seguidas, cada una con su Épica") {
  std::unique_ptr<Save> s(new Save());
  save_from_json(*s, "{}");
  Rng rng(42);
  static PullResult out[100];
  s->t1 = 99;
  CHECK(gacha_pull(*s, 100, 1, false, rng, 1, out) == 0);   // (no llega)
  CHECK(s->t1 == 99);
  s->t1 = 250;
  REQUIRE(gacha_pull(*s, 100, 1, false, rng, 1, out) == 100);
  CHECK(s->t1 == 150);
  for (int k = 0; k < 10; ++k) {
    bool epic = false;
    for (int i = 0; i < 10; ++i) epic = epic || out[k * 10 + i].rarity >= 4;
    CHECK(epic);
  }
  s->t10 = 9;
  CHECK(gacha_pull(*s, 100, 1, true, rng, 1, out) == 0);
  s->t10 = 12;
  CHECK(gacha_pull(*s, 100, 1, true, rng, 1, out) == 100);
  CHECK(s->t10 == 2);
  CHECK(s->stats.pulls == 200);
}

TEST_CASE("Amuleto Iris: con la Pokédex de la región completa, shiny al 2 % en su banner") {
  std::unique_ptr<Save> s(new Save());
  save_from_json(*s, "{}");
  int have, total;
  gacha_dex_of(*s, 1, have, total);
  CHECK(total == 151);
  CHECK_FALSE(gacha_has_iris(*s, 1));
  CHECK(gacha_shiny_rate(*s, 1) == doctest::Approx(0.01f));
  for (int i = 0; i < kDexCount; ++i) if (kDex[i].gen == 1) s->owned_at[kDex[i].dex] = 1;
  CHECK(gacha_has_iris(*s, 1));
  CHECK_FALSE(gacha_has_iris(*s, 2));
  CHECK(gacha_shiny_rate(*s, 1) == doctest::Approx(0.02f));
  // Medido: muchas tiradas en Kanto con el amuleto (los shiny no se repiten:
  // se cuentan los que salen, nuevos o no) — alrededor del 2 %.
  Rng rng(7);
  static PullResult out[100];
  int shiny = 0, n = 0;
  for (int r = 0; r < 60; ++r) {
    s->t1 = 100;
    std::fill(std::begin(s->shiny_at), std::end(s->shiny_at), 0);
    const int got = gacha_pull(*s, 100, 1, false, rng, 1, out);
    for (int i = 0; i < got; ++i) shiny += out[i].shiny;
    n += got;
  }
  const double rate = double(shiny) / n;
  MESSAGE("shiny con el Amuleto Iris: " << rate * 100 << " %");
  CHECK(rate > 0.014);
  CHECK(rate < 0.027);
}

TEST_CASE("gacha: el cartel del banner, legendarios de verdad (y los que eligió el dueño)") {
  int16_t f[3];
  // (el dueño, 2026-10-30) Kanto: Dragonite en vez de Articuno; Paldea: Koraidon y
  // Gholdengo (Miraidon no está en la Pokédex del juego); «Todas»: tres famosos.
  REQUIRE(gacha_featured(1, f) == 3);
  CHECK((f[0] == 149 || f[1] == 149 || f[2] == 149));
  CHECK((f[0] != 144 && f[1] != 144 && f[2] != 144));
  REQUIRE(gacha_featured(9, f) == 3);
  CHECK(f[0] == 1007);
  CHECK(f[1] == 1000);
  CHECK(mon_by_dex(f[2])->leg);
  REQUIRE(gacha_featured(kGachaAll, f) == 3);
  for (int i = 0; i < 3; ++i) CHECK(mon_by_dex(f[i])->leg);
  for (int gen = 2; gen <= 8; ++gen) {
    const int n = gacha_featured(gen, f);
    REQUIRE(n == 3);
    int legs = 0;
    for (int i = 0; i < kDexCount; ++i) legs += kDex[i].gen == gen && kDex[i].leg && kDex[i].rarity == 5;
    for (int i = 0; i < n && i < legs; ++i) CHECK_MESSAGE(mon_by_dex(f[i])->leg, mon_by_dex(f[i])->name);
  }
}

TEST_CASE("gacha: el banner «Todas» tira de todas las regiones, con su pity y su Amuleto Iris") {
  Save s;
  new_save(s, "t", 1);
  s.t1 = 1000;
  Rng rng(7);
  PullResult out[100];
  std::set<int> gens;
  for (int k = 0; k < 5; ++k) {
    REQUIRE(gacha_pull(s, 100, kGachaAll, false, rng, 1, out) == 100);
    for (const PullResult& r : out) gens.insert(mon_by_dex(r.dex)->gen);
  }
  CHECK(gens.size() >= 7);
  CHECK(s.pity[kGachaAll] < kPity5);
  int have, total;
  gacha_dex_of(s, kGachaAll, have, total);
  CHECK(total == kDexCount);
  CHECK(std::string(gacha_banner(kGachaAll).name) == "Todas");
  CHECK(gacha_next(9) == kGachaAll);
  CHECK(gacha_next(kGachaAll) == 1);
  CHECK(gacha_prev(1) == kGachaAll);
}

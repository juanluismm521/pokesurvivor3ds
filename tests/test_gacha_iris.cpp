// Gacha ×100 y Amuletos Iris (web, 22 y 23-10-2026).
#include <doctest/doctest.h>

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

TEST_CASE("gacha: el cartel del banner, sólo legendarios de verdad") {
  for (int gen = 1; gen <= 9; ++gen) {
    int16_t f[3];
    const int n = gacha_featured(gen, f);
    REQUIRE(n == 3);
    int legs = 0;
    for (int i = 0; i < kDexCount; ++i) legs += kDex[i].gen == gen && kDex[i].leg && kDex[i].rarity == 5;
    for (int i = 0; i < n && i < legs; ++i) CHECK_MESSAGE(mon_by_dex(f[i])->leg, mon_by_dex(f[i])->name);
  }
}

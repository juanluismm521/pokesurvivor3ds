// test_rng.cpp — la aleatoriedad con semilla es repetible y está en rango.
#include <doctest/doctest.h>

#include "core/rng.h"

TEST_CASE("Rng: misma semilla, misma secuencia") {
  ps::Rng a(1234), b(1234), c(99);
  bool distinta = false;
  for (int i = 0; i < 1000; ++i) {
    uint32_t va = a.next_u32();
    CHECK(va == b.next_u32());
    if (va != c.next_u32()) distinta = true;
  }
  CHECK(distinta);
}

TEST_CASE("Rng: rangos") {
  ps::Rng r(7);
  double suma = 0;
  for (int i = 0; i < 100000; ++i) {
    float f = r.next();
    REQUIRE(f >= 0.0f);
    REQUIRE(f < 1.0f);
    suma += f;
    int k = r.range_int(3, 5);
    REQUIRE(k >= 3);
    REQUIRE(k <= 5);
  }
  CHECK(suma / 100000 == doctest::Approx(0.5).epsilon(0.01));
}

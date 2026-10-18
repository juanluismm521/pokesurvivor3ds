// test_version.cpp — prueba mínima del esqueleto.
#include <doctest/doctest.h>

#include <cstring>

#include "core/version.h"

TEST_CASE("el título del juego es el del port") {
  CHECK(std::strcmp(ps::kGameTitle, "PokéSurvivor 3DS") == 0);
}

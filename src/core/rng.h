// rng.h — aleatoriedad con semilla (sustituye a Math.random de la web).
// La misma semilla da siempre la misma secuencia: pruebas y repeticiones.
#pragma once
#include <cstdint>

namespace ps {

class Rng {
 public:
  explicit Rng(uint32_t seed = 1) { reseed(seed); }
  void reseed(uint32_t seed);

  uint32_t next_u32();
  // [0, 1), como Math.random().
  float next() { return (next_u32() >> 8) * (1.0f / 16777216.0f); }
  // U.rand(a, b)
  float range(float a, float b) { return a + next() * (b - a); }
  // U.randInt(a, b): entero en [a, b] incluidos.
  int range_int(int a, int b) { return a + int(next() * float(b - a + 1)); }
  bool chance(float p) { return next() < p; }

 private:
  uint32_t s_[4];
};

}  // namespace ps

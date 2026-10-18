// rng.cpp — xoshiro128** sembrado con splitmix32.
#include "core/rng.h"

namespace ps {

namespace {
uint32_t splitmix32(uint32_t& x) {
  uint32_t z = (x += 0x9e3779b9u);
  z = (z ^ (z >> 16)) * 0x85ebca6bu;
  z = (z ^ (z >> 13)) * 0xc2b2ae35u;
  return z ^ (z >> 16);
}
inline uint32_t rotl(uint32_t x, int k) { return (x << k) | (x >> (32 - k)); }
}  // namespace

void Rng::reseed(uint32_t seed) {
  uint32_t x = seed;
  for (auto& s : s_) s = splitmix32(x);
}

uint32_t Rng::next_u32() {
  const uint32_t result = rotl(s_[1] * 5, 7) * 9;
  const uint32_t t = s_[1] << 9;
  s_[2] ^= s_[0];
  s_[3] ^= s_[1];
  s_[1] ^= s_[2];
  s_[0] ^= s_[3];
  s_[2] ^= t;
  s_[3] = rotl(s_[3], 11);
  return result;
}

}  // namespace ps

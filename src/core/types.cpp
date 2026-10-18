// types.cpp
#include "core/types.h"

#include <cstring>

#include "core/util.h"

namespace ps {

float type_mult(int atk, const int8_t def[2]) {
  if (atk < 0 || atk >= T_COUNT) return 1;
  float m = 1;
  for (int i = 0; i < 2; ++i) {
    const int d = def[i];
    if (d < 0) continue;
    const float g = kTypeChart[atk][d];
    if (g == 2.0f) m *= 1.6f;
    else if (g == 0.5f) m *= 0.6f;
    else if (g == 0.0f) m *= 0.3f;
  }
  return clamp(m, 0.3f, 2.4f);
}

int type_from_id(const char* id) {
  for (int i = 0; i < T_COUNT; ++i) if (!std::strcmp(kTypeId[i], id)) return i;
  return T_NONE;
}

}  // namespace ps

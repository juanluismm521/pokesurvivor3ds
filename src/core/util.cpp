// util.cpp
#include "core/util.h"

#include <cstdio>

namespace ps {

void mmss(float s, char* out, int n) {
  const int m = int(std::floor(s / 60));
  const int sec = int(std::floor(std::fmod(s, 60.0f)));
  std::snprintf(out, n, "%d:%02d", m, sec);
}

}  // namespace ps

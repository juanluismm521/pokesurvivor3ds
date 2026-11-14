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

// Formas regionales: su id en la web (500037 = Vulpix de Alola) y al revés.
int form_web_id(int dex) {
  if (dex <= kNationalDex) return dex;
  for (int i = 0; i < kFormWebCount; ++i) if (kFormWeb[i].dex == dex) return kFormWeb[i].web;
  return dex;
}
int form_from_web(int web_id) {
  if (web_id >= 1 && web_id <= kNationalDex) return web_id;
  for (int i = 0; i < kFormWebCount; ++i) if (kFormWeb[i].web == web_id) return kFormWeb[i].dex;
  return 0;
}

}  // namespace ps

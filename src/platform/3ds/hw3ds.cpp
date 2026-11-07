// hw3ds.cpp — ver hw3ds.h.
#include "platform/3ds/hw3ds.h"

#include <3ds.h>

#include "core/halloween.h"
#include "platform/3ds/cloud3ds.h"

namespace ps {
namespace hw3ds {

namespace {
int g_force = -1;
// La hora UTC: la del servidor si se sabe; si no, la de la consola tomada como
// hora de España (la mayoría la tiene en hora local).
int64_t utc_now(bool& from_server) {
  int64_t t;
  from_server = cloud3ds::server_now_ms(t);
  if (from_server) return t;
  const int64_t local = int64_t(osGetTime()) - 2208988800000LL;   // (osGetTime cuenta desde 1900, en hora local)
  // De hora de España a UTC (+1 en invierno, +2 en verano): madrid_midnight sabe cuál toca.
  int y, m, d;
  madrid_date(local, y, m, d);
  const int64_t day0 = madrid_midnight(y, m, d);
  const int64_t local_day0 = (local / 86400000) * 86400000;
  return day0 + (local - local_day0);
}
}  // namespace

int64_t utc_ms() {
  bool srv;
  return utc_now(srv);
}

void force(int v) { g_force = v; }
bool forced() { return g_force >= 0; }

bool on() {
  if (g_force >= 0) return g_force != 0;
  bool srv;
  return halloween_at(utc_now(srv));
}

bool countdown(bool& on_now, int64_t& ms) {
  bool srv;
  const int64_t t = utc_now(srv);
  if (g_force == 1) {   // (forzado: lo que quede si de verdad es Halloween; si no, 3 días)
    bool o; int64_t m;
    on_now = true;
    ms = halloween_countdown(t, o, m) && o ? m : 3 * 86400000LL;
    return true;
  }
  if (g_force == 0) return false;
  return halloween_countdown(t, on_now, ms);
}

}  // namespace hw3ds
}  // namespace ps

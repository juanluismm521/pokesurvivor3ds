// saver3ds.cpp — ver saver3ds.h.
#include "platform/3ds/saver3ds.h"

#include <3ds.h>

#include <string>

#include "core/save_io.h"

namespace ps {
namespace saver3ds {

namespace {
std::string g_dir;
Save g_pending, g_work;        // la última copia pedida / la que se está escribiendo
volatile bool g_has = false, g_busy = false, g_quit = false;
LightLock g_lock, g_raw;
LightEvent g_ev, g_idle;
Thread g_thr = nullptr;
int g_done = 0, g_slowest = 0;

void write_now(const Save& s) {
  const u64 t0 = osGetTime();
  lock_raw();
  const std::string text = save_full_json(s).dump();
  unlock_raw();
  save_write_text(g_dir.c_str(), text);
  const int ms = int(osGetTime() - t0);
  if (ms > g_slowest) g_slowest = ms;
  g_done++;
}

void worker(void*) {
  while (true) {
    LightEvent_Wait(&g_ev);
    while (true) {
      LightLock_Lock(&g_lock);
      if (!g_has) { g_busy = false; LightLock_Unlock(&g_lock); LightEvent_Signal(&g_idle); break; }
      g_work = g_pending;   // (la copia se hace con el cerrojo: el principal no la toca a medias)
      g_has = false;
      g_busy = true;
      LightLock_Unlock(&g_lock);
      write_now(g_work);
    }
    if (g_quit) break;
  }
}
}  // namespace

void init(const char* dir) {
  g_dir = dir;
  LightLock_Init(&g_lock);
  LightLock_Init(&g_raw);
  LightEvent_Init(&g_ev, RESET_ONESHOT);
  LightEvent_Init(&g_idle, RESET_STICKY);
  LightEvent_Signal(&g_idle);
  s32 prio = 0x30;
  svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
  // Menos prioridad que el juego: usa el rato que el principal espera al
  // refresco de la pantalla. En el núcleo 1 si se puede (la vieja lo comparte
  // con el sistema al 30 %); si no, en el del juego.
  g_thr = threadCreate(worker, nullptr, 64 * 1024, prio + 1, 1, false);
  if (!g_thr) g_thr = threadCreate(worker, nullptr, 64 * 1024, prio + 1, -2, false);
}

void save(const Save& s) {
  if (!g_thr) { write_now(s); return; }   // (sin hilo: como antes)
  LightLock_Lock(&g_lock);
  g_pending = s;
  g_has = true;
  LightEvent_Clear(&g_idle);
  LightLock_Unlock(&g_lock);
  LightEvent_Signal(&g_ev);
}

void flush() {
  if (!g_thr) return;
  LightEvent_Signal(&g_ev);
  LightEvent_WaitTimeout(&g_idle, 5000000000LL);   // (5 s como mucho)
}

void shutdown() {
  if (!g_thr) return;
  flush();
  g_quit = true;
  LightEvent_Signal(&g_ev);
  threadJoin(g_thr, 5000000000ULL);
  threadFree(g_thr);
  g_thr = nullptr;
}

void lock_raw() { LightLock_Lock(&g_raw); }
void unlock_raw() { LightLock_Unlock(&g_raw); }
int saves_done() { return g_done; }
int slowest_ms() { return g_slowest; }

}  // namespace saver3ds
}  // namespace ps

// progress.cpp — misiones, logros y estadísticas (progress.js de la web).
// El juego avisa de lo que pasa con progress_event(); esto actualiza la
// partida guardada. (La parte del guardado llega en la Fase 5: de momento sólo
// lo que la partida necesita, como el contador de la Sentencia.)
#include "core/game.h"

namespace ps {

void progress_event(Game& g, int ev, int a, int b, float f) {
  (void)g; (void)ev; (void)a; (void)b; (void)f;
}

// Un enemigo ha caído.
void progress_kill(Game& g, const Enemy& e) {
  // Los legendarios de la Sentencia de Arceus no cuentan como jefes: tienen su contador.
  if (judgment_active(g) && e.boss) {
    if (g.judg.phase == 2) g.judg.downs++;
    progress_event(g, EV_KILL, 0, 0);
    return;
  }
  const int legend = (e.legend || (g.rift.arena && e.boss)) ? e.dex : 0;
  progress_event(g, EV_KILL, e.boss ? 1 : 0, legend);
}

}  // namespace ps

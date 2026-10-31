// red_main.cpp — herramienta: ¿cómo va el juego en grupo por internet en casas de
// verdad? El anfitrión de siempre (el anfitrión junta los botones) contra el servidor
// de anfitrión (relay.py LOCK), en varias redes, con el servidor real y relojes que no
// van exactos (ver online_sim.h). Compílalo sin sanitizers (build-rel) para medir.
//   ps_red [segundos por partida] [repeticiones] [colchón del reloj elástico]
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#include "online_sim.h"

using namespace ps;
using online_sim::Home;

int main(int argc, char** argv) {
  const int secs = argc > 1 ? std::atoi(argv[1]) : 40;
  const int reps = argc > 2 ? std::atoi(argv[2]) : 2;
  const int cushion = argc > 3 ? std::atoi(argv[3]) : 3;
  online_sim::RelayServer srv;
  if (!srv.start(PS_RELAY_PY, 27000 + int(getpid() % 2000))) return 1;
  struct Case { const char* name; int n; Home h[4]; };
  // ping, vaivén, picos, pérdidas, reloj
  const Case cases[] = {
      {"dos casas, wifi normal (60 ms)", 2, {{60, 15, 0.01, 0.01, 1.0}, {60, 15, 0.01, 0.01, 1.003}}},
      {"wifi buena (40) y regular (120, ±40, 3 %)", 2, {{40, 10, 0.01, 0.01, 1.0}, {120, 40, 0.02, 0.03, 0.997}}},
      {"anfitrión con wifi regular (120) e invitado buena (40)", 2, {{120, 40, 0.02, 0.03, 1.002}, {40, 10, 0.01, 0.01, 1.0}}},
      {"tres (60, 60, 100)", 3, {{60, 15, 0.01, 0.01, 1.0}, {60, 15, 0.01, 0.01, 1.003}, {100, 25, 0.01, 0.02, 0.998}}},
  };
  std::printf("%d s por partida (los 5 primeros no cuentan) · %d repeticiones · colchón %d\n", secs, reps, cushion);
  std::printf("latencia = cuánto va por delante lo que pulsas · tirones y saltos por minuto · velocidad en pasos/s\n\n");
  for (const Case& c : cases) {
    std::printf("== %s\n", c.name);
    for (int relay = 0; relay < 2; ++relay) {
      double ahead[4] = {}, hit[4] = {}, bur[4] = {}, spd[4] = {};
      bool ok = true;
      for (int r = 0; r < reps; ++r) {
        const online_sim::Result res = online_sim::play(srv.port, c.n, c.h, relay != 0, secs, -1, cushion, 4242 + uint32_t(r) * 7);
        if (!res.same || res.desync || res.compared == 0) ok = false;
        for (int i = 0; i < c.n; ++i) { ahead[i] += res.ahead_ms[i] / reps; hit[i] += res.hitches[i] / reps; bur[i] += res.bursts[i] / reps; spd[i] += res.speed[i] / reps; }
      }
      std::printf("  %-9s", relay ? "servidor" : "anfitrión");
      for (int i = 0; i < c.n; ++i) std::printf(" | J%d: %4.0f ms · %5.1f tirones · %5.1f saltos · %4.1f/s", i, ahead[i], hit[i], bur[i], spd[i]);
      std::printf(" | %s\n", ok ? "iguales" : "¡SEPARADAS!");
      std::fflush(stdout);
    }
  }
  return 0;
}

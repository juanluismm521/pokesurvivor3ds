// balance_main.cpp — herramienta: ¿cuánto ayuda la megapiedra comparada con
// los objetos de la web? El bot juega partidas enteras (sin ser inmortal) con
// cada objeto puesto desde el principio y apunta cuánto aguanta y cuánto mata.
//   ps_balance [partidas_por_pokemon] [segundos] [nivel_mejoras 0..1] [inmortal]
// (nivel_mejoras: fracción del máximo de cada mejora del menú; por defecto 1)
#include <cstdio>
#include <cstdlib>
#include <memory>

#include "core/mega.h"
#include "sim.h"

using namespace ps;

int main(int argc, char** argv) {
  const int seeds = argc > 1 ? std::atoi(argv[1]) : 6;
  const double secs = argc > 2 ? std::atof(argv[2]) : 1800;
  const double ups = argc > 3 ? std::atof(argv[3]) : 1;
  const bool immortal = argc > 4 && std::atoi(argv[4]) != 0;   // (mide sólo el ataque: derrotas en el tiempo)
  RunConfig base;
  for (int u = 0; u < UP_COUNT; ++u) base.upgrades[u] = uint8_t(kUpgrades[u].max * ups + 0.5);
  // Pokémon con mega de distintos tipos y estilos (físicos, especiales, lentos…).
  const int dexes[] = {94, 6, 448, 282, 248, 257, 130, 9};
  struct Cfg { const char* name; int item; };
  const Cfg cfgs[] = {{"sin objeto", IT_NONE}, {"Cinta Elección", IT_CHOICEBAND}, {"Vidasfera", IT_LIFEORB},
                      {"Garra Rápida", IT_QUICKCLAW}, {"Restos", IT_LEFTOVERS}, {"Megapiedra", IT_MEGA_A}};
  std::unique_ptr<Game> g(new Game());
  std::printf("%-15s %9s %9s %8s %7s %6s\n", "objeto", "aguanta", "derrotas", "nivel", "muertes", "megas");
  for (const Cfg& c : cfgs) {
    double t = 0, k = 0, lv = 0;
    int dead = 0, n = 0, megas = 0;
    for (int dex : dexes)
      for (int s = 0; s < seeds; ++s) {
        bool was = false;
        SimHook hook = [&](Game& gg) {
          Player& p = gg.players[0];
          // El objeto, siempre (aunque el bot coja otro).
          if (gg.state == ST_PLAYING && p.item != c.item && c.item != IT_NONE) player_equip(gg, p, c.item);
          if (c.item == IT_NONE && p.item >= 0 && gg.state == ST_PLAYING) player_equip(gg, p, IT_NONE);
          if (p.mega >= 0 && !was) megas++;
          was = p.mega >= 0;
        };
        const SimStats st = sim_run(*g, 5000 + s * 31 + dex, dex, secs, immortal, &hook, &base);
        t += st.seconds; k += st.kills; lv += st.level; dead += st.dead; n++;
      }
    std::printf("%-15s %8.0fs %9.0f %8.1f %4d/%-2d %6.1f\n", c.name, t / n, k / n, lv / n, dead, n, double(megas) / n);
    std::fflush(stdout);
  }
}

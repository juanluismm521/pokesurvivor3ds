// treasure_main.cpp — herramienta: ¿está equilibrado el tesoro de la Luminosfera?
// Para Pokémon lentos, normales y rápidos y en varios minutos de partida: cuántas
// veces se llega a tiempo (kTreasureTime) y cuánto se tarda (ver treasure_sim.h).
//   ps_tesoro [partidas_por_caso] [nivel_mejoras 0..1]
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "treasure_sim.h"

using namespace ps;

int main(int argc, char** argv) {
  const int seeds = argc > 1 ? std::atoi(argv[1]) : 8;
  const float ups = argc > 2 ? float(std::atof(argv[2])) : 0.5f;
  // Shuckle (80, el más lento), Hypno (106, la mediana), Weavile (130).
  const int dexes[] = {213, 97, 461};
  const double minutes[] = {4, 12, 22};
  std::printf("tesoro: %.0f s, a %.0f-%.0f px · mejoras al %.0f %%\n", kTreasureTime, kTreasureMin, kTreasureMax, ups * 100);
  std::printf("%-9s %6s %8s %9s %9s %9s %9s %7s %8s\n", "Pokémon", "minuto", "llega", "mediana", "máx.", "distancia", "camino", "isla", "vida");
  for (int dex : dexes)
    for (double m : minutes) {
      std::vector<float> ts;
      int ok = 0, n = 0, island = 0;
      double d = 0, hp = 0, walk = 0;
      for (int s = 0; s < seeds; ++s) {
        const TreasureTry r = treasure_try(uint32_t(1000 + s * 17 + dex), dex, m, ups);
        if (!r.placed) continue;
        n++; d += r.dist; hp += r.hp_lost; walk += r.walk;
        if (!r.path) island++;
        if (r.reached) { ok++; ts.push_back(r.secs); }
      }
      std::sort(ts.begin(), ts.end());
      std::printf("%-9s %6.0f %4d/%-3d %8.1fs %8.1fs %8.0fpx %8.0fpx %7d %7.0f%%\n", mon_by_dex(dex)->name, m, ok, n,
                  ts.empty() ? 0.0 : double(ts[ts.size() / 2]), ts.empty() ? 0.0 : double(ts.back()), n ? d / n : 0.0,
                  n ? walk / std::max(1, n - island) : 0.0, island, n ? hp / n * 100 : 0.0);
      std::fflush(stdout);
    }
}

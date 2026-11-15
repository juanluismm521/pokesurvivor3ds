// levels_main.cpp — herramienta: cómo se sube de nivel y cuánto se aguanta (equilibrio).
// El bot juega partidas enteras (mortal) con varios Pokémon y apunta el nivel a los 5, 10,
// 15 y 20 minutos, cuánto aguanta, si llega a Eternatus y si le gana.
//   ps_levels [partidas_por_pokemon] [segundos] [mejoras 0..1] [coste_xp] [rapido 0/1] [vida_a] [vida_p]
// (coste_xp: × lo del port; 0.333 = como la web. vida_a/p: la curva de port_hp_k; sin ellos, la del juego)
// Inmortal: mide la presión (enemigos vivos de media, daño recibido por minuto en vidas
// enteras) y lo que hace el jugador (jefes vencidos, Eternatus), minuto a minuto.
#include <cstdio>
#include <cstdlib>
#include <memory>

#include "core/diff.h"
#include "sim.h"

namespace ps { extern float g_bal_xp_k; extern float g_bal_hp_a, g_bal_hp_p; }
using namespace ps;

int main(int argc, char** argv) {
  const int seeds = argc > 1 ? std::atoi(argv[1]) : 3;
  const double secs = argc > 2 ? std::atof(argv[2]) : 1500;
  const double ups = argc > 3 ? std::atof(argv[3]) : 0.4;
  g_bal_xp_k = argc > 4 ? float(std::atof(argv[4])) : 1;
  const int quick = argc > 5 ? std::atoi(argv[5]) : 0;
  if (argc > 6) g_bal_hp_a = float(std::atof(argv[6]));
  if (argc > 7) g_bal_hp_p = float(std::atof(argv[7]));
  RunConfig base;
  for (int u = 0; u < UP_COUNT; ++u) base.upgrades[u] = uint8_t(kUpgrades[u].max * ups + 0.5);
  base.mode = uint8_t(quick);
  const int dexes[] = {6, 25, 94, 448, 130, 9, 257, 3, 133, 143};
  const int B = 5;   // tramos de 5 minutos (equivalentes)
  double lv[B] = {}, crowd[B] = {}, hurt[B] = {}, smp[B] = {}, dps[B] = {};
  double bosses = 0, ewon = 0, kills = 0, eterna_t = 0; int n = 0, en = 0;
  std::unique_ptr<Game> g(new Game());
  for (int dex : dexes)
    for (int s = 0; s < seeds; ++s) {
      float last_hp = -1; double et0 = -1, last_dmg = 0;
      SimHook hook = [&](Game& gg) {
        const Player& p = gg.players[0];
        const double teq = gg.time / (quick ? 0.65 : 1.0);
        const int b = int(teq / 300);
        if (b >= 0 && b < B) {
          int alive = 0;
          for (const Enemy& e : gg.enemies) if (!e.dead) alive++;
          crowd[b] += alive; lv[b] += p.level; smp[b] += 1;
          // (daño por segundo, en «enemigos normales» de ese momento: el de la web, sin el factor del port)
          dps[b] += (p.dmg_dealt - last_dmg) * 60 / diff_mob_hp(0, gg.time, 1);
          if (last_hp >= 0 && p.hp < last_hp) hurt[b] += (last_hp - p.hp) / std::fmax(1.0f, p.max_hp);
        }
        last_hp = p.hp; last_dmg = p.dmg_dealt;
        if (gg.eterna.st != 0 && et0 < 0) et0 = gg.time;
        if (gg.eterna_wins > 0 && et0 >= 0) { eterna_t += gg.time - et0; et0 = 1e9; }
      };
      const SimStats st = sim_run(*g, 7000 + s * 37 + dex, dex, secs, true, &hook, &base);
      bosses += st.bosses; kills += st.kills; ewon += g->eterna_wins > 0; n++;
      if (g->eterna_wins > 0) en++;
    }
  std::printf("vida a=%.2f p=%.2f · ", double(g_bal_hp_a), double(g_bal_hp_p));
  std::printf("coste_xp %.2f %s · %d partidas · jefes %.1f · derrotas %.0f · Eternatus vencido %d/%d (%.0f s de media)\n", double(g_bal_xp_k),
              quick ? "rápido" : "normal", n, bosses / n, kills / n, int(ewon), n, en ? eterna_t / en : 0.0);
  std::printf("  min (equiv.)  nivel  enemigos  vidas/min  poder (enemigos/s)\n");
  for (int b = 0; b < B; ++b)
    if (smp[b] > 0) std::printf("  %2d-%2d        %5.1f  %7.1f  %8.2f  %8.2f\n", b * 5, b * 5 + 5, lv[b] / smp[b], crowd[b] / smp[b], hurt[b] / smp[b] * 3600, dps[b] / smp[b]);
}

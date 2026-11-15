// powers_main.cpp — herramienta: cuánto vale cada poder de Eternatus (y «Megaevolución +»).
// El bot (inmortal, al nivel típico del minuto 20) juega del 20:10 al 28:20 con un Pokémon con mega;
// apunta su daño por segundo en «enemigos normales» de cada momento y las veces que le pegan.
//   ps_powers [partidas_por_pokemon] [mp_time] [mp_cd] [mp_atk]
#include <cstdio>
#include <cstdlib>
#include <memory>

#include "core/diff.h"
#include "core/mega.h"
#include "core/power.h"
#include "sim.h"

using namespace ps;

int main(int argc, char** argv) {
  const int seeds = argc > 1 ? std::atoi(argv[1]) : 3;
  if (argc > 2) g_mp_time = float(std::atof(argv[2]));
  if (argc > 3) g_mp_cd = float(std::atof(argv[3]));
  if (argc > 4) g_mp_atk = float(std::atof(argv[4]));
  const int dexes[] = {6, 94, 448, 282, 248, 257, 130, 9, 3, 65};
  // (sin piedra y sin poder; con piedra y sin poder; Megaevolución de Eternatus sin piedra;
  //  con piedra: Megaevolución +, Dinamax, Z, Teracristal)
  struct Cfg { const char* name; bool stone; int power; };
  const Cfg cfgs[] = {{"nada", false, PWK_NONE}, {"piedra", true, PWK_NONE}, {"Mega (Eternatus)", false, PWK_MEGA},
                      {"piedra + Mega +", true, PWK_KEEP}, {"piedra + Dinamax", true, PWK_DYNA}, {"piedra + Z", true, PWK_Z},
                      {"piedra + Tera", true, PWK_TERA}};
  std::printf("Megaevolución +: +%.0f s, −%.0f s de recarga, ×%.2f\n", double(g_mp_time), double(g_mp_cd), double(g_mp_atk));
  std::unique_ptr<Game> g(new Game());
  for (const Cfg& c : cfgs) {
    double dps = 0, hurt = 0; int n = 0;
    for (int dex : dexes)
      for (int s = 0; s < seeds; ++s) {
        RunConfig base;
        for (int u = 0; u < UP_COUNT; ++u) base.upgrades[u] = uint8_t(kUpgrades[u].max * 0.4 + 0.5);
        base.demo_time = uint16_t(kEternaAt + 10);
        base.demo_level = 22;
        bool set = false;
        double d0 = 0, t0 = 0, dsum = 0; float last_hp = -1; double h = 0;
        SimHook hook = [&](Game& gg) {
          Player& p = gg.players[0];
          if (!set && gg.state == ST_PLAYING) {
            set = true;
            if (c.stone) player_equip(gg, p, IT_MEGA_A);
            if (c.power != PWK_NONE) {
              power_offer(gg, p);
              int pick = -1;
              for (int i = 0; i < p.npower_opts; ++i) if (p.power_opts[i] == c.power) pick = i;
              if (pick < 0) { p.choice = CH_NONE; gg.state = ST_PLAYING; } else power_choose(gg, p, pick);
            }
            d0 = p.dmg_dealt; t0 = gg.time;
          }
          if (set) {
            if (c.stone && p.item != IT_MEGA_A && gg.state == ST_PLAYING) player_equip(gg, p, IT_MEGA_A);   // (aunque el bot coja otro)
            if (!c.stone && is_mega_stone(p.item) && gg.state == ST_PLAYING) player_equip(gg, p, IT_NONE);
            dsum += (p.dmg_dealt - d0) / diff_mob_hp(0, gg.time, 1); d0 = p.dmg_dealt;
            if (last_hp >= 0 && p.hp < last_hp) h += (last_hp - p.hp) / std::fmax(1.0f, p.max_hp);
            last_hp = p.hp;
          }
        };
        sim_run(*g, 9100 + s * 13 + dex, dex, kEternaAt + 10 + 490, true, &hook, &base);
        const double secs = g->time - t0;
        if (secs > 60) { dps += dsum / secs; hurt += h / secs * 60; n++; }
      }
    std::printf("%-20s poder %6.3f  vidas/min %5.2f  (%d)\n", c.name, dps / n, hurt / n, n);
    std::fflush(stdout);
  }
}

// sim_main.cpp — herramienta: el bot juega una partida y escribe un resumen.
//   ps_sim <semilla> <dex> <segundos> [inmortal] [carpeta_imagenes] [cada_s]
// Con carpeta, guarda una imagen PPM del estado cada `cada_s` segundos (mapa,
// enemigos en rojo, proyectiles en amarillo, objetos en azul, jugador en verde).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

#include "sim.h"

using namespace ps;

static void dump(Game& g, const char* dir, int n) {
  const int W = 400, H = 240;   // la pantalla de arriba, a escala 0,5
  std::vector<uint8_t> img(W * H * 3);
  const float l = g.camera.left(), t = g.camera.top();
  auto put = [&](int x, int y, uint8_t r, uint8_t gg, uint8_t b) {
    if (x < 0 || y < 0 || x >= W || y >= H) return;
    uint8_t* p = &img[(y * W + x) * 3]; p[0] = r; p[1] = gg; p[2] = b;
  };
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x) {
      const float wx = l + x * 2, wy = t + y * 2;
      const uint8_t k = g.world.terrain_at(World::tile_of(wx), World::tile_of(wy));
      if (k == TR_WALL) put(x, y, 90, 80, 70); else if (k == TR_LIQUID) put(x, y, 40, 90, 170); else put(x, y, 40, 90, 45);
    }
  auto disc = [&](float wx, float wy, float r, uint8_t R, uint8_t G, uint8_t B) {
    const int cx = int((wx - l) / 2), cy = int((wy - t) / 2), rr = std::max(1, int(r / 2));
    for (int y = -rr; y <= rr; ++y) for (int x = -rr; x <= rr; ++x) if (x * x + y * y <= rr * rr) put(cx + x, cy + y, R, G, B);
  };
  g.world.for_props_in_rect(l, t, l + 800, t + 480, [&](Prop& p) { disc(p.x, p.y, p.r, p.solid ? 120 : 60, p.solid ? 70 : 140, 40); });
  for (const Pickup& q : g.pickups) disc(q.x, q.y, 4, 80, 200, 255);
  for (const Enemy& e : g.enemies) disc(e.x, e.y, e.r, e.boss ? 255 : 220, e.boss ? 200 : 40, 40);
  for (const Projectile& q : g.projectiles) disc(q.x, q.y, std::max(3.0f, q.r), 255, q.friendly ? 240 : 80, q.friendly ? 60 : 200);
  const Player& p = g.pl();
  disc(p.x, p.y, p.r, 60, 255, 90);
  char path[256];
  std::snprintf(path, sizeof(path), "%s/f%04d.ppm", dir, n);
  FILE* f = std::fopen(path, "wb");
  if (!f) return;
  std::fprintf(f, "P6\n%d %d\n255\n", W, H);
  std::fwrite(img.data(), 1, img.size(), f);
  std::fclose(f);
}

int main(int argc, char** argv) {
  const uint32_t seed = argc > 1 ? uint32_t(std::atoi(argv[1])) : 1;
  const int dex = argc > 2 ? std::atoi(argv[2]) : 4;
  const double secs = argc > 3 ? std::atof(argv[3]) : 300;
  const bool imm = argc > 4 && std::atoi(argv[4]);
  const char* dir = argc > 5 ? argv[5] : nullptr;
  const double every = argc > 6 ? std::atof(argv[6]) : 30;
  std::unique_ptr<Game> g(new Game());
  int frame = 0;
  double next = every;
  int last_st = 0, last_ph = 0;
  SimHook hook = [&](Game& gg) {
    if (dir && gg.time >= next) { dump(gg, dir, frame++); next += every; }
    // Eternatus: cuándo empieza cada fase y cómo acaba
    if (gg.eterna.st != last_st || gg.eterna.phase != last_ph) {
      const Enemy* e = gg.enemy_by_id(gg.eterna.boss_id);
      std::printf("[eternatus] t=%.0f s · estado %d · fase %d · vida %.0f/%.0f · jugador %.0f/%.0f\n", gg.time, gg.eterna.st, gg.eterna.phase,
                  e ? e->hp : 0.0f, e ? e->max_hp : 0.0f, gg.pl().hp, gg.pl().max_hp);
      last_st = gg.eterna.st; last_ph = gg.eterna.phase;
    }
    static int rb_ph = 0; static bool rb_on = false;
    if (gg.rboss.on != rb_on || (gg.rboss.on && gg.rboss.phase != rb_ph)) {
      const Enemy* e = gg.enemy_by_id(gg.rboss.boss_id);
      std::printf("[grieta] t=%.0f %s · fase %d · %s · vida %.0f/%.0f · grietas ganadas %d\n", gg.time, gg.rboss.on ? "pelea" : "fin", gg.rboss.phase,
                  e ? mon_by_dex(e->dex)->name : "-", e ? e->hp : 0.0f, e ? e->max_hp : 0.0f, gg.rift.n);
      rb_on = gg.rboss.on; rb_ph = gg.rboss.phase;
    }
    static double nx = 0;
    if (gg.eterna.st == 2 && gg.time >= nx) {
      nx = gg.time + 20;
      const Enemy* e = gg.enemy_by_id(gg.eterna.boss_id);
      const Player& q = gg.pl();
      std::printf("  · t=%.0f vida %.0f · jugador %.0f/%.0f en (%.0f,%.0f), Eternatus en (%.0f,%.0f) · daño hecho %.0f · enemigos %d\n", gg.time, e ? e->hp : 0.0f, q.hp, q.max_hp,
                  q.x - gg.eterna.cx, q.y - gg.eterna.cy, e ? e->x - gg.eterna.cx : 0.0f, e ? e->y - gg.eterna.cy : 0.0f, q.dmg_dealt, gg.enemies.n);
    }
  };
  const SimStats s = sim_run(*g, seed, dex, secs, imm, &hook);
  const Player& p = g->pl();
  std::printf("tiempo %.1f s · nivel %d · derrotados %d · jefes %d · monedas %d · vida %.0f/%.0f · daño hecho %.0f\n",
              s.seconds, s.level, s.kills, s.bosses, s.coins, p.hp, p.max_hp, p.dmg_dealt);
  std::printf("enemigos %d (máx %d) · proyectiles máx %d · suelo %d · grietas %d · cartas %d · %s\n",
              g->enemies.n, s.max_enemies, s.max_proj, g->pickups.n, s.rifts, s.cards, s.dead ? "cae" : "vivo");
  std::printf("Eternatus vencido %d · poder %d · evolución %d (dex %d)\n", g->eterna_wins, int(p.epower), int(p.evo_stage), int(p.dex));
  for (int i = 0; i < p.nmoves; ++i) std::printf("  mov %s nv%d%s\n", p.moves[i].name(), p.moves[i].lvl, p.moves[i].evolved ? " (evolucionado)" : "");
  return 0;
}

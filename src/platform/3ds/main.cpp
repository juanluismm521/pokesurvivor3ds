// main.cpp — arranque y bucle principal en la 3DS.
//   Paso fijo de 60 Hz (si un fotograma tarda, se dan hasta 3 pasos para que la
//   partida no vaya a cámara lenta), controles y modo AUTOTEST: si existe
//   sdmc:/3ds/pokesurvivor-3ds/autotest.txt, el bot juega solo y escribe FPS y
//   memoria libre en autotest.log, y el juego se cierra al terminar.
//   autotest.txt puede llevar "segundos dex inmortal" (por defecto 180 25 1).
#include <3ds.h>
#include <citro2d.h>

#include <sys/stat.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

#include "core/bot.h"
#include "core/game.h"
#include "core/version.h"
#include "platform/3ds/render.h"

using namespace ps;

namespace {
constexpr const char* kDir = "sdmc:/3ds/pokesurvivor-3ds";

struct Autotest {
  bool on = false;
  float seconds = 180;
  int dex = 25;
  bool immortal = true;
  FILE* log = nullptr;
  Bot bot;
  double next_log = 10;
  int min_fps = 999;
  double fps_sum = 0;
  int fps_n = 0;
};

void read_autotest(Autotest& a) {
  char path[96];
  std::snprintf(path, sizeof(path), "%s/autotest.txt", kDir);
  FILE* f = std::fopen(path, "r");
  if (!f) return;
  a.on = true;
  int imm = 1;
  if (std::fscanf(f, "%f %d %d", &a.seconds, &a.dex, &imm) >= 1) a.immortal = imm != 0;
  std::fclose(f);
  std::snprintf(path, sizeof(path), "%s/autotest.log", kDir);
  a.log = std::fopen(path, "w");
}

u32 mem_free() { return osGetMemRegionFree(MEMREGION_APPLICATION); }

void start(Game& g, Renderer& r, int dex, uint32_t seed) {
  RunConfig cfg;
  cfg.dex = dex;
  cfg.seed = seed;
  cfg.judged = 1;
  g.start_run(cfg);
  r.preload_run(g);
  r.card_cursor = 0;
}
}  // namespace

int main() {
  osSetSpeedupEnable(true);
  romfsInit();
  gfxInitDefault();
  C3D_Init(C3D_DEFAULT_CMDBUF_SIZE * 2);
  C2D_Init(8192);
  C2D_Prepare();
  mkdir("sdmc:/3ds", 0777);
  mkdir(kDir, 0777);

  C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
  C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

  static Renderer r;
  if (!r.init()) {
    consoleInit(GFX_BOTTOM, nullptr);
    std::printf("No se pudieron cargar los recursos (romfs).\nSTART: salir\n");
    while (aptMainLoop()) { hidScanInput(); if (hidKeysDown() & KEY_START) break; gspWaitForVBlank(); }
    gfxExit();
    return 1;
  }

  std::unique_ptr<Game> g(new Game());
  Autotest at;
  read_autotest(at);
  const u32 mem0 = mem_free();
  if (at.log) {
    bool n3ds = false;
    APT_CheckNew3DS(&n3ds);
    std::fprintf(at.log, "%s %s · autotest %.0f s · dex %d · %s · %s\n", kGameTitle, kVersion, at.seconds, at.dex,
                 at.immortal ? "inmortal" : "normal", n3ds ? "New 3DS" : "3DS");
    std::fprintf(at.log, "memoria libre al arrancar: %lu KB · lineal %lu KB\n", (unsigned long)(mem0 / 1024), (unsigned long)(linearSpaceFree() / 1024));
    std::fflush(at.log);
  }
  start(*g, r, at.on ? at.dex : 4, at.on ? 12345u : uint32_t(osGetTime()));
  g->debug_immortal = at.on && at.immortal;
  if (at.on) at.bot.init(12345);
  if (at.log) {
    std::fprintf(at.log, "precargados %d Pokémon · %lu KB de sprites · memoria libre %lu KB\n", r.sprites.loaded(),
                 (unsigned long)(r.sprites.bytes() / 1024), (unsigned long)(mem_free() / 1024));
    std::fflush(at.log);
  }

  u64 last = osGetTime();
  double acc = 0;
  u64 fps_t = last;
  int fps_frames = 0;
  int bond_first = -1;

  while (aptMainLoop()) {
    hidScanInput();
    const u32 down = hidKeysDown(), held = hidKeysHeld();
    if (!at.on && (down & KEY_START) && (held & KEY_SELECT)) break;   // START+SELECT: salir
    if (at.on && (down & KEY_START)) break;

    Input in;
    if (at.on) {
      // El bot decide (y resuelve él las pantallas de elección).
      Input bi;
      if (at.bot.decide(*g, bi)) in = bi;
      if (g->state == ST_OVER) start(*g, r, at.dex, uint32_t(osGetTime()));
    } else {
      touchPosition tp;
      hidTouchRead(&tp);
      const bool touch = down & KEY_TOUCH;
      if (g->state == ST_PLAYING) {
        circlePosition cp;
        hidCircleRead(&cp);
        float ax = std::abs(cp.dx) > 18 ? cp.dx / 150.0f : 0, ay = std::abs(cp.dy) > 18 ? -cp.dy / 150.0f : 0;
        if (held & KEY_DLEFT) ax = -1;
        if (held & KEY_DRIGHT) ax = 1;
        if (held & KEY_DUP) ay = -1;
        if (held & KEY_DDOWN) ay = 1;
        const float m = std::hypot(ax, ay);
        if (m > 1) { ax /= m; ay /= m; }
        in.ax = ax; in.ay = ay;
        if (down & KEY_L) in.cycle = -1;
        if (down & (KEY_R | KEY_X)) in.cycle = 1;
        if (touch && tp.py >= 150 && tp.py < 216) {
          const Player& p = g->pl();
          const float S = 66, GAP = 8, x0 = (320 - (p.nslots * S + (p.nslots - 1) * GAP)) / 2;
          for (int i = 0; i < p.nslots; ++i) if (tp.px >= x0 + i * (S + GAP) && tp.px < x0 + i * (S + GAP) + S) in.select = int8_t(i);
        }
        if (g->judg.phase == 1 && (down & (KEY_A | KEY_B | KEY_TOUCH))) g->skip_judgment();
        if (down & KEY_START) g->toggle_pause();
      } else if (g->state == ST_PAUSED) {
        if (down & KEY_START) g->toggle_pause();
        if (down & KEY_SELECT) g->quit_run();
      } else if (g->state == ST_LEVELUP) {
        if (down & KEY_DUP) r.card_cursor = (r.card_cursor + g->ncards - 1) % g->ncards;
        if (down & KEY_DDOWN) r.card_cursor = (r.card_cursor + 1) % g->ncards;
        if (touch && tp.py >= 92 && tp.py < 92 + 44 * g->ncards) { r.card_cursor = (tp.py - 92) / 44; g->pick_card(r.card_cursor); r.card_cursor = 0; }
        else if (down & KEY_A) { g->pick_card(r.card_cursor); r.card_cursor = 0; }
        else if (down & KEY_Y) g->reroll_cards();
      } else if (g->state == ST_CHOICE && g->choice == CH_ITEM) {
        if (down & (KEY_DUP | KEY_DDOWN)) r.card_cursor ^= 1;
        if (touch && tp.py >= 96 && tp.py < 204) { r.card_cursor = (tp.py - 96) / 54; g->pick_item(r.card_cursor == 1); r.card_cursor = 0; }
        else if (down & KEY_A) { g->pick_item(r.card_cursor == 1); r.card_cursor = 0; }
      } else if (g->state == ST_CHOICE && g->choice == CH_BOND) {
        int8_t loose[4]; int nl, grow;
        const int mode = player_bond_options(g->pl(), loose, nl, grow);
        if (nl > 0) {
          if (down & KEY_DUP) r.card_cursor = (r.card_cursor + nl - 1) % nl;
          if (down & KEY_DDOWN) r.card_cursor = (r.card_cursor + 1) % nl;
          if (touch && tp.py >= 92 && tp.py < 92 + 32 * nl) r.card_cursor = (tp.py - 92) / 32;
          if ((down & KEY_A) || touch) {
            const int pick = loose[clamp(r.card_cursor, 0, nl - 1)];
            if (mode == 1) { int one[1] = {pick}; g->pick_bond(one, 1); bond_first = -1; r.card_cursor = 0; }
            else if (bond_first < 0) bond_first = pick;
            else if (pick != bond_first) { int two[2] = {bond_first, pick}; g->pick_bond(two, 2); bond_first = -1; r.card_cursor = 0; }
          }
        }
      } else if (g->state == ST_OVER) {
        if (down & KEY_A) start(*g, r, g->pl().dex, uint32_t(osGetTime()));
      }
    }

    // --- paso fijo ---
    const u64 now = osGetTime();
    acc += double(now - last);
    last = now;
    if (acc > 100) acc = 100;
    int steps = 0;
    while (acc >= 1000.0 / 60 && steps < 3) {
      g->step(&in);
      in.select = -1; in.cycle = 0;
      acc -= 1000.0 / 60;
      steps++;
    }

    // --- dibujo ---
    r.begin_frame(*g);
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
    C2D_SceneBegin(top);
    r.draw_top(*g);
    C2D_TargetClear(bottom, C2D_Color32(16, 22, 34, 255));
    C2D_SceneBegin(bottom);
    r.draw_bottom(*g, at.on);
    C3D_FrameEnd(0);

    // --- FPS ---
    fps_frames++;
    if (now - fps_t >= 1000) {
      r.fps = int(std::lround(fps_frames * 1000.0 / double(now - fps_t)));
      fps_t = now;
      fps_frames = 0;
      if (at.on && g->time > 3) {
        at.fps_sum += r.fps; at.fps_n++;
        if (r.fps < at.min_fps) at.min_fps = r.fps;
      }
    }
    if (at.on && at.log && g->time >= at.next_log) {
      at.next_log += 10;
      std::fprintf(at.log, "t=%4.0f s · %2d FPS (media %.1f, mín %d) · enemigos %d · proyectiles %d · partículas %d · Pokémon cargados %d (%lu KB) · libre %lu KB · lineal %lu KB\n",
                   g->time, r.fps, at.fps_n ? at.fps_sum / at.fps_n : 0.0, at.min_fps == 999 ? 0 : at.min_fps, g->enemies.n, g->projectiles.n,
                   g->fx.parts.n, r.sprites.loaded(), (unsigned long)(r.sprites.bytes() / 1024), (unsigned long)(mem_free() / 1024),
                   (unsigned long)(linearSpaceFree() / 1024));
      std::fflush(at.log);
    }
    if (at.on && g->time >= at.seconds) break;
  }

  if (at.log) {
    std::fprintf(at.log, "FIN · %.0f s de partida · FPS media %.1f · mín %d · nivel %d · derrotados %d · libre %lu KB\n", g->time,
                 at.fps_n ? at.fps_sum / at.fps_n : 0.0, at.min_fps == 999 ? 0 : at.min_fps, g->pl().level, g->pl().kills,
                 (unsigned long)(mem_free() / 1024));
    std::fclose(at.log);
  }
  C2D_Fini();
  C3D_Fini();
  gfxExit();
  romfsExit();
  return 0;
}

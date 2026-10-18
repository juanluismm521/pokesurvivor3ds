// main.cpp — arranque y bucle principal en la 3DS.
//   Paso fijo de 60 Hz para la partida (si un fotograma tarda, se dan hasta 3
//   pasos para que no vaya a cámara lenta). Las pantallas están en app.cpp.
//   Modo AUTOTEST: si existe sdmc:/3ds/pokesurvivor-3ds/autotest.txt, el bot
//   juega solo, escribe FPS y memoria libre en autotest.log y el juego se
//   cierra al terminar. autotest.txt = "segundos dex inmortal" (180 25 1).
#include <3ds.h>
#include <citro2d.h>

#include <malloc.h>
#include <sys/stat.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

#include "core/game.h"
#include "core/save_io.h"
#include "core/settings.h"
#include "core/version.h"
#include "platform/3ds/app.h"
#include "platform/3ds/audio3ds.h"
#include "platform/3ds/render.h"

using namespace ps;

namespace {
constexpr const char* kDir = "sdmc:/3ds/pokesurvivor-3ds";

struct Autotest {
  bool on = false;
  float seconds = 180;
  int dex = 25;
  bool immortal = true;
  bool tour = false;
  float warp = 0;   // adelanta el reloj de la partida (para probar la Sentencia sin esperar)
  FILE* log = nullptr;
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
  char first[8] = "";
  if (std::fscanf(f, "%7s", first) == 1 && !std::strcmp(first, "ui")) a.tour = true;
  else { std::rewind(f); if (std::fscanf(f, "%f %d %d %f", &a.seconds, &a.dex, &imm, &a.warp) >= 1) a.immortal = imm != 0; }
  std::fclose(f);
  std::snprintf(path, sizeof(path), "%s/autotest.log", kDir);
  a.log = std::fopen(path, "w");
}

u32 mem_free() { return osGetMemRegionFree(MEMREGION_APPLICATION); }
// Heap en uso (malloc): para ver fugas (el emulador no da bien la memoria libre).
unsigned long heap_kb() { return (unsigned long)(mallinfo().uordblks / 1024); }
}  // namespace

int main() {
  const u64 boot_t0 = svcGetSystemTick();
  osSetSpeedupEnable(true);
  romfsInit();
  gfxInitDefault();
  C3D_Init(C3D_DEFAULT_CMDBUF_SIZE * 2);
  C2D_Init(8192);
  C2D_Prepare();
  mkdir("sdmc:/3ds", 0777);
  mkdir(kDir, 0777);
  hidSetRepeatParameters(18, 5);

  C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
  C3D_RenderTarget* top_r = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
  C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

  audio::init();   // (si no hay dspfirm.cdc, sin sonido)
  static Renderer r;
  if (!r.init()) {
    consoleInit(GFX_BOTTOM, nullptr);
    std::printf("No se pudieron cargar los recursos (romfs).\nSTART: salir\n");
    while (aptMainLoop()) { hidScanInput(); if (hidKeysDown() & KEY_START) break; gspWaitForVBlank(); }
    gfxExit();
    return 1;
  }

  // Partida guardada y ajustes (en la SD).
  std::unique_ptr<Save> save(new Save());
  const int loaded = save_read_file(kDir, *save);
  if (!loaded) new_save(*save, "Jugador", int64_t(osGetTime()) - 2208988800000LL);
  static Settings settings;
  settings_read(kDir, settings);

  std::unique_ptr<Game> g(new Game());
  static App app;
  app.init(&r, g.get(), save.get(), &settings, kDir);

  const unsigned long boot_ms = (unsigned long)((svcGetSystemTick() - boot_t0) * 1000ull / SYSCLOCK_ARM11);
  Autotest at;
  read_autotest(at);
  if (at.log) {
    bool n3ds = false;
    APT_CheckNew3DS(&n3ds);
    std::fprintf(at.log, "%s %s · autotest %.0f s (desde %.0f s) · dex %d · %s · %s\n", kGameTitle, kVersion, at.seconds, at.warp, at.dex,
                 at.immortal ? "inmortal" : "normal", n3ds ? "New 3DS" : "3DS");
    std::fprintf(at.log, "memoria libre al arrancar: %lu KB · lineal %lu KB · partida guardada: %s · arranque %lu ms\n", (unsigned long)(mem_free() / 1024),
                 (unsigned long)(linearSpaceFree() / 1024), loaded == 1 ? "sí" : loaded == 2 ? "copia" : "nueva", boot_ms);
    std::fflush(at.log);
  }
  if (at.tour) { app.start_tour(); at.on = false; }
  if (at.on) {
    app.set_autotest(at.dex, at.immortal, at.warp);
    at.next_log = at.warp + 10;
    if (at.log) {
      std::fprintf(at.log, "precargados %d Pokémon · %lu KB de sprites · memoria libre %lu KB · partida lista en %lu ms\n", r.sprites.loaded(),
                   (unsigned long)(r.sprites.bytes() / 1024), (unsigned long)(mem_free() / 1024), (unsigned long)app.start_ms);
      std::fflush(at.log);
    }
  }

  u64 last = osGetTime();
  double acc = 0;
  u64 fps_t = last;
  int fps_frames = 0;

  while (aptMainLoop() && !app.quit) {
    hidScanInput();
    UiIn in;
    in.down = hidKeysDown();
    in.held = hidKeysHeld();
    in.rep = hidKeysDownRepeat();
    if ((in.down & KEY_START) && (in.held & KEY_SELECT)) break;   // START+SELECT: salir
    if (at.on && (in.down & KEY_START)) break;
    touchPosition tp;
    hidTouchRead(&tp);
    in.tap = in.down & KEY_TOUCH;
    in.tx = tp.px; in.ty = tp.py;
    circlePosition cp;
    hidCircleRead(&cp);
    in.cx = cp.dx / 150.0f; in.cy = -cp.dy / 150.0f;

    const u64 now = osGetTime();
    const double frame_ms = double(now - last);
    acc += frame_ms;
    last = now;
    if (acc > 100) acc = 100;
    app.update(in, float(std::fmin(frame_ms, 100.0) / 1000.0));

    // --- paso fijo de la partida ---
    if (app.scene == SC_RUN) {
      Input gi = app.pending_input;
      int steps = 0;
      while (acc >= 1000.0 / 60 && steps < 3) {
        g->step(&gi);
        for (const SfxEvent& e : g->sfx_queue) audio::play_game(e);
        gi.select = -1; gi.cycle = 0;
        acc -= 1000.0 / 60;
        steps++;
      }
    } else {
      acc = 0;
    }

    audio::music(app.scene == SC_RUN ? g->music : int(MU_VILLAGE));
    audio::set_volume(settings.music, settings.sfx);
    audio::update(float(std::fmin(frame_ms, 100.0) / 1000.0));

    // --- dibujo ---
    r.begin_frame(*g);
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    // 3D: con el deslizador subido (y el ajuste activado), una imagen por ojo.
    const float slider = settings.stereo ? osGet3DSliderState() : 0;
    gfxSet3D(slider > 0.01f);
    C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
    C2D_SceneBegin(top);
    app.draw_top(slider > 0.01f ? -slider : 0);
    if (slider > 0.01f) {
      C2D_TargetClear(top_r, C2D_Color32(0, 0, 0, 255));
      C2D_SceneBegin(top_r);
      app.draw_top(slider);
    }
    C2D_TargetClear(bottom, C2D_Color32(16, 22, 34, 255));
    C2D_SceneBegin(bottom);
    app.draw_bottom();
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
      std::fprintf(at.log, "t=%4.0f s · %2d FPS (media %.1f, mín %d) · enemigos %d · proyectiles %d · partículas %d · Pokémon cargados %d (%lu KB) · libre %lu KB · lineal %lu KB · heap %lu KB\n",
                   g->time, r.fps, at.fps_n ? at.fps_sum / at.fps_n : 0.0, at.min_fps == 999 ? 0 : at.min_fps, g->enemies.n, g->projectiles.n,
                   g->fx.parts.n, r.sprites.loaded(), (unsigned long)(r.sprites.bytes() / 1024), (unsigned long)(mem_free() / 1024),
                   (unsigned long)(linearSpaceFree() / 1024), heap_kb());
      std::fflush(at.log);
    }
    if (at.on && g->time >= at.seconds) break;
  }

  if (at.log) {
    std::fprintf(at.log, "cargas de sprites durante la partida: %lu · hilo principal media %lu us, máx %lu us · lectura en el hilo máx %lu ms\n",
                 (unsigned long)r.sprites.late_loads,
                 (unsigned long)(r.sprites.late_loads ? r.sprites.late_total_us / r.sprites.late_loads : 0),
                 (unsigned long)r.sprites.late_max_us, (unsigned long)(r.sprites.read_max_us / 1000));
    std::fprintf(at.log, "FIN · %.0f s de partida · FPS media %.1f · mín %d · nivel %d · derrotados %d · libre %lu KB\n", g->time,
                 at.fps_n ? at.fps_sum / at.fps_n : 0.0, at.min_fps == 999 ? 0 : at.min_fps, g->pl().level, g->pl().kills,
                 (unsigned long)(mem_free() / 1024));
    std::fclose(at.log);
  }
  if (!at.on && g->dirty) { save->updated = int64_t(osGetTime()) - 2208988800000LL; save_write_file(kDir, *save); }
  audio::exit();
  C2D_Fini();
  C3D_Fini();
  gfxExit();
  romfsExit();
  return 0;
}

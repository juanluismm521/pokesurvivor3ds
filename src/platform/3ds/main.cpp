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
#include "platform/3ds/cloud3ds.h"
#include "platform/3ds/datos3ds.h"
#include "platform/3ds/net3ds.h"
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
  float warp = 0;       // adelanta el reloj de la partida (para probar la Sentencia sin esperar)
  char move[24] = "";   // empezar con este movimiento (id de la web), para ver las familias nuevas
  bool coop = false, host = false, online = false;
  bool look = false;                // "coop mirar …": sólo busca salas (para ver la pantalla)
  bool die = false;                 // "morir": se deja caer y, cuando le levantan, se cierra
  int down_state = 0;               // 0 vivo, 1 caído, 2 levantado
  double revived_at = 0;
  bool nube = false, https = false, datos = false;   // "datos": busca datos nuevos de la web y lo apunta            // "nube usuario contraseña": entra y sincroniza (pruebas)
  char user[32] = "", pass[64] = "";
  long long hash_logged = -1;   // "coop host|join segundos dex inmortal": cooperativo con otro emulador
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
  char role[8] = "";
  if (std::fscanf(f, "%7s", first) == 1 && !std::strcmp(first, "ui")) a.tour = true;
  else if (!std::strcmp(first, "https")) a.https = true;
  else if (!std::strcmp(first, "datos")) a.datos = true;
  else if (!std::strcmp(first, "nube") && std::fscanf(f, "%31s %63s", a.user, a.pass) == 2) a.nube = true;
  else if (!std::strcmp(first, "coop") && std::fscanf(f, "%7s %f %d %d", role, &a.seconds, &a.dex, &imm) >= 1) {
    a.coop = true; a.host = !std::strcmp(role, "host"); a.look = !std::strcmp(role, "mirar"); a.immortal = imm != 0;
    char extra[16] = "";
    while (std::fscanf(f, "%15s", extra) == 1) {
      if (!std::strcmp(extra, "online")) a.online = true;
      if (!std::strcmp(extra, "morir")) a.die = true;
    }
  }
  else { std::rewind(f); if (std::fscanf(f, "%f %d %d %f %23s", &a.seconds, &a.dex, &imm, &a.warp, a.move) >= 1) a.immortal = imm != 0; }
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
#ifdef PS_DEPURAR
  {   // rejilla.txt en la SD: depuración de colisiones (ver Renderer::debug_grid).
      // Sólo en las builds de pruebas (make DEPURAR=1), nunca en la del dueño.
    char rp[96];
    std::snprintf(rp, sizeof(rp), "%s/rejilla.txt", kDir);
    if (FILE* f = std::fopen(rp, "rb")) { std::fclose(f); r.debug_grid = true; r.chunks.debug_marks = true; }
  }
#endif

  // Partida guardada y ajustes (en la SD).
  std::unique_ptr<Save> save(new Save());
  const int loaded = save_read_file(kDir, *save);
  if (!loaded) new_save(*save, "Jugador", int64_t(osGetTime()) - 2208988800000LL);
  static Settings settings;
  settings_read(kDir, settings);

  cloud3ds::init(kDir);
  // Datos del juego: los de la SD (el último paquete bajado de la web) o los de fábrica.
  const std::string datos_boot = datos3ds::init(kDir);
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
  if (at.log) { std::fprintf(at.log, "datos al arrancar: %s\n", datos_boot.c_str()); std::fflush(at.log); }
  if (at.tour) { app.start_tour(); at.on = false; }
  if (at.datos) {
    // Busca datos nuevos, los aplica si los hay, lo apunta y se cierra.
    const u64 t0 = osGetTime();
    datos3ds::start_check();
    while (aptMainLoop() && datos3ds::busy() && osGetTime() - t0 < 60000) gspWaitForVBlank();
    std::string msg;
    const bool applied = datos3ds::apply_pending(msg);
    if (at.log) {
      std::fprintf(at.log, "datos: %s · %s · programa nuevo en la web: %s · %.1f s\n", datos3ds::status().c_str(), applied ? msg.c_str() : "nada aplicado",
                   datos3ds::newer_program() ? "sí" : "no", (osGetTime() - t0) / 1000.0);
      std::fprintf(at.log, "Impactrueno: daño %.1f · Placaje: %s\n", double(kMoves[move_index("thunder-shock")].v[MS_DMG]), kMoves[move_index("tackle")].name);
      std::fclose(at.log); at.log = nullptr;
    }
    datos3ds::shutdown();
    cloud3ds::shutdown();
    audio::exit(); C2D_Fini(); C3D_Fini(); gfxExit(); romfsExit();
    return 0;
  }
  if (at.https) {
    std::string err;
    const u64 t0 = osGetTime();
    const int st = cloud3ds::probe_https(err);
    if (at.log) { std::fprintf(at.log, "https: %d %s · %.1f s\n", st, err.c_str(), (osGetTime() - t0) / 1000.0); std::fclose(at.log); at.log = nullptr; }
    cloud3ds::shutdown();
    audio::exit(); C2D_Fini(); C3D_Fini(); gfxExit(); romfsExit();
    return 0;
  }
  if (at.nube) {
    // Entra en la cuenta, sincroniza, lo apunta en el log y se cierra.
    at.on = false;
    const u64 t0 = osGetTime();
    app.cloud_test(at.user, at.pass);
    bool code_logged = false;
    app.show_account();   // (para ver en las capturas la pantalla de la cuenta y el código)
    while (aptMainLoop() && app.cloud_done == 0 && osGetTime() - t0 < 90000) {
      app.cloud_tick();
      C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
      C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255)); C2D_SceneBegin(top); app.draw_top(0);
      C2D_TargetClear(bottom, C2D_Color32(16, 22, 34, 255)); C2D_SceneBegin(bottom); app.draw_bottom();
      C3D_FrameEnd(0);
      std::string gc, gu;
      if (!code_logged && cloud3ds::google_code(gc, gu) && at.log) { code_logged = true; std::fprintf(at.log, "google: entra en %s y escribe %s\n", gu.c_str(), gc.c_str()); std::fflush(at.log); }
      gspWaitForVBlank();
    }
    if (at.log) {
      int owned = 0;
      for (int d = 1; d <= kMaxDex; ++d) if (save->owned_at[d]) owned++;
      std::fprintf(at.log, "nube: %s · %s · monedas %d · tickets %d/%d · Pokémon %d · shinies %d · %.0f s\n", app.cloud_done > 0 ? "BIEN" : "MAL",
                   app.cloud_done > 0 ? cloud3ds::account().name.c_str() : cloud3ds::error().c_str(), int(save->coins), int(save->t1), int(save->t10),
                   owned, int(save->stats.shinies), (osGetTime() - t0) / 1000.0);
      std::fclose(at.log);
      at.log = nullptr;
    }
    cloud3ds::shutdown();
    audio::exit(); C2D_Fini(); C3D_Fini(); gfxExit(); romfsExit();
    return 0;
  } else if (!at.on && !at.tour) {
    app.cloud_autostart();   // (si hay cuenta, se sincroniza al encender)
    if (settings.datos_auto) datos3ds::start_check();   // (y, si está activado, los datos nuevos de la web)
  }
  if (at.on && at.coop) {
    app.set_coop_autotest(at.host, at.dex, at.immortal, at.online, at.die);
    app.auto_look = at.look;
    if (at.log) { std::fprintf(at.log, "cooperativo: %s%s\n", at.host ? "anfitrión" : "invitado", at.online ? " · por internet" : ""); std::fflush(at.log); }
  } else if (at.on) {
    app.set_autotest(at.dex, at.immortal, at.warp, at.move[0] ? move_index(at.move) : -1);
    at.next_log = at.warp + 10;
    if (at.log && at.move[0]) std::fprintf(at.log, "empieza con %s (%d): %s\n", at.move, move_index(at.move), kMoves[g->pl().moves[0].def].id);
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
      app.advance(acc, float(std::fmin(frame_ms, 100.0) / 1000.0));
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
      std::fprintf(at.log, "t=%4.0f s · %2d FPS (media %.1f, mín %d) · enemigos %d · proyectiles %d · partículas %d · objetos %d · Pokémon cargados %d (%lu KB) · libre %lu KB · lineal %lu KB · heap %lu KB\n",
                   g->time, r.fps, at.fps_n ? at.fps_sum / at.fps_n : 0.0, at.min_fps == 999 ? 0 : at.min_fps, g->enemies.n, g->projectiles.n,
                   g->fx.parts.n, r.props_drawn, r.sprites.loaded(), (unsigned long)(r.sprites.bytes() / 1024), (unsigned long)(mem_free() / 1024),
                   (unsigned long)(linearSpaceFree() / 1024), heap_kb());
      std::fflush(at.log);
    }
    // Cooperativo: la huella cada 10 s (para comparar los logs de las consolas).
    if (at.on && at.log && app.coop()) {
      const net::Lockstep& ls = app.lockstep();
      if (ls.last_hash_tick() > 0 && ls.last_hash_tick() % 600 == 0 && ls.last_hash_tick() != at.hash_logged) {
        at.hash_logged = ls.last_hash_tick();
        std::fprintf(at.log, "huella paso %lld = %08lx · jugadores %d · esperando %d fotogramas · desincronizado %s · retraso %d · ping %d ms\n", at.hash_logged,
                     (unsigned long)ls.last_hash(), g->team_size(), ls.stall_frames(), ls.desync() ? "SÍ" : "no", ls.delay(), net3ds::rtt_ms());
        std::fflush(at.log);
      }
    }
    // Modo "morir": apunta cuándo cae y cuándo le levantan; luego se cierra.
    if (at.on && at.die && app.coop() && app.scene == SC_RUN) {
      const Player& me = g->me();
      if (at.down_state == 0 && me.dead) { at.down_state = 1; if (at.log) { std::fprintf(at.log, "caído a los %.1f s\n", g->time); std::fflush(at.log); } }
      else if (at.down_state == 1 && !me.dead) { at.down_state = 2; at.revived_at = g->time; if (at.log) { std::fprintf(at.log, "¡levantado a los %.1f s! (vida %.0f / %.0f)\n", g->time, me.hp, me.max_hp); std::fflush(at.log); } }
      else if (at.down_state == 2 && g->time - at.revived_at > 3) break;
    }
    if (at.on && g->time >= at.seconds) break;
    if (at.on && at.coop && g->state == ST_OVER && app.scene == SC_RUN) break;
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
  datos3ds::shutdown();
  cloud3ds::shutdown();
  audio::exit();
  C2D_Fini();
  C3D_Fini();
  gfxExit();
  romfsExit();
  return 0;
}

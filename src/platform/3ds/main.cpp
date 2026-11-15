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
#include <unistd.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "core/weekly.h"
#include "core/game.h"
#include "core/evo.h"
#include "core/power.h"
#include "core/i18n.h"
#include "core/save_io.h"
#include "core/settings.h"
#include "core/version.h"
#include "platform/3ds/app.h"
#include "platform/3ds/audio3ds.h"
#include "platform/3ds/cloud3ds.h"
#include "platform/3ds/crash3ds.h"
#include "platform/3ds/datos3ds.h"
#include "gen/build.h"
#include "platform/3ds/prog3ds.h"
#include "platform/3ds/saver3ds.h"
#include "platform/3ds/net3ds.h"
#include "platform/3ds/hw3ds.h"
#include "platform/3ds/render.h"

using namespace ps;

namespace {
constexpr const char* kDir = "sdmc:/3ds/pokesurvivor-3ds";
}  // namespace
// Pila del hilo principal (libctru la reserva al arrancar; por defecto 32 KB,
// justa en el .cia, donde cada lectura del romfs pasa por más capas).
extern "C" { u32 __stacksize__ = 256 * 1024; }
namespace {
int g_hitch_n = 0, g_hitch_max = 0;   // tirones (fotogramas de más de 50 ms)
int g_ms_logic = 0, g_ms_ground = 0, g_ms_draw = 0;   // en qué se fue el fotograma anterior
char g_hitch_where[24] = "-";

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
  bool nube = false, https = false, datos = false;
  int programa = 0;
  bool coop_mega = false;   // "coop … mega": empiezan con su megapiedra
  int level = 0, stone = 0;
  bool mark = false, peace = false, still = false;   // "… paz": sin enemigos
  char room_pass[12] = "";
  char room_host[16] = "";   // grupo: "sala=NOMBRE": el invitado entra en la sala de ese anfitrión (y no en la primera)   // grupo: "pass=X": contraseña de la sala (anfitrión) o la que prueba el invitado
  bool boot = false;          // "… arranque": lo que hace al encender (buscar versión nueva si está activado)
  int prog_state = -1;
  bool quick = false;         // "… rapido": modo rápido (la Sentencia a los 20 min)
  int diff = 0;               // "… dificil" / "pesadilla"
  bool orbs = false;          // "… esferas": te da esferas al empezar (el bot las usa)
  bool real = false;          // "… real": con la partida guardada de la SD (el premio se guarda de verdad)
  bool to_menu = false;       // "… menu": tras el resumen, B (al menú) y 6 s en el menú
  bool end_screen = false;    // "… fin": al acabar el tiempo, termina la partida y enseña el resumen 8 s (captura)
  u64 end_at = 0;
  bool tab_moved = false;
  bool bond = false;          // "… vinculo": te da un vínculo y marca el primer movimiento (captura)
  int rocks = 0;              // "… rocas=N": golpea una roca en N chunks del mapa (prueba de la caché del mundo)
  float mega_wait = 0;        // "… megaespera=S": espera S s con la piedra antes de pulsar Y          // "… marca": la posición de tu Pokémon en un cuadradito (para el tráiler)   // "… nivel=N piedra=b": para grabar el tráiler
  float mega_at = 5;          // "… megaen=S": la piedra a los S s de partida
  bool menu = false;          // "menu": el menú principal un minuto (con lo que haga al encender)
  bool upgrades = false;      // "mejoras": la mejora «Más pasivas…» y las listas de movimientos (capturas)
  bool treasure = false;      // "… tesoro": el tesoro de la Luminosfera a los 3 s
  int mode_demo = 0;          // "modo" / "medalla": capturas de la elección de modo y de la medalla
  bool win = false;
  int hw = -1;                // "… halloween" / "… nohalloween": forzar el evento (o quitarlo) en las pruebas
  bool gift_demo = false;     // "regalo": la pantalla de los regalos, cerrado y abierto (capturas)
  bool shop_demo = false;     // "tienda": la Tienda Kecleon, compra una MT y una Menta y enseña la Mochila (capturas)
  bool ficha_demo = false;    // "ficha": la ficha de tu compañero: naturaleza, habilidad y MT (capturas)
  char abil[40] = "", nature[16] = "";   // "… habilidad=static naturaleza=bold": las de la partida
  int weekly = -2;            // "… semanal" (el de esta semana) o "semanal=noche" (con esa condición: id de la web o «noche»)
  bool aura = false;          // "… aura": con el aura de maestro (maestría completa)
  bool skin = false;          // "… disfraz": el compañero con su disfraz de Halloween           // "… ganar": modo rápido, acelera la Sentencia hasta ganar a Arceus (capturas de la victoria)
  bool rooms = false;         // "salas": crear sala y lista con candado (capturas)
  bool gacha100 = false;      // "gacha100": tirada ×100 y su resumen (capturas)
  bool gacha_all = false;     // "gachatodas": lo mismo en la región «Todas»
  bool news = false, update_demo = false;   // "actualizar": Actualizaciones buscando y bajando (para grabarlo)        // "novedades [dex]": la pantalla de novedades y el menú con ese compañero (capturas)
  bool textos = false;      // "textos": mide los textos de movimientos, objetos y megas (los que no caben, al log)
  int fallo = 0;            // "fallo [hilo]": se rompe a propósito (prueba de fallo.log), en el principal o en un hilo
  bool galeria = false;     // "galeria": las megas, una página tras otra (y si cargan)   // "programa": busca, descarga y sale (se instala al salir) · "programa volver": vuelve a la anterior   // "datos": busca datos nuevos de la web y lo apunta            // "nube usuario contraseña": entra y sincroniza (pruebas)
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
  char first[16] = "";
  char role[8] = "";
  if (std::fscanf(f, "%15s", first) == 1 && !std::strcmp(first, "ui")) a.tour = true;
  else if (!std::strcmp(first, "https")) a.https = true;
  else if (!std::strcmp(first, "galeria")) a.galeria = true;
  else if (!std::strcmp(first, "textos")) a.textos = true;
  else if (!std::strcmp(first, "actualizar")) a.update_demo = true;
  else if (!std::strcmp(first, "gacha100")) a.gacha100 = true;
  else if (!std::strcmp(first, "gachatodas")) a.gacha100 = a.gacha_all = true;
  else if (!std::strcmp(first, "salas")) a.rooms = true;
  else if (!std::strcmp(first, "menu")) a.menu = true;
  else if (!std::strcmp(first, "mejoras")) a.upgrades = true;
  else if (!std::strcmp(first, "modo")) a.mode_demo = 1;
  else if (!std::strcmp(first, "medalla")) a.mode_demo = 2;
  else if (!std::strcmp(first, "tienda")) a.shop_demo = true;
  else if (!std::strcmp(first, "ficha")) a.ficha_demo = true;
  else if (!std::strcmp(first, "regalo")) a.gift_demo = true;
  else if (!std::strcmp(first, "novedades")) { a.news = true; a.dex = 6; if (std::fscanf(f, "%d", &a.dex) != 1) a.dex = 6; }
  else if (!std::strcmp(first, "fallo")) { char w[16] = ""; a.fallo = (std::fscanf(f, "%15s", w) == 1 && !std::strcmp(w, "hilo")) ? 2 : 1; }
  else if (!std::strcmp(first, "programa")) { char w[16] = ""; a.programa = (std::fscanf(f, "%15s", w) == 1 && !std::strcmp(w, "volver")) ? 2 : 1; }
  else if (!std::strcmp(first, "nube") && std::fscanf(f, "%31s %63s", a.user, a.pass) == 2) a.nube = true;
  else if (!std::strcmp(first, "coop") && std::fscanf(f, "%7s %f %d %d", role, &a.seconds, &a.dex, &imm) >= 1) {
    a.coop = true; a.host = !std::strcmp(role, "host"); a.look = !std::strcmp(role, "mirar"); a.immortal = imm != 0;
    char extra[40] = "";
    while (std::fscanf(f, "%39s", extra) == 1) {
      if (!std::strcmp(extra, "online")) a.online = true;
      if (!std::strcmp(extra, "morir")) a.die = true;
      if (!std::strcmp(extra, "mega")) a.coop_mega = true;
      if (!std::strncmp(extra, "desde=", 6)) a.warp = float(std::atof(extra + 6));
      if (!std::strncmp(extra, "nivel=", 6)) a.level = std::atoi(extra + 6);
      if (!std::strcmp(extra, "fin")) a.end_screen = true;
      if (!std::strcmp(extra, "real")) a.real = true;
      if (!std::strcmp(extra, "menu")) { a.to_menu = true; a.end_screen = true; }
      if (!std::strcmp(extra, "rapido")) a.quick = true;
      if (!std::strcmp(extra, "ganar")) a.win = true;
      if (!std::strcmp(extra, "halloween")) a.hw = 1;
      if (!std::strcmp(extra, "nohalloween")) a.hw = 0;
      if (!std::strcmp(extra, "disfraz")) a.skin = true;
      if (!std::strncmp(extra, "pass=", 5)) std::snprintf(a.room_pass, sizeof(a.room_pass), "%.11s", extra + 5);
      if (!std::strncmp(extra, "sala=", 5)) std::snprintf(a.room_host, sizeof(a.room_host), "%.15s", extra + 5);
    }
  }
  else {
    std::rewind(f);
    if (std::fscanf(f, "%f %d %d %f %23s", &a.seconds, &a.dex, &imm, &a.warp, a.move) >= 1) a.immortal = imm != 0;
    // (para grabar: nivel=N megaen=S piedra=b…; también pueden ir en el hueco del movimiento)
    auto flag = [&](const char* w) {
      if (!std::strncmp(w, "nivel=", 6)) a.level = std::atoi(w + 6);
      else if (!std::strncmp(w, "megaen=", 7)) a.mega_at = float(std::atof(w + 7));
      else if (!std::strcmp(w, "piedra=b")) a.stone = 1;
      else if (!std::strcmp(w, "marca")) a.mark = true;
      else if (!std::strcmp(w, "paz")) a.peace = true;
      else if (!std::strcmp(w, "quieto")) a.still = true;
      else if (!std::strncmp(w, "megaespera=", 11)) a.mega_wait = float(std::atof(w + 11));
      else if (!std::strncmp(w, "rocas=", 6)) a.rocks = std::atoi(w + 6);
      else if (!std::strcmp(w, "vinculo")) a.bond = true;
      else if (!std::strcmp(w, "fin")) a.end_screen = true;
      else if (!std::strcmp(w, "real")) a.real = true;
      else if (!std::strcmp(w, "menu")) { a.to_menu = true; a.end_screen = true; }
      else if (!std::strcmp(w, "esferas")) a.orbs = true;
      else if (!std::strcmp(w, "tesoro")) a.treasure = true;
      else if (!std::strcmp(w, "rapido")) a.quick = true;
      else if (!std::strcmp(w, "dificil")) a.diff = 1;
      else if (!std::strcmp(w, "pesadilla")) a.diff = 2;
      else if (!std::strcmp(w, "ganar")) a.win = true;
      else if (!std::strcmp(w, "halloween")) a.hw = 1;
      else if (!std::strcmp(w, "nohalloween")) a.hw = 0;
      else if (!std::strcmp(w, "disfraz")) a.skin = true;
      else if (!std::strcmp(w, "aura")) a.aura = true;
      else if (!std::strcmp(w, "semanal")) a.weekly = -1;
      else if (!std::strncmp(w, "semanal=", 8)) {
        a.weekly = -1;
        const char* id = !std::strcmp(w + 8, "noche") ? "night" : w + 8;
        for (int c = 0; c < WC_COUNT; ++c) if (!std::strcmp(kWeeklyConds[c].id, id)) a.weekly = c;
      }
      else if (!std::strcmp(w, "arranque")) a.boot = true;
      else return false;
      return true;
    };
    char w[24];
    while (std::fscanf(f, "%23s", w) == 1) flag(w);
    if (flag(a.move)) a.move[0] = 0;
  }
  // (Halloween y disfraz: en cualquier autotest, también los de pantallas: «menu halloween»)
  std::rewind(f);
  for (char w[48]; std::fscanf(f, "%47s", w) == 1;) {
    if (!std::strncmp(w, "habilidad=", 10)) std::snprintf(a.abil, sizeof(a.abil), "%s", w + 10);
    else if (!std::strncmp(w, "naturaleza=", 11)) std::snprintf(a.nature, sizeof(a.nature), "%.15s", w + 11);
    else if (!std::strcmp(w, "halloween")) a.hw = 1;
    else if (!std::strcmp(w, "nohalloween")) a.hw = 0;
    else if (!std::strcmp(w, "disfraz")) a.skin = true;
    else if (!std::strcmp(w, "aura")) a.aura = true;
  }
  std::fclose(f);
  std::snprintf(path, sizeof(path), "%s/autotest.log", kDir);
  a.log = std::fopen(path, "w");
}

u32 mem_free() { return osGetMemRegionFree(MEMREGION_APPLICATION); }
// Heap en uso (malloc): para ver fugas (el emulador no da bien la memoria libre).
unsigned long heap_kb() { return (unsigned long)(mallinfo().uordblks / 1024); }
}  // namespace

// Pantalla de «Instalando…» al salir (la pinta prog3ds::apply).
Renderer* g_show_r = nullptr;
C3D_RenderTarget *g_show_top = nullptr, *g_show_bottom = nullptr;
void show_install(const char* msg, int pct) {
  if (!g_show_r) return;
  C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
  C2D_TargetClear(g_show_top, C2D_Color32(16, 22, 34, 255));
  C2D_SceneBegin(g_show_top);
  C2D_TextBufClear(g_show_r->tbuf);
  g_show_r->text("PokéSurvivor 3DS", 200, 90, 0.7f, C2D_Color32(255, 203, 61, 255), 1);
  g_show_r->text(msg, 200, 124, 0.45f, C2D_Color32(234, 242, 255, 255), 1);
  C2D_DrawRectSolid(80, 150, 0, 240, 10, C2D_Color32(0, 0, 0, 200));
  C2D_DrawRectSolid(80, 150, 0, 240.0f * pct / 100, 10, C2D_Color32(255, 203, 61, 255));
  g_show_r->text("No apagues la consola", 200, 172, 0.36f, C2D_Color32(160, 175, 200, 255), 1);
  C2D_TargetClear(g_show_bottom, C2D_Color32(16, 22, 34, 255));
  C2D_SceneBegin(g_show_bottom);
  C3D_FrameEnd(0);
}

int main(int argc, char** argv) {
  const u64 boot_t0 = svcGetSystemTick();
  mkdir("sdmc:/3ds", 0777);
  mkdir(kDir, 0777);
  crash3ds::init(kDir);   // (si se rompe: fallo.log y arranque.log en la SD)
  osSetSpeedupEnable(true);
  crash3ds::step("romfs");
  romfsInit();
  crash3ds::step("gráficos");
  gfxInitDefault();
  C3D_Init(C3D_DEFAULT_CMDBUF_SIZE * 2);
  C2D_Init(8192);
  C2D_Prepare();
  hidSetRepeatParameters(18, 5);

  C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
  C3D_RenderTarget* top_r = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
  C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

  crash3ds::step("sonido");
  audio::init();   // (si no hay dspfirm.cdc, sin sonido)
  crash3ds::step("recursos (romfs)");
  static Renderer r;
  if (!r.init()) {
    consoleInit(GFX_BOTTOM, nullptr);
    std::printf("No se pudieron cargar los recursos (romfs).\nSTART: salir\n");
    while (aptMainLoop()) { hidScanInput(); if (hidKeysDown() & KEY_START) break; gspWaitForVBlank(); }
    gfxExit();
    return 1;
  }
  // Sprites en segundo plano también en los menús (Pokédex, gacha, iniciales):
  // abrirlos cargaba de golpe muchos Pokémon y el juego se quedaba pillado.
  r.sprites.async = true;
#ifdef PS_DEPURAR
  {   // rejilla.txt en la SD: depuración de colisiones (ver Renderer::debug_grid).
      // Sólo en las builds de pruebas (make DEPURAR=1), nunca en la del dueño.
    char rp[96];
    std::snprintf(rp, sizeof(rp), "%s/rejilla.txt", kDir);
    if (FILE* f = std::fopen(rp, "rb")) { std::fclose(f); r.debug_grid = true; r.chunks.debug_marks = true; }
  }
#endif

  // Partida guardada y ajustes (en la SD).
  crash3ds::step("partida guardada");
  std::unique_ptr<Save> save(new Save());
  const int loaded = save_read_file(kDir, *save);
  if (!loaded) new_save(*save, "Jugador", int64_t(osGetTime()) - 2208988800000LL);
  static Settings settings;
  settings_read(kDir, settings);
  // Sin idioma elegido (la primera vez): el de la consola; español si lo es, si no inglés.
  if (!settings.lang_set) {
    u8 cl = CFG_LANGUAGE_ES;
    if (R_SUCCEEDED(cfguInit())) { CFGU_GetSystemLanguage(&cl); cfguExit(); }
    settings.lang = cl == CFG_LANGUAGE_ES ? 0 : 1;
  }
  // «idioma.txt» en la SD (pruebas): en inglés, y lo que no se traduce entero
  // va a idioma.log al salir (ver tr_record).
  char idioma_path[96];
  std::snprintf(idioma_path, sizeof(idioma_path), "%s/idioma.txt", kDir);
  const bool idioma_test = access(idioma_path, F_OK) == 0;
  if (idioma_test) { settings.lang = 1; tr_record(true); }

  crash3ds::step("nube");
  cloud3ds::init(kDir);
  crash3ds::step("guardado y datos");
  // Datos del juego: los de la SD (el último paquete bajado de la web) o los de fábrica.
  saver3ds::init(kDir);
  const std::string datos_boot = datos3ds::init(kDir);
  {   // Actualizaciones del programa: de qué .3dsx se ha cargado (en Azahar no lo
      // dice: programa_ruta.txt en la SD, sólo para pruebas).
    char ruta[128] = "";
    if (argc > 0 && argv && argv[0]) std::snprintf(ruta, sizeof(ruta), "%s", argv[0]);
    char rp[96];
    std::snprintf(rp, sizeof(rp), "%s/programa_ruta.txt", kDir);
    if (FILE* f = std::fopen(rp, "rb")) { if (std::fscanf(f, "%127s", ruta) != 1) ruta[0] = 0; std::fclose(f); }
    crash3ds::step(ruta[0] ? ruta : "actualizaciones (sin ruta)");
    prog3ds::init(kDir, ruta);
  }
  crash3ds::step("pantallas");
  std::unique_ptr<Game> g(new Game());
  static App app;
  app.init(&r, g.get(), save.get(), &settings, kDir);

  const unsigned long boot_ms = (unsigned long)((svcGetSystemTick() - boot_t0) * 1000ull / SYSCLOCK_ARM11);
  Autotest at;
  read_autotest(at);
  hw3ds::force(at.hw);   // (pruebas: Halloween sí o no; el menú lo nota solo, App::update)
  if (at.log) {
    bool n3ds = false;
    APT_CheckNew3DS(&n3ds);
    std::fprintf(at.log, "%s %s · autotest %.0f s (desde %.0f s) · dex %d · %s · %s\n", kGameTitle, kVersion, at.seconds, at.warp, at.dex,
                 at.immortal ? "inmortal" : "normal", n3ds ? "New 3DS" : "3DS");
    std::fprintf(at.log, "memoria libre al arrancar: %lu KB · lineal %lu KB · partida guardada: %s · arranque %lu ms\n", (unsigned long)(mem_free() / 1024),
                 (unsigned long)(linearSpaceFree() / 1024), loaded == 1 ? "sí" : loaded == 2 ? "copia" : "nueva", boot_ms);
    std::fflush(at.log);
  }
  if (at.log) { std::fprintf(at.log, "datos al arrancar: %s · Pikachu: vida %d\n", datos_boot.c_str(), int(mon_by_dex(25)->hp)); std::fflush(at.log); }
  if (at.tour) { app.start_tour(); at.on = false; }
  if (at.news) { app.start_news_demo(at.dex); at.on = false; }
  if (at.gacha100) { app.start_gacha_demo(at.gacha_all); at.on = false; }
  if (at.rooms) { app.start_rooms_demo(); at.on = false; }
  if (at.menu) { app.start_menu_demo(); at.on = false; }
  if (at.upgrades) { app.start_upgrades_demo(); at.on = false; }
  if (at.mode_demo) { app.start_mode_demo(at.mode_demo == 2); at.on = false; }
  // (naturaleza y habilidad de la partida del autotest: «habilidad=static naturaleza=bold»)
  for (int i = 0; i < kAbilCount; ++i) if (!std::strcmp(kAbils[i].id, at.abil)) app.auto_abil = i;
  for (int i = 0; i < kNatureCount; ++i) if (!std::strcmp(kNatures[i].id, at.nature)) app.auto_nature = i;
  if (at.shop_demo) { app.start_shop_demo(); at.on = false; }
  if (at.ficha_demo) { app.start_ficha_demo(); at.on = false; }
  if (at.gift_demo) { app.start_gift_demo(); at.on = false; }
  if (at.update_demo) { app.start_update_demo(); at.on = false; }
  if (at.fallo) {
    if (at.log) { std::fprintf(at.log, "fallo a propósito (%s)\n", at.fallo == 2 ? "hilo" : "principal"); std::fclose(at.log); at.log = nullptr; }
    auto boom = [](void*) { crash3ds::guard("prueba"); __builtin_trap(); };
    if (at.fallo == 2) { threadCreate(boom, nullptr, 16 * 1024, 0x30, -2, true); for (int i = 0; i < 300; ++i) gspWaitForVBlank(); }
    else boom(nullptr);
  }
  if (at.textos) {
    r.check_texts(at.log);
    // Y para las capturas: cartas de nivel con la descripción más larga y la
    // elección de objeto con una megapiedra.
    RunConfig cfg;
    cfg.dex = 6; cfg.seed = 1;
    g->start_run(cfg);
    Player& p = g->players[0];
    int longest = 0;
    for (int i = 1; i < kMoveCount; ++i) if (r.text_width(kMoves[i].desc, 0.32f) > r.text_width(kMoves[longest].desc, 0.32f)) longest = i;
    C2D_TextBufClear(r.tbuf);
    p.ncards = 3;
    p.cards[0] = Card(); p.cards[0].type = CT_NEW; p.cards[0].move = int16_t(longest);
    p.cards[1] = Card(); p.cards[1].type = CT_UP; p.cards[1].move = 0;
    p.cards[2] = Card(); p.cards[2].type = CT_STAT; p.cards[2].stat = LS_DEF;
    for (int k = 0; k < 6 && aptMainLoop(); ++k) {
      // (3 y 4: la elección de evolución, con Charmander y con Eevee: core/evo.h; 5: el poder de
      // Eternatus con Charizard, que tiene mega: core/power.h)
      if (k == 5) {
        RunConfig c3; c3.dex = 6; c3.seed = 2; c3.judged = 1;
        g->start_run(c3);
        power_offer(*g, g->players[0]);
        r.card_cursor = 1;
      } else if (k >= 3) {
        RunConfig c2; c2.dex = k == 3 ? 4 : 133; c2.seed = 2; c2.judged = 1;
        g->start_run(c2);
        Player& q = g->players[0];
        q.level = k == 3 ? 16 : 20;
        evo_update(*g, q);
        r.preload_run(*g);
        r.card_cursor = 0;
      } else if (k == 0) { g->state = ST_LEVELUP; p.picked = false; }
      else if (k == 1) { p.level = 30; p.four_up = true; levelup_offer(*g, p); g->state = ST_LEVELUP; p.picked = false; }   // (las 4 cartas, con 2 pasivas)
      else { g->state = ST_CHOICE; p.choice = CH_ITEM; p.item_cur = IT_MEGA_A; p.item_next = IT_SHELLBELL; }
      for (int f = 0; f < 240 && aptMainLoop(); ++f) {
        r.sprites.poll();   // (los sprites de la elección de evolución)
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(top, C2D_Color32(16, 22, 34, 255));
        C2D_TargetClear(bottom, C2D_Color32(16, 22, 34, 255));
        C2D_SceneBegin(bottom);
        r.draw_bottom(*g, false);
        C3D_FrameEnd(0);
      }
    }
    if (at.log) { std::fclose(at.log); at.log = nullptr; }
    app.quit = true;
    at.on = false;
  } else if (at.galeria) {
    // Todas las megas, 8 por página (3 s cada una), en normal y en shiny,
    // andando y girando por las 8 direcciones: para verlas en las capturas y
    // comprobar que cada sprite carga (los fallos, al log).
    int bad = 0, checked = 0;
    constexpr int kPer = 8;
    const int pages = (kMegaCount + kPer - 1) / kPer;
    for (int sh = 0; sh < 2 && aptMainLoop(); ++sh)
      for (int pg = 0; pg < pages && aptMainLoop(); ++pg) {
        Animator an[kPer];
        int n = 0;
        for (int k = 0; k < kPer && pg * kPer + k < kMegaCount; ++k, ++n) {
          const MegaDef& d = kMegas[pg * kPer + k];
          const bool own = d.own_sprite && (!sh || has_shiny(d.sprite));
          an[k].init(own ? d.sprite : d.base, own ? 1.0f : 1.15f, sh != 0);
          an[k].loop(A_WALK);
        }
        for (int f = 0; f < 180 && aptMainLoop(); ++f) {
          r.sprites.poll();
          for (int k = 0; k < n; ++k) { an[k].update(1.0f / 60); an[k].dir = int8_t((f / 22) % 8); }
          C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
          C2D_TargetClear(top, C2D_Color32(30, 44, 36, 255));
          C2D_SceneBegin(top);
          C2D_TextBufClear(r.tbuf);
          char b[64];
          std::snprintf(b, sizeof(b), "Megas %d/%d · %s", pg + 1, pages, sh ? "shiny" : "normal");
          r.text(b, 200, 2, 0.4f, C2D_Color32(255, 203, 61, 255), 1);
          for (int k = 0; k < n; ++k) {
            const MegaDef& d = kMegas[pg * kPer + k];
            const float x = 50 + (k % 4) * 100, y = 98 + (k / 4) * 112;
            C2D_DrawEllipseSolid(x - 16, y - 5, 0, 32, 10, C2D_Color32(0, 0, 0, 70));
            r.draw_sprite(an[k], x, y, 0, 1);
            r.text_fit(d.mon.name, x - 48, y + 4, 96, 0.3f, C2D_Color32(255, 255, 255, 255));
            if (an[k].dex == d.base) r.text("(sprite normal)", x, y + 15, 0.28f, C2D_Color32(170, 180, 200, 255), 1, false);
          }
          C2D_TargetClear(bottom, C2D_Color32(16, 22, 34, 255));
          C2D_SceneBegin(bottom);
          C3D_FrameEnd(0);
        }
        for (int k = 0; k < n; ++k) {   // ¿ha cargado cada uno?
          checked++;
          if (!r.sprites.get(an[k].dex, an[k].shiny) || !anim_meta(an[k].dex, A_WALK, an[k].shiny) || !anim_meta(an[k].dex, A_IDLE, an[k].shiny)) {
            bad++;
            if (at.log) std::fprintf(at.log, "galería: FALLA %s (%s, sprite %d)\n", kMegas[pg * kPer + k].mon.name, sh ? "shiny" : "normal", an[k].dex);
          }
        }
      }
    if (at.log) { std::fprintf(at.log, "galería: %d megas · %d comprobadas · %d fallos\n", kMegaCount, checked, bad); std::fclose(at.log); at.log = nullptr; }
    app.quit = true;
    at.on = false;
  } else if (at.programa) {
    // Actualizar el programa (o volver atrás): espera a que esté listo y sale;
    // al salir se instala (como al cerrar el juego) y se apunta en programa.log.
    const u64 t0 = osGetTime();
    if (at.programa == 1) prog3ds::start_check(true); else prog3ds::start_rollback();
    while (aptMainLoop() && osGetTime() - t0 < 240000) {
      const int st = prog3ds::state();
      if (st == prog3ds::ST_READY || st == prog3ds::ST_FAILED || st == prog3ds::ST_UPTODATE) break;
      gspWaitForVBlank();
    }
    if (at.log) {
      std::fprintf(at.log, "programa: build %d · %s · %s · pendiente: %s · %.1f s\n", PS_BUILD, prog3ds::supported() ? (prog3ds::is_cia() ? ".cia" : ".3dsx") : "sin ruta",
                   prog3ds::status().c_str(), prog3ds::pending() ? prog3ds::pending_text().c_str() : "nada", (osGetTime() - t0) / 1000.0);
      std::fclose(at.log); at.log = nullptr;
    }
    at.on = false;
    app.quit = true;   // (al salir se instala)
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
    saver3ds::shutdown();   // (la partida sincronizada, escrita antes de salir)
    cloud3ds::shutdown();
    audio::exit(); C2D_Fini(); C3D_Fini(); gfxExit(); romfsExit();
    return 0;
  } else if ((!at.on && !at.tour) || at.boot) {
    // ("… arranque" en el autotest: como al encender de verdad, para probar que la
    // versión nueva se baja mientras se juega y se instala al salir)
    if (!at.boot) app.cloud_autostart();   // (si hay cuenta, se sincroniza al encender)
    // La versión nueva del juego se busca SIEMPRE al encender y se baja sola (el
    // dueño, 2026-10-27); se instala al salir. La opción de Ajustes es para los
    // datos de equilibrio de la web.
    prog3ds::start_check(true);
    if (at.boot && at.log) { std::fprintf(at.log, "arranque: buscar versión nueva SÍ\n"); std::fflush(at.log); }
  }
  if (at.on && at.coop) {
    std::snprintf(app.auto_pass_, sizeof(app.auto_pass_), "%s", at.room_pass);
    std::snprintf(app.auto_room_, sizeof(app.auto_room_), "%s", at.room_host);
    app.autolog = at.log;
    app.auto_quick_ = at.quick;
    app.auto_win = at.win;
    app.auto_real = at.real;
    app.auto_skin = at.skin;
    app.auto_aura = at.aura;
    app.auto_weekly = at.weekly;
    app.set_coop_autotest(at.host, at.dex, at.immortal, at.online, at.die);
    app.auto_mega = at.coop_mega;
    app.auto_coop_time = int(at.warp);
    app.auto_level = at.level;
    app.auto_look = at.look;
    if (at.log) { std::fprintf(at.log, "cooperativo: %s%s\n", at.host ? "anfitrión" : "invitado", at.online ? " · por internet" : ""); std::fflush(at.log); }
  } else if (at.on) {
    // (movimiento "mega": el bot recibe la megapiedra a los 5 s y megaevoluciona)
    const bool mega = !std::strcmp(at.move, "mega") || !std::strcmp(at.move, "megashiny");
    app.auto_shiny = !std::strcmp(at.move, "megashiny");
    app.auto_mega = mega;
    app.auto_level = at.level;
    app.auto_mega_at = at.mega_at;
    app.auto_stone = at.stone;
    r.trailer_mark = at.mark;
    app.auto_peace = at.peace;
    app.auto_still = at.still;
    app.auto_bond = at.bond;
    app.auto_hold_over = at.end_screen;
    app.auto_real = at.real;
    app.auto_orbs = at.orbs;
    app.auto_diff = at.diff;
    app.auto_treasure = at.treasure;
    app.auto_mode = at.quick ? 1 : 0;
    app.auto_win = at.win;
    app.auto_skin = at.skin;
    app.auto_aura = at.aura;
    app.auto_weekly = at.weekly;
    app.auto_mega_wait = at.mega_wait;
    app.set_autotest(at.dex, at.immortal, at.warp, at.move[0] && !mega ? move_index(at.move) : -1);
    at.next_log = at.warp + 10;
    if (at.rocks > 0) {   // (como tras minutos rompiendo cosas por el mapa: chunks con props cambiados)
      const float cw = kChunkTiles * kTS;
      int done = 0;
      for (int k = 0, x = 0, y = 0, dx = 0, dy = -1; done < at.rocks && k < 20000; ++k) {
        bool hit = false;
        g->world.for_props_in_rect(x * cw + 1, y * cw + 1, (x + 1) * cw - 1, (y + 1) * cw - 1, [&](Prop& p) {
          if (!hit && p.destructible && p.hp >= p.max_hp) { p.hp = p.max_hp * 0.5f; hit = true; }
        });
        done += hit;
        if (x == y || (x < 0 && x == -y) || (x > 0 && x == 1 - y)) { const int t = dx; dx = -dy; dy = t; }
        x += dx; y += dy;
      }
      if (at.log) { std::fprintf(at.log, "rocas golpeadas en %d chunks\n", done); std::fflush(at.log); }
    }
    if (at.log && at.move[0]) std::fprintf(at.log, "empieza con %s (%d): %s\n", at.move, move_index(at.move), kMoves[g->pl().moves[0].def].id);
    if (at.log) {
      std::fprintf(at.log, "precargados %d Pokémon · %lu KB de sprites · memoria libre %lu KB · partida lista en %lu ms\n", r.sprites.loaded(),
                   (unsigned long)(r.sprites.bytes() / 1024), (unsigned long)(mem_free() / 1024), (unsigned long)app.start_ms);
      std::fflush(at.log);
    }
  }

  u64 last = osGetTime();
  const double boot_s = last / 1000.0;
  double acc = 0;
  u64 fps_t = last;
  int fps_frames = 0;

  int frames_shown = 0;
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
    // Vigilante de tirones: un fotograma de más de 50 ms se nota (el juego se
    // queda «pillado» y la música puede cortarse). Se apunta en el log.
    if (frame_ms > 50 && fps_frames + r.fps > 30) {
      g_hitch_n++;
      if (at.log && g_hitch_n <= 30) { std::fprintf(at.log, "tirón: %.0f ms en la pantalla %d (a los %.1f s) · el anterior: juego %d ms, suelo %d ms, dibujo %d ms\n", frame_ms, int(app.scene), osGetTime() / 1000.0 - boot_s, g_ms_logic, g_ms_ground, g_ms_draw); std::fflush(at.log); }
      if (int(frame_ms) > g_hitch_max) { g_hitch_max = int(frame_ms); std::snprintf(g_hitch_where, sizeof(g_hitch_where), "pantalla %d", int(app.scene)); }
    }
    acc += frame_ms;
    last = now;
    if (acc > 100) acc = 100;
    const u64 tl0 = osGetTime();
    app.update(in, float(std::fmin(frame_ms, 100.0) / 1000.0));

    // --- paso fijo de la partida ---
    if (app.scene == SC_RUN) {
      app.advance(acc, float(std::fmin(frame_ms, 100.0) / 1000.0));
    } else {
      acc = 0;
    }

    {
      // (Halloween: en partida, lo que era al empezarla; en los menús, lo de ahora)
      const int mt = app.scene == SC_RUN ? g->music : int(MU_VILLAGE);
      const bool hwm = app.scene == SC_RUN ? g->hw : app.menu_hw_;
      audio::music(mt >= 0 && hwm ? audio::kHwTrack + mt : mt);
    }
    audio::set_volume(settings.music, settings.sfx);
    audio::update(float(std::fmin(frame_ms, 100.0) / 1000.0));

    // --- dibujo ---
    const u64 tg0 = osGetTime();
    g_ms_logic = int(tg0 - tl0);
    r.begin_frame(*g);
    const u64 td0 = osGetTime();
    g_ms_ground = int(td0 - tg0);
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
    if (++frames_shown == 120) crash3ds::boot_done();   // (2 s en pantalla: el arranque ha ido bien)
    g_ms_draw = int(osGetTime() - td0);

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
      // Jugando de verdad: si va lento (menos de 30 FPS), se apunta en la SD
      // qué había en pantalla y en qué se fue el tiempo (rendimiento.log),
      // como mucho una línea cada 5 s y 200 por sesión, para encontrar el porqué.
      static int slow_lines = 0;
      static double slow_last = -100;
      if (!at.on && app.scene == SC_RUN && r.fps < 30 && slow_lines < 200 && g->time - slow_last >= 5) {
        slow_last = g->time;
        char lp[96];
        std::snprintf(lp, sizeof(lp), "%s/rendimiento.log", kDir);
        if (FILE* lf = std::fopen(lp, slow_lines == 0 ? "w" : "a")) {
          if (slow_lines == 0) std::fprintf(lf, "PokéSurvivor 3DS %s · build %d · %s\n", kVersion, PS_BUILD, prog3ds::is_cia() ? ".cia" : ".3dsx");
          std::fprintf(lf, "t=%.0f s · %d FPS · %s · grupo %d · enemigos %d · proyectiles %d · partículas %d · objetos %d · peligros %d · "
                       "juego %d ms · suelo %d ms · dibujo %d ms · sprites %d · lineal %lu KB · heap %lu KB\n",
                       g->time, r.fps, g->world.biome_name(), g->team_size(), g->enemies.n, g->projectiles.n, g->fx.parts.n, g->pickups.n,
                       g->hazards.n, g_ms_logic, g_ms_ground, g_ms_draw, r.sprites.loaded(), (unsigned long)(linearSpaceFree() / 1024), heap_kb());
          std::fclose(lf);
          slow_lines++;
        }
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
    // (autotest) La actualización del programa: cada cambio de estado, con el reloj.
    if (at.on && at.log && prog3ds::state() != at.prog_state) {
      at.prog_state = prog3ds::state();
      std::fprintf(at.log, "programa: %s · a los %.0f s de partida (pantalla %d)\n", prog3ds::status().c_str(), g->time, int(app.scene));
      std::fflush(at.log);
    }
    // Cooperativo: la huella cada 10 s (para comparar los logs de las consolas).
    if (at.on && at.log && app.coop()) {
      const net::Lockstep& ls = app.lockstep();
      if (ls.last_hash_tick() > 0 && ls.last_hash_tick() % 600 == 0 && ls.last_hash_tick() != at.hash_logged) {
        at.hash_logged = ls.last_hash_tick();
        std::fprintf(at.log, "huella paso %lld = %08lx · jugadores %d · esperando %d fotogramas · desincronizado %s · retraso %d · ping %d ms · por delante %d · tirones %d · saltos %d · %s\n", at.hash_logged,
                     (unsigned long)ls.last_hash(), g->team_size(), ls.stall_frames(), ls.desync() ? "SÍ" : "no", ls.delay(), net3ds::rtt_ms(),
                     ls.ahead(), ls.starved(), ls.bursts(), ls.relay() ? "servidor" : ls.host() ? "anfitrión" : "invitado");
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
    if (at.on && at.end_screen && (g->time >= at.seconds || g->state == ST_OVER)) {
      if (g->state != ST_OVER) {
        if (!at.coop) g->quit_run();
        else if (at.host) app.pending_input.cmd = CMD_QUIT;   // (se va el anfitrión: se acaba para todos)
        at.end_at = osGetTime();
      } else {
        if (at.end_at == 0) at.end_at = osGetTime();
        // En grupo, a mitad de la espera, el resumen del compañero (como pulsar R).
        if (at.coop && osGetTime() - at.end_at > 4000 && !at.tab_moved) { app.over_tab_next(); at.tab_moved = true; }
        if (osGetTime() - at.end_at > 8000) {
          if (!at.to_menu) break;
          // «menu»: como pulsar B en el resumen; luego 6 s en el menú (la medalla, guardar...).
          if (app.scene == SC_RUN) { app.leave_to_menu(); at.end_at = osGetTime(); if (at.log) { std::fprintf(at.log, "resumen → menú\n"); std::fflush(at.log); } }
          else break;   // (6-8 s en el menú sin romperse: listo)
        }
      }
      continue;
    }
    if (at.on && g->time >= at.seconds) break;
    if (at.on && at.coop && !at.end_screen && g->state == ST_OVER && app.scene == SC_RUN) break;
  }

  if (at.log) {
    std::fprintf(at.log, "cargas de sprites durante la partida: %lu · hilo principal media %lu us, máx %lu us · lectura en el hilo máx %lu ms\n",
                 (unsigned long)r.sprites.late_loads,
                 (unsigned long)(r.sprites.late_loads ? r.sprites.late_total_us / r.sprites.late_loads : 0),
                 (unsigned long)r.sprites.late_max_us, (unsigned long)(r.sprites.read_max_us / 1000));
    std::fprintf(at.log, "tirones (fotogramas de más de 50 ms): %d · el peor %d ms (%s) · guardados %d, el más lento %d ms (en segundo plano)\n",
                 g_hitch_n, g_hitch_max, g_hitch_where, saver3ds::saves_done(), saver3ds::slowest_ms());
    if (at.coop) std::fprintf(at.log, "grupo: modo %s · contraseña rechazada %d veces\n", g->cfg.mode == MODE_QUICK ? "rápido" : "normal", app.denied);
    std::fprintf(at.log, "FIN · %.0f s de partida · FPS media %.1f · mín %d · nivel %d · derrotados %d · libre %lu KB\n", g->time,
                 at.fps_n ? at.fps_sum / at.fps_n : 0.0, at.min_fps == 999 ? 0 : at.min_fps, g->pl().level, g->pl().kills,
                 (unsigned long)(mem_free() / 1024));
    std::fclose(at.log);
  }
  saver3ds::shutdown();   // (lo que quedara por escribir, antes de salir)
  if (idioma_test) {
    std::snprintf(idioma_path, sizeof(idioma_path), "%s/idioma.log", kDir);
    if (FILE* f = std::fopen(idioma_path, "w")) {
      tr_misses([](const char* es, const char* en, void* ud) { std::fprintf(static_cast<FILE*>(ud), "%s\t→\t%s\n", es, en); }, f);
      std::fclose(f);
    }
  }
  if (!at.on && g->dirty) { save->updated = int64_t(osGetTime()) - 2208988800000LL; save_write_file(kDir, *save); }
  audio::exit();
  romfsExit();   // (el .3dsx se puede cambiar cuando ya no se lee de él)
  // Versión nueva (o volver a la anterior) pendiente: se instala ahora.
  if (prog3ds::pending_at_exit()) {
    g_show_r = &r; g_show_top = top; g_show_bottom = bottom;
    const std::string res = prog3ds::apply(show_install);
    show_install(res.c_str(), 100);
    char lp[96];   // (lo que ha pasado, para mirarlo luego: programa.log)
    std::snprintf(lp, sizeof(lp), "%s/programa.log", kDir);
    if (FILE* lf = std::fopen(lp, "ab")) { std::fprintf(lf, "build %d → %s\n", PS_BUILD, res.c_str()); std::fclose(lf); }
    for (int i = 0; i < 90 && aptMainLoop(); ++i) gspWaitForVBlank();   // (un momento para leerlo)
  }
  prog3ds::shutdown();
  cloud3ds::shutdown();
  C2D_Fini();
  C3D_Fini();
  gfxExit();
  return 0;
}

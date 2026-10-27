// app.h — las pantallas del juego fuera (y alrededor) de la partida:
// test de personalidad, compañero, menú, colección, mejoras, gacha, misiones,
// ajustes y créditos (test.js, menu.js, gacha-ui.js, slots.js, goals-ui.js,
// settings-ui.js de la web). Arriba se ve el mundo con Pokémon paseando
// (el Backdrop de game.js) o el detalle; abajo, los botones (táctil o cruceta).
#pragma once
#include <3ds.h>

#include "core/bot.h"
#include "core/gacha.h"
#include "core/game.h"
#include "core/lockstep.h"
#include "core/save.h"
#include "core/settings.h"
#include "platform/3ds/net3ds.h"
#include "platform/3ds/render.h"

namespace ps {

enum Scene : int8_t {
  SC_TEST, SC_RESULT, SC_STARTERS, SC_MENU, SC_DEX, SC_UPGRADES, SC_BIND, SC_BAN, SC_START, SC_GACHA, SC_PULL,
  SC_GOALS, SC_SETTINGS, SC_CREDITS, SC_RUN, SC_COOP, SC_LOBBY, SC_ACCOUNT, SC_CLOUDASK, SC_UPDATE, SC_NEWS, SC_MOVE2,
  SC_MODE,   // Jugar: normal o rápido
  SC_ROOMSET  // Crear sala: modo y contraseña
};

struct UiIn {
  u32 down = 0, held = 0, rep = 0;   // rep: pulsaciones con repetición (cruceta)
  bool tap = false;                   // toque nuevo en la pantalla de abajo
  int tx = 0, ty = 0;
  float cx = 0, cy = 0;               // Circle Pad (-1..1)
};

struct Wanderer { int16_t dex; Animator anim; float x, y, r, vx, vy, t; };

class App {
 public:
  bool init(Renderer* r, Game* g, Save* s, Settings* st, const char* dir);
  void set_autotest(int dex, bool immortal, float warp = 0, int move = -1);
  // Autotest del cooperativo: crea la sala (o se une a la primera que vea),
  // empieza con 2 y el bot juega (para dos emuladores en la misma sala).
  void set_coop_autotest(bool host, int dex, bool immortal, bool online = false, bool suicidal = false);
  // Recorrido por todas las pantallas (para sacar capturas en el emulador).
  void start_tour();
  void start_news_demo(int dex);
  void start_gacha_demo();
  void start_rooms_demo();
  void start_menu_demo();
  void in_mode(const UiIn& in);
  void in_roomset(const UiIn& in);
  void bottom_roomset();
  void coop_create_room();
  // Sala: modo y contraseña de la que creas; la contraseña con la que entras y
  // las banderas de la sala en la que estás.
  int room_mode_ = 0;
  char room_pass_[12] = "";
  char join_pass_[12] = "";
  uint8_t room_flags_ = 0;
  // Autotest del grupo: contraseña de la sala (anfitrión) o la que prueba el invitado, y modo.
  char auto_pass_[12] = "";
  bool auto_quick_ = false;
  int denied = 0;               // (autotest) veces que el anfitrión no te ha dejado entrar
  FILE* autolog = nullptr;      // (autotest) el log, para apuntar lo que pasa en la sala
  void bottom_mode();
  void start_update_demo();        // autotest "actualizar": busca y baja la versión nueva (para grabarlo)   // autotest "novedades [dex]": las novedades y el menú con ese compañero
  bool tour = false;
  void update(const UiIn& in, float dt);
  void draw_top(float eye = 0);
  void draw_bottom();
  bool quit = false;
  bool autotest = false;
  uint32_t start_ms = 0;   // lo que tardó en empezar la última partida
  Scene scene = SC_MENU;
  Input pending_input;          // entrada de la partida de este fotograma
  // Da los pasos de la partida que tocan (solo o en grupo, con el lockstep)
  // y toca sus sonidos. `acc`: milisegundos acumulados.
  void advance(double& acc, float dt);
  // Cooperativo (para el log del autotest y la pantalla).
  bool coop() const { return coop_; }
  const net::Lockstep& lockstep() const { return ls_; }
  Bot bot;

 private:
  void go(Scene s);
  void start_run();
  void save_now();
  void apply_settings();
  // Fondo del menú
  void backdrop_init();
  void backdrop_step(float dt);
  void draw_backdrop(float dim, float eye = 0);
  // Entrada de cada pantalla
  void in_run(const UiIn& in);
  void in_test(const UiIn& in);
  void in_menu(const UiIn& in);
  void in_dex(const UiIn& in);
  void in_upgrades(const UiIn& in);
  void in_gacha(const UiIn& in);
  void in_pull(const UiIn& in, float dt);
  void in_goals(const UiIn& in);
  void in_settings(const UiIn& in);
  void in_coop(const UiIn& in);
  void in_lobby(const UiIn& in, float dt);
  void in_account(const UiIn& in);
  void in_cloudask(const UiIn& in);
  // Dibujo
  void bottom_test();
  void bottom_menu();
  void bottom_dex();
  void bottom_upgrades();
  void bottom_gacha();
  void bottom_pull();
  void bottom_goals();
  void bottom_settings();
  void bottom_credits();
  void bottom_coop();
  void bottom_lobby();
  void bottom_account();
  void in_update(const UiIn& in);   // Actualizaciones del juego (app_update.cpp)
  void bottom_update();
  void bottom_cloudask();
  // Novedades (app_news.cpp): SELECT en el menú; se abren solas una vez.
  void in_news(const UiIn& in);
  void bottom_news();
  void top_news(float eye);
  bool news_unseen() const;
  void open_news();
  void mega_badge(int dex);     // menú: las megapiedras de tu compañero
  void coop_overlay();          // arriba: «esperando a…», avisos de la red
  void top_detail(int dex, bool shiny, bool owned);
  // Colección → SELECT: los ataques que puede aprender, grandes y con su ficha.
  void in_dex_moves(const UiIn& in, int dex);
  void bottom_dex_moves(int dex);
  void top_dex_move(int dex);
  int dex_moves(int dex, int16_t* out);
  void y_hint(float x, float y, bool shiny);   // botón Y que se pulsa solo · «Normal · Shiny»
  float y_hint_w();                            // (su ancho)
  int atajos_row() const;                      // Mejoras: fila de «Atajos» (tras «Megapiedra» si tiene mega)
  // Texto que cabe en `maxw`: baja la letra si hace falta y, si ni así, lo corta con «…».
  void fit_text(const char* s, float x, float y, float maxw, float size, u32 color, bool shadow = true);   // botón Y que se pulsa solo · «Normal · Shiny»
  void top_pull();
  void top_cloud_badge();          // pantalla de arriba: con quién has entrado y la última sincronización
  void top_tickets_badge(float cx, float y);   // pantalla de arriba: los tickets que te quedan
  void draw_mon(int dex, bool shiny, float x, float y, float scale, int mode = 0, int dir = 0);
  void header(const char* title);
  bool button(float x, float y, float w, float h, const char* label, bool sel, const UiIn* in = nullptr, u32 col = 0);
  void notices(float dt);

  Renderer* r_ = nullptr;
  Game* g_ = nullptr;
  Save* s_ = nullptr;
  Settings* st_ = nullptr;
  const char* dir_ = nullptr;
  float t_ = 0;
  int cur_ = 0, cur2_ = 0, scroll_ = 0;
  // test
  int q_ = 0;
  TraitScores scores_;
  int res_trait_ = 0, res_dex_ = 1;
  // colección
  int gen_ = 1, filter_ = 0;
  bool view_shiny_ = false;
  bool dex_moves_ = false;      // Colección: la vista de ataques (SELECT)
  int mv_cur_ = 0;
  int dexmv_ = 0;               // (recorrido «ui»: pasos en la vista de ataques)
  // mejoras
  int up_dex_ = 0;
  // gacha
  PullResult pulls_[100];
  // ×100: diez giros, cada uno con lo mejor de su ×10; al final, el resumen
  // ordenado de mejor a peor (sum_, con scroll).
  int16_t sum_[100];
  int sum_scroll_ = 0;
  int iris_mask_ = -1;          // Amuletos Iris que ya tenías (bit por región; -1 aún sin mirar)
  int iris_gen_ = 0;            // la tirada te ha dado el Amuleto Iris de esa región
  int nspins() const { return npulls_ == 100 ? 10 : npulls_; }
  const PullResult& spin(int i) const;
  int npulls_ = 0, pull_i_ = 0;
  float pull_t_ = 0;
  bool pull_fast_ = false, pull_done_ = false;
  int tab_ = 0;
  // fondo
  Wanderer wd_[20];
  Camera bcam_;
  float bt_ = 0, bbiome_t_ = 0;
  int bbiome_ = 0;
  Rng urng_{777};
  // avisos (misiones y logros)
  float notice_t_ = 0;
  int bond_first_ = -1;
  bool auto_bond_given_ = false;
  bool paused_ = false;         // en grupo: la pausa es sólo de esta consola
  // créditos
  char* credits_ = nullptr;
  int credit_lines_ = 0;
  float tour_t_ = 0;
  int tour_i_ = 0;
  int auto_dex_ = 25;
  int auto_move_ = -1;
  float auto_pause_ = -1;        // autotest con movimiento: pausa un rato (resumen en las capturas)
  bool auto_paused_ = false;
  bool newer_told_ = false;      // ya se ha dicho que hay un programa nuevo
  bool auto_immortal_ = true;
  // cooperativo
  RunConfig::Member my_member() const;
  void lobby_net(float dt);
  void lobby_send_roster();
  void coop_host_start();
  void coop_begin(const RunConfig& cfg, int idx, int delay);
  void coop_end(const char* msg);
  void coop_net();               // paquetes de la partida en grupo
  void coop_leave();             // sale de la sala y apaga la radio
  bool coop_ = false;
  net::Lockstep ls_;
  RunConfig coop_cfg_;
  uint16_t coop_nodes_[kMaxPlayers] = {};
  bool start_acked_[kMaxPlayers] = {};
  struct LobbyMate { uint16_t node = 0; RunConfig::Member m; int rtt = 0; };
  bool coop_online_ = false;    // «En grupo»: por internet (si no, cerca)
  LobbyMate lobby_[kMaxPlayers];
  int nlobby_ = 0;
  net3ds::Room rooms_[8];
  int nrooms_ = 0;
  bool scanned_ = false;
  float lobby_t_ = 0, coop_over_t_ = 0, quit_t_ = -1;
  char coop_msg_[96] = {};
  bool auto_coop_ = false, auto_host_ = false;
 public:
  bool auto_look = false;       // autotest: busca salas sin entrar
 private:
  // sincronizar con pokesurvivor.com
 public:
  void cloud_tick();             // aplica lo que llegue de la nube (cada fotograma)
  void cloud_autostart();        // al arrancar y al acabar una partida (si hay cuenta)
  bool cloud_test(const char* user, const char* pass);   // autotest "nube"
  void show_account() { go(SC_ACCOUNT); }   // autotest "nube google": la pantalla de la cuenta en las capturas
  int cloud_done = 0;            // (autotest) 1 = bien, -1 = mal
 private:
  int64_t cloud_t0_updated_ = 0; // `updated` de la partida al empezar a sincronizar
  char cloud_msg_[128] = {};
  bool over_synced_ = false;
  uint32_t choice_sig_ = 0;     // la elección que hay en pantalla (cartas, objeto, vínculo)
  int cloud_t0_partner_ = -1;   // compañero al empezar a sincronizar
  void over_tab_step(int d);
  void orb_next(const Player& me);
  bool update_bar() const;
  Scene update_back_ = SC_SETTINGS;   // a dónde vuelve B desde Actualizaciones
  int orb_sel_ = 0;             // esfera elegida (A la usa, B pasa a la siguiente)
  int over_tab_ = -1;           // fin de la partida: de quién es el resumen de arriba (L/R; -1: tuyo)
  int spect_ = -1;              // caído en grupo: a quién sigue la cámara (-1: a ti)
  bool crash_seen_ = false;
  bool news_checked_ = false;
  bool news_demo_ = false, update_demo_ = false, gacha_demo_ = false, rooms_demo_ = false, menu_demo_ = false;
  float news_t_ = 0;   // ya se ha mirado si hay novedades sin ver     // ya se ha visto el aviso del fallo de la vez anterior
  int auto_mega_up_ = 0;
  float auto_warp_ = 0;         // (dónde empezó el reloj del autotest)
  bool auto_leveled_ = false;
  float auto_stone_t_ = 0;
  float auto_mega_t0_ = -1;
  bool still_now_ = false;        // autotest: nivel de la mejora «Megapiedra» (para ver megas)
 public:
  bool auto_mega = false;       // autotest "… mega": el bot con su megapiedra
  bool auto_shiny = false;      // autotest "… megashiny": y en shiny
  // Autotest para grabar (tráiler): "… mega nivel=40 megaen=6 piedra=b"
  int auto_level = 0;           // sube a este nivel al empezar (el bot elige las cartas)
  float auto_mega_at = 5;       // segundos de partida (desde el principio, aunque se adelante el reloj) hasta darle la piedra
  int auto_stone = 0;
  int auto_coop_time = 0;
  float auto_mega_wait = 0;     // "… megaespera=S": con la piedra puesta, espera S s antes de pulsar Y
  bool auto_peace = false;
  bool auto_still = false;      // "… quieto": quieto mirando abajo hasta 3 s después de megaevolucionar      // "… paz": sin enemigos (para ver la megaevolución)       // en grupo (anfitrión): "coop … desde=N nivel=N" (lo lleva la sala)           // 0: IT_MEGA_A · 1: IT_MEGA_B (Charizard Y, Absol Z…)
  bool auto_bond = false;
  bool auto_hold_over = false;
  int auto_mode = 0;            // autotest "… rapido": modo rápido
  bool auto_orbs = false;       // autotest "esferas": una de cada al empezar
  bool auto_orbs_given_ = false;
  void over_tab_next();         // (autotest: como pulsar R en el resumen)  // autotest "fin": no vuelve a empezar al acabar (se ve el resumen)    // autotest "vinculo": te da un vínculo y deja marcado el primero (captura)
 private:
  bool cloud_want_ = false;     // sincronizar en cuanto se pueda (había otra en marcha)
  bool cloud_asked_ = false;     // ya se ha preguntado al abrir
  bool login_now_ = false;             // abrir el teclado para entrar (desde la pregunta)
  Scene account_back_ = SC_SETTINGS;   // a dónde vuelve B desde la cuenta
  float auto_t_ = 0;
  uint64_t coop_retry_ = 0;     // próximo intento de conectar con el servidor (ms)
  bool coop_err_init_ = false;  // el mensaje es del intento de conectar
  int coop_fails_ = 0;          // intentos seguidos sin poder conectar
};

}  // namespace ps

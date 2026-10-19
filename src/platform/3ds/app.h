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
  SC_TEST, SC_RESULT, SC_STARTERS, SC_MENU, SC_DEX, SC_UPGRADES, SC_BIND, SC_BAN, SC_GACHA, SC_PULL,
  SC_GOALS, SC_SETTINGS, SC_CREDITS, SC_RUN, SC_COOP, SC_LOBBY
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
  void set_autotest(int dex, bool immortal, float warp = 0);
  // Autotest del cooperativo: crea la sala (o se une a la primera que vea),
  // empieza con 2 y el bot juega (para dos emuladores en la misma sala).
  void set_coop_autotest(bool host, int dex, bool immortal);
  // Recorrido por todas las pantallas (para sacar capturas en el emulador).
  void start_tour();
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
  void coop_overlay();          // arriba: «esperando a…», avisos de la red
  void top_detail(int dex, bool shiny, bool owned);
  void top_pull();
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
  // mejoras
  int up_dex_ = 0;
  // gacha
  PullResult pulls_[10];
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
  bool paused_ = false;         // en grupo: la pausa es sólo de esta consola
  // créditos
  char* credits_ = nullptr;
  int credit_lines_ = 0;
  float tour_t_ = 0;
  int tour_i_ = 0;
  int auto_dex_ = 25;
  bool auto_immortal_ = true;
  // cooperativo
  RunConfig::Member my_member() const;
  void lobby_net(float dt);
  void lobby_send_roster();
  void coop_host_start();
  void coop_begin(const RunConfig& cfg, int idx);
  void coop_end(const char* msg);
  void coop_net();               // paquetes de la partida en grupo
  void coop_leave();             // sale de la sala y apaga la radio
  bool coop_ = false;
  net::Lockstep ls_;
  RunConfig coop_cfg_;
  uint16_t coop_nodes_[kMaxPlayers] = {};
  bool start_acked_[kMaxPlayers] = {};
  struct LobbyMate { uint16_t node = 0; RunConfig::Member m; };
  LobbyMate lobby_[kMaxPlayers];
  int nlobby_ = 0;
  net3ds::Room rooms_[8];
  int nrooms_ = 0;
  bool scanned_ = false;
  float lobby_t_ = 0, coop_over_t_ = 0, quit_t_ = -1;
  char coop_msg_[96] = {};
  bool auto_coop_ = false, auto_host_ = false;
  float auto_t_ = 0;
};

}  // namespace ps

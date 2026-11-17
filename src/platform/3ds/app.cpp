// app.cpp — ver app.h.
#include "platform/3ds/crash3ds.h"
#include "platform/3ds/app.h"
#include "platform/3ds/hw3ds.h"
#include "core/mastery.h"
#include "core/gift.h"
#include "core/update.h"
#include "core/halloween.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/gacha.h"
#include "core/i18n.h"
#include "core/progress.h"
#include "core/runclaim.h"
#include "core/abilities.h"
#include "core/mega.h"
#include "core/power.h"
#include "core/mons.h"
#include "core/orbs.h"
#include "core/save_io.h"
#include "core/stats.h"
#include "core/version.h"
#include "core/datapack.h"
#include "platform/3ds/datos3ds.h"
#include "core/upgrades.h"
#include "core/util.h"
#include "platform/3ds/app_ui.h"
#include "platform/3ds/audio3ds.h"
#include "platform/3ds/cloud3ds.h"
#include "platform/3ds/net3ds.h"
#include "platform/3ds/prog3ds.h"
#include "platform/3ds/saver3ds.h"

namespace ps {


// ---------------- arranque ----------------

bool App::init(Renderer* r, Game* g, Save* s, Settings* st, const char* dir) {
  r_ = r; g_ = g; s_ = s; st_ = st; dir_ = dir;
  apply_settings();
  load_credits();
  backdrop_init();
  g_->now_ms = now_ms();
  if (!s_->partner) { q_ = 0; scores_ = TraitScores(); go(SC_TEST); }
  else go(SC_MENU);
  return true;
}

// Créditos (tools/gen_credits.py), en el idioma elegido.
void App::load_credits() {
  std::free(credits_);
  credits_ = nullptr;
  credit_lines_ = 0;
  credits_lang_ = st_->lang;
  FILE* f = std::fopen(st_->lang == 1 ? "romfs:/credits_en.txt" : "romfs:/credits.txt", "rb");
  if (!f && st_->lang == 1) f = std::fopen("romfs:/credits.txt", "rb");
  if (!f) return;
  std::fseek(f, 0, SEEK_END);
  const long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  credits_ = static_cast<char*>(std::malloc(n + 1));
  const size_t got = std::fread(credits_, 1, n, f);
  credits_[got] = 0;
  std::fclose(f);
  for (char* p = credits_; *p; ++p) if (*p == '\n') { *p = 0; credit_lines_++; }
}

void App::set_autotest(int dex, bool immortal, float warp, int move) {
  autotest = true;
  auto_dex_ = dex;
  auto_move_ = move;
  auto_immortal_ = immortal;
  bot.init(12345);
  start_run();
  auto_warp_ = warp;
  if (warp > 0) g_->time = warp;
}

void App::start_tour() {
  tour = true;
  tour_t_ = 0; tour_i_ = 0;
  // Partida de prueba: sin compañero (empieza en el test) y con tickets para el gacha.
  s_->partner = 0;
  s_->t1 = 30; s_->t10 = 1;
  q_ = 0; scores_ = TraitScores();
  go(SC_TEST);
}

void App::start_news_demo(int dex) {
  news_demo_ = true;
  news_t_ = 0;
  news_checked_ = cloud_asked_ = true;
  if (mon_by_dex(dex)) s_->partner = int16_t(dex);
  go(SC_NEWS);
}

// Autotest "gacha100": 100 tickets, tirada ×100 en el banner de tu compañero y su resumen.
void App::start_gacha_demo(bool all) {
  gacha_demo_ = true;
  if (all) bgen_ = kGachaAll;
  else if (hw3ds::on()) bgen_ = kGachaSpooky;   // (Halloween: en la Noche Espectral)
  news_t_ = 0;
  news_checked_ = cloud_asked_ = true;
  s_->t1 += 100;
  go(SC_GACHA);
}

// Autotest "salas": la pantalla de crear sala (rápido, con contraseña) y una
// lista de salas de ejemplo (candado, modo rápido), sin red.
// Autotest "menu": el menú principal un minuto (la barra de la versión nueva).
// Autotest "mejoras": Pokémonedas de sobra y el compañero con «Ataque inicial» y
// «Repertorio» comprados (para ver las listas con Físico / Especial).
void App::start_upgrades_demo() {
  up_demo_ = true;
  news_t_ = 0;
  news_checked_ = cloud_asked_ = true;
  if (s_->coins < 9000) s_->coins = 9000;
  s_->pfour[s_->partner] = 0;
  s_->pupg[s_->partner][UP_START] = uint8_t(kUpgrades[UP_START].max);
  s_->pupg[s_->partner][UP_MOVES] = uint8_t(kUpgrades[UP_MOVES].max);
  go(SC_MENU);
}

void App::start_menu_demo() {
  menu_demo_ = true;
  news_t_ = 0;
  news_checked_ = cloud_asked_ = true;
  go(SC_MENU);
}

// Autotest "modo": la pantalla de elegir modo con el rápido marcado. "medalla": el
// menú y la Colección (en la región de tu compañero) para ver la medalla de Arceus.
// (Jugar, cur_: 0 normal, 1 rápido, 2 el desafío semanal)
constexpr int kModeWeekly = MODE_COUNT;

void App::start_mode_demo(bool medal) {
  mode_demo_ = medal ? 2 : 1;
  news_t_ = 0;
  news_checked_ = cloud_asked_ = true;
  if (medal) go(SC_MENU);
  else { go(SC_MODE); cur_ = MODE_QUICK; }
}

void App::start_rooms_demo() {
  rooms_demo_ = true;
  news_t_ = 0;
  news_checked_ = cloud_asked_ = true;
  room_mode_ = MODE_QUICK;
  std::snprintf(room_pass_, sizeof(room_pass_), "pika123");
  go(SC_ROOMSET);
  cur_ = 1;
}

void App::start_update_demo() {
  update_demo_ = true;
  news_t_ = 0;
  news_checked_ = cloud_asked_ = true;
  go(SC_UPDATE);
}

void App::apply_settings() {
  i18n_set(st_->lang == 1 ? LANG_EN : LANG_ES);
  // Calidad automática según el modelo: alta en la New 3DS, media en la vieja.
  bool n3ds = false;
  APT_CheckNew3DS(&n3ds);
  g_->fx.quality = st_->quality < 0 ? (n3ds ? 2 : 1) : st_->quality;
  r_->old_3ds = !n3ds;
  g_->fx.dmg_numbers = st_->dmg_numbers;
  g_->camera.calm = st_->calm;
  r_->show_fps = st_->fps;
}

void App::save_now() {
  // (en la hora del servidor: el reloj de la 3DS va en hora local y la web en UTC)
  s_->updated = now_ms() + cloud3ds::account().delta;
  saver3ds::save(*s_);   // (en segundo plano: el juego no se para)
  g_->dirty = false;
}

void App::leave_to_menu() {
  if (coop_) coop_leave();
  go(SC_MENU);
}

void App::go(Scene sc) {
  scene = sc;
  cur_ = cur2_ = scroll_ = 0;
  if (sc == SC_MENU) {
    progress_check_ach(*s_, now_ms());
    // Amuletos Iris nuevos (completaste una región jugando: rescates, shinies…).
    int mask = 0;
    for (int gen = 1; gen <= 9; ++gen) if (gacha_has_iris(*s_, gen)) mask |= 1 << gen;
    if (iris_mask_ >= 0)
      for (int gen = 1; gen <= 9; ++gen)
        if ((mask & (1 << gen)) && !(iris_mask_ & (1 << gen))) {
          char t[80];
          std::snprintf(t, sizeof(t), "¡Amuleto Iris de %s!", kBanners[gen - 1].name);
          g_notices.push(t, "Tienes su Pokédex completa: shiny al 2 % en su banner", -1);
        }
    iris_mask_ = mask;
    if (g_->state != ST_UI) { g_->state = ST_UI; backdrop_init(); }
  }
  if (sc == SC_DEX) { gen_ = mon_by_dex(s_->partner) ? (mon_by_dex(s_->partner)->reg > 0 ? kGenForms : mon_by_dex(s_->partner)->gen) : 1; view_shiny_ = false; dex_moves_ = false; }
  // Mejoras: siempre las de tu compañero de ahora (antes se quedaba el último
  // que se miró aquí y, si habías elegido otro, salía el de antes).
  if (sc == SC_UPGRADES) up_dex_ = s_->partner;
}

// Esferas: la siguiente que tengas (dando la vuelta).
void App::orb_next(const Player& me) {
  for (int k = 1; k <= 4; ++k) if (me.orb_id[(orb_sel_ + k) & 3] >= 0) { orb_sel_ = (orb_sel_ + k) & 3; return; }
}

void App::over_tab_step(int d) {
  Game& g = *g_;
  if (over_tab_ < 0 || over_tab_ >= g.nplayers) over_tab_ = g.local;
  for (int k = 0; k < g.nplayers; ++k) {
    over_tab_ = (over_tab_ + d + g.nplayers) % g.nplayers;
    if (g.players[over_tab_].active || over_tab_ == g.local) break;
  }
}
void App::over_tab_next() { over_tab_step(1); }

void App::start_run() {
  over_synced_ = false;
  over_tab_ = -1;
  RunConfig cfg;
  cfg.dex = autotest ? auto_dex_ : s_->partner;
  cfg.mode = uint8_t(autotest ? auto_mode : st_->play_mode);
  // Dificultad (core/diff.h): la elegida en Jugar, si está desbloqueada
  cfg.diff = uint8_t(autotest ? auto_diff : (s_->mode_unlocked(st_->diff) ? st_->diff : 0));
  cfg.shiny = autotest ? auto_shiny : s_->partner_shiny && s_->owns_shiny(cfg.dex);
  for (int k = 0; k < UP_COUNT; ++k) cfg.upgrades[k] = autotest ? 0 : s_->pupg[cfg.dex][k];
  cfg.mega_up = autotest ? uint8_t(auto_mega_up_) : s_->pmega[cfg.dex];
  cfg.four_up = autotest ? true : s_->pfour[cfg.dex] > 0;   // (en las pruebas, con las 4 cartas)
  for (int k = 0; k < 4; ++k) cfg.pbind[k] = autotest ? -1 : s_->pbind[cfg.dex][k];
  for (int k = 0; k < 3; ++k) cfg.pban[k] = autotest ? -1 : s_->pban[cfg.dex][k];
  cfg.pstart = autotest ? int16_t(auto_move_) : s_->pstart[cfg.dex];
  cfg.pmove2 = autotest ? int16_t(-1) : s_->pmove2[cfg.dex];
  if (autotest && auto_move_ >= 0) cfg.upgrades[UP_START] = 1;
  cfg.seed = autotest ? 12345u : uint32_t(osGetTime());
  // Halloween (core/halloween.h): lo que sea al empezar vale toda la partida.
  cfg.hw = hw3ds::on() ? 1 : 0;
  cfg.skin = int8_t(autotest ? (auto_skin ? skin_for_dex(cfg.dex) : -1) : skin_worn(*s_, cfg.dex));
  cfg.aura = autotest ? auto_aura : mastery_has(s_->mastery[cfg.dex], MT_AURA);
  // Naturaleza, habilidad y MT de tu compañero (core/mons.h). Las pruebas, sin ellas
  // (o las que diga el autotest), salvo con la partida de la SD («real»).
  const bool mine = !autotest || auto_real;
  cfg.nature = int8_t(mine ? nature_of(*s_, cfg.dex) : auto_nature);
  cfg.abil = int16_t(mine ? abil_of(*s_, cfg.dex).id : auto_abil);
  cfg.taught = mine ? tm_taught_bits(*s_, cfg.dex) : TmBits{};
  cfg.judged = autotest ? 1 : s_->stats.judged;
  for (int d = 0; d <= kMaxDex; ++d) if (s_->legends[d]) cfg.legends_beaten[d >> 3] |= uint8_t(1 << (d & 7));
  // Desafío semanal (core/weekly.h): su Pokémon tal cual (sin mejoras, ni shiny, ni naturaleza,
  // ni MT, ni disfraz, ni aura), su mapa, en modo normal y dificultad Normal. Gasta el intento.
  if (weekly_run_ || (autotest && auto_weekly > -2)) {
    weekly_run_ = false;
    Weekly w = weekly_now();
    if (autotest && auto_weekly >= 0) { w.cond = int8_t(auto_weekly); if (w.cond == WC_MONO && w.type < 0) w.type = T_FIRE; }
    RunConfig wc;
    wc.dex = w.dex; wc.seed = w.seed; wc.mode = MODE_NORMAL; wc.diff = 0;
    wc.weekly = 1; wc.wk_cond = w.cond; wc.wk_items = w.no_items; wc.wk_type = w.type;
    std::snprintf(wc.wk_week, sizeof(wc.wk_week), "%s", w.week);
    wc.abil = int16_t(abil_of(*s_, w.dex).id);
    wc.hw = cfg.hw;
    wc.judged = cfg.judged;
    std::memcpy(wc.legends_beaten, cfg.legends_beaten, sizeof(wc.legends_beaten));
    cfg = wc;
    if (!autotest) { std::snprintf(s_->wtry, sizeof(s_->wtry), "%s", w.week); save_now(); }
  }
  const u64 t0 = svcGetSystemTick();
  g_->now_ms = now_ms();
  r_->sprites.async = false;   // precarga: todo en el momento
  g_->start_run(cfg, autotest && !auto_real ? nullptr : s_);
  // (sin /run/start: con su id el servidor no paga el reclamo; ver cloud::send_claims)
  g_->debug_immortal = autotest && auto_immortal_;
  if (autotest && auto_power >= 0) { Player& ap = g_->pl(); ap.epower = int8_t(auto_power); ap.power_cd = auto_power_at; }   // (capturas de los poderes)
  g_->debug_win = autotest && auto_win;   // (capturas y pruebas de la victoria: Game::debug_win)
  apply_settings();
  r_->preload_run(*g_);
  r_->sprites.async = true;
  start_ms = uint32_t((svcGetSystemTick() - t0) * 1000ull / SYSCLOCK_ARM11);
  r_->card_cursor = 0;
  bond_first_ = -1;
  scene = SC_RUN;
}

// ---------------- fondo del menú ----------------

void App::backdrop_init() {
  World& w = g_->world;
  // Halloween: el menú de noche, con calabazas y Pokémon Fantasma paseando (game.js de la web).
  menu_hw_ = hw3ds::on();
  w.reset(World::kMenuSeed);
  w.set_halloween(menu_hw_);
  w.set_biome(menu_hw_ ? kBiomeHw + bbiome_ % 3 : bbiome_, true);
  bcam_ = Camera();
  bcam_.x = 0; bcam_.y = 0;
  for (int i = 0; i < 20; ++i) {
    Wanderer& d = wd_[i];
    d.dex = menu_hw_ ? CAST_HW[i] : CAST[i];
    d.anim.init(d.dex, 1);
    d.r = body_radius(d.dex);
    for (int k = 0; k < 20; ++k) {
      d.x = bcam_.x + urng_.range(-1, 1) * bcam_.w * 0.6f;
      d.y = bcam_.y + urng_.range(-1, 1) * bcam_.h * 0.6f;
      if (w.is_free(d.x, d.y, d.r)) break;
    }
    d.vx = d.vy = 0; d.t = 0;
  }
}

void App::backdrop_step(float dt) {
  World& w = g_->world;
  bt_ += dt;
  w.update(dt, bcam_.left(), bcam_.top(), bcam_.w, bcam_.h);
  // Deriva lenta de la cámara.
  bcam_.x += std::cos(bt_ * 0.05f) * 14 * dt;
  bcam_.y += std::sin(bt_ * 0.037f) * 10 * dt;
  bbiome_t_ += dt;
  if (bbiome_t_ > 30) { bbiome_t_ = 0; bbiome_ = (bbiome_ + 1) % 3; w.set_biome(menu_hw_ ? kBiomeHw + bbiome_ : bbiome_); }
  for (Wanderer& d : wd_) {
    d.t -= dt;
    if (d.t <= 0) {
      // Pasea, se para o "ataca" al aire de vez en cuando.
      d.t = urng_.range(1, 3.5f);
      const float r = urng_.next();
      if (r < 0.55f) { const float a = urng_.next() * 6.2832f, s = urng_.range(25, 55); d.vx = std::cos(a) * s; d.vy = std::sin(a) * s; }
      else { d.vx = d.vy = 0; if (r > 0.85f) { static const int8_t A[3] = {A_ATTACK, A_SHOOT, A_CHARGE}; d.anim.play(A[urng_.range_int(0, 2)]); } }
    }
    d.x += d.vx * dt; d.y += d.vy * dt;
    w.collide(d.x, d.y, d.r);
    const bool moving = std::fabs(d.vx) + std::fabs(d.vy) > 1;
    if (moving) d.anim.dir = int8_t(dir_from_angle(std::atan2(d.vy, d.vx)));
    d.anim.loop(moving ? A_WALK : A_IDLE);
    d.anim.update(dt);
    // Si se queda fuera de cámara, reaparece dentro.
    if (!bcam_.sees(d.x, d.y, 120)) {
      for (int k = 0; k < 20; ++k) {
        d.x = bcam_.x + urng_.range(-1, 1) * bcam_.w * 0.7f;
        d.y = bcam_.y + urng_.range(-1, 1) * bcam_.h * 0.7f;
        if (w.is_free(d.x, d.y, d.r)) break;
      }
    }
  }
}

void App::draw_backdrop(float dim, float eye) {
  World& w = g_->world;
  const float vl = bcam_.left(), vt = bcam_.top();
  r_->set_parallax(eye * 3);
  r_->draw_ground(w, bcam_, vl, vt, bt_);
  r_->set_parallax(eye * 1.5f);
  struct It { float y; const Prop* p; const Wanderer* d; int i; };
  static It items[700];
  int n = 0;
  w.for_props_in_rect(vl - 70, vt - 70, vl + bcam_.w + 70, vt + bcam_.h + 70, [&](Prop& p) { if (n < 680) items[n++] = {p.flat ? p.y - 4000 : p.y, &p, nullptr, 0}; });
  for (const Wanderer& d : wd_) items[n++] = {d.y, nullptr, &d, 0};
  for (int k = 0; k < n; ++k) items[k].i = k;
  std::sort(items, items + n, [](const It& a, const It& b) { return a.y < b.y || (a.y == b.y && a.i < b.i); });
  for (int i = 0; i < n; ++i) {
    if (items[i].p) { r_->draw_prop(*items[i].p, w, vl, vt, bt_); continue; }
    const Wanderer& d = *items[i].d;
    const float x = std::floor((d.x - vl) * 0.5f), y = std::floor((d.y - vt) * 0.5f);
    const float sr = shadow_size(d.dex) == 0 ? 5 : shadow_size(d.dex) == 1 ? 8 : 12;
    C2D_DrawEllipseSolid(x - sr, y - sr * 0.42f, 0, sr * 2, sr * 0.84f, C2D_Color32(0, 0, 0, 87));
    r_->draw_sprite(d.anim, x, y, 0, 1);
  }
  r_->set_parallax(0);
  // Halloween: la noche (drawNight de la web): velo oscuro y la luz naranja de las calabazas.
  if (menu_hw_) {
    C2D_DrawRectSolid(0, 0, 0, TW, TH, C2D_Color32(8, 4, 24, 120));
    for (int i = 0; i < n; ++i) {
      const Prop* p = items[i].p;
      if (!p || p->type != PR_PUMPKIN) continue;
      const float x = (p->x - vl) * 0.5f, y = (p->y - 14 - vt) * 0.5f;
      if (x < -60 || x > TW + 60 || y < -60 || y > TH + 60) continue;
      const float f = 1 + std::sin(bt_ * 7 + p->wiggle) * 0.05f + std::sin(bt_ * 13 + p->wiggle * 2) * 0.03f;
      for (int k = 0; k < 4; ++k) {
        const float rr = (16 + k * 9) * f;
        C2D_DrawEllipseSolid(x - rr, y - rr * 0.8f, 0, rr * 2, rr * 1.6f, C2D_Color32(255, 138, 42, uint8_t(26 - k * 5)));
      }
    }
  }
  const u32 e0 = C2D_Color32(0, 0, 0, 0), e1 = C2D_Color32(0, 0, 0, 110);
  C2D_DrawRectangle(0, 0, 0, TW, 36, e1, e1, e0, e0);
  C2D_DrawRectangle(0, TH - 36, 0, TW, 36, e0, e0, e1, e1);
  if (dim > 0) C2D_DrawRectSolid(0, 0, 0, TW, TH, C2D_Color32(8, 12, 20, uint8_t(dim * 255)));
}

// ---------------- utilidades de dibujo ----------------

// mode: 0 normal, 3 silueta (no lo tienes)
void App::draw_mon(int dex, bool shiny, float x, float y, float scale, int mode, int dir) {
  Animator a;
  a.init(dex, scale, shiny);
  a.dir = int8_t(dir);
  // Idle animado con el reloj de la interfaz (o andar, si su reposo es de un fotograma: Animator::start).
  const AnimMeta* m = anim_meta(dex, a.cur, a.shiny);
  if (m) {
    float total = 0;
    for (int i = 0; i < m->n; ++i) total += kAnimDur[m->dur + i];
    float t = std::fmod(t_ * 60, total);
    int f = 0;
    while (f < m->n - 1 && t >= kAnimDur[m->dur + f]) { t -= kAnimDur[m->dur + f]; f++; }
    a.frame = int16_t(f);
  }
  if (mode == 3) {
    SpriteSet* s = r_->sprites.get(dex, false);
    C2D_Image img; int ox, oy, w, h;
    if (m && s && r_->sprites.frame(s, a.cur, std::min<int>(dir, m->r - 1), a.frame, img, ox, oy, w, h)) {
      C2D_ImageTint tint;
      C2D_PlainImageTint(&tint, C2D_Color32(20, 26, 44, 220), 1.0f);
      C2D_DrawImageAt(img, std::floor(x - m->ax * scale + ox * scale), std::floor(y - m->ay * scale + oy * scale), 0, &tint, scale, scale);
    }
    return;
  }
  r_->draw_sprite(a, x, y, mode == 4 ? 3 : 0, 1);   // (4: teñido de morado, el hermano de la Tienda Kecleon)
}

void App::header(const char* title) {
  C2D_DrawRectSolid(0, 0, 0, BW, 22, C2D_Color32(10, 14, 24, 255));
  r_->text(title, 8, 4, 0.42f, C_GOLD);
  char b[64];
  std::snprintf(b, sizeof(b), "%d", int(s_->coins));
  const int ic = r_->icons.find("coin");
  float x = 312 - r_->text_width(b, 0.4f);
  r_->text(b, x, 4, 0.4f, C_TEXT);
  if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), x - 14, 5, 0);
}

bool App::button(float x, float y, float w, float h, const char* label, bool sel, const UiIn* in, u32 col) {
  C2D_DrawRectSolid(x, y, 0, w, h, sel ? C_SEL : C_PANEL);
  if (sel) C2D_DrawRectSolid(x, y, 0, 3, h, col ? col : C_GOLD);
  r_->text(label, x + w / 2, y + h / 2 - 9, 0.5f, C_TEXT, 1);   // (fuente grande, centrada)
  return in_rect(in, x, y, w, h);
}

void App::notices(float dt) {
  if (!g_notices.n) return;
  notice_t_ += dt;
  if (notice_t_ > 2.8f) { notice_t_ = 0; g_notices.pop(); }
}

// ---------------- bucle ----------------


void App::update(const UiIn& in0, float dt) {
  t_ += dt;
  UiIn in = in0;
  // Halloween empieza o acaba con el juego abierto (o llega la hora del servidor): en los
  // menús, el fondo cambia al momento (una partida sigue como empezó: RunConfig::hw).
  if ((hw_check_t_ -= dt) <= 0) {
    hw_check_t_ = 2;
    if (g_->state == ST_UI && hw3ds::on() != menu_hw_) backdrop_init();
  }
  if (update_demo_) {   // autotest "actualizar": la pantalla de Actualizaciones buscando y bajando
    const float t0 = news_t_;
    news_t_ += dt;
    if (t0 < 2 && news_t_ >= 2) prog3ds::start_check(true);
    if (news_t_ > 40) quit = true;
  }
  if (menu_demo_) { news_t_ += dt; if (news_t_ > 60) quit = true; }
  if (shop_demo_) shop_demo_step(dt);
  if (ficha_demo_) ficha_demo_step(dt);
  if (gift_demo_) gift_demo_step(dt);
  if (up_demo_) {   // autotest "mejoras": «Más pasivas a niveles altos», el segundo movimiento y el inicial
    const float t0 = news_t_;
    news_t_ += dt;
    auto at = [&](float t) { return t0 < t && news_t_ >= t; };
    if (at(1)) { go(SC_UPGRADES); cur_ = four_row(); scroll_ = std::max(0, cur_ - 6); }
    if (at(6)) in.down |= KEY_A;   // (la compra)
    if (at(10)) { scene = SC_MOVE2; cur_ = 2; scroll_ = 0; }
    if (at(14)) cur_ = 3;
    if (at(18)) { scene = SC_START; cur_ = 1; scroll_ = 0; }
    if (news_t_ > 23) quit = true;
  }
  if (mode_demo_) {
    const float t0 = news_t_;
    news_t_ += dt;
    if (mode_demo_ == 2 && t0 < 6 && news_t_ >= 6) {   // a la Colección, con el cursor en tu compañero
      go(SC_DEX);
      static int16_t list[256];
      const int n = dex_list(*s_, gen_, filter_, list);
      for (int j = 0; j < n; ++j) if (list[j] == s_->partner) cur_ = j;
      scroll_ = cur_ / 6 > 3 ? cur_ / 6 - 3 : 0;
    }
    if (mode_demo_ == 2 && t0 < 10 && news_t_ >= 10) { gen_ = kGenForms; cur_ = 0; scroll_ = 0; }   // (y las formas regionales)
    // (la dificultad: Normal, luego Difícil y Pesadilla, que está bloqueada si no se ha vencido a Eternatus)
    if (mode_demo_ == 1) st_->diff = news_t_ < 5 ? 0 : news_t_ < 10 ? 1 : 2;
    if (mode_demo_ == 1 && t0 < 5 && news_t_ >= 5 && s_->mode_wins[0] <= 0) s_->mode_wins[0] = 1;   // (Difícil, desbloqueada para la captura)
    if (mode_demo_ == 1 && news_t_ >= 14) cur_ = kModeWeekly;   // (y el desafío semanal)
    if (news_t_ > (mode_demo_ == 1 ? 20 : 14)) quit = true;
    if (scene == SC_MODE || scene == SC_DEX) return;   // (sin tocar nada)
  }
  if (rooms_demo_) {
    const float t0 = news_t_;
    news_t_ += dt;
    if (t0 < 6 && news_t_ >= 6) {
      // (salas de ejemplo: así se ven en la lista)
      static const char* const H[3] = {"Ana", "BETO", "Cris"};
      nrooms_ = 3;
      for (int i = 0; i < 3; ++i) { std::snprintf(rooms_[i].host, sizeof(rooms_[i].host), "%s", H[i]); rooms_[i].players = 1 + i; rooms_[i].index = i; }
      rooms_[0].flags = net3ds::ROOM_LOCKED | net3ds::ROOM_QUICK; rooms_[1].flags = 0; rooms_[2].flags = net3ds::ROOM_QUICK;
      scanned_ = true;
      scene = SC_COOP;
      cur_ = 1;
    }
    if (t0 < 11 && news_t_ >= 11) { coop_online_ = true; nrooms_ = 0; }   // (la pestaña «Por internet», sin el servidor a la vista)
    if (t0 < 16 && news_t_ >= 16) {   // (la sala ya creada, en modo rápido y con contraseña)
      nlobby_ = 3;
      for (int i = 0; i < 3; ++i) { lobby_[i].node = uint16_t(i + 1); lobby_[i].m = my_member(); }
      lobby_[1].m.dex = 94; lobby_[2].m.dex = 448;
      std::snprintf(lobby_[1].m.name, 16, "BETO"); std::snprintf(lobby_[2].m.name, 16, "Cris");
      room_flags_ = net3ds::ROOM_QUICK | net3ds::ROOM_LOCKED;
      scene = SC_LOBBY;
    }
    if (t0 < 19 && news_t_ >= 19) other_ver_t_ = 99;   // (y el aviso de otra versión en la sala)
    if (news_t_ > 24) quit = true;
    return;   // (sin red ni entrada: sólo se dibuja)
  }
  if (gacha_demo_) {
    const float t0 = news_t_;
    news_t_ += dt;
    if (scene == SC_GACHA && t0 < 8 && news_t_ >= 8) { cur_ = 2; in.down |= KEY_A; }        // ×100 (100 tickets; antes, 8 s del banner)
    if (scene == SC_PULL && pull_done_ && news_t_ > 30 && int(news_t_ / 2) != int(t0 / 2)) in.rep |= KEY_DDOWN;   // (más cartas)
    if (news_t_ > 40) quit = true;
  }
  if (news_demo_) {   // autotest "novedades": baja por la lista, va al menú y sale
    news_t_ += dt;
    if (scene == SC_NEWS && news_t_ > 5 && int(news_t_ / 0.7f) != int((news_t_ - dt) / 0.7f)) in.rep |= KEY_DDOWN;
    if (scene == SC_NEWS && news_t_ > 12) go(SC_MENU);
    if (news_t_ > 18) quit = true;
  }
  if (tour) {
    // Cada 3 s, el paso siguiente del recorrido (como si se pulsara).
    tour_t_ += dt;
    if (tour_t_ >= 3) {
      tour_t_ = 0;
      const int i = tour_i_++;
      static bool pulled = false;
      static int bought = 0;
      static bool shinied = false;
      static const Scene ORDER[] = {SC_NEWS, SC_DEX, SC_UPGRADES, SC_START, SC_MOVE2, SC_GACHA, SC_GOALS, SC_GOALS, SC_GOALS, SC_SETTINGS, SC_UPDATE, SC_CREDITS, SC_COOP, SC_COOP, SC_COOP};
      if (scene == SC_TEST) in.down |= KEY_A;                              // responde
      else if (scene == SC_RESULT && i < 10) { cur_ = 1; in.down |= KEY_A; } // "elegir otro"
      else if (scene == SC_STARTERS) in.down |= KEY_B;
      else if (scene == SC_RESULT) { cur_ = 0; in.down |= KEY_A; }        // acepta
      else if (scene == SC_GACHA && !pulled) { pulled = true; cur_ = 0; in.down |= KEY_A; }
      else if (scene == SC_UPGRADES && bought < 3) { bought++; in.down |= KEY_A; }   // compra mejoras (el tirón al guardar)
      else if (scene == SC_DEX && !shinied) { shinied = true; cur_ = 1; in.down |= KEY_Y; }   // la lista en shiny (Y)
      else if (scene == SC_DEX && dexmv_ < 3) {   // sus ataques (SELECT) y dos a la derecha
        if (dexmv_ == 0) {
          static int16_t list[256];
          const int n = dex_list(*s_, gen_, filter_, list);
          for (int j = 0; j < n; ++j) if (list[j] == s_->partner) cur_ = j;
          in.down |= KEY_SELECT;
        } else { in.rep |= KEY_DRIGHT; in.down |= KEY_DRIGHT; }
        dexmv_++;
      }
      else if (scene == SC_PULL) { if (pull_done_) in.down |= KEY_A; else tour_i_--; }
      else {
        static int k = -1;
        if (scene == SC_GOALS && k >= 4 && k <= 5) { in.down |= KEY_R; k++; }
        else if (++k < int(sizeof(ORDER) / sizeof(ORDER[0]))) {
          if (ORDER[k] != scene) {
            go(ORDER[k]);
            if (ORDER[k] == SC_COOP) { coop_online_ = true; net3ds::set_online(true); coop_fails_ = 0; coop_retry_ = 0; }   // (por internet)
          }
        }
        else quit = true;
      }
    }
  }
  g_->now_ms = now_ms();
  cloud_tick();
  // (avisos sólo en los menús: nunca en plena partida ni en una sala)
  const bool calm = scene != SC_RUN && scene != SC_COOP && scene != SC_LOBBY && !coop_;
  {   // versión nueva del juego descargada: se avisa una vez
    static bool told = false;
    if (!told && calm && prog3ds::state() == prog3ds::ST_READY) { told = true; g_notices.push("Versión nueva del juego lista", "Se instala al salir (Ajustes → Actualizaciones)", -1); }
  }
  {   // Regalos firmados (core/gift.h): en el menú, los que son para ti, una sola vez.
    std::string gj;
    if (calm && scene == SC_MENU && !autotest && prog3ds::take_gifts(gj)) apply_gifts(gj);
  }
  // Novedades sin ver: se abren solas una vez (antes de preguntar por la cuenta).
  if (scene == SC_MENU && !news_checked_ && !autotest && !tour) {
    news_checked_ = true;
    if (news_unseen()) { open_news(); return; }
  }
  // Al abrir: ¿conectar con pokesurvivor.com? (una vez; no si ya hay cuenta o dijo «Nunca»)
  if (scene == SC_MENU && !cloud_asked_ && !autotest && !tour && !st_->cloud_never && !cloud3ds::account().on) {
    cloud_asked_ = true;
    go(SC_CLOUDASK);
  }
  notices(dt);
  if (scene != SC_RUN) {
    backdrop_step(dt);
    // Sonidos de la interfaz (los mismos de la web).
    if (in.rep & (KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT)) audio::play("select");
    if (in.down & (KEY_A | KEY_TOUCH)) audio::play("click");
    if (in.down & KEY_B) audio::play("back");
    if (in.down & (KEY_L | KEY_R)) audio::play("select");
  }
  // El aviso de que la vez anterior se rompió: se va con cualquier botón.
  if (crash3ds::last()[0] && scene != SC_RUN && (in.down & (KEY_A | KEY_B | KEY_X | KEY_Y | KEY_START))) { crash_seen_ = true; if (scene == SC_MENU) return; }
  switch (scene) {
    case SC_RUN: in_run(in); break;
    case SC_TEST: case SC_RESULT: case SC_STARTERS: in_test(in); break;
    case SC_MENU: in_menu(in); break;
    case SC_MODE: in_mode(in); break;
    case SC_SHOP: in_shop(in); break;
    case SC_GIFT: in_gift(in); break;
    case SC_ROOMSET: in_roomset(in); break;
    case SC_DEX: in_dex(in); break;
    case SC_UPGRADES: case SC_BIND: case SC_BAN: case SC_START: case SC_MOVE2: in_upgrades(in); break;
    case SC_GACHA:
      if (auto_press_t_ >= 0 && gacha_net_ == 0 && (auto_press_t_ += dt) > 6) {   // (autotest: como pulsar A)
        auto_press_t_ = 3.5f; cur_ = 0;   // (y otra vez cada 2,5 s: el aviso siempre a la vista en las capturas)
        UiIn fake = in; fake.down = KEY_A;
        in_gacha(fake);
        break;
      }
      in_gacha(in);
      break;
    case SC_PULL: in_pull(in, dt); break;
    case SC_GOALS: in_goals(in); break;
    case SC_SETTINGS: in_settings(in); break;
    case SC_COOP: in_coop(in); break;
    case SC_LOBBY: in_lobby(in, dt); break;
    case SC_ACCOUNT: in_account(in); break;
    case SC_CLOUDASK: in_cloudask(in); break;
    case SC_UPDATE: in_update(in); break;
    case SC_NEWS: in_news(in); break;
    case SC_CREDITS:
      if (in.rep & KEY_DDOWN) scroll_ = std::min(scroll_ + 1, std::max(0, credit_lines_ - 10));
      if (in.rep & KEY_DUP) scroll_ = std::max(0, scroll_ - 1);
      if (in.held & KEY_CPAD_DOWN) scroll_ = std::min(scroll_ + 1, std::max(0, credit_lines_ - 10));
      if (in.down & KEY_B) go(SC_MENU);
      break;
  }
}

// ---------------- partida ----------------

// Al acabar la partida (una vez): se guarda, se apunta lo ganado con su reclamo para
// pokesurvivor.com (sin él, el servidor lo quita al sincronizar) y se sincroniza si hay cuenta.
void App::run_over_once() {
  Game& g = *g_;
  if (g.dirty) save_now();
  if (over_synced_) return;
  over_synced_ = true;
  if (g.save && (!autotest || auto_real)) cloud3ds::queue_claim(run_claim(g));
  cloud_autostart();
}

void App::in_run(const UiIn& in) {
  Game& g = *g_;
  const Player& me = g.me();
  Input gi;
  // Caído en grupo: la cámara pasa a un compañero vivo (L/R cambia, abajo);
  // si cae él también, al siguiente; al levantarte, otra vez a ti.
  if (me.dead && g.nplayers > 1 && g.state != ST_OVER) {
    auto ok = [&](int i) { return i >= 0 && i < g.nplayers && g.players[i].active && (i == g.local || !g.players[i].dead); };
    if (!ok(spect_)) {
      spect_ = g.local;
      for (int i = 0; i < g.nplayers; ++i) if (i != g.local && ok(i)) { spect_ = i; break; }
    }
  } else {
    spect_ = -1;
  }
  // El cursor de las cartas/objeto/vínculo vuelve arriba sólo cuando aparece
  // una elección NUEVA (antes volvía al elegir, y como la pantalla sigue unos
  // fotogramas hasta que la partida usa la elección, se veía saltar arriba).
  {
    uint32_t sig = uint32_t(g.state) * 2654435761u ^ uint32_t(me.choice) * 40503u ^ uint32_t(me.item_next + 7) * 9176u;
    for (int i = 0; i < me.ncards; ++i)
      sig = sig * 31u + uint32_t(me.cards[i].type * 1000 + me.cards[i].move * 10 + me.cards[i].stat + 50);
    if (sig != choice_sig_) {
      choice_sig_ = sig;
      // (el primer objeto: el cursor ya en «cogerlo»)
      r_->card_cursor = g.state == ST_CHOICE && me.choice == CH_ITEM && me.item_cur < 0 ? 1 : 0;
    }
  }
  if (autotest) {
    bot.decide(g, gi, g.local);
    if (auto_level > 0 && !auto_leveled_ && !coop_ && g.state == ST_PLAYING) {   // (tráiler: con nivel y ataques; en grupo lo hace la sala)
      auto_leveled_ = true;
      Player& p = g.players[g.local];
      while (p.level < auto_level) g.pending_levels += player_gain_xp(p, (p.xp_next - p.xp) / std::fmax(0.01f, p.xp_mul) + 0.01f);
    }
    if (auto_mega && g.time - auto_warp_ > auto_mega_at && g.state == ST_PLAYING && g.players[g.local].item < 0) {
      player_equip(g, g.players[g.local], auto_stone ? IT_MEGA_B : IT_MEGA_A);
      auto_stone_t_ = float(g.time);
    }
    // (para grabar) con la piedra puesta, espera antes de pulsar Y: se ve el aviso abajo
    if (gi.cmd == CMD_MEGA && auto_mega_wait > 0 && float(g.time) - auto_stone_t_ < auto_mega_wait) gi.cmd = CMD_NONE;
    if (auto_peace && !coop_) g.enemies.clear();   // (para grabar) tu Pokémon solo, sin enemigos
    // (para grabar) quieto mirando hacia abajo hasta 3 s después de megaevolucionar; luego anda
    if (auto_still && !coop_) {
      const Player& me2 = g.players[g.local];
      if (me2.mega >= 0 && auto_mega_t0_ < 0) auto_mega_t0_ = float(g.time);
      if (auto_mega_t0_ < 0 || float(g.time) - auto_mega_t0_ < 3) { gi.ax = gi.ay = 0; still_now_ = true; }
      else still_now_ = false;
    }
    if (auto_orbs && !auto_orbs_given_ && g.time > 2 && g.state == ST_PLAYING) {
      auto_orbs_given_ = true;
      Player& p = g.players[g.local];
      for (int id : {ORB_LUMINOUS, ORB_HEAL, ORB_SLUMBER, ORB_QUICK, ORB_FOCUS}) orb_add(p, id);
    }
    if (auto_treasure && !auto_treasure_given_ && g.time > 3 && g.state == ST_PLAYING) {   // (el tesoro, para las capturas)
      auto_treasure_given_ = true;
      orb_place_treasure(g, g.players[g.local]);
    }
    if (auto_bond && !coop_) {   // (captura del vínculo con el primero ya elegido)
      Player& p = g.players[g.local];
      if (g.time > 4 && !auto_bond_given_ && g.state == ST_PLAYING) { auto_bond_given_ = true; p.bonds = 1; }
      if (g.state == ST_CHOICE && p.choice == CH_BOND) {
        int8_t loose[4]; int nl, grow;
        player_bond_options(p, loose, nl, grow);
        gi = Input();
        r_->bond_first = nl > 0 ? loose[0] : -1;
        r_->card_cursor = nl > 1 ? 1 : 0;
      }
    }
    if (g.time > 30 && g.time < 30.05) r_->legend_t = 5;   // (para ver la leyenda en las capturas)
    // Con un movimiento elegido: pausa 6 s (de verdad) para ver el resumen.
    if (auto_move_ >= 0 && !auto_paused_ && g.state == ST_PLAYING && g.time > 95) { gi.cmd = CMD_PAUSE; auto_paused_ = true; auto_pause_ = float(osGetTime() % 100000000); }
    else if (auto_pause_ >= 0 && g.state == ST_PAUSED) {
      gi = Input();   // (el bot quitaría la pausa)
      if (float(osGetTime() % 100000000) - auto_pause_ > 6000) { gi.cmd = CMD_PAUSE; auto_pause_ = -1; }
    }
    // (el final de la partida, por el MISMO camino que la del jugador: guardar, reclamo y nube)
    if (g.state == ST_OVER && auto_real) run_over_once();
    if (g.state == ST_OVER && !coop_ && !auto_hold_over) start_run();
  } else if (g.state == ST_PLAYING || (g.state == ST_CHOICE && me.choice == CH_NONE) || (g.state == ST_LEVELUP && me.picked)) {
    // (en grupo: mientras otro elige, tú sigues moviendo el cursor de huecos)
    float ax = std::fabs(in.cx) > 0.12f ? in.cx : 0, ay = std::fabs(in.cy) > 0.12f ? in.cy : 0;
    if (in.held & KEY_DLEFT) ax = -1;
    if (in.held & KEY_DRIGHT) ax = 1;
    if (in.held & KEY_DUP) ay = -1;
    if (in.held & KEY_DDOWN) ay = 1;
    const float m = std::hypot(ax, ay);
    if (m > 1) { ax /= m; ay /= m; }
    gi.ax = ax; gi.ay = ay;
    if (me.dead && g.nplayers > 1) {
      // Caído en grupo: L/R cambia a quién sigue la cámara (los vivos y tú).
      auto ok = [&](int i) { return i >= 0 && i < g.nplayers && g.players[i].active && (i == g.local || !g.players[i].dead); };
      const int dir = (in.down & (KEY_L | KEY_ZL)) ? -1 : (in.down & (KEY_R | KEY_X | KEY_ZR)) ? 1 : 0;
      if (dir)
        for (int k = 1; k <= g.nplayers; ++k) {
          const int i = ((spect_ + dir * k) % g.nplayers + g.nplayers) % g.nplayers;
          if (ok(i)) { spect_ = i; break; }
        }
    } else {
      if (in.down & (KEY_L | KEY_ZL)) gi.cycle = -1;
      if (in.down & (KEY_R | KEY_X | KEY_ZR)) gi.cycle = 1;
      // Y: megaevolucionar (con la megapiedra puesta y sin recarga).
      if ((in.down & KEY_Y) && (mega_ready(me) || power_ready(me))) gi.cmd = CMD_MEGA;   // (o el poder de Eternatus)
      // Esferas (del port: en la web, X usa y Y cambia en el mando): A usa la
      // elegida, B elige la siguiente que tengas; tocar su casilla la usa.
      if (me.orb_id[orb_sel_ & 3] < 0) orb_next(me);
      r_->orb_sel = orb_sel_ & 3;
      if (in.down & KEY_B) { const int was = orb_sel_; orb_next(me); if (orb_sel_ != was) audio::play("hover"); }
      if ((in.down & KEY_A) && gi.cmd == CMD_NONE && me.orb_id[orb_sel_ & 3] >= 0) { gi.cmd = CMD_ORB; gi.arg = int8_t(orb_sel_ & 3); }
      for (int i = 0; i < 4; ++i) {
        float ox, oy;
        Renderer::orb_slot_pos(i, ox, oy);
        if (me.orb_id[i] >= 0 && in_rect(&in, ox - 2, oy - 2, Renderer::kOrbS + 4, Renderer::kOrbS + 4)) { orb_sel_ = i; gi.cmd = CMD_ORB; gi.arg = int8_t(i); }
      }
    }
    // Tocar el minimapa: la leyenda (otra vez: se quita).
    if (in.tap && in.ty >= 44 && in.ty < 44 + Renderer::kMapH) r_->legend_t = r_->legend_t > 0 ? 0 : 5;
    if (in.tap && in.ty >= Renderer::kSlotY && in.ty < Renderer::kSlotY + Renderer::kSlotS) {
      const float S = Renderer::kSlotS, GAP = Renderer::kSlotGap, x0 = (320 - (me.nslots * S + (me.nslots - 1) * GAP)) / 2;
      for (int i = 0; i < me.nslots; ++i) if (in.tx >= x0 + i * (S + GAP) && in.tx < x0 + i * (S + GAP) + S) gi.select = int8_t(i);
    }
    if (g.judg.phase == 1 && (in.down & (KEY_A | KEY_B) || in.tap)) gi.cmd = CMD_SKIP;
    if (in.down & KEY_START) {
      gi.cmd = CMD_PAUSE;
      paused_ = !paused_;
      if (g.save && g.dirty) save_now();
    }
    // En grupo la pausa no para la partida: SELECT en la pausa también abandona.
    if (paused_ && g.nplayers > 1 && (in.down & KEY_SELECT)) gi.cmd = CMD_QUIT;
  } else if (g.state == ST_PAUSED) {
    if (in.down & (KEY_START | KEY_B)) { gi.cmd = CMD_PAUSE; paused_ = false; }
    if (in.down & KEY_SELECT) gi.cmd = CMD_QUIT;
  } else if (g.state == ST_LEVELUP) {
    const int nc = me.ncards > 0 ? me.ncards : 1;
    if (in.rep & KEY_DUP) r_->card_cursor = (r_->card_cursor + nc - 1) % nc;
    if (in.rep & KEY_DDOWN) r_->card_cursor = (r_->card_cursor + 1) % nc;
    int tapped = -1;
    for (int i = 0; i < me.ncards; ++i)
      if (in.ty >= Renderer::card_y(i, me.ncards) && in.ty < Renderer::card_y(i, me.ncards) + Renderer::card_h(me.ncards)) tapped = i;
    if (in.tap && tapped >= 0) { r_->card_cursor = tapped; gi.cmd = CMD_CARD; gi.arg = int8_t(r_->card_cursor); }
    else if (in.down & KEY_A) { gi.cmd = CMD_CARD; gi.arg = int8_t(r_->card_cursor); }
    else if (in.down & KEY_Y) gi.cmd = CMD_REROLL;
  } else if (g.state == ST_CHOICE && me.choice == CH_ITEM) {
    if (in.rep & (KEY_DUP | KEY_DDOWN)) r_->card_cursor ^= 1;
    if (in.tap && in.ty >= 96 && in.ty < 204) { r_->card_cursor = (in.ty - 96) / 54; gi.cmd = CMD_ITEM; gi.arg = r_->card_cursor == 1; }
    else if (in.down & KEY_A) { gi.cmd = CMD_ITEM; gi.arg = r_->card_cursor == 1; }
  } else if (g.state == ST_CHOICE && me.choice == CH_POWER) {
    const int nc = me.npower_opts > 0 ? me.npower_opts : 1;
    if (in.rep & KEY_DUP) r_->card_cursor = (r_->card_cursor + nc - 1) % nc;
    if (in.rep & KEY_DDOWN) r_->card_cursor = (r_->card_cursor + 1) % nc;
    const float ch = Renderer::power_card_h(nc);
    const bool touched = in.tap && in.ty >= Renderer::kEvoTop && in.ty < Renderer::kEvoTop + ch * nc;
    if (touched) r_->card_cursor = clamp(int((in.ty - Renderer::kEvoTop) / ch), 0, nc - 1);
    if (touched || (in.down & KEY_A)) { gi.cmd = CMD_POWER; gi.arg = int8_t(clamp(r_->card_cursor, 0, nc - 1)); }
  } else if (g.state == ST_CHOICE && me.choice == CH_EVO) {
    // ¿Evolucionar? (core/evo.h) Una carta por opción y la última, mantener su aspecto.
    const int nc = me.evo_nopts + 1;
    if (in.rep & KEY_DUP) r_->card_cursor = (r_->card_cursor + nc - 1) % nc;
    if (in.rep & KEY_DDOWN) r_->card_cursor = (r_->card_cursor + 1) % nc;
    const float ch = Renderer::evo_card_h(nc);
    const bool touched = in.tap && in.ty >= Renderer::kEvoTop && in.ty < Renderer::kEvoTop + ch * nc;
    if (touched) r_->card_cursor = clamp(int((in.ty - Renderer::kEvoTop) / ch), 0, nc - 1);
    if (touched || (in.down & KEY_A)) { gi.cmd = CMD_EVO; gi.arg = int8_t(clamp(r_->card_cursor, 0, nc - 1)); }
  } else if (g.state == ST_CHOICE && me.choice == CH_BOND) {
    int8_t loose[4]; int nl, grow;
    const int mode = player_bond_options(me, loose, nl, grow);
    if (nl > 0) {
      if (in.rep & KEY_DUP) r_->card_cursor = (r_->card_cursor + nl - 1) % nl;
      if (in.rep & KEY_DDOWN) r_->card_cursor = (r_->card_cursor + 1) % nl;
      const bool touched = in.tap && in.ty >= 92 && in.ty < 92 + 32 * nl;
      if (touched) r_->card_cursor = (in.ty - 92) / 32;
      if ((in.down & KEY_A) || touched) {
        const int pick = loose[clamp(r_->card_cursor, 0, nl - 1)];
        if (mode == 1) { gi.cmd = CMD_BOND; gi.arg = 1; gi.moves[0] = int8_t(pick); bond_first_ = -1; }
        else if (bond_first_ < 0) bond_first_ = pick;
        else if (pick == bond_first_) bond_first_ = -1;   // (otra vez en el marcado: se quita)
        else { gi.cmd = CMD_BOND; gi.arg = 2; gi.moves[0] = int8_t(bond_first_); gi.moves[1] = int8_t(pick); bond_first_ = -1; }
      } else if (in.down & KEY_B) bond_first_ = -1;   // (B: quitar el primero)
    }
    bool still = false;   // (si ese movimiento ya no está suelto, fuera la marca)
    for (int i = 0; i < nl; ++i) if (loose[i] == bond_first_) still = true;
    if (!still) bond_first_ = -1;
    r_->bond_first = bond_first_;
  } else if (g.state == ST_OVER) {
    paused_ = false;
    // Resumen de arriba: en grupo, L/R cambia de jugador.
    if (over_tab_ < 0 || over_tab_ >= g.nplayers) over_tab_ = g.local;
    if (g.nplayers > 1 && (in.down & (KEY_L | KEY_R))) { over_tab_step((in.down & KEY_R) ? 1 : -1); audio::play("hover"); }
    run_over_once();
    if (coop_) {
      // En grupo: un momento para que los demás reciban lo último; luego, al menú.
      if (coop_over_t_ > 1.5f && (in.down & (KEY_A | KEY_B))) { coop_leave(); go(SC_MENU); }
    } else {
      if (in.down & KEY_A) start_run();
      if (in.down & KEY_B) go(SC_MENU);
    }
  }
  // (el paso de la partida lo da main con el reloj; aquí sólo la entrada.
  // Las órdenes y la elección de hueco esperan a que un paso las use.)
  const Input prev = pending_input;
  pending_input = gi;
  if (gi.cmd == CMD_NONE && prev.cmd != CMD_NONE) {
    pending_input.cmd = prev.cmd; pending_input.arg = prev.arg;
    for (int k = 0; k < 4; ++k) pending_input.moves[k] = prev.moves[k];
  }
  if (gi.select < 0) pending_input.select = prev.select;
  if (!gi.cycle) pending_input.cycle = prev.cycle;
}

// ---------------- test de personalidad y compañero ----------------

void App::in_test(const UiIn& in) {
  if (scene == SC_TEST) {
    const TestQuestion& q = kTest[q_];
    if (in.rep & KEY_DUP) cur_ = (cur_ + 3) % 4;
    if (in.rep & KEY_DDOWN) cur_ = (cur_ + 1) % 4;
    int pick = -1;
    for (int i = 0; i < 4; ++i) if (in_rect(&in, 10, 64 + i * 42, 300, 38)) pick = i;
    if (in.down & KEY_A) pick = cur_;
    if (pick >= 0) {
      const TestAnswer& a = q.a[pick];
      for (int k = 0; k < a.n; ++k) scores_.add(a.p[k].trait, a.p[k].pts);
      q_++;
      cur_ = 0;
      if (q_ >= kTestCount) {
        Rng rr{uint32_t(osGetTime())};
        personality_result(scores_, rr, res_trait_, res_dex_);
        scene = SC_RESULT;
      }
    }
    return;
  }
  auto choose = [&](int dex) {
    s_->starter = int16_t(dex);
    s_->partner = int16_t(dex);
    std::snprintf(s_->trait, sizeof(s_->trait), "%s", kTraits[res_trait_].id);
    for (int i = 0; i < scores_.n; ++i) s_->trait_scores[scores_.order[i]] = scores_.pts[i];
    save_grant_dex(*s_, dex, FROM_STARTER, now_ms());
    if (!s_->created) s_->created = now_ms();
    save_now();
    char b[96];
    std::snprintf(b, sizeof(b), "¡%s se une a ti!", mon_by_dex(dex)->name);
    g_notices.push(b, "", -1);
    go(SC_MENU);
  };
  if (scene == SC_RESULT) {
    if (in.rep & (KEY_DUP | KEY_DDOWN)) cur_ ^= 1;
    if (in_rect(&in, 20, 120, 280, 40) || ((in.down & KEY_A) && cur_ == 0)) { choose(res_dex_); return; }
    if (in_rect(&in, 20, 170, 280, 40) || ((in.down & KEY_A) && cur_ == 1)) { scene = SC_STARTERS; cur_ = 0; for (int i = 0; i < kStarterCount; ++i) if (kStarters[i].dex == res_dex_) cur_ = i; }
    return;
  }
  // SC_STARTERS: rejilla de 6 columnas.
  const int cols = 6;
  if (in.rep & KEY_DLEFT) cur_ = std::max(0, cur_ - 1);
  if (in.rep & KEY_DRIGHT) cur_ = std::min(kStarterCount - 1, cur_ + 1);
  if (in.rep & KEY_DUP) cur_ = std::max(0, cur_ - cols);
  if (in.rep & KEY_DDOWN) cur_ = std::min(kStarterCount - 1, cur_ + cols);
  for (int i = 0; i < kStarterCount; ++i) {
    const float x = 4 + (i % cols) * 52, y = 26 + (i / cols) * 38;
    if (in_rect(&in, x, y, 50, 36)) { if (cur_ == i) { choose(kStarters[i].dex); return; } cur_ = i; }
  }
  if (in.down & KEY_A) { choose(kStarters[cur_].dex); return; }
  if (in.down & KEY_B) { scene = SC_RESULT; cur_ = 1; }
}

void App::bottom_test() {
  if (scene == SC_TEST) {
    char b[64];
    std::snprintf(b, sizeof(b), "Pregunta %d de %d", q_ + 1, kTestCount);
    header(b);
    C2D_DrawRectSolid(0, 22, 0, BW * q_ / kTestCount, 3, C_GOLD);
    for (int i = 0; i < 4; ++i) {
      const float y = 64 + i * 42;
      C2D_DrawRectSolid(10, y, 0, 300, 38, i == cur_ ? C_SEL : C_PANEL);
      if (i == cur_) C2D_DrawRectSolid(10, y, 0, 3, 38, C_GOLD);
      // Centrada si cabe en una línea; si no, en dos.
      const char* t = kTest[q_].a[i].text;
      if (r_->text_width(t, 0.4f) <= 290) r_->text(t, 160, y + 11, 0.4f, C_TEXT, 1);
      else r_->text_wrap(t, 160, y + 5, 0.4f, C_TEXT, 290, 1);
    }
    return;
  }
  if (scene == SC_RESULT) {
    header("Tu compañero");
    const Mon* m = mon_by_dex(res_dex_);
    char b[96];
    std::snprintf(b, sizeof(b), "¡%s quiere ser tu compañero!", m->name);
    r_->text_wrap(b, 160, 40, 0.48f, C_TEXT, 300, 1);
    button(20, 120, 280, 40, "¡Vamos juntos!", cur_ == 0);
    button(20, 170, 280, 40, "Elegir otro compañero", cur_ == 1);
    return;
  }
  header("Elige tu compañero");
  for (int i = 0; i < kStarterCount; ++i) {
    const float x = 4 + (i % 6) * 52, y = 26 + (i / 6) * 38;
    C2D_DrawRectSolid(x, y, 0, 50, 36, i == cur_ ? C_SEL : C_PANEL);
    draw_mon(kStarters[i].dex, false, x + 25, y + 31, 1);
  }
  r_->text("A: elegir · B: volver", 160, 226, 0.34f, C_MUTED, 1);
}

// ---------------- menú principal ----------------

// Botones del menú: 3 × 3.
static float menu_x(int i) { return 8 + (i % 3) * 104.0f; }
static float menu_y(int i) { return 66 + (i / 3) * 52.0f; }

// Con la cuenta de pokesurvivor.com, el gacha sólo con conexión: se comprueba al entrar.
// (force: el autotest «gachared» también la comprueba)
void App::open_gacha(bool force) {
  gacha_net_ = cloud3ds::account().on && (!autotest || force) ? 1 : 0;
  gacha_net_t_ = 0;
  go(SC_GACHA);
}

void App::in_menu(const UiIn& in) {
  const int n = 9;
  if (in.rep & KEY_DUP) cur_ = (cur_ + n - 3) % n;
  if (in.rep & KEY_DDOWN) cur_ = (cur_ + 3) % n;
  if (in.rep & KEY_DLEFT) cur_ = cur_ / 3 * 3 + (cur_ % 3 + 2) % 3;
  if (in.rep & KEY_DRIGHT) cur_ = cur_ / 3 * 3 + (cur_ % 3 + 1) % 3;
  int pick = -1;
  for (int i = 0; i < n; ++i) if (in_rect(&in, menu_x(i), menu_y(i), 96, 46)) pick = i;
  if (in.down & KEY_A) pick = cur_;
  // Y: alterna normal / shiny del compañero (si tienes su shiny).
  if ((in.down & KEY_Y) && s_->owns_shiny(s_->partner)) { s_->partner_shiny = !s_->partner_shiny; save_now(); }
  // X: la cuenta de pokesurvivor.com (sincronizar, entrar, salir).
  if (in.down & KEY_X) { account_back_ = SC_MENU; go(SC_ACCOUNT); return; }
  if (in.down & KEY_SELECT) { audio::play("select"); open_news(); return; }
  // La barra de la versión nueva (o START): a Actualizaciones.
  if ((in.down & KEY_START) || (update_bar() && in_rect(&in, 6, 40, 308, 20))) { audio::play("select"); update_back_ = SC_MENU; go(SC_UPDATE); return; }
  // R: la Tienda Kecleon (arriba a la derecha).
  if (in.down & KEY_R) { audio::play("select"); open_shop(); return; }
  if (pick < 0) return;
  cur_ = pick;
  switch (pick) {
    case 0: go(SC_MODE); cur_ = st_->play_mode; break;   // (normal o rápido)
    case 1: cur_ = 0; coop_msg_[0] = 0; coop_fails_ = 0; coop_err_init_ = false; coop_retry_ = 0; go(SC_COOP); break;
    case 2: go(SC_DEX); break;
    case 3: go(SC_UPGRADES); break;
    case 4:
      // Halloween: la primera vez, en la Noche Espectral (como la web).
      if (hw3ds::on() && !spooky_seen_) { spooky_seen_ = true; bgen_ = kGachaSpooky; }
      open_gacha(false);
      break;
    case 5: go(SC_GOALS); break;
    case 6: go(SC_SETTINGS); break;
    case 7: go(SC_CREDITS); break;
    case 8: quit = true; break;
  }
}

// ---------------- Jugar: normal o rápido ----------------

namespace {
struct ModeInfo { const char* title; const char* sub; const char* text; };
const ModeInfo kModes[MODE_COUNT] = {
    {"Normal", "La Sentencia a los 30 minutos",
     "La partida de siempre: aguanta la horda, sube de nivel, cambia de mapa cada 4 minutos y, a los 30, Arceus desciende."},
    {"Rápido", "La Sentencia a los 20 minutos",
     "Arceus llega a los 20 minutos y aquí SÍ se le puede derrotar: tumba a sus 6 legendarios y bajará la guardia. Difícil: ¡medalla si ganas!"},
};
}  // namespace

Weekly App::weekly_now() const {
  char day[16], week[16];
  period_ids(now_ms(), day, sizeof(day), week, sizeof(week));
  return weekly_of(week);
}


void App::in_mode(const UiIn& in) {
  if (in.rep & KEY_DUP) cur_ = (cur_ + kModeWeekly) % (kModeWeekly + 1);
  if (in.rep & KEY_DDOWN) cur_ = (cur_ + 1) % (kModeWeekly + 1);
  cur_ = clamp(cur_, 0, int(kModeWeekly));
  // Dificultad: izquierda / derecha (o L / R, o tocar las flechas); las bloqueadas no se eligen.
  int dd = 0;
  if (in.rep & (KEY_DLEFT | KEY_L)) dd = -1;
  if (in.rep & (KEY_DRIGHT | KEY_R)) dd = 1;
  if (in_rect(&in, 12, 196, 60, 22)) dd = -1;
  if (in_rect(&in, 248, 196, 60, 22)) dd = 1;
  if (dd) {
    const int nd = clamp(st_->diff + dd, 0, DF_COUNT - 1);
    if (nd != st_->diff) { st_->diff = nd; settings_write(dir_, *st_); audio::play("select"); }
  }
  int pick = -1;
  for (int i = 0; i <= kModeWeekly; ++i) if (in_rect(&in, 12, 28 + i * 56, 296, 52)) pick = i;
  if (in.down & KEY_A) pick = cur_;
  if (in.down & KEY_B) { go(SC_MENU); return; }
  if (pick < 0) return;
  cur_ = pick;
  if (pick == kModeWeekly) {   // el desafío: un intento por semana
    const Weekly w = weekly_now();
    if (!std::strcmp(s_->wtry, w.week)) { audio::play("error"); return; }
    weekly_run_ = true;
    start_run();
    return;
  }
  if (!s_->mode_unlocked(st_->diff)) { audio::play("error"); return; }
  if (st_->play_mode != pick) { st_->play_mode = pick; settings_write(dir_, *st_); }
  start_run();
}

void App::bottom_mode() {
  header("Jugar");
  for (int i = 0; i < MODE_COUNT; ++i) {
    const float y = 28 + i * 56;
    const bool sel = i == cur_;
    C2D_DrawRectSolid(12, y, 0, 296, 52, sel ? C_SEL : C_PANEL);
    if (sel) C2D_DrawRectSolid(12, y, 0, 3, 52, C_GOLD);
    r_->text(kModes[i].title, 24, y + 2, 0.55f, sel ? C_GOLD : C_TEXT);
    r_->text_fit(kModes[i].sub, 300, y + 7, 170, 0.34f, i == MODE_QUICK ? C2D_Color32(0xff, 0x9a, 0x4d, 255) : C_MUTED, false, 2);
    r_->text_wrap(kModes[i].text, 24, y + 24, 0.3f, C_TEXT, 278);
  }
  // El desafío semanal: su condición y si te queda el intento de esta semana
  {
    const Weekly w = weekly_now();
    const float y = 28 + kModeWeekly * 56;
    const bool sel = cur_ == kModeWeekly, used = !std::strcmp(s_->wtry, w.week);
    const u32 vio = C2D_Color32(0xc4, 0x7b, 0xff, 255);
    C2D_DrawRectSolid(12, y, 0, 296, 52, sel ? C2D_Color32(52, 40, 92, 255) : C2D_Color32(34, 28, 60, 255));
    C2D_DrawRectSolid(12, y, 0, sel ? 3 : 2, 52, vio);
    r_->text(tr("Desafío semanal"), 24, y + 2, 0.55f, sel ? C2D_Color32(0xe2, 0xc4, 0xff, 255) : C_TEXT);
    r_->text_fit(used ? tr("Ya lo has jugado esta semana") : tr("1 intento esta semana"), 300, y + 7, 130, 0.34f,
                 used ? C_MUTED : C2D_Color32(0x5f, 0xe0, 0x8a, 255), false, 2);
    char lb[96], b[160];
    weekly_label(w, lb, sizeof(lb));
    const Mon* wm = mon_by_dex(w.dex);
    std::snprintf(b, sizeof(b), "%s · %s", wm ? tr(wm->name) : "?", tr(lb));
    r_->text_fit(b, 24, y + 24, 278, 0.36f, C_GOLD, false, 0);
    r_->text_fit(tr("Todos con el mismo Pokémon y mapa · aguanta 3 min: ticket ×10"), 24, y + 38, 278, 0.3f, C_MUTED, false, 0);
  }
  if (cur_ == kModeWeekly) {
    C2D_DrawRectSolid(12, 196, 0, 296, 24, C_PANEL);
    r_->text_fit(tr("El desafío se juega siempre en Normal"), 160, 202, 280, 0.36f, C_MUTED, false, 1);
    r_->text(tr("A: jugar · B: volver"), 160, 226, 0.34f, C_MUTED, 1);
    return;
  }
  // Dificultad (para los dos modos): < Normal / Difícil / Pesadilla >
  {
    const int d = clamp(st_->diff, 0, DF_COUNT - 1);
    const bool open = s_->mode_unlocked(d);
    const u32 col = rgb(kDiffColor[d]);
    C2D_DrawRectSolid(12, 196, 0, 296, 24, C_PANEL);
    C2D_DrawRectSolid(12, 196, 0, 296, 2, col);
    r_->text(d > 0 ? "<" : " ", 30, 199, 0.55f, C_TEXT, 1);
    r_->text(d < DF_COUNT - 1 ? ">" : " ", 290, 199, 0.55f, C_TEXT, 1);
    char b[96];
    if (open) {
      if (d == 0) std::snprintf(b, sizeof(b), "%s", tr("Normal"));
      else std::snprintf(b, sizeof(b), tr("%s · ×%.1f monedas y tickets"), tr(kDiffName[d]), double(kDiffModes[d].reward));
      r_->text_fit(b, 160, 200, 230, 0.42f, col, true, 1);
    } else {
      std::snprintf(b, sizeof(b), tr("Vence a Eternatus en %s para desbloquearlo"), tr(kDiffName[d - 1]));
      r_->text_fit(b, 160, 202, 230, 0.34f, C_MUTED, false, 1);
    }
  }
  r_->text(tr("A: jugar · < >: dificultad · B: volver"), 160, 226, 0.34f, C_MUTED, 1);
}

void App::bottom_menu() {
  // Fondo: degradado de azul noche con un toque del color de tu compañero
  const Mon* pm = mon_by_dex(s_->partner);
  const uint32_t tcol = pm && pm->types[0] >= 0 ? kTypeColor[pm->types[0]] : 0x4fd1ff;
  C2D_DrawRectangle(0, 0, 0, BW, 240, rgb(mix(0x182238, tcol, 0.12f)), rgb(mix(0x182238, tcol, 0.12f)), rgb(0x0a0e18), rgb(0x0a0e18));
  // Cabecera: el nombre del juego y lo que tienes, en cápsulas
  C2D_DrawRectangle(0, 0, 0, BW, 24, C2D_Color32(8, 12, 22, 250), C2D_Color32(8, 12, 22, 250), C2D_Color32(8, 12, 22, 200), C2D_Color32(8, 12, 22, 200));
  C2D_DrawRectSolid(0, 24, 0, BW, 1, rgbf(0xffd23f, 0.35f));
  r_->text("PokéSurvivor", 8, 4, 0.48f, C_GOLD);
  char b[128];
  auto pill = [&](float rx, const char* icon, const char* txt, u32 col) -> float {   // (de derecha a izquierda)
    const float w = r_->text_width(txt, 0.38f) + 24;
    rrect(rx - w, 4, w, 16, C2D_Color32(28, 38, 62, 255));
    rframe(rx - w, 4, w, 16, C2D_Color32(58, 76, 112, 255));
    const int ic = icon ? r_->icons.find(icon) : -1;
    if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), rx - w + 4, 6, 0);
    r_->text(txt, rx - 4, 4, 0.38f, col, 2);
    return rx - w - 4;
  };
  float rx = BW - 4;
  std::snprintf(b, sizeof(b), "%d", int(s_->coins)); rx = pill(rx, "coin", b, C_TEXT);
  std::snprintf(b, sizeof(b), "%d", int(s_->t10)); rx = pill(rx, "ticket10", b, C_TEXT);
  std::snprintf(b, sizeof(b), "%d", int(s_->t1)); rx = pill(rx, "ticket", b, C_TEXT);
  // Halloween: los caramelos (sólo durante el evento; fuera de él no se ven, aunque
  // te queden: el dueño, 07-11-2026).
  if (hw3ds::on()) { std::snprintf(b, sizeof(b), "%d", int(s_->candy)); rx = pill(rx, "candy", b, C2D_Color32(255, 138, 42, 255)); }
  if (update_bar()) {
    // La versión nueva: buscando, bajando (con su barra) o lista para instalar.
    const int st = prog3ds::state();
    const bool ready = st == prog3ds::ST_READY;
    const float k = ready ? 0.6f + std::sin(t_ * 4) * 0.4f : 1;
    rrect(6, 36, 308, 22, ready ? rgbf(0x1f5a32, 0.95f) : C2D_Color32(24, 34, 60, 240));
    C2D_DrawRectSolid(7, 37, 0, 3, 20, ready ? rgbf(0x5fe08a, k) : C_GOLD);
    const UpdateEntry& e = prog3ds::latest();
    if (st == prog3ds::ST_DOWNLOADING) {
      const float p = prog3ds::pct() / 100.0f;
      std::snprintf(b, sizeof(b), "Bajando %s · %d %%", e.version.c_str(), prog3ds::pct());
      r_->text_fit(b, 14, 39, 204, 0.4f, C_TEXT, false, 0);
      C2D_DrawRectSolid(222, 44, 0, 84, 7, C2D_Color32(0, 0, 0, 160));
      C2D_DrawRectSolid(222, 44, 0, 84 * p, 7, C_GOLD);
    } else if (ready) {
      if (prog3ds::pending_back()) std::snprintf(b, sizeof(b), "Anterior lista: al salir");
      else std::snprintf(b, sizeof(b), "¡%s lista! Al salir", e.version.c_str());
      r_->text_fit(b, 14, 39, 236, 0.4f, rgbf(0xb8ffcf, 1), false, 0);
      r_->text("START", 306, 41, 0.32f, rgbf(0x5fe08a, k), 2);
    } else {
      r_->text("Buscando versión nueva…", 160, 39, 0.4f, C_MUTED, 1);
    }
  } else {
    // Tus números: partidas, mejor tiempo, mejor nivel, Pokémon (y medallas de Arceus)
    char tt[16]; mmss(float(s_->stats.best_time), tt, sizeof(tt));
    const int medals = s_->arceus_medals();
    struct St { const char* icon; char v[24]; const char* lbl; } st[4];
    std::snprintf(st[0].v, sizeof(st[0].v), "%d", int(s_->stats.runs)); st[0].icon = "flag"; st[0].lbl = "Runs";
    std::snprintf(st[1].v, sizeof(st[1].v), "%s", tt); st[1].icon = "clock"; st[1].lbl = "Mejor tiempo";
    std::snprintf(st[2].v, sizeof(st[2].v), "%d", int(s_->stats.best_level)); st[2].icon = "up"; st[2].lbl = "Mejor nivel";
    std::snprintf(st[3].v, sizeof(st[3].v), "%d/%d", s_->owned_count(), kDexCount); st[3].icon = "ball"; st[3].lbl = "Pokémon";
    for (int k = 0; k < 4; ++k) {
      const float x = 8 + k * 77, y = 31, w = 73;
      rgrad(x, y, w, 28, C2D_Color32(30, 40, 66, 235), C2D_Color32(18, 24, 42, 235));
      rframe(x, y, w, 28, C2D_Color32(52, 68, 102, 255));
      const int ic = r_->icons.find(st[k].icon);
      if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), x + 4, y + 7, 0, nullptr, 1.15f, 1.15f);
      r_->text_fit(st[k].v, x + 24, y + 1, w - 27, 0.44f, C_TEXT, false, 0);
      r_->text_fit(tr(st[k].lbl), x + 24, y + 16, w - 27, 0.28f, C_MUTED, false, 0);
    }
    if (medals > 0) {   // (las medallas de Arceus, en la esquina de «Runs»)
      r_->draw_medal(8 + 73 - 8, 31 + 8, 4.5f);
      std::snprintf(b, sizeof(b), "×%d", medals);
      r_->text(b, 8 + 73 - 15, 31 + 1, 0.28f, C_GOLD, 2, false);
    }
  }
  // Los botones: tarjetas con su icono y su color (Jugar, siempre destacado)
  const int pend = progress_pending(*s_, now_ms());
  static const char* const L[9] = {"Jugar", "En grupo", "Pokémon", "Mejoras", "Gacha", "Misiones", "Ajustes", "Créditos", "Salir"};
  static const char* const IC[9] = {"ball", "friends", "ball2", "up", "ticket10", "trophy", "gear", "heart", nullptr};
  static const uint32_t AC[9] = {0x5fe08a, 0x5fc8ff, 0xff6b6b, 0xffcb3d, 0xc47bff, 0xff9a4d, 0x9fb0cc, 0xff8ad8, 0x8a96b0};
  for (int i = 0; i < 9; ++i) {
    const float x = menu_x(i), y = menu_y(i), w = 96, h = 46;
    const bool sel = i == cur_;
    const uint32_t ac = AC[i];
    if (sel) rrect(x - 3, y - 3, w + 6, h + 6, rgbf(ac, 0.18f + 0.12f * std::sin(t_ * 4)));
    C2D_DrawRectSolid(x - 1, y + 2, 0, w + 2, h, C2D_Color32(0, 0, 0, 110));
    const float ka = i == 0 ? 0.42f : sel ? 0.34f : 0.14f;
    rgrad(x, y, w, h, rgb(mix(0x1c2438, ac, ka)), rgb(mix(0x0e1424, ac, ka * 0.4f)));
    C2D_DrawRectSolid(x + 1, y + 1, 0, w - 2, 1, rgbf(0xffffff, sel ? 0.22f : 0.1f));
    rframe(x, y, w, h, sel ? rgb(0xffd23f) : rgb(mix(0x2a3450, ac, 0.45f)));
    // Icono, sobre un disco de su color
    const float cx = x + w / 2, cy = y + 15;
    C2D_DrawCircleSolid(cx, cy, 0, 11, rgb(mix(0x0c1220, ac, 0.35f)));
    const int ic = IC[i] ? r_->icons.find(IC[i]) : -1;
    if (ic >= 0) {   // (centrado en su disco: los iconos miden 12 px)
      const float iw = r_->icons.sub(ic).width * 1.5f, ih = r_->icons.sub(ic).height * 1.5f;
      C2D_DrawImageAt(r_->icons.image(ic), std::round(cx - iw / 2), std::round(cy - ih / 2), 0, nullptr, 1.5f, 1.5f);
    }
    else {   // (Salir: una puerta con su flecha)
      C2D_DrawRectSolid(cx - 6, cy - 7, 0, 8, 14, rgb(0xb7c4dc));
      C2D_DrawRectSolid(cx - 5, cy - 6, 0, 6, 12, rgb(0x3a4560));
      C2D_DrawRectSolid(cx + 1, cy - 1, 0, 6, 2, rgb(0xeaf2ff));
      C2D_DrawTriangle(cx + 9, cy, rgb(0xeaf2ff), cx + 5, cy - 4, rgb(0xeaf2ff), cx + 5, cy + 4, rgb(0xeaf2ff), 0);
    }
    r_->text_fit(L[i], cx, y + 28, w - 8, 0.44f, sel || i == 0 ? rgb(0xffffff) : C_TEXT, true, 1);
    if (i == 5 && pend) { std::snprintf(b, sizeof(b), "%d", pend); C2D_DrawCircleSolid(x + 86, y + 8, 0, 7, C2D_Color32(255, 95, 109, 255)); r_->text(b, x + 86, y + 1, 0.34f, C_TEXT, 1, false); }
  }
  // Abajo: novedades (con un punto si hay sin ver) y el cambio a shiny.
  {
    const float w = r_->text_width("SELECT: ¡Novedades!", 0.34f) + 12;
    rrect(6, 222, w, 15, C2D_Color32(40, 32, 12, 230));
    rframe(6, 222, w, 15, rgbf(0xffd23f, 0.55f));
    r_->text("SELECT: ¡Novedades!", 12, 223, 0.34f, C_GOLD, 0);
    if (news_unseen()) C2D_DrawCircleSolid(6 + w, 223, 0, 3.5f, C2D_Color32(255, 95, 109, 255));
  }
  if (s_->owns_shiny(s_->partner)) r_->text("Y: normal / shiny", 312, 224, 0.32f, C_MUTED, 2);
}

// ¿Se ve la barra de la versión nueva en el menú? (buscando, bajando o lista)
bool App::update_bar() const {
  const int st = prog3ds::state();
  return st == prog3ds::ST_CHECKING || st == prog3ds::ST_DOWNLOADING || st == prog3ds::ST_READY;
}

// ---------------- colección ----------------

// Pokémon de la generación que pasan el filtro (todos, tengo, me faltan, shiny, rareza 5..1).
// Formas regionales (sección de la colección después de las 9 generaciones): nombre y color de cada región
const char* const kRegionName[5] = {"", "Alola", "Galar", "Hisui", "Paldea"};
const uint32_t kRegionColor[5] = {0, 0xff9a4d, 0x7fb8ff, 0xc9a26a, 0xff6b9a};

int dex_list(const Save& s, int gen, int filter, int16_t* out) {
  int n = 0;
  for (int i = 0; i < kDexCount; ++i) {
    const Mon& p = kDex[i];
    // (las formas regionales, en su sección: la última, después de las 9 generaciones)
    if (gen == App::kGenForms ? p.reg <= 0 : (p.gen != gen || p.reg > 0)) continue;
    const bool own = s.owns(p.dex), sh = s.owns_shiny(p.dex);
    if (filter == 1 && !own) continue;
    if (filter == 2 && own) continue;
    if (filter == 3 && !sh) continue;
    if (filter >= 4 && p.rarity != 9 - filter) continue;
    out[n++] = p.dex;
  }
  // En orden de la Pokédex nacional (las formas regionales: por región, Alola, Galar, Hisui y Paldea)
  std::stable_sort(out, out + n, [](int16_t a, int16_t b) {
    const Mon* x = mon_by_dex(a); const Mon* y = mon_by_dex(b);
    const int rx = x ? x->reg : 0, ry = y ? y->reg : 0;
    if (rx != ry) return rx < ry;
    const int nx = x && x->num > 0 ? x->num : a, ny = y && y->num > 0 ? y->num : b;
    return nx != ny ? nx < ny : a < b;
  });
  return n;
}
namespace {
const char* const FILTERS[9] = {"Todos", "Tengo", "Me faltan", "Shiny", "Legendaria", "Épica", "Rara", "Poco común", "Común"};
}  // namespace

void App::in_dex(const UiIn& in) {
  static int16_t list[256];
  const int n = dex_list(*s_, gen_, filter_, list);
  if (dex_moves_) {
    if (n && s_->owns(list[std::min(cur_, n - 1)])) { in_ficha(in, list[std::min(cur_, n - 1)]); return; }
    dex_moves_ = false;
  }
  // SELECT: su ficha (ataques, naturaleza, habilidad y MT: app_ficha.cpp).
  if (n && (in.down & KEY_SELECT) && s_->owns(list[std::min(cur_, n - 1)])) {
    dex_moves_ = true; mv_cur_ = 0; ficha_tab_ = FT_MOVES; fi_cur_ = 0; fi_scroll_ = 0;
    audio::play("confirm");
    return;
  }
  const int cols = 6;
  if (in.down & KEY_L) { gen_ = gen_ == 1 ? kGenForms : gen_ - 1; cur_ = 0; scroll_ = 0; }
  if (in.down & KEY_R) { gen_ = gen_ == kGenForms ? 1 : gen_ + 1; cur_ = 0; scroll_ = 0; }
  if (in.down & KEY_START) { filter_ = (filter_ + 1) % 9; cur_ = 0; scroll_ = 0; }
  if (in.rep & KEY_DLEFT) cur_ = std::max(0, cur_ - 1);
  if (in.rep & KEY_DRIGHT) cur_ = std::min(n - 1, cur_ + 1);
  if (in.rep & KEY_DUP) cur_ = std::max(0, cur_ - cols);
  if (in.rep & KEY_DDOWN) cur_ = std::min(n - 1, cur_ + cols);
  if (cur_ < 0) cur_ = 0;
  const int row = cur_ / cols;
  if (row < scroll_) scroll_ = row;
  if (row >= scroll_ + 4) scroll_ = row - 3;
  for (int i = 0; i < 24; ++i) {
    const int k = scroll_ * cols + i;
    if (k >= n) break;
    if (in_rect(&in, 4 + (i % cols) * 52, 42 + (i / cols) * 44, 50, 42)) cur_ = k;
  }
  // X: cambia de disfraz (los que hayas comprado para ese Pokémon, y sin ninguno). De
  // momento sólo hay los de bruja de Halloween; con más, se van pasando uno tras otro.
  if (n && (in.down & KEY_X) && s_->owns(list[std::min(cur_, n - 1)])) {
    const int d = list[std::min(cur_, n - 1)];
    int mine[8], nm = 0;
    for (int k = 0; k < kSkinCount && nm < 8; ++k) if (kSkins[k].sell && kSkins[k].dex == d && s_->skin_at[k] > 0) mine[nm++] = k;
    if (nm) {
      const int cur = skin_worn(*s_, d);
      int next = -1;
      if (cur < 0) next = mine[0];
      else for (int i = 0; i < nm; ++i) if (mine[i] == cur) next = i + 1 < nm ? mine[i + 1] : -1;
      skin_wear(*s_, d, next, now_ms());
      audio::play("select");
      char b[96];
      std::snprintf(b, sizeof(b), "%s", next >= 0 ? kSkins[next].name : mon_by_dex(d)->name);
      g_notices.push(b, next >= 0 ? "Disfraz puesto" : "Sin disfraz", -1);
      save_now();
    } else {
      audio::play("error");
      g_notices.push("No tiene disfraces.", "", -1);
    }
  }
  if (n && (in.down & KEY_Y)) {   // (toda la lista en shiny: los que tengas en shiny)
    bool any = false;
    for (int i = 0; i < n && !any; ++i) any = s_->owns_shiny(list[i]);
    if (any) view_shiny_ = !view_shiny_;
  }
  if (n && (in.down & KEY_A) && s_->owns(list[cur_])) {
    s_->partner = list[cur_];
    s_->partner_shiny = view_shiny_ && s_->owns_shiny(list[cur_]);
    save_now();
    char b[96];
    std::snprintf(b, sizeof(b), "%s%s es ahora tu compañero", mon_by_dex(list[cur_])->name, s_->partner_shiny ? " shiny" : "");
    g_notices.push(b, "", -1);
  }
  if (in.down & KEY_B) go(SC_MENU);
}

void App::bottom_dex() {
  static int16_t list[256];
  const int n = dex_list(*s_, gen_, filter_, list);
  if (dex_moves_ && n) { bottom_ficha(list[std::min(cur_, n - 1)]); return; }
  int have = 0, total = 0;
  char b[96];
  if (gen_ == kGenForms) {   // Formas regionales: su cabecera y la leyenda de las regiones
    for (int i = 0; i < kDexCount; ++i) if (kDex[i].reg > 0) { total++; if (s_->owns(kDex[i].dex)) have++; }
    std::snprintf(b, sizeof(b), "%s %d/%d", tr("Formas regionales"), have, total);
    header(b);
    float lx = 160 - 150;
    std::snprintf(b, sizeof(b), "L/R · START: %s ·", tr(FILTERS[filter_]));
    r_->text(b, lx, 25, 0.32f, C_MUTED, 0, false);
    lx += r_->text_width(b, 0.32f) + 6;
    for (int r = 1; r <= 4; ++r) {
      const char* rn = tr(kRegionName[r]);
      C2D_DrawRectSolid(lx, 29, 0, 7, 7, rgb(kRegionColor[r]));
      r_->text(rn, lx + 9, 25, 0.32f, rgb(kRegionColor[r]), 0, false);
      lx += r_->text_width(rn, 0.32f) + 16;
    }
  } else {
  for (int i = 0; i < kDexCount; ++i) if (kDex[i].gen == gen_) { total++; if (s_->owns(kDex[i].dex)) have++; }
  // Con la región completa, su Amuleto Iris (shiny al 2 % en su banner).
  std::snprintf(b, sizeof(b), "%s %d/%d%s", GEN_NAMES[gen_ - 1], have, total, have >= total ? " · Amuleto Iris" : "");
  header(b);
  if (have >= total) std::snprintf(b, sizeof(b), "L/R generación · START: %s · shiny al 2 %% en su banner", tr(FILTERS[filter_]));
  else std::snprintf(b, sizeof(b), "L/R generación · START: %s · Amuleto Iris: faltan %d", tr(FILTERS[filter_]), total - have);
  r_->text_fit(b, 160, 25, 308, 0.34f, C_MUTED, false, 1);
  }
  for (int i = 0; i < 24; ++i) {
    const int k = scroll_ * 6 + i;
    if (k >= n) break;
    const int dex = list[k];
    const float x = 4 + (i % 6) * 52, y = 42 + (i / 6) * 44;
    const bool own = s_->owns(dex);
    C2D_DrawRectSolid(x, y, 0, 50, 42, k == cur_ ? C_SEL : C_PANEL);
    {   // Formas regionales: un toque del color de su región (degradado de abajo y su filo)
      const Mon* rm = mon_by_dex(dex);
      if (rm && rm->reg > 0 && rm->reg < 5) {
        const uint32_t rc = kRegionColor[rm->reg];
        C2D_DrawRectangle(x, y + 22, 0, 50, 20, rgbf(rc, 0), rgbf(rc, 0), rgbf(rc, own ? 0.35f : 0.15f), rgbf(rc, own ? 0.35f : 0.15f));
        C2D_DrawRectSolid(x, y + 40, 0, 50, 2, rgb(rc));
      }
    }
    const bool sh = s_->owns_shiny(dex);
    draw_mon(dex, view_shiny_ && sh, x + 25, y + 36, 1, own ? 0 : 3);
    {   // (su número de la Pokédex nacional: las formas regionales, el de su especie)
      const Mon* lm = mon_by_dex(dex);
      std::snprintf(b, sizeof(b), "%03d", lm && lm->num > 0 ? int(lm->num) : dex);
    }
    r_->text(b, x + 2, y + 1, 0.28f, C2D_Color32(255, 255, 255, 150), 0, false);
    // Tienes el shiny: la estrellita (como en la web). Tu compañero: punto dorado.
    const int si = r_->ui_icon[7];
    if (sh && si >= 0) C2D_DrawImageAt(r_->icons.image(si), x + 39, y + 1, 0, nullptr, 0.8f, 0.8f);
    if (dex == s_->partner) C2D_DrawRectSolid(x + (sh ? 33 : 43), y + 3, 0, 4, 4, C_GOLD);
    if (dex >= 0 && dex <= kMaxDex && s_->arceus_win[dex] > 0) r_->draw_medal(x + 7, y + 34, 4.5f);   // (derrotó a Arceus)
  }
  if (!n) r_->text("Ninguno con este filtro.", 160, 110, 0.4f, C_MUTED, 1);
  r_->text(view_shiny_ ? "A: compañero · Y: normales · SELECT: ficha · B: volver" : "A: compañero · Y: shinys · SELECT: ficha · B: volver", 160, 226, 0.32f, C_MUTED, 1);
}

void App::fit_text(const char* s, float x, float y, float maxw, float size, u32 color, bool shadow) {
  static const float kSizes[] = {0.36f, 0.32f, 0.28f};
  float sz = size;
  for (float k : kSizes) { if (r_->text_width(s, sz) <= maxw || sz <= k) break; sz = k; }
  if (r_->text_width(s, sz) <= maxw) { r_->text(s, x, y + (size - sz) * 18, sz, color, 0, shadow); return; }
  // Ni con la letra pequeña: se corta (por letras enteras de UTF-8) y «…».
  std::string t = tr(s);
  while (!t.empty() && r_->text_width((t + "…").c_str(), sz) > maxw) {
    t.pop_back();
    while (!t.empty() && (uint8_t(t.back()) & 0xC0) == 0x80) t.pop_back();
  }
  r_->text((t + "…").c_str(), x, y + (size - sz) * 18, sz, color, 0, shadow);
}

int App::four_row() const { return UP_COUNT + (has_mega(up_dex_) ? 1 : 0); }   // «Más pasivas a niveles altos» (del port)
int App::atajos_row() const { return four_row() + 1; }

float App::y_hint_w() {
  return 51 + r_->text_width("Normal", 0.38f) + r_->text_width("Shiny", 0.38f);
}

// El botón Y de la consola, verde, que se pulsa solo cada poco, con
// «Normal · Shiny» y el que se ve resaltado.
void App::y_hint(float x, float y, bool shiny) {
  const bool down = std::fmod(t_, 1.4f) < 0.22f;
  const float dy = down ? 2 : 0;
  C2D_DrawCircleSolid(x + 9, y + 11, 0, 9, C2D_Color32(20, 60, 30, 255));                       // sombra
  C2D_DrawCircleSolid(x + 9, y + 9 + dy, 0, 9, down ? C2D_Color32(70, 170, 90, 255) : C2D_Color32(95, 205, 115, 255));
  r_->text("Y", x + 9, y + 2 + dy, 0.42f, C2D_Color32(255, 255, 255, 255), 1, false);
  const u32 on = C_GOLD, off = C2D_Color32(170, 180, 200, 200);
  r_->text("Normal", x + 24, y + 3, 0.38f, shiny ? off : on);
  const float w = r_->text_width("Normal", 0.38f);
  r_->text("·", x + 28 + w, y + 3, 0.38f, off);
  const int si = r_->ui_icon[7];
  if (si >= 0) C2D_DrawImageAt(r_->icons.image(si), x + 37 + w, y + 4, 0, nullptr, 1, 1);
  r_->text("Shiny", x + 51 + w, y + 3, 0.38f, shiny ? C2D_Color32(255, 140, 150, 255) : off);
}

void App::top_detail(int dex, bool shiny, bool own) {
  const Mon* p = mon_by_dex(dex);
  if (!p) return;
  C2D_DrawRectSolid(0, 0, 0, TW, TH, C2D_Color32(8, 12, 20, 170));
  const uint32_t tc = kTypeColor[p->types[0]];
  C2D_DrawEllipseSolid(40, 150, 0, 140, 34, rgbf(own ? (shiny ? 0x9ae6ff : tc) : 0x5b83d6, 0.35f));
  // (con su disfraz puesto, si lo lleva)
  const int sk = own ? skin_worn(*s_, dex) : -1;
  draw_mon(sk >= 0 ? kSkins[sk].sprite : dex, own && shiny && s_->owns_shiny(dex), 110, 170, 3, own ? 0 : 3);
  if (own && s_->owns_shiny(dex)) y_hint(12, 214, shiny);
  {
    // X: cambiar de disfraz (si tiene alguno comprado).
    bool any = false;
    for (int k = 0; k < kSkinCount; ++k) if (kSkins[k].sell && kSkins[k].dex == dex && s_->skin_at[k] > 0) any = true;
    if (own && any) r_->text(sk >= 0 ? "X: quitar el disfraz" : "X: ponerle el disfraz", 110, 4, 0.34f, C2D_Color32(255, 160, 80, 255), 1);
  }
  char b[128];
  std::snprintf(b, sizeof(b), "Nº %03d · %s", p->num > 0 ? int(p->num) : int(p->dex), GEN_NAMES[p->gen - 1]);
  r_->text(b, 300, 10, 0.38f, C_MUTED, 1);
  r_->text(own ? p->name : "???", 300, 24, 0.66f, C_TEXT, 1);
  std::snprintf(b, sizeof(b), "%s %s", std::string(size_t(p->rarity), '*').c_str(), kRarityName[p->rarity]);
  r_->text(b, 300, 50, 0.38f, rgb(kRarityColor[p->rarity]), 1);
  if (!own) {
    std::snprintf(b, sizeof(b), "Aún no lo tienes. Consíguelo en el banner de %s o derrotándolo cuando salga shiny.", GEN_NAMES[p->gen - 1]);
    r_->text_wrap(b, 300, 90, 0.38f, C_MUTED, 180, 1);
    return;
  }
  // Maestría (core/mastery.h): sus 4 premios y lo que falta para el siguiente
  {
    const int32_t pts = s_->mastery[dex];
    const int lv = mastery_level(pts), nx = mastery_next(pts);
    const float X = 10, Y = 22, W = 180;
    C2D_DrawRectSolid(X, Y, 0, W, 40, C2D_Color32(14, 20, 34, 200));
    C2D_DrawRectSolid(X, Y, 0, 2, 40, lv ? C_GOLD : C2D_Color32(70, 84, 110, 255));
    r_->text(tr("Maestría"), X + 8, Y + 2, 0.36f, lv ? C_GOLD : C_MUTED);
    for (int q = 0; q < MT_COUNT; ++q) {   // (una medalla por premio: llenas las conseguidas)
      const float cx = X + W - 14 - (MT_COUNT - 1 - q) * 15, cy = Y + 9;
      const bool on = q < lv;
      C2D_DrawCircleSolid(cx, cy, 0, 6, on ? C2D_Color32(120, 84, 10, 255) : C2D_Color32(40, 48, 66, 255));
      C2D_DrawCircleSolid(cx, cy, 0, 4.5f, on ? (q == MT_AURA ? C2D_Color32(255, 246, 200, 255) : C_GOLD) : C2D_Color32(60, 70, 92, 255));
    }
    const float k = nx < 0 ? 1.0f : float(pts - (nx ? kMasteryTiers[nx - 1].at : 0)) / float(kMasteryTiers[nx].at - (nx ? kMasteryTiers[nx - 1].at : 0));
    C2D_DrawRectSolid(X + 8, Y + 19, 0, W - 16, 5, C2D_Color32(0, 0, 0, 150));
    C2D_DrawRectSolid(X + 8, Y + 19, 0, (W - 16) * std::max(0.0f, std::min(1.0f, k)), 5, C_GOLD);
    if (nx >= 0) std::snprintf(b, sizeof(b), "%d / %d · %s", int(pts), int(kMasteryTiers[nx].at), tr(kMasteryTiers[nx].name));
    else std::snprintf(b, sizeof(b), tr("%d · ¡Completa! Aura de maestro"), int(pts));
    r_->text_fit(b, X + 8, Y + 26, W - 16, 0.3f, lv >= MT_TITLE ? C_GOLD : C_TEXT, false);
  }
  for (int k = 0; k < 2; ++k) {
    if (p->types[k] < 0) continue;
    const float x = p->types[1] >= 0 ? 252 + k * 52 : 278;
    C2D_DrawRectSolid(x, 66, 0, 46, 14, rgb(kTypeColor[p->types[k]]));
    r_->text(kTypeName[p->types[k]], x + 23, 66, 0.32f, C_TEXT, 1);
  }
  // Con su naturaleza (core/mons.h): el stat que sube, en verde; el que baja, en rojo.
  const int nat = nature_of(*s_, dex);
  Mon q;
  mon_with_nature(*p, nat, q);
  const NatureDef& nd = kNatures[nat];
  auto stat = [&](const char* label, float v, float m, const char* txt, float y, int ns) {
    const u32 col = ns >= 0 && nd.up == ns ? C2D_Color32(0x5f, 0xe0, 0x8a, 255) : ns >= 0 && nd.down == ns ? C2D_Color32(0xff, 0x7a, 0x7a, 255) : C_MUTED;
    r_->text(label, 210, y, 0.32f, col);
    C2D_DrawRectSolid(272, y + 3, 0, 80, 7, C2D_Color32(0, 0, 0, 140));
    C2D_DrawRectSolid(272, y + 3, 0, 80 * std::min(1.0f, v / m), 7, C_GOLD);
    r_->text(txt, 392, y, 0.32f, col == C_MUTED ? C_TEXT : col, 2);
  };
  char t1[16], t2[16], t3[16];
  const int ns_atk = q.sa > q.pa ? NS_SA : NS_PA;
  std::snprintf(t1, sizeof(t1), "%d", p->hp); std::snprintf(t2, sizeof(t2), "×%.2f", double(q.atk)); std::snprintf(t3, sizeof(t3), "%d", int(q.spd));
  stat("Vida", float(p->hp), 183, t1, 84, -1);
  stat("Ataque", q.atk, 1.55f, t2, 96, ns_atk);
  stat("Velocidad", float(q.spd), 137, t3, 108, NS_SPD);
  // Naturaleza y habilidad (se cambian en su ficha: SELECT).
  {
    char l[32];
    if (nd.up < 0) std::snprintf(l, sizeof(l), "%s", tr("neutra"));
    else std::snprintf(l, sizeof(l), "+%s −%s", tr(nd.up == NS_PA ? "Atq" : nd.up == NS_SA ? "AtE" : nd.up == NS_DF ? "Def" : nd.up == NS_SD ? "DfE" : "Vel"),
                       tr(nd.down == NS_PA ? "Atq" : nd.down == NS_SA ? "AtE" : nd.down == NS_DF ? "Def" : nd.down == NS_SD ? "DfE" : "Vel"));
    r_->text(tr("Naturaleza"), 210, 124, 0.32f, C_MUTED);
    std::snprintf(b, sizeof(b), "%s · %s", nature_name(nat), l);
    r_->text_fit(b, 276, 124, 118, 0.34f, C_TEXT);
    const AbilPick ab = abil_of(*s_, dex);
    r_->text(tr("Habilidad"), 210, 138, 0.32f, C_MUTED);
    r_->text_fit(ab.id >= 0 ? abil_name(ab.id) : "—", 276, 138, 118, 0.34f, ab.slot == 2 ? C_GOLD : C_TEXT);
  }
  const TmBits taught = tm_taught_bits(*s_, dex);
  std::snprintf(b, sizeof(b), "Empieza con %s", kMoves[move_start_for(dex, s_->pstart[dex], s_->pupg[dex][UP_START] > 0, taught)].name);
  r_->text_fit(b, 300, 154, 196, 0.36f, C_GOLD, true, 1);
  int16_t pool[kMaxMoves];
  const int np = move_pool_for_dex(dex, p->types, pool, kMaxMoves, taught);
  for (int i = 0; i < np && i < 20; ++i) {
    const int ic = r_->move_icon[pool[i]];
    if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), 214 + (i % 10) * 18, 170 + (i / 10) * 18, 0);
  }
  if (np > 20) { std::snprintf(b, sizeof(b), "+%d", np - 20); r_->text(b, 396, 188, 0.32f, C_MUTED, 2); }
  if (dex == s_->partner) r_->text("Es tu compañero", 300, 210, 0.38f, C2D_Color32(0x5f, 0xe0, 0x8a, 255), 1);
  else r_->text("SELECT: su ficha", 300, 210, 0.34f, C_MUTED, 1);
  if (dex >= 0 && dex <= kMaxDex && s_->arceus_win[dex] > 0) {   // medalla: derrotó a Arceus (modo rápido)
    r_->draw_medal(214, 228, 7);
    r_->text("¡Derrotó a Arceus!", 228, 222, 0.36f, C_GOLD, 0);
  }
}

// ---------------- colección: sus ataques ----------------
// (la web, 2026-10-25: iconos al doble y, al tocarlos, tipo, categoría y qué hace)

namespace {
// Iconos de 16 px al doble, 8 por fila; con muchos (sus MT), más pequeños y 10 por fila.
struct MvGrid { int cols; float x0, y0, step, icon; };
MvGrid mv_grid(int n) { return n > 32 ? MvGrid{10, 14, 48, 30, 1.5f} : MvGrid{8, 16, 48, 37, 2.0f}; }
}

int App::dex_moves(int dex, int16_t* out) {
  const Mon* m = mon_by_dex(dex);
  return m ? move_pool_for_dex(dex, m->types, out, kMaxMoves, tm_taught_bits(*s_, dex)) : 0;   // (con sus MT)
}

void App::in_dex_moves(const UiIn& in, int dex) {
  int16_t pool[kMaxMoves];
  const int n = dex_moves(dex, pool);
  if (!n) { dex_moves_ = false; return; }
  const MvGrid G = mv_grid(n);
  if (in.rep & KEY_DLEFT) mv_cur_--;
  if (in.rep & KEY_DRIGHT) mv_cur_++;
  if (in.rep & KEY_DUP) mv_cur_ -= G.cols;
  if (in.rep & KEY_DDOWN) mv_cur_ += G.cols;
  mv_cur_ = clamp(mv_cur_, 0, n - 1);
  for (int i = 0; i < n; ++i)
    if (in_rect(&in, G.x0 + (i % G.cols) * G.step - 2, G.y0 + (i / G.cols) * G.step - 2, G.step - 1, G.step - 1)) { mv_cur_ = i; audio::play("hover"); }
}

void App::bottom_dex_moves(int dex) {
  int16_t pool[kMaxMoves];
  const int n = dex_moves(dex, pool);
  char b[64];
  std::snprintf(b, sizeof(b), "%s · %s", mon_by_dex(dex)->name, tr("Ataques"));
  header(b);
  const MvGrid G = mv_grid(n);
  const float box = G.icon * 16;
  const TmBits taught = tm_taught_bits(*s_, dex);
  for (int i = 0; i < n; ++i) {
    const float x = G.x0 + (i % G.cols) * G.step, y = G.y0 + (i / G.cols) * G.step;
    const bool sel = i == mv_cur_;
    const MoveDef& d = kMoves[pool[i]];
    C2D_DrawRectSolid(x - 2, y - 2, 0, box + 4, box + 4, sel ? C_GOLD : rgb(kTypeColor[d.type], 90));
    C2D_DrawRectSolid(x, y, 0, box, box, sel ? C_SEL : C_PANEL);
    const int ic = r_->move_icon[pool[i]];
    if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), x, y, 0, nullptr, G.icon, G.icon);
    // (los aprendidos con MT, con su marca: «MT» pequeñito en la esquina)
    const int tm = tm_index_of_move(pool[i]);
    if (tm >= 0 && taught.test(tm)) { C2D_DrawRectSolid(x + box - 13, y, 0, 13, 8, C2D_Color32(127, 147, 255, 230)); r_->text("MT", x + box - 6.5f, y - 1, 0.24f, C_TEXT, 1, false); }
  }
  r_->text("Toca uno para ver qué hace · L/R: pestaña · B: volver", 160, 226, 0.32f, C_MUTED, 1);
}

void App::top_dex_move(int dex) {
  int16_t pool[kMaxMoves];
  const int n = dex_moves(dex, pool);
  if (!n) return;
  const int id = pool[clamp(mv_cur_, 0, n - 1)];
  const MoveDef& d = kMoves[id];
  C2D_DrawRectSolid(0, 0, 0, TW, TH, C2D_Color32(8, 12, 20, 170));
  // A la izquierda, el Pokémon; a la derecha, el ataque.
  draw_mon(dex, false, 70, 200, 2);
  const int ic = r_->move_icon[id];
  if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), 150, 20, 0, nullptr, 3, 3);
  r_->text_fit(d.name, 206, 22, 186, 0.62f, C_TEXT, true);
  // Tipo y categoría.
  C2D_DrawRectSolid(206, 46, 0, 64, 16, rgb(kTypeColor[d.type]));
  r_->text(kTypeName[d.type], 238, 47, 0.34f, C_TEXT, 1);
  const char* cat = d.kind == K_BUFF ? "Mejora" : d.cat == CAT_PHYSICAL ? "Físico" : "Especial";
  C2D_DrawRectSolid(276, 46, 0, 70, 16, C2D_Color32(30, 40, 62, 255));
  r_->text(cat, 311, 47, 0.34f, d.kind == K_BUFF ? C_MUTED : C_GOLD, 1);
  // Qué hace.
  r_->text_wrap(d.desc, 150, 80, 0.4f, C_TEXT, 240);
  // De dónde le viene y si evoluciona.
  const Mon* m = mon_by_dex(dex);
  const bool natural = d.type == T_NORMAL || d.type == m->types[0] || d.type == m->types[1];
  const int tmi = tm_index_of_move(id);
  float y = 172;
  if (tmi >= 0 && tm_taught(*s_, dex, tmi)) { r_->text_fit("Lo aprendió con una MT (Tienda Kecleon)", 150, y, 240, 0.34f, C2D_Color32(0x9a, 0xe6, 0xff, 255)); y += 16; }
  else if (!natural) { r_->text_fit("Lo aprende por MT o tutor (como en los juegos)", 150, y, 240, 0.34f, C2D_Color32(0x9a, 0xe6, 0xff, 255)); y += 16; }
  if (id == move_start_for(dex, s_->pstart[dex], s_->pupg[dex][UP_START] > 0, tm_taught_bits(*s_, dex))) { r_->text("Empieza con él", 150, y, 0.34f, C_GOLD); y += 16; }
  if (d.evo_need >= 0 && d.evo_name) {
    char b[96];
    std::snprintf(b, sizeof(b), "Evoluciona a %s: al nivel máximo, con una carta de %s", d.evo_name, kNeedName[d.evo_need]);
    r_->text_wrap(b, 150, y, 0.34f, C2D_Color32(0xff, 0x8a, 0xd8, 255), 240);
  }
}

// ---------------- mejoras ----------------

namespace {
// Etiqueta «Físico» / «Especial» / «Mejora» de un ataque en las listas de elegir
// (centrada en cx). Con el lado fuerte del Pokémon, en dorado; el otro, apagado.
void cat_tag(Renderer* r, int id, const Mon* mon, float cx, float y) {
  const MoveDef& d = kMoves[id];
  const bool buff = d.kind == K_BUFF || d.cat < 0;
  const bool good = !buff && stats_fit(mon, d.cat) >= 0.999f;
  C2D_DrawRectSolid(cx - 24, y + 2, 0, 48, 14, buff ? C2D_Color32(30, 40, 62, 255) : d.cat == CAT_PHYSICAL ? C2D_Color32(120, 52, 30, 255) : C2D_Color32(40, 60, 130, 255));
  r->text(buff ? "Mejora" : d.cat == CAT_PHYSICAL ? "Físico" : "Especial", cx, y + 2, 0.32f, buff ? C_MUTED : good ? C_GOLD : C2D_Color32(200, 205, 220, 255), 1);
}
}  // namespace

// Arriba, en «Ataque inicial» y «Repertorio»: el ataque del cursor, de qué tipo es,
// si es físico o especial y lo que rinde con este Pokémon (su lado fuerte, al 100 %).
void App::top_pick_move(int id) {
  const MoveDef& d = kMoves[id];
  const Mon* m = mon_by_dex(up_dex_);
  C2D_DrawRectSolid(196, 36, 0, 200, 150, C2D_Color32(8, 12, 20, 190));
  const int ic = r_->move_icon[id];
  if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), 202, 42, 0, nullptr, 2, 2);
  r_->text_fit(d.name, 240, 44, 150, 0.46f, C_TEXT, true);
  C2D_DrawRectSolid(240, 62, 0, 64, 16, rgb(kTypeColor[d.type]));
  r_->text(kTypeName[d.type], 272, 63, 0.34f, C_TEXT, 1);
  const bool buff = d.kind == K_BUFF || d.cat < 0;
  C2D_DrawRectSolid(310, 62, 0, 70, 16, buff ? C2D_Color32(30, 40, 62, 255) : d.cat == CAT_PHYSICAL ? C2D_Color32(120, 52, 30, 255) : C2D_Color32(40, 60, 130, 255));
  r_->text(buff ? "Mejora" : d.cat == CAT_PHYSICAL ? "Físico" : "Especial", 345, 63, 0.34f, buff ? C_MUTED : C_GOLD, 1);
  r_->text_wrap(d.desc, 202, 84, 0.34f, C_TEXT, 188);
  if (buff) return;
  char b[96];
  const int pct = int(std::lround(stats_fit(m, d.cat) * 100));
  if (pct >= 100) std::snprintf(b, sizeof(b), "Es su lado fuerte: rinde al 100 %%");
  else std::snprintf(b, sizeof(b), "No es su lado fuerte: rinde al %d %%", pct);
  r_->text_wrap(b, 202, 160, 0.32f, pct >= 100 ? C_GOLD : C2D_Color32(255, 150, 120, 255), 188);
}

namespace {
// Orden alfabético en español (sin acentos y la ñ tras la n, como localeCompare 'es').
int es_key(const char*& p) {
  const unsigned char c = static_cast<unsigned char>(*p);
  if (c == 0xC3 && p[1]) {
    const unsigned char d = static_cast<unsigned char>(p[1]);
    p += 2;
    switch (d | 0x20) {
      case 0xA1: return 'a' * 2; case 0xA9: return 'e' * 2; case 0xAD: return 'i' * 2;
      case 0xB3: return 'o' * 2; case 0xBA: case 0xBC: return 'u' * 2;
      case 0xB1: return 'n' * 2 + 1;   // ñ
      default: return 0x200 + d;
    }
  }
  p++;
  return (c >= 'A' && c <= 'Z' ? c + 32 : c) * 2;
}
int es_cmp(const char* a, const char* b) {
  while (*a && *b) {
    const int x = es_key(a), y = es_key(b);
    if (x != y) return x < y ? -1 : 1;
  }
  return *a ? 1 : *b ? -1 : 0;
}

// Movimientos para empezar (G.startChoices), por nombre.
int start_choices_sorted(const Save& s, int dex, int16_t* out) {
  const int n = move_start_choices(dex, out, kMaxMoves, tm_taught_bits(s, dex));
  for (int a = 1; a < n; ++a) {
    const int16_t v = out[a];
    int b = a - 1;
    while (b >= 0 && es_cmp(kMoves[out[b]].name, kMoves[v].name) > 0) { out[b + 1] = out[b]; --b; }
    out[b + 1] = v;
  }
  return n;
}
// «Repertorio»: candidatos al segundo movimiento, por nombre (el inicial no).
int move2_choices_sorted(const Save& s, int dex, int16_t* out) {
  const int st = move_start_for(dex, s.pstart[dex], s.pupg[dex][UP_START] > 0, tm_taught_bits(s, dex));
  const int n = move_second_choices(dex, st, out, kMaxMoves, tm_taught_bits(s, dex));
  for (int a = 1; a < n; ++a) {
    const int16_t v = out[a];
    int b = a - 1;
    while (b >= 0 && es_cmp(kMoves[out[b]].name, kMoves[v].name) > 0) { out[b + 1] = out[b]; --b; }
    out[b + 1] = v;
  }
  return n;
}
}  // namespace

void App::in_upgrades(const UiIn& in) {
  if (!s_->owns(up_dex_)) up_dex_ = s_->partner;
  // Cambiar de Pokémon (L/R): el compañero primero, luego el resto que tienes.
  if (in.down & (KEY_L | KEY_R)) {
    const int dir = (in.down & KEY_R) ? 1 : -1;
    int d = up_dex_;
    for (int k = 0; k <= kMaxDex; ++k) { d = (d + dir + kMaxDex + 1) % (kMaxDex + 1); if (s_->owns(d)) break; }
    up_dex_ = d;
  }
  if (scene == SC_BIND) {
    // Atajos: 4 huecos, izquierda/derecha cambia el movimiento.
    int16_t pool[kMaxMoves];
    int np = move_pool_for_dex(up_dex_, mon_by_dex(up_dex_)->types, pool, kMaxMoves, tm_taught_bits(*s_, up_dex_));
    const int st = move_start_for(up_dex_, s_->pstart[up_dex_], s_->pupg[up_dex_][UP_START] > 0, tm_taught_bits(*s_, up_dex_));
    bool has = false;
    for (int i = 0; i < np; ++i) if (pool[i] == st) has = true;
    if (!has) pool[np++] = int16_t(st);
    if (in.rep & KEY_DUP) cur_ = (cur_ + 3) % 4;
    if (in.rep & KEY_DDOWN) cur_ = (cur_ + 1) % 4;
    if (in.rep & (KEY_DLEFT | KEY_DRIGHT)) {
      int16_t& slot = s_->pbind[up_dex_][cur_];
      int idx = -1;
      for (int i = 0; i < np; ++i) if (pool[i] == slot) idx = i;
      idx += (in.rep & KEY_DRIGHT) ? 1 : -1;
      if (idx >= np) idx = -1;
      if (idx < -1) idx = np - 1;
      const int id = idx < 0 ? -1 : pool[idx];
      for (int j = 0; j < 4; ++j) if (id >= 0 && s_->pbind[up_dex_][j] == id) s_->pbind[up_dex_][j] = -1;   // un movimiento, un hueco
      slot = int8_t(id);
      g_->dirty = true;
    }
    if (in.down & KEY_B) { save_now(); scene = SC_UPGRADES; cur_ = atajos_row(); }
    return;
  }
  if (scene == SC_START) {
    // Movimiento inicial (comprado una vez; se elige gratis cuando quieras).
    int16_t ids[kMaxMoves];
    const int n = start_choices_sorted(*s_, up_dex_, ids);
    if (in.rep & KEY_DUP) cur_ = std::max(0, cur_ - 1);
    if (in.rep & KEY_DDOWN) cur_ = std::min(n - 1, cur_ + 1);
    if (cur_ < scroll_) scroll_ = cur_;
    if (cur_ >= scroll_ + 9) scroll_ = cur_ - 8;
    bool tap = false;
    for (int i = 0; i < 9 && scroll_ + i < n; ++i) if (in_rect(&in, 10, 40 + i * 20, 300, 18)) { cur_ = scroll_ + i; tap = true; }
    if (((in.down & KEY_A) || tap) && n) {
      const int id = ids[cur_], base = move_base_start_for(up_dex_);
      s_->pstart[up_dex_] = int16_t(id == base ? -1 : id);
      // El inicial no puede estar vetado.
      int16_t* l = s_->pban[up_dex_];
      for (int k = 0; k < 3; ++k) if (l[k] == id) { for (int q = k; q < 2; ++q) l[q] = l[q + 1]; l[2] = -1; break; }
      g_->dirty = true;
      audio::play("select");
      char b[96];
      std::snprintf(b, sizeof(b), "%s empezará con %s", mon_by_dex(up_dex_)->name, kMoves[id].name);
      g_notices.push(b, "", -1);
    }
    if (in.down & KEY_B) { save_now(); scene = SC_UPGRADES; cur_ = UP_START; }
    return;
  }
  if (scene == SC_MOVE2) {
    // «Repertorio» comprado: el segundo movimiento (fila 0 = al azar, como en la web).
    int16_t ids[kMaxMoves];
    const int n = move2_choices_sorted(*s_, up_dex_, ids) + 1;
    if (in.rep & KEY_DUP) cur_ = std::max(0, cur_ - 1);
    if (in.rep & KEY_DDOWN) cur_ = std::min(n - 1, cur_ + 1);
    if (cur_ < scroll_) scroll_ = cur_;
    if (cur_ >= scroll_ + 9) scroll_ = cur_ - 8;
    bool tap = false;
    for (int i = 0; i < 9 && scroll_ + i < n; ++i) if (in_rect(&in, 10, 40 + i * 20, 300, 18)) { cur_ = scroll_ + i; tap = true; }
    if ((in.down & KEY_A) || tap) {
      const int id = cur_ == 0 ? -1 : ids[cur_ - 1];
      s_->pmove2[up_dex_] = int16_t(id);
      if (id >= 0) {   // (el elegido no puede estar vetado)
        int16_t* l = s_->pban[up_dex_];
        for (int k = 0; k < 3; ++k) if (l[k] == id) { for (int q = k; q < 2; ++q) l[q] = l[q + 1]; l[2] = -1; break; }
      }
      g_->dirty = true;
      audio::play("select");
      char b[96];
      if (id >= 0) std::snprintf(b, sizeof(b), "%s empezará también con %s", mon_by_dex(up_dex_)->name, kMoves[id].name);
      else std::snprintf(b, sizeof(b), "%s: segundo movimiento al azar", mon_by_dex(up_dex_)->name);
      g_notices.push(b, "", -1);
    }
    if (in.down & KEY_B) { save_now(); scene = SC_UPGRADES; cur_ = UP_MOVES; }
    return;
  }
  if (scene == SC_BAN) {
    int16_t pool[kMaxMoves];
    int np0 = move_pool_for_dex(up_dex_, mon_by_dex(up_dex_)->types, pool, kMaxMoves, tm_taught_bits(*s_, up_dex_)), np = 0;
    const int st = move_start_for(up_dex_, s_->pstart[up_dex_], s_->pupg[up_dex_][UP_START] > 0, tm_taught_bits(*s_, up_dex_));
    for (int i = 0; i < np0; ++i) if (pool[i] != st) pool[np++] = pool[i];   // el de inicio no se veta
    if (in.rep & KEY_DUP) cur_ = std::max(0, cur_ - 1);
    if (in.rep & KEY_DDOWN) cur_ = std::min(np - 1, cur_ + 1);
    if (cur_ < scroll_) scroll_ = cur_;
    if (cur_ >= scroll_ + 9) scroll_ = cur_ - 8;
    const int max = s_->pupg[up_dex_][UP_BAN];
    if ((in.down & KEY_A) && np) {
      const int id = pool[cur_];
      int16_t* l = s_->pban[up_dex_];
      int at = -1, cnt = 0;
      for (int k = 0; k < 3; ++k) { if (l[k] == id) at = k; if (l[k] >= 0) cnt++; }
      if (at >= 0) { for (int k = at; k < 2; ++k) l[k] = l[k + 1]; l[2] = -1; }
      else if (cnt >= max) { char b[64]; std::snprintf(b, sizeof(b), "Ya tienes %d vetados: quita uno primero", max); g_notices.push(b, "", -1); }
      else l[cnt] = int8_t(id);
      g_->dirty = true;
    }
    if (in.down & KEY_B) { save_now(); scene = SC_UPGRADES; cur_ = UP_BAN; }
    return;
  }
  const int n = atajos_row() + 1;   // las de la web, «Megapiedra» (si tiene mega), «Más pasivas…» y Atajos
  if (cur_ >= n) cur_ = n - 1;      // (al cambiar a un Pokémon sin mega)
  if (in.rep & KEY_DUP) cur_ = (cur_ + n - 1) % n;
  if (in.rep & KEY_DDOWN) cur_ = (cur_ + 1) % n;
  if (cur_ < scroll_) scroll_ = cur_;
  if (cur_ >= scroll_ + 7) scroll_ = cur_ - 6;
  for (int i = 0; i < 7; ++i) if (in_rect(&in, 6, 28 + i * 28, 308, 26)) { if (cur_ == scroll_ + i) { /* toque doble: comprar */ } cur_ = scroll_ + i; }
  if (in.down & KEY_A) {
    if (cur_ == atajos_row()) { scene = SC_BIND; cur_ = 0; return; }
    if (cur_ == four_row()) {   // «Más pasivas a niveles altos» (del port): una vez, kFourUpCost
      if (!s_->pfour[up_dex_] && s_->coins >= kFourUpCost) {
        s_->coins -= kFourUpCost;
        s_->pfour[up_dex_] = 1;
        audio::play("coin");
        audio::play("buy");
        save_now();
        char b[96];
        std::snprintf(b, sizeof(b), "%s · Más pasivas a niveles altos", mon_by_dex(up_dex_)->name);
        g_notices.push(b, "Desde el nivel 25: al menos 2 cartas pasivas de 4", -1);
      }
      return;
    }
    if (cur_ == UP_COUNT) {   // «Megapiedra» (del port)
      uint8_t& ml = s_->pmega[up_dex_];
      const int price = mega_up_cost(ml);
      if (ml < kMegaUpMax && s_->coins >= price) {
        s_->coins -= price;
        ml++;
        audio::play("coin");
        audio::play("buy");
        save_now();
        char b[96];
        std::snprintf(b, sizeof(b), "%s · Megapiedra: nivel %d (%d %%)", mon_by_dex(up_dex_)->name, ml, int(mega_chance(ml) * 100 + 0.5f));
        g_notices.push(b, "", -1);
      }
      return;
    }
    uint8_t& lv = s_->pupg[up_dex_][cur_];
    // Movimiento inicial ya comprado: A abre la lista para elegirlo.
    if (cur_ == UP_START && lv >= kUpgrades[cur_].max) { scene = SC_START; cur_ = 0; scroll_ = 0; return; }
    // Repertorio ya comprado: A abre la lista para elegir el segundo movimiento.
    if (cur_ == UP_MOVES && lv >= kUpgrades[cur_].max) { scene = SC_MOVE2; cur_ = 0; scroll_ = 0; return; }
    if (lv < kUpgrades[cur_].max) {
      const int price = upgrade_cost(cur_, lv);
      if (s_->coins >= price) {
        s_->coins -= price;
        lv++;
        audio::play("coin");
        audio::play("buy");
        save_now();
        char b[96];
        std::snprintf(b, sizeof(b), "%s · %s: nivel %d", mon_by_dex(up_dex_)->name, kUpgrades[cur_].name, lv);
        g_notices.push(b, "", -1);
      }
    }
  }
  if ((in.down & KEY_Y) && cur_ == UP_BAN && s_->pupg[up_dex_][UP_BAN] > 0) { scene = SC_BAN; cur_ = 0; scroll_ = 0; }
  if ((in.down & KEY_Y) && cur_ == UP_START && s_->pupg[up_dex_][UP_START] > 0) { scene = SC_START; cur_ = 0; scroll_ = 0; }
  if (in.down & KEY_B) go(SC_MENU);
}

void App::bottom_upgrades() {
  char b[128];
  std::snprintf(b, sizeof(b), "Mejoras · %s", mon_by_dex(up_dex_)->name);
  header(b);
  if (scene == SC_BIND) {
    r_->text_wrap("Elige qué movimiento va siempre en cada hueco, para la memoria muscular. Si aún no tienes tantos movimientos, va al hueco más cercano.", 160, 28, 0.32f, C_MUTED, 300, 1);
    for (int i = 0; i < 4; ++i) {
      const float y = 80 + i * 34;
      C2D_DrawRectSolid(10, y, 0, 300, 30, i == cur_ ? C_SEL : C_PANEL);
      std::snprintf(b, sizeof(b), "%d", i + 1);
      r_->text(b, 20, y + 7, 0.42f, C_GOLD);
      const int id = s_->pbind[up_dex_][i];
      r_->text_fit(id >= 0 ? kMoves[id].name : "Libre", 200, y + 7, 150, 0.42f, C_TEXT, true, 1);
      if (i == cur_) { r_->text("<", 120, y + 7, 0.42f, C_MUTED, 1); r_->text(">", 280, y + 7, 0.42f, C_MUTED, 1); }
    }
    r_->text("Arriba/abajo: hueco · izquierda/derecha: movimiento · B: volver", 160, 226, 0.3f, C_MUTED, 1);
    return;
  }
  if (scene == SC_START) {
    int16_t ids[kMaxMoves];
    const int n = start_choices_sorted(*s_, up_dex_, ids);
    const int cur = move_start_for(up_dex_, s_->pstart[up_dex_], true, tm_taught_bits(*s_, up_dex_)), base = move_base_start_for(up_dex_);
    r_->text("Elige con qué ataque empieza (gratis, cuando quieras)", 160, 25, 0.34f, C_MUTED, 1);
    for (int i = 0; i < 9 && scroll_ + i < n; ++i) {
      const int id = ids[scroll_ + i];
      const float y = 40 + i * 20;
      C2D_DrawRectSolid(10, y, 0, 300, 18, scroll_ + i == cur_ ? C_SEL : C_PANEL);
      const int ic = r_->move_icon[id];
      if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), 14, y + 1, 0);
      r_->text_fit(kMoves[id].name, 36, y + 2, 118, 0.36f, id == cur ? C_GOLD : C_TEXT, true);
      cat_tag(r_, id, mon_by_dex(up_dex_), 182, y);
      if (id == base) r_->text("(de serie)", 256, y + 3, 0.3f, C_MUTED, 2);
      if (id == cur) r_->text("ELEGIDO", 304, y + 2, 0.32f, C_GOLD, 2);
    }
    r_->text("A: elegir · B: volver", 160, 226, 0.32f, C_MUTED, 1);
    return;
  }
  if (scene == SC_MOVE2) {
    int16_t ids[kMaxMoves];
    const int n = move2_choices_sorted(*s_, up_dex_, ids) + 1;
    const int cur = s_->pmove2[up_dex_];
    r_->text("Elige tu segundo movimiento (gratis, cuando quieras)", 160, 25, 0.34f, C_MUTED, 1);
    for (int i = 0; i < 9 && scroll_ + i < n; ++i) {
      const int row = scroll_ + i, id = row == 0 ? -1 : ids[row - 1];
      const float y = 40 + i * 20;
      C2D_DrawRectSolid(10, y, 0, 300, 18, row == cur_ ? C_SEL : C_PANEL);
      const bool on = id == cur || (row == 0 && (cur < 0 || cur >= kMoveCount));
      if (id < 0) r_->text("Al azar (como en la web)", 36, y + 2, 0.36f, on ? C_GOLD : C_TEXT);
      else {
        const int ic = r_->move_icon[id];
        if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), 14, y + 1, 0);
        r_->text_fit(kMoves[id].name, 36, y + 2, 150, 0.36f, on ? C_GOLD : C_TEXT, true);
        cat_tag(r_, id, mon_by_dex(up_dex_), 222, y);
      }
      if (on) r_->text("ELEGIDO", 304, y + 2, 0.32f, C_GOLD, 2);
    }
    r_->text("A: elegir · B: volver", 160, 226, 0.32f, C_MUTED, 1);
    return;
  }
  if (scene == SC_BAN) {
    int16_t pool[kMaxMoves];
    int np0 = move_pool_for_dex(up_dex_, mon_by_dex(up_dex_)->types, pool, kMaxMoves, tm_taught_bits(*s_, up_dex_)), np = 0;
    const int st = move_start_for(up_dex_, s_->pstart[up_dex_], s_->pupg[up_dex_][UP_START] > 0, tm_taught_bits(*s_, up_dex_));
    for (int i = 0; i < np0; ++i) if (pool[i] != st) pool[np++] = pool[i];
    int cnt = 0;
    for (int k = 0; k < 3; ++k) if (s_->pban[up_dex_][k] >= 0) cnt++;
    std::snprintf(b, sizeof(b), "Movimientos que no quieres en las cartas (%d/%d)", cnt, s_->pupg[up_dex_][UP_BAN]);
    r_->text(b, 160, 25, 0.34f, C_MUTED, 1);
    for (int i = 0; i < 9 && scroll_ + i < np; ++i) {
      const int id = pool[scroll_ + i];
      bool on = false;
      for (int k = 0; k < 3; ++k) if (s_->pban[up_dex_][k] == id) on = true;
      const float y = 40 + i * 20;
      C2D_DrawRectSolid(10, y, 0, 300, 18, scroll_ + i == cur_ ? C_SEL : C_PANEL);
      const int ic = r_->move_icon[id];
      if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), 14, y + 1, 0);
      r_->text_fit(kMoves[id].name, 36, y + 2, 210, 0.36f, on ? C2D_Color32(255, 95, 109, 255) : C_TEXT, true);
      if (on) r_->text("VETADO", 304, y + 2, 0.32f, C2D_Color32(255, 95, 109, 255), 2);
    }
    r_->text("A: vetar / quitar · B: volver", 160, 226, 0.32f, C_MUTED, 1);
    return;
  }
  for (int i = 0; i < 7; ++i) {
    const int k = scroll_ + i;
    if (k > atajos_row()) break;
    const float y = 28 + i * 28;
    C2D_DrawRectSolid(6, y, 0, 308, 26, k == cur_ ? C_SEL : C_PANEL);
    if (k == atajos_row()) { r_->text("Atajos de movimientos", 160, y + 6, 0.4f, C_TEXT, 1); continue; }
    if (k == four_row()) {   // «Más pasivas a niveles altos» (del port)
      const bool got = s_->pfour[up_dex_] > 0;
      const int ic = r_->ui_icon[13];   // (cartas)
      if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), 10, y + 1, 0, nullptr, 2, 2);
      r_->text_fit("Más pasivas a niveles altos", 38, y + 1, 228, 0.38f, C_TEXT, true);   // (largo: hasta el precio)
      r_->text_fit("Nivel 25: 2 de 4 cartas, pasivas", 38, y + 13, 168, 0.28f, C_MUTED);
      C2D_DrawRectSolid(210, y + 18, 0, 4, 4, got ? C_GOLD : C2D_Color32(60, 70, 96, 255));
      if (got) r_->text("Máx.", 308, y + 4, 0.36f, C_MUTED, 2);
      else {
        std::snprintf(b, sizeof(b), "%d", kFourUpCost);
        r_->text(b, 308, y + 2, 0.38f, s_->coins >= kFourUpCost ? C_GOLD : C2D_Color32(140, 110, 70, 255), 2);
      }
      continue;
    }
    if (k == UP_COUNT) {   // «Megapiedra» (del port, sólo con mega)
      const int ml = s_->pmega[up_dex_];
      r_->draw_mega_stone(mega_index(up_dex_, 0), 10, y + 1, 24);
      if (mega_count(up_dex_) > 1) r_->draw_mega_stone(mega_index(up_dex_, 1), 22, y + 11, 14);   // (y la otra, pequeña)
      r_->text("Megapiedra", 38, y + 1, 0.38f, C_TEXT);
      std::snprintf(b, sizeof(b), "%d %% de que un objeto sea su megapiedra", int(mega_chance(ml) * 100 + 0.5f));
      r_->text(b, 38, y + 13, 0.28f, C_MUTED, 0, false);
      for (int p = 0; p < kMegaUpMax; ++p) C2D_DrawRectSolid(210 + p * 6, y + 18, 0, 4, 4, p < ml ? C_GOLD : C2D_Color32(60, 70, 96, 255));
      if (ml >= kMegaUpMax) r_->text("Máx.", 308, y + 4, 0.36f, C_MUTED, 2);
      else {
        std::snprintf(b, sizeof(b), "%d", mega_up_cost(ml));
        r_->text(b, 308, y + 2, 0.38f, s_->coins >= mega_up_cost(ml) ? C_GOLD : C2D_Color32(140, 110, 70, 255), 2);
      }
      continue;
    }
    const UpgradeDef& u = kUpgrades[k];
    const int lv = s_->pupg[up_dex_][k];
    const int ic = r_->icons.find(u.icon);
    if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), 10, y + 1, 0, nullptr, 2, 2);
    r_->text_fit(u.name, 38, y + 1, 168, 0.38f, C_TEXT, true);
    if (k == UP_MOVES && lv >= u.max) {   // comprada: cuál es el segundo y que se cambia con A
      const int m2 = s_->pmove2[up_dex_];
      std::snprintf(b, sizeof(b), "2.º: %s · A: elegir", m2 >= 0 && m2 < kMoveCount ? kMoves[m2].name : "al azar");
      r_->text_fit(b, 38, y + 13, 168, 0.28f, C_GOLD);
    } else r_->text_fit(u.unit, 38, y + 13, 168, 0.28f, C_MUTED);
    for (int p = 0; p < u.max; ++p) C2D_DrawRectSolid(210 + p * 6 - (u.max > 8 ? 12 : 0), y + 18, 0, 4, 4, p < lv ? C_GOLD : C2D_Color32(60, 70, 96, 255));
    if (lv >= u.max) r_->text("Máx.", 308, y + 4, 0.36f, C_MUTED, 2);
    else {
      std::snprintf(b, sizeof(b), "%d", upgrade_cost(k, lv));
      r_->text(b, 308, y + 2, 0.38f, s_->coins >= upgrade_cost(k, lv) ? C_GOLD : C2D_Color32(140, 110, 70, 255), 2);
    }
  }
  const char* hint = "A: comprar · L/R: otro Pokémon · B: volver";
  if (cur_ == UP_BAN && s_->pupg[up_dex_][UP_BAN]) hint = "A: comprar · Y: elegir vetados · L/R: Pokémon";
  if (cur_ == UP_START && s_->pupg[up_dex_][UP_START]) hint = "A o Y: elegir movimiento · L/R: otro Pokémon · B: volver";
  r_->text(hint, 160, 226, 0.32f, C_MUTED, 1);
}

// ---------------- gacha ----------------

void App::in_gacha(const UiIn& in) {
  // L/R (o tocar las flechas del selector): región anterior / siguiente.
  // (L/R ya suenan en update(); al tocar suena el clic)
  const bool ev = hw3ds::on();
  if (pulling_) return;   // (esperando la tirada del servidor)
  // Comprobando la conexión (con cuenta): se espera a que acabe lo que haya en marcha y se prueba
  if (gacha_net_ == 1) {
    gacha_net_t_ += 1.0f / 60;
    const cloud3ds::JobState st = cloud3ds::state();
    if (cloud3ds::check_job() && (st == cloud3ds::JOB_DONE || st == cloud3ds::JOB_FAILED)) {
      gacha_net_ = st == cloud3ds::JOB_DONE ? 0 : 2;
      if (gacha_net_ == 2) audio::play("error");
      cloud3ds::finish();
    } else if (st == cloud3ds::JOB_IDLE) {
      if (!cloud3ds::start_check()) gacha_net_ = 2;
    }
    if (in.down & KEY_B) { gacha_net_ = 0; go(SC_MENU); }
    return;
  }
  if (gacha_net_ == 2) {   // sin conexión: sólo volver
    if ((in.down & (KEY_B | KEY_A)) || in.tap) { gacha_net_ = 0; go(SC_MENU); }
    return;
  }
  if (!gacha_open(bgen_, ev)) bgen_ = 1;   // (se acabó Halloween con el gacha abierto)
  if ((in.down & KEY_L) || in_rect(&in, 10, 58, 90, 34)) bgen_ = gacha_prev(bgen_, ev);
  if ((in.down & KEY_R) || in_rect(&in, 220, 58, 90, 34)) bgen_ = gacha_next(bgen_, ev);
  // Se tira de 10 en 10 o de 100 en 100 (web, 22-10-2026: ya no hay ×1).
  // (tras una tirada no queda ningún botón elegido: pulsando A seguido para pasar
  // las animaciones se encadenaban tiradas sin querer; arreglo de la web del 25-10)
  if (cur_ < 0 && (in.rep & (KEY_DUP | KEY_DDOWN))) cur_ = 0;
  else if (in.rep & KEY_DUP) cur_ = (cur_ + 3) % 4;
  else if (in.rep & KEY_DDOWN) cur_ = (cur_ + 1) % 4;
  if (cur_ > 3) cur_ = 3;
  int pick = -1;
  for (int i = 0; i < 4; ++i) if (in_rect(&in, 20, 116 + i * 27, 280, 24)) pick = i;
  if ((in.down & KEY_A) && cur_ >= 0) pick = cur_;
  if (in.down & KEY_B) { go(SC_MENU); return; }
  if (pick < 0) return;
  cur_ = pick;
  const int n = pick < 2 ? 10 : 100;
  const bool t10 = pick == 1 || pick == 3;
  if (!gacha_open(bgen_, hw3ds::on())) return;
  if (gacha_locked()) {
    audio::play("error");
    g_notices.push("Región Todas bloqueada", "No disponible con cuenta online, para evitar trampas", -1);
    return;
  }
  {
    int c1 = 0, c10 = 0;
    if (!gacha_cost(*s_, n, t10, c1, c10)) { audio::play("error"); g_notices.push("No tienes tickets suficientes: consíguelos jugando", "", -1); return; }
  }
  // Con la cuenta de pokesurvivor.com, se tira en su servidor (desde el 15-11-2026 sólo valen
  // los Pokémon que salen ahí): sale cuando contesta (pull_tick).
  if (cloud3ds::account().on && !autotest) {
    if (cloud3ds::state() != cloud3ds::JOB_IDLE) { audio::play("error"); g_notices.push("Un momento: se está sincronizando", "Prueba otra vez en unos segundos.", -1); return; }
    pull_had_iris_ = gacha_has_iris(*s_, bgen_);
    cloud_t0_updated_ = s_->updated; cloud_t0_partner_ = s_->partner * 2 + (s_->partner_shiny ? 1 : 0);
    if (cloud3ds::start_pull(save_full_json(*s_), n, bgen_, t10)) { pulling_ = true; pull_wait_t_ = 0; audio::play("select"); }
    return;
  }
  const bool had_iris = gacha_has_iris(*s_, bgen_);
  Rng rr(uint32_t(osGetTime()) ^ 0x9e3779b9u);
  npulls_ = gacha_pull(*s_, n, bgen_, t10, rr, now_ms(), pulls_);
  if (!npulls_) { audio::play("error"); g_notices.push("No tienes tickets suficientes: consíguelos jugando", "", -1); return; }
  iris_gen_ = !had_iris && gacha_has_iris(*s_, bgen_) ? bgen_ : -1;
  pull_show();
}

// Enseña la tirada que acaba de salir (del port o del servidor): el resumen de la ×100 y la tragaperras.
void App::pull_show() {
  progress_check_ach(*s_, now_ms());
  save_now();
  // Resumen de la ×100: de mejor a peor (shiny, rareza, nuevo).
  if (npulls_ == 100) {
    for (int i = 0; i < 100; ++i) sum_[i] = int16_t(i);
    auto score = [&](int i) { const PullResult& r = pulls_[i]; return (r.shiny ? 100 : 0) + r.rarity * 10 + (r.is_new ? 1 : 0); };
    std::stable_sort(sum_, sum_ + 100, [&](int16_t a, int16_t b) { return score(a) > score(b); });
    sum_scroll_ = 0;
  }
  pull_i_ = 0; pull_t_ = 0; pull_fast_ = false; pull_done_ = false;
  scene = SC_PULL;
}

// ×100: lo mejor de cada ×10 (es lo que enseña la tragaperras en ese giro).
const PullResult& App::spin(int i) const {
  if (npulls_ != 100) return pulls_[i];
  int best = i * 10;
  auto score = [&](int k) { const PullResult& r = pulls_[k]; return (r.shiny ? 100 : 0) + r.rarity * 10 + (r.is_new ? 1 : 0); };
  for (int k = i * 10 + 1; k < i * 10 + 10; ++k) if (score(k) > score(best)) best = k;
  return pulls_[best];
}

void App::in_pull(const UiIn& in, float dt) {
  const float prev = pull_t_;
  pull_t_ += dt * (pull_fast_ ? 4 : 1);
  if (!pull_done_) {
    // Sonidos de la tragaperras (slots.js): palanca, cada rodillo y el premio.
    const PullResult& pr = spin(pull_i_);
    const bool ten = npulls_ >= 10, big = pr.rarity >= 4 || pr.shiny;
    auto at = [&](float x) { return prev < x && pull_t_ >= x; };
    if (at(0.001f)) audio::play("ball");
    const float spin_end = ten ? 0.25f : 0.6f;
    for (int k = 0; k < 3; ++k) {
      const float stop = spin_end + k * (ten ? 0.08f : 0.32f) + (k == 2 && big ? (ten ? 0.5f : 1.1f) : 0);
      if (at(stop)) {
        audio::play("shake");
        if (k == 2) {
          char b[16]; std::snprintf(b, sizeof(b), "reveal_%d", pr.rarity);
          audio::play(b);
          if (pr.rarity == 5) audio::play("legend");
          if (pr.shiny) audio::play("shiny");
        }
      }
    }
  }
  // El resumen de la ×100: arriba/abajo recorre las cartas.
  if (pull_done_ && npulls_ == 100) {
    if (in.rep & KEY_DUP) sum_scroll_ = std::max(0, sum_scroll_ - 1);
    if (in.rep & KEY_DDOWN) sum_scroll_ = std::min(100 / 8 - 3, sum_scroll_ + 1);
  }
  if (in.down & (KEY_A | KEY_B) || (in.tap && !(pull_done_ && npulls_ == 100 && in.ty > 40 && in.ty < 220))) {
    if (pull_done_) {
      scene = SC_GACHA;
      cur_ = -1;   // (ningún botón de tirar elegido: otra A no vuelve a tirar)
      if (iris_gen_ >= 0) {
        char t[80];
        std::snprintf(t, sizeof(t), "¡Amuleto Iris de %s!", gacha_banner(iris_gen_).name);
        g_notices.push(t, "Tienes su Pokédex completa: shiny al 2 % en su banner", -1);
        audio::play("legend");
        iris_gen_ = -1;
      }
      return;
    }
    pull_fast_ = true;
  }
  // Duración de cada tirada (la ×10, rápida; las grandes, con suspense).
  if (pull_done_) return;
  const bool ten = npulls_ >= 10;
  const PullResult& r = spin(pull_i_);
  const bool big = r.rarity >= 4 || r.shiny;
  const float dur = (ten ? 0.7f : 1.8f) + (big ? (ten ? 0.5f : 1.1f) : 0);
  if (pull_t_ >= dur) {
    pull_t_ = 0;
    pull_i_++;
    if (pull_i_ >= nspins()) { pull_i_ = nspins() - 1; pull_done_ = true; }
  }
}

// «Todas» con la cuenta de pokesurvivor.com: no se puede tirar (la tirada va por su servidor).
bool App::gacha_locked() const { return bgen_ == kGachaAll && cloud3ds::account().on; }

// Candado (arriba a la izquierda en x, y; s = escala: 1 → 12×14)
void App::padlock(float x, float y, float s, u32 c) {
  x -= 6 * s;
  C2D_DrawRectSolid(x + 2 * s, y, 0, 8 * s, 1.5f * s, c);                // arco
  C2D_DrawRectSolid(x + 2 * s, y, 0, 1.5f * s, 6 * s, c);
  C2D_DrawRectSolid(x + 8.5f * s, y, 0, 1.5f * s, 6 * s, c);
  C2D_DrawRectSolid(x, y + 6 * s, 0, 12 * s, 8 * s, c);                  // cuerpo
  C2D_DrawRectSolid(x + 5.25f * s, y + 8.5f * s, 0, 1.5f * s, 3 * s, C2D_Color32(40, 22, 32, 255));   // ojo
}

void App::bottom_gacha() {
  const BannerDef& bn = gacha_banner(bgen_);
  char b[128];
  std::snprintf(b, sizeof(b), "Gacha · %s", bn.name);
  header(b);
  int all = 0, own = 0;
  gacha_dex_of(*s_, bgen_, own, all);
  std::snprintf(b, sizeof(b), "Tienes %d de %d · Tiradas sin legendaria: %d / %d", own, all, int(s_->pity[bgen_]), kPity5);
  r_->text_fit(b, 160, 24, 304, 0.34f, C_MUTED, false, 1);
  std::snprintf(b, sizeof(b), "Tickets: %d · Tickets ×10: %d", int(s_->t1), int(s_->t10));
  r_->text(b, 160, 36, 0.38f, C_GOLD, 1);
  // Amuleto Iris de esta región: lo tienes (shiny al 2 %) o lo que te falta.
  // (la Noche Espectral de Halloween no tiene: sus shiny ya salen al 2 %)
  if (bgen_ == kGachaSpooky) std::snprintf(b, sizeof(b), "Fantasma de todas las regiones · shiny al 2 %% · hasta el 2 de noviembre");
  else if (own >= all) std::snprintf(b, sizeof(b), "Amuleto Iris de %s · shiny al 2 %%", bn.name);
  else std::snprintf(b, sizeof(b), "Amuleto Iris: te faltan %d (shiny al 2 %% en este banner)", all - own);
  r_->text_fit(b, 160, 48, 304, 0.32f, bgen_ == kGachaSpooky ? C2D_Color32(255, 160, 80, 255) : own >= all ? C2D_Color32(0xff, 0x8a, 0xd8, 255) : C_MUTED, false, 1);
  // Selector de región: anterior · ACTUAL · siguiente (L/R o tocar a los lados)
  // y un punto por región con la actual marcada.
  const bool ev = hw3ds::on();
  const int prev = gacha_prev(bgen_, ev), next = gacha_next(bgen_, ev);
  C2D_DrawRectSolid(10, 58, 0, 300, 54, C_PANEL);
  const u32 arrow = C2D_Color32(255, 210, 63, 255);
  C2D_DrawTriangle(16, 75, arrow, 24, 69, arrow, 24, 81, arrow, 0);
  C2D_DrawTriangle(304, 75, arrow, 296, 69, arrow, 296, 81, arrow, 0);
  r_->text("L", 30, 69, 0.36f, C_GOLD, 0);
  r_->text("R", 290, 69, 0.36f, C_GOLD, 2);
  r_->text(gacha_banner(prev).name, 44, 69, 0.36f, C_MUTED, 0);
  r_->text(gacha_banner(next).name, 276, 69, 0.36f, C_MUTED, 2);
  r_->text(bn.name, 160, 65, 0.5f, C_GOLD, 1);
  const int ndots = ev ? 11 : 10;   // (el 10.º: «Todas»; el 11.º, la Noche Espectral en Halloween)
  for (int i = 1; i <= ndots; ++i) {
    const float x = 160 + (i - (ndots + 1) / 2.0f) * 14;
    const int gi = i == 10 ? kGachaAll : i == 11 ? kGachaSpooky : i;
    if (gi == bgen_) C2D_DrawRectSolid(x - 3, 95, 0, 7, 7, C_GOLD);
    else C2D_DrawRectSolid(x - 2, 96, 0, 5, 5, C2D_Color32(90, 100, 125, 255));
  }
  static const char* const L[4] = {"Tirar ×10 (10 tickets)", "Tirar ×10 (1 ticket ×10)", "Tirar ×100 (100 tickets)", "Tirar ×100 (10 tickets ×10)"};
  if (gacha_locked() && gacha_net_ == 0) {
    // Con la cuenta de pokesurvivor.com se tira en su servidor y «Todas» es sólo de la 3DS:
    // bloqueada, y que se vea (candado, sin botones de tirar).
    const u32 red = rgb(0xff6b6b);
    rrect(16, 114, 288, 106, C2D_Color32(40, 22, 32, 250));
    rframe(16, 114, 288, 106, red);
    padlock(160, 119, 1.3f, red);
    r_->text_fit("Bloqueada con tu cuenta", 160, 139, 272, 0.46f, rgb(0xff8a8a), true, 1);
    r_->text_wrap("pokesurvivor.com está dificultando la sincronización y, para evitar las trampas de algunos jugadores, la región Todas no está disponible con cuenta online.",
                  160, 157, 0.34f, C_TEXT, 272, 1);
    r_->text_fit("Cambia de región con L / R para tirar", 160, 205, 272, 0.36f, C_GOLD, false, 1);
    r_->text("L / R: cambiar de región · B: volver", 160, 226, 0.32f, C_MUTED, 1);
    return;
  }
  for (int i = 0; i < 4; ++i) {
    int c1, c10;
    const bool ok = gacha_cost(*s_, i < 2 ? 10 : 100, i == 1 || i == 3, c1, c10);
    const float y = 116 + i * 27;
    C2D_DrawRectSolid(20, y, 0, 280, 24, i == cur_ ? C_SEL : C_PANEL);
    if (i == cur_) C2D_DrawRectSolid(20, y, 0, 3, 24, C_GOLD);
    r_->text(L[i], 160, y + 4, 0.4f, ok ? C_TEXT : C2D_Color32(110, 120, 140, 255), 1);
  }
  r_->text(cur_ < 0 ? "Elige con arriba/abajo (o toca) · B: volver" : "Cada ×10 garantiza una Épica · B: volver", 160, 226, 0.32f, C_MUTED, 1);
  if (gacha_net_ != 0) {   // comprobando la conexión, o sin ella
    C2D_DrawRectSolid(0, 0, 0, 320, 240, C2D_Color32(6, 10, 20, 225));
    if (gacha_net_ == 1) {
      rrect(40, 92, 240, 56, C2D_Color32(24, 34, 60, 250));
      rframe(40, 92, 240, 56, rgb(0xffd23f));
      const int dots = int(gacha_net_t_ * 3) % 4;
      char w[80];
      std::snprintf(w, sizeof(w), "%s%.*s", tr("Conectando con pokesurvivor.com"), dots, "...");
      r_->text_fit(w, 160, 102, 228, 0.4f, C_GOLD, false, 1);
      r_->text(tr("B: volver"), 160, 124, 0.3f, C_MUTED, 1);
    } else {
      rrect(14, 44, 292, 156, C2D_Color32(24, 34, 60, 250));
      rframe(14, 44, 292, 156, rgb(0xff6b6b));
      r_->text(tr("Hace falta conexión a internet"), 160, 54, 0.5f, rgb(0xff8a8a), 1, true);
      r_->text_wrap(tr("pokesurvivor.com está limitando el gacha sin conexión para evitar a la gente que hace trampas. Estamos tratando de solucionarlo: disculpa las molestias."),
                    160, 82, 0.42f, C_TEXT, 268, 1);
      r_->text_wrap(tr("Conéctate a internet y vuelve a entrar en el gacha."), 160, 154, 0.36f, C_GOLD, 268, 1);
      r_->text(tr("A o B: volver al menú"), 160, 182, 0.32f, C_MUTED, 1);
    }
    return;
  }
  if (pulling_) {   // tirando en el servidor de pokesurvivor.com
    C2D_DrawRectSolid(0, 0, 0, 320, 240, C2D_Color32(6, 10, 20, 200));
    rrect(40, 92, 240, 56, C2D_Color32(24, 34, 60, 250));
    rframe(40, 92, 240, 56, rgb(0xffd23f));
    const int dots = int(pull_wait_t_ * 3) % 4;
    char w[64];
    std::snprintf(w, sizeof(w), "%s%.*s", tr("Tirando en pokesurvivor.com"), dots, "...");
    r_->text(w, 160, 102, 0.42f, C_GOLD, 1);
    r_->text(tr("Con tu cuenta, el gacha va por su servidor"), 160, 124, 0.3f, C_MUTED, 1);
  }
}

void App::top_pull() {
  // Tragaperras (slots.js, simplificada): palanca, 3 rodillos con Poké Balls
  // de la rareza y un letrero luminoso.
  C2D_DrawRectSolid(0, 0, 0, TW, TH, C2D_Color32(10, 6, 20, 230));
  const float mx = 120, my = 36;
  C2D_DrawRectSolid(mx, my, 0, 160, 150, rgb(0xc8323c));
  C2D_DrawRectSolid(mx + 4, my + 4, 0, 152, 4, rgb(0xef6464));
  C2D_DrawRectSolid(mx + 10, my + 14, 0, 140, 22, rgb(0x2a0f14));
  const PullResult& r = npulls_ == 100 && pull_done_ ? pulls_[sum_[0]] : spin(pull_i_);   // (×100 acabada: lo mejor de todo)
  const bool ten = npulls_ >= 10;
  const bool big = r.rarity >= 4 || r.shiny;
  const float t = pull_t_;
  const float spin_end = ten ? 0.25f : 0.6f;
  static const char* const BALL[6] = {"ball", "ball", "ball2", "ball3", "ballLux", "ball4"};
  const char* sym = r.shiny ? "shiny" : BALL[r.rarity];
  char led[64];
  bool stopped[3];
  for (int k = 0; k < 3; ++k) {
    const float stop = spin_end + k * (ten ? 0.08f : 0.32f) + (k == 2 && big ? (ten ? 0.5f : 1.1f) : 0);
    stopped[k] = pull_done_ || t >= stop;
    const float x = mx + 16 + k * 46, y = my + 50;
    C2D_DrawRectSolid(x, y, 0, 36, 46, rgb(0xf4efe2));
    const char* s = stopped[k] ? sym : BALL[1 + int(t * 20 + k * 3) % 5];
    const int ic = r_->icons.find(s);
    if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), x + 6, y + 11, 0, nullptr, 2, 2);
  }
  if (stopped[2]) std::snprintf(led, sizeof(led), "%s%s", r.shiny ? "SHINY · " : "", kRarityName[r.rarity]);
  else if (stopped[1] && big) std::snprintf(led, sizeof(led), "%s", r.shiny ? "¡¿SHINY?!" : r.rarity == 5 ? "¡¿LEGENDARIO?!" : "¡¿ÉPICO?!");
  else if (npulls_ == 100) std::snprintf(led, sizeof(led), "×10 · %d/10", pull_i_ + 1);
  else if (ten) std::snprintf(led, sizeof(led), "TIRADA %d/10", pull_i_ + 1);
  else std::snprintf(led, sizeof(led), "¡SUERTE!");
  r_->text(led, mx + 80, my + 18, 0.4f, stopped[2] ? rgb(r.shiny ? 0x9ae6ff : kRarityColor[r.rarity]) : rgb(0xffe9a0), 1);
  // Palanca.
  const float lev = t < 0.2f ? t / 0.2f : t < 0.4f ? 1 - (t - 0.2f) / 0.2f : 0;
  C2D_DrawRectSolid(mx + 164, my + 30 + lev * 40, 0, 6, 50 - lev * 40, rgb(0xc9d2de));
  C2D_DrawCircleSolid(mx + 167, my + 26 + lev * 40, 0, 7, rgb(0xff5f6d));
  // El premio, cuando para.
  if (stopped[2]) {
    draw_mon(r.dex, r.shiny, mx + 80, my + 140, 2);
    char b[96];
    std::snprintf(b, sizeof(b), "%s%s", r.shiny ? "★ " : "", mon_by_dex(r.dex)->name);
    r_->text(b, 200, 196, 0.5f, C_TEXT, 1);
    if (r.is_new) r_->text(r.shiny ? "¡SHINY NUEVO!" : "¡NUEVO!", 200, 216, 0.42f, C_GOLD, 1);
    // Nuevo: con qué naturaleza y habilidad le ha tocado (la oculta, en dorado), como la web.
    if (r.is_new) {
      const AbilPick ab = abil_of(*s_, r.dex);
      C2D_DrawRectSolid(292, 92, 0, 104, 80, C2D_Color32(20, 12, 34, 220));
      C2D_DrawRectSolid(292, 92, 0, 104, 2, rgb(kRarityColor[r.rarity]));
      r_->text(tr("Naturaleza"), 344, 98, 0.3f, C_MUTED, 1);
      r_->text_fit(nature_name(nature_of(*s_, r.dex)), 344, 110, 98, 0.38f, C_TEXT, false, 1);
      r_->text(tr("Habilidad"), 344, 132, 0.3f, C_MUTED, 1);
      r_->text_fit(ab.id >= 0 ? abil_name(ab.id) : "—", 344, 144, 98, 0.38f, ab.slot == 2 ? C_GOLD : C_TEXT, false, 1);
      if (ab.slot == 2) r_->text(tr("oculta"), 344, 158, 0.28f, C_GOLD, 1);
    }
    else { std::snprintf(b, sizeof(b), "Repetido · +%d", r.refund); r_->text(b, 200, 216, 0.38f, C_MUTED, 1); }
  }
}

void App::bottom_pull() {
  char b[96];
  if (npulls_ == 100 && pull_done_) {
    // Resumen de la ×100: nuevos, shiny y Pokémonedas; las cartas de mejor a peor.
    int nn = 0, ns = 0, coins = 0;
    for (int i = 0; i < 100; ++i) { nn += pulls_[i].is_new; ns += pulls_[i].shiny; coins += pulls_[i].refund; }
    header("Resumen de la ×100");
    std::snprintf(b, sizeof(b), "%d nuevos · %d shiny · +%d Pokémonedas", nn, ns, coins);
    r_->text_fit(b, 160, 26, 304, 0.4f, C_GOLD, true, 1);
    constexpr int COLS = 8;
    for (int row = 0; row < 3; ++row)
      for (int c = 0; c < COLS; ++c) {
        const int k = (sum_scroll_ + row) * COLS + c;
        if (k >= 100) break;
        const PullResult& r = pulls_[sum_[k]];
        const float x = 6 + c * 39, y = 44 + row * 58;
        C2D_DrawRectSolid(x, y, 0, 37, 55, C_PANEL);
        C2D_DrawRectSolid(x, y, 0, 37, 3, rgb(r.shiny ? 0x9ae6ff : kRarityColor[r.rarity]));
        draw_mon(r.dex, r.shiny, x + 18, y + 40, 1);
        if (r.is_new) r_->text(r.shiny ? "SHINY" : "NUEVO", x + 18, y + 43, 0.26f, C_GOLD, 1, false);
        else { std::snprintf(b, sizeof(b), "+%d", r.refund); r_->text(b, x + 18, y + 43, 0.26f, C_MUTED, 1, false); }
      }
    std::snprintf(b, sizeof(b), "Arriba/abajo: más cartas (%d-%d de 100) · A: cerrar", sum_scroll_ * COLS + 1, std::min(100, (sum_scroll_ + 3) * COLS));
    r_->text(b, 160, 226, 0.32f, C_MUTED, 1);
    return;
  }
  header(npulls_ == 100 ? "Lo mejor de cada ×10" : "Resultados");
  for (int i = 0; i <= pull_i_ && i < nspins(); ++i) {
    const PullResult& r = spin(i);
    const bool shown = pull_done_ || i < pull_i_;
    const float x = 6 + (i % 5) * 62, y = 30 + (i / 5) * 92;
    C2D_DrawRectSolid(x, y, 0, 58, 86, C_PANEL);
    if (!shown) continue;
    C2D_DrawRectSolid(x, y, 0, 58, 3, rgb(r.shiny ? 0x9ae6ff : kRarityColor[r.rarity]));
    draw_mon(r.dex, r.shiny, x + 29, y + 50, 1);
    std::snprintf(b, sizeof(b), "%s", std::string(size_t(r.rarity), '*').c_str());
    r_->text(b, x + 29, y + 4, 0.32f, rgb(kRarityColor[r.rarity]), 1);
    r_->text(mon_by_dex(r.dex)->name, x + 29, y + 56, 0.28f, C_TEXT, 1, false);
    if (r.is_new) r_->text(r.shiny ? "¡SHINY!" : "¡NUEVO!", x + 29, y + 70, 0.3f, C_GOLD, 1, false);
    else { std::snprintf(b, sizeof(b), "+%d", r.refund); r_->text(b, x + 29, y + 70, 0.3f, C_MUTED, 1, false); }
  }
  r_->text(pull_done_ ? "A: cerrar" : "A: más rápido", 160, 226, 0.34f, C_MUTED, 1);
}

// ---------------- misiones, logros y estadísticas ----------------

void App::in_goals(const UiIn& in) {
  if (in.down & KEY_L) { tab_ = (tab_ + 2) % 3; cur_ = scroll_ = 0; }
  if (in.down & KEY_R) { tab_ = (tab_ + 1) % 3; cur_ = scroll_ = 0; }
  if (in.down & KEY_B) { go(SC_MENU); return; }
  if (tab_ == 0) {
    progress_ensure(*s_, now_ms());
    if (in.rep & KEY_DUP) cur_ = (cur_ + 4) % 5;
    if (in.rep & KEY_DDOWN) cur_ = (cur_ + 1) % 5;
    for (int i = 0; i < 5; ++i) if (in_rect(&in, 6, 40 + i * 36 + (i >= 3 ? 12 : 0), 308, 34)) cur_ = i;
    if (in.down & KEY_A) { if (progress_claim(*s_, cur_ < 3 ? cur_ : cur_ - 3, cur_ >= 3, now_ms())) save_now(); }
    if ((in.down & KEY_Y) && cur_ < 3) { if (progress_reroll(*s_, cur_, now_ms())) save_now(); }
  } else if (tab_ == 1) {
    if (in.rep & KEY_DUP) cur_ = std::max(0, cur_ - 1);
    if (in.rep & KEY_DDOWN) cur_ = std::min(kAchCount - 1, cur_ + 1);
    if (cur_ < scroll_) scroll_ = cur_;
    if (cur_ >= scroll_ + 6) scroll_ = cur_ - 5;
  }
}

void App::bottom_goals() {
  static const char* const T[3] = {"Misiones", "Logros", "Estadísticas"};
  char b[160];
  std::snprintf(b, sizeof(b), "%s  (L/R)", tr(T[tab_]));
  header(b);
  if (tab_ == 0) {
    progress_ensure(*s_, now_ms());
    r_->text("Diarias", 8, 26, 0.36f, C_MUTED);
    r_->text("Semanales", 8, 26 + 3 * 36 + 12, 0.36f, C_MUTED);
    for (int i = 0; i < 5; ++i) {
      const Mission& m = i < 3 ? s_->daily[i] : s_->weekly[i - 3];
      const float y = 40 + i * 36 + (i >= 3 ? 12 : 0);
      C2D_DrawRectSolid(6, y, 0, 308, 34, i == cur_ ? C_SEL : C_PANEL);
      if (m.def < 0) continue;
      mission_text(m, b, sizeof(b));
      fit_text(b, 12, y + 1, 296, 0.4f, m.claimed ? C_MUTED : C_TEXT);
      const float f = std::min(1.0f, float(m.prog) / std::max(1, int(m.goal)));
      C2D_DrawRectSolid(12, y + 22, 0, 180, 6, C2D_Color32(0, 0, 0, 140));
      C2D_DrawRectSolid(12, y + 22, 0, 180 * f, 6, mission_done(m) ? C2D_Color32(0x5f, 0xe0, 0x8a, 255) : C_GOLD);
      std::snprintf(b, sizeof(b), "%d/%d", int(std::min(m.prog, m.goal)), int(m.goal));
      r_->text(b, 196, y + 18, 0.3f, C_MUTED, 0, false);
      const Reward& rw = kMissions[m.def].reward;
      if (m.claimed) std::snprintf(b, sizeof(b), "Recogido");
      else if (int(rw.t10)) std::snprintf(b, sizeof(b), "%d ×10 · %d", int(rw.t10), int(rw.coins));
      else std::snprintf(b, sizeof(b), "%d tk · %d", int(rw.t1), int(rw.coins));
      r_->text(b, 308, y + 18, 0.3f, mission_done(m) && !m.claimed ? C_GOLD : C_MUTED, 2, false);
    }
    r_->text(s_->m_rerolled ? "A: recoger premio" : "A: recoger · Y: cambiar una diaria (1 al día)", 160, 226, 0.32f, C_MUTED, 1);
  } else if (tab_ == 1) {
    int got = 0;
    for (int i = 0; i < kAchCount; ++i) if (s_->ach[i]) got++;
    std::snprintf(b, sizeof(b), "%d de %d logros", got, kAchCount);
    r_->text(b, 160, 25, 0.36f, C_MUTED, 1);
    for (int i = 0; i < 6; ++i) {
      const int k = scroll_ + i;
      if (k >= kAchCount) break;
      const AchDef& a = kAch[k];
      const float y = 42 + i * 30;
      C2D_DrawRectSolid(6, y, 0, 308, 28, k == cur_ ? C_SEL : C_PANEL);
      char medal[12];
      std::snprintf(medal, sizeof(medal), "medal%d", s_->ach[k] ? a.tier : 0);
      const int ic = r_->icons.find(s_->ach[k] ? medal : "medalOff");
      if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), 10, y + 2, 0, nullptr, 2, 2);
      fit_text(a.name, 38, y, 230, 0.4f, s_->ach[k] ? C_TEXT : C_MUTED);
      fit_text(a.desc, 38, y + 16, 230, 0.28f, C_MUTED, false);
      std::snprintf(b, sizeof(b), "%d/%d", std::min(ach_value(*s_, k), int(a.goal)), int(a.goal));
      r_->text(b, 308, y + 8, 0.32f, s_->ach[k] ? C_GOLD : C_MUTED, 2, false);
    }
  } else {
    const SaveStats& S = s_->stats;
    char t1[16], t2[16];
    mmss(float(S.best_time), t1, sizeof(t1));
    mmss(float(S.judge_time), t2, sizeof(t2));
    const char* lines[] = {"Partidas", "Mejor tiempo", "Mejor nivel", "Derrotados", "Jefes derrotados", "Grietas ganadas",
                           "Evoluciones", "Misiones completadas", "Tiradas", "Shinies", "Monedas ganadas", "Veces ante Arceus", "Mejor tiempo ante Arceus"};
    char vals[13][24];
    std::snprintf(vals[0], 24, "%d", int(S.runs)); std::snprintf(vals[1], 24, "%s", t1); std::snprintf(vals[2], 24, "%d", int(S.best_level));
    std::snprintf(vals[3], 24, "%d", int(S.total_kills)); std::snprintf(vals[4], 24, "%d", int(S.bosses_total)); std::snprintf(vals[5], 24, "%d", int(S.rift_wins));
    std::snprintf(vals[6], 24, "%d", int(S.evolutions)); std::snprintf(vals[7], 24, "%d", int(S.missions_done)); std::snprintf(vals[8], 24, "%d", int(S.pulls));
    std::snprintf(vals[9], 24, "%d", s_->shiny_count()); std::snprintf(vals[10], 24, "%d", int(S.coins_earned)); std::snprintf(vals[11], 24, "%d", int(S.judged));
    std::snprintf(vals[12], 24, "%s", t2);
    for (int i = 0; i < 13; ++i) {
      r_->text(lines[i], 16, 28 + i * 15, 0.34f, C_MUTED);
      r_->text(vals[i], 304, 28 + i * 15, 0.34f, C_TEXT, 2);
    }
  }
}

// ---------------- ajustes ----------------

// Filas de Ajustes (10: caben con 19 px cada una).
constexpr int kSetY = 24, kSetStep = 19, kSetH = 17;

void App::in_settings(const UiIn& in) {
  const int n = 10;
  if (in.rep & KEY_DUP) cur_ = (cur_ + n - 1) % n;
  if (in.rep & KEY_DDOWN) cur_ = (cur_ + 1) % n;
  for (int i = 0; i < n; ++i) if (in_rect(&in, 6, kSetY + i * kSetStep, 308, kSetH)) cur_ = i;
  int d = 0;
  if (in.rep & KEY_DLEFT) d = -1;
  if (in.rep & (KEY_DRIGHT | KEY_A)) d = 1;
  if (in.tap && in_rect(&in, 6, kSetY + cur_ * kSetStep, 308, kSetH)) d = 1;
  if (d) {
    switch (cur_) {
      case 0: st_->lang ^= 1; break;
      case 1: st_->quality = ((st_->quality + 1 + d + 4) % 4) - 1; break;
      case 2: st_->dmg_numbers = !st_->dmg_numbers; break;
      case 3: st_->calm = !st_->calm; break;
      case 4: st_->fps = !st_->fps; break;
      case 5: st_->music = clamp(st_->music + d * 10, 0, 100); break;
      case 6: st_->sfx = clamp(st_->sfx + d * 10, 0, 100); break;
      case 7: st_->stereo = !st_->stereo; break;
      case 8: settings_write(dir_, *st_); go(SC_ACCOUNT); return;
      case 9:   // la pantalla de actualizaciones (los datos van con cada versión: 0.26.2)
        settings_write(dir_, *st_); update_back_ = SC_SETTINGS; go(SC_UPDATE); return;
    }
    apply_settings();
  }
  if (in.down & KEY_B) { settings_write(dir_, *st_); go(SC_MENU); }
}

void App::bottom_settings() {
  header("Ajustes");
  static const char* const L[10] = {"Idioma", "Calidad", "Números de daño", "Menos sacudidas", "Mostrar FPS", "Música", "Efectos", "3D", "Cuenta web",
                                    "Actualizaciones"};
  char v[10][24];
  std::snprintf(v[0], 24, "%s", st_->lang == 1 ? "English" : "Español");
  static const char* const Q[4] = {"Auto", "Baja", "Media", "Alta"};
  if (st_->quality < 0) {   // en automático, qué modelo ha detectado
    bool n3ds = false;
    APT_CheckNew3DS(&n3ds);
    std::snprintf(v[1], 24, "%s · %s", tr("Auto"), n3ds ? "New 3DS" : "3DS");
  } else {
    std::snprintf(v[1], 24, "%s", tr(Q[st_->quality + 1]));
  }
  std::snprintf(v[2], 24, "%s", tr(st_->dmg_numbers ? "Sí" : "No"));
  std::snprintf(v[3], 24, "%s", tr(st_->calm ? "Sí" : "No"));
  std::snprintf(v[4], 24, "%s", tr(st_->fps ? "Sí" : "No"));
  std::snprintf(v[5], 24, "%d %%", st_->music);
  std::snprintf(v[6], 24, "%d %%", st_->sfx);
  std::snprintf(v[7], 24, "%s", tr(st_->stereo ? "Sí" : "No"));
  std::snprintf(v[8], 24, "%.23s", cloud3ds::account().on ? cloud3ds::account().name.c_str() : tr("Sin conectar"));
  std::snprintf(v[9], 24, "%s (A)", kVersion);
  for (int i = 0; i < 10; ++i) {
    const float y = kSetY + i * kSetStep;
    C2D_DrawRectSolid(6, y, 0, 308, kSetH, i == cur_ ? C_SEL : C_PANEL);
    // (el texto largo, un poco más pequeño para que quepa con su valor)
    const float sz = r_->text_width(tr(L[i]), 0.38f) > 250 ? 0.3f : 0.38f;
    r_->text(L[i], 14, y + (sz < 0.38f ? 4 : 2), sz, C_TEXT);
    r_->text(v[i], 306, y + 2, 0.38f, C_GOLD, 2, false);
  }
  // De qué versión de la web son los datos (reference/web/UPSTREAM, por gen_data).
  char ver[96];
  const DataPackInfo& dp = datapack_current();
  std::snprintf(ver, sizeof(ver), "Versión %s · datos de la web del %s", kVersion, dp.on ? dp.fecha.c_str() : kWebBaseDate);
  r_->text(ver, 160, 215, 0.28f, C_MUTED, 1);
  r_->text("Izquierda/derecha: cambiar · B: guardar y volver", 160, 226, 0.32f, C_MUTED, 1);
}

// Píldora con los tickets (normal y ×10) y su icono, centrada en cx.
void App::top_tickets_badge(float cx, float y) {
  const int i1 = r_->icons.find("ticket"), i10 = r_->icons.find("ticket10");
  char a[16], b[16];
  std::snprintf(a, sizeof(a), "%d", int(s_->t1));
  std::snprintf(b, sizeof(b), "%d", int(s_->t10));
  const float k = 1.5f, isz = 12 * k;
  const float wa = r_->text_width(a, 0.5f), wb = r_->text_width(b, 0.5f);
  const float w = 10 + isz + 4 + wa + 14 + isz + 4 + wb + 10, h = 24;
  const float x = std::floor(cx - w / 2);
  C2D_DrawRectSolid(x + 2, y, 0, w - 4, h, C2D_Color32(14, 18, 34, 215));
  C2D_DrawRectSolid(x, y + 2, 0, w, h - 4, C2D_Color32(14, 18, 34, 215));
  C2D_DrawRectSolid(x + 2, y, 0, w - 4, 1, C_GOLD);
  C2D_DrawRectSolid(x + 2, y + h - 1, 0, w - 4, 1, C_GOLD);
  C2D_DrawRectSolid(x, y + 2, 0, 1, h - 4, C_GOLD);
  C2D_DrawRectSolid(x + w - 1, y + 2, 0, 1, h - 4, C_GOLD);
  float px = x + 10;
  if (i1 >= 0) C2D_DrawImageAt(r_->icons.image(i1), px, y + 3, 0, nullptr, k, k);
  px += isz + 4;
  r_->text(a, px, y + 3, 0.5f, s_->t1 > 0 ? C_TEXT : C_MUTED);
  px += wa + 14;
  if (i10 >= 0) C2D_DrawImageAt(r_->icons.image(i10), px, y + 3, 0, nullptr, k, k);
  px += isz + 4;
  r_->text(b, px, y + 3, 0.5f, s_->t10 > 0 ? C_GOLD : C_MUTED);
}

void App::bottom_credits() {
  if (credits_lang_ != st_->lang) load_credits();   // (se ha cambiado de idioma)
  header("Créditos");
  const char* p = credits_;
  int line = 0;
  float y = 28;
  for (int i = 0; p && i < credit_lines_; ++i) {
    if (line >= scroll_ && y < 220) {
      const bool title = p[0] == '#';
      r_->text(title ? p + 2 : p, 8, y, title ? 0.4f : 0.32f, title ? C_GOLD : C_TEXT, 0, false);
      y += title ? 16 : 13;
    }
    line++;
    p += std::strlen(p) + 1;
  }
  r_->text("Arriba/abajo: desplazar · B: volver", 160, 226, 0.32f, C_MUTED, 1);
}

// ---------------- dibujo ----------------

void App::draw_top(float eye) {
  if (scene == SC_RUN) {
    // Caído en grupo: la cámara sigue al compañero elegido con L/R (sólo en
    // esta consola: el núcleo la vuelve a poner en ti en cada paso).
    if (spect_ >= 0 && spect_ < g_->nplayers && spect_ != g_->local && g_->me().dead) {
      g_->camera.x = g_->players[spect_].view_x;
      g_->camera.y = g_->players[spect_].view_y;
    }
    if (still_now_) g_->players[g_->local].anim.dir = int8_t(dir_from_angle(kPi / 2));   // (grabación: mirando abajo)
    r_->draw_top(*g_, eye); coop_overlay();
    if (g_->state == ST_OVER) r_->draw_over_summary(*g_, over_tab_ < 0 ? g_->local : over_tab_);   // (la insignia de la nube, sólo en el menú: aquí tapaba el resumen)
  }
  else {
    if (eye <= 0) C2D_TextBufClear(r_->tbuf);
    draw_backdrop(scene == SC_MENU ? 0.15f : 0.45f, eye);
    switch (scene) {
      case SC_TEST:
        r_->text_wrap(kTest[q_].q, 200, 80, 0.6f, C_TEXT, 360, 1);
        break;
      case SC_RESULT: case SC_STARTERS: {
        const int dex = scene == SC_RESULT ? res_dex_ : kStarters[cur_].dex;
        C2D_DrawEllipseSolid(130, 172, 0, 140, 36, rgbf(kTypeColor[mon_by_dex(dex)->types[0]], 0.35f));
        draw_mon(dex, false, 200, 196, 3);
        r_->text(mon_by_dex(dex)->name, 200, 14, 0.7f, C_TEXT, 1);
        if (scene == SC_RESULT) {
          char b[64];
          std::snprintf(b, sizeof(b), "%s", kTraits[res_trait_].id);
          b[0] = char(std::toupper(b[0]));
          r_->text(b, 200, 44, 0.5f, C_GOLD, 1);
          r_->text_wrap(kTraits[res_trait_].text, 200, 66, 0.4f, C_TEXT, 340, 1);
        } else {
          const StarterDef& s = kStarters[cur_];
          char b[128];
          std::snprintf(b, sizeof(b), "Empieza con %s · %s, %s", kMoves[s.start].name, s.traits[0], s.traits[1]);
          r_->text_fit(b, 200, 44, 390, 0.38f, C_GOLD, true, 1);
        }
        break;
      }
      case SC_SHOP: top_shop(); break;
      case SC_GIFT: top_gift(); break;
      case SC_MODE: {   // (tu compañero y el reloj de la Sentencia del modo elegido)
        if (cur_ == kModeWeekly) {   // el desafío de la semana: su Pokémon, su condición y cuánto le queda
          const Weekly w = weekly_now();
          const Mon* wm = mon_by_dex(w.dex);
          C2D_DrawRectSolid(0, 0, 0, TW, TH, C2D_Color32(20, 10, 40, 120));
          C2D_DrawEllipseSolid(30, 176, 0, 160, 34, rgbf(wm ? kTypeColor[wm->types[0]] : 0xc47bff, 0.32f));
          draw_mon(w.dex, false, 110, 196, 3);
          r_->text(tr("DESAFÍO SEMANAL"), 200, 8, 0.62f, C2D_Color32(0xe2, 0xc4, 0xff, 255), 1);
          char lb[96], b[256];
          weekly_label(w, lb, sizeof(lb));
          r_->text_fit(tr(lb), 296, 40, 196, 0.5f, C_GOLD, true, 1);
          weekly_desc(w, b, sizeof(b));
          r_->text_wrap(tr(b), 296, 62, 0.36f, C_TEXT, 192, 1);
          std::snprintf(b, sizeof(b), "%s · %s", wm ? tr(wm->name) : "?", tr("sin mejoras, naturaleza neutra"));
          r_->text_fit(b, 296, 140, 196, 0.36f, C_MUTED, false, 1);
          const int64_t left = weekly_ends_in(now_ms());
          const int dd = int(left / 86400000), hh = int(left / 3600000 % 24);
          std::snprintf(b, sizeof(b), tr("Termina en %d d %d h"), dd, hh);
          r_->text(b, 296, 160, 0.4f, C2D_Color32(0xff, 0x9a, 0x4d, 255), 1);
          const bool prize = !std::strcmp(s_->wch, w.week);
          r_->text_fit(prize ? tr("Ticket ×10 de esta semana: ¡conseguido!") : tr("Aguanta 3 minutos: ticket ×10"), 296, 180, 196, 0.36f,
                       prize ? C2D_Color32(0x5f, 0xe0, 0x8a, 255) : C_GOLD, false, 1);
          r_->text_fit(tr("No suma maestría"), 296, 198, 196, 0.32f, C_MUTED, false, 1);
          break;
        }
        const int dex = s_->partner;
        const bool sh = s_->partner_shiny && s_->owns_shiny(dex);
        C2D_DrawEllipseSolid(140, 186, 0, 120, 30, rgbf(sh ? 0x9ae6ff : kTypeColor[mon_by_dex(dex)->types[0]], 0.3f));
        draw_mon(dex, sh, 200, 204, 3);
        r_->text(cur_ == MODE_QUICK ? "20:00" : "30:00", 200, 18, 1.0f, cur_ == MODE_QUICK ? C2D_Color32(0xff, 0x9a, 0x4d, 255) : C_GOLD, 1);
        r_->text(cur_ == MODE_QUICK ? "Modo rápido" : "Modo normal", 200, 58, 0.5f, C_TEXT, 1);
        if (cur_ == MODE_QUICK) {   // (aquí Arceus se puede derrotar: con su medalla)
          r_->draw_medal(118, 92, 8);
          r_->text("¡Arceus se puede derrotar!", 210, 84, 0.45f, C_GOLD, 1);
        } else r_->text("Arceus es invencible: aguanta todo lo que puedas", 200, 84, 0.38f, C_MUTED, 1);
        break;
      }
      case SC_MENU: {
        const int dex = s_->partner;
        const bool sh = s_->partner_shiny && s_->owns_shiny(dex);
        C2D_DrawEllipseSolid(140, 186, 0, 120, 30, rgbf(sh ? 0x9ae6ff : kTypeColor[mon_by_dex(dex)->types[0]], 0.3f));
        const int psk = skin_worn(*s_, dex);
        draw_mon(psk >= 0 ? kSkins[psk].sprite : dex, sh, 200, 204, 3);
        char b[64];
        std::snprintf(b, sizeof(b), "%s%s", mon_by_dex(dex)->name, sh ? " ★" : "");
        // (con la insignia de la cuenta arriba a la izquierda, el nombre más abajo)
        const float ny = cloud3ds::account().on ? 54 : 10;
        // (arriba, si es largo, se aparta a la izquierda de la Tienda Kecleon: top_shop_badge)
        const float nw = r_->text_width(b, 0.7f) + (dex >= 0 && dex <= kMaxDex && s_->arceus_win[dex] > 0 ? 24 : 0);
        const float nx = ny < 50 ? std::max(6 + nw / 2, std::min(200.0f, TW - 4 - 144 - 6 - nw / 2)) : 200;
        r_->text(b, nx, ny, 0.7f, C_TEXT, 1);
        // Medalla de Arceus de tu compañero, al lado de su nombre.
        if (dex >= 0 && dex <= kMaxDex && s_->arceus_win[dex] > 0) r_->draw_medal(nx + r_->text_width(b, 0.7f) / 2 + 14, ny + 14, 7);
        // Su tipo (o sus dos tipos), como en la Pokédex.
        const Mon* pm = mon_by_dex(dex);
        for (int k = 0; k < 2; ++k) {
          if (pm->types[k] < 0) continue;
          const float x = nx + (pm->types[1] >= 0 ? -50 + k * 52 : -23);
          C2D_DrawRectSolid(x, ny + 26, 0, 46, 14, rgb(kTypeColor[pm->types[k]]));
          r_->text(kTypeName[pm->types[k]], x + 23, ny + 26, 0.32f, C_TEXT, 1);
        }
        r_->text("PokéSurvivor 3DS · port de juanluismm520", 200, 222, 0.34f, C2D_Color32(255, 255, 255, 140), 1, false);
        // Halloween: cuenta atrás (desde dos semanas antes, cuánto falta; durante, cuánto queda).
        {
          bool on; int64_t ms;
          if (hw3ds::countdown(on, ms)) {
            char t[32], cd[80];
            hw_time(ms, t, sizeof(t));
            std::snprintf(cd, sizeof(cd), on ? "Halloween termina en %s" : "Halloween empieza en %s", t);
            // (con megapiedras, a la izquierda de su panel, que está abajo a la derecha: ver mega_badge)
            const int nm = mega_count(dex);
            const float right = nm ? TW - 8 - std::max(92.0f, nm * 26.0f + (nm - 1) * 14.0f + 24) - 6 : TW - 6;
            const float cx = nm ? (6 + 18 + right) / 2 : 200, maxw = right - 6 - 18;
            const float w = r_->text_fit(cd, cx, 200, maxw, 0.38f, C2D_Color32(255, 160, 80, 255), false, 1);
            const int pi = r_->ui_icon[17];
            if (pi >= 0) C2D_DrawImageAt(r_->icons.image(pi), cx - w / 2 - 16, 202, 0);
          }
        }
        top_shop_badge();                                                 // (arriba a la derecha: la Tienda Kecleon, R)
        if (s_->owns_shiny(dex)) y_hint(TW - 6 - y_hint_w(), 56, sh);  // (debajo, sin salirse)
        mega_badge(dex);
        top_cloud_badge();
        break;
      }
      case SC_DEX: {
        static int16_t list[256];
        const int n = dex_list(*s_, gen_, filter_, list);
        if (n && dex_moves_) top_ficha(list[std::min(cur_, n - 1)]);
        else if (n) top_detail(list[std::min(cur_, n - 1)], view_shiny_, s_->owns(list[std::min(cur_, n - 1)]));
        break;
      }
      case SC_UPGRADES: case SC_BIND: case SC_BAN: case SC_START: case SC_MOVE2: {
        const bool sh = up_dex_ == s_->partner && s_->partner_shiny && s_->owns_shiny(up_dex_);
        const bool pick = scene == SC_START || scene == SC_MOVE2;   // (el ataque a la derecha)
        draw_mon(up_dex_, sh, pick ? 96 : 200, 200, 3);
        r_->text(mon_by_dex(up_dex_)->name, pick ? 96 : 200, 14, 0.6f, C_TEXT, 1);
        if (scene == SC_START || scene == SC_MOVE2) {
          int16_t ids[kMaxMoves];
          const int n = scene == SC_START ? start_choices_sorted(*s_, up_dex_, ids) : move2_choices_sorted(*s_, up_dex_, ids);
          const int row = scene == SC_START ? cur_ : cur_ - 1;   // (en «Repertorio», la fila 0 es «al azar»)
          if (row >= 0 && row < n) top_pick_move(ids[row]);
        }
        if (scene == SC_UPGRADES && cur_ < UP_COUNT) r_->text_wrap(kUpgrades[cur_].unit, 200, 44, 0.42f, C_GOLD, 340, 1);
        else if (scene == SC_UPGRADES && cur_ == four_row())
          r_->text_wrap("Desde el nivel 25 (o con 8 movimientos), al menos 2 de las 4 cartas son pasivas: poder, velocidad, defensa, recarga… Desde el 40, al menos 3.",
                        200, 40, 0.38f, C_GOLD, 360, 1);
        else if (scene == SC_UPGRADES && cur_ == UP_COUNT && has_mega(up_dex_)) {
          // (sus megas: con dos, X e Y o normal y Z)
          std::string ms;
          for (int k = 0; k < 2; ++k) {
            const int mi = mega_index(up_dex_, k);
            if (mi < 0) continue;
            if (!ms.empty()) ms += " o ";
            ms += kMegas[mi].mon.name;
            ms += " (" + std::to_string(int(mega_boost(mi).time)) + " s)";
          }
          r_->text_wrap(("Su megapiedra: Y en la partida y se convierte en " + ms).c_str(), 200, 44, 0.42f, C_GOLD, 340, 1);
        }
        break;
      }
      case SC_GACHA: {
        const BannerDef& bn = gacha_banner(bgen_);
        C2D_DrawRectangle(0, 0, 0, TW, TH, rgb(bn.color[0], 230), rgb(bn.color[1], 230), rgb(bn.color[1], 230), rgb(bn.color[2], 230));
        char b[64];
        if (bgen_ == kGachaAll) std::snprintf(b, sizeof(b), "Todas las regiones");
        else if (bgen_ == kGachaSpooky) std::snprintf(b, sizeof(b), "Noche Espectral");
        else std::snprintf(b, sizeof(b), "%s Misterioso", bn.name);
        r_->text(b, 200, 12, 0.7f, C_GOLD, 1);
        // Flechas a los lados: L / R cambian de región.
        {
          const bool ev = hw3ds::on();
          const int prev = gacha_prev(bgen_, ev), next = gacha_next(bgen_, ev);
          const u32 a = C2D_Color32(255, 255, 255, 200);
          C2D_DrawTriangle(6, 15, a, 13, 9, a, 13, 21, a, 0);
          C2D_DrawTriangle(394, 15, a, 387, 9, a, 387, 21, a, 0);
          char nb[48];
          std::snprintf(nb, sizeof(nb), "L · %s", gacha_banner(prev).name);
          r_->text(nb, 17, 10, 0.32f, C_TEXT, 0);
          std::snprintf(nb, sizeof(nb), "%s · R", gacha_banner(next).name);
          r_->text(nb, 383, 10, 0.32f, C_TEXT, 2);
          std::snprintf(nb, sizeof(nb), "%d / %d", bgen_ == kGachaAll ? 10 : bgen_ == kGachaSpooky ? 11 : bgen_, ev ? 11 : 10);
          r_->text(nb, 200, 34, 0.32f, C_TEXT, 1);
        }
        top_tickets_badge(200, 50);
        int16_t f[3];
        const int nf = gacha_featured(bgen_, f);
        for (int i = 0; i < nf; ++i) draw_mon(f[i], false, 90 + i * 110, 170, 2, 0, 1);
        if (gacha_locked()) {   // (con la cuenta: bloqueada, bien a la vista)
          C2D_DrawRectSolid(0, 76, 0, TW, 104, C2D_Color32(10, 12, 24, 170));
          const u32 red = rgb(0xff6b6b);
          rrect(110, 104, 180, 44, C2D_Color32(40, 22, 32, 240));
          rframe(110, 104, 180, 44, red);
          padlock(134, 113, 1.6f, red);
          r_->text_fit("Bloqueada", 220, 109, 130, 0.6f, rgb(0xff8a8a), true, 1);
          r_->text_fit("con cuenta online, para evitar trampas", 200, 152, 300, 0.36f, C_TEXT, true, 1);
        }
        for (int r = 5; r >= 1; --r) {
          static int16_t pool[1100];
          std::snprintf(b, sizeof(b), "%s %.1f%% · %d", kRarityName[r], double(kGachaRates[r - 1]), gacha_pool(r, bgen_, pool));
          r_->text(b, 10, 180 + (5 - r) * 11, 0.3f, rgb(kRarityColor[r]), 0, false);
        }
        r_->text(bgen_ == kGachaSpooky ? "Shiny 2% (Halloween)" : gacha_has_iris(*s_, bgen_) ? "Shiny 2% (Amuleto Iris)" : "Shiny 1%", 390, 224, 0.3f, rgb(0x9ae6ff), 2, false);
        break;
      }
      case SC_PULL: top_pull(); break;
      case SC_NEWS: top_news(eye); break;
      default: break;
    }
  }
  // La vez anterior el juego se rompió: qué pasó (para una foto) y dónde está el log.
  if (crash3ds::last()[0] && !crash_seen_ && scene != SC_RUN) {
    C2D_DrawRectSolid(10, 150, 0, 380, 82, C2D_Color32(60, 16, 24, 240));
    C2D_DrawRectSolid(10, 150, 0, 3, 82, C2D_Color32(255, 95, 109, 255));
    r_->text("La última vez el juego se cerró por un fallo", 200, 154, 0.42f, C_TEXT, 1);
    r_->text_wrap(crash3ds::last(), 200, 172, 0.32f, C_GOLD, 370, 1);
    r_->text("Haz una foto de esto (o manda fallo_anterior.log de la SD) · A: seguir", 200, 216, 0.3f, C_MUTED, 1);
  }
  // Avisos de misiones y logros (arriba, encima de todo).
  if (g_notices.n) {
    const Notice& n = g_notices.q[0];
    C2D_DrawRectSolid(40, 190, 0, 320, n.sub[0] ? 40 : 26, C2D_Color32(20, 26, 44, 235));
    C2D_DrawRectSolid(40, 190, 0, 3, n.sub[0] ? 40 : 26, C_GOLD);
    r_->text_fit(n.title, 200, 194, 310, 0.42f, C_TEXT, false, 1);
    if (n.sub[0]) r_->text_fit(n.sub, 200, 212, 310, 0.34f, C_GOLD, false, 1);
  }
}

void App::draw_bottom() {
  if (scene == SC_RUN) { r_->draw_bottom(*g_, autotest); return; }
  C2D_TextBufClear(r_->tbuf);
  C2D_DrawRectSolid(0, 0, 0, BW, BH, C_BG);
  switch (scene) {
    case SC_TEST: case SC_RESULT: case SC_STARTERS: bottom_test(); break;
    case SC_MENU: bottom_menu(); break;
    case SC_MODE: bottom_mode(); break;
    case SC_SHOP: bottom_shop(); break;
    case SC_GIFT: bottom_gift(); break;
    case SC_ROOMSET: bottom_roomset(); break;
    case SC_DEX: bottom_dex(); break;
    case SC_UPGRADES: case SC_BIND: case SC_BAN: case SC_START: case SC_MOVE2: bottom_upgrades(); break;
    case SC_GACHA: bottom_gacha(); break;
    case SC_PULL: bottom_pull(); break;
    case SC_GOALS: bottom_goals(); break;
    case SC_SETTINGS: bottom_settings(); break;
    case SC_CREDITS: bottom_credits(); break;
    case SC_COOP: bottom_coop(); break;
    case SC_LOBBY: bottom_lobby(); break;
    case SC_ACCOUNT: bottom_account(); break;
    case SC_CLOUDASK: bottom_cloudask(); break;
    case SC_UPDATE: bottom_update(); break;
    case SC_NEWS: bottom_news(); break;
    default: break;
  }
}

}  // namespace ps

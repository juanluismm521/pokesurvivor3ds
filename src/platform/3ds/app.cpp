// app.cpp — ver app.h.
#include "platform/3ds/app.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/i18n.h"
#include "core/progress.h"
#include "core/save_io.h"
#include "core/upgrades.h"
#include "core/util.h"
#include "platform/3ds/audio3ds.h"

namespace ps {

namespace {
constexpr float BW = 320, BH = 240, TW = 400, TH = 240;
const u32 C_BG = C2D_Color32(16, 22, 34, 255);
const u32 C_PANEL = C2D_Color32(30, 40, 62, 255);
const u32 C_SEL = C2D_Color32(52, 70, 110, 255);
const u32 C_TEXT = C2D_Color32(234, 242, 255, 255);
const u32 C_MUTED = C2D_Color32(0x8f, 0xa3, 0xc4, 255);
const u32 C_GOLD = C2D_Color32(0xff, 0xcb, 0x3d, 255);
const char* const GEN_NAMES[9] = {"Kanto", "Johto", "Hoenn", "Sinnoh", "Teselia", "Kalos", "Alola", "Galar", "Paldea"};
// Pokémon que pasean por el menú (CAST de game.js).
const int16_t CAST[20] = {1, 4, 7, 25, 133, 52, 39, 35, 16, 43, 60, 69, 74, 92, 129, 147, 113, 143, 54, 79};

int64_t now_ms() { return int64_t(osGetTime()) - 2208988800000LL; }   // (osGetTime cuenta desde 1900)

bool in_rect(const UiIn* in, float x, float y, float w, float h) {
  return in && in->tap && in->tx >= x && in->tx < x + w && in->ty >= y && in->ty < y + h;
}
}  // namespace

// ---------------- arranque ----------------

bool App::init(Renderer* r, Game* g, Save* s, Settings* st, const char* dir) {
  r_ = r; g_ = g; s_ = s; st_ = st; dir_ = dir;
  apply_settings();
  // Créditos (tools/gen_credits.py).
  FILE* f = std::fopen("romfs:/credits.txt", "rb");
  if (f) {
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    credits_ = static_cast<char*>(std::malloc(n + 1));
    const size_t got = std::fread(credits_, 1, n, f);
    credits_[got] = 0;
    std::fclose(f);
    for (char* p = credits_; *p; ++p) if (*p == '\n') { *p = 0; credit_lines_++; }
  }
  backdrop_init();
  g_->now_ms = now_ms();
  if (!s_->partner) { q_ = 0; scores_ = TraitScores(); go(SC_TEST); }
  else go(SC_MENU);
  return true;
}

void App::set_autotest(int dex, bool immortal, float warp) {
  autotest = true;
  auto_dex_ = dex;
  auto_immortal_ = immortal;
  bot.init(12345);
  start_run();
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
  s_->updated = now_ms();
  save_write_file(dir_, *s_);
  g_->dirty = false;
}

void App::go(Scene sc) {
  scene = sc;
  cur_ = cur2_ = scroll_ = 0;
  if (sc == SC_MENU) {
    progress_check_ach(*s_, now_ms());
    if (g_->state != ST_UI) { g_->state = ST_UI; backdrop_init(); }
  }
  if (sc == SC_DEX) { gen_ = mon_by_dex(s_->partner) ? mon_by_dex(s_->partner)->gen : 1; view_shiny_ = false; }
  if (sc == SC_UPGRADES && !s_->owns(up_dex_)) up_dex_ = s_->partner;
}

void App::start_run() {
  RunConfig cfg;
  cfg.dex = autotest ? auto_dex_ : s_->partner;
  cfg.shiny = !autotest && s_->partner_shiny && s_->owns_shiny(cfg.dex);
  for (int k = 0; k < UP_COUNT; ++k) cfg.upgrades[k] = autotest ? 0 : s_->pupg[cfg.dex][k];
  for (int k = 0; k < 4; ++k) cfg.pbind[k] = autotest ? -1 : s_->pbind[cfg.dex][k];
  for (int k = 0; k < 3; ++k) cfg.pban[k] = autotest ? -1 : s_->pban[cfg.dex][k];
  cfg.seed = autotest ? 12345u : uint32_t(osGetTime());
  cfg.judged = autotest ? 1 : s_->stats.judged;
  for (int d = 0; d <= kMaxDex; ++d) if (s_->legends[d]) cfg.legends_beaten[d >> 3] |= uint8_t(1 << (d & 7));
  const u64 t0 = svcGetSystemTick();
  g_->now_ms = now_ms();
  r_->sprites.async = false;   // precarga: todo en el momento
  g_->start_run(cfg, autotest ? nullptr : s_);
  g_->debug_immortal = autotest && auto_immortal_;
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
  w.reset(4242);
  w.set_biome(bbiome_, true);
  bcam_ = Camera();
  bcam_.x = 0; bcam_.y = 0;
  for (int i = 0; i < 20; ++i) {
    Wanderer& d = wd_[i];
    d.dex = CAST[i];
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
  if (bbiome_t_ > 30) { bbiome_t_ = 0; bbiome_ = (bbiome_ + 1) % 3; w.set_biome(bbiome_); }
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
  // Idle animado con el reloj de la interfaz.
  const AnimMeta* m = anim_meta(dex, A_IDLE, a.shiny);
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
    if (m && s && r_->sprites.frame(s, A_IDLE, std::min<int>(dir, m->r - 1), a.frame, img, ox, oy, w, h)) {
      C2D_ImageTint tint;
      C2D_PlainImageTint(&tint, C2D_Color32(20, 26, 44, 220), 1.0f);
      C2D_DrawImageAt(img, std::floor(x - m->ax * scale + ox * scale), std::floor(y - m->ay * scale + oy * scale), 0, &tint, scale, scale);
    }
    return;
  }
  r_->draw_sprite(a, x, y, 0, 1);
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
  if (tour) {
    // Cada 3 s, el paso siguiente del recorrido (como si se pulsara).
    tour_t_ += dt;
    if (tour_t_ >= 3) {
      tour_t_ = 0;
      const int i = tour_i_++;
      static bool pulled = false;
      static const Scene ORDER[] = {SC_DEX, SC_UPGRADES, SC_GACHA, SC_GOALS, SC_GOALS, SC_GOALS, SC_SETTINGS, SC_CREDITS};
      if (scene == SC_TEST) in.down |= KEY_A;                              // responde
      else if (scene == SC_RESULT && i < 10) { cur_ = 1; in.down |= KEY_A; } // "elegir otro"
      else if (scene == SC_STARTERS) in.down |= KEY_B;
      else if (scene == SC_RESULT) { cur_ = 0; in.down |= KEY_A; }        // acepta
      else if (scene == SC_GACHA && !pulled) { pulled = true; cur_ = 1; in.down |= KEY_A; }
      else if (scene == SC_PULL) { if (pull_done_) in.down |= KEY_A; else tour_i_--; }
      else {
        static int k = -1;
        if (scene == SC_GOALS && k >= 3 && k <= 4) { in.down |= KEY_R; k++; }
        else if (++k < 8) { if (ORDER[k] != scene) go(ORDER[k]); }
        else quit = true;
      }
    }
  }
  g_->now_ms = now_ms();
  notices(dt);
  if (scene != SC_RUN) {
    backdrop_step(dt);
    // Sonidos de la interfaz (los mismos de la web).
    if (in.rep & (KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT)) audio::play("select");
    if (in.down & (KEY_A | KEY_TOUCH)) audio::play("click");
    if (in.down & KEY_B) audio::play("back");
    if (in.down & (KEY_L | KEY_R)) audio::play("select");
  }
  switch (scene) {
    case SC_RUN: in_run(in); break;
    case SC_TEST: case SC_RESULT: case SC_STARTERS: in_test(in); break;
    case SC_MENU: in_menu(in); break;
    case SC_DEX: in_dex(in); break;
    case SC_UPGRADES: case SC_BIND: case SC_BAN: in_upgrades(in); break;
    case SC_GACHA: in_gacha(in); break;
    case SC_PULL: in_pull(in, dt); break;
    case SC_GOALS: in_goals(in); break;
    case SC_SETTINGS: in_settings(in); break;
    case SC_CREDITS:
      if (in.rep & KEY_DDOWN) scroll_ = std::min(scroll_ + 1, std::max(0, credit_lines_ - 10));
      if (in.rep & KEY_DUP) scroll_ = std::max(0, scroll_ - 1);
      if (in.held & KEY_CPAD_DOWN) scroll_ = std::min(scroll_ + 1, std::max(0, credit_lines_ - 10));
      if (in.down & KEY_B) go(SC_MENU);
      break;
  }
}

// ---------------- partida ----------------

void App::in_run(const UiIn& in) {
  Game& g = *g_;
  Input gi;
  if (autotest) {
    Input bi;
    if (bot.decide(g, bi)) gi = bi;
    if (g.state == ST_OVER) start_run();
  } else if (g.state == ST_PLAYING) {
    float ax = std::fabs(in.cx) > 0.12f ? in.cx : 0, ay = std::fabs(in.cy) > 0.12f ? in.cy : 0;
    if (in.held & KEY_DLEFT) ax = -1;
    if (in.held & KEY_DRIGHT) ax = 1;
    if (in.held & KEY_DUP) ay = -1;
    if (in.held & KEY_DDOWN) ay = 1;
    const float m = std::hypot(ax, ay);
    if (m > 1) { ax /= m; ay /= m; }
    gi.ax = ax; gi.ay = ay;
    if (in.down & (KEY_L | KEY_ZL)) gi.cycle = -1;
    if (in.down & (KEY_R | KEY_X | KEY_ZR)) gi.cycle = 1;
    if (in.tap && in.ty >= 150 && in.ty < 216) {
      const Player& p = g.pl();
      const float S = 66, GAP = 8, x0 = (320 - (p.nslots * S + (p.nslots - 1) * GAP)) / 2;
      for (int i = 0; i < p.nslots; ++i) if (in.tx >= x0 + i * (S + GAP) && in.tx < x0 + i * (S + GAP) + S) gi.select = int8_t(i);
    }
    if (g.judg.phase == 1 && (in.down & (KEY_A | KEY_B) || in.tap)) g.skip_judgment();
    if (in.down & KEY_START) { g.toggle_pause(); if (g.save && g.dirty) save_now(); }
  } else if (g.state == ST_PAUSED) {
    if (in.down & (KEY_START | KEY_B)) g.toggle_pause();
    if (in.down & KEY_SELECT) g.quit_run();
  } else if (g.state == ST_LEVELUP) {
    if (in.rep & KEY_DUP) r_->card_cursor = (r_->card_cursor + g.ncards - 1) % g.ncards;
    if (in.rep & KEY_DDOWN) r_->card_cursor = (r_->card_cursor + 1) % g.ncards;
    if (in.tap && in.ty >= 92 && in.ty < 92 + 44 * g.ncards) { r_->card_cursor = (in.ty - 92) / 44; g.pick_card(r_->card_cursor); r_->card_cursor = 0; }
    else if (in.down & KEY_A) { g.pick_card(r_->card_cursor); r_->card_cursor = 0; }
    else if (in.down & KEY_Y) g.reroll_cards();
  } else if (g.state == ST_CHOICE && g.choice == CH_ITEM) {
    if (in.rep & (KEY_DUP | KEY_DDOWN)) r_->card_cursor ^= 1;
    if (in.tap && in.ty >= 96 && in.ty < 204) { r_->card_cursor = (in.ty - 96) / 54; g.pick_item(r_->card_cursor == 1); r_->card_cursor = 0; }
    else if (in.down & KEY_A) { g.pick_item(r_->card_cursor == 1); r_->card_cursor = 0; }
  } else if (g.state == ST_CHOICE && g.choice == CH_BOND) {
    int8_t loose[4]; int nl, grow;
    const int mode = player_bond_options(g.pl(), loose, nl, grow);
    if (nl > 0) {
      if (in.rep & KEY_DUP) r_->card_cursor = (r_->card_cursor + nl - 1) % nl;
      if (in.rep & KEY_DDOWN) r_->card_cursor = (r_->card_cursor + 1) % nl;
      const bool touched = in.tap && in.ty >= 92 && in.ty < 92 + 32 * nl;
      if (touched) r_->card_cursor = (in.ty - 92) / 32;
      if ((in.down & KEY_A) || touched) {
        const int pick = loose[clamp(r_->card_cursor, 0, nl - 1)];
        if (mode == 1) { int one[1] = {pick}; g.pick_bond(one, 1); bond_first_ = -1; r_->card_cursor = 0; }
        else if (bond_first_ < 0) bond_first_ = pick;
        else if (pick != bond_first_) { int two[2] = {bond_first_, pick}; g.pick_bond(two, 2); bond_first_ = -1; r_->card_cursor = 0; }
      }
    }
  } else if (g.state == ST_OVER) {
    if (g.dirty) save_now();
    if (in.down & KEY_A) start_run();
    if (in.down & KEY_B) go(SC_MENU);
  }
  // (el paso de la partida lo da main con el reloj; aquí sólo la entrada)
  pending_input = gi;
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

void App::in_menu(const UiIn& in) {
  const int n = 8;
  if (in.rep & KEY_DUP) cur_ = (cur_ + n - 2) % n;
  if (in.rep & KEY_DDOWN) cur_ = (cur_ + 2) % n;
  if (in.rep & (KEY_DLEFT | KEY_DRIGHT)) cur_ ^= 1;
  int pick = -1;
  for (int i = 0; i < n; ++i) if (in_rect(&in, 8 + (i % 2) * 156, 70 + (i / 2) * 40, 148, 36)) pick = i;
  if (in.down & KEY_A) pick = cur_;
  // Y: alterna normal / shiny del compañero (si tienes su shiny).
  if ((in.down & KEY_Y) && s_->owns_shiny(s_->partner)) { s_->partner_shiny = !s_->partner_shiny; save_now(); }
  if (pick < 0) return;
  cur_ = pick;
  switch (pick) {
    case 0: start_run(); break;
    case 1: go(SC_DEX); break;
    case 2: go(SC_UPGRADES); break;
    case 3: go(SC_GACHA); break;
    case 4: go(SC_GOALS); break;
    case 5: go(SC_SETTINGS); break;
    case 6: go(SC_CREDITS); break;
    case 7: quit = true; break;
  }
}

void App::bottom_menu() {
  header("PokéSurvivor");
  char b[128];
  const int t1i = r_->icons.find("ticket"), t10i = r_->icons.find("ticket10");
  if (t1i >= 0) C2D_DrawImageAt(r_->icons.image(t1i), 8, 28, 0);
  std::snprintf(b, sizeof(b), "%d", int(s_->t1));
  r_->text(b, 24, 27, 0.4f, C_TEXT);
  if (t10i >= 0) C2D_DrawImageAt(r_->icons.image(t10i), 60, 28, 0);
  std::snprintf(b, sizeof(b), "%d", int(s_->t10));
  r_->text(b, 76, 27, 0.4f, C_TEXT);
  std::snprintf(b, sizeof(b), "Pokémon %d / %d", s_->owned_count(), kDexCount);
  r_->text(b, 312, 27, 0.4f, C_MUTED, 2);
  char tt[16]; mmss(float(s_->stats.best_time), tt, sizeof(tt));
  std::snprintf(b, sizeof(b), "Runs %d · Mejor tiempo %s · Mejor nivel %d", int(s_->stats.runs), tt, int(s_->stats.best_level));
  r_->text(b, 160, 46, 0.34f, C_MUTED, 1);
  const int pend = progress_pending(*s_, now_ms());
  static const char* const L[8] = {"Jugar", "Pokémon", "Mejoras", "Gacha", "Misiones", "Ajustes", "Créditos", "Salir"};
  for (int i = 0; i < 8; ++i) {
    const float x = 8 + (i % 2) * 156, y = 70 + (i / 2) * 40;
    button(x, y, 148, 36, L[i], i == cur_);
    if (i == 4 && pend) { std::snprintf(b, sizeof(b), "%d", pend); C2D_DrawCircleSolid(x + 138, y + 8, 0, 7, C2D_Color32(255, 95, 109, 255)); r_->text(b, x + 138, y + 1, 0.34f, C_TEXT, 1, false); }
  }
  r_->text(s_->owns_shiny(s_->partner) ? "Y: normal / shiny" : "", 160, 228, 0.32f, C_MUTED, 1);
}

// ---------------- colección ----------------

namespace {
// Pokémon de la generación que pasan el filtro (todos, tengo, me faltan, shiny, rareza 5..1).
int dex_list(const Save& s, int gen, int filter, int16_t* out) {
  int n = 0;
  for (int i = 0; i < kDexCount; ++i) {
    const Mon& p = kDex[i];
    if (p.gen != gen) continue;
    const bool own = s.owns(p.dex), sh = s.owns_shiny(p.dex);
    if (filter == 1 && !own) continue;
    if (filter == 2 && own) continue;
    if (filter == 3 && !sh) continue;
    if (filter >= 4 && p.rarity != 9 - filter) continue;
    out[n++] = p.dex;
  }
  return n;
}
const char* const FILTERS[9] = {"Todos", "Tengo", "Me faltan", "Shiny", "Legendaria", "Épica", "Rara", "Poco común", "Común"};
}  // namespace

void App::in_dex(const UiIn& in) {
  static int16_t list[256];
  const int n = dex_list(*s_, gen_, filter_, list);
  const int cols = 6;
  if (in.down & KEY_L) { gen_ = gen_ == 1 ? 9 : gen_ - 1; cur_ = 0; scroll_ = 0; }
  if (in.down & KEY_R) { gen_ = gen_ == 9 ? 1 : gen_ + 1; cur_ = 0; scroll_ = 0; }
  if (in.down & KEY_X) { filter_ = (filter_ + 1) % 9; cur_ = 0; scroll_ = 0; }
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
  if (n && (in.down & KEY_Y) && s_->owns_shiny(list[cur_])) view_shiny_ = !view_shiny_;
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
  int have = 0, total = 0;
  for (int i = 0; i < kDexCount; ++i) if (kDex[i].gen == gen_) { total++; if (s_->owns(kDex[i].dex)) have++; }
  char b[96];
  std::snprintf(b, sizeof(b), "%s %d/%d", GEN_NAMES[gen_ - 1], have, total);
  header(b);
  std::snprintf(b, sizeof(b), "L/R generación · X: %s", tr(FILTERS[filter_]));
  r_->text(b, 160, 25, 0.34f, C_MUTED, 1);
  for (int i = 0; i < 24; ++i) {
    const int k = scroll_ * 6 + i;
    if (k >= n) break;
    const int dex = list[k];
    const float x = 4 + (i % 6) * 52, y = 42 + (i / 6) * 44;
    const bool own = s_->owns(dex);
    C2D_DrawRectSolid(x, y, 0, 50, 42, k == cur_ ? C_SEL : C_PANEL);
    draw_mon(dex, false, x + 25, y + 36, 1, own ? 0 : 3);
    std::snprintf(b, sizeof(b), "%03d", dex);
    r_->text(b, x + 2, y + 1, 0.28f, C2D_Color32(255, 255, 255, 150), 0, false);
    if (dex == s_->partner) C2D_DrawRectSolid(x + 42, y + 3, 0, 5, 5, C_GOLD);
    else if (s_->owns_shiny(dex)) C2D_DrawRectSolid(x + 42, y + 3, 0, 5, 5, C2D_Color32(0x9a, 0xe6, 0xff, 255));
  }
  if (!n) r_->text("Ninguno con este filtro.", 160, 110, 0.4f, C_MUTED, 1);
  r_->text("A: compañero · Y: normal/shiny · B: volver", 160, 226, 0.32f, C_MUTED, 1);
}

void App::top_detail(int dex, bool shiny, bool own) {
  const Mon* p = mon_by_dex(dex);
  if (!p) return;
  C2D_DrawRectSolid(0, 0, 0, TW, TH, C2D_Color32(8, 12, 20, 170));
  const uint32_t tc = kTypeColor[p->types[0]];
  C2D_DrawEllipseSolid(40, 150, 0, 140, 34, rgbf(own ? (shiny ? 0x9ae6ff : tc) : 0x5b83d6, 0.35f));
  draw_mon(dex, own && shiny, 110, 170, 3, own ? 0 : 3);
  char b[128];
  std::snprintf(b, sizeof(b), "Nº %03d · %s", p->dex, GEN_NAMES[p->gen - 1]);
  r_->text(b, 300, 14, 0.38f, C_MUTED, 1);
  r_->text(own ? p->name : "???", 300, 30, 0.7f, C_TEXT, 1);
  std::snprintf(b, sizeof(b), "%s %s", std::string(size_t(p->rarity), '*').c_str(), kRarityName[p->rarity]);
  r_->text(b, 300, 58, 0.4f, rgb(kRarityColor[p->rarity]), 1);
  if (!own) {
    std::snprintf(b, sizeof(b), "Aún no lo tienes. Consíguelo en el banner de %s o derrotándolo cuando salga shiny.", GEN_NAMES[p->gen - 1]);
    r_->text_wrap(b, 300, 90, 0.38f, C_MUTED, 180, 1);
    return;
  }
  for (int k = 0; k < 2; ++k) {
    if (p->types[k] < 0) continue;
    const float x = p->types[1] >= 0 ? 252 + k * 52 : 278;
    C2D_DrawRectSolid(x, 76, 0, 46, 14, rgb(kTypeColor[p->types[k]]));
    r_->text(kTypeName[p->types[k]], x + 23, 76, 0.32f, C_TEXT, 1);
  }
  auto stat = [&](const char* label, float v, float m, const char* txt, float y) {
    r_->text(label, 210, y, 0.34f, C_MUTED);
    C2D_DrawRectSolid(272, y + 3, 0, 80, 7, C2D_Color32(0, 0, 0, 140));
    C2D_DrawRectSolid(272, y + 3, 0, 80 * std::min(1.0f, v / m), 7, C_GOLD);
    r_->text(txt, 392, y, 0.34f, C_TEXT, 2);
  };
  char t1[16], t2[16], t3[16];
  std::snprintf(t1, sizeof(t1), "%d", p->hp); std::snprintf(t2, sizeof(t2), "×%.2f", double(p->atk)); std::snprintf(t3, sizeof(t3), "%d", p->spd);
  stat("Vida", float(p->hp), 183, t1, 98);
  stat("Ataque", p->atk, 1.55f, t2, 112);
  stat("Velocidad", float(p->spd), 137, t3, 126);
  std::snprintf(b, sizeof(b), "Empieza con %s", kMoves[move_start_for(dex)].name);
  r_->text(b, 300, 144, 0.36f, C_GOLD, 1);
  int16_t pool[64];
  const int np = move_pool_for(p->types, pool, 64);
  for (int i = 0; i < np && i < 20; ++i) {
    const int ic = r_->move_icon[pool[i]];
    if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), 214 + (i % 10) * 18, 162 + (i / 10) * 18, 0);
  }
  if (dex == s_->partner) r_->text("Es tu compañero", 300, 206, 0.4f, C2D_Color32(0x5f, 0xe0, 0x8a, 255), 1);
}

// ---------------- mejoras ----------------

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
    int16_t pool[64];
    int np = move_pool_for(mon_by_dex(up_dex_)->types, pool, 64);
    const int st = move_start_for(up_dex_);
    bool has = false;
    for (int i = 0; i < np; ++i) if (pool[i] == st) has = true;
    if (!has) pool[np++] = int16_t(st);
    if (in.rep & KEY_DUP) cur_ = (cur_ + 3) % 4;
    if (in.rep & KEY_DDOWN) cur_ = (cur_ + 1) % 4;
    if (in.rep & (KEY_DLEFT | KEY_DRIGHT)) {
      int8_t& slot = s_->pbind[up_dex_][cur_];
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
    if (in.down & KEY_B) { save_now(); scene = SC_UPGRADES; cur_ = UP_COUNT; }
    return;
  }
  if (scene == SC_BAN) {
    int16_t pool[64];
    int np0 = move_pool_for(mon_by_dex(up_dex_)->types, pool, 64), np = 0;
    const int st = move_start_for(up_dex_);
    for (int i = 0; i < np0; ++i) if (pool[i] != st) pool[np++] = pool[i];   // el de inicio no se veta
    if (in.rep & KEY_DUP) cur_ = std::max(0, cur_ - 1);
    if (in.rep & KEY_DDOWN) cur_ = std::min(np - 1, cur_ + 1);
    if (cur_ < scroll_) scroll_ = cur_;
    if (cur_ >= scroll_ + 9) scroll_ = cur_ - 8;
    const int max = s_->pupg[up_dex_][UP_BAN];
    if ((in.down & KEY_A) && np) {
      const int id = pool[cur_];
      int8_t* l = s_->pban[up_dex_];
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
  const int n = UP_COUNT + 1;   // + Atajos
  if (in.rep & KEY_DUP) cur_ = (cur_ + n - 1) % n;
  if (in.rep & KEY_DDOWN) cur_ = (cur_ + 1) % n;
  if (cur_ < scroll_) scroll_ = cur_;
  if (cur_ >= scroll_ + 7) scroll_ = cur_ - 6;
  for (int i = 0; i < 7; ++i) if (in_rect(&in, 6, 28 + i * 28, 308, 26)) { if (cur_ == scroll_ + i) { /* toque doble: comprar */ } cur_ = scroll_ + i; }
  if (in.down & KEY_A) {
    if (cur_ == UP_COUNT) { scene = SC_BIND; cur_ = 0; return; }
    uint8_t& lv = s_->pupg[up_dex_][cur_];
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
      r_->text(id >= 0 ? kMoves[id].name : "Libre", 160, y + 7, 0.42f, C_TEXT, 1);
      if (i == cur_) { r_->text("<", 120, y + 7, 0.42f, C_MUTED, 1); r_->text(">", 280, y + 7, 0.42f, C_MUTED, 1); }
    }
    r_->text("Arriba/abajo: hueco · izquierda/derecha: movimiento · B: volver", 160, 226, 0.3f, C_MUTED, 1);
    return;
  }
  if (scene == SC_BAN) {
    int16_t pool[64];
    int np0 = move_pool_for(mon_by_dex(up_dex_)->types, pool, 64), np = 0;
    const int st = move_start_for(up_dex_);
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
      r_->text(kMoves[id].name, 36, y + 2, 0.36f, on ? C2D_Color32(255, 95, 109, 255) : C_TEXT);
      if (on) r_->text("VETADO", 304, y + 2, 0.32f, C2D_Color32(255, 95, 109, 255), 2);
    }
    r_->text("A: vetar / quitar · B: volver", 160, 226, 0.32f, C_MUTED, 1);
    return;
  }
  for (int i = 0; i < 7; ++i) {
    const int k = scroll_ + i;
    if (k > UP_COUNT) break;
    const float y = 28 + i * 28;
    C2D_DrawRectSolid(6, y, 0, 308, 26, k == cur_ ? C_SEL : C_PANEL);
    if (k == UP_COUNT) { r_->text("Atajos de movimientos", 160, y + 6, 0.4f, C_TEXT, 1); continue; }
    const UpgradeDef& u = kUpgrades[k];
    const int lv = s_->pupg[up_dex_][k];
    const int ic = r_->icons.find(u.icon);
    if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), 10, y + 1, 0, nullptr, 2, 2);
    r_->text(u.name, 38, y + 1, 0.38f, C_TEXT);
    r_->text(u.unit, 38, y + 13, 0.28f, C_MUTED, 0, false);
    for (int p = 0; p < u.max; ++p) C2D_DrawRectSolid(210 + p * 6 - (u.max > 8 ? 12 : 0), y + 18, 0, 4, 4, p < lv ? C_GOLD : C2D_Color32(60, 70, 96, 255));
    if (lv >= u.max) r_->text("Máx.", 308, y + 4, 0.36f, C_MUTED, 2);
    else {
      std::snprintf(b, sizeof(b), "%d", upgrade_cost(k, lv));
      r_->text(b, 308, y + 2, 0.38f, s_->coins >= upgrade_cost(k, lv) ? C_GOLD : C2D_Color32(140, 110, 70, 255), 2);
    }
  }
  r_->text(cur_ == UP_BAN && s_->pupg[up_dex_][UP_BAN] ? "A: comprar · Y: elegir vetados · L/R: Pokémon" : "A: comprar · L/R: otro Pokémon · B: volver", 160, 226, 0.32f, C_MUTED, 1);
}

// ---------------- gacha ----------------

void App::in_gacha(const UiIn& in) {
  if (in.down & KEY_L) gen_ = gen_ == 1 ? 9 : gen_ - 1;
  if (in.down & KEY_R) gen_ = gen_ == 9 ? 1 : gen_ + 1;
  if (in.rep & KEY_DUP) cur_ = (cur_ + 2) % 3;
  if (in.rep & KEY_DDOWN) cur_ = (cur_ + 1) % 3;
  int pick = -1;
  for (int i = 0; i < 3; ++i) if (in_rect(&in, 20, 120 + i * 34, 280, 30)) pick = i;
  if (in.down & KEY_A) pick = cur_;
  if (in.down & KEY_B) { go(SC_MENU); return; }
  if (pick < 0) return;
  cur_ = pick;
  const int n = pick == 0 ? 1 : 10;
  Rng rr(uint32_t(osGetTime()) ^ 0x9e3779b9u);
  npulls_ = gacha_pull(*s_, n, gen_, pick == 2, rr, now_ms(), pulls_);
  if (!npulls_) { audio::play("error"); g_notices.push("No tienes tickets suficientes: consíguelos jugando", "", -1); return; }
  progress_check_ach(*s_, now_ms());
  save_now();
  pull_i_ = 0; pull_t_ = 0; pull_fast_ = false; pull_done_ = false;
  scene = SC_PULL;
}

void App::in_pull(const UiIn& in, float dt) {
  const float prev = pull_t_;
  pull_t_ += dt * (pull_fast_ ? 4 : 1);
  if (!pull_done_) {
    // Sonidos de la tragaperras (slots.js): palanca, cada rodillo y el premio.
    const PullResult& pr = pulls_[pull_i_];
    const bool ten = npulls_ == 10, big = pr.rarity >= 4 || pr.shiny;
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
  if (in.down & (KEY_A | KEY_B) || in.tap) {
    if (pull_done_) { scene = SC_GACHA; return; }
    pull_fast_ = true;
  }
  // Duración de cada tirada (la ×10, rápida; las grandes, con suspense).
  if (pull_done_) return;
  const bool ten = npulls_ == 10;
  const PullResult& r = pulls_[pull_i_];
  const bool big = r.rarity >= 4 || r.shiny;
  const float dur = (ten ? 0.7f : 1.8f) + (big ? (ten ? 0.5f : 1.1f) : 0);
  if (pull_t_ >= dur) {
    pull_t_ = 0;
    pull_i_++;
    if (pull_i_ >= npulls_) { pull_i_ = npulls_ - 1; pull_done_ = true; }
  }
}

void App::bottom_gacha() {
  const BannerDef& bn = kBanners[gen_ - 1];
  char b[128];
  std::snprintf(b, sizeof(b), "Gacha · %s", bn.name);
  header(b);
  int all = 0, own = 0;
  for (int i = 0; i < kDexCount; ++i) if (kDex[i].gen == gen_) { all++; if (s_->owns(kDex[i].dex)) own++; }
  std::snprintf(b, sizeof(b), "Tienes %d de %d · L/R otro banner", own, all);
  r_->text(b, 160, 26, 0.36f, C_MUTED, 1);
  std::snprintf(b, sizeof(b), "Tiradas sin legendaria: %d / %d", int(s_->pity[gen_]), kPity5);
  r_->text(b, 160, 44, 0.36f, C_TEXT, 1);
  std::snprintf(b, sizeof(b), "Tickets: %d · Tickets ×10: %d", int(s_->t1), int(s_->t10));
  r_->text(b, 160, 62, 0.38f, C_GOLD, 1);
  static const char* const L[3] = {"Tirar ×1 (1 ticket)", "Tirar ×10 (10 tickets)", "Tirar ×10 (1 ticket ×10)"};
  for (int i = 0; i < 3; ++i) {
    int c1, c10;
    const bool ok = gacha_cost(*s_, i == 0 ? 1 : 10, i == 2, c1, c10);
    C2D_DrawRectSolid(20, 120 + i * 34, 0, 280, 30, i == cur_ ? C_SEL : C_PANEL);
    if (i == cur_) C2D_DrawRectSolid(20, 120 + i * 34, 0, 3, 30, C_GOLD);
    r_->text(L[i], 160, 127 + i * 34, 0.42f, ok ? C_TEXT : C2D_Color32(110, 120, 140, 255), 1);
  }
  r_->text("La ×10 garantiza al menos una Épica · B: volver", 160, 226, 0.32f, C_MUTED, 1);
}

void App::top_pull() {
  // Tragaperras (slots.js, simplificada): palanca, 3 rodillos con Poké Balls
  // de la rareza y un letrero luminoso.
  C2D_DrawRectSolid(0, 0, 0, TW, TH, C2D_Color32(10, 6, 20, 230));
  const float mx = 120, my = 36;
  C2D_DrawRectSolid(mx, my, 0, 160, 150, rgb(0xc8323c));
  C2D_DrawRectSolid(mx + 4, my + 4, 0, 152, 4, rgb(0xef6464));
  C2D_DrawRectSolid(mx + 10, my + 14, 0, 140, 22, rgb(0x2a0f14));
  const PullResult& r = pulls_[pull_i_];
  const bool ten = npulls_ == 10;
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
    else { std::snprintf(b, sizeof(b), "Repetido · +%d", r.refund); r_->text(b, 200, 216, 0.38f, C_MUTED, 1); }
  }
}

void App::bottom_pull() {
  header("Resultados");
  for (int i = 0; i <= pull_i_ && i < npulls_; ++i) {
    const PullResult& r = pulls_[i];
    const bool shown = pull_done_ || i < pull_i_;
    const float x = 6 + (i % 5) * 62, y = 30 + (i / 5) * 92;
    C2D_DrawRectSolid(x, y, 0, 58, 86, C_PANEL);
    if (!shown) continue;
    C2D_DrawRectSolid(x, y, 0, 58, 3, rgb(r.shiny ? 0x9ae6ff : kRarityColor[r.rarity]));
    draw_mon(r.dex, r.shiny, x + 29, y + 50, 1);
    char b[32];
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
      r_->text(b, 12, y + 2, 0.36f, m.claimed ? C_MUTED : C_TEXT);
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
      r_->text(a.name, 38, y + 1, 0.36f, s_->ach[k] ? C_TEXT : C_MUTED);
      r_->text(a.desc, 38, y + 14, 0.28f, C_MUTED, 0, false);
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

void App::in_settings(const UiIn& in) {
  const int n = 8;
  if (in.rep & KEY_DUP) cur_ = (cur_ + n - 1) % n;
  if (in.rep & KEY_DDOWN) cur_ = (cur_ + 1) % n;
  for (int i = 0; i < n; ++i) if (in_rect(&in, 6, 28 + i * 24, 308, 22)) cur_ = i;
  int d = 0;
  if (in.rep & KEY_DLEFT) d = -1;
  if (in.rep & (KEY_DRIGHT | KEY_A)) d = 1;
  if (in.tap && in_rect(&in, 6, 28 + cur_ * 24, 308, 22)) d = 1;
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
    }
    apply_settings();
  }
  if (in.down & KEY_B) { settings_write(dir_, *st_); go(SC_MENU); }
}

void App::bottom_settings() {
  header("Ajustes");
  static const char* const L[8] = {"Idioma", "Calidad", "Números de daño", "Menos sacudidas", "Mostrar FPS", "Música", "Efectos", "3D"};
  char v[8][24];
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
  for (int i = 0; i < 8; ++i) {
    const float y = 28 + i * 24;
    C2D_DrawRectSolid(6, y, 0, 308, 22, i == cur_ ? C_SEL : C_PANEL);
    r_->text(L[i], 14, y + 4, 0.38f, C_TEXT);
    r_->text(v[i], 306, y + 4, 0.38f, C_GOLD, 2, false);
  }
  r_->text("Izquierda/derecha: cambiar · B: guardar y volver", 160, 226, 0.32f, C_MUTED, 1);
}

void App::bottom_credits() {
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
  if (scene == SC_RUN) { r_->draw_top(*g_, eye); }
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
          r_->text(b, 200, 44, 0.38f, C_GOLD, 1);
        }
        break;
      }
      case SC_MENU: {
        const int dex = s_->partner;
        const bool sh = s_->partner_shiny && s_->owns_shiny(dex);
        C2D_DrawEllipseSolid(140, 186, 0, 120, 30, rgbf(sh ? 0x9ae6ff : kTypeColor[mon_by_dex(dex)->types[0]], 0.3f));
        draw_mon(dex, sh, 200, 204, 3);
        char b[64];
        std::snprintf(b, sizeof(b), "%s%s", mon_by_dex(dex)->name, sh ? " ★" : "");
        r_->text(b, 200, 10, 0.7f, C_TEXT, 1);
        r_->text("PokéSurvivor 3DS", 200, 222, 0.34f, C2D_Color32(255, 255, 255, 120), 1, false);
        break;
      }
      case SC_DEX: {
        static int16_t list[256];
        const int n = dex_list(*s_, gen_, filter_, list);
        if (n) top_detail(list[std::min(cur_, n - 1)], view_shiny_, s_->owns(list[std::min(cur_, n - 1)]));
        break;
      }
      case SC_UPGRADES: case SC_BIND: case SC_BAN: {
        const bool sh = up_dex_ == s_->partner && s_->partner_shiny && s_->owns_shiny(up_dex_);
        draw_mon(up_dex_, sh, 200, 200, 3);
        r_->text(mon_by_dex(up_dex_)->name, 200, 14, 0.6f, C_TEXT, 1);
        if (scene == SC_UPGRADES && cur_ < UP_COUNT) r_->text_wrap(kUpgrades[cur_].unit, 200, 44, 0.42f, C_GOLD, 340, 1);
        break;
      }
      case SC_GACHA: {
        const BannerDef& bn = kBanners[gen_ - 1];
        C2D_DrawRectangle(0, 0, 0, TW, TH, rgb(bn.color[0], 230), rgb(bn.color[1], 230), rgb(bn.color[1], 230), rgb(bn.color[2], 230));
        char b[64];
        std::snprintf(b, sizeof(b), "%s Misterioso", bn.name);
        r_->text(b, 200, 12, 0.7f, C_GOLD, 1);
        int16_t f[3];
        const int nf = gacha_featured(gen_, f);
        for (int i = 0; i < nf; ++i) draw_mon(f[i], false, 90 + i * 110, 170, 2, 0, 1);
        for (int r = 5; r >= 1; --r) {
          int16_t pool[256];
          std::snprintf(b, sizeof(b), "%s %.1f%% · %d", kRarityName[r], double(kGachaRates[r - 1]), gacha_pool(r, gen_, pool));
          r_->text(b, 10, 180 + (5 - r) * 11, 0.3f, rgb(kRarityColor[r]), 0, false);
        }
        r_->text("Shiny 1%", 390, 224, 0.3f, rgb(0x9ae6ff), 2, false);
        break;
      }
      case SC_PULL: top_pull(); break;
      default: break;
    }
  }
  // Avisos de misiones y logros (arriba, encima de todo).
  if (g_notices.n) {
    const Notice& n = g_notices.q[0];
    C2D_DrawRectSolid(40, 190, 0, 320, n.sub[0] ? 40 : 26, C2D_Color32(20, 26, 44, 235));
    C2D_DrawRectSolid(40, 190, 0, 3, n.sub[0] ? 40 : 26, C_GOLD);
    r_->text(n.title, 200, 194, 0.42f, C_TEXT, 1);
    if (n.sub[0]) r_->text(n.sub, 200, 212, 0.34f, C_GOLD, 1);
  }
}

void App::draw_bottom() {
  if (scene == SC_RUN) { r_->draw_bottom(*g_, autotest); return; }
  C2D_TextBufClear(r_->tbuf);
  C2D_DrawRectSolid(0, 0, 0, BW, BH, C_BG);
  switch (scene) {
    case SC_TEST: case SC_RESULT: case SC_STARTERS: bottom_test(); break;
    case SC_MENU: bottom_menu(); break;
    case SC_DEX: bottom_dex(); break;
    case SC_UPGRADES: case SC_BIND: case SC_BAN: bottom_upgrades(); break;
    case SC_GACHA: bottom_gacha(); break;
    case SC_PULL: bottom_pull(); break;
    case SC_GOALS: bottom_goals(); break;
    case SC_SETTINGS: bottom_settings(); break;
    case SC_CREDITS: bottom_credits(); break;
    default: break;
  }
}

}  // namespace ps

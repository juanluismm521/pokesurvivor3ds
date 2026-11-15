// app_gift.cpp — la pantalla de los regalos (core/gift.h). Al llegar al menú, si el
// servidor tiene regalos para ti, sale un paquete con lo que trae; con A se abre
// (confeti y un mensaje) y queda en tu partida. Varios: uno detrás de otro.
#include <3ds.h>

#include <cmath>
#include <cstdio>

#include "core/gamedata.h"
#include "core/gift.h"
#include "core/i18n.h"
#include "core/update.h"
#include "platform/3ds/app.h"
#include "platform/3ds/app_ui.h"
#include "platform/3ds/audio3ds.h"
#include "platform/3ds/cloud3ds.h"

namespace ps {

namespace {
const u32 kRed = C2D_Color32(232, 57, 74, 255), kRedDark = C2D_Color32(170, 34, 52, 255);
const u32 kRibbon = C2D_Color32(255, 210, 94, 255), kGreen = C2D_Color32(95, 224, 138, 255);
const u32 kConfetti[6] = {C2D_Color32(255, 210, 94, 255), C2D_Color32(95, 200, 255, 255), C2D_Color32(126, 224, 122, 255),
                          C2D_Color32(255, 138, 216, 255), C2D_Color32(255, 138, 42, 255), C2D_Color32(196, 123, 255, 255)};
}  // namespace

void App::apply_gifts(const std::string& json) {
  if (autotest || no_gifts) return;   // (las capturas de las pruebas, sin los regalos de verdad del servidor)
  static Gift all[kMaxGiftsFile];
  const int n = gift_parse(json, kUpdateKey, all, kMaxGiftsFile);
  const cloud3ds::Account& acc = cloud3ds::account();
  gift_n_ = 0;
  for (int i = 0; i < n && gift_n_ < kGiftQueue; ++i)
    if (gift_for(all[i], s_->name, acc.on ? acc.name.c_str() : "") && !gift_received(*s_, all[i].id)) gift_q_[gift_n_++] = all[i];
  if (!gift_n_) return;
  gift_i_ = 0;
  gift_open_ = false;
  gift_t_ = 0;
  audio::play("shiny");
  go(SC_GIFT);
}

void App::in_gift(const UiIn& in) {
  gift_t_ += 1.0f / 60;
  if (gift_i_ >= gift_n_) { go(SC_MENU); return; }
  const bool press = (in.down & KEY_A) || in.tap;
  if (!gift_open_) {
    if (!press) return;
    gift_apply(*s_, gift_q_[gift_i_], now_ms());
    gift_open_ = true;
    gift_t_ = 0;
    audio::play("legend");
    if (!autotest) save_now();
    return;
  }
  if (!press || gift_t_ < 0.6f) return;   // (que se vea el confeti antes de seguir)
  audio::play("confirm");
  if (++gift_i_ < gift_n_) { gift_open_ = false; gift_t_ = 0; return; }
  go(SC_MENU);
  if (!autotest) cloud_autostart();   // (lo recibido, a pokesurvivor.com)
}

void App::top_gift() {
  if (gift_i_ >= gift_n_) return;
  const Gift& gf = gift_q_[gift_i_];
  const float t = gift_t_;
  const float cx = 200, base = 176;
  char b[160];
  if (!gift_open_) {
    if (gift_n_ - gift_i_ > 1) std::snprintf(b, sizeof(b), "¡Tienes %d regalos!", gift_n_ - gift_i_);
    else std::snprintf(b, sizeof(b), "¡Tienes un regalo!");
    r_->text(b, 200, 10, 0.72f, C_GOLD, 1);
    const char* nota = i18n_lang() == LANG_EN && !gf.nota_en.empty() ? gf.nota_en.c_str() : gf.nota.c_str();
    if (nota[0]) r_->text_wrap(nota, 200, 38, 0.4f, C_TEXT, 370, 1);
    // El paquete: bota un poco y, de vez en cuando, se menea.
    const float bob = std::sin(t * 3) * 3;
    const float shake = std::fmod(t, 2.4f) < 0.35f ? std::sin(t * 60) * 2.5f : 0;
    const float x = cx + shake, y = base + bob;
    C2D_DrawEllipseSolid(cx - 62, base + 2, 0, 124, 16, C2D_Color32(0, 0, 0, 70));
    C2D_DrawRectSolid(x - 52, y - 70, 0, 104, 70, kRed);                 // caja
    C2D_DrawRectSolid(x - 52, y - 70, 0, 104, 6, kRedDark);
    C2D_DrawRectSolid(x - 58, y - 88, 0, 116, 20, kRed);                 // tapa
    C2D_DrawRectSolid(x - 58, y - 72, 0, 116, 4, kRedDark);
    C2D_DrawRectSolid(x - 8, y - 88, 0, 16, 88, kRibbon);                // cinta
    C2D_DrawRectSolid(x - 52, y - 40, 0, 104, 12, kRibbon);
    C2D_DrawEllipseSolid(x - 30, y - 106, 0, 28, 20, kRibbon);           // lazo
    C2D_DrawEllipseSolid(x + 2, y - 106, 0, 28, 20, kRibbon);
    C2D_DrawEllipseSolid(x - 7, y - 101, 0, 14, 12, C2D_Color32(230, 170, 60, 255));
    // Brillos alrededor.
    for (int i = 0; i < 6; ++i) {
      const float a = i * 1.047f + t * 0.8f, k = 0.5f + 0.5f * std::sin(t * 4 + i);
      const float sx = cx + std::cos(a) * 96, sy = base - 50 + std::sin(a) * 52;
      C2D_DrawRectSolid(sx - 1, sy - 4 * k, 0, 2, 8 * k, C2D_Color32(255, 243, 200, u8(200 * k)));
      C2D_DrawRectSolid(sx - 4 * k, sy - 1, 0, 8 * k, 2, C2D_Color32(255, 243, 200, u8(200 * k)));
    }
    return;
  }
  // Abierto: la tapa sale volando, confeti y el mensaje.
  const float k = std::fmin(1.0f, t * 1.6f);
  r_->text("¡Regalo abierto!", 200, 10, 0.72f, kGreen, 1);
  const bool en = i18n_lang() == LANG_EN;
  const std::string& m = en && !gf.msg_en.empty() ? gf.msg_en : gf.msg;
  if (!m.empty()) r_->text_wrap(m.c_str(), 200, 30, 0.4f, C_TEXT, 388, 1);   // (del servidor: ya en su idioma)
  else r_->text_wrap("¡Gracias por jugar a PokéSurvivor en tu 3DS y por apoyar este port!", 200, 38, 0.4f, C_TEXT, 370, 1);
  // (la caja, más abajo y más pequeña: arriba va el mensaje, de hasta 6 líneas)
  const float ob = 232;
  C2D_DrawEllipseSolid(cx - 46, ob + 1, 0, 92, 10, C2D_Color32(0, 0, 0, 70));
  C2D_DrawRectSolid(cx - 38, ob - 46, 0, 76, 46, kRed);
  C2D_DrawRectSolid(cx - 38, ob - 46, 0, 76, 4, kRedDark);
  C2D_DrawRectSolid(cx - 6, ob - 46, 0, 12, 46, kRibbon);
  const float ly = ob - 60 - k * 40, lx = cx + k * 80;
  C2D_DrawRectSolid(lx - 42, ly, 0, 84, 14, C2D_Color32(232, 57, 74, u8(255 * (1 - k * 0.7f))));
  // Luz que sale de la caja.
  C2D_DrawTriangle(cx - 34, ob - 46, C2D_Color32(255, 243, 200, 120), cx + 34, ob - 46, C2D_Color32(255, 243, 200, 120),
                   cx, ob - 46 - 50 * k, C2D_Color32(255, 243, 200, 0), 0);
  // Confeti: sale de la caja y cae.
  for (int i = 0; i < 40; ++i) {
    const float a = -1.57f + (float((i * 37) % 40) / 40 - 0.5f) * 2.6f;
    const float v = 90 + (i * 53) % 80;
    const float tt = std::fmin(t, 2.5f);
    const float px = cx + std::cos(a) * v * tt;
    const float py = ob - 50 + std::sin(a) * v * tt + 60 * tt * tt;
    if (py > 236) continue;
    C2D_DrawRectSolid(px, py, 0, 3 + i % 2, 3, kConfetti[i % 6]);
  }
}

void App::bottom_gift() {
  header(gift_open_ ? "¡Recibido!" : "Regalo");
  if (gift_i_ >= gift_n_) return;
  const Gift& gf = gift_q_[gift_i_];
  r_->text("De: PokéSurvivor 3DS", 160, 26, 0.34f, C_MUTED, 1);
  char b[96];
  float y = 42;   // (hasta 7 filas antes del botón)
  auto row = [&](int icon, int dex, bool shiny, const char* txt, bool medal) {
    C2D_DrawRectSolid(16, y, 0, 288, 19, gift_open_ ? C2D_Color32(30, 60, 44, 230) : C_PANEL);
    if (medal) r_->draw_medal(32, y + 10, 5);
    else if (dex > 0) draw_mon(dex, shiny, 32, y + 18, 0.5f);
    else if (icon >= 0 && r_->ui_icon[icon] >= 0) C2D_DrawImageAt(r_->icons.image(r_->ui_icon[icon]), 24, y + 2, 0);
    r_->text(txt, 48, y + 2, 0.4f, gift_open_ ? kGreen : C_TEXT);
    if (gift_open_) r_->text("+", 296, y + 1, 0.46f, kGreen, 2);
    y += 22;
  };
  if (gf.coins) { std::snprintf(b, sizeof(b), "%d Pokémonedas", gf.coins); row(1, 0, false, b, false); }
  if (gf.t1) { std::snprintf(b, sizeof(b), "%d tickets", gf.t1); row(2, 0, false, b, false); }
  if (gf.t10) { std::snprintf(b, sizeof(b), "%d tickets ×10", gf.t10); row(3, 0, false, b, false); }
  if (gf.candy) { std::snprintf(b, sizeof(b), "%d caramelos", gf.candy); row(16, 0, false, b, false); }
  if (gf.medal > 0 && mon_by_dex(gf.medal)) { std::snprintf(b, sizeof(b), "Medalla de Arceus: %s", mon_by_dex(gf.medal)->name); row(-1, 0, false, b, true); }
  for (int i = 0; i < gf.nshiny; ++i)
    if (const Mon* m = mon_by_dex(gf.shiny[i])) { std::snprintf(b, sizeof(b), "%s shiny", m->name); row(-1, gf.shiny[i], true, b, false); }
  if (!gift_open_) {
    const float pulse = 0.75f + 0.25f * std::sin(gift_t_ * 5);
    C2D_DrawRectSolid(80, 200, 0, 160, 26, C2D_Color32(u8(232 * pulse), 57, 74, 255));
    r_->text("A: ¡Abrir el regalo!", 160, 205, 0.48f, C2D_Color32(255, 255, 255, 255), 1);
  } else {
    r_->text(gift_i_ + 1 < gift_n_ ? "A: el siguiente regalo" : "A: seguir", 160, 210, 0.42f, C_TEXT, 1);
  }
}

// Autotest «regalo» (capturas): los dos de ejemplo, sin red ni guardar (con el mensaje
// que llevan los de verdad, tools/regalo.py mensaje=… mensaje_en=…).
static const char kGiftMsg[] = "¡Gracias por jugar a PokéSurvivor en tu 3DS y por apoyar este port! Como agradecimiento por todos vuestros reportes de bugs y vuestro apoyo, os envío este regalo. ¡Muy pronto llegarán la Tienda Kecleon y más idiomas!";
static const char kGiftMsgEn[] = "Thanks for playing PokéSurvivor on your 3DS and for supporting this port! As a thank-you for all your bug reports and support, here's a gift for you. The Kecleon Shop and more languages are coming very soon!";
void App::start_gift_demo() {
  gift_demo_ = true;
  news_t_ = 0;
  news_checked_ = cloud_asked_ = true;
  gift_n_ = 0;
  Gift& a = gift_q_[gift_n_++];
  a.id = "demo-gracias"; a.para = "*"; a.coins = 7000; a.t10 = 10; a.candy = 100;
  a.nota = "¡Gracias por jugar al port de PokéSurvivor en tu 3DS! Esto es para ti.";
  a.nota_en = "Thanks for playing the PokéSurvivor port on your 3DS! This is for you.";
  a.msg = kGiftMsg; a.msg_en = kGiftMsgEn;
  Gift& v = gift_q_[gift_n_++];
  v.id = "demo-victoria"; v.para = s_->name; v.coins = 12898; v.t1 = 61; v.t10 = 10; v.candy = 88; v.medal = 94;
  v.shiny[0] = 192; v.nshiny = 1;
  v.nota = "Premios de la victoria perdida"; v.nota_en = "Rewards from your lost victory";
  v.msg = kGiftMsg; v.msg_en = kGiftMsgEn;
  gift_i_ = 0; gift_open_ = false; gift_t_ = 0;
  go(SC_GIFT);
}

void App::gift_demo_step(float dt) {
  const float t0 = news_t_;
  news_t_ += dt;
  auto at = [&](float s) { return t0 < s && news_t_ >= s; };
  UiIn press{};
  press.down = KEY_A;
  if (at(5) || at(9) || at(14) || at(18)) in_gift(press);
  if (news_t_ > 22) quit = true;
}

}  // namespace ps

// app_shop.cpp — la Tienda Kecleon de Halloween (shop.js y shop-ui.js de la web):
// los disfraces de bruja por caramelos. Sólo abre en Halloween (hw3ds::on); lo
// comprado se queda para siempre y se pone o quita aquí o en la Colección (START).
#include <3ds.h>

#include <cmath>
#include <cstdio>

#include "core/halloween.h"
#include "core/i18n.h"
#include "core/progress.h"
#include "platform/3ds/app.h"
#include "platform/3ds/app_ui.h"
#include "platform/3ds/audio3ds.h"
#include "platform/3ds/hw3ds.h"

namespace ps {

namespace {
// Los que se venden (en el orden de kSkins).
int sellable(int* out) {
  int n = 0;
  for (int k = 0; k < kSkinCount; ++k) if (kSkins[k].sell) out[n++] = k;
  return n;
}
constexpr float kRowY = 76, kRowH = 44;
}  // namespace

// Autotest «tienda»: caramelos de sobra, la tienda (2 s), compra el de Pikachu (a los
// 6 s), Pikachu de compañero y el menú con su disfraz (a los 12 s) y fin.
void App::start_shop_demo() {
  shop_demo_ = true;
  news_t_ = 0;
  news_checked_ = cloud_asked_ = true;
  s_->candy = 760;
  s_->candy_known = true;
  if (!s_->owns(25)) s_->owned_at[25] = now_ms();
  open_shop();
}

void App::shop_demo_step(float dt) {
  const float t0 = news_t_;
  news_t_ += dt;
  if (t0 < 6 && news_t_ >= 6) {
    int list[8];
    const int n = sellable(list);
    for (int i = 0; i < n; ++i) if (kSkins[list[i]].dex == 25) { cur_ = i; skin_buy(*s_, list[i], true, now_ms()); }
  }
  if (t0 < 12 && news_t_ >= 12) { s_->partner = 25; s_->partner_shiny = false; go(SC_MENU); }
  if (news_t_ > 20) quit = true;
}

void App::open_shop() {
  cur_ = 0;
  go(SC_SHOP);
}

void App::in_shop(const UiIn& in) {
  int list[8];
  const int n = sellable(list);
  if (in.down & KEY_B) { go(SC_MENU); return; }
  if (!n) return;
  if (in.rep & KEY_DUP) cur_ = (cur_ + n - 1) % n;
  if (in.rep & KEY_DDOWN) cur_ = (cur_ + 1) % n;
  cur_ = clamp(cur_, 0, n - 1);
  int pick = -1;
  for (int i = 0; i < n; ++i) if (in_rect(&in, 10, kRowY + i * kRowH, 300, kRowH - 4)) pick = i;
  if (in.down & KEY_A) pick = cur_;
  if (pick < 0) return;
  cur_ = pick;
  const int k = list[pick];
  const SkinDef& sk = kSkins[k];
  if (s_->skin_at[k] > 0) {
    // Ya es tuyo: ponérselo o quitárselo (si tienes ese Pokémon).
    if (!s_->owns(sk.dex)) { audio::play("error"); g_notices.push("Aún no tienes a ese Pokémon", "Consíguelo y se lo podrás poner", -1); return; }
    const bool on = skin_worn(*s_, sk.dex) == k;
    skin_wear(*s_, sk.dex, on ? -1 : k, now_ms());
    audio::play("select");
    save_now();
    return;
  }
  switch (skin_buy(*s_, k, hw3ds::on(), now_ms())) {
    case SKIN_OK: {
      audio::play("legend");
      char t[96];
      std::snprintf(t, sizeof(t), "¡%s es tuyo!", sk.name);
      g_notices.push(t, s_->owns(sk.dex) ? "Ya lo lleva puesto (cámbialo en la Colección con START)" : "Se lo pondrá cuando consigas a ese Pokémon", -1);
      save_now();
      break;
    }
    case SKIN_NO_CANDY: audio::play("error"); g_notices.push("No tienes caramelos suficientes.", "Los sueltan los Pokémon Fantasma y las calabazas", -1); break;
    case SKIN_NOT_EVENT: audio::play("error"); g_notices.push("Las skins de bruja sólo se venden en Halloween.", "", -1); break;
    default: break;
  }
}

void App::bottom_shop() {
  header("Tienda Kecleon");
  char b[128];
  // Tus caramelos, arriba.
  const int ci = r_->ui_icon[16];
  if (ci >= 0) C2D_DrawImageAt(r_->icons.image(ci), 8, 27, 0);
  std::snprintf(b, sizeof(b), "%d caramelos", int(s_->candy));
  r_->text(b, 26, 26, 0.42f, C2D_Color32(255, 138, 42, 255));
  r_->text_wrap("Disfraces de bruja para siempre, por caramelos. Los sueltan los Pokémon Fantasma y las calabazas en Halloween.",
                160, 44, 0.32f, C_MUTED, 296, 1);
  const bool ev = hw3ds::on();
  int list[8];
  const int n = sellable(list);
  for (int i = 0; i < n; ++i) {
    const int k = list[i];
    const SkinDef& sk = kSkins[k];
    const float y = kRowY + i * kRowH;
    const bool sel = i == cur_;
    C2D_DrawRectSolid(10, y, 0, 300, kRowH - 4, sel ? C_SEL : C_PANEL);
    if (sel) C2D_DrawRectSolid(10, y, 0, 3, kRowH - 4, C_GOLD);
    const bool owned = s_->skin_at[k] > 0, worn = skin_worn(*s_, sk.dex) == k;
    draw_mon(sk.sprite, false, 34, y + 37, sk.dex == 94 ? 0.75f : 0.95f);   // (Gengar ya es grande)
    r_->text_fit(sk.name, 64, y + 3, 236, 0.44f, sel ? C_GOLD : C_TEXT);
    // Segunda línea: el precio (o si es tuyo) a la derecha y qué hace A a la izquierda.
    if (!owned) {
      std::snprintf(b, sizeof(b), "%d caramelos", kSkinPrice);
      r_->text(b, 302, y + 21, 0.38f, ev && s_->candy >= kSkinPrice ? C2D_Color32(255, 138, 42, 255) : C2D_Color32(140, 110, 70, 255), 2);
      r_->text(ev ? "A: comprar" : "Sólo en Halloween", 64, y + 23, 0.32f, C_MUTED);
    } else {
      r_->text(worn ? "Lo lleva puesto" : "Comprado", 302, y + 21, 0.38f, C2D_Color32(0x5f, 0xe0, 0x8a, 255), 2);
      r_->text(!s_->owns(sk.dex) ? "Te falta ese Pokémon" : worn ? "A: quitárselo" : "A: ponérselo", 64, y + 23, 0.32f, C_MUTED);
    }
  }
  r_->text("Próximamente, más cosas en la Tienda Kecleon", 160, 212, 0.32f, C2D_Color32(126, 224, 122, 255), 1);
  r_->text("A: comprar o ponérselo · B: volver", 160, 226, 0.32f, C_MUTED, 1);
}

void App::top_shop() {
  // Kecleon con su sombrero de bruja (y el disfraz elegido, en grande).
  int list[8];
  const int n = sellable(list);
  int hat = -1;
  for (int k = 0; k < kSkinCount; ++k) if (!kSkins[k].sell) hat = k;
  const bool ev = hw3ds::on();
  r_->text("Tienda Kecleon", 200, 8, 0.7f, ev ? C2D_Color32(255, 138, 42, 255) : C_GOLD, 1);
  r_->text_wrap(ev ? "¡Bienvenido! Esta noche, disfraces de bruja." : "¡Bienvenido! Pronto habrá más cosas en la tienda.", 200, 32, 0.38f, C_TEXT, 380, 1);
  C2D_DrawEllipseSolid(40, 186, 0, 120, 26, C2D_Color32(0, 0, 0, 70));
  draw_mon(ev && hat >= 0 ? kSkins[hat].sprite : 352, false, 100, 204, 2.5f);
  if (n) {
    const SkinDef& sk = kSkins[list[clamp(cur_, 0, n - 1)]];
    C2D_DrawEllipseSolid(230, 186, 0, 120, 26, C2D_Color32(0, 0, 0, 70));
    draw_mon(sk.sprite, false, 290, 204, 3);
  }
  // Cuenta atrás: lo que queda de Halloween.
  bool on; int64_t ms;
  if (hw3ds::countdown(on, ms) && on) {
    char b[64], t[32];
    hw_time(ms, t, sizeof(t));
    std::snprintf(b, sizeof(b), "Halloween termina en %s", t);
    r_->text(b, 200, 222, 0.36f, C2D_Color32(255, 200, 140, 255), 1);
  }
}

// El botón R de la consola (gris, alargado), que se pulsa solo cada poco (como el X de la cuenta).
static void r_button(Renderer* r, float cx, float cy, float t) {
  const float ph = std::fmod(t + 0.35f, 1.4f);
  const bool down = ph > 0.7f && ph < 0.92f;
  const float dy = down ? 1.5f : 0;
  C2D_DrawRectSolid(cx - 10, cy - 6 + 1.5f, 0, 20, 13, C2D_Color32(30, 34, 48, 255));
  C2D_DrawRectSolid(cx - 10, cy - 6 + dy, 0, 20, 12, down ? C2D_Color32(120, 128, 150, 255) : C2D_Color32(165, 172, 192, 255));
  C2D_DrawRectSolid(cx - 10, cy - 6 + dy, 0, 20, 2, C2D_Color32(220, 226, 240, 255));
  r->text("R", cx, cy - 6 + dy, 0.36f, C2D_Color32(30, 34, 48, 255), 1, false);
}

// Menú, arriba a la derecha: la Tienda Kecleon (R). En Halloween, Kecleon de bruja.
void App::top_shop_badge() {
  const bool hw = hw3ds::on();
  const float w = 144, h = 46, x0 = TW - 4 - w, y0 = 4;
  C2D_DrawRectSolid(x0, y0, 0, w, h, C2D_Color32(12, 18, 32, 200));
  C2D_DrawRectSolid(x0, y0, 0, 2, h, hw ? C2D_Color32(255, 138, 42, 255) : C2D_Color32(95, 205, 115, 255));
  int hat = -1;
  for (int k = 0; k < kSkinCount; ++k) if (!kSkins[k].sell) hat = k;
  draw_mon(hw && hat >= 0 ? kSkins[hat].sprite : 352, false, x0 + 22, y0 + h - 2, 1.3f);
  const u32 col = hw ? C2D_Color32(255, 160, 80, 255) : C_TEXT;
  // (en inglés, «Kecleon / Shop»: Kecleon Shop)
  const bool es = i18n_lang() == LANG_ES;
  r_->text_fit(es ? "Tienda" : "Kecleon", x0 + 42, y0 + 6, 74, 0.38f, col);
  r_->text_fit(es ? "Kecleon" : "Tienda", x0 + 42, y0 + 22, 74, 0.38f, col);
  r_button(r_, x0 + w - 14, y0 + h / 2, t_);
}

// «2 d 11:03:42» o «11:03:42» (cuenta atrás de Halloween).
void App::hw_time(int64_t ms, char* out, int cap) {
  const int64_t s = ms / 1000;
  const int d = int(s / 86400), h = int(s / 3600 % 24), m = int(s / 60 % 60), sec = int(s % 60);
  if (d > 0) std::snprintf(out, size_t(cap), "%d d %02d:%02d:%02d", d, h, m, sec);
  else std::snprintf(out, size_t(cap), "%02d:%02d:%02d", h, m, sec);
}

}  // namespace ps

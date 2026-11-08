// app_shop.cpp — la Tienda Kecleon (data/shop.js y ui/shop-ui.js de la web, 20-10-2026):
// Mentas, Cápsulas Habilidad y las 8 MT de cada día, por Pokémonedas, y la Mochila.
// Arriba, el puesto de los dos Kecleon (el verde y su hermano morado, el shiny): toldo
// de rayas, estanterías con género, mostrador; saltan y dan las gracias al comprar.
// Abajo, dos pestañas (L/R): Comprar y Mochila. Lo comprado se usa en la ficha de cada
// Pokémon (Colección → SELECT). En Halloween, además, los disfraces de bruja (caramelos).
#include <3ds.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "core/abilities.h"
#include "core/halloween.h"
#include "core/i18n.h"
#include "core/mons.h"
#include "core/progress.h"
#include "platform/3ds/app.h"
#include "platform/3ds/app_ui.h"
#include "platform/3ds/audio3ds.h"
#include "platform/3ds/hw3ds.h"

namespace ps {

namespace {
// Filas de las listas de abajo.
enum ShopRow : int8_t { SR_HEAD, SR_ITEM, SR_TM, SR_SKIN, SR_BAG_ITEM, SR_BAG_TM, SR_BAG_CANDY, SR_EMPTY };
struct Row { ShopRow kind; int16_t id; const char* text; };
constexpr int kMaxRows = 96;
constexpr float kListY = 44, kListH = 172, kHeadH = 16, kRowH = 30;
constexpr float kShelfY[2] = {84, 112};
const u32 C_COIN = C2D_Color32(0xff, 0xd2, 0x3f, 255);
const u32 C_OK = C2D_Color32(0x5f, 0xe0, 0x8a, 255);
const u32 C_CANDY = C2D_Color32(255, 138, 42, 255);

float row_h(const Row& r) { return r.kind == SR_HEAD ? kHeadH : r.kind == SR_EMPTY ? 54 : kRowH; }
bool selectable(const Row& r) { return r.kind != SR_HEAD && r.kind != SR_EMPTY; }

// Halloween: los disfraces que se venden (en el orden de kSkins).
int sellable(int* out) {
  int n = 0;
  for (int k = 0; k < kSkinCount; ++k) if (kSkins[k].sell) out[n++] = k;
  return n;
}

// Las MT de la Mochila, por nombre.
int bag_tms(const Save& s, int* out) {
  int n = 0;
  for (int i = 0; i < kTmCount; ++i) if (s.tms[i] > 0) out[n++] = i;
  std::sort(out, out + n, [](int a, int b) { return std::strcmp(tr(kMoves[kTms[a].move].name), tr(kMoves[kTms[b].move].name)) < 0; });
  return n;
}

// Qué hay en la estantería (dibujos de la web: Menta, Baya, MT, Cápsulas, Poké Ball).
const char* const kShelf[] = {"mint", "berry", "tm", "capsule", "ball", "capsuleh", "tm", "mint", "berry", "capsule", "tm", "ball", "mint", "capsuleh"};
}  // namespace

int App::shop_rows(void* out_) {
  Row* out = static_cast<Row*>(out_);
  int n = 0;
  auto add = [&](ShopRow k, int id, const char* t = nullptr) { if (n < kMaxRows) out[n++] = Row{k, int16_t(id), t}; };
  if (shop_tab_ == 0) {
    if (hw3ds::on()) {   // (sólo en Halloween: los disfraces de bruja, por caramelos)
      add(SR_HEAD, 0, "Halloween: skins de bruja");
      int list[8];
      const int ns = sellable(list);
      for (int i = 0; i < ns; ++i) add(SR_SKIN, list[i]);
    }
    add(SR_HEAD, 0, "Objetos");
    for (int k = 0; k < BAG_COUNT; ++k) add(SR_ITEM, k);
    add(SR_HEAD, 0, "MT de hoy");
    for (int i = 0; i < shop_ntoday_; ++i) add(SR_TM, shop_today_[i]);
  } else {
    int tms[kMaxTms];
    const int nt = bag_tms(*s_, tms);
    bool any = nt > 0;
    for (int k = 0; k < BAG_COUNT; ++k) if (s_->bag[k] > 0) any = true;
    const bool candy = hw3ds::on() && s_->candy > 0;   // (fuera de Halloween, los caramelos no se ven)
    if (!any && !candy) { add(SR_EMPTY, 0, "Tu Mochila está vacía. Compra algo en la pestaña Comprar."); return n; }
    if (any || candy) add(SR_HEAD, 0, "Objetos");
    for (int k = 0; k < BAG_COUNT; ++k) if (s_->bag[k] > 0) add(SR_BAG_ITEM, k);
    if (candy) add(SR_BAG_CANDY, 0);
    if (nt) add(SR_HEAD, 0, "MT");
    for (int i = 0; i < nt; ++i) add(SR_BAG_TM, tms[i]);
  }
  return n;
}

// Cuántos de tus Pokémon pueden aprender cada MT (al abrir y al enseñar una).
void App::shop_counts() {
  for (int i = 0; i < kTmCount && i < kMaxTms; ++i) shop_learn_[i] = 0;
  for (int i = 0; i < kDexCount; ++i) {
    const int d = kDex[i].dex;
    if (!s_->owns(d)) continue;
    const uint64_t can = kDexTms[d] & ~tm_natural_bits(d) & ~s_->ptm[d];
    for (int t = 0; t < kTmCount && t < kMaxTms; ++t) if ((can >> t) & 1u) shop_learn_[t]++;
  }
}

void App::shop_say(const char* s) {
  std::snprintf(shop_say_, sizeof(shop_say_), "%s", s);
  shop_say_t_ = 4.5f;
}

void App::open_shop() {
  shop_tab_ = 0;
  shop_sel_ = -1;
  shop_scroll_ = 0;
  shop_cheer_t_ = 0;
  shop_nbits_ = 0;
  shop_t0_ = t_;
  char day[12];
  shop_day(hw3ds::utc_ms(), day);
  shop_ntoday_ = shop_daily(*s_, day, shop_today_);
  std::snprintf(shop_day_, sizeof(shop_day_), "%s", day);
  shop_counts();
  shop_say("¡Bienvenido a la Tienda Kecleon!");
  // Las MT de hoy ya vistas (el punto rojo del menú se quita).
  if (std::strcmp(st_->shop_seen, day) != 0) { std::snprintf(st_->shop_seen, sizeof(st_->shop_seen), "%s", day); settings_write(dir_, *st_); }
  go(SC_SHOP);
}

bool App::shop_new_day() const {
  char day[12];
  shop_day(hw3ds::utc_ms(), day);
  return std::strcmp(st_->shop_seen, day) != 0;
}

// Saltos y confeti en la escena de arriba (y los Kecleon miran al que compra).
void App::shop_cheer() {
  shop_cheer_t_ = 1.2f;
  static const u32 C[6] = {0xff5f6d, 0xffd23f, 0x5fe08a, 0x5fc8ff, 0xc47bff, 0xffffff};
  for (int i = 0; i < 26 && shop_nbits_ < kShopBits; ++i) {
    ShopBit& b = shop_bits_[shop_nbits_++];
    b.x = 116 + urng_.range(-60, 60);
    b.y = 70 + urng_.range(-10, 10);
    b.vx = urng_.range(-70, 70);
    b.vy = urng_.range(-150, -40);
    b.t = 0;
    b.life = urng_.range(0.9f, 1.6f);
    b.c = C[i % 6];
  }
}

void App::in_shop(const UiIn& in) {
  Row rows[kMaxRows];
  const int n = shop_rows(rows);
  if (in.down & KEY_B) { go(SC_MENU); return; }
  // Pestañas: L/R o tocándolas.
  int tab = shop_tab_;
  if (in.down & (KEY_L | KEY_R)) tab ^= 1;
  if (in_rect(&in, 8, 24, 150, 18)) tab = 0;
  if (in_rect(&in, 162, 24, 150, 18)) tab = 1;
  if (tab != shop_tab_) { shop_tab_ = tab; shop_sel_ = -1; shop_scroll_ = 0; audio::play("select"); return; }
  // La primera fila que se puede elegir.
  auto first = [&]() { for (int i = 0; i < n; ++i) if (selectable(rows[i])) return i; return -1; };
  if (shop_sel_ < 0 || shop_sel_ >= n || !selectable(rows[shop_sel_])) shop_sel_ = first();
  if (shop_sel_ < 0) return;
  auto step = [&](int d) {
    for (int i = shop_sel_ + d; i >= 0 && i < n; i += d) if (selectable(rows[i])) { shop_sel_ = i; audio::play("hover"); return; }
  };
  if (in.rep & KEY_DUP) step(-1);
  if (in.rep & KEY_DDOWN) step(1);
  // Toques: elegir una fila; tocar la elegida es como A.
  bool act = (in.down & KEY_A) != 0;
  {
    float y = kListY - shop_scroll_;
    for (int i = 0; i < n; ++i) {
      const float h = row_h(rows[i]);
      if (selectable(rows[i]) && y + h > kListY && y < kListY + kListH && in_rect(&in, 8, std::max(y, kListY), 304, std::min(y + h, kListY + kListH) - std::max(y, kListY))) {
        if (shop_sel_ == i) act = true; else { shop_sel_ = i; audio::play("hover"); }
      }
      y += h;
    }
  }
  // Que la elegida se vea entera.
  {
    float y = 0;
    for (int i = 0; i < shop_sel_; ++i) y += row_h(rows[i]);
    const float h = row_h(rows[shop_sel_]);
    // (y la cabecera de su sección, si es la primera fila de ella)
    const float top = shop_sel_ > 0 && rows[shop_sel_ - 1].kind == SR_HEAD ? y - kHeadH : y;
    if (top < shop_scroll_) shop_scroll_ = top;
    if (y + h > shop_scroll_ + kListH) shop_scroll_ = y + h - kListH;
  }
  if (!act) return;
  const Row& r = rows[shop_sel_];
  switch (r.kind) {
    case SR_ITEM: {
      const ShopBuy b = shop_buy_item(*s_, r.id);
      if (b == BUY_OK) {
        audio::play("coin");
        shop_cheer();
        shop_say(urng_.next() < 0.5f ? "¡Muchas gracias!" : "¡Gracias, vuelve pronto!");
        char t[96];
        std::snprintf(t, sizeof(t), "%s", tr("¡Gracias por tu compra! Está en tu Mochila."));
        g_notices.push(kShopItems[r.id].name, t, -1);
        save_now();
      } else {
        audio::play("error");
        g_notices.push(b == BUY_NO_COINS ? "No tienes Pokémonedas suficientes." : "Ya tienes muchas: úsalas primero.", "", -1);
      }
      break;
    }
    case SR_TM: {
      const ShopBuy b = shop_buy_tm(*s_, r.id, shop_day_);
      if (b == BUY_OK) {
        audio::play("coin");
        shop_cheer();
        shop_say(urng_.next() < 0.5f ? "¡Muchas gracias!" : "¡Gracias, vuelve pronto!");
        char t[96];
        std::snprintf(t, sizeof(t), "%s %s", tr("MT"), tr(kMoves[kTms[r.id].move].name));
        g_notices.push(t, "¡Gracias por tu compra! Está en tu Mochila.", -1);
        save_now();
      } else {
        audio::play("error");
        g_notices.push(b == BUY_NO_COINS ? "No tienes Pokémonedas suficientes." : b == BUY_GONE ? "Esa MT ya no está a la venta." : "Ya tienes muchas: úsalas primero.", "", -1);
      }
      break;
    }
    case SR_SKIN: {
      const SkinDef& sk = kSkins[r.id];
      if (s_->skin_at[r.id] > 0) { audio::play("error"); g_notices.push("Ya tienes esta skin.", "Póntela o quítatela en la Colección con X", -1); break; }
      switch (skin_buy(*s_, r.id, hw3ds::on(), now_ms())) {
        case SKIN_OK: {
          audio::play("legend");
          shop_cheer();
          shop_say("¡Feliz Halloween! Póntela en la ficha del Pokémon.");
          char t[96];
          std::snprintf(t, sizeof(t), "¡%s es tuyo!", sk.name);
          g_notices.push(t, s_->owns(sk.dex) ? "Ya lo lleva puesto (cámbialo en la Colección con X)" : "Se lo pondrá cuando consigas a ese Pokémon", -1);
          save_now();
          break;
        }
        case SKIN_NO_CANDY: audio::play("error"); g_notices.push("No tienes caramelos suficientes.", "Los sueltan los Pokémon Fantasma y las calabazas", -1); break;
        case SKIN_NOT_EVENT: audio::play("error"); g_notices.push("Las skins de bruja sólo se venden en Halloween.", "", -1); break;
        default: break;
      }
      break;
    }
    case SR_BAG_ITEM: case SR_BAG_TM: {
      // A la ficha de tu compañero, en la pestaña que toca (ahí se usa).
      audio::play("confirm");
      open_ficha(s_->partner, r.kind == SR_BAG_TM ? FT_TM : r.id == BAG_MINT ? FT_NATURE : FT_ABILITY);
      break;
    }
    default: break;
  }
}

void App::bottom_shop() {
  Row rows[kMaxRows];
  const int n = shop_rows(rows);
  // La lista (lo que se sale de su caja lo tapan la cabecera y el pie, abajo).
  char b[160];
  float y = kListY - shop_scroll_;
  for (int i = 0; i < n; ++i) {
    const Row& r = rows[i];
    const float h = row_h(r);
    if (y + h < kListY || y > kListY + kListH) { y += h; continue; }
    const bool sel = i == shop_sel_;
    if (r.kind == SR_HEAD) {
      r_->text(r.text, 10, y + 1, 0.36f, C_GOLD);
      C2D_DrawRectSolid(14 + r_->text_width(r.text, 0.36f), y + 8, 0, 296 - r_->text_width(r.text, 0.36f), 1, C2D_Color32(255, 203, 61, 70));
      y += h;
      continue;
    }
    if (r.kind == SR_EMPTY) {
      r_->text_wrap(r.text, 160, y + 6, 0.38f, C_MUTED, 280, 1);
      y += h;
      continue;
    }
    C2D_DrawRectSolid(8, y + 1, 0, 304, h - 2, sel ? C_SEL : C_PANEL);
    if (sel) C2D_DrawRectSolid(8, y + 1, 0, 3, h - 2, C_GOLD);
    // Icono (24 px) en su cuadrito.
    C2D_DrawRectSolid(14, y + 3, 0, 26, 26, C2D_Color32(10, 14, 24, 160));
    auto icon = [&](const char* name) { const int ic = r_->icons.find(name); if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), 15, y + 4, 0, nullptr, 2, 2); };
    auto price = [&](int p, bool can, const char* icon_name, u32 col) {
      std::snprintf(b, sizeof(b), "%d", p);
      const float w = r_->text_width(b, 0.4f);
      r_->text(b, 306, y + 8, 0.4f, can ? col : C2D_Color32(140, 110, 70, 255), 2);
      const int ic = r_->icons.find(icon_name);
      if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), 306 - w - 15, y + 9, 0);
    };
    switch (r.kind) {
      case SR_ITEM: {
        const ShopItemDef& it = kShopItems[r.id];
        icon(it.icon);
        r_->text_fit(it.name, 46, y + 2, 186, 0.4f, sel ? C_GOLD : C_TEXT);
        if (s_->bag[r.id] > 0) std::snprintf(b, sizeof(b), "%s", tr("tienes {x}"));
        if (s_->bag[r.id] > 0) { char t[32]; std::snprintf(t, sizeof(t), "%d", int(s_->bag[r.id])); std::string q = b; const size_t p = q.find("{x}"); if (p != std::string::npos) q.replace(p, 3, t); r_->text_fit(q.c_str(), 46, y + 16, 180, 0.32f, C_GOLD); }
        else r_->text_fit(r.id == BAG_MINT ? "Cambia la naturaleza" : r.id == BAG_CAP ? "Cambia la habilidad (no la oculta)" : "Cambia la habilidad, incluida la oculta", 46, y + 16, 180, 0.32f, C_MUTED);
        price(it.price, s_->coins >= it.price, "coin", C_COIN);
        break;
      }
      case SR_TM: {
        const MoveDef& d = kMoves[kTms[r.id].move];
        const int ic = r_->move_icon[kTms[r.id].move];
        if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), 15, y + 4, 0, nullptr, 1.5f, 1.5f);
        C2D_DrawRectSolid(8, y + 1, 0, 3, h - 2, sel ? C_GOLD : rgb(kTypeColor[d.type]));
        std::snprintf(b, sizeof(b), "%s %s", tr("MT"), tr(d.name));
        r_->text_fit(b, 46, y + 2, 150, 0.4f, sel ? C_GOLD : C_TEXT);
        // Tipo (color) a la derecha del nombre.
        const float tx = 46 + std::min(150.0f, r_->text_width(b, 0.4f)) + 4;
        C2D_DrawRectSolid(tx, y + 5, 0, 30, 9, rgb(kTypeColor[d.type]));
        r_->text_fit(kTypeName[d.type], tx + 15, y + 4, 28, 0.28f, C_TEXT, false, 1);
        const int dex = s_->partner;
        u32 col = C_MUTED;
        if (dex > 0 && tm_can_learn(*s_, dex, r.id)) { std::snprintf(b, sizeof(b), "%s", tr("{x} la puede aprender")); col = C_GOLD; }
        else if (dex > 0 && tm_taught(*s_, dex, r.id)) std::snprintf(b, sizeof(b), "%s", tr("{x} ya la sabe"));
        else std::snprintf(b, sizeof(b), "%s", tr("La pueden aprender {x} de tus Pokémon"));
        std::string q = b;
        const size_t p = q.find("{x}");
        if (p != std::string::npos) {
          char t[32];
          if (col == C_GOLD || q.find(tr("ya la sabe")) != std::string::npos) std::snprintf(t, sizeof(t), "%s", mon_by_dex(dex)->name);
          else std::snprintf(t, sizeof(t), "%d", shop_learn_[r.id]);
          q.replace(p, 3, t);
        }
        if (s_->tms[r.id] > 0) { char t[24]; std::snprintf(t, sizeof(t), " · ×%d", int(s_->tms[r.id])); q += t; }
        r_->text_fit(q.c_str(), 46, y + 16, 196, 0.32f, col);
        price(tm_price(r.id), s_->coins >= tm_price(r.id), "coin", C_COIN);
        break;
      }
      case SR_SKIN: {
        const SkinDef& sk = kSkins[r.id];
        draw_mon(sk.sprite, false, 27, y + 28, 0.55f);
        r_->text_fit(sk.name, 46, y + 2, 186, 0.4f, sel ? C_GOLD : C_TEXT);
        const bool owned = s_->skin_at[r.id] > 0;
        r_->text_fit(owned ? "Comprada. Póntela o quítatela en la ficha del Pokémon." : "Sombrero de bruja, para siempre", 46, y + 16, 190, 0.32f, owned ? C_OK : C_MUTED);
        if (!owned) price(kSkinPrice, s_->candy >= kSkinPrice, "candy", C_CANDY);
        break;
      }
      case SR_BAG_ITEM: {
        const ShopItemDef& it = kShopItems[r.id];
        icon(it.icon);
        r_->text_fit(it.name, 46, y + 2, 200, 0.4f, sel ? C_GOLD : C_TEXT);
        r_->text_fit(r.id == BAG_MINT ? "A: a la ficha de tu compañero (Naturaleza)" : "A: a la ficha de tu compañero (Habilidad)", 46, y + 16, 200, 0.32f, C_MUTED);
        std::snprintf(b, sizeof(b), "×%d", int(s_->bag[r.id]));
        r_->text(b, 306, y + 8, 0.42f, C_GOLD, 2);
        break;
      }
      case SR_BAG_CANDY: {
        icon("candy");
        r_->text_fit("Caramelos de Halloween", 46, y + 2, 200, 0.4f, sel ? C_GOLD : C_TEXT);
        r_->text_fit("Para las skins de bruja (pestaña Comprar)", 46, y + 16, 200, 0.32f, C_MUTED);
        std::snprintf(b, sizeof(b), "×%d", int(s_->candy));
        r_->text(b, 306, y + 8, 0.42f, C_CANDY, 2);
        break;
      }
      case SR_BAG_TM: {
        const MoveDef& d = kMoves[kTms[r.id].move];
        const int ic = r_->move_icon[kTms[r.id].move];
        if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), 15, y + 4, 0, nullptr, 1.5f, 1.5f);
        C2D_DrawRectSolid(8, y + 1, 0, 3, h - 2, sel ? C_GOLD : rgb(kTypeColor[d.type]));
        std::snprintf(b, sizeof(b), "%s %s", tr("MT"), tr(d.name));
        r_->text_fit(b, 46, y + 2, 200, 0.4f, sel ? C_GOLD : C_TEXT);
        std::string q = shop_learn_[r.id] ? tr("La pueden aprender {x} de tus Pokémon") : tr("Ninguno de tus Pokémon la puede aprender");
        const size_t p = q.find("{x}");
        if (p != std::string::npos) { char t[16]; std::snprintf(t, sizeof(t), "%d", shop_learn_[r.id]); q.replace(p, 3, t); }
        r_->text_fit(q.c_str(), 46, y + 16, 200, 0.32f, shop_learn_[r.id] ? C_MUTED : C2D_Color32(200, 120, 110, 255));
        std::snprintf(b, sizeof(b), "×%d", int(s_->tms[r.id]));
        r_->text(b, 306, y + 8, 0.42f, C_GOLD, 2);
        break;
      }
      default: break;
    }
    y += h;
  }
  C2D_DrawRectSolid(0, 0, 0, BW, kListY, C_BG);
  C2D_DrawRectSolid(0, kListY + kListH, 0, BW, BH - kListY - kListH, C_BG);
  header("Tienda Kecleon");
  // Pestañas.
  static const char* const T[2] = {"Comprar", "Mochila"};
  for (int i = 0; i < 2; ++i) {
    const float x = 8 + i * 154;
    const bool on = shop_tab_ == i;
    C2D_DrawRectSolid(x, 24, 0, 150, 18, on ? C_SEL : C2D_Color32(22, 30, 48, 255));
    if (on) C2D_DrawRectSolid(x, 40, 0, 150, 2, C_GOLD);
    const int ic = r_->icons.find(i == 0 ? "shop" : "bag");
    const float tw = r_->text_width(T[i], 0.4f);
    if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), x + 75 - tw / 2 - 9, 27, 0);
    r_->text(T[i], x + 75 + 7, 25, 0.4f, on ? C_GOLD : C_MUTED, 1);
  }
  r_->text("L", 14, 26, 0.32f, C_MUTED, 0, false);
  r_->text("R", 306, 26, 0.32f, C_MUTED, 2, false);
  // Barra de desplazamiento.
  float total = 0;
  for (int i = 0; i < n; ++i) total += row_h(rows[i]);
  if (total > kListH) {
    C2D_DrawRectSolid(314, kListY, 0, 3, kListH, C2D_Color32(0, 0, 0, 120));
    C2D_DrawRectSolid(314, kListY + kListH * shop_scroll_ / total, 0, 3, kListH * kListH / total, C2D_Color32(255, 203, 61, 160));
  }
  r_->text(shop_tab_ == 0 ? "A: comprar · L/R: Mochila · B: volver" : "A: usarlo (ficha) · L/R: Comprar · B: volver", 160, 226, 0.32f, C_MUTED, 1);
}

// ---------------- arriba: el puesto de los Kecleon ----------------

void App::shop_stall(float dt) {
  // (avanza el confeti y los saltos; lo llama top_shop)
  if (shop_say_t_ > 0) shop_say_t_ -= dt;
  if (shop_cheer_t_ > 0) shop_cheer_t_ -= dt;
  for (int i = 0; i < shop_nbits_;) {
    ShopBit& b = shop_bits_[i];
    b.t += dt;
    if (b.t >= b.life) { shop_bits_[i] = shop_bits_[--shop_nbits_]; continue; }
    b.vy += 260 * dt;
    b.x += b.vx * dt;
    b.y += b.vy * dt;
    ++i;
  }
}

void App::top_shop() {
  shop_stall(1.0f / 60);
  const float t = t_;
  // Cielo de tarde (bandas), nubes y colinas.
  static const u32 SKY[6] = {0x6fb8f0, 0x84c6f3, 0x9ad3f5, 0xb2def6, 0xc9e8f6, 0xdff1f5};
  for (int i = 0; i < 6; ++i) C2D_DrawRectSolid(0, i * 30, 0, TW, 30, rgb(SKY[i]));
  for (int i = 0; i < 4; ++i) {
    const float cx = std::fmod(40 + i * 113 + t * (6 + i * 2), TW + 80) - 40, cy = 18 + (i * 37) % 50;
    C2D_DrawEllipseSolid(cx, cy, 0, 46, 14, C2D_Color32(255, 255, 255, 200));
    C2D_DrawEllipseSolid(cx + 12, cy - 6, 0, 28, 14, C2D_Color32(255, 255, 255, 220));
  }
  for (int i = 0; i < 9; ++i) C2D_DrawEllipseSolid(-30 + i * 52, 150, 0, 70, 50, i & 1 ? rgb(0x4fa56a) : rgb(0x5cb877));
  C2D_DrawRectSolid(0, 175, 0, TW, 65, rgb(0x6cc46f));   // hierba
  C2D_DrawRectSolid(0, 196, 0, TW, 44, rgb(0xd9b98a));   // camino
  for (int i = 0; i < 26; ++i) C2D_DrawRectSolid(float((i * 61) % 400), 200 + float((i * 17) % 34), 0, 3, 2, rgb(0xb89466));
  // El puesto (a la izquierda; a la derecha, la ficha de lo elegido).
  const float x0 = 16, x1 = 216, roof = 28;
  C2D_DrawRectSolid(x0 + 6, roof + 18, 0, x1 - x0 - 12, 104, rgb(0xc89a62));   // pared de tablas
  for (int i = 0; i < 7; ++i) C2D_DrawRectSolid(x0 + 6, roof + 30 + i * 14, 0, x1 - x0 - 12, 1, rgb(0xa77c48));
  // Estanterías con género (y el que se acaba de comprar, brillando).
  for (int s = 0; s < 2; ++s) {
    C2D_DrawRectSolid(x0 + 26, kShelfY[s] + 13, 0, x1 - x0 - 52, 4, rgb(0x7a5230));
    for (int k = 0; k < 8; ++k) {
      const int ic = r_->icons.find(kShelf[(k + s * 5) % 14]);
      const float bob = shop_cheer_t_ > 0 && ((k + s) & 1) ? -std::fabs(std::sin(shop_cheer_t_ * 9)) * 3 : 0;
      if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), x0 + 30 + k * 18, kShelfY[s] + bob, 0);
    }
  }
  // Farolillos (se encienden y titilan).
  for (int k = 0; k < 2; ++k) {
    const float lx = k ? x1 - 14 : x0 + 14, ly = roof + 26;
    const float g = 0.7f + 0.3f * std::sin(t * 5 + k);
    C2D_DrawCircleSolid(lx, ly, 0, 10, rgbf(0xffd27a, 0.25f * g));
    C2D_DrawRectSolid(lx - 4, ly - 5, 0, 8, 10, rgbf(0xffb347, g));
  }
  // Los dos Kecleon detrás del mostrador: pasean, saltan al comprar y miran a quien compra.
  for (int k = 0; k < 2; ++k) {
    const float base = k ? 156 : 72;
    const float walk = std::sin(t * 0.7f + k * 2.1f) * 18;
    const float hop = shop_cheer_t_ > 0 ? std::fabs(std::sin((shop_cheer_t_ + k * 0.15f) * 8)) * 12 : std::fmod(t + k * 1.7f, 5.0f) < 0.3f ? 4 : 0;
    const int dir = shop_cheer_t_ > 0 ? 0 : (std::cos(t * 0.7f + k * 2.1f) > 0 ? (k ? 6 : 2) : (k ? 2 : 6));
    draw_mon(352, false, x0 + base + walk, 161 - hop, 1.7f, k == 1 ? 4 : 0, dir);
  }
  // Mostrador con su paño, delante.
  C2D_DrawRectSolid(x0, 158, 0, x1 - x0, 34, rgb(0x8c5a2b));
  C2D_DrawRectSolid(x0, 158, 0, x1 - x0, 6, rgb(0xa86e38));
  C2D_DrawRectSolid(x0 + 20, 164, 0, x1 - x0 - 40, 18, rgb(0x5fe08a));
  C2D_DrawRectSolid(x0 + 20, 182, 0, x1 - x0 - 40, 2, rgb(0x3d9a5c));
  r_->text("TIENDA KECLEON", (x0 + x1) / 2, 164, 0.42f, C2D_Color32(20, 60, 34, 255), 1, false);
  // Postes y toldo de rayas (verde y blanco), con borde ondulado.
  C2D_DrawRectSolid(x0, roof + 10, 0, 8, 150, rgb(0x7a5230));
  C2D_DrawRectSolid(x1 - 8, roof + 10, 0, 8, 150, rgb(0x7a5230));
  for (int i = 0; i < 10; ++i) {
    const float sx = x0 - 6 + i * 21.2f;
    C2D_DrawRectSolid(sx, roof, 0, 21.2f, 18, i & 1 ? rgb(0xf4f8ff) : rgb(0x5fe08a));
    C2D_DrawCircleSolid(sx + 10.6f, roof + 18, 0, 10.6f, i & 1 ? rgb(0xf4f8ff) : rgb(0x5fe08a));
  }
  C2D_DrawRectSolid(x0 - 6, roof - 4, 0, x1 - x0 + 12, 5, rgb(0x3d9a5c));
  // Confeti.
  for (int i = 0; i < shop_nbits_; ++i) {
    const ShopBit& b = shop_bits_[i];
    const float a = 1 - b.t / b.life;
    C2D_DrawRectSolid(b.x, b.y, 0, 3, std::sin(b.t * 12) > 0 ? 2 : 3, rgbf(b.c, std::min(1.0f, a * 2)));
  }
  // Bocadillo del Kecleon morado.
  if (shop_say_t_ > 0 && shop_say_[0]) {
    const float a = std::min(1.0f, shop_say_t_ * 2);
    const char* s = tr(shop_say_);
    const float w = std::min(196.0f, r_->text_width(s, 0.36f) + 14);
    const float bx = 116 - w / 2, by = 6;
    C2D_DrawRectSolid(bx, by, 0, w, 18, rgbf(0xffffff, 0.95f * a));
    C2D_DrawTriangle(160, by + 18, rgbf(0xffffff, a), 168, by + 18, rgbf(0xffffff, a), 170, by + 25, rgbf(0xffffff, a), 0);
    r_->text_fit(s, 116, by + 2, w - 8, 0.36f, C2D_Color32(30, 34, 48, 255), false, 1);
  }

  // ---- a la derecha: lo elegido ----
  Row rows[kMaxRows];
  const int n = shop_rows(rows);
  const float px = 226, pw = 168;
  C2D_DrawRectSolid(px, 8, 0, pw, 224, C2D_Color32(10, 14, 26, 215));
  C2D_DrawRectSolid(px, 8, 0, pw, 2, C_GOLD);
  char b[160];
  if (shop_sel_ < 0 || shop_sel_ >= n || !selectable(rows[shop_sel_])) {
    r_->text_wrap(shop_tab_ ? "Para usarlo, abre la colección, elige un Pokémon y pulsa Menta, Cápsula o Enseñar MT en su ficha."
                            : "¡Bienvenido! Mentas, Cápsulas y MT. Lo que compres va a tu Mochila.",
                  px + pw / 2, 30, 0.36f, C_TEXT, pw - 16, 1);
  } else {
    const Row& r = rows[shop_sel_];
    float y = 16;
    // El nombre, a la derecha del icono: los largos («Cápsula Habilidad Oculta») en dos
    // líneas; lo de debajo empieza donde acaba (antes se montaba encima).
    auto title = [&](const char* name, float yy) {
      const float w = pw - 54;
      r_->text_wrap(name, px + 50, yy + 2, 0.42f, C_GOLD, w);
      // Las líneas de verdad (palabra a palabra, como corta el texto).
      const std::string t = tr(name);
      int lines = 1;
      std::string line;
      for (size_t i = 0; i <= t.size(); ++i) {
        const size_t e = std::min(t.find(' ', i), t.size());
        const std::string word = t.substr(i, e - i);
        const std::string cand = line.empty() ? word : line + " " + word;
        if (!line.empty() && r_->text_width(cand.c_str(), 0.42f) > w) { ++lines; line = word; }
        else line = cand;
        i = e;
      }
      return std::max(44.0f, 8.0f + lines * 17.0f);
    };
    auto big_icon = [&](const char* name) { const int ic = r_->icons.find(name); if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), px + 8, y, 0, nullptr, 3, 3); };
    if (r.kind == SR_ITEM || r.kind == SR_BAG_ITEM) {
      const ShopItemDef& it = kShopItems[r.id];
      big_icon(it.icon);
      y += title(it.name, y);
      y += r_->text_wrap(it.desc, px + 8, y, 0.36f, C_TEXT, pw - 16) + 8;
      std::snprintf(b, sizeof(b), "%s: %d", tr("En tu Mochila"), int(s_->bag[r.id]));
      r_->text(b, px + 8, y, 0.36f, C_GOLD); y += 18;
      // Lo que tiene ahora tu compañero (lo que cambiaría).
      const int dex = s_->partner;
      if (dex > 0) {
        if (r.id == BAG_MINT) {
          const int nat = nature_of(*s_, dex);
          std::snprintf(b, sizeof(b), "%s: %s", mon_by_dex(dex)->name, nature_name(nat));
        } else {
          const AbilPick ab = abil_of(*s_, dex);
          std::snprintf(b, sizeof(b), "%s: %s", mon_by_dex(dex)->name, ab.id >= 0 ? abil_name(ab.id) : "—");
        }
        r_->text_wrap(b, px + 8, y, 0.34f, C_MUTED, pw - 16);
      }
    } else if (r.kind == SR_TM || r.kind == SR_BAG_TM) {
      const int mv = kTms[r.id].move;
      const MoveDef& d = kMoves[mv];
      const int ic = r_->move_icon[mv];
      if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), px + 8, y, 0, nullptr, 2.5f, 2.5f);
      std::snprintf(b, sizeof(b), "%s %s", tr("MT"), tr(d.name));
      y += title(b, y);
      C2D_DrawRectSolid(px + 8, y, 0, 60, 14, rgb(kTypeColor[d.type]));
      r_->text(kTypeName[d.type], px + 38, y, 0.32f, C_TEXT, 1);
      const bool buff = d.kind == K_BUFF || d.cat < 0;
      C2D_DrawRectSolid(px + 72, y, 0, 64, 14, buff ? C2D_Color32(30, 40, 62, 255) : d.cat == CAT_PHYSICAL ? C2D_Color32(120, 52, 30, 255) : C2D_Color32(40, 60, 130, 255));
      r_->text(buff ? "Mejora" : d.cat == CAT_PHYSICAL ? "Físico" : "Especial", px + 104, y, 0.32f, buff ? C_MUTED : C_GOLD, 1);
      y += 20;
      y += r_->text_wrap(d.desc, px + 8, y, 0.34f, C_TEXT, pw - 16) + 6;
      std::string q = shop_learn_[r.id] ? tr("La pueden aprender {x} de tus Pokémon") : tr("Ninguno de tus Pokémon la puede aprender");
      const size_t p = q.find("{x}");
      if (p != std::string::npos) { char t2[16]; std::snprintf(t2, sizeof(t2), "%d", shop_learn_[r.id]); q.replace(p, 3, t2); }
      y += r_->text_wrap(q.c_str(), px + 8, y, 0.34f, shop_learn_[r.id] ? C_OK : C2D_Color32(200, 120, 110, 255), pw - 16) + 4;
      if (s_->tms[r.id] > 0) { std::snprintf(b, sizeof(b), "%s: %d", tr("En tu Mochila"), int(s_->tms[r.id])); r_->text(b, px + 8, y, 0.34f, C_GOLD); }
      // Hasta cuándo están estas MT (medianoche en España).
      if (r.kind == SR_TM) {
        int yy, mm, dd;
        const int64_t now = hw3ds::utc_ms();
        madrid_date(now + 86400000LL, yy, mm, dd);
        const int64_t left = madrid_midnight(yy, mm, dd) - now;
        char tt[32];
        hw_time(left > 0 ? left : 0, tt, sizeof(tt));
        std::snprintf(b, sizeof(b), "%s %s", tr("MT nuevas en"), tt);
        r_->text(b, px + pw / 2, 214, 0.34f, C2D_Color32(0x9a, 0xe6, 0xff, 255), 1);
      }
    } else if (r.kind == SR_SKIN) {
      const SkinDef& sk = kSkins[r.id];
      C2D_DrawEllipseSolid(px + 30, 150, 0, 110, 22, C2D_Color32(0, 0, 0, 80));
      draw_mon(sk.sprite, false, px + pw / 2, 166, 2.4f);
      r_->text(sk.name, px + pw / 2, 18, 0.46f, C_CANDY, 1);
      std::snprintf(b, sizeof(b), "%d %s", int(s_->candy), tr("caramelos"));
      r_->text(b, px + pw / 2, 190, 0.38f, C_CANDY, 1);
      bool on; int64_t ms;
      if (hw3ds::countdown(on, ms) && on) {
        char tt[32];
        hw_time(ms, tt, sizeof(tt));
        std::snprintf(b, sizeof(b), "Halloween termina en %s", tt);
        r_->text_fit(b, px + pw / 2, 210, pw - 10, 0.32f, C2D_Color32(255, 200, 140, 255), false, 1);
      }
    } else if (r.kind == SR_BAG_CANDY) {
      big_icon("candy");
      r_->text_wrap("Caramelos de Halloween", px + 50, y + 2, 0.42f, C_CANDY, pw - 54);
      y += 44;
      r_->text_wrap("Lo sueltan los Pokémon Fantasma y las calabazas en Halloween. Con ellos se compran las skins de bruja en la Tienda Kecleon.", px + 8, y, 0.34f, C_TEXT, pw - 16);
    }
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
// Con MT nuevas sin ver (cada día), un punto rojo.
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
  if (shop_new_day()) {
    const float k = 0.75f + 0.25f * std::sin(t_ * 5);
    C2D_DrawCircleSolid(x0 + w - 6, y0 + 6, 0, 5 * k + 1, C2D_Color32(255, 95, 109, 255));
  }
}

// «2 d 11:03:42» o «11:03:42» (cuenta atrás de Halloween y de las MT del día).
void App::hw_time(int64_t ms, char* out, int cap) {
  const int64_t s = ms / 1000;
  const int d = int(s / 86400), h = int(s / 3600 % 24), m = int(s / 60 % 60), sec = int(s % 60);
  if (d > 0) std::snprintf(out, size_t(cap), "%d d %02d:%02d:%02d", d, h, m, sec);
  else std::snprintf(out, size_t(cap), "%02d:%02d:%02d", h, m, sec);
}

// Autotest «tienda»: monedas de sobra; la tienda (Comprar), compra una MT y una Menta,
// pasa a la Mochila, va a la ficha de su compañero y vuelve al menú.
void App::start_shop_demo() {
  shop_demo_ = true;
  news_t_ = 0;
  news_checked_ = cloud_asked_ = true;
  s_->coins = std::max<int32_t>(s_->coins, 9000);
  if (!s_->owns(25)) s_->owned_at[25] = now_ms();
  if (!s_->partner) s_->partner = 25;
  open_shop();
}

void App::shop_demo_step(float dt) {
  const float t0 = news_t_;
  news_t_ += dt;
  auto at = [&](float s) { return t0 < s && news_t_ >= s; };
  UiIn none;
  if (at(2)) { shop_sel_ = -1; for (int i = 0; i < 2; ++i) { UiIn d; d.rep = KEY_DDOWN; in_shop(d); } }   // la Cápsula Habilidad Oculta
  if (at(4)) { shop_sel_ = -1; for (int i = 0; i < 3; ++i) { UiIn d; d.rep = KEY_DDOWN; in_shop(d); } }   // a la primera MT
  if (at(6)) { UiIn a; a.down = KEY_A; in_shop(a); }                                                      // la compra
  if (at(9)) { shop_sel_ = -1; in_shop(none); UiIn a; a.down = KEY_A; in_shop(a); }                          // una Menta
  if (at(12)) { UiIn r; r.down = KEY_R; in_shop(r); }                                                     // Mochila
  if (at(15)) { UiIn d; d.rep = KEY_DDOWN; in_shop(d); }
  if (at(18)) { UiIn r; r.down = KEY_R; in_shop(r); for (int i = 0; i < 8; ++i) { UiIn d; d.rep = KEY_DDOWN; in_shop(d); } }   // las últimas MT
  if (at(21)) go(SC_MENU);
  if (news_t_ > 24) quit = true;
}

}  // namespace ps

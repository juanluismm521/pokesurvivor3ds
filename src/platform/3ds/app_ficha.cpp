// app_ficha.cpp — la ficha de un Pokémon en la Colección (SELECT), con pestañas (L/R):
//   Ataques     los que puede aprender, grandes y con su ficha (app.cpp)
//   Naturaleza  la suya (+10 % a un stat, −10 % a otro) y las 25; con una Menta se cambia
//   Habilidad   las suyas (la oculta, en dorado) y qué hace cada una; con una Cápsula se cambia
//   MT          las que aprende en los juegos: las aprendidas y las que tienes para enseñar
// Es donde se usa lo de la Mochila (Tienda Kecleon), como en la ficha de la web (menu.js,
// monExtras: «Menta», «Cápsula», «Cápsula Oculta» y «Enseñar MT»).
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
#include "core/stats.h"
#include "platform/3ds/app.h"
#include "platform/3ds/app_ui.h"
#include "platform/3ds/audio3ds.h"
#include "platform/3ds/hw3ds.h"

namespace ps {

namespace {
const char* const kTabName[FT_COUNT] = {"Ataques", "Naturaleza", "Habilidad", "MT"};
const u32 C_UP = C2D_Color32(0x5f, 0xe0, 0x8a, 255), C_DOWN = C2D_Color32(0xff, 0x7a, 0x7a, 255);
// Nombre corto de cada stat de las naturalezas (como en los juegos).
const char* const kStatShort[NS_COUNT] = {"Atq", "AtE", "Def", "DfE", "Vel"};
const char* const kStatLong[NS_COUNT] = {"Ataque", "At. Esp.", "Defensa", "Def. Esp.", "Velocidad"};

// «+Vel −Atq» o «neutra».
void nat_line(int nat, char* out, int cap) {
  const NatureDef& n = kNatures[nat];
  if (n.up < 0) std::snprintf(out, size_t(cap), "%s", tr("neutra"));
  else std::snprintf(out, size_t(cap), "+%s −%s", tr(kStatShort[n.up]), tr(kStatShort[n.down]));
}

// MT de la pestaña MT: las aprendidas primero y luego las que puede aprender (por nombre).
int ficha_tms(const Save& s, int dex, int* out) {
  int n = 0;
  if (dex <= 0 || dex > kMaxDex) return 0;
  const TmBits all = kDexTms[dex] & ~tm_natural_bits(dex);
  auto by_name = [](int a, int b) { return std::strcmp(tr(kMoves[kTms[a].move].name), tr(kMoves[kTms[b].move].name)) < 0; };
  for (int t = 0; t < kTmCount; ++t) if (all.test(t) && tm_taught(s, dex, t)) out[n++] = t;
  std::sort(out, out + n, by_name);
  const int k = n;
  for (int t = 0; t < kTmCount; ++t) if (all.test(t) && !tm_taught(s, dex, t)) out[n++] = t;
  std::sort(out + k, out + n, by_name);
  return n;
}
constexpr float kTmRowY = 62, kTmRowH = 26;
constexpr int kTmRows = 6;
}  // namespace

// Desde la Mochila: la ficha de ese Pokémon (en la Colección), en la pestaña que toca.
void App::open_ficha(int dex, int tab) {
  go(SC_DEX);
  const Mon* m = mon_by_dex(dex);
  if (!m || !s_->owns(dex)) return;
  gen_ = m->gen;
  filter_ = 0;
  static int16_t list[256];
  const int n = dex_list(*s_, gen_, filter_, list);
  for (int i = 0; i < n; ++i) if (list[i] == dex) cur_ = i;
  scroll_ = std::max(0, cur_ / 6 - 1);
  dex_moves_ = true;
  ficha_tab_ = tab;
  mv_cur_ = 0;
  fi_cur_ = tab == FT_NATURE ? nature_of(*s_, dex) : 0;
  fi_scroll_ = 0;
}

void App::in_ficha(const UiIn& in, int dex) {
  if (in.down & (KEY_B | KEY_SELECT)) { dex_moves_ = false; audio::play("back"); return; }
  int tab = ficha_tab_;
  if (in.down & KEY_L) tab = (tab + FT_COUNT - 1) % FT_COUNT;
  if (in.down & KEY_R) tab = (tab + 1) % FT_COUNT;
  for (int i = 0; i < FT_COUNT; ++i) if (in_rect(&in, 4 + i * 78, 24, 76, 18)) tab = i;
  if (tab != ficha_tab_) {
    ficha_tab_ = tab;
    fi_cur_ = tab == FT_NATURE ? nature_of(*s_, dex) : 0;
    fi_scroll_ = 0;
    audio::play("select");
    return;
  }
  if (ficha_tab_ == FT_MOVES) { in_dex_moves(in, dex); return; }
  const Mon* mon = mon_by_dex(dex);
  char t[128];
  if (ficha_tab_ == FT_NATURE) {
    // 25 casillas, 5 × 5.
    if (in.rep & KEY_DLEFT) fi_cur_--;
    if (in.rep & KEY_DRIGHT) fi_cur_++;
    if (in.rep & KEY_DUP) fi_cur_ -= 5;
    if (in.rep & KEY_DDOWN) fi_cur_ += 5;
    fi_cur_ = clamp(fi_cur_, 0, kNatureCount - 1);
    bool act = (in.down & KEY_A) != 0;
    for (int i = 0; i < kNatureCount; ++i)
      if (in_rect(&in, 6 + (i % 5) * 62, 62 + (i / 5) * 31, 60, 29)) { if (fi_cur_ == i) act = true; else { fi_cur_ = i; audio::play("hover"); } }
    if (!act) return;
    if (fi_cur_ == nature_of(*s_, dex)) { audio::play("error"); g_notices.push("Ya tiene esa naturaleza.", "", -1); return; }
    if (!bag_take(*s_, BAG_MINT)) { audio::play("error"); g_notices.push("No tienes Mentas.", "Cómpralas en la Tienda Kecleon (R en el menú)", -1); return; }
    nature_set(*s_, dex, fi_cur_, now_ms());
    audio::play("coin");
    std::snprintf(t, sizeof(t), "%s", tr("{x} ahora tiene naturaleza {x}"));
    std::string q = t;
    size_t p = q.find("{x}"); if (p != std::string::npos) q.replace(p, 3, mon->name);
    p = q.find("{x}"); if (p != std::string::npos) q.replace(p, 3, nature_name(fi_cur_));
    g_notices.push(q.c_str(), "", -1);
    save_now();
    return;
  }
  if (ficha_tab_ == FT_ABILITY) {
    const int16_t* l = abil_slots(dex);
    int slots[3], ns = 0;
    for (int i = 0; i < 3; ++i) if (l[i] >= 0 && !(i == 1 && l[1] == l[0])) slots[ns++] = i;
    if (!ns) return;
    if (in.rep & KEY_DUP) fi_cur_--;
    if (in.rep & KEY_DDOWN) fi_cur_++;
    fi_cur_ = clamp(fi_cur_, 0, ns - 1);
    bool act = (in.down & KEY_A) != 0;
    for (int i = 0; i < ns; ++i)
      if (in_rect(&in, 6, 62 + i * 52, 308, 50)) { if (fi_cur_ == i) act = true; else { fi_cur_ = i; audio::play("hover"); } }
    if (!act) return;
    const int slot = slots[fi_cur_];
    if (slot == abil_of(*s_, dex).slot) { audio::play("error"); g_notices.push("Ya tiene esa habilidad.", "", -1); return; }
    // La oculta, sólo con la Cápsula Oculta; las otras, con la Cápsula (o la Oculta, si no hay).
    const int item = slot == 2 ? BAG_CAPH : s_->bag[BAG_CAP] > 0 ? BAG_CAP : BAG_CAPH;
    if (!bag_take(*s_, item)) {
      audio::play("error");
      g_notices.push(slot == 2 ? "Para la oculta hace falta una Cápsula Habilidad Oculta." : "No tienes Cápsulas Habilidad.", "Cómpralas en la Tienda Kecleon (R en el menú)", -1);
      return;
    }
    abil_set(*s_, dex, slot, now_ms());
    audio::play("coin");
    std::snprintf(t, sizeof(t), "%s", tr("{x} ahora tiene la habilidad {x}"));
    std::string q = t;
    size_t p = q.find("{x}"); if (p != std::string::npos) q.replace(p, 3, mon->name);
    p = q.find("{x}"); if (p != std::string::npos) q.replace(p, 3, abil_name(l[slot]));
    g_notices.push(q.c_str(), "", -1);
    save_now();
    return;
  }
  // MT
  int tms[kMaxTms];
  const int n = ficha_tms(*s_, dex, tms);
  if (!n) return;
  if (in.rep & KEY_DUP) fi_cur_--;
  if (in.rep & KEY_DDOWN) fi_cur_++;
  fi_cur_ = clamp(fi_cur_, 0, n - 1);
  if (fi_cur_ < fi_scroll_) fi_scroll_ = fi_cur_;
  if (fi_cur_ >= fi_scroll_ + kTmRows) fi_scroll_ = fi_cur_ - kTmRows + 1;
  bool act = (in.down & KEY_A) != 0;
  for (int i = 0; i < kTmRows && fi_scroll_ + i < n; ++i)
    if (in_rect(&in, 6, kTmRowY + i * kTmRowH, 304, kTmRowH - 2)) { if (fi_cur_ == fi_scroll_ + i) act = true; else { fi_cur_ = fi_scroll_ + i; audio::play("hover"); } }
  if (!act) return;
  const int tm = tms[fi_cur_];
  if (tm_taught(*s_, dex, tm)) { audio::play("error"); g_notices.push("Ya la sabe.", "", -1); return; }
  if (s_->tms[tm] <= 0) { audio::play("error"); g_notices.push("No tienes esa MT.", "Cada día hay otras 8 en la Tienda Kecleon", -1); return; }
  if (!tm_teach(*s_, dex, tm)) { audio::play("error"); return; }
  audio::play("legend");
  std::snprintf(t, sizeof(t), "%s", tr("¡{x} ha aprendido {x}!"));
  std::string q = t;
  size_t p = q.find("{x}"); if (p != std::string::npos) q.replace(p, 3, mon->name);
  p = q.find("{x}"); if (p != std::string::npos) q.replace(p, 3, tr(kMoves[kTms[tm].move].name));
  g_notices.push(q.c_str(), "Ahora sale en sus cartas de nivel", -1);
  save_now();
}

void App::bottom_ficha(int dex) {
  const Mon* mon = mon_by_dex(dex);
  char b[160];
  if (ficha_tab_ == FT_MOVES) bottom_dex_moves(dex);
  else {
    std::snprintf(b, sizeof(b), "%s · %s", mon->name, tr(kTabName[ficha_tab_]));
    header(b);
  }
  // Pestañas.
  for (int i = 0; i < FT_COUNT; ++i) {
    const float x = 4 + i * 78;
    const bool on = ficha_tab_ == i;
    C2D_DrawRectSolid(x, 24, 0, 76, 18, on ? C_SEL : C2D_Color32(22, 30, 48, 255));
    if (on) C2D_DrawRectSolid(x, 40, 0, 76, 2, C_GOLD);
    r_->text_fit(kTabName[i], x + 38, 25, 72, 0.36f, on ? C_GOLD : C_MUTED, false, 1);
  }
  if (ficha_tab_ == FT_MOVES) return;   // (su pie lo pone bottom_dex_moves)

  if (ficha_tab_ == FT_NATURE) {
    const int cur = nature_of(*s_, dex);
    char l[48];
    nat_line(cur, l, sizeof(l));
    std::snprintf(b, sizeof(b), "%s: %s (%s)", tr("Ahora"), nature_name(cur), l);
    r_->text_fit(b, 8, 45, 236, 0.36f, C_TEXT);
    const int ic = r_->icons.find("mint");
    std::snprintf(b, sizeof(b), "×%d", int(s_->bag[BAG_MINT]));
    r_->text(b, 312, 45, 0.38f, s_->bag[BAG_MINT] ? C_GOLD : C_MUTED, 2);
    if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), 312 - r_->text_width(b, 0.38f) - 15, 46, 0);
    for (int i = 0; i < kNatureCount; ++i) {
      const float x = 6 + (i % 5) * 62, y = 62 + (i / 5) * 31;
      const bool sel = i == fi_cur_, mine = i == cur;
      C2D_DrawRectSolid(x, y, 0, 60, 29, sel ? C_SEL : C_PANEL);
      if (mine) { C2D_DrawRectSolid(x, y, 0, 60, 2, C_GOLD); C2D_DrawRectSolid(x, y + 27, 0, 60, 2, C_GOLD); }
      r_->text_fit(nature_name(i), x + 30, y + 2, 56, 0.34f, mine ? C_GOLD : sel ? C_TEXT : C2D_Color32(210, 220, 236, 255), false, 1);
      const NatureDef& n = kNatures[i];
      if (n.up < 0) r_->text_fit(tr("neutra"), x + 30, y + 16, 56, 0.28f, C_MUTED, false, 1);
      else {
        char u[16], d[16];
        std::snprintf(u, sizeof(u), "+%s", tr(kStatShort[n.up]));
        std::snprintf(d, sizeof(d), "−%s", tr(kStatShort[n.down]));
        r_->text(u, x + 29, y + 16, 0.28f, C_UP, 2, false);
        r_->text(d, x + 31, y + 16, 0.28f, C_DOWN, 0, false);
      }
    }
    r_->text(s_->bag[BAG_MINT] ? "A: usar una Menta · L/R: pestaña · B: volver" : "Sin Mentas: cómpralas en la Tienda Kecleon · B: volver", 160, 226, 0.32f, C_MUTED, 1);
    return;
  }

  if (ficha_tab_ == FT_ABILITY) {
    const int16_t* l = abil_slots(dex);
    const AbilPick cur = abil_of(*s_, dex);
    int ic = r_->icons.find("capsule");
    std::snprintf(b, sizeof(b), "×%d", int(s_->bag[BAG_CAP]));
    if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), 8, 46, 0);
    r_->text(b, 22, 45, 0.36f, s_->bag[BAG_CAP] ? C_GOLD : C_MUTED);
    ic = r_->icons.find("capsuleh");
    if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), 60, 46, 0);
    std::snprintf(b, sizeof(b), "×%d", int(s_->bag[BAG_CAPH]));
    r_->text(b, 74, 45, 0.36f, s_->bag[BAG_CAPH] ? C_GOLD : C_MUTED);
    r_->text_fit("La oculta sale 1 de cada 20", 312, 45, 200, 0.32f, C_MUTED, false, 2);
    int ns = 0;
    for (int i = 0; i < 3; ++i) {
      if (l[i] < 0 || (i == 1 && l[1] == l[0])) continue;
      const float y = 62 + ns * 52;
      const bool sel = ns == fi_cur_, mine = i == cur.slot;
      C2D_DrawRectSolid(6, y, 0, 308, 50, sel ? C_SEL : C_PANEL);
      if (sel) C2D_DrawRectSolid(6, y, 0, 3, 50, C_GOLD);
      r_->text_fit(abil_name(l[i]), 14, y + 2, 180, 0.42f, i == 2 ? C_GOLD : C_TEXT);
      if (mine) r_->text("La tiene", 306, y + 4, 0.34f, C_UP, 2);
      else if (i == 2) r_->text("oculta", 306, y + 4, 0.34f, C_GOLD, 2);
      char d1[200], d2[120];
      abil_desc(l[i], mon, d1, sizeof(d1), d2, sizeof(d2));
      std::string d = tr(d1);
      if (d2[0]) { d += " "; d += tr(d2); }
      r_->text_wrap(d.c_str(), 14, y + 20, 0.3f, C_MUTED, 292);
      ns++;
    }
    if (!ns) r_->text("Este Pokémon no tiene habilidades.", 160, 110, 0.38f, C_MUTED, 1);
    r_->text("A: usar una Cápsula · L/R: pestaña · B: volver", 160, 226, 0.32f, C_MUTED, 1);
    return;
  }

  // MT
  int tms[kMaxTms];
  const int n = ficha_tms(*s_, dex, tms);
  int taught = 0;
  for (int i = 0; i < n; ++i) if (tm_taught(*s_, dex, tms[i])) taught++;
  std::snprintf(b, sizeof(b), "%s: %d / %d", tr("Aprendidas"), taught, n);
  r_->text(b, 8, 45, 0.36f, C_TEXT);
  if (!n) r_->text_wrap("En los juegos no aprende ninguna de estas MT con ataques que no sean ya de sus tipos.", 160, 90, 0.38f, C_MUTED, 280, 1);
  char today[9];
  shop_day(hw3ds::utc_ms(), today);
  int sold[kShopDaily];
  const int nsold = shop_daily(*s_, today, sold);
  for (int i = 0; i < kTmRows && fi_scroll_ + i < n; ++i) {
    const int k = fi_scroll_ + i, tm = tms[k];
    const float y = kTmRowY + i * kTmRowH;
    const bool sel = k == fi_cur_;
    const MoveDef& d = kMoves[kTms[tm].move];
    C2D_DrawRectSolid(6, y, 0, 304, kTmRowH - 2, sel ? C_SEL : C_PANEL);
    C2D_DrawRectSolid(6, y, 0, 3, kTmRowH - 2, sel ? C_GOLD : rgb(kTypeColor[d.type]));
    const int ic = r_->move_icon[kTms[tm].move];
    if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), 12, y + 3, 0, nullptr, 1.125f, 1.125f);
    r_->text_fit(d.name, 34, y + 3, 150, 0.38f, sel ? C_GOLD : C_TEXT);
    if (tm_taught(*s_, dex, tm)) r_->text("Aprendida", 304, y + 4, 0.34f, C_UP, 2);
    else if (s_->tms[tm] > 0) { std::snprintf(b, sizeof(b), "%s ×%d", tr("Enseñar MT"), int(s_->tms[tm])); r_->text(b, 304, y + 4, 0.34f, C_GOLD, 2); }
    else {
      bool today_sale = false;
      for (int j = 0; j < nsold; ++j) if (sold[j] == tm) today_sale = true;
      r_->text(today_sale ? "Hoy en la tienda" : "No la tienes", 304, y + 4, 0.32f, today_sale ? C2D_Color32(0x9a, 0xe6, 0xff, 255) : C_MUTED, 2);
    }
  }
  if (n > kTmRows) {
    const float hh = kTmRows * kTmRowH;
    C2D_DrawRectSolid(313, kTmRowY, 0, 3, hh, C2D_Color32(0, 0, 0, 120));
    C2D_DrawRectSolid(313, kTmRowY + hh * fi_scroll_ / n, 0, 3, hh * kTmRows / n, C2D_Color32(255, 203, 61, 160));
  }
  r_->text("A: enseñar la MT · L/R: pestaña · B: volver", 160, 226, 0.32f, C_MUTED, 1);
}

void App::top_ficha(int dex) {
  if (ficha_tab_ == FT_MOVES) { top_dex_move(dex); return; }
  const Mon* base = mon_by_dex(dex);
  if (!base) return;
  char b[200];
  C2D_DrawRectSolid(0, 0, 0, TW, TH, C2D_Color32(8, 12, 20, 170));
  // El Pokémon (con su disfraz, si lo lleva), a la izquierda.
  const int sk = skin_worn(*s_, dex);
  const bool sh = dex == s_->partner && s_->partner_shiny && s_->owns_shiny(dex);
  C2D_DrawEllipseSolid(20, 176, 0, 130, 30, rgbf(kTypeColor[base->types[0]], 0.3f));
  draw_mon(sk >= 0 ? kSkins[sk].sprite : dex, sh, 85, 196, 2.6f);
  r_->text(base->name, 85, 12, 0.6f, C_TEXT, 1);

  if (ficha_tab_ == FT_NATURE) {
    const int cur = nature_of(*s_, dex), pick = clamp(fi_cur_, 0, kNatureCount - 1);
    Mon now, then;
    mon_with_nature(*base, cur, now);
    mon_with_nature(*base, pick, then);
    const float x = 186;
    if (pick == cur) std::snprintf(b, sizeof(b), "%s %s", tr("Naturaleza"), nature_name(cur));
    else std::snprintf(b, sizeof(b), "%s → %s", nature_name(cur), nature_name(pick));
    r_->text_fit(b, x, 14, 206, 0.5f, C_GOLD, true);
    // Barras: lo de ahora y, si eliges otra, cómo quedaría.
    const float vals[NS_COUNT][2] = {{now.pa, then.pa}, {now.sa, then.sa}, {now.df, then.df}, {now.sd, then.sd}, {float(now.spd), float(then.spd)}};
    const float maxv[NS_COUNT] = {1.7f, 1.7f, 230, 230, 160};
    const NatureDef& pn = kNatures[pick];
    for (int st = 0; st < NS_COUNT; ++st) {
      const float y = 44 + st * 26;
      const u32 col = pn.up == st ? C_UP : pn.down == st ? C_DOWN : C_TEXT;
      r_->text(kStatLong[st], x, y, 0.36f, col);
      C2D_DrawRectSolid(x + 76, y + 4, 0, 90, 8, C2D_Color32(0, 0, 0, 150));
      C2D_DrawRectSolid(x + 76, y + 4, 0, 90 * std::min(1.0f, vals[st][1] / maxv[st]), 8, pn.up == st ? C_UP : pn.down == st ? C_DOWN : C_GOLD);
      if (st < 2) std::snprintf(b, sizeof(b), "×%.2f", double(vals[st][1]));
      else std::snprintf(b, sizeof(b), "%d", int(std::lround(vals[st][1])));
      r_->text(b, x + 208, y, 0.36f, col, 2);
    }
    char l[48];
    nat_line(pick, l, sizeof(l));
    r_->text_fit(l, x + 104, 178, 200, 0.4f, C_MUTED, false, 1);
    std::snprintf(b, sizeof(b), "%s", stats_side_of(&then) == CAT_SPECIAL ? tr("Pega mejor con ataques especiales") : tr("Pega mejor con ataques físicos"));
    r_->text_fit(b, x + 104, 196, 206, 0.36f, C_TEXT, false, 1);
    if (pick != cur) r_->text_fit(s_->bag[BAG_MINT] ? "A: cambiarla (gasta una Menta)" : "Hace falta una Menta (Tienda Kecleon)", x + 104, 216, 206, 0.36f, s_->bag[BAG_MINT] ? C_GOLD : C_DOWN, false, 1);
    return;
  }

  if (ficha_tab_ == FT_ABILITY) {
    const int16_t* l = abil_slots(dex);
    int slots[3], ns = 0;
    for (int i = 0; i < 3; ++i) if (l[i] >= 0 && !(i == 1 && l[1] == l[0])) slots[ns++] = i;
    if (!ns) return;
    const int slot = slots[clamp(fi_cur_, 0, ns - 1)];
    const AbilPick cur = abil_of(*s_, dex);
    const float x = 186;
    r_->text(tr("Habilidad"), x, 14, 0.38f, C_MUTED);
    r_->text_fit(abil_name(l[slot]), x, 30, 206, 0.58f, slot == 2 ? C_GOLD : C_TEXT, true);
    float y = 58;
    if (slot == 2) { r_->text("Habilidad oculta", x, y, 0.36f, C_GOLD); y += 16; }
    char d1[200], d2[120];
    abil_desc(l[slot], base, d1, sizeof(d1), d2, sizeof(d2));
    y += r_->text_wrap(d1, x, y, 0.4f, C_TEXT, 206) + 6;
    if (d2[0]) y += r_->text_wrap(d2, x, y, 0.36f, C_MUTED, 206) + 6;
    if (slot == cur.slot) r_->text("Es la que tiene", x, 214, 0.4f, C_UP);
    else r_->text_fit(slot == 2 ? (s_->bag[BAG_CAPH] ? "A: cambiarla (Cápsula Habilidad Oculta)" : "Hace falta una Cápsula Habilidad Oculta")
                                : (s_->bag[BAG_CAP] || s_->bag[BAG_CAPH] ? "A: cambiarla (gasta una Cápsula)" : "Hace falta una Cápsula Habilidad"),
                      x, 214, 206, 0.36f, s_->bag[slot == 2 ? BAG_CAPH : BAG_CAP] || (slot < 2 && s_->bag[BAG_CAPH]) ? C_GOLD : C_DOWN);
    return;
  }

  // MT: el ataque elegido.
  int tms[kMaxTms];
  const int n = ficha_tms(*s_, dex, tms);
  if (!n) return;
  const int tm = tms[clamp(fi_cur_, 0, n - 1)];
  const int mv = kTms[tm].move;
  const MoveDef& d = kMoves[mv];
  const float x = 186;
  const int ic = r_->move_icon[mv];
  if (ic >= 0) C2D_DrawImageAt(r_->icons.image(ic), x, 12, 0, nullptr, 2.5f, 2.5f);
  std::snprintf(b, sizeof(b), "%s %s", tr("MT"), tr(d.name));
  r_->text_wrap(b, x + 46, 16, 0.46f, C_TEXT, 160);
  C2D_DrawRectSolid(x, 56, 0, 64, 16, rgb(kTypeColor[d.type]));
  r_->text(kTypeName[d.type], x + 32, 57, 0.34f, C_TEXT, 1);
  const bool buff = d.kind == K_BUFF || d.cat < 0;
  C2D_DrawRectSolid(x + 70, 56, 0, 70, 16, buff ? C2D_Color32(30, 40, 62, 255) : d.cat == CAT_PHYSICAL ? C2D_Color32(120, 52, 30, 255) : C2D_Color32(40, 60, 130, 255));
  r_->text(buff ? "Mejora" : d.cat == CAT_PHYSICAL ? "Físico" : "Especial", x + 105, 57, 0.34f, buff ? C_MUTED : C_GOLD, 1);
  float y = 80 + r_->text_wrap(d.desc, x, 80, 0.38f, C_TEXT, 206) + 18;
  if (!buff) {
    const int pct = int(std::lround(stats_fit(base, d.cat) * 100));
    if (pct >= 100) std::snprintf(b, sizeof(b), "%s", tr("Es su lado fuerte: rinde al 100 %"));
    else { std::snprintf(b, sizeof(b), "%s", tr("No es su lado fuerte: rinde al {x} %")); std::string q = b; const size_t p = q.find("{x}"); if (p != std::string::npos) q.replace(p, 3, std::to_string(pct)); std::snprintf(b, sizeof(b), "%s", q.c_str()); }
    y += r_->text_wrap(b, x, y, 0.34f, pct >= 100 ? C_GOLD : C2D_Color32(255, 150, 120, 255), 206) + 6;
  }
  if (tm_taught(*s_, dex, tm)) r_->text("La sabe: sale en sus cartas de nivel", x, 214, 0.36f, C_UP);
  else if (s_->tms[tm] > 0) r_->text("A: enseñársela (gasta la MT)", x, 214, 0.36f, C_GOLD);
  else r_->text_fit("Cómprala en la Tienda Kecleon cuando esté", x, 214, 206, 0.34f, C_MUTED);
}

}  // namespace ps

namespace ps {

// Autotest «ficha»: su compañero (Pikachu si no tiene) con una Menta, una Cápsula, una
// Cápsula Oculta y dos MT que aprende; la ficha pestaña a pestaña, usándolo todo, y la
// Colección con su disfraz puesto y quitado (X).
void App::start_ficha_demo() {
  ficha_demo_ = true;
  news_t_ = 0;
  news_checked_ = cloud_asked_ = true;
  if (!s_->partner || !s_->owns(s_->partner)) { if (!s_->owns(25)) s_->owned_at[25] = now_ms(); s_->partner = 25; }
  const int dex = s_->partner;
  s_->bag[BAG_MINT] += 1; s_->bag[BAG_CAP] += 1; s_->bag[BAG_CAPH] += 1;
  s_->bag_known |= 7;
  int l[kMaxTms];
  const int n = tm_learnable(*s_, dex, l);
  for (int i = 0; i < n && i < 2; ++i) { s_->tms[l[i]] += 1; s_->tms_known.set(l[i]); }
  for (int k = 0; k < kSkinCount; ++k) if (kSkins[k].sell && kSkins[k].dex == dex && !s_->skin_at[k]) s_->skin_at[k] = now_ms();
  if (s_->mastery[dex] < 31250) s_->mastery[dex] = 31250;   // (la maestría, a medias: foto y título)
  open_ficha(dex, FT_MOVES);
}

void App::ficha_demo_step(float dt) {
  const float t0 = news_t_;
  news_t_ += dt;
  auto at = [&](float s) { return t0 < s && news_t_ >= s; };
  const int dex = s_->partner;
  auto press = [&](u32 down, u32 rep = 0) { UiIn in; in.down = down; in.rep = rep; in_dex(in); };
  if (at(4)) press(KEY_R);                                  // Naturaleza
  if (at(6)) { press(0, KEY_DRIGHT); press(0, KEY_DDOWN); }  // otra
  if (at(9)) press(KEY_A);                                  // (la Menta)
  if (at(12)) press(KEY_R);                                 // Habilidad
  if (at(14)) press(0, KEY_DDOWN);
  if (at(17)) press(KEY_A);                                 // (la Cápsula)
  if (at(20)) press(KEY_R);                                 // MT
  if (at(23)) {                                             // la primera que tiene, enseñada
    int tms[kMaxTms];
    const int n = ficha_tms(*s_, dex, tms);
    for (int i = 0; i < n; ++i) if (s_->tms[tms[i]] > 0 && !tm_taught(*s_, dex, tms[i])) { fi_cur_ = i; break; }
    press(KEY_A);
  }
  if (at(26)) press(KEY_R);                                 // Ataques (con la MT nueva)
  if (at(29)) press(KEY_B);                                 // la Colección: naturaleza y habilidad arriba
  if (at(32)) press(KEY_X);                                 // el disfraz (X)
  if (at(35)) press(KEY_X);
  if (at(38)) go(SC_MENU);
  if (news_t_ > 41) quit = true;
}

}  // namespace ps

// render.cpp — la partida en las dos pantallas.
//   Arriba (400×240): el mundo a escala 0,5 (1 píxel de arte = 1 píxel), como
//   drawScene/render de game.js, más los avisos y la barra del jefe.
//   Abajo (320×240, táctil): vida, nivel, reloj, movimientos y las elecciones
//   (cartas de nivel, objeto, vínculo), pausa y resumen.
#include "platform/3ds/render.h"
#include "core/stats.h"

#include <string>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/i18n.h"
#include "core/mega.h"
#include "core/orbs.h"
#include "core/util.h"

namespace ps {

// Colores de cada jugador en el cooperativo (coop.js COLORS).
static const uint32_t kCoopColor[4] = {0xffcb3d, 0x5fc8ff, 0x7ee07a, 0xff8ad8};

namespace {
constexpr float TOP_W = 400, TOP_H = 240, BOT_W = 320, BOT_H = 240;
const u32 BLACK = C2D_Color32(0, 0, 0, 255);

// 3D: desplazamiento horizontal de la capa que se está dibujando (cada ojo
// ve las capas desplazadas al revés; 0 = en el plano de la pantalla).
float g_par = 0;

struct View {
  float l, t;
  float sx(float wx) const { return std::floor((wx - l) * 0.5f + 0.5f) + g_par; }
  float sy(float wy) const { return std::floor((wy - t) * 0.5f + 0.5f); }
};

void ellipse(float cx, float cy, float rx, float ry, u32 c) {
  if (rx <= 0 || ry <= 0) return;
  C2D_DrawEllipseSolid(cx - rx, cy - ry, 0, rx * 2, ry * 2, c);
}

// Anillo de puntos (los aros de fx.js), con tope de puntos para la consola.
void dot_ring(float cx, float cy, float rx, float ry, float size, u32 c, float a0 = 0, float a1 = kTau) {
  const float len = std::fabs(a1 - a0) * std::max(rx, ry);
  int n = int(len / 2.5f);
  n = clamp(n, 8, 140);
  for (int i = 0; i <= n; ++i) {
    const float a = a0 + (a1 - a0) * i / n;
    C2D_DrawRectSolid(std::floor(cx + std::cos(a) * rx), std::floor(cy + std::sin(a) * ry), 0, size, size, c);
  }
}

// Rectángulo girado: a lo largo de `angle` desde (x, y), de u0 a u1, ancho v0..v1.
void quad(float x, float y, float angle, float u0, float u1, float v0, float v1, u32 c) {
  const float ca = std::cos(angle), sa = std::sin(angle);
  auto P = [&](float u, float v, float& ox, float& oy) { ox = x + ca * u - sa * v; oy = y + sa * u + ca * v; };
  float ax, ay, bx, by, cx, cy, dx, dy;
  P(u0, v0, ax, ay); P(u1, v0, bx, by); P(u1, v1, cx, cy); P(u0, v1, dx, dy);
  C2D_DrawTriangle(ax, ay, c, bx, by, c, cx, cy, c, 0);
  C2D_DrawTriangle(ax, ay, c, cx, cy, c, dx, dy, c, 0);
}

float step_alpha(float k) { return k > 0.66f ? 1 : k > 0.33f ? 0.66f : 0.33f; }

void bar(float x, float y, float w, float h, float frac, u32 col, u32 bg = C2D_Color32(0, 0, 0, 140)) {
  C2D_DrawRectSolid(x, y, 0, w, h, bg);
  if (frac > 0) C2D_DrawRectSolid(x, y, 0, w * clamp(frac, 0.0f, 1.0f), h, col);
}

void triangle_arrow(float x, float y, float a, float len, float half, u32 c) {
  C2D_DrawTriangle(x + std::cos(a) * len, y + std::sin(a) * len, c,
                   x + std::cos(a + 2.45f) * half, y + std::sin(a + 2.45f) * half, c,
                   x + std::cos(a - 2.45f) * half, y + std::sin(a - 2.45f) * half, c, 0);
}
}  // namespace

void Renderer::set_parallax(float p) { g_par = p; }

// ---------------- carga ----------------

bool Renderer::init() {
  tbuf = C2D_TextBufNew(8192);
  // Fuente propia (Pixelify Sans, tools/gen_font.py); si falta, la del sistema.
  font = C2D_FontLoad("romfs:/gfx/font.bcfnt");
  if (font) C2D_FontSetFilter(font, GPU_NEAREST, GPU_NEAREST);
  font_small = font ? C2D_FontLoad("romfs:/gfx/font_small.bcfnt") : nullptr;
  if (font_small) C2D_FontSetFilter(font_small, GPU_NEAREST, GPU_NEAREST);
  font_k = font ? 1.2f : 1.0f;
  if (!props.load("romfs:/gfx/props.t3x", "romfs:/gfx/props.idx")) return false;
  if (!vfx.load("romfs:/gfx/vfx.t3x", "romfs:/gfx/vfx.idx")) return false;
  if (!icons.load("romfs:/gfx/icons.t3x", "romfs:/gfx/icons.idx")) return false;
  chunks.init();
  sprites.init();
  minimap.init();
  static const char* const PT[PR_COUNT] = {"tree", "rock", "chest", "grass", "altar", "spring", "bigchest", "trap", "pumpkin"};
  char name[48];
  for (int b = 0; b < kBiomeCount && b < 32; ++b)
    for (int t = 0; t < PR_COUNT; ++t)
      for (int v = 0; v < 8; ++v) { std::snprintf(name, sizeof(name), "%d/%s/%d", b, PT[t], v); prop_idx[b][t][v] = props.find(name); }
  for (int v = 0; v < 4; ++v) { std::snprintf(name, sizeof(name), "rift/%d", v); rift_idx[v] = props.find(name); }
  for (int s = 0; s < SH_COUNT; ++s)
    for (int p = 0; p < PAL_COUNT; ++p)
      for (int f = 0; f < 3; ++f) {
        std::snprintf(name, sizeof(name), "%s|%s|%d", kShapeId[s], p < T_COUNT ? kTypeId[p] : "aura", f);
        vfx_idx[s][p][f] = vfx.find(name);
      }
  for (int t = 0; t < T_COUNT; ++t) { std::snprintf(name, sizeof(name), "type:%s", kTypeId[t]); type_icon[t] = icons.find(name); }
  for (int m = 0; m < kMoveCount; ++m) { std::snprintf(name, sizeof(name), "move:%s", kMoves[m].id); move_icon[m] = icons.find(name); }
  for (int k = 0; k < kMegaMax; ++k) {
    std::snprintf(name, sizeof(name), "stone:%d", k);
    stone_icon[k] = int16_t(k < kMegaCount ? icons.find(name) : -1);
  }
  static const char* const UI[20] = {"heart", "coin", "ticket", "ticket10", "berry", "magnet", "bomb", "shiny", "shield", "friends", "clock", "star", "bond", "cards", "up", "book",
                                     "candy", "pumpkin", "witch", "shop"};
  for (int i = 0; i < 20; ++i) ui_icon[i] = icons.find(UI[i]);
  return true;
}

// Precarga los Pokémon de la partida (el tuyo, el elenco y los jefes).
void Renderer::preload_run(Game& g) {
  for (int i = 0; i < g.nplayers; ++i) if (g.players[i].active) sprites.get(g.players[i].dex, g.players[i].shiny);
  // Disfraces de Halloween (y el del gran Gengar, si la partida es de Halloween).
  for (int i = 0; i < g.nplayers; ++i) {
    const Player& p = g.players[i];
    if (p.active && p.skin >= 0 && p.skin < kSkinCount) sprites.get(kSkins[p.skin].sprite, p.shiny && has_shiny(kSkins[p.skin].sprite));
  }
  // Sus megas (las dos formas, y en shiny si es shiny y la hay): cargadas ya,
  // para que al megaevolucionar no desaparezca un momento mientras carga.
  for (int i = 0; i < g.nplayers; ++i) {
    const Player& p = g.players[i];
    if (!p.active) continue;
    for (int s = 0; s < 2; ++s) {
      const int k = mega_index(p.dex, s);
      if (k >= 0 && kMegas[k].own_sprite) sprites.get(kMegas[k].sprite, p.shiny && has_shiny(kMegas[k].sprite));
    }
  }
  for (int t = 0; t < g.ncast; ++t)
    for (int i = 0; i < g.cast[t].n; ++i) sprites.get(g.cast[t].list[i].dex, false);
  for (int b = 0; b < g.nbosses; ++b) sprites.get(g.bosses[b].dex, false);
  if (world_seed != uint32_t(g.world.seed())) { chunks.clear(); world_seed = uint32_t(g.world.seed()); }
}

void Renderer::begin_frame(Game& g) {
  sprites.poll();
  if (world_seed != uint32_t(g.world.seed()) || terrain_gen != g.world.terrain_gen()) {
    chunks.clear(); world_seed = uint32_t(g.world.seed()); terrain_gen = g.world.terrain_gen();
  }
  // Pinta 3 filas de casillas por fotograma (un chunk en ~6 fotogramas); más si
  // lo que se ve está sin pintar.
  const int base = old_3ds ? 1 : 3;
  chunks.work(g.world, chunks.pending() > 2 ? base * 2 : base);
  if (g.state != ST_UI && g.state != ST_OVER) minimap.work(g, old_3ds ? 4 : 8);
}

// ---------------- utilidades ----------------

Renderer::FontPick Renderer::pick_font(float size) const {
  if (!font) return {nullptr, size, 0};   // fuente del sistema: tal cual
  // citro2d escala todas las fuentes para que 1.0 sea una celda de 30 px (la
  // del sistema): el tamaño real de una fuente es celda/30.
  auto cell = [](C2D_Font f) { return float(C2D_FontGetInfo(f)->tglp->cellHeight); };
  const float want = size * font_k * 30;   // alto de celda que se pedía
  if (font_small) {
    const float c = cell(font_small);
    // (hasta 0,36: Tiny5; desde 0,38, la grande, que se lee mucho mejor)
    if (want < 13.2f) return {font_small, c / 30, std::round((want - c) / 2)};
  }
  const float c = cell(font);
  const float n = want < c * 1.7f ? 1 : want < c * 2.6f ? 2 : 3;
  return {font, n * c / 30, std::round((want - n * c) / 2)};
}

void Renderer::text(const char* s, float x, float y, float size, u32 color, int align, bool shadow) {
  if (i18n_lang() != LANG_ES) s = tr(s);
  const FontPick fp = pick_font(size);
  C2D_Text t;
  C2D_TextFontParse(&t, fp.f, tbuf, s);
  C2D_TextOptimize(&t);
  float w = 0, h = 0;
  C2D_TextGetDimensions(&t, fp.k, fp.k, &w, &h);
  if (align == 1) x -= w / 2;
  else if (align == 2) x -= w;
  // En píxeles enteros + ¼: con muestreo "nearest", si la letra cae justo en
  // medio píxel se pierden columnas; con ¼ nunca cae en la frontera (ni aunque
  // citro2d centre luego cada línea y sume medio píxel).
  x = std::floor(x + 0.5f) + 0.25f;
  y = std::floor(y + fp.dy + 0.5f) + 0.25f;
  if (shadow) {
    C2D_DrawText(&t, C2D_WithColor, x + 1, y + 1, 0, fp.k, fp.k, C2D_Color32(0, 0, 0, (color >> 24) * 3 / 4));
    C2D_DrawText(&t, C2D_WithColor, x - 1, y, 0, fp.k, fp.k, C2D_Color32(0, 0, 0, (color >> 24) / 2));
  }
  C2D_DrawText(&t, C2D_WithColor, x, y, 0, fp.k, fp.k, color);
}

// Texto con salto de línea automático (ancho `width`); align 1 = centrado en x.
float Renderer::text_wrap(const char* s, float x, float y, float size, u32 color, float width, int align) {
  if (i18n_lang() != LANG_ES) s = tr(s);
  const FontPick fp = pick_font(size);
  C2D_Text t;
  C2D_TextFontParse(&t, fp.f, tbuf, s);
  C2D_TextOptimize(&t);
  float w = 0, h = 0;
  C2D_TextGetDimensions(&t, fp.k, fp.k, &w, &h);
  x = std::floor(x + 0.5f) + 0.25f;   // (ver text())
  y = std::floor(y + fp.dy + 0.5f) + 0.25f;
  const u32 flags = C2D_WithColor | C2D_WordWrap | (align == 1 ? C2D_AlignCenter : 0);
  C2D_DrawText(&t, flags, x + 1, y + 1, 0, fp.k, fp.k, C2D_Color32(0, 0, 0, (color >> 24) * 3 / 4), width);
  C2D_DrawText(&t, flags, x, y, 0, fp.k, fp.k, color, width);
  // Alto aproximado: líneas según el ancho.
  const int lines = int(w / width) + 1;
  return h * lines;
}

float Renderer::text_fit(const char* s, float x, float y, float maxw, float size, u32 color, bool shadow, int align) {
  std::string t = i18n_lang() != LANG_ES ? tr(s) : s;
  float sz = size;
  for (float k : {0.32f, 0.28f}) { if (text_width(t.c_str(), sz) <= maxw || sz <= k) break; sz = k; }
  if (text_width(t.c_str(), sz) > maxw) {
    while (!t.empty() && text_width((t + "…").c_str(), sz) > maxw) {
      t.pop_back();
      while (!t.empty() && (uint8_t(t.back()) & 0xC0) == 0x80) t.pop_back();   // (letras enteras de UTF-8)
    }
    t += "…";
  }
  text(t.c_str(), x, y + (size - sz) * 14, sz, color, align, shadow);
  return text_width(t.c_str(), sz);
}

int Renderer::text_fit2(const char* s, float x, float y, float maxw, float size, u32 color) {
  std::string t = i18n_lang() != LANG_ES ? tr(s) : s;
  if (text_width(t.c_str(), size) <= maxw) { text(t.c_str(), x, y + 4, size, color, 0, false); return 1; }
  // La primera línea: hasta el último espacio que quepa.
  const float sz = 0.28f;
  size_t cut = 0;
  for (size_t i = 0; i < t.size(); ++i) {
    if (t[i] != ' ') continue;
    if (text_width(t.substr(0, i).c_str(), sz) > maxw) break;
    cut = i;
  }
  if (cut == 0) { text_fit(t.c_str(), x, y + 4, maxw, size, color); return 1; }
  text(t.substr(0, cut).c_str(), x, y, sz, color, 0, false);
  text_fit(t.substr(cut + 1).c_str(), x, y + 10, maxw, sz, color);
  return 2;
}

void Renderer::draw_mega_stone(int k, float x, float y, float s) {
  const int ic = k >= 0 && k < kMegaCount ? stone_icon[k] : -1;
  if (ic >= 0) {
    const float sc = s / icons.sub(ic).width;
    C2D_DrawImageAt(icons.image(ic), std::floor(x), std::floor(y), 0, nullptr, sc, sc);
    return;
  }
  const float r = s / 2, cx = x + r, cy = y + r;
  C2D_DrawCircleSolid(cx, cy, 0, r, C2D_Color32(40, 20, 64, 255));
  C2D_DrawCircleSolid(cx, cy, 0, r - std::fmax(1.0f, s * 0.08f), C2D_Color32(150, 100, 255, 255));
  C2D_DrawCircleSolid(cx + r * 0.12f, cy + r * 0.12f, 0, r * 0.62f, C2D_Color32(110, 60, 220, 255));
  // El símbolo mega: un anillo de colores.
  static const uint32_t C[6] = {0xff5f6d, 0xffd23f, 0x5fe08a, 0x5fc8ff, 0xc47bff, 0xff8ad8};
  const float rr = r * 0.48f, d = std::fmax(1.0f, s * 0.11f);
  for (int k = 0; k < 12; ++k) {
    const float a = k * kTau / 12;
    C2D_DrawRectSolid(cx + std::cos(a) * rr - d / 2, cy + std::sin(a) * rr - d / 2, 0, d, d, rgb(C[k % 6]));
  }
  C2D_DrawCircleSolid(cx - r * 0.38f, cy - r * 0.38f, 0, std::fmax(1.0f, r * 0.2f), C2D_Color32(255, 255, 255, 200));
}

void Renderer::draw_item_icon(int id, float x, float y, float s, int dex) {
  if (is_mega_stone(id)) { draw_mega_stone(dex >= 0 ? mega_index(dex, id - IT_MEGA_A) : -1, x, y, s); return; }
  const char* name = item_icon(id);
  const int ic = name ? icons.find(name) : -1;
  if (ic < 0) return;
  const float k = s / icons.sub(ic).width;
  C2D_DrawImageAt(icons.image(ic), std::floor(x), std::floor(y), 0, nullptr, k, k);
}

float Renderer::text_width(const char* s, float size) {
  if (i18n_lang() != LANG_ES) s = tr(s);
  const FontPick fp = pick_font(size);
  C2D_Text t;
  C2D_TextFontParse(&t, fp.f, tbuf, s);
  float w = 0, h = 0;
  C2D_TextGetDimensions(&t, fp.k, fp.k, &w, &h);
  return w;
}

// flash_mode: 0 nada, 1 blanco entero (enemigos), 2 medio blanco (tu Pokémon)
void Renderer::draw_sprite(const Animator& a, float sx, float sy, u32 flash_mode, float alpha) {
  if (alpha <= 0) return;
  const AnimMeta* m = anim_meta(a.dex, a.cur, a.shiny);
  SpriteSet* s = sprites.get(a.dex, a.shiny);
  C2D_Image img;
  int ox, oy, w, h;
  const int row = m ? std::min<int>(a.dir, m->r - 1) : 0;
  if (!m || !s || !sprites.frame(s, a.cur, row, std::min<int>(a.frame, m->n - 1), img, ox, oy, w, h)) {
    if (!s && sprites.loading(a.dex, a.shiny)) return;   // llega en unos fotogramas
    // Respaldo: bolita.
    ellipse(sx, sy - 12 * a.scale, 8 * a.scale, 8 * a.scale, C2D_Color32(0x6b, 0x7a, 0x94, 255));
    return;
  }
  const float k = a.scale;
  const float x = std::floor(sx - m->ax * k + ox * k + 0.5f), y = std::floor(sy - m->ay * k + oy * k + 0.5f);
  C2D_ImageTint tint;
  if (flash_mode == 1) C2D_PlainImageTint(&tint, C2D_Color32f(1, 1, 1, alpha), 1.0f);
  else if (flash_mode == 2) C2D_PlainImageTint(&tint, C2D_Color32f(1, 1, 1, alpha), 0.55f);
  else C2D_PlainImageTint(&tint, C2D_Color32f(1, 1, 1, alpha), 0.0f);
  C2D_DrawImageAt(img, x, y, 0, &tint, k, k);
}

void Renderer::draw_shape(int shape, int pal, int frame, float sx, float sy, float angle, float scale, float alpha) {
  if (shape < 0 || shape >= SH_COUNT || alpha <= 0) return;
  if (pal < 0 || pal >= PAL_COUNT) pal = T_NORMAL;
  const int frames = kShapeInfo[shape].frames;
  const int idx = vfx_idx[shape][pal][((frame % frames) + frames) % frames];
  if (idx < 0) return;
  // 16 direcciones: menos parpadeo.
  if (angle != 0) angle = float(jsround(angle / (kPi / 8))) * (kPi / 8);
  C2D_ImageTint tint;
  C2D_PlainImageTint(&tint, C2D_Color32f(1, 1, 1, alpha), 0.0f);
  C2D_DrawImageAtRotated(vfx.image(idx), std::floor(sx + 0.5f), std::floor(sy + 0.5f), 0, angle, &tint, scale, scale);
}

// ---------------- esferas ----------------

void Renderer::draw_orb(int id, float cx, float cy, float r, float alpha) {
  if (id < 0 || id >= ORB_COUNT) return;
  const uint32_t c = kOrbs[id].col;
  const uint32_t d = ((((c >> 16) & 255) * 55 / 100) << 16) | ((((c >> 8) & 255) * 55 / 100) << 8) | ((c & 255) * 55 / 100);
  C2D_DrawCircleSolid(cx, cy, 0, r + 1, C2D_Color32(16, 12, 24, uint8_t(230 * alpha)));
  C2D_DrawCircleSolid(cx, cy, 0, r, rgbf(d, alpha));
  C2D_DrawCircleSolid(cx - r * 0.12f, cy - r * 0.12f, 0, r * 0.82f, rgbf(c, alpha));
  C2D_DrawCircleSolid(cx - r * 0.35f, cy - r * 0.38f, 0, r * 0.28f, C2D_Color32(255, 255, 255, uint8_t(220 * alpha)));
}

void Renderer::draw_medal(float cx, float cy, float s) {
  // Cinta (dos tiras) y el disco: borde oscuro, oro, brillo y la rueda de Arceus.
  const u32 ra = rgb(0x3d6bd6), rb = rgb(0xd6453d);
  C2D_DrawTriangle(cx - s * 0.55f, cy - s * 1.25f, ra, cx - s * 0.05f, cy - s * 1.25f, ra, cx - s * 0.1f, cy - s * 0.2f, ra, 0);
  C2D_DrawTriangle(cx + s * 0.55f, cy - s * 1.25f, rb, cx + s * 0.05f, cy - s * 1.25f, rb, cx + s * 0.1f, cy - s * 0.2f, rb, 0);
  C2D_DrawCircleSolid(cx, cy, 0, s + 1, C2D_Color32(60, 40, 0, 255));
  C2D_DrawCircleSolid(cx, cy, 0, s, rgb(0xd9a520));
  C2D_DrawCircleSolid(cx - s * 0.08f, cy - s * 0.08f, 0, s * 0.78f, rgb(0xffd25e));
  for (int k = 0; k < 8; ++k) {
    const float a = k * kTau / 8;
    C2D_DrawCircleSolid(cx + std::cos(a) * s * 0.5f, cy + std::sin(a) * s * 0.5f, 0, std::fmax(0.6f, s * 0.12f), rgb(0xb07a10));
  }
  C2D_DrawCircleSolid(cx - s * 0.3f, cy - s * 0.35f, 0, std::fmax(0.6f, s * 0.16f), C2D_Color32(255, 255, 255, 220));
}

// ---------------- fin de la partida: resumen de ataques (arriba) ----------------

void Renderer::draw_over_summary(Game& g, int pi) {
  if (pi < 0 || pi >= g.nplayers) pi = g.local;
  const Player& p = g.players[pi];
  char b[96];
  C2D_DrawRectSolid(0, 0, 0, TOP_W, TOP_H, C2D_Color32(8, 12, 24, 215));
  // Pestañas (en grupo): L ◀ Tú · BETO · … ▶ R
  float y = 6;
  if (g.nplayers > 1) {
    int n = 0;
    for (int i = 0; i < g.nplayers; ++i) if (g.players[i].active || i == pi) n++;
    const float w = std::fmin(110.0f, 330.0f / std::max(1, n));
    float x = 200 - w * n / 2;
    text("L", x - 12, y + 2, 0.4f, rgb(0x8fa3c4), 1);
    for (int i = 0; i < g.nplayers; ++i) {
      if (!g.players[i].active && i != pi) continue;
      const bool on = i == pi;
      C2D_DrawRectSolid(x + 2, y, 0, w - 4, 16, on ? rgb(kCoopColor[i % 4]) : C2D_Color32(30, 40, 62, 255));
      const char* nm = i == g.local ? "Tú" : g.cfg.members[i].name;
      text_fit(nm, x + w / 2, y + 2, w - 10, 0.36f, on ? C2D_Color32(16, 20, 34, 255) : rgb(0xeaf2ff), false, 1);
      x += w;
    }
    text("R", x + 12, y + 2, 0.4f, rgb(0x8fa3c4), 1);
    y += 22;
  }
  // Quién: Pokémon y nivel.
  std::snprintf(b, sizeof(b), "%s%s · Nv. %d", mon_by_dex(p.dex)->name, p.shiny ? " ★" : "", p.level);
  text(b, 200, y, 0.5f, rgb(0xffcb3d), 1);
  y += 20;
  text("Ataque", 14, y, 0.34f, rgb(0x8fa3c4), 0);
  text("Daño", 300, y, 0.34f, rgb(0x8fa3c4), 2);
  text("por segundo", 388, y, 0.34f, rgb(0x8fa3c4), 2);
  y += 14;
  double total = 0;
  const float now = float(g.time);
  for (int i = 0; i < p.nmoves && i < 8; ++i) {
    const MoveInst& m = p.moves[i];
    const float ry = y + i * 17;
    if (i % 2 == 0) C2D_DrawRectSolid(8, ry - 1, 0, 384, 16, C2D_Color32(255, 255, 255, 14));
    const int ic = move_icon[m.def];
    if (ic >= 0) C2D_DrawImageAt(icons.image(ic), 12, ry, 0, nullptr, 0.9f, 0.9f);
    std::snprintf(b, sizeof(b), "%s%s Nv.%d", m.name(), m.evolved ? " +" : "", m.lvl);
    text_fit(b, 30, ry + 1, 170, 0.38f, rgb(0xeaf2ff), true);
    const int cat = m.cat();
    text(cat == CAT_PHYSICAL ? "Físico" : cat == CAT_SPECIAL ? "Especial" : "Mejora", 222, ry + 2, 0.32f, rgb(cat < 0 ? 0x8fa3c4 : kTypeColor[m.type()]), 1);
    if (cat >= 0) {
      const float secs = std::fmax(5.0f, now - m.since);
      total += m.dealt;
      std::snprintf(b, sizeof(b), "%d", int(jsround(m.dealt)));
      text(b, 300, ry + 1, 0.38f, rgb(0xeaf2ff), 2);
      std::snprintf(b, sizeof(b), "%d/s", int(jsround(m.dealt / secs)));
      text(b, 388, ry + 1, 0.38f, rgb(0xffd23f), 2);
    } else {
      text("—", 300, ry + 1, 0.38f, rgb(0x8fa3c4), 2);
      text("—", 388, ry + 1, 0.38f, rgb(0x8fa3c4), 2);
    }
  }
  y += std::max(1, std::min(8, p.nmoves)) * 17 + 4;
  C2D_DrawRectSolid(8, y, 0, 384, 1, C2D_Color32(80, 95, 130, 255));
  y += 4;
  std::snprintf(b, sizeof(b), "%d", int(jsround(total)));
  text("Daño total", 30, y, 0.42f, rgb(0xffcb3d), 0);
  text(b, 300, y, 0.42f, rgb(0xffcb3d), 2);
  std::snprintf(b, sizeof(b), "%d/s", int(jsround(total / std::fmax(5.0f, now))));
  text(b, 388, y, 0.42f, rgb(0xffd23f), 2);
  y += 20;
  if (y > TOP_H - 22) y = TOP_H - 22;
  if (p.item >= 0) {
    draw_item_icon(p.item, 12, y, 16, p.dex);
    const char* nm = item_name(p.item, p.dex);
    text(nm, 32, y + 2, 0.38f, rgb(0xffd23f), 0);
    const float nw = text_width(nm, 0.38f);
    text_fit(item_desc(p.item, p.dex), 38 + nw, y + 3, 352 - nw, 0.32f, rgb(0xb7c4dc));
  } else {
    text("Sin objeto", 32, y + 2, 0.38f, rgb(0x8fa3c4), 0);
  }
}

// ---------------- pantalla de arriba ----------------

void Renderer::draw_ground(World& w, const Camera& cam, float vl, float vt, float now) {
  View v{vl, vt};
  const int biome = w.biome();
  const float fk = w.fade_k();
  // --- suelo ---
  const int32_t c0x = int32_t(std::floor(v.l / kCW)), c1x = int32_t(std::floor((v.l + cam.w) / kCW));
  const int32_t c0y = int32_t(std::floor(v.t / kCW)), c1y = int32_t(std::floor((v.t + cam.h) / kCW));
  // Precarga: pide antes los chunks del anillo de alrededor (se pintan en el
  // otro hilo) y luego los visibles, que así quedan los más recientes.
  if (chunks.threaded())
    for (int32_t cy = c0y - 1; cy <= c1y + 1; ++cy)
      for (int32_t cx = c0x - 1; cx <= c1x + 1; ++cx)
        if (cx < c0x || cx > c1x || cy < c0y || cy > c1y) chunks.get(w, cx, cy, biome);
  // Trozo aún sin pintar: suelo liso y, encima, sus paredes y su agua sacadas
  // del terreno (una pared que choca SIEMPRE se ve).
  auto rough = [&](int32_t cx, int32_t cy, float x, float y) {
    const Biome& B = kBiomes[biome];
    C2D_DrawRectSolid(x, y, 0, kChunkPx, kChunkPx, rgb(B.floor[0]));
    const float ts = kTS * 0.5f;   // casilla en pantalla
    for (int j = 0; j < kChunkTiles; ++j)
      for (int i = 0; i < kChunkTiles; ++i) {
        const uint8_t t = w.terrain_at(cx * kChunkTiles + i, cy * kChunkTiles + j);
        if (t == TR_WALL) {
          C2D_DrawRectSolid(x + i * ts, y + j * ts, 0, ts, ts, rgb(B.wallTop[0]));
          C2D_DrawRectSolid(x + i * ts, y + j * ts + ts - 3, 0, ts, 3, rgb(B.wallFaceDark));
        } else if (t == TR_LIQUID) {
          C2D_DrawRectSolid(x + i * ts, y + j * ts, 0, ts, ts, rgb(B.liq));
        }
      }
  };
  for (int32_t cy = c0y; cy <= c1y; ++cy)
    for (int32_t cx = c0x; cx <= c1x; ++cx) {
      const float x = v.sx(cx * kCW), y = v.sy(cy * kCW);
      C2D_Image* cur = chunks.get(w, cx, cy, biome);
      if (fk >= 0) {
        C2D_Image* old = chunks.get(w, cx, cy, w.fade_from());
        if (old) C2D_DrawImageAt(*old, x, y, 0);
        else if (!cur) rough(cx, cy, x, y);
        if (cur) { C2D_ImageTint t; C2D_AlphaImageTint(&t, old ? fk : 1.0f); C2D_DrawImageAt(*cur, x, y, 0, &t); }
      } else if (cur) {
        C2D_DrawImageAt(*cur, x, y, 0);
      } else {
        rough(cx, cy, x, y);
      }
    }

  // --- brillos del agua / burbujas de la lava ---
  {
    const Biome& B = kBiomes[biome];
    const bool lava = B.liquid == LQ_LAVA || B.liquid == LQ_POISON || B.liquid == LQ_SPARK;
    const int32_t t0x = World::tile_of(v.l), t1x = World::tile_of(v.l + cam.w), t0y = World::tile_of(v.t), t1y = World::tile_of(v.t + cam.h);
    for (int32_t ty = t0y; ty <= t1y; ++ty)
      for (int32_t tx = t0x; tx <= t1x; ++tx) {
        if (w.terrain_at(tx, ty) != TR_LIQUID) continue;
        const float ph = float(hash(tx, ty, 77) * 6.28);
        const float k = (std::sin(now * (lava ? 2.2f : 1.6f) + ph) + 1) / 2;
        const float a = lava ? 0.35f + k * 0.5f : k * 0.7f;
        const float px = v.sx(tx * kTS) + std::floor(float(hash(tx, ty, 78)) * 12 + 2) + std::round(k * 2);
        const float py = v.sy(ty * kTS) + std::floor(float(hash(tx, ty, 79)) * 12 + 2);
        C2D_DrawRectSolid(px, py, 0, lava ? 2 : 3, 1, rgbf(B.foam, a));
        if (lava && k > 0.8f) C2D_DrawRectSolid(px - std::round(k * 2), py - 2, 0, 1, 1, rgbf(B.foam, a));
      }
  }

}

void Renderer::draw_prop(const Prop& p, World& w, float vl, float vt, float now) {
  View v{vl, vt};
  const int biome = w.biome();
  const float fk = w.fade_k();
  int variant = 0;
  switch (p.type) {
    case PR_GRASS: variant = std::sin(now * 2.4f + p.wiggle) > 0.55f ? 1 : 0; break;
    case PR_ROCK: case PR_PUMPKIN: variant = p.hp < p.max_hp * 0.5f ? 1 : 0; break;
    case PR_ALTAR: variant = p.kind * 2 + (p.used ? 1 : 0); break;
    case PR_SPRING: variant = p.charge < 0.15f ? 1 : 0; break;
    case PR_TRAP: variant = p.kind; break;
    default: break;
  }
  int idx = prop_idx[biome][p.type][variant];
  // (las calabazas sólo están dibujadas en los biomas de noche: en otro, p. ej. con una grieta, la de la noche)
  if (idx < 0 && p.type == PR_PUMPKIN) idx = prop_idx[kBiomeHw][PR_PUMPKIN][variant];
  if (idx < 0) return;
  const Tex3DS_SubTexture& s = props.sub(idx);
  const float x = v.sx(p.x), y = v.sy(p.y);
  if (p.flat) { C2D_DrawImageAt(props.image(idx), std::floor(x - s.width / 2.0f), std::floor(y - 4 - s.height / 2.0f), 0); return; }
  if (p.type == PR_ALTAR && !p.used) {
    static const uint32_t AC[4] = {0xff5f6d, 0x5fd4ff, 0xff8ad8, 0xffd23f};
    ellipse(x, y - s.height + 11, 6.5f, 6.5f, rgbf(AC[p.kind & 3], 0.22f + std::sin(now * 3 + p.wiggle) * 0.1f));
  }
  if (p.type == PR_PUMPKIN) {
    // Farol: la luz de dentro, temblando como una vela (world.js de la web).
    const float a = 0.16f + std::sin(now * 7 + p.wiggle) * 0.04f + std::sin(now * 13 + p.wiggle * 2) * 0.03f;
    ellipse(x, y - s.height * 0.45f, 26, 18, rgbf(0xff9a3d, a));
  }
  if (p.type == PR_BIGCHEST && p.open > 0)
    dot_ring(x, y - s.height / 2.0f, 13, 13, 2, rgb(0xffd23f), -kPi / 2, -kPi / 2 + kTau * std::fmin(1.0f, p.open / 3.5f));
  if (p.type != PR_GRASS) ellipse(x, y, p.r * 0.55f, p.r * 0.19f, C2D_Color32(0, 0, 0, 77));
  const float px = std::floor(x - s.width / 2.0f), py = std::floor(y - s.height + (p.type == PR_GRASS ? 2 : 1));
  if (fk >= 0) {
    const int oi = prop_idx[w.fade_from()][p.type][variant];
    if (oi >= 0) {
      const Tex3DS_SubTexture& os = props.sub(oi);
      C2D_ImageTint t; C2D_AlphaImageTint(&t, 1 - fk);
      C2D_DrawImageAt(props.image(oi), std::floor(x - os.width / 2.0f), std::floor(y - os.height + (p.type == PR_GRASS ? 2 : 1)), 0, &t);
    }
    C2D_ImageTint t; C2D_AlphaImageTint(&t, fk);
    C2D_DrawImageAt(props.image(idx), px, py, 0, &t);
  } else {
    C2D_DrawImageAt(props.image(idx), px, py, 0);
  }
  props_drawn++;
  if (p.flash > 0) { C2D_ImageTint t; C2D_PlainImageTint(&t, C2D_Color32f(1, 1, 1, 0.6f), 1); C2D_DrawImageAt(props.image(idx), px, py, 0, &t); }
}


namespace {
enum DKind : int8_t { D_PROP, D_ENEMY, D_PLAYER, D_RIFT, D_ARCEUS, D_LOST, D_GOAL };
struct DItem { float y; int8_t kind; const void* p; int16_t i = 0; };
DItem items[1600];
}  // namespace

void Renderer::draw_top(Game& g, float eye) {
  if (eye <= 0) { C2D_TextBufClear(tbuf); props_drawn = 0; }
  const float far = eye * 3.0f, mid = eye * 1.5f;
  const Camera& cam = g.camera;
  View v{cam.left() - cam.sx, cam.top() - cam.sy};
  const float now = float(g.time);
  World& w = g.world;

  g_par = far;
  draw_ground(w, cam, v.l, v.t, now);

  // --- avisos en el suelo ---
  const bool blink = std::sin(now * 1000 / 60) > 0;
  for (const Hazard& h : g.hazards) {
    if (h.hit) continue;
    const float k = std::fmin(1.0f, h.t / h.delay);
    if (!h.beam) {
      const float x = v.sx(h.x), y = v.sy(h.y), r = h.r * 0.5f;
      ellipse(x, y, r, r * 0.75f, rgbf(h.color, 0.16f));
      ellipse(x, y, r * k, r * 0.75f * k, rgbf(h.color, 0.32f));
      dot_ring(x, y, r, r * 0.75f, 1, rgbf(h.color, k > 0.75f && blink ? 0.95f : 0.6f));
    } else {
      const float x = v.sx(h.x), y = v.sy(h.y), len = h.len * 0.5f, hw = h.w * 0.25f;
      quad(x, y, h.angle, 0, len, -hw, hw, rgbf(h.color, 0.14f));
      quad(x, y, h.angle, 0, len, -hw * k, hw * k, rgbf(h.color, 0.34f));
      const u32 c = rgbf(h.color, k > 0.75f && blink ? 0.95f : 0.55f);
      quad(x, y, h.angle, 0, len, -hw - 1, -hw, c);
      quad(x, y, h.angle, 0, len, hw, hw + 1, c);
    }
  }

  // --- el tesoro de la Luminosfera: un cofrecito que brilla, con su columna de luz ---
  if (g.treasure.on && cam.sees(g.treasure.x, g.treasure.y, 60)) {
    const float x = v.sx(g.treasure.x), y = v.sy(g.treasure.y), k = 0.5f + std::sin(now * 4) * 0.5f;
    C2D_DrawRectangle(x - 6, y - 70, 0, 6, 70, rgbf(0xffe680, 0), rgbf(0xffe680, 0.35f * k + 0.15f), rgbf(0xffe680, 0), rgbf(0xffe680, 0.35f * k + 0.15f));
    C2D_DrawRectangle(x, y - 70, 0, 6, 70, rgbf(0xffe680, 0.35f * k + 0.15f), rgbf(0xffe680, 0), rgbf(0xffe680, 0.35f * k + 0.15f), rgbf(0xffe680, 0));
    ellipse(x, y, 14 + k * 3, 6 + k, rgbf(0xffe680, 0.35f + k * 0.2f));
    C2D_DrawRectSolid(x - 7, y - 10, 0, 14, 9, rgb(0xb8860b));
    C2D_DrawRectSolid(x - 7, y - 12, 0, 14, 3, rgb(0xffd23f));
    C2D_DrawRectSolid(x - 1, y - 8, 0, 2, 3, rgb(0xfff6c0));
    for (int i = 0; i < 3; ++i) {
      const float a = now * 2 + i * 2.094f;
      C2D_DrawRectSolid(std::floor(x + std::cos(a) * 11), std::floor(y - 14 + std::sin(a) * 4 - k * 3), 0, 1, 1, rgb(0xffffff));
    }
  }

  // --- campos en el suelo ---
  for (const Field& f : g.fields) {
    if (!cam.sees(f.x, f.y, 60)) continue;
    const float x = v.sx(f.x), y = v.sy(f.y), r = f.r * 0.5f;
    if (f.kind == FK_VORTEX) {
      // Remolino: mancha de su color y tres arcos que giran.
      const float a = std::fmin(1.0f, std::fmin((f.life - f.t) * 2, f.t * 6));
      ellipse(x, y, r, r * 0.6f, rgbf(f.col, 0.22f * a));
      for (int k = 0; k < 3; ++k) {
        const float rr = r * (0.35f + k * 0.28f), a0 = now * (3 - k * 0.6f) + k * 2;
        dot_ring(x, y, rr, rr * 0.6f, 1, rgbf(f.col, 0.6f * a), a0, a0 + 3.4f);
      }
    } else if (f.kind == FK_STUNZONE) {
      // Aviso de la zona de aturdir: se llena hasta que estalla.
      const float k = clamp(f.t / f.delay, 0.0f, 1.0f);
      ellipse(x, y, r * k, r * k * 0.6f, rgbf(f.col, 0.15f + 0.25f * k));
      dot_ring(x, y, r, r * 0.6f, 1, rgbf(f.col, 0.8f));
    } else if (f.kind == FK_PATCH) {
      const float a = std::fmin(1.0f, std::fmin((f.life - f.t) * 2, f.t * 6));
      ellipse(x, y, r, r * 0.62f, rgbf(f.col, 0.32f * a));
      dot_ring(x, y, r * 0.8f, r * 0.5f, 1, rgbf(f.col, 0.5f * a));
    } else if (f.kind == FK_MINE) {
      const bool armed = f.t >= f.arm, bl = armed && std::sin(now * 8 + f.x) > 0.3f;
      ellipse(x, y, r * 0.45f, r * 0.28f, rgbf(f.col, 0.18f));
      draw_shape(f.vis_proj, f.vis_pal, 0, x, y - 2, 0, 1, bl ? 1 : 0.75f);
    } else if (f.kind == FK_METEOR) {
      const float k = clamp(f.t / f.fall, 0.0f, 1.0f);
      ellipse(x, y, r * (0.3f + k * 0.7f), r * 0.6f * (0.3f + k * 0.7f), C2D_Color32(0, 0, 0, uint8_t((0.2f + k * 0.3f) * 255)));
      dot_ring(x, y, r, r * 0.6f, 1, rgbf(f.col, 0.6f));
    } else if (f.kind == FK_TURRET) {
      ellipse(x, y, 5.5f, 2.25f, C2D_Color32(0, 0, 0, 77));
    }
  }

  // --- objetos del suelo ---
  for (const Pickup& p : g.pickups) {
    if (!cam.sees(p.x, p.y, 30)) continue;
    static const uint32_t COL[PK_COUNT] = {0x7fe0ff, 0xffd95e, 0x5fe08a, 0xff9ed8, 0xff7b3d, 0xffd23f, 0xffd23f, 0xc47bff, 0xffd23f, 0xc9b8ff, 0xff8a2a};
    static const uint32_t GLOW[PK_COUNT] = {0x2a8fd0, 0xc08a10, 0x1f8f4d, 0xb03d84, 0xb03d10, 0xb8860b, 0xc08a10, 0x8a3fd0, 0xc08a10, 0x7e5be0, 0xb04a10};
    static const float R[PK_COUNT] = {4.5f, 7, 6.5f, 6.5f, 6.5f, 5.5f, 7, 8, 9, 8, 7};
    const float bounce = std::fabs(std::sin(p.t * 0.8f)) * 3;
    const float r = R[p.kind] * 0.5f;
    const float x = v.sx(p.x), gy = v.sy(p.y), y = v.sy(p.y - 6 - bounce);
    ellipse(x, gy, r * 0.9f, r * 0.35f, C2D_Color32(0, 0, 0, 71));
    ellipse(x, y, r * 2, r * 2, rgbf(GLOW[p.kind], 0.3f));
    if (p.kind == PK_XP || p.kind == PK_XPBIG) {
      const u32 c = rgb(COL[p.kind]);
      C2D_DrawTriangle(x, y - r, c, x + r * 0.78f, y, c, x, y + r, c, 0);
      C2D_DrawTriangle(x, y - r, c, x, y + r, c, x - r * 0.78f, y, c, 0);
      C2D_DrawRectSolid(std::floor(x - r * 0.25f), std::floor(y - r * 0.3f), 0, 1, 1, C2D_Color32(255, 255, 255, 204));
    } else if (p.kind == PK_SPHERE) {
      const int id = int(p.value);
      if (id >= 0 && id < ORB_COUNT) {
        ellipse(x, y, 8, 8, rgbf(kOrbs[id].col, 0.3f + std::sin(p.t * 1.5f) * 0.12f));
        draw_orb(id, x, y, 6);
      }
    } else if (p.kind == PK_COIN) {
      const float sxk = std::fmax(0.25f, std::fabs(std::cos(p.t * 0.9f)));
      ellipse(x, y, r * sxk, r, rgb(COL[p.kind]));
      ellipse(x, y, r * sxk * 0.5f, r * 0.5f, rgb(0xb8860b));
    } else {
      int ic = -1;
      switch (p.kind) {
        case PK_HEAL: ic = ui_icon[4]; break;
        case PK_MAGNET: ic = ui_icon[5]; break;
        case PK_BOMB: ic = ui_icon[6]; break;
        case PK_TICKET: ic = ui_icon[2]; break;
        case PK_TICKET10: ic = ui_icon[3]; break;
        case PK_CANDY: ic = ui_icon[16]; break;
        case PK_ITEM: {
          ellipse(x, y, 9, 9, rgbf(is_mega_stone(int(p.value)) ? 0xc47bff : 0xffe14d, 0.35f + std::sin(p.t * 1.5f) * 0.15f));
          // (la megapiedra: la de tu Pokémon, que es la que te llevarías)
          if (is_mega_stone(int(p.value))) { draw_item_icon(int(p.value), std::floor(x - 7), std::floor(y - 7), 14, g.me().dex); break; }
          ic = icons.find(kItems[int(p.value)].id);
          break;
        }
        default: break;
      }
      if (ic >= 0) {
        const float sz = (p.kind == PK_TICKET10 || p.kind == PK_ITEM ? 24 : p.kind == PK_CANDY ? 20 : 18) * 0.5f;
        const float k = sz / icons.sub(ic).width;
        C2D_DrawImageAt(icons.image(ic), std::floor(x - sz / 2), std::floor(y - sz / 2), 0, nullptr, k, k);
      }
    }
  }

  // --- lo que tiene "pies", ordenado por Y ---
  g_par = mid;
  int n = 0;
  const float pad = 70;
  w.for_props_in_rect(v.l - pad, v.t - pad, v.l + cam.w + pad, v.t + cam.h + pad, [&](Prop& p) {
    if (n < 1200 && cam.sees(p.x, p.y, 70)) items[n++] = {p.flat ? p.y - 4000 : p.y, D_PROP, &p};
  });
  for (const Enemy& e : g.corpses) if (n < 1560 && cam.sees(e.x, e.y, 80)) items[n++] = {e.y, D_ENEMY, &e};
  for (const Enemy& e : g.enemies) if (n < 1560 && !e.arceus && cam.sees(e.x, e.y, 80)) items[n++] = {e.y, D_ENEMY, &e};   // (Arceus lo dibuja la Sentencia)
  for (int i = 0; i < g.nplayers; ++i) if (g.players[i].active) items[n++] = {g.players[i].y, D_PLAYER, &g.players[i]};
  if (g.rift.open) items[n++] = {g.rift.y, D_RIFT, nullptr};
  if (g.judg.a_on && g.judg.ported) items[n++] = {g.judg.ay, D_ARCEUS, nullptr};
  if (g.lost.on) { items[n++] = {g.lost.ty - 4000, D_GOAL, nullptr}; items[n++] = {g.lost.y, D_LOST, nullptr}; }
  // Orden por Y (a igualdad, el orden de llegada; sort sin memoria extra).
  for (int k = 0; k < n; ++k) items[k].i = int16_t(k);
  std::sort(items, items + n, [](const DItem& a, const DItem& b) { return a.y < b.y || (a.y == b.y && a.i < b.i); });

  for (int i = 0; i < n; ++i) {
    const DItem& it = items[i];
    switch (it.kind) {
      case D_PROP: draw_prop(*static_cast<const Prop*>(it.p), w, v.l, v.t, now); break;
      case D_ENEMY: {
        const Enemy& e = *static_cast<const Enemy*>(it.p);
        const float alpha = e.dead ? clamp(e.fade / 0.5f, 0.0f, 1.0f) : 1;
        if (alpha <= 0) break;
        const float x = v.sx(e.x), y = v.sy(e.y), r = e.r * 0.5f;
        const bool gold = e.golden || e.behavior == BH_THIEF;
        if ((e.boss || e.shiny || gold) && !e.dead) {
          const uint32_t c = e.ai.fury ? 0xff4a4a : e.shiny ? 0x9ae6ff : gold ? 0xffd23f : 0xffd95e;
          ellipse(x, y, r * 1.6f, r * 0.6f, rgbf(c, (gold && !e.boss ? 0.32f : 0.22f) + std::sin(e.bob * 0.5f) * 0.06f));
        }
        const float sr = (shadow_size(e.dex) == 0 ? 5 : shadow_size(e.dex) == 1 ? 8 : 12) * e.scale;
        ellipse(x, y, sr, sr * 0.42f, C2D_Color32(0, 0, 0, uint8_t(87 * alpha)));
        const float lift = ((e.flying && !e.boss) ? 1 + std::sin(e.bob) * 1.5f : 0) + e.jump_lift;
        draw_sprite(e.anim, x, v.sy(e.y - lift), e.flash > 0 ? 1 : 0, alpha);
        if (e.dead) break;
        // Dormido (Soporesfera): zetas que suben.
        if (e.sleep_t > 0) {
          for (int i = 0; i < 2; ++i) {
            const float k = std::fmod(now * 0.7f + i * 0.5f, 1.0f);
            text("z", x + 4 + k * 6, v.sy(e.y - lift - e.body_h - 4) - k * 10, 0.32f + k * 0.12f, rgbf(0xd9ccff, 1 - k), 1, false);
          }
        }
        // Aturdido: estrellitas girando sobre la cabeza.
        if (e.stun_t > 0) {
          for (int i = 0; i < 3; ++i) {
            const float a = now * 5 + i * 2.094f;
            draw_shape(SH_STAR, T_ELECTRIC, 0, x + std::cos(a) * 4.5f, v.sy(e.y - lift - e.body_h - 6 + std::sin(a) * 3), 0, 0.6f, 1);
          }
        }
        if (e.shield > 0) {
          ellipse(x, y - e.body_h * 0.225f, r * 1.5f, e.body_h * 0.325f, C2D_Color32(127, 200, 255, 36));
          dot_ring(x, y - e.body_h * 0.225f, r * 1.5f, e.body_h * 0.325f, 1, rgbf(0x9fd8ff, 0.4f));
        }
        if (e.burn > 0 || e.poison > 0) ellipse(x, y, r * 1.2f, r * 0.5f, rgbf(e.burn > 0 ? 0xff8a3d : 0xc86bdc, 0.45f));
        int sup = e.behavior == BH_HEALER ? ui_icon[0] : e.behavior == BH_SHIELDER ? ui_icon[8] : e.behavior == BH_SUMMONER ? ui_icon[9] : -1;
        if (sup >= 0) {
          const float k = 6.0f / icons.sub(sup).width;
          C2D_DrawImageAt(icons.image(sup), std::floor(x - 3), std::floor(y - e.body_h * 0.5f - (e.hp < e.max_hp ? 13 : 9)), 0, nullptr, k, k);
        }
        if (e.boss || e.hp < e.max_hp) {
          const float bw = e.boss ? 45 : std::fmax(11.0f, r * 2);
          const float by = v.sy(e.y - e.body_h - 10);
          bar(std::floor(x - bw / 2), by, bw, 2, e.hp / e.max_hp, rgb(e.boss ? 0xffcb3d : 0xff5f6d), C2D_Color32(0, 0, 0, 153));
        }
        break;
      }
      case D_PLAYER: {
        const Player& p = *static_cast<const Player*>(it.p);
        const float x = v.sx(p.x), y = v.sy(p.y), r = p.r * 0.5f;
        if (p.slot_n[p.active_slot] && !p.dead)
          dot_ring(x, y, r * 1.5f, r * 0.6f, 1, rgbf(kTypeColor[p.moves[p.slots[p.active_slot][0]].type()], 0.4f));
        const float sr = (shadow_size(p.dex) == 0 ? 5 : shadow_size(p.dex) == 1 ? 8 : 12) * p.scale;
        ellipse(x, y, sr, sr * 0.42f, C2D_Color32(0, 0, 0, 87));
        // Megaevolucionado: aura de colores que late a sus pies (más fuerte si
        // no tiene sprite propio: así se nota que es la mega).
        static const uint32_t RB[6] = {0xff5f6d, 0xffd23f, 0x5fe08a, 0x5fc8ff, 0xc47bff, 0xff8ad8};
        if (p.mega >= 0) {
          const float pu = 0.5f + 0.5f * std::sin(now * 5);
          const bool own = p.anim.dex != p.dex;   // (con su sprite de mega o con el suyo y aura)
          const uint32_t c = RB[int(now * 4) % 6];
          ellipse(x, y, r * (2.4f + pu * 0.4f), r * (0.9f + pu * 0.15f), rgbf(c, own ? 0.22f : 0.35f));
          dot_ring(x, y, r * (2.6f + pu * 0.5f), r * (1.0f + pu * 0.2f), 1, rgbf(RB[(int(now * 4) + 3) % 6], 0.7f));
          if (!own) ellipse(x, y - p.body_h * 0.25f, r * 1.9f, p.body_h * 0.42f, rgbf(c, 0.12f + pu * 0.08f));
        }
        if (p.power.def >= 0 && !p.dead)
          dot_ring(x, y, r * 2.1f, r * 0.85f, 2, rgbf(kPowers[p.power.def].color, 0.35f + std::sin(now * 8.3f) * 0.15f));
        const float alpha = p.invuln > 0 && !p.dead ? (int(std::floor(p.invuln * 18)) % 2 ? 0.45f : 1) : 1;
        // Transformándose: el Pokémon en blanco brillante.
        // Con su disfraz de Halloween (sus hojas: kSkins), salvo megaevolucionado.
        if (p.skin >= 0 && p.skin < kSkinCount && p.anim.dex == p.dex) {
          Animator a = p.anim;
          a.dex = kSkins[p.skin].sprite;
          a.shiny = p.anim.shiny && has_shiny(a.dex);
          draw_sprite(a, x, y, p.mega_tf > 0 ? 1 : p.flash > 0 ? 2 : 0, alpha);
        } else draw_sprite(p.anim, x, y, p.mega_tf > 0 ? 1 : p.flash > 0 ? 2 : 0, alpha);
        if (p.mega >= 0) {
          const float cy = y - p.body_h * 0.5f;
          if (p.mega_tf > 0) {
            // Rayos de luz de colores que giran y un círculo que se abre.
            const float k = 1 - p.mega_tf / kMegaTransform;
            for (int ray = 0; ray < 8; ++ray) {
              const float a = now * 3 + ray * kTau / 8;
              quad(x, cy, a, 6, 26 + k * 40, -1.5f, 1.5f, rgbf(RB[ray % 6], 0.75f * (1 - k)));
            }
            dot_ring(x, cy, 10 + k * 70, 10 + k * 60, 2, rgbf(0xffffff, 0.9f * (1 - k)));
            dot_ring(x, cy, 6 + k * 50, 6 + k * 42, 1, rgbf(RB[int(now * 10) % 6], 0.9f * (1 - k)));
          }
          // El símbolo mega sobre la cabeza.
          const float hy = y - p.body_h - 9 + std::sin(now * 3) * 1.5f;
          for (int q = 0; q < 6; ++q) {
            const float a = now * 2 + q * kTau / 6;
            C2D_DrawRectSolid(x + std::cos(a) * 4 - 1, hy + std::sin(a) * 4 - 1, 0, 2, 2, rgb(RB[q]));
          }
          C2D_DrawRectSolid(x - 1, hy - 1, 0, 2, 2, C2D_Color32(255, 255, 255, 255));
        }
        // (atk, patk, satk, spd, def, regen)
        static const uint32_t BC[BS_COUNT] = {0xff6b4d, 0xff6b4d, 0xc47bff, 0x7fe0ff, 0xb0b8c8, 0x5fe08a};
        int bi = 0;
        for (int b = 0; b < BS_COUNT; ++b) {
          if (p.buffs[b].n <= 0) continue;
          const float rr = r * 1.8f + bi * 2.5f;
          dot_ring(x, y, rr, rr * 0.42f, 1, rgbf(BC[b], 0.6f), 0, kTau * (p.buffs[b].n / 5.0f));
          bi++;
        }
        // Cooperativo: anillo de "levantando" y, sobre los compañeros, su nombre.
        if (g.nplayers > 1) {
          const int who = int(&p - g.players);
          if (p.dead && p.revive_t > 0) dot_ring(x, y, r * 2.4f, r * 1.0f, 2, rgb(0x5fe08a), -kPi / 2, -kPi / 2 + kTau * std::fmin(1.0f, p.revive_t / 3));
          if (who != g.local) {
            const float ty = y - p.body_h * 0.5f - 14;
            text(g.cfg.members[who].name, x, ty, 0.3f, rgb(p.dead ? 0xff8a96 : kCoopColor[who & 3]), 1);
            if (p.dead) text("Quédate a su lado", x, ty - 9, 0.3f, rgb(0xff8a96), 1);
          }
        }
        break;
      }
      case D_RIFT: {
        const auto& R = g.rift;
        const int idx = rift_idx[int(std::floor(R.t * 8)) % 4];
        const float a = std::fmin(1.0f, R.t * 2) * std::fmin(1.0f, R.ttl / 1.5f);
        const float x = v.sx(R.x), y = v.sy(R.y);
        ellipse(x, y, 17, 6, rgbf(0x7b3cff, 0.25f * a));
        if (idx >= 0) {
          const Tex3DS_SubTexture& s = props.sub(idx);
          C2D_ImageTint t; C2D_AlphaImageTint(&t, a);
          C2D_DrawImageAt(props.image(idx), std::floor(x - s.width / 2.0f), std::floor(y - s.height + 2), 0, &t);
        }
        break;
      }
      case D_ARCEUS: {
        const auto& J = g.judg;
        const float near = clamp(1 - J.lift / 900, 0.0f, 1.0f);
        const float x = v.sx(J.ax), y = v.sy(J.ay);
        ellipse(x, y, 22 * (0.4f + near * 0.6f), 8 * (0.4f + near * 0.6f), C2D_Color32(0, 0, 0, uint8_t((0.15f + near * 0.3f) * 255)));
        // Aura en el suelo, del color de su Tabla (en la pelea).
        const uint32_t ac = J.phase == 2 && J.plate >= 0 ? kTypeColor[J.plate] : 0xffd25e;
        if (J.lift < 40) ellipse(x, y, J.ar * 1.1f, J.ar * 0.4f, rgbf(ac, 0.22f + std::sin(now * 3) * 0.06f));
        // La rueda dorada detrás de él.
        const float wy = v.sy(J.ay - J.lift - J.abody_h * 0.55f), R = J.abody_h * 0.31f;
        dot_ring(x, wy, R, R, 2, rgbf(0xffd25e, 0.55f));
        static const uint32_t GEMS[6] = {0xff6b4d, 0x4fa8ff, 0x5fe08a, 0xffe14d, 0xb37bff, 0xc9d2de};
        for (int k = 0; k < 12; ++k) {
          const float a = J.spin * 0.6f + k * kTau / 12;
          ellipse(x + std::cos(a) * R * 0.91f, wy + std::sin(a) * R * 0.91f, 2, 2, rgbf(GEMS[k % 6], 0.7f));
        }
        bool hurt = false;   // (destello al pegarle)
        for (const Enemy& e : g.enemies) if (e.arceus) { hurt = e.flash > 0; break; }
        draw_sprite(J.aanim, x, v.sy(J.ay - J.lift), hurt ? 1 : 0, 1);
        break;
      }
      case D_GOAL: {
        const auto& L = g.lost;
        const float k = 0.5f + std::sin(now * 4) * 0.5f;
        const float x = v.sx(L.tx), y = v.sy(L.ty);
        ellipse(x, y, 28, 20, rgbf(0x5fe08a, 0.25f + k * 0.15f));
        dot_ring(x, y, 28, 20, 1, rgbf(0x7dffb0, 0.9f));
        if (ui_icon[0] >= 0) C2D_DrawImageAt(icons.image(ui_icon[0]), std::floor(x - 4), std::floor(y - 15 - k * 3), 0, nullptr, 8.0f / 12, 8.0f / 12);
        break;
      }
      case D_LOST: {
        const auto& L = g.lost;
        const float x = v.sx(L.x), y = v.sy(L.y);
        ellipse(x, y, L.r * 0.75f, L.r * 0.3f, rgbf(0x7dffb0, 0.3f));
        draw_sprite(L.anim, x, y, L.flash > 0 ? 1 : 0, 1);
        const float h = body_height(L.dex) * 0.5f;
        if (!L.follow) text("!", x, y - h - 18 - std::fabs(std::sin(now * 5)) * 3, 0.6f, rgb(0x7dffb0), 1);
        bar(x - 7.5f, y - h - 3, 15, 2, L.hp / L.max_hp, rgb(0x5fe08a), C2D_Color32(0, 0, 0, 153));
        break;
      }
    }
  }

  // --- proyectiles ---
  for (const Projectile& p : g.projectiles) {
    if (!cam.sees(p.x, p.y, 20)) continue;
    ellipse(v.sx(p.x), v.sy(p.y), p.r * 0.45f, p.r * 0.19f, C2D_Color32(0, 0, 0, 64));
    if (!p.friendly) ellipse(v.sx(p.x), v.sy(p.y), p.r * 0.75f, p.r * 0.3f, C2D_Color32(255, 60, 80, 89));
  }
  for (const Projectile& p : g.projectiles) {
    if (!cam.sees(p.x, p.y, 20)) continue;
    const int shape = p.has_vis ? int(p.vis_proj) : int(kTypeShape[T_NORMAL]);
    const int pal = p.has_vis ? int(p.vis_pal) : int(T_NORMAL);
    const float a = p.orb ? 1 : std::fmin(1.0f, (p.life - p.t) * 4);
    float angle = 0;
    if (shape >= 0 && kShapeInfo[shape].dir) angle = p.orb ? p.oa + kPi / 2 : std::atan2(p.vy, p.vx);
    else if (shape >= 0 && kShapeInfo[shape].spin) angle = p.t * 9;
    draw_shape(shape, pal, int(std::floor(p.t * 12)), v.sx(p.x), v.sy(p.y - kLift), angle, p.r >= 8 ? 1.5f : 1, a);
  }

  // --- campos en el aire ---
  for (const Field& f : g.fields) {
    if (!cam.sees(f.x, f.y, 200)) continue;
    if (f.kind == FK_METEOR && f.t > 0) {
      const float k = clamp(f.t / f.fall, 0.0f, 1.0f);
      draw_shape(f.vis_proj, f.vis_pal, int(std::floor(now * 12)), v.sx(f.x + (1 - k) * 40), v.sy(f.y - (1 - k) * 170 - 6), std::atan2(170.0f, -40.0f), 1.5f, 1);
    } else if (f.kind == FK_TURRET) {
      const float fade = std::fmin(1.0f, std::fmin((f.life - f.t) * 2, f.t * 5));
      const float bob = std::sin(now * 4 + f.x) * 2 - (f.kick > 0 ? 2 : 0);
      draw_shape(f.vis_proj, f.vis_pal, int(std::floor(now * 8)), v.sx(f.x), v.sy(f.y - 16 + bob), 0, 1.5f, fade);
    }
  }

  // --- efectos ---
  const Fx& fx = g.fx;
  for (const FxBeam& b : fx.beams) {
    const float k = 1 - b.t / b.life;
    const uint32_t* P = pal_colors(b.pal);
    const float a = std::round(b.angle / (kPi / 16)) * (kPi / 16);
    const float x = v.sx(b.x), y = v.sy(b.y), len = b.len * 0.5f, al = step_alpha(k);
    if (b.shape == SH_LIGHTNING) {
      float py = 0;
      const float seg = 2;
      for (float d = 0; d < len; d += seg) {
        const float ny = d + seg >= len ? 0 : std::round((float(hash(int(d), int(now * 30), int(b.seed))) - 0.5f) * b.width * 0.5f);
        const float lo = std::fmin(py, ny), hi = std::fmax(py, ny);
        quad(x, y, a, d - 1, d + seg + 1, lo - 2, hi + 2, rgbf(P[3], al));
        quad(x, y, a, d, d + seg, lo - 1, hi + 1, rgbf(P[4], al));
        py = ny;
      }
    } else {
      const float hw = std::fmax(1.0f, std::round(b.width * 0.25f * (0.6f + 0.4f * k)));
      quad(x, y, a, 0, len, -hw - 1, hw + 1, rgbf(P[1], al));
      quad(x, y, a, 0, len, -hw, hw, rgbf(P[2], al));
      quad(x, y, a, 0, len, -std::round(hw * 0.55f), std::round(hw * 0.55f), rgbf(P[3], al));
      quad(x, y, a, 0, len, -1, 1, rgbf(P[4], al));
      if (b.shape >= 0 && b.shape < SH_COUNT) {
        const float gap = 13, off = std::fmod(now * 210, gap);
        for (float d = off; d < len - 3; d += gap) {
          const float yo = std::sin(d * 0.6f + b.seed) * b.width * 0.09f;
          draw_shape(b.shape, b.pal, int(d / gap + now * 12), x + std::cos(a) * d - std::sin(a) * yo, y + std::sin(a) * d + std::cos(a) * yo, 0.0001f, 1.5f, al);
        }
      }
    }
  }
  for (const FxSlash& s : fx.slashes) {
    const float k = 1 - s.t / s.life;
    const uint32_t* P = pal_colors(s.pal);
    const float half = std::fmin(s.arc, kTau) / 2;
    const float r = s.radius * 0.5f * (0.72f + 0.28f * (1 - k));
    const int thick = int(std::round(3 * k));
    const float al = step_alpha(k);
    const int n = clamp(int(r * half * 2), 8, 90);
    for (int i = 0; i <= n; ++i) {
      const float u = float(i) / n, a = s.angle - half + u * half * 2;
      const int th = int(std::round(std::sin(kPi * u) * thick));
      for (int j = -1; j <= th; ++j) {
        const float rr = r - j;
        const uint32_t c = j < 0 ? P[1] : j == th ? P[4] : j > th / 2 ? P[3] : P[2];
        C2D_DrawRectSolid(std::floor(v.sx(s.x) + std::cos(a) * rr), std::floor(v.sy(s.y) + std::sin(a) * rr), 0, 1, 1, rgbf(c, al));
      }
    }
  }
  for (const FxRing& r : fx.rings) {
    const float k = r.t / r.life;
    const float rad = lerp(r.r0, r.r1, k) * 0.5f;
    if (rad > 260) continue;
    dot_ring(v.sx(r.x), v.sy(r.y), rad, rad * 0.9f, r.width >= 4 ? 2 : 1, rgbf(r.color, step_alpha(1 - k)));
  }
  for (const Particle& p : fx.parts) {
    const float s = p.s * 0.5f;
    C2D_DrawRectSolid(std::floor(v.sx(p.x)), std::floor(v.sy(p.y)), 0, s, s, rgbf(p.color, step_alpha(1 - p.t / p.life)));
  }
  for (const FxSprite& s : fx.sprites) {
    const float k = 1 - s.t / s.life;
    if (s.twinkle) {
      const float ph = s.t / s.life;
      const int st = ph < 0.15f ? 0 : ph < 0.32f ? 1 : ph < 0.62f ? 2 : ph < 0.82f ? 1 : 0;
      const float X = v.sx(s.x), Y = v.sy(s.y);
      auto dot = [&](int dx, int dy, uint32_t c) { C2D_DrawRectSolid(X + dx, Y + dy, 0, 1, 1, rgb(c)); };
      dot(0, 0, 0xffffff);
      if (st >= 1) { dot(1, 0, 0xfff3a8); dot(-1, 0, 0xfff3a8); dot(0, 1, 0xfff3a8); dot(0, -1, 0xfff3a8); }
      if (st >= 2) {
        dot(2, 0, 0xfff3a8); dot(-2, 0, 0xfff3a8); dot(0, 2, 0xfff3a8); dot(0, -2, 0xfff3a8);
        dot(3, 0, 0x9ae6ff); dot(-3, 0, 0x9ae6ff); dot(0, 3, 0x9ae6ff); dot(0, -3, 0x9ae6ff);
        dot(1, 1, 0x9ae6ff); dot(-1, 1, 0x9ae6ff); dot(1, -1, 0x9ae6ff); dot(-1, -1, 0x9ae6ff);
      }
      continue;
    }
    draw_shape(s.shape, s.pal, int(std::floor(s.t * s.fr)), v.sx(s.x), v.sy(s.y), s.angle, s.scale, s.fade ? step_alpha(k) : 1);
  }
  for (const FxText& t : fx.texts) {
    const float k = 1 - t.t / t.life;
    text(t.s, v.sx(t.x), v.sy(t.y) - 8, t.big ? 0.48f : 0.36f, rgbf(t.color, std::fmin(1.0f, k * 2.2f)), 1);
  }

  if (debug_grid) {
    // Lo que de verdad choca, sobre lo que se ve.
    g_par = far;
    auto box = [](float x, float y, float s, u32 c) {
      C2D_DrawRectSolid(x, y, 0, s, 1, c); C2D_DrawRectSolid(x, y + s - 1, 0, s, 1, c);
      C2D_DrawRectSolid(x, y, 0, 1, s, c); C2D_DrawRectSolid(x + s - 1, y, 0, 1, s, c);
    };
    const int32_t t0x = World::tile_of(v.l), t1x = World::tile_of(v.l + cam.w), t0y = World::tile_of(v.t), t1y = World::tile_of(v.t + cam.h);
    for (int32_t ty = t0y; ty <= t1y; ++ty)
      for (int32_t tx = t0x; tx <= t1x; ++tx) {
        const uint8_t t = w.terrain_at(tx, ty);
        if (t == TR_WALL) box(v.sx(tx * kTS), v.sy(ty * kTS), kTS * 0.5f, C2D_Color32(255, 40, 40, 255));
        else if (t == TR_LIQUID) box(v.sx(tx * kTS), v.sy(ty * kTS), kTS * 0.5f, C2D_Color32(60, 140, 255, 255));
        // Lo que tenía el chunk pintado (verde = pared, cian = líquido), en un cuadradito dentro.
        const int32_t cx = int32_t(std::floor(tx / 16.0)), cy = int32_t(std::floor(ty / 16.0));
        if (const uint8_t* pt = chunks.painted_terrain(cx, cy, w.biome())) {
          const uint8_t q = pt[(ty - cy * 16) * 16 + (tx - cx * 16)];
          if (q == TR_WALL) box(v.sx(tx * kTS) + 4, v.sy(ty * kTS) + 4, 8, C2D_Color32(40, 255, 40, 255));
          else if (q == TR_LIQUID) box(v.sx(tx * kTS) + 4, v.sy(ty * kTS) + 4, 8, C2D_Color32(0, 255, 255, 255));
        }
      }
    // Bordes de los chunks (donde empieza cada uno según el mundo): gris.
    for (int32_t cy = int32_t(std::floor(v.t / kCW)); cy * kCW <= v.t + cam.h; ++cy)
      C2D_DrawRectSolid(0, v.sy(cy * kCW) + 1, 0, 400, 1, C2D_Color32(200, 200, 200, 255));
    for (int32_t cx = int32_t(std::floor(v.l / kCW)); cx * kCW <= v.l + cam.w; ++cx)
      C2D_DrawRectSolid(v.sx(cx * kCW) + 1, 0, 0, 1, 240, C2D_Color32(200, 200, 200, 255));
    g_par = mid;
    for (int i = 0; i < g.nplayers; ++i) {
      const Player& p = g.players[i];
      if (!p.active) continue;
      const float x = v.sx(p.x), y = v.sy(p.y);
      dot_ring(x, y, p.r * 0.5f, p.r * 0.5f, 1, C2D_Color32(255, 255, 0, 255));
      C2D_DrawRectSolid(x - 3, y, 0, 7, 1, C2D_Color32(255, 0, 255, 255));
      C2D_DrawRectSolid(x, y - 3, 0, 1, 7, C2D_Color32(255, 0, 255, 255));
    }
  }

  g_par = 0;
  // --- el pilar de luz de Arceus ---
  if (g.judg.a_on && g.judg.ported && g.judg.phase == 1) {
    const auto& J = g.judg;
    const float a = clamp((J.ct - 5.1f) / 0.6f, 0.0f, 1.0f) * clamp((7.9f + 0.6f - J.ct) / 0.6f, 0.0f, 1.0f);
    if (a > 0) {
      const float near = clamp(1 - J.lift / 900, 0.0f, 1.0f);
      const float hw = (40 + near * 40 + std::sin(J.ct * 9) * 6) * 0.5f;
      const float x = v.sx(J.ax), bot = v.sy(J.ay + 6);
      // (Halloween: el del gran Gengar, morado y naranja)
      const u32 edge = J.gengar ? C2D_Color32(180, 107, 255, 0) : C2D_Color32(255, 210, 94, 0),
                mid = J.gengar ? C2D_Color32(255, 200, 140, uint8_t(178 * a)) : C2D_Color32(255, 248, 220, uint8_t(178 * a));
      C2D_DrawRectangle(x - hw, 0, 0, hw, bot, edge, mid, edge, mid);
      C2D_DrawRectangle(x, 0, 0, hw, bot, mid, edge, mid, edge);
      ellipse(x, bot, hw * 1.5f, hw * 0.54f, C2D_Color32(255, 243, 200, uint8_t(255 * a * (0.35f + near * 0.55f) * 0.6f)));
    }
  }

  // --- clima (en pantalla) ---
  if (g.weather.kind >= 0) {
    const WeatherDef& d = kWeather[g.weather.kind];
    const float k = weather_level(g);
    if (k > 0) {
      C2D_DrawRectSolid(0, 0, 0, TOP_W, TOP_H, C2D_Color32(d.tint[0], d.tint[1], d.tint[2], uint8_t(d.tint_a * k * 255)));
      static const int WANT[WX_COUNT] = {7, 22, 14, 16, 0};
      const int want = int(WANT[g.weather.kind] * k);
      for (int i = 0; i < want; ++i) {
        // Partículas deterministas por índice y tiempo (sólo se ven).
        const float ph = float(hash(i, 1, 99)) * 20;
        const float tt = now + ph;
        if (g.weather.kind == WX_RAIN) {
          const float x = std::fmod(float(hash(i, 2, 99)) * 600 - tt * 60, 600.0f) - 100, y = std::fmod(tt * 360, 260.0f) - 10;
          for (int j = 0; j < 3; ++j) C2D_DrawRectSolid(x + 400 - j, y - j * 3, 0, 1, 3, C2D_Color32(207, 226, 255, 128));
        } else if (g.weather.kind == WX_SNOW) {
          const float x = std::fmod(float(hash(i, 2, 99)) * 400 + std::sin(tt) * 10 + 400, 400.0f), y = std::fmod(tt * 30, 250.0f) - 5;
          C2D_DrawRectSolid(x, y, 0, 1, 1, C2D_Color32(255, 255, 255, 178));
        } else if (g.weather.kind == WX_SAND) {
          const float x = 420 - std::fmod(tt * 220, 440.0f), y = float(hash(i, 3, 99)) * 240;
          C2D_DrawRectSolid(x, y, 0, 3, 1, C2D_Color32(232, 207, 143, 102));
        } else if (g.weather.kind == WX_SUN) {
          const float x = float(hash(i, 2, 99)) * 400, y = 245 - std::fmod(tt * 11, 250.0f);
          C2D_DrawRectSolid(x, y, 0, 1, 1, C2D_Color32(255, 241, 176, 115));
        }
      }
      if (g.weather.kind == WX_FOG) {
        const u32 f0 = C2D_Color32(200, 205, 220, 0), f1 = C2D_Color32(200, 205, 220, uint8_t(115 * k));
        C2D_DrawRectangle(0, 0, 0, 80, TOP_H, f1, f0, f1, f0);
        C2D_DrawRectangle(TOP_W - 80, 0, 0, 80, TOP_H, f0, f1, f0, f1);
      }
    }
  }
  // Viñeta (bordes oscuros).
  {
    const u32 e0 = C2D_Color32(0, 0, 0, 0), e1 = C2D_Color32(0, 0, 0, 110);
    C2D_DrawRectangle(0, 0, 0, TOP_W, 36, e1, e1, e0, e0);
    C2D_DrawRectangle(0, TOP_H - 36, 0, TOP_W, 36, e0, e0, e1, e1);
    C2D_DrawRectangle(0, 0, 0, 50, TOP_H, e1, e0, e1, e0);
    C2D_DrawRectangle(TOP_W - 50, 0, 0, 50, TOP_H, e0, e1, e0, e1);
  }

  // --- avisos en pantalla ---
  if (!judgment_cinematic(g)) {
    const Enemy* boss = nullptr;
    for (const Enemy& e : g.enemies) if (e.boss && !e.dead) { boss = &e; break; }
    if (boss) {
      bar(80, 6, 240, 7, boss->hp / boss->max_hp, rgb(0xffcb3d));
      text(mon_by_dex(boss->dex)->name, 200, 4, 0.36f, C2D_Color32(32, 22, 10, 255), 1, false);
    } else if (g.judg.phase == 2 && g.judg.open && g.judg.ahp_max > 0) {
      // Modo rápido, con la guardia baja: la vida de Arceus (del color de su Tabla).
      const float k = clamp(g.judg.ahp / g.judg.ahp_max, 0.0f, 1.0f);
      C2D_DrawRectSolid(48, 3, 0, 304, 13, C2D_Color32(20, 14, 4, 220));
      C2D_DrawRectSolid(50, 5, 0, 300 * k, 9, rgb(kTypeColor[g.judg.plate]));
      C2D_DrawRectSolid(50, 5, 0, 300 * k, 3, C2D_Color32(255, 255, 255, 70));
      if (g.judg.gengar) text(g.judg.won ? "GENGAR · ¡DERROTADO!" : "GENGAR", 200, 4, 0.36f, rgb(0xffd9a8), 1, true);
      else text(g.judg.won ? "ARCEUS · ¡DERROTADO!" : "ARCEUS", 200, 4, 0.36f, rgb(0xfff3c8), 1, true);
    }
    char b[96];
    float info_y = boss ? 16 : (g.judg.phase == 2 && g.judg.open && g.judg.ahp_max > 0) ? 19 : 6;
    if (g.rift.arena) {
      char t[16]; mmss(rift_time_left(g), t, sizeof(t));
      std::snprintf(b, sizeof(b), "GRIETA · %s", t);
      text(b, 200, info_y, 0.4f, rgb(0xd9c2ff), 1); info_y += 12;
    }
    if (g.events.cur == EVN_HORDE || g.events.cur == EVN_GOLD) {
      const int left = std::max(0, int(std::ceil(g.events.cur_dur - g.events.cur_t)));
      if (g.events.cur == EVN_GOLD) std::snprintf(b, sizeof(b), "HORA DORADA · %d s", left);
      else std::snprintf(b, sizeof(b), "HORDA %s · %d s", kTypeName[g.events.horde_type], left);
      text(b, 200, info_y, 0.4f, g.events.cur == EVN_GOLD ? rgb(0xffd23f) : rgb(kTypeColor[g.events.horde_type]), 1); info_y += 12;
    }
    if (g.judg.phase == 2) {
      char t[16]; mmss(g.judg.arena_t, t, sizeof(t));
      // (Halloween: la Noche Espectral del gran Gengar bruja, sin Tabla)
      const bool gg = g.judg.gengar;
      std::snprintf(b, sizeof(b), gg ? "NOCHE ESPECTRAL · %s · JEFES %d" : "SENTENCIA · %s · JEFES %d", t, g.judg.downs);
      text(b, 200, info_y, 0.4f, rgb(gg ? 0xff8a2a : 0xffd25e), 1); info_y += 12;
      // Su Tabla (tipo) y el daño que le lleváis (modo rápido: legendarios que quedan).
      const char* pn = gg ? "Gengar bruja" : plate_name(g.judg.plate);
      if (g.judg.can_win && !g.judg.open)
        std::snprintf(b, sizeof(b), "%s (%s) · LEGENDARIOS %d/%d: túmbalos y bajará la guardia", pn, kTypeName[g.judg.plate],
                      g.judg.downs, kQuickLegends);
      else if (g.judg.open)
        std::snprintf(b, sizeof(b), "%s (%s) · ¡GUARDIA BAJA! Vida %d / %d", pn, kTypeName[g.judg.plate],
                      int(jsround(g.judg.ahp)), int(jsround(g.judg.ahp_max)));
      else std::snprintf(b, sizeof(b), gg ? "%s (%s) · DAÑO A GENGAR %d%s" : "%s (%s) · DAÑO A ARCEUS %d%s", pn, kTypeName[g.judg.plate],
                         int(jsround(float(g.judg.dealt))), g.judg.shielded ? " · le protegen" : "");
      text_fit(b, 200, info_y, 380, 0.34f, rgb(kTypeColor[g.judg.plate]), true, 1);
    }
    // Flechas en el borde: grieta, Pokémon perdido, ladrones, Arceus, shinies.
    const Player& pl = g.pl();
    auto edge = [&](float wx, float wy, uint32_t col, const char* label) {
      const float sx = v.sx(wx), sy = v.sy(wy);
      if (sx > 0 && sx < TOP_W && sy > 0 && sy < TOP_H) return;
      const float M = 22, ex = clamp(sx, M, TOP_W - M), ey = clamp(sy, M + 20, TOP_H - M - 10);
      const float a = std::atan2(sy - ey, sx - ex);
      triangle_arrow(ex, ey, a, 16, 9, BLACK);
      triangle_arrow(ex, ey, a, 14, 7, rgbf(col, 0.65f + std::sin(now * 6.2f) * 0.35f));
      const float ca = std::cos(a), lx = ex - ca * 14, w = text_width(label, 0.34f);
      // (siempre dentro de la pantalla: en inglés los textos son más largos)
      const float left = clamp(ca > 0.3f ? lx - w : ca < -0.3f ? lx : lx - w / 2, 2.0f, TOP_W - 2 - w);
      text(label, left, ey - std::sin(a) * 12 - 5, 0.34f, rgb(col), 0);
    };
    if (g.nplayers > 1)
      for (int i = 0; i < g.nplayers; ++i) {
        const Player& o = g.players[i];
        if (i == g.local || !o.active) continue;
        edge(o.x, o.y - 10, o.dead ? 0xff8a96 : kCoopColor[i & 3], g.cfg.members[i].name);
      }
    if (g.rift.open) {
      std::snprintf(b, sizeof(b), "GRIETA · %d m · %d s", int(dist(pl.x, pl.y, g.rift.x, g.rift.y) / 32), int(std::ceil(g.rift.ttl)));
      edge(g.rift.x, g.rift.y - 40, 0xb48aff, b);
    }
    if (g.lost.on) {
      const float tx = g.lost.follow ? g.lost.tx : g.lost.x, ty = g.lost.follow ? g.lost.ty : g.lost.y - 10;
      std::snprintf(b, sizeof(b), "%s · %d m", g.lost.follow ? "Marca" : mon_by_dex(g.lost.dex)->name, int(dist(pl.x, pl.y, tx, ty) / 32));
      edge(tx, ty, 0x7dffb0, b);
    }
    for (const Enemy& e : g.enemies) {
      if (e.dead) continue;
      if (e.behavior == BH_THIEF) edge(e.x, e.y - 10, 0xffd23f, mon_by_dex(e.dex)->name);
      else if (e.shiny) edge(e.x, e.y - 10, 0x9ae6ff, "shiny");
    }
    if (g.treasure.on) {   // (Luminosfera)
      std::snprintf(b, sizeof(b), "TESORO · %d m", int(dist(pl.x, pl.y, g.treasure.x, g.treasure.y) / 32));
      edge(g.treasure.x, g.treasure.y - 10, 0xffe680, b);
    }
    if (g.judg.phase == 2) {
      std::snprintf(b, sizeof(b), "%s · %d m", g.judg.gengar ? "GENGAR" : "ARCEUS", int(dist(pl.x, pl.y, g.judg.ax, g.judg.ay) / 32));
      edge(g.judg.ax, g.judg.ay - g.judg.abody_h * 0.5f, 0xffd25e, b);
    }
    // Aviso al abrirse una grieta.
    if (g.rift.open && g.rift.t < 4.5f) {
      const float a = std::fmin(1.0f, std::fmin(g.rift.t * 4, (4.5f - g.rift.t) * 2));
      C2D_DrawRectSolid(30, 40, 0, 340, 34, C2D_Color32(26, 11, 51, uint8_t(224 * a)));
      text("¡SE HA ABIERTO UNA GRIETA!", 200, 42, 0.55f, rgbf(0xe2d0ff, a), 1);
      text("Sigue la flecha morada: dentro te espera un legendario", 200, 60, 0.36f, rgbf(0xc9b0ff, a), 1);
    }
    if (g.banner_t > 0 && g.banner[0]) {
      // Aviso del centro: en una línea si cabe; si no, partido en líneas y, si
      // aun así es largo, con la letra algo menor (antes se salía por los lados).
      const float a = std::fmin(1.0f, g.banner_t * 1.6f);
      const float W = 376;
      float sz = 0.52f;
      int lines = 1;
      if (text_width(g.banner, sz) > W) {
        sz = 0.44f;
        lines = int(text_width(g.banner, sz) / W) + 1;
        if (lines > 2) { sz = 0.36f; lines = int(text_width(g.banner, sz) / W) + 1; }
      }
      const float lh = sz * 34, h = lines * lh + 6;
      C2D_DrawRectSolid(200 - (lines > 1 ? W / 2 + 6 : text_width(g.banner, sz) / 2 + 8), 72, 0,
                        lines > 1 ? W + 12 : text_width(g.banner, sz) + 16, h, C2D_Color32(10, 14, 28, uint8_t(150 * a)));
      if (lines == 1) text(g.banner, 200, 74, sz, rgbf(0xffcb3d, a), 1);
      else text_wrap(g.banner, 200, 74, sz, rgbf(0xffcb3d, a), W, 1);
    }
  }

  // --- la Sentencia ---
  if (g.judg.phase == 1) {
    const auto& J = g.judg;
    const float t = J.ct;
    if (t < 5.2f) C2D_DrawRectSolid(0, 0, 0, TOP_W, TOP_H, C2D_Color32(10, 6, 2, uint8_t(clamp(t / 1.1f, 0.0f, 1.0f) * 0.86f * 255)));
    const float a = clamp((t - 1.3f + 0.3f) / 0.5f, 0.0f, 1.0f) * clamp((4.7f + 0.2f - t) / 0.35f, 0.0f, 1.0f);
    if (a > 0) {
      const uint32_t col = J.gengar ? 0xb46bff : 0xffd25e;
      dot_ring(200, 80, 56, 56, 2, rgbf(col, a * 0.9f));
      dot_ring(200, 80, 46, 46, 1, rgbf(col, a * 0.9f));
      Animator big = J.aanim;
      big.cur = A_IDLE; big.frame = 0; big.dir = 0; big.scale = 2;
      draw_sprite(big, 200, 120, 1, a * 0.85f);
      // (Halloween: «LA NOCHE DE GENGAR», judgment.js de la web)
      text(J.gengar ? "LA NOCHE" : "LA SENTENCIA", 200, 148, 0.9f, rgbf(col, a), 1);
      text(J.gengar ? "DE GENGAR" : "DE ARCEUS", 200, 178, 0.7f, rgbf(col, a), 1);
      if (t > 2.9f) text(J.gengar ? "¡Truco o trato!" : "El tiempo se ha agotado…", 200, 204, 0.4f,
                         rgbf(J.gengar ? 0xffd9a8 : 0xf3e2b0, a * clamp((t - 2.9f) / 0.6f, 0.0f, 1.0f)), 1);
    }
    if (t > 5.1f) {
      const float k = clamp((t - 5.1f) / 0.5f, 0.0f, 1.0f) * clamp((10.1f - t) / 0.6f, 0.0f, 1.0f);
      const float bh = std::round(TOP_H * 0.11f * k);
      C2D_DrawRectSolid(0, 0, 0, TOP_W, bh, BLACK);
      C2D_DrawRectSolid(0, TOP_H - bh, 0, TOP_W, bh, BLACK);
    }
    const float bb = clamp((t - 7.9f - 0.15f) / 0.3f, 0.0f, 1.0f) * clamp((10.1f - t) / 0.5f, 0.0f, 1.0f);
    if (bb > 0) {
      text("¡ARCEUS HA DESCENDIDO!", 200, 44, 0.7f, rgbf(0xffd25e, bb), 1);
      text("Es invencible: huye y aguanta todo lo que puedas", 200, 70, 0.38f, rgbf(0xffe9b0, bb), 1);
    }
    if (J.can_skip && t < 9.1f) text("A: saltar", 392, 224, 0.36f, rgbf(0xf3e2b0, 0.55f + std::sin(t * 4) * 0.2f), 2);
  } else if (g.judg.phase == 2) {
    const u32 e0 = C2D_Color32(120, 40, 10, 0), e1 = C2D_Color32(120, 40, 10, 90);
    C2D_DrawRectangle(0, 0, 0, 70, TOP_H, e1, e0, e1, e0);
    C2D_DrawRectangle(TOP_W - 70, 0, 0, 70, TOP_H, e0, e1, e0, e1);
  }

  // --- destello de pantalla completa ---
  if (g.flash_t > 0) C2D_DrawRectSolid(0, 0, 0, TOP_W, TOP_H, rgbf(g.flash_col, std::fmin(1.0f, g.flash_t / g.flash_dur * 1.4f)));
}

// ---------------- pantalla de abajo ----------------

void Renderer::draw_bottom(Game& g, bool autotest) {
  C2D_TextBufClear(tbuf);
  const Player& p = g.me();
  C2D_DrawRectSolid(0, 0, 0, BOT_W, BOT_H, C2D_Color32(16, 22, 34, 255));
  char b[128];

  // --- experiencia, vida y nivel ---
  bar(0, 0, BOT_W, 5, p.xp / p.xp_next, rgb(0x4fd1ff));
  text(p.mon ? p.mon->name : "", 8, 8, 0.5f, rgb(0xeaf2ff), 0);
  std::snprintf(b, sizeof(b), "Nv. %d", p.level);
  text(b, 8, 26, 0.45f, rgb(0xffcb3d), 0);
  bar(60, 30, 172, 10, p.hp / p.max_hp, rgb(0xff5f6d));
  std::snprintf(b, sizeof(b), "%d / %d", int(std::ceil(p.hp)), int(jsround(p.max_hp)));
  text(b, 146, 29, 0.36f, C2D_Color32(255, 255, 255, 240), 1);
  char t[16]; mmss(float(g.time), t, sizeof(t));
  text(t, 312, 6, 0.7f, C2D_Color32(255, 255, 255, 240), 2);
  // Potenciadores (al lado de la vida).
  {
    static const char* const LBL[BS_COUNT] = {"ATK", "ATK", "AT.E", "VEL", "DEF", "REG"};
    static const uint32_t COL[BS_COUNT] = {0xff6b4d, 0xff6b4d, 0xc47bff, 0x7fe0ff, 0xb0b8c8, 0x5fe08a};
    float bx = 238;
    for (int k = 0; k < BS_COUNT; ++k) {
      if (p.buffs[k].n <= 0) continue;
      std::snprintf(b, sizeof(b), "%s×%d", LBL[k], p.buffs[k].n);
      text(b, bx, 28, 0.34f, rgb(COL[k]), 0);
      bx += text_width(b, 0.34f) + 4;
    }
  }

  // --- elecciones ---
  if ((g.state == ST_LEVELUP && p.picked) || (g.state == ST_CHOICE && p.choice == CH_NONE)) {
    // En grupo: has elegido (o eliges otro) y se espera a los demás.
    char names[64] = "";
    for (int i = 0; i < g.nplayers; ++i) {
      const Player& o = g.players[i];
      if (!o.active || i == g.local) continue;
      if ((g.state == ST_LEVELUP && !o.picked) || (g.state == ST_CHOICE && o.choice != CH_NONE)) {
        if (names[0]) std::strncat(names, ", ", sizeof(names) - std::strlen(names) - 1);
        std::strncat(names, g.cfg.members[i].name, sizeof(names) - std::strlen(names) - 1);
      }
    }
    text(g.state == ST_LEVELUP ? "Esperando a que elijan carta…" : "Un compañero está eligiendo…", 160, 96, 0.45f, rgb(0xffcb3d), 1);
    text(names, 160, 116, 0.4f, rgb(0xeaf2ff), 1);
  } else if (g.state == ST_LEVELUP) {
    const bool four = p.ncards > 3;
    if (four) std::snprintf(b, sizeof(b), "¡Nivel %d! 4 cartas (%d pasivas)", p.level, levelup_min_passive(p));
    else std::snprintf(b, sizeof(b), "¡Nivel %d! Elige una mejora", p.level);
    text_fit(b, 160, four ? 68 : 74, 300, four ? 0.44f : 0.5f, rgb(0xffcb3d), true, 1);
    const float ch = card_h(p.ncards);
    for (int i = 0; i < p.ncards; ++i) {
      const Card& c = p.cards[i];
      const float y = card_y(i, p.ncards);
      const bool sel = i == card_cursor;
      C2D_DrawRectSolid(10, y, 0, 300, ch, sel ? C2D_Color32(52, 70, 110, 255) : C2D_Color32(30, 40, 62, 255));
      if (sel) { C2D_DrawRectSolid(10, y, 0, 3, ch, rgb(0xffcb3d)); }
      const char* title = "";
      const char* desc = "";
      char tag[64] = "";
      int ic = -1;
      uint32_t col = 0x8fa3c4;
      char fit[40] = "";
      if (c.type == CT_NEW) {
        const MoveDef& d = kMoves[c.move];
        title = d.name; desc = d.desc; ic = move_icon[c.move]; col = kTypeColor[d.type];
        // Tipo · Físico/Especial, y si es de su lado flojo, cuánto rinde (stats.h).
        std::snprintf(tag, sizeof(tag), "%s%s", kTypeName[d.type], d.cat == CAT_PHYSICAL ? " · Físico" : d.cat == CAT_SPECIAL ? " · Especial" : "");
        const float f = stats_fit(p.mon, d.cat);
        if (f < 0.95f) std::snprintf(fit, sizeof(fit), "rinde un %d %%", int(jsround(f * 100)));
      }
      else if (c.type == CT_UP) { const MoveInst& m = p.moves[c.move]; title = m.name(); desc = move_next_up_text(m); std::snprintf(tag, sizeof(tag), "Nv.%d > %d", m.lvl, m.lvl + 1); ic = move_icon[m.def]; col = kTypeColor[m.type()]; }
      else if (c.type == CT_EVO) {
        const MoveInst& m = p.moves[c.move];
        static char tt[80];
        std::snprintf(tt, sizeof(tt), "%s → %s", m.name(), m.d().evo_name);
        title = tt; desc = kEvoMods[m.kind()].text; std::snprintf(tag, sizeof(tag), "EVOLUCIÓN"); ic = move_icon[m.def]; col = 0xffd23f;
      } else {
        const LevelStatDef& s = kLevelStats[c.stat];
        title = s.title; desc = s.desc; ic = icons.find(s.icon);
        if (c.evo_for >= 0) { std::snprintf(tag, sizeof(tag), "Evoluciona %s", p.moves[c.evo_for].name()); col = 0xffd23f; }
        else if (four) { std::snprintf(tag, sizeof(tag), "Pasiva"); col = 0x7dffb0; }
        else std::snprintf(tag, sizeof(tag), "Mejora");
      }
      const float isz = four ? 24.0f : 28.0f;
      if (ic >= 0) C2D_DrawImageAt(icons.image(ic), 16, y + (ch - isz) / 2, 0, nullptr, isz / icons.sub(ic).width, isz / icons.sub(ic).width);
      const float tw = std::fmax(text_width(tag, 0.34f), fit[0] ? text_width(fit, 0.3f) : 0);
      text_fit(title, 50, y + (four ? 1 : 3), 254 - tw - 6, four ? 0.42f : 0.45f, C2D_Color32(240, 244, 255, 255), true);
      text(tag, 304, y + (four ? 2 : 4), 0.34f, rgb(col), 2);
      if (fit[0]) text(fit, 304, y + (four ? 12 : 14), 0.3f, rgb(0xff8a8a), 2);
      text_fit2(desc ? desc : "", 50, y + (four ? 14 : 18), 256, 0.32f, rgb(0xb7c4dc));
    }
    const int left = 1 + p.reroll_bonus - p.rerolls;
    std::snprintf(b, sizeof(b), "A: elegir · Y: cambiar cartas (%d)", left > 0 ? left : 0);
    text(b, 160, 226, 0.34f, rgb(0x8fa3c4), 1);
  } else if (g.state == ST_CHOICE && p.choice == CH_ITEM) {
    text(p.item_cur < 0 ? "¿Te lo quedas?" : "Sólo puedes llevar un objeto", 160, 74, 0.48f, rgb(0xffcb3d), 1);
    for (int i = 0; i < 2; ++i) {
      const int it = i == 0 ? p.item_cur : p.item_next;
      const float y = 96 + i * 54;
      const bool sel = i == card_cursor;
      C2D_DrawRectSolid(10, y, 0, 300, 48, sel ? C2D_Color32(52, 70, 110, 255) : C2D_Color32(30, 40, 62, 255));
      if (it < 0) {   // (no llevas ninguno: dejarlo)
        text("Dejarlo", 54, y + 4, 0.46f, C2D_Color32(240, 244, 255, 255), 0);
        text("Sigues sin objeto (puedes coger otro luego)", 54, y + 24, 0.32f, rgb(0xb7c4dc), 0, false);
        continue;
      }
      draw_item_icon(it, 16, y + 8, 30, p.dex);
      const char* who = i == 0 ? "El que llevas" : p.item_cur < 0 ? "Cogerlo" : "El nuevo";
      text_fit(item_name(it, p.dex), 54, y + 4, 250 - text_width(who, 0.34f) - 6, 0.46f, C2D_Color32(240, 244, 255, 255), true);
      text(who, 304, y + 4, 0.34f, rgb(0x8fa3c4), 2);
      text_fit2(item_desc(it, p.dex), 54, y + 22, 250, 0.32f, rgb(0xb7c4dc));
    }
    text("A: quedarte con el elegido", 160, 226, 0.34f, rgb(0x8fa3c4), 1);
  } else if (g.state == ST_CHOICE && p.choice == CH_BOND) {
    int8_t loose[4]; int nl, grow;
    const int mode = player_bond_options(p, loose, nl, grow);
    text_fit(mode == 1 ? "¡Vínculo! Elige qué movimiento se une" : "¡Vínculo! Elige dos para unir", 160, 74, 300, 0.44f, rgb(0xff8ad8), false, 1);
    for (int i = 0; i < nl; ++i) {
      const MoveInst& m = p.moves[loose[i]];
      const float y = 92 + i * 32;
      const bool sel = i == card_cursor;
      const bool first = mode != 1 && loose[i] == bond_first;   // (el primero ya elegido)
      if (first) C2D_DrawRectSolid(8, y - 2, 0, 304, 32, rgb(0xff8ad8));
      C2D_DrawRectSolid(10, y, 0, 300, 28, sel ? C2D_Color32(80, 52, 90, 255) : first ? C2D_Color32(64, 40, 74, 255) : C2D_Color32(30, 40, 62, 255));
      if (move_icon[m.def] >= 0) C2D_DrawImageAt(icons.image(move_icon[m.def]), 16, y + 4, 0, nullptr, 1.25f, 1.25f);
      std::snprintf(b, sizeof(b), "%s  Nv.%d", m.name(), m.lvl);
      text_fit(b, 44, y + 6, first ? 172 : 260, 0.42f, C2D_Color32(240, 244, 255, 255), true);
      if (first) {
        C2D_DrawRectSolid(226, y + 5, 0, 80, 18, rgb(0xff8ad8));
        text_fit("1.º ELEGIDO", 266, y + 8, 74, 0.34f, C2D_Color32(40, 16, 48, 255), false, 1);
      }
    }
    if (mode == 1) text("A: elegir", 160, 226, 0.34f, rgb(0x8fa3c4), 1);
    else if (bond_first < 0) text("A: elegir el primero", 160, 226, 0.34f, rgb(0x8fa3c4), 1);
    else text("A: elegir el segundo · B (o A en el marcado): quitar", 160, 226, 0.34f, rgb(0x8fa3c4), 1);
  } else if (g.state == ST_PAUSED) {
    // Resumen de la partida (pauseSummary de run-ui.js): ataques con su
    // categoría, daño hecho y daño por segundo, y las estadísticas.
    text("PAUSA", 160, 44, 0.6f, rgb(0xffcb3d), 1);
    text("Ataque", 12, 64, 0.32f, rgb(0x8fa3c4), 0);
    text("Daño", 236, 64, 0.32f, rgb(0x8fa3c4), 2);
    text("por seg.", 308, 64, 0.32f, rgb(0x8fa3c4), 2);
    for (int i = 0; i < p.nmoves && i < 8; ++i) {
      const MoveInst& m = p.moves[i];
      const float y = 78 + i * 13;
      const int ic = move_icon[m.def];
      if (ic >= 0) C2D_DrawImageAt(icons.image(ic), 10, y, 0, nullptr, 0.75f, 0.75f);
      std::snprintf(b, sizeof(b), "%s Nv.%d", m.name(), m.lvl);
      text_fit(b, 24, y, 118, 0.32f, rgb(0xeaf2ff), true);
      const int cat = m.cat();
      text(cat == CAT_PHYSICAL ? "Físico" : cat == CAT_SPECIAL ? "Especial" : "Mejora", 168, y, 0.3f, rgb(cat < 0 ? 0x8fa3c4 : kTypeColor[m.type()]), 1);
      if (cat >= 0) {
        const float secs = std::fmax(5.0f, float(g.time) - m.since);
        std::snprintf(b, sizeof(b), "%d", int(jsround(m.dealt)));
        text(b, 236, y, 0.32f, rgb(0xeaf2ff), 2);
        std::snprintf(b, sizeof(b), "%d/s", int(jsround(m.dealt / secs)));
        text(b, 308, y, 0.32f, rgb(0xffd23f), 2);
      }
    }
    // Ataque físico y especial (con sus danzas) y defensas.
    const float ap = player_atk(p) * stats_fit(p.mon, CAT_PHYSICAL) * (1 + p.buffs[BS_PATK].n * p.buffs[BS_PATK].amount);
    const float as = player_atk(p) * stats_fit(p.mon, CAT_SPECIAL) * (1 + p.buffs[BS_SATK].n * p.buffs[BS_SATK].amount);
    std::snprintf(b, sizeof(b), "Ataque ×%.2f · At. Esp. ×%.2f · Defensa %d · Def. Esp. %d", ap, as, p.mon->df, p.mon->sd);
    for (char* c = b; *c; ++c) if (*c == '.' && c[1] >= '0' && c[1] <= '9' && c > b && c[-1] >= '0' && c[-1] <= '9') *c = ',';
    text_fit(b, 160, 186, 304, 0.3f, rgb(0xb7c4dc), true, 1);
    if (p.item >= 0) {   // el objeto que llevas, con lo que hace (entero)
      std::snprintf(b, sizeof(b), "%s: %s", item_name(p.item, p.dex), item_desc(p.item, p.dex));
      text_fit(b, 8, 209, 304, 0.3f, rgb(0xffd23f));
    }
    std::snprintf(b, sizeof(b), "Velocidad %d · Reducción %d %% · Regeneración %.1f/s", int(jsround(player_spd(g, p))), int(jsround(player_reduction(p) * 100)), player_regen(p));
    text_fit(b, 160, 199, 304, 0.3f, rgb(0xb7c4dc), true, 1);
    text("START: seguir · SELECT: abandonar la partida", 160, 224, 0.34f, rgb(0xeaf2ff), 1);
  } else if (g.state == ST_OVER) {
    const RunSummary& s = g.summary;
    if (s.victory) {
      // ¡Arceus derrotado! (modo rápido): título, medalla y premio.
      const float pulse = 0.75f + 0.25f * std::sin(float(g.time) * 4);
      text(s.hw ? "¡HAS DERROTADO AL GRAN GENGAR!" : "¡HAS DERROTADO A ARCEUS!", 160, 70, 0.55f, rgbf(s.hw ? 0xff8a2a : 0xffd25e, pulse), 1);
      draw_medal(30, 84, 10);
      draw_medal(290, 84, 10);
    } else text(s.dead ? "Te has debilitado" : "Fin de la partida", 160, 74, 0.6f, rgb(0xffcb3d), 1);
    mmss(s.time, t, sizeof(t));
    std::snprintf(b, sizeof(b), "Tiempo %s · Nivel %d · %d derrotados · %d jefes", t, s.level, s.kills, s.bosses);
    text_fit(b, 160, 100, 304, 0.38f, rgb(0xeaf2ff), false, 1);
    if (s.hw && s.candy > 0) std::snprintf(b, sizeof(b), "+%d Pokémonedas · %d tickets · %d ×10 · %d caramelos", s.coins, s.t1, s.t10, s.candy);
    else std::snprintf(b, sizeof(b), "+%d Pokémonedas · %d tickets · %d ×10", s.coins, s.t1, s.t10);
    text_fit(b, 160, 118, 304, 0.4f, rgb(0xffd23f), false, 1);
    std::snprintf(b, sizeof(b), "tiempo %d · derrotados %d · jefes %d · recogidas %d", s.parts[0], s.parts[1], s.parts[2], s.parts[3]);
    text(b, 160, 136, 0.32f, rgb(0x8fa3c4), 1);
    if (s.judge_time >= 0) {
      mmss(s.judge_time, t, sizeof(t));
      std::snprintf(b, sizeof(b), s.hw ? "Ante Gengar: %s · %d legendarios · daño %d" : "Ante Arceus: %s · %d legendarios · daño %d", t, s.judge_bosses,
                    int(jsround(float(s.judge_dmg))));
      text_fit(b, 160, 152, 304, 0.36f, rgb(0xffd25e), false, 1);
    }
    if (s.victory) {
      std::snprintf(b, sizeof(b), "Premio: +%d Pokémonedas · +%d tickets ×10 · Medalla de Arceus", s.win_coins, s.win_t10);
      text_fit(b, 160, 168, 304, 0.36f, rgb(0x5fe08a), true, 1);
    } else if (s.record) text("¡Nuevo récord de tiempo!", 160, 168, 0.42f, rgb(0x5fe08a), 1);
    if (s.ncaught) {
      char sh[96] = "Shinies:";
      for (int i = 0; i < s.ncaught && i < 4; ++i) {
        char one[40];
        std::snprintf(one, sizeof(one), " %s%s", mon_by_dex(s.caught_dex[i])->name, s.caught_new[i] ? "" : " (+300)");
        std::strncat(sh, one, sizeof(sh) - std::strlen(sh) - 1);
      }
      text_fit(sh, 160, 184, 304, 0.36f, rgb(0x9ae6ff), true, 1);
    }
    text(g.nplayers > 1 ? "A o B: volver al menú" : "A: otra partida · B: menú", 160, 206, 0.45f, rgb(0xeaf2ff), 1);
    text(g.nplayers > 1 ? "Arriba, los ataques de cada uno (L/R)" : "Arriba, el daño de cada ataque", 160, 224, 0.32f, rgb(0x8fa3c4), 1);
  } else {
    // --- minimapa (con franjas de información encima) ---
    const float MX = 8, MY = 44, MW = 304, MH = kMapH;
    minimap.draw(g, *this, MX, MY, MW, MH, float(g.time));
    const u32 strip = C2D_Color32(8, 12, 22, 170);
    C2D_DrawRectSolid(MX, MY, 0, MW, 11, strip);
    text(g.world.biome_name(), MX + 3, MY + 1, 0.32f, rgb(0xb7c4dc), 0, false);
    if (g.time < 8 && legend_t <= 0) text("Toca el mapa: qué es cada cosa", MX + MW - 3, MY + 1, 0.32f, rgb(0x9ae6ff), 2, false);
    else {
      if (g.hw) std::snprintf(b, sizeof(b), "%d derrotados · %d monedas · %d caramelos", p.kills, p.coins, p.candy);
      else std::snprintf(b, sizeof(b), "%d derrotados · %d monedas", p.kills, p.coins);
      text(b, MX + MW - 3, MY + 1, 0.32f, rgb(0xffd23f), 2, false);
    }
    {
      // Abajo del todo, a lo ancho: el objeto que llevas y lo que hace. Encima,
      // a la izquierda clima y poder de altar; a la derecha, los vínculos.
      float ly = MY + MH - 11;
      if (p.item >= 0) {
        C2D_DrawRectSolid(MX, ly, 0, MW, 11, strip);
        draw_item_icon(p.item, MX + 2, ly, 10, p.dex);
        const float nw = text_width(item_name(p.item, p.dex), 0.32f);
        text(item_name(p.item, p.dex), MX + 14, ly + 1, 0.32f, rgb(0xffd23f), 0, false);
        text_fit(item_desc(p.item, p.dex), MX + 18 + nw, ly + 1, MW - 22 - nw, 0.32f, rgb(0xeaf2ff));
        ly -= 11;
        // Megaevolución: lista (Y), en marcha (lo que queda) o en recarga.
        if (is_mega_stone(p.item)) {
          C2D_DrawRectSolid(MX, ly, 0, MW, 11, strip);
          if (p.mega >= 0) {
            const float k = clamp(p.mega_t / mega_boost(p.mega).time, 0.0f, 1.0f);
            static const uint32_t RB[6] = {0xff5f6d, 0xffd23f, 0x5fe08a, 0x5fc8ff, 0xc47bff, 0xff8ad8};
            const float bw = (MW - 4) * k;
            for (int s = 0; s < 6; ++s) C2D_DrawRectSolid(MX + 2 + bw * s / 6, ly + 2, 0, bw / 6 + 0.5f, 7, rgbf(RB[(s + int(g.time * 6)) % 6], 0.75f));
            std::snprintf(b, sizeof(b), "%s · %d s", kMegas[p.mega].mon.name, int(std::ceil(p.mega_t)));
            text(b, MX + MW / 2, ly + 1, 0.32f, rgb(0xffffff), 1);
          } else if (p.mega_cd > 0) {
            const float k = 1 - clamp(p.mega_cd / kMegaCooldown, 0.0f, 1.0f);
            C2D_DrawRectSolid(MX + 2, ly + 2, 0, (MW - 4) * k, 7, C2D_Color32(110, 80, 170, 200));
            std::snprintf(b, sizeof(b), "Megaevolución: recarga %d s", int(std::ceil(p.mega_cd)));
            text(b, MX + MW / 2, ly + 1, 0.32f, rgb(0xc9bde8), 1, false);
          } else {
            const float pulse = 0.6f + 0.4f * std::sin(float(g.time) * 6);
            text("Y: ¡Megaevolucionar!", MX + MW / 2, ly + 1, 0.32f, rgbf(0xffd23f, pulse), 1);
          }
          ly -= 11;
        }
      }
      const float row0 = ly;
      if (g.weather.kind >= 0 && weather_level(g) > 0) {
        std::snprintf(b, sizeof(b), "%s · %ds", kWeather[g.weather.kind].name, int(std::ceil(g.weather.dur - g.weather.t)));
        C2D_DrawRectSolid(MX, ly, 0, text_width(b, 0.32f) + 6, 11, strip);
        text(b, MX + 3, ly + 1, 0.32f, rgb(kWeather[g.weather.kind].color), 0, false);
        ly -= 11;
      }
      if (p.power.def >= 0) {
        std::snprintf(b, sizeof(b), "%s %ds", kPowers[p.power.def].name, int(std::ceil(p.power.t)));
        C2D_DrawRectSolid(MX, ly, 0, text_width(b, 0.32f) + 6, 11, strip);
        text(b, MX + 3, ly + 1, 0.32f, rgb(kPowers[p.power.def].color), 0, false);
        ly -= 11;
      }
      // Las mejoras de las esferas que tienes puestas, con lo que les queda.
      for (int id = 0; id < ORB_COUNT; ++id) {
        if (!(p.orb_buff[id] > 0)) continue;
        std::snprintf(b, sizeof(b), "%s · %ds", orb_effect(id), int(std::ceil(p.orb_buff[id])));
        C2D_DrawRectSolid(MX, ly, 0, text_width(b, 0.32f) + 6, 11, strip);
        text(b, MX + 3, ly + 1, 0.32f, rgb(kOrbs[id].col), 0, false);
        ly -= 11;
      }
      float ry = row0;
      if (p.bonds > 0) {
        std::snprintf(b, sizeof(b), "Vínculos: %d", p.bonds);
        C2D_DrawRectSolid(MX + MW - text_width(b, 0.32f) - 6, ry, 0, text_width(b, 0.32f) + 6, 11, strip);
        text(b, MX + MW - 3, ry + 1, 0.32f, rgb(0xff8ad8), 2, false);
      }
    }

    if (legend_t > 0) { minimap.legend(*this, MX, MY + 11, MW, MH - 11); legend_t -= 1.0f / 60; }

    // --- huecos de movimientos (tocar para cambiar) ---
    const float S = kSlotS, GAP = kSlotGap;
    const int n = std::max(1, p.nslots);
    const float x0 = (BOT_W - (n * S + (n - 1) * GAP)) / 2;
    for (int i = 0; i < p.nslots; ++i) {
      const float x = x0 + i * (S + GAP), y = kSlotY;
      const bool act = i == p.active_slot;
      const bool bond = p.slot_n[i] > 1;
      C2D_DrawRectSolid(x, y, 0, S, S, act ? C2D_Color32(52, 70, 110, 255) : C2D_Color32(28, 36, 56, 255));
      if (act) { C2D_DrawRectSolid(x, y, 0, S, 2, rgb(bond ? 0xff8ad8 : 0xffcb3d)); C2D_DrawRectSolid(x, y + S - 2, 0, S, 2, rgb(bond ? 0xff8ad8 : 0xffcb3d)); }
      for (int j = 0; j < p.slot_n[i]; ++j) {
        const MoveInst& m = p.moves[p.slots[i][j]];
        const int ic = move_icon[m.def];
        const float sz = p.slot_n[i] > 1 ? 1.0f : 1.75f;
        const float ix = p.slot_n[i] > 1 ? x + 5 + (j % 2) * 18 : x + 8, iy = p.slot_n[i] > 1 ? y + 3 + (j / 2) * 15 : y + 3;
        if (ic >= 0) C2D_DrawImageAt(icons.image(ic), ix, iy, 0, nullptr, sz, sz);
        // Recarga.
        if (m.kind() != K_ORBIT && act) {
          const float cd = clamp(m.t / std::fmax(0.05f, m.get(MS_CD) * p.cd_mul), 0.0f, 1.0f);
          if (cd > 0) C2D_DrawRectSolid(ix, iy, 0, 16 * sz, 16 * sz * cd, C2D_Color32(0, 0, 0, 120));
        }
      }
      const MoveInst& m0 = p.moves[p.slots[i][0]];
      std::snprintf(b, sizeof(b), "%d", i + 1);
      text(b, x + 2, y + 1, 0.3f, C2D_Color32(255, 255, 255, 170), 0);
      std::snprintf(b, sizeof(b), "%sNv%d", m0.evolved ? "★" : "", m0.lvl);
      text(b, x + S / 2, y + S - 12, 0.3f, rgb(m0.evolved ? 0xffd23f : 0xeaf2ff), 1);
    }
    if (p.nslots) {
      const MoveInst& m = p.moves[p.slots[p.active_slot][0]];
      text_fit(m.name(), 160, kSlotY + S + 3, 200, 0.36f, rgb(kTypeColor[m.type()]), true, 1);
    }
    // --- esferas: dos casillas a cada lado (A usa la elegida, B cambia; tocar la usa) ---
    if (orb_count(p) > 0) {
      const float cdk = clamp(p.orb_cd / kOrbCd, 0.0f, 1.0f);
      for (int i = 0; i < kOrbSlots; ++i) {
        float ox, oy;
        orb_slot_pos(i, ox, oy);
        const int id = p.orb_id[i];
        const bool sel = id >= 0 && i == orb_sel;
        const bool fresh = i == p.orb_new && p.orb_new_t > 0;
        C2D_DrawRectSolid(ox - 1, oy - 1, 0, kOrbS + 2, kOrbS + 2, sel ? rgb(0xffd23f) : fresh ? rgbf(kOrbs[id].col, 0.6f + std::sin(float(g.time) * 10) * 0.4f) : id >= 0 ? rgbf(kOrbs[id].col, 0.6f) : C2D_Color32(40, 52, 80, 120));
        C2D_DrawRectSolid(ox, oy, 0, kOrbS, kOrbS, C2D_Color32(20, 26, 44, 235));
        if (id < 0) continue;
        draw_orb(id, ox + kOrbS / 2, oy + kOrbS / 2, kOrbS * 0.32f);
        if (cdk > 0) C2D_DrawRectSolid(ox, oy, 0, kOrbS, kOrbS * cdk, C2D_Color32(0, 0, 0, 120));
        if (p.orb_n[i] > 1) { std::snprintf(b, sizeof(b), "×%d", p.orb_n[i]); text(b, ox + kOrbS - 1, oy + kOrbS - 10, 0.3f, rgb(0xeaf2ff), 2); }
        if (sel) text("A", ox + 2, oy, 0.3f, rgb(0xffd23f), 0);
      }
      // El nombre de la elegida (o de la que acabas de coger), bajo las casillas.
      const int show = p.orb_new >= 0 && p.orb_new_t > 0 && p.orb_id[p.orb_new] >= 0 ? p.orb_new : orb_sel;
      if (show >= 0 && show < kOrbSlots && p.orb_id[show] >= 0) {
        float ox, oy;
        orb_slot_pos(show, ox, oy);
        (void)oy;
        text_fit(kOrbs[p.orb_id[show]].name, show < 2 ? 4 : 316, 232, 64, 0.28f, rgb(kOrbs[p.orb_id[show]].col), true, show < 2 ? 0 : 2);
      }
    }
  }
  std::snprintf(b, sizeof(b), "%d FPS%s", fps, autotest ? " · AUTOTEST" : "");
  // Grabaciones del tráiler: sin la etiqueta y con la posición de tu Pokémon en
  // la pantalla de arriba codificada en un cuadradito (R = x/2, G = y).
  if (trailer_mark) {
    const Player& me = g.me();
    // (el mundo va al doble de la pantalla: a píxeles de la pantalla de arriba)
    const float sx = (me.x - g.camera.left()) * 0.5f, sy = (me.y - me.body_h * 0.5f - g.camera.top()) * 0.5f;
    C2D_DrawRectSolid(0, 236, 0, 4, 4, C2D_Color32(u8(clamp(sx / 2, 0.0f, 255.0f)), u8(clamp(sy, 0.0f, 255.0f)), 255, 255));
  }
  if ((show_fps || autotest) && !trailer_mark) text(b, 316, 214, 0.32f, fps >= 50 ? rgb(0x5fe08a) : fps >= 30 ? rgb(0xffd23f) : rgb(0xff5f6d), 2, false);
}

}  // namespace ps

namespace ps {

// Autotest "textos": mide con la letra de la consola todos los textos de
// movimientos, objetos y megas en los huecos donde se dibujan.
int Renderer::check_texts(FILE* log) {
  int bad = 0, n = 0;
  auto fits1 = [&](const char* s, float maxw) { return text_width(s, 0.28f) <= maxw; };
  auto fits2 = [&](const char* s, float maxw) {   // (como text_fit2, en el idioma elegido)
    std::string t = i18n_lang() != LANG_ES ? tr(s) : s;
    s = t.c_str();
    if (text_width(s, 0.32f) <= maxw) return true;
    size_t cut = 0;
    for (size_t i = 0; i < t.size(); ++i) {
      if (t[i] != ' ') continue;
      if (text_width(t.substr(0, i).c_str(), 0.28f) > maxw) break;
      cut = i;
    }
    return cut > 0 && text_width(t.substr(cut + 1).c_str(), 0.28f) <= maxw;
  };
  auto chk = [&](bool ok, const char* where, const char* s) {
    if ((++n & 31) == 0) C2D_TextBufClear(tbuf);
    if (ok) return;
    bad++;
    if (log) std::fprintf(log, "texto: no cabe (%s): %s\n", where, s);
  };
  char b[160];
  const float tagw = text_width("Psíquico · Especial", 0.34f);
  for (int i = 0; i < kMoveCount; ++i) {
    const MoveDef& d = kMoves[i];
    chk(fits1(d.name, 254 - tagw - 6), "carta", d.name);
    chk(fits2(d.desc, 256), "carta", d.desc);
    MoveInst m = move_instance(i);
    for (int l = 1; l < move_max_lvl(); ++l) {
      const char* u = move_next_up_text(m);
      if (u) chk(fits2(u, 256), "carta", u);
      move_level_up(m);
    }
    if (d.evo_name) {
      std::snprintf(b, sizeof(b), "%s → %s", d.name, d.evo_name);
      chk(fits1(b, 254 - text_width("EVOLUCIÓN", 0.34f) - 6), "carta", b);
      chk(fits2(kEvoMods[d.kind].text, 256), "carta", kEvoMods[d.kind].text);
      std::snprintf(b, sizeof(b), "%s Nv.5", d.evo_name);
      chk(fits1(b, 118), "pausa", b);
    }
    std::snprintf(b, sizeof(b), "%s Nv.5", d.name);
    chk(fits1(b, 118), "pausa", b);
    chk(fits1(d.name, 150), "atajos", d.name);
  }
  for (int i = 0; i < LS_COUNT; ++i) chk(fits2(kLevelStats[i].desc, 256), "carta", kLevelStats[i].desc);
  const float whow = text_width("El que llevas", 0.34f);
  for (int i = 0; i < IT_COUNT; ++i) {
    chk(fits1(item_name(i, 1), 250 - whow - 6), "objeto", item_name(i, 1));
    chk(fits2(item_desc(i, 1), 250), "objeto", item_desc(i, 1));
  }
  for (int k = 0; k < kMegaCount; ++k) {
    const int it = IT_MEGA_A + kMegas[k].slot;
    chk(fits1(item_name(it, kMegas[k].base), 250 - whow - 6), "objeto", item_name(it, kMegas[k].base));
    chk(fits2(item_desc(it, kMegas[k].base), 250), "objeto", item_desc(it, kMegas[k].base));
  }
  C2D_TextBufClear(tbuf);
  if (log) std::fprintf(log, "textos: %d medidos, %d no caben\n", n, bad);
  return bad;
}

}  // namespace ps

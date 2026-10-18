// render.cpp — la partida en las dos pantallas.
//   Arriba (400×240): el mundo a escala 0,5 (1 píxel de arte = 1 píxel), como
//   drawScene/render de game.js, más los avisos y la barra del jefe.
//   Abajo (320×240, táctil): vida, nivel, reloj, movimientos y las elecciones
//   (cartas de nivel, objeto, vínculo), pausa y resumen.
#include "platform/3ds/render.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/util.h"

namespace ps {

namespace {
constexpr float TOP_W = 400, TOP_H = 240, BOT_W = 320, BOT_H = 240;
const u32 BLACK = C2D_Color32(0, 0, 0, 255);

struct View {
  float l, t;
  float sx(float wx) const { return std::floor((wx - l) * 0.5f + 0.5f); }
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

// ---------------- carga ----------------

bool Renderer::init() {
  tbuf = C2D_TextBufNew(8192);
  // Fuente propia (Pixelify Sans, tools/gen_font.py); si falta, la del sistema.
  font = C2D_FontLoad("romfs:/gfx/font.bcfnt");
  if (font) C2D_FontSetFilter(font, GPU_NEAREST, GPU_NEAREST);
  font_k = font ? 1.2f : 1.0f;
  if (!props.load("romfs:/gfx/props.t3x", "romfs:/gfx/props.idx")) return false;
  if (!vfx.load("romfs:/gfx/vfx.t3x", "romfs:/gfx/vfx.idx")) return false;
  if (!icons.load("romfs:/gfx/icons.t3x", "romfs:/gfx/icons.idx")) return false;
  chunks.init();
  static const char* const PT[PR_COUNT] = {"tree", "rock", "chest", "grass", "altar", "spring", "bigchest", "trap"};
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
  for (int m = 0; m < kMoveCount && m < 64; ++m) { std::snprintf(name, sizeof(name), "move:%s", kMoves[m].id); move_icon[m] = icons.find(name); }
  static const char* const UI[16] = {"heart", "coin", "ticket", "ticket10", "berry", "magnet", "bomb", "shiny", "shield", "friends", "clock", "star", "bond", "cards", "up", "book"};
  for (int i = 0; i < 16; ++i) ui_icon[i] = icons.find(UI[i]);
  return true;
}

// Precarga los Pokémon de la partida (el tuyo, el elenco y los jefes).
void Renderer::preload_run(Game& g) {
  sprites.get(g.pl().dex, g.pl().shiny);
  for (int t = 0; t < g.ncast; ++t)
    for (int i = 0; i < g.cast[t].n; ++i) sprites.get(g.cast[t].list[i].dex, false);
  for (int b = 0; b < g.nbosses; ++b) sprites.get(g.bosses[b].dex, false);
  if (world_seed != uint32_t(g.world.seed())) { chunks.clear(); world_seed = uint32_t(g.world.seed()); }
}

void Renderer::begin_frame(Game& g) {
  if (world_seed != uint32_t(g.world.seed())) { chunks.clear(); world_seed = uint32_t(g.world.seed()); }
  // Pinta 3 filas de casillas por fotograma (un chunk en ~6 fotogramas); más si
  // lo que se ve está sin pintar.
  chunks.work(g.world, chunks.pending() > 2 ? 6 : 3);
}

// ---------------- utilidades ----------------

void Renderer::text(const char* s, float x, float y, float size, u32 color, int align, bool shadow) {
  C2D_Text t;
  C2D_TextFontParse(&t, font, tbuf, s);
  C2D_TextOptimize(&t);
  float w = 0, h = 0;
  size *= font_k;
  C2D_TextGetDimensions(&t, size, size, &w, &h);
  if (align == 1) x -= w / 2;
  else if (align == 2) x -= w;
  if (shadow) {
    C2D_DrawText(&t, C2D_WithColor, x + 1, y + 1, 0, size, size, C2D_Color32(0, 0, 0, (color >> 24) * 3 / 4));
    C2D_DrawText(&t, C2D_WithColor, x - 1, y, 0, size, size, C2D_Color32(0, 0, 0, (color >> 24) / 2));
  }
  C2D_DrawText(&t, C2D_WithColor, x, y, 0, size, size, color);
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

// ---------------- pantalla de arriba ----------------

namespace {
enum DKind : int8_t { D_PROP, D_ENEMY, D_PLAYER, D_RIFT, D_ARCEUS, D_LOST, D_GOAL };
struct DItem { float y; int8_t kind; const void* p; };
DItem items[900];
}  // namespace

void Renderer::draw_top(Game& g) {
  C2D_TextBufClear(tbuf);
  const Camera& cam = g.camera;
  View v{cam.left() - cam.sx, cam.top() - cam.sy};
  const float now = float(g.time);
  World& w = g.world;
  const int biome = w.biome();
  const float fk = w.fade_k();

  // --- suelo ---
  const int32_t c0x = int32_t(std::floor(v.l / kCW)), c1x = int32_t(std::floor((v.l + cam.w) / kCW));
  const int32_t c0y = int32_t(std::floor(v.t / kCW)), c1y = int32_t(std::floor((v.t + cam.h) / kCW));
  for (int32_t cy = c0y; cy <= c1y; ++cy)
    for (int32_t cx = c0x; cx <= c1x; ++cx) {
      const float x = v.sx(cx * kCW), y = v.sy(cy * kCW);
      C2D_Image* cur = chunks.get(w, cx, cy, biome);
      if (fk >= 0) {
        C2D_Image* old = chunks.get(w, cx, cy, w.fade_from());
        if (old) C2D_DrawImageAt(*old, x, y, 0);
        if (cur) { C2D_ImageTint t; C2D_AlphaImageTint(&t, fk); C2D_DrawImageAt(*cur, x, y, 0, &t); }
      } else if (cur) {
        C2D_DrawImageAt(*cur, x, y, 0);
      } else {
        C2D_DrawRectSolid(x, y, 0, kChunkPx, kChunkPx, rgb(kBiomes[biome].floor[0]));
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

  // --- campos en el suelo ---
  for (const Field& f : g.fields) {
    if (!cam.sees(f.x, f.y, 60)) continue;
    const float x = v.sx(f.x), y = v.sy(f.y), r = f.r * 0.5f;
    if (f.kind == FK_PATCH) {
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
    static const uint32_t COL[PK_COUNT] = {0x7fe0ff, 0xffd95e, 0x5fe08a, 0xff9ed8, 0xff7b3d, 0xffd23f, 0xffd23f, 0xc47bff, 0xffd23f};
    static const uint32_t GLOW[PK_COUNT] = {0x2a8fd0, 0xc08a10, 0x1f8f4d, 0xb03d84, 0xb03d10, 0xb8860b, 0xc08a10, 0x8a3fd0, 0xc08a10};
    static const float R[PK_COUNT] = {4.5f, 7, 6.5f, 6.5f, 6.5f, 5.5f, 7, 8, 9};
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
        case PK_ITEM: {
          ellipse(x, y, 9, 9, rgbf(0xffe14d, 0.35f + std::sin(p.t * 1.5f) * 0.15f));
          ic = icons.find(kItems[int(p.value)].id);
          break;
        }
        default: break;
      }
      if (ic >= 0) {
        const float sz = (p.kind == PK_TICKET10 || p.kind == PK_ITEM ? 24 : 18) * 0.5f;
        const float k = sz / icons.sub(ic).width;
        C2D_DrawImageAt(icons.image(ic), std::floor(x - sz / 2), std::floor(y - sz / 2), 0, nullptr, k, k);
      }
    }
  }

  // --- lo que tiene "pies", ordenado por Y ---
  int n = 0;
  const float pad = 70;
  w.for_props_in_rect(v.l - pad, v.t - pad, v.l + cam.w + pad, v.t + cam.h + pad, [&](Prop& p) {
    if (n < 600 && cam.sees(p.x, p.y, 70)) items[n++] = {p.flat ? p.y - 4000 : p.y, D_PROP, &p};
  });
  for (const Enemy& e : g.corpses) if (n < 880 && cam.sees(e.x, e.y, 80)) items[n++] = {e.y, D_ENEMY, &e};
  for (const Enemy& e : g.enemies) if (n < 880 && cam.sees(e.x, e.y, 80)) items[n++] = {e.y, D_ENEMY, &e};
  items[n++] = {g.pl().y, D_PLAYER, &g.pl()};
  if (g.rift.open) items[n++] = {g.rift.y, D_RIFT, nullptr};
  if (g.judg.a_on && g.judg.ported) items[n++] = {g.judg.ay, D_ARCEUS, nullptr};
  if (g.lost.on) { items[n++] = {g.lost.ty - 4000, D_GOAL, nullptr}; items[n++] = {g.lost.y, D_LOST, nullptr}; }
  std::stable_sort(items, items + n, [](const DItem& a, const DItem& b) { return a.y < b.y; });

  for (int i = 0; i < n; ++i) {
    const DItem& it = items[i];
    switch (it.kind) {
      case D_PROP: {
        const Prop& p = *static_cast<const Prop*>(it.p);
        int variant = 0;
        switch (p.type) {
          case PR_GRASS: variant = std::sin(now * 2.4f + p.wiggle) > 0.55f ? 1 : 0; break;
          case PR_ROCK: variant = p.hp < p.max_hp * 0.5f ? 1 : 0; break;
          case PR_ALTAR: variant = p.kind * 2 + (p.used ? 1 : 0); break;
          case PR_SPRING: variant = p.charge < 0.15f ? 1 : 0; break;
          case PR_TRAP: variant = p.kind; break;
          default: break;
        }
        const int idx = prop_idx[biome][p.type][variant];
        if (idx < 0) break;
        const Tex3DS_SubTexture& s = props.sub(idx);
        const float x = v.sx(p.x), y = v.sy(p.y);
        if (p.flat) { C2D_DrawImageAt(props.image(idx), std::floor(x - s.width / 2.0f), std::floor(y - 4 - s.height / 2.0f), 0); break; }
        if (p.type == PR_ALTAR && !p.used) {
          static const uint32_t AC[4] = {0xff5f6d, 0x5fd4ff, 0xff8ad8, 0xffd23f};
          ellipse(x, y - s.height + 11, 6.5f, 6.5f, rgbf(AC[p.kind & 3], 0.22f + std::sin(now * 3 + p.wiggle) * 0.1f));
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
        if (p.flash > 0) { C2D_ImageTint t; C2D_PlainImageTint(&t, C2D_Color32f(1, 1, 1, 0.6f), 1); C2D_DrawImageAt(props.image(idx), px, py, 0, &t); }
        break;
      }
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
        const float sr = (shadow_size(p.dex) == 0 ? 5 : shadow_size(p.dex) == 1 ? 8 : 12);
        ellipse(x, y, sr, sr * 0.42f, C2D_Color32(0, 0, 0, 87));
        if (p.power.def >= 0 && !p.dead)
          dot_ring(x, y, r * 2.1f, r * 0.85f, 2, rgbf(kPowers[p.power.def].color, 0.35f + std::sin(now * 8.3f) * 0.15f));
        const float alpha = p.invuln > 0 && !p.dead ? (int(std::floor(p.invuln * 18)) % 2 ? 0.45f : 1) : 1;
        draw_sprite(p.anim, x, y, p.flash > 0 ? 2 : 0, alpha);
        static const uint32_t BC[BS_COUNT] = {0xff6b4d, 0x7fe0ff, 0xb0b8c8, 0x5fe08a};
        int bi = 0;
        for (int b = 0; b < BS_COUNT; ++b) {
          if (p.buffs[b].n <= 0) continue;
          const float rr = r * 1.8f + bi * 2.5f;
          dot_ring(x, y, rr, rr * 0.42f, 1, rgbf(BC[b], 0.6f), 0, kTau * (p.buffs[b].n / 5.0f));
          bi++;
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
        if (J.lift < 40) ellipse(x, y, J.ar * 1.1f, J.ar * 0.4f, rgbf(0xffd25e, 0.22f + std::sin(now * 3) * 0.06f));
        // La rueda dorada detrás de él.
        const float wy = v.sy(J.ay - J.lift - J.abody_h * 0.55f), R = J.abody_h * 0.31f;
        dot_ring(x, wy, R, R, 2, rgbf(0xffd25e, 0.55f));
        static const uint32_t GEMS[6] = {0xff6b4d, 0x4fa8ff, 0x5fe08a, 0xffe14d, 0xb37bff, 0xc9d2de};
        for (int k = 0; k < 12; ++k) {
          const float a = J.spin * 0.6f + k * kTau / 12;
          ellipse(x + std::cos(a) * R * 0.91f, wy + std::sin(a) * R * 0.91f, 2, 2, rgbf(GEMS[k % 6], 0.7f));
        }
        draw_sprite(J.aanim, x, v.sy(J.ay - J.lift), 0, 1);
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

  // --- el pilar de luz de Arceus ---
  if (g.judg.a_on && g.judg.ported && g.judg.phase == 1) {
    const auto& J = g.judg;
    const float a = clamp((J.ct - 5.1f) / 0.6f, 0.0f, 1.0f) * clamp((7.9f + 0.6f - J.ct) / 0.6f, 0.0f, 1.0f);
    if (a > 0) {
      const float near = clamp(1 - J.lift / 900, 0.0f, 1.0f);
      const float hw = (40 + near * 40 + std::sin(J.ct * 9) * 6) * 0.5f;
      const float x = v.sx(J.ax), bot = v.sy(J.ay + 6);
      const u32 edge = C2D_Color32(255, 210, 94, 0), mid = C2D_Color32(255, 248, 220, uint8_t(178 * a));
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
    }
    char b[96];
    float info_y = boss ? 16 : 6;
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
      std::snprintf(b, sizeof(b), "SENTENCIA · %s · JEFES %d", t, g.judg.downs);
      text(b, 200, info_y, 0.4f, rgb(0xffd25e), 1);
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
      const float ca = std::cos(a);
      text(label, ex - ca * 14, ey - std::sin(a) * 12 - 5, 0.34f, rgb(col), ca > 0.3f ? 2 : ca < -0.3f ? 0 : 1);
    };
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
    if (g.judg.phase == 2) {
      std::snprintf(b, sizeof(b), "ARCEUS · %d m", int(dist(pl.x, pl.y, g.judg.ax, g.judg.ay) / 32));
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
      const float a = std::fmin(1.0f, g.banner_t * 1.6f);
      text(g.banner, 200, 74, 0.52f, rgbf(0xffcb3d, a), 1);
    }
  }

  // --- la Sentencia ---
  if (g.judg.phase == 1) {
    const auto& J = g.judg;
    const float t = J.ct;
    if (t < 5.2f) C2D_DrawRectSolid(0, 0, 0, TOP_W, TOP_H, C2D_Color32(10, 6, 2, uint8_t(clamp(t / 1.1f, 0.0f, 1.0f) * 0.86f * 255)));
    const float a = clamp((t - 1.3f + 0.3f) / 0.5f, 0.0f, 1.0f) * clamp((4.7f + 0.2f - t) / 0.35f, 0.0f, 1.0f);
    if (a > 0) {
      dot_ring(200, 80, 56, 56, 2, rgbf(0xffd25e, a * 0.9f));
      dot_ring(200, 80, 46, 46, 1, rgbf(0xffd25e, a * 0.9f));
      Animator big = J.aanim;
      big.cur = A_IDLE; big.frame = 0; big.dir = 0; big.scale = 2;
      draw_sprite(big, 200, 120, 1, a * 0.85f);
      text("LA SENTENCIA", 200, 148, 0.9f, rgbf(0xffd25e, a), 1);
      text("DE ARCEUS", 200, 178, 0.7f, rgbf(0xffd25e, a), 1);
      if (t > 2.9f) text("El tiempo se ha agotado…", 200, 204, 0.4f, rgbf(0xf3e2b0, a * clamp((t - 2.9f) / 0.6f, 0.0f, 1.0f)), 1);
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
  const Player& p = g.pl();
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
  std::snprintf(b, sizeof(b), "%s · %d derrotados · %d monedas", g.world.biome_name(), p.kills, p.coins);
  text(b, 160, 46, 0.36f, rgb(0x8fa3c4), 1);
  // Buffs y poder de altar.
  {
    static const char* const LBL[BS_COUNT] = {"ATK", "VEL", "DEF", "REG"};
    static const uint32_t COL[BS_COUNT] = {0xff6b4d, 0x7fe0ff, 0xb0b8c8, 0x5fe08a};
    float bx = 238;
    for (int k = 0; k < BS_COUNT; ++k) {
      if (p.buffs[k].n <= 0) continue;
      std::snprintf(b, sizeof(b), "%s×%d", LBL[k], p.buffs[k].n);
      text(b, bx, 28, 0.34f, rgb(COL[k]), 0);
      bx += 26;
    }
    if (p.power.def >= 0) {
      std::snprintf(b, sizeof(b), "%s %ds", kPowers[p.power.def].name, int(std::ceil(p.power.t)));
      text(b, 160, 60, 0.36f, rgb(kPowers[p.power.def].color), 1);
    }
    if (g.weather.kind >= 0 && weather_level(g) > 0) {
      std::snprintf(b, sizeof(b), "%s · %ds", kWeather[g.weather.kind].name, int(std::ceil(g.weather.dur - g.weather.t)));
      text(b, 8, 60, 0.34f, rgb(kWeather[g.weather.kind].color), 0);
    }
  }

  // --- elecciones ---
  if (g.state == ST_LEVELUP) {
    std::snprintf(b, sizeof(b), "¡Nivel %d! Elige una mejora", p.level);
    text(b, 160, 74, 0.5f, rgb(0xffcb3d), 1);
    for (int i = 0; i < g.ncards; ++i) {
      const Card& c = g.cards[i];
      const float y = 92 + i * 44;
      const bool sel = i == card_cursor;
      C2D_DrawRectSolid(10, y, 0, 300, 40, sel ? C2D_Color32(52, 70, 110, 255) : C2D_Color32(30, 40, 62, 255));
      if (sel) { C2D_DrawRectSolid(10, y, 0, 3, 40, rgb(0xffcb3d)); }
      const char* title = "";
      const char* desc = "";
      char tag[64] = "";
      int ic = -1;
      uint32_t col = 0x8fa3c4;
      if (c.type == CT_NEW) { title = kMoves[c.move].name; desc = kMoves[c.move].desc; std::snprintf(tag, sizeof(tag), "%s", kTypeName[kMoves[c.move].type]); ic = move_icon[c.move]; col = kTypeColor[kMoves[c.move].type]; }
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
        else std::snprintf(tag, sizeof(tag), "Mejora");
      }
      if (ic >= 0) C2D_DrawImageAt(icons.image(ic), 16, y + 6, 0, nullptr, 28.0f / icons.sub(ic).width, 28.0f / icons.sub(ic).width);
      text(title, 50, y + 3, 0.45f, C2D_Color32(240, 244, 255, 255), 0);
      text(tag, 304, y + 4, 0.34f, rgb(col), 2);
      text(desc ? desc : "", 50, y + 21, 0.32f, rgb(0xb7c4dc), 0, false);
    }
    const int left = 1 + p.reroll_bonus - p.rerolls;
    std::snprintf(b, sizeof(b), "A: elegir · Y: cambiar cartas (%d)", left > 0 ? left : 0);
    text(b, 160, 226, 0.34f, rgb(0x8fa3c4), 1);
  } else if (g.state == ST_CHOICE && g.choice == CH_ITEM) {
    text("Sólo puedes llevar un objeto", 160, 74, 0.48f, rgb(0xffcb3d), 1);
    for (int i = 0; i < 2; ++i) {
      const int it = i == 0 ? g.item_cur : g.item_next;
      const float y = 96 + i * 54;
      const bool sel = i == card_cursor;
      C2D_DrawRectSolid(10, y, 0, 300, 48, sel ? C2D_Color32(52, 70, 110, 255) : C2D_Color32(30, 40, 62, 255));
      const int ic = icons.find(kItems[it].id);
      if (ic >= 0) C2D_DrawImageAt(icons.image(ic), 16, y + 8, 0, nullptr, 2.5f, 2.5f);
      text(kItems[it].name, 54, y + 4, 0.46f, C2D_Color32(240, 244, 255, 255), 0);
      text(i == 0 ? "El que llevas" : "El nuevo", 304, y + 4, 0.34f, rgb(0x8fa3c4), 2);
      text(kItems[it].desc, 54, y + 24, 0.32f, rgb(0xb7c4dc), 0, false);
    }
    text("A: quedarte con el elegido", 160, 226, 0.34f, rgb(0x8fa3c4), 1);
  } else if (g.state == ST_CHOICE && g.choice == CH_BOND) {
    int8_t loose[4]; int nl, grow;
    const int mode = player_bond_options(p, loose, nl, grow);
    text(mode == 1 ? "¡Vínculo! Elige qué movimiento se une" : "¡Vínculo! Elige dos movimientos para unir", 160, 74, 0.44f, rgb(0xff8ad8), 1);
    for (int i = 0; i < nl; ++i) {
      const MoveInst& m = p.moves[loose[i]];
      const float y = 92 + i * 32;
      const bool sel = i == card_cursor;
      C2D_DrawRectSolid(10, y, 0, 300, 28, sel ? C2D_Color32(80, 52, 90, 255) : C2D_Color32(30, 40, 62, 255));
      if (move_icon[m.def] >= 0) C2D_DrawImageAt(icons.image(move_icon[m.def]), 16, y + 4, 0, nullptr, 1.25f, 1.25f);
      std::snprintf(b, sizeof(b), "%s  Nv.%d", m.name(), m.lvl);
      text(b, 44, y + 6, 0.42f, C2D_Color32(240, 244, 255, 255), 0);
    }
    text("A: elegir", 160, 226, 0.34f, rgb(0x8fa3c4), 1);
  } else if (g.state == ST_PAUSED) {
    text("PAUSA", 160, 90, 0.9f, rgb(0xffcb3d), 1);
    text("START: seguir · SELECT: abandonar la partida", 160, 130, 0.4f, rgb(0xeaf2ff), 1);
  } else if (g.state == ST_OVER) {
    const RunSummary& s = g.summary;
    text(s.dead ? "Te has debilitado" : "Fin de la partida", 160, 74, 0.6f, rgb(0xffcb3d), 1);
    mmss(s.time, t, sizeof(t));
    std::snprintf(b, sizeof(b), "Tiempo %s · Nivel %d · %d derrotados · %d jefes", t, s.level, s.kills, s.bosses);
    text(b, 160, 100, 0.38f, rgb(0xeaf2ff), 1);
    std::snprintf(b, sizeof(b), "+%d Pokémonedas · %d tickets · %d ×10", s.coins, s.t1, s.t10);
    text(b, 160, 118, 0.4f, rgb(0xffd23f), 1);
    std::snprintf(b, sizeof(b), "tiempo %d · derrotados %d · jefes %d · recogidas %d", s.parts[0], s.parts[1], s.parts[2], s.parts[3]);
    text(b, 160, 136, 0.32f, rgb(0x8fa3c4), 1);
    if (s.judge_time >= 0) {
      mmss(s.judge_time, t, sizeof(t));
      std::snprintf(b, sizeof(b), "Ante Arceus: %s · %d legendarios", t, s.judge_bosses);
      text(b, 160, 152, 0.36f, rgb(0xffd25e), 1);
    }
    text("A: otra partida", 160, 200, 0.45f, rgb(0xeaf2ff), 1);
  } else {
    // --- huecos de movimientos (tocar para cambiar) ---
    const float S = 66, GAP = 8;
    const int n = std::max(1, p.nslots);
    const float x0 = (BOT_W - (n * S + (n - 1) * GAP)) / 2;
    for (int i = 0; i < p.nslots; ++i) {
      const float x = x0 + i * (S + GAP), y = 150;
      const bool act = i == p.active_slot;
      const bool bond = p.slot_n[i] > 1;
      C2D_DrawRectSolid(x, y, 0, S, S, act ? C2D_Color32(52, 70, 110, 255) : C2D_Color32(28, 36, 56, 255));
      if (act) { C2D_DrawRectSolid(x, y, 0, S, 2, rgb(bond ? 0xff8ad8 : 0xffcb3d)); C2D_DrawRectSolid(x, y + S - 2, 0, S, 2, rgb(bond ? 0xff8ad8 : 0xffcb3d)); }
      for (int j = 0; j < p.slot_n[i]; ++j) {
        const MoveInst& m = p.moves[p.slots[i][j]];
        const int ic = move_icon[m.def];
        const float sz = p.slot_n[i] > 1 ? 1.4f : 2.5f;
        const float ix = p.slot_n[i] > 1 ? x + 6 + (j % 2) * 28 : x + 13, iy = p.slot_n[i] > 1 ? y + 6 + (j / 2) * 28 : y + 6;
        if (ic >= 0) C2D_DrawImageAt(icons.image(ic), ix, iy, 0, nullptr, sz, sz);
        // Recarga.
        if (m.kind() != K_ORBIT && act) {
          const float cd = clamp(m.t / std::fmax(0.05f, m.get(MS_CD) * p.cd_mul), 0.0f, 1.0f);
          if (cd > 0) C2D_DrawRectSolid(ix, iy, 0, 16 * sz, 16 * sz * cd, C2D_Color32(0, 0, 0, 120));
        }
      }
      const MoveInst& m0 = p.moves[p.slots[i][0]];
      std::snprintf(b, sizeof(b), "%d", i + 1);
      text(b, x + 3, y + 1, 0.34f, C2D_Color32(255, 255, 255, 180), 0);
      std::snprintf(b, sizeof(b), "%sNv%d", m0.evolved ? "★" : "", m0.lvl);
      text(b, x + S / 2, y + S - 14, 0.32f, rgb(m0.evolved ? 0xffd23f : 0xeaf2ff), 1);
    }
    if (p.nslots) {
      const MoveInst& m = p.moves[p.slots[p.active_slot][0]];
      text(m.name(), 160, 128, 0.44f, rgb(kTypeColor[m.type()]), 1);
    }
    if (p.item >= 0) {
      const int ic = icons.find(kItems[p.item].id);
      if (ic >= 0) C2D_DrawImageAt(icons.image(ic), 8, 104, 0, nullptr, 1.5f, 1.5f);
      text(kItems[p.item].name, 30, 106, 0.34f, rgb(0xeaf2ff), 0);
    }
    if (p.bonds > 0) { std::snprintf(b, sizeof(b), "Vínculos: %d", p.bonds); text(b, 312, 106, 0.34f, rgb(0xff8ad8), 2); }
    text("L/R: cambiar · START: pausa", 160, 226, 0.32f, rgb(0x5d6f8c), 1);
  }
  std::snprintf(b, sizeof(b), "%d FPS%s", fps, autotest ? " · AUTOTEST" : "");
  text(b, 316, 214, 0.32f, fps >= 50 ? rgb(0x5fe08a) : fps >= 30 ? rgb(0xffd23f) : rgb(0xff5f6d), 2, false);
}

}  // namespace ps

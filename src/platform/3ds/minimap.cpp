// minimap.cpp — ver minimap.h.
#include "platform/3ds/minimap.h"

#include <algorithm>
#include <cmath>

#include "core/world.h"
#include "platform/3ds/gfx.h"
#include "platform/3ds/render.h"

namespace ps {

namespace {
inline u32 morton(u32 x, u32 y) {
  static const u8 tbl[8] = {0, 1, 4, 5, 16, 17, 20, 21};
  return tbl[x & 7] | (tbl[y & 7] << 1);
}
inline u16 to565(uint32_t c) {
  const u32 r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
  return u16(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}
// Mezcla dos colores 0xRRGGBB (k = cuánto de b).
uint32_t mix(uint32_t a, uint32_t b, float k) {
  auto ch = [&](int s) { return uint32_t(std::lround(((a >> s) & 255) * (1 - k) + ((b >> s) & 255) * k)) << s; };
  return ch(16) | ch(8) | ch(0);
}
const uint32_t kMate[4] = {0xffcb3d, 0x5fc8ff, 0x7ee07a, 0xff8ad8};
const u32 kBlack = C2D_Color32(0, 0, 0, 255);

// Cuadrado con borde negro (se ve sobre cualquier terreno).
void mark(float x, float y, float s, u32 c) {
  C2D_DrawRectSolid(std::floor(x - s / 2) - 1, std::floor(y - s / 2) - 1, 0, s + 2, s + 2, kBlack);
  C2D_DrawRectSolid(std::floor(x - s / 2), std::floor(y - s / 2), 0, s, s, c);
}
void dot(float x, float y, float r, u32 c) {
  C2D_DrawCircleSolid(x, y, 0, r + 1, kBlack);
  C2D_DrawCircleSolid(x, y, 0, r, c);
}
// Flecha (triángulo) en (x, y) hacia `a`, de largo `len`.
void arrow(float x, float y, float a, float len, u32 c, bool outline = true) {
  const float ca = std::cos(a), sa = std::sin(a);
  auto tri = [&](float L, float W, u32 col) {
    const float tx = x + ca * L * 0.6f, ty = y + sa * L * 0.6f;
    const float bx = x - ca * L * 0.4f, by = y - sa * L * 0.4f;
    C2D_DrawTriangle(tx, ty, col, bx - sa * W, by + ca * W, col, bx + sa * W, by - ca * W, col, 0);
  };
  if (outline) tri(len + 3, len * 0.5f + 1.6f, kBlack);
  tri(len, len * 0.5f, c);
}
}  // namespace

void Minimap::init() {
  for (auto& t : tex_) {
    C3D_TexInit(&t, kTexW, kTexH, GPU_RGB565);
    C3D_TexSetFilter(&t, GPU_NEAREST, GPU_NEAREST);
  }
}

void Minimap::work(Game& g, int rows) {
  const int back = front_ ^ 1;
  World& w = g.world;
  const Player& p = g.me();
  if (row_ < 0) {   // otra vuelta, centrada en ti
    ox_[back] = World::tile_of(p.x) - kTexW / 2;
    oy_[back] = World::tile_of(p.y) - kTexH / 2;
    biome_[back] = w.biome();
    row_ = 0;
  }
  const Biome& B = kBiomes[biome_[back] >= 0 && biome_[back] < kBiomeCount ? biome_[back] : 0];
  // Suelo apagado (para que se vean los iconos), paredes oscuras con algo de
  // su color y el agua (o lava) con el suyo.
  const u16 cf = to565(mix(B.floor[0], 0x101828, 0.45f));
  const u16 cw = to565(mix(B.wallTop[0], 0x000000, 0.55f));
  const u16 cl = to565(mix(B.liq, 0x000000, 0.15f));
  u16* dst = static_cast<u16*>(tex_[back].data);
  for (int k = 0; k < rows && row_ < kTexH; ++k, ++row_) {
    const int y = row_, ty = kTexH - 1 - y;   // (la GPU guarda las filas de abajo arriba)
    for (int x = 0; x < kTexW; ++x) {
      const uint8_t t = w.terrain_at(ox_[back] + x, oy_[back] + y);
      dst[((ty >> 3) * (kTexW >> 3) + (x >> 3)) * 64 + morton(u32(x), u32(ty))] = t == TR_WALL ? cw : t == TR_LIQUID ? cl : cf;
    }
  }
  if (row_ >= kTexH) {
    C3D_TexFlush(&tex_[back]);
    ready_[back] = true;
    front_ = back;
    row_ = -1;
  }
}

void Minimap::draw(Game& g, Renderer& r, float x, float y, float w, float h, float now) {
  (void)r;
  const Player& me = g.me();
  const float U = kTS / kPxPerTile;   // mundo por píxel
  // Centrado en tu vista (a píxel entero: el mapa avanza sin temblar).
  const float cx = std::floor(g.camera.x / U) * U, cy = std::floor(g.camera.y / U) * U;
  const float left = cx - w / 2 * U, top = cy - h / 2 * U;
  C2D_DrawRectSolid(x - 1, y - 1, 0, w + 2, h + 2, C2D_Color32(70, 84, 112, 255));
  C2D_DrawRectSolid(x, y, 0, w, h, C2D_Color32(8, 12, 22, 255));

  // --- terreno ---
  if (ready_[front_]) {
    const int f = front_;
    const float tx0 = left / kTS - ox_[f], ty0 = top / kTS - oy_[f];
    const float tw = w / kPxPerTile, th = h / kPxPerTile;
    const float ax0 = std::max(0.0f, tx0), ax1 = std::min(float(kTexW), tx0 + tw);
    const float ay0 = std::max(0.0f, ty0), ay1 = std::min(float(kTexH), ty0 + th);
    if (ax1 > ax0 && ay1 > ay0) {
      sub_.width = u16(std::lround((ax1 - ax0) * kPxPerTile));
      sub_.height = u16(std::lround((ay1 - ay0) * kPxPerTile));
      sub_.left = ax0 / kTexW; sub_.right = ax1 / kTexW;
      sub_.top = 1.0f - ay0 / kTexH; sub_.bottom = 1.0f - ay1 / kTexH;
      const C2D_Image img{&tex_[f], &sub_};
      C2D_DrawImageAt(img, std::floor(x + (ax0 - tx0) * kPxPerTile), std::floor(y + (ay0 - ty0) * kPxPerTile), 0);
    }
  }

  auto to = [&](float wx, float wy, float& sx, float& sy) {
    sx = x + (wx - left) / U; sy = y + (wy - top) / U;
    return sx >= x + 2 && sx <= x + w - 2 && sy >= y + 2 && sy <= y + h - 2;
  };
  // Fuera del mapa: flecha en el borde, hacia allí.
  auto edge = [&](float wx, float wy, u32 c) {
    float sx, sy;
    if (to(wx, wy, sx, sy)) return;
    const float mx = x + w / 2, my = y + h / 2;
    const float a = std::atan2(sy - my, sx - mx);
    const float ex = clamp(sx, x + 6, x + w - 6), ey = clamp(sy, y + 6, y + h - 6);
    arrow(ex, ey, a, 7, c);
  };
  float sx, sy;
  const float pulse = 0.5f + 0.5f * std::sin(now * 6);

  // --- lo que ves arriba ---
  {
    const float vw = g.camera.w / U, vh = g.camera.h / U;
    const float vx = std::floor(x + (g.camera.x - left) / U - vw / 2), vy = std::floor(y + (g.camera.y - top) / U - vh / 2);
    const u32 c = C2D_Color32(255, 255, 255, 110);
    C2D_DrawRectSolid(vx, vy, 0, vw, 1, c);
    C2D_DrawRectSolid(vx, vy + vh - 1, 0, vw, 1, c);
    C2D_DrawRectSolid(vx, vy, 0, 1, vh, c);
    C2D_DrawRectSolid(vx + vw - 1, vy, 0, 1, vh, c);
  }

  // --- escenario: cofres, altares, manantiales ---
  g.world.for_props_in_rect(left, top, left + w * U, top + h * U, [&](Prop& pr) {
    if (pr.gone || !to(pr.x, pr.y, sx, sy)) return;
    if (pr.type == PR_CHEST) mark(sx, sy, 3, rgb(0xf2c443));
    else if (pr.type == PR_BIGCHEST) mark(sx, sy, 5, rgb(pr.open > 0 ? 0xfff3b0 : 0xffd23f));
    else if (pr.type == PR_ALTAR && !pr.used) {
      C2D_DrawTriangle(sx, sy - 4, kBlack, sx - 4, sy + 1, kBlack, sx + 4, sy + 1, kBlack, 0);
      C2D_DrawTriangle(sx, sy + 5, kBlack, sx - 4, sy, kBlack, sx + 4, sy, kBlack, 0);
      const u32 c = rgb(0xff8ad8);
      C2D_DrawTriangle(sx, sy - 3, c, sx - 3, sy + 1, c, sx + 3, sy + 1, c, 0);
      C2D_DrawTriangle(sx, sy + 4, c, sx - 3, sy, c, sx + 3, sy, c, 0);
    } else if (pr.type == PR_SPRING) mark(sx, sy, 3, rgb(pr.charge >= 1 ? 0x5fe8d8 : 0x2f6b66));
  });

  // --- objetos especiales del suelo ---
  for (const Pickup& q : g.pickups) {
    if ((q.kind == PK_ITEM || q.kind == PK_TICKET || q.kind == PK_TICKET10 || q.kind == PK_HEAL) && to(q.x, q.y, sx, sy))
      mark(sx, sy, 2, rgb(q.kind == PK_HEAL ? 0x5fe08a : q.kind == PK_ITEM ? 0xffffff : 0xd9a6ff));
  }

  // --- enemigos: rojos; shiny, celestes; jefes, grandes y parpadeando ---
  for (const Enemy& e : g.enemies) {
    if (e.dead || e.boss) continue;
    if (!to(e.x, e.y, sx, sy)) continue;
    if (e.shiny) mark(sx, sy, 3, rgb(0x9ae6ff));
    else C2D_DrawRectSolid(std::floor(sx - 1), std::floor(sy - 1), 0, 2, 2, rgb(0xff5f6d));
  }
  for (const Enemy& e : g.enemies) {
    if (e.dead || !e.boss) continue;
    const uint32_t c = e.judge ? 0xffd25e : 0xffcb3d;
    if (to(e.x, e.y, sx, sy)) {
      C2D_DrawCircleSolid(sx, sy, 0, 5 + pulse * 2, rgbf(c, 0.35f));
      mark(sx, sy, 5, rgb(c));
    } else edge(e.x, e.y, rgb(c));
  }

  // --- grieta, Pokémon perdido y su casa, Arceus ---
  if (g.rift.open) {
    if (to(g.rift.x, g.rift.y, sx, sy)) { C2D_DrawCircleSolid(sx, sy, 0, 5 + pulse * 2, rgbf(0xb48aff, 0.5f)); dot(sx, sy, 3, rgb(0xb48aff)); }
    else edge(g.rift.x, g.rift.y, rgb(0xb48aff));
  }
  if (g.lost.on) {
    if (to(g.lost.tx, g.lost.ty, sx, sy)) {   // casa
      const u32 c = rgb(0x5fe08a);
      C2D_DrawTriangle(sx, sy - 6, kBlack, sx - 6, sy, kBlack, sx + 6, sy, kBlack, 0);
      C2D_DrawRectSolid(sx - 4, sy - 1, 0, 8, 6, kBlack);
      C2D_DrawTriangle(sx, sy - 5, c, sx - 5, sy, c, sx + 5, sy, c, 0);
      C2D_DrawRectSolid(sx - 3, sy, 0, 6, 4, c);
    } else edge(g.lost.tx, g.lost.ty, rgb(0x5fe08a));
    if (to(g.lost.x, g.lost.y, sx, sy)) dot(sx, sy, 2.5f, rgb(0x7dffb0));
    else edge(g.lost.x, g.lost.y, rgb(0x7dffb0));
  }
  if (g.judg.a_on) {
    if (to(g.judg.ax, g.judg.ay, sx, sy)) { C2D_DrawCircleSolid(sx, sy, 0, 6 + pulse * 2, rgbf(0xffd25e, 0.4f)); dot(sx, sy, 4, rgb(0xffd25e)); }
    else edge(g.judg.ax, g.judg.ay, rgb(0xffd25e));
  }

  // --- compañeros y tú ---
  for (int i = 0; i < g.nplayers; ++i) {
    const Player& o = g.players[i];
    if (!o.active || i == g.local) continue;
    const uint32_t c = o.dead && std::fmod(now, 0.6f) < 0.3f ? 0xff5f6d : kMate[i & 3];
    if (to(o.x, o.y, sx, sy)) dot(sx, sy, 3, rgb(c));
    else edge(o.x, o.y, rgb(c));
  }
  if (std::hypot(me.vx_, me.vy_) > 20) ang_ = std::atan2(me.vy_, me.vx_);
  if (to(me.x, me.y, sx, sy)) arrow(sx, sy, ang_, 9, me.dead ? rgb(0xff5f6d) : C2D_Color32(255, 255, 255, 255));
}

void Minimap::legend(Renderer& r, float x, float y, float w, float h) {
  C2D_DrawRectSolid(x, y, 0, w, h, C2D_Color32(8, 12, 22, 225));
  struct Row { int kind; uint32_t col; const char* label; };
  static const Row ROWS[] = {
    {0, 0xffffff, "Tú"}, {1, 0x5fc8ff, "Compañero"}, {2, 0xff5f6d, "Enemigo"}, {3, 0x9ae6ff, "Shiny"},
    {4, 0xffcb3d, "Jefe"}, {5, 0xf2c443, "Cofre"}, {6, 0xff8ad8, "Altar"}, {3, 0x5fe8d8, "Manantial"},
    {7, 0xb48aff, "Grieta"}, {8, 0x5fe08a, "Casa del perdido"}, {7, 0xffd25e, "Arceus"}, {9, 0xffffff, "Lo que ves arriba"},
  };
  const int n = int(sizeof(ROWS) / sizeof(ROWS[0])), per = (n + 2) / 3;
  for (int i = 0; i < n; ++i) {
    const float cx = x + 12 + (i / per) * (w / 3), cy = y + 14 + (i % per) * (h - 16) / per;
    const u32 c = rgb(ROWS[i].col);
    switch (ROWS[i].kind) {
      case 0: arrow(cx, cy, 0, 9, c); break;
      case 1: dot(cx, cy, 3, c); break;
      case 2: C2D_DrawRectSolid(cx - 1, cy - 1, 0, 2, 2, c); break;
      case 3: mark(cx, cy, 3, c); break;
      case 4: C2D_DrawCircleSolid(cx, cy, 0, 6, rgbf(ROWS[i].col, 0.35f)); mark(cx, cy, 5, c); break;
      case 5: mark(cx, cy, 3, c); break;
      case 6: C2D_DrawTriangle(cx, cy - 3, c, cx - 3, cy + 1, c, cx + 3, cy + 1, c, 0); C2D_DrawTriangle(cx, cy + 4, c, cx - 3, cy, c, cx + 3, cy, c, 0); break;
      case 7: C2D_DrawCircleSolid(cx, cy, 0, 6, rgbf(ROWS[i].col, 0.45f)); dot(cx, cy, 3, c); break;
      case 8: C2D_DrawTriangle(cx, cy - 5, c, cx - 5, cy, c, cx + 5, cy, c, 0); C2D_DrawRectSolid(cx - 3, cy, 0, 6, 4, c); break;
      case 9: { const u32 v = C2D_Color32(255, 255, 255, 140); C2D_DrawRectSolid(cx - 6, cy - 4, 0, 12, 1, v); C2D_DrawRectSolid(cx - 6, cy + 3, 0, 12, 1, v); C2D_DrawRectSolid(cx - 6, cy - 4, 0, 1, 8, v); C2D_DrawRectSolid(cx + 5, cy - 4, 0, 1, 8, v); break; }
    }
    r.text(ROWS[i].label, cx + 10, cy - 5, 0.32f, C2D_Color32(234, 242, 255, 255), 0, false);
  }
}

}  // namespace ps

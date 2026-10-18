// render.h — dibuja la partida en las dos pantallas (game.js render + hud.js).
#pragma once
#include <3ds.h>
#include <citro2d.h>

#include "core/game.h"
#include "platform/3ds/gfx.h"

namespace ps {

struct Renderer {
  Atlas props, vfx, icons;
  SpriteBank sprites;
  ChunkCache chunks;
  C2D_TextBuf tbuf = nullptr;
  C2D_Font font = nullptr;
  int fps = 60;
  int card_cursor = 0;
  uint32_t world_seed = 0;
  // Índices de imágenes de los atlas (buscados al cargar).
  int prop_idx[32][PR_COUNT][8];
  int rift_idx[4];
  int vfx_idx[SH_COUNT][PAL_COUNT][3];
  int type_icon[T_COUNT];
  int move_icon[64];
  int ui_icon[16];

  bool init();
  void preload_run(Game& g);
  void begin_frame(Game& g);        // reparte el pintado del suelo
  void draw_top(Game& g);
  void draw_bottom(Game& g, bool autotest);

  // utilidades
  void text(const char* s, float x, float y, float size, u32 color, int align = 0, bool shadow = true);
  void draw_sprite(const Animator& a, float sx, float sy, u32 flash_mode, float alpha);
  void draw_shape(int shape, int pal, int frame, float sx, float sy, float angle, float scale, float alpha);
};

}  // namespace ps

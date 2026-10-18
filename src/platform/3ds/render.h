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
  float font_k = 1;              // escala de la fuente propia frente a la del sistema
  int fps = 60;
  bool show_fps = false;
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
  // eye: 0 sin 3D; con 3D, -k para el ojo izquierdo y +k para el derecho.
  void draw_top(Game& g, float eye = 0);
  void set_parallax(float p);
  void draw_bottom(Game& g, bool autotest);
  void draw_ground(World& w, const Camera& cam, float vl, float vt, float now);
  void draw_prop(const Prop& p, World& w, float vl, float vt, float now);

  // utilidades
  void text(const char* s, float x, float y, float size, u32 color, int align = 0, bool shadow = true);
  float text_wrap(const char* s, float x, float y, float size, u32 color, float width, int align = 0);
  float text_width(const char* s, float size);
  void draw_sprite(const Animator& a, float sx, float sy, u32 flash_mode, float alpha);
  void draw_shape(int shape, int pal, int frame, float sx, float sy, float angle, float scale, float alpha);
};

}  // namespace ps

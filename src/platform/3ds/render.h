// render.h — dibuja la partida en las dos pantallas (game.js render + hud.js).
#pragma once
#include <3ds.h>
#include <citro2d.h>

#include "core/game.h"
#include "platform/3ds/gfx.h"
#include "platform/3ds/minimap.h"

namespace ps {

struct Renderer {
  Atlas props, vfx, icons;
  SpriteBank sprites;
  ChunkCache chunks;
  Minimap minimap;
  // Pantalla de abajo en la partida: minimapa y huecos de ataques (la entrada táctil usa lo mismo).
  float legend_t = 0;           // segundos que queda la leyenda del minimapa
  static constexpr float kMapH = 126, kSlotY = 174, kSlotS = 44, kSlotGap = 6;
  C2D_TextBuf tbuf = nullptr;
  C2D_Font font = nullptr;        // Pixelify (celda de 20 px): textos grandes, a 1× o 2×
  C2D_Font font_small = nullptr;  // Tiny5 (celda de 10 px): textos pequeños, a 1×
  float font_k = 1;              // escala de la fuente propia frente a la del sistema
  // Fuente, escala y ajuste vertical para un tamaño (`size` como en la fuente
  // del sistema). Las fuentes pixel sólo se ven nítidas a escala entera.
  struct FontPick { C2D_Font f; float k; float dy; };
  FontPick pick_font(float size) const;
  int fps = 60;
  int props_drawn = 0;   // objetos del escenario dibujados en el último fotograma (autotest)
  bool show_fps = false;
  bool trailer_mark = false;   // autotest "… marca": para grabar el tráiler (ver draw_bottom)
  // Depuración (rejilla.txt en la SD): casillas de colisión (rojo pared, azul
  // líquido) y los pies y el círculo de choque de los jugadores, encima de todo.
  bool debug_grid = false;
  bool old_3ds = false;          // pinta el suelo más despacio (menos filas por fotograma)
  int card_cursor = 0;
  int bond_first = -1;   // vínculo: el primer movimiento ya elegido (índice en moves), -1 ninguno
  uint32_t world_seed = 0, terrain_gen = 0;
  // Índices de imágenes de los atlas (buscados al cargar).
  int prop_idx[32][PR_COUNT][8];
  int rift_idx[4];
  int vfx_idx[SH_COUNT][PAL_COUNT][3];
  int type_icon[T_COUNT];
  int move_icon[kMaxMoves];
  static constexpr int kMegaMax = 160;   // (más que megas hay: kMegaCount)
  int16_t stone_icon[kMegaMax];   // icono de cada megapiedra (-1: sin dibujo)
  int ui_icon[16];

  bool init();
  void preload_run(Game& g);
  void begin_frame(Game& g);        // reparte el pintado del suelo
  // eye: 0 sin 3D; con 3D, -k para el ojo izquierdo y +k para el derecho.
  void draw_top(Game& g, float eye = 0);
  void set_parallax(float p);
  void draw_bottom(Game& g, bool autotest);
  // Fin de la partida, arriba: los ataques del jugador `pi` (daño y por segundo),
  // el total y su objeto; en grupo, con una pestaña por jugador (L/R).
  void draw_over_summary(Game& g, int pi);
  void draw_ground(World& w, const Camera& cam, float vl, float vt, float now);
  void draw_prop(const Prop& p, World& w, float vl, float vt, float now);

  // utilidades
  void text(const char* s, float x, float y, float size, u32 color, int align = 0, bool shadow = true);
  float text_wrap(const char* s, float x, float y, float size, u32 color, float width, int align = 0);
  float text_width(const char* s, float size);
  // Texto que cabe en maxw: baja la letra hasta la pequeña y, si ni así, lo
  // corta con «…». `align` como en text(). Devuelve el ancho dibujado.
  float text_fit(const char* s, float x, float y, float maxw, float size, u32 color, bool shadow = false, int align = 0);
  // Igual, pero si no cabe en una línea lo parte en dos (letra pequeña, la
  // segunda a y + 10). Devuelve cuántas líneas ha usado.
  int text_fit2(const char* s, float x, float y, float maxw, float size, u32 color);
  // Autotest "textos": cuántos textos no caben (y los apunta en `log`).
  int check_texts(FILE* log);
  // Megapiedra de la mega `k` (kMegas), con su dibujo ("stone:k" en el atlas de
  // iconos); sin dibujo o con k < 0, una genérica con el símbolo mega. Lado `s`.
  void draw_mega_stone(int k, float x, float y, float s);
  // Icono de un objeto (los de la web o la megapiedra de `dex`), de lado `s`.
  void draw_item_icon(int id, float x, float y, float s, int dex = -1);
  void draw_sprite(const Animator& a, float sx, float sy, u32 flash_mode, float alpha);
  void draw_shape(int shape, int pal, int frame, float sx, float sy, float angle, float scale, float alpha);
};

}  // namespace ps

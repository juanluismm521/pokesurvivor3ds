// gfx.h — texturas de la 3DS: atlas de arte, banco de sprites de Pokémon y
// caché de chunks del suelo (pintados con el mismo código que la web).
#pragma once
#include <3ds.h>
#include <citro2d.h>

#include <cstdint>

#include "core/gamedata.h"
#include "core/tiles_paint.h"

namespace ps {

class World;

// Color de la web (0xRRGGBB) + alfa → color de citro2d.
inline u32 rgb(uint32_t c, uint8_t a = 255) { return C2D_Color32((c >> 16) & 255, (c >> 8) & 255, c & 255, a); }
inline u32 rgbf(uint32_t c, float a) { return rgb(c, uint8_t(a <= 0 ? 0 : a >= 1 ? 255 : a * 255)); }

// ---------------- atlas (props, vfx, iconos) ----------------
class Atlas {
 public:
  bool load(const char* t3x, const char* idx);
  void free();
  // Imagen por nombre (o null). Se busca una vez al cargar: guarda el índice.
  int find(const char* name) const;
  C2D_Image image(int i) { return C2D_Image{&tex_, &subs_[i]}; }
  const Tex3DS_SubTexture& sub(int i) const { return subs_[i]; }
  int count() const { return n_; }

 private:
  C3D_Tex tex_{};
  Tex3DS_Texture t3x_ = nullptr;
  Tex3DS_SubTexture* subs_ = nullptr;
  char (*names_)[32] = nullptr;
  int n_ = 0;
};

// ---------------- sprites de Pokémon ----------------
struct SpriteFrame { uint16_t img; uint8_t flip; int16_t ox, oy; };
struct SpriteSet {
  int16_t dex = -1;
  bool shiny = false;
  uint32_t stamp = 0;
  int npages = 0;
  C3D_Tex tex[2];
  Tex3DS_Texture t3x[2] = {nullptr, nullptr};
  int nimages = 0;
  Tex3DS_SubTexture* subs = nullptr;      // 2 por imagen: normal y en espejo
  uint8_t* page_of = nullptr;
  uint8_t an[A_COUNT] = {}, ar[A_COUNT] = {};
  SpriteFrame* frames[A_COUNT] = {};
  uint32_t bytes = 0;
};

class SpriteBank {
 public:
  static constexpr int kSlots = 96;
  // Devuelve el juego de sprites (lo carga si hace falta; null si falla).
  SpriteSet* get(int dex, bool shiny);
  // Imagen de un fotograma y su desplazamiento dentro del fotograma.
  bool frame(SpriteSet* s, int anim, int row, int f, C2D_Image& img, int& ox, int& oy, int& w, int& h);
  void touch_all_free();
  uint32_t bytes() const;
  int loaded() const;
  uint32_t loads = 0;

 private:
  bool load(SpriteSet& s, int dex, bool shiny);
  void unload(SpriteSet& s);
  SpriteSet slots_[kSlots];
  uint32_t stamp_ = 0;
};

// ---------------- chunks del suelo ----------------
class ChunkCache {
 public:
  static constexpr int kSlots = 16;
  void init();
  void clear();          // al cambiar de mundo o de bioma
  // Textura del chunk si ya está pintado (si no, lo pide y devuelve null).
  C2D_Image* get(World& w, int32_t cx, int32_t cy, int biome);
  // Pinta un poco (hasta `rows` filas de casillas) del chunk pendiente.
  void work(World& w, int rows);
  int pending() const;

 private:
  struct Slot {
    int32_t cx, cy; int biome; uint32_t gen; bool used, ready, painting; int row; uint32_t stamp;
    C3D_Tex tex; Tex3DS_SubTexture sub; C2D_Image img;
  };
  Slot slots_[kSlots];
  uint32_t stamp_ = 0;
  uint32_t* buf_ = nullptr;     // RGBA8 de un chunk (256×256)
  ChunkPainter painter_;
  int painting_ = -1;
};

// Copia una imagen RGBA8 lineal (de arriba abajo) a una textura de la GPU
// (formato en mosaico de 8×8, RGB565).
void upload_rgb565(C3D_Tex& tex, const uint32_t* rgba, int w, int h);

}  // namespace ps

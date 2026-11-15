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

// ---- piezas de la interfaz: rectángulos con las esquinas recortadas, degradados, marcos ----
inline uint32_t mix(uint32_t a, uint32_t b, float t) {
  t = t < 0 ? 0 : t > 1 ? 1 : t;
  auto ch = [&](int s) { const float x = float((a >> s) & 255), y = float((b >> s) & 255); return uint32_t(x + (y - x) * t) << s; };
  return ch(16) | ch(8) | ch(0);
}
inline void rrect(float x, float y, float w, float h, u32 c) {
  C2D_DrawRectSolid(x + 1, y, 0, w - 2, h, c);
  C2D_DrawRectSolid(x, y + 1, 0, 1, h - 2, c);
  C2D_DrawRectSolid(x + w - 1, y + 1, 0, 1, h - 2, c);
}
inline void rgrad(float x, float y, float w, float h, u32 top, u32 bot) {
  C2D_DrawRectangle(x + 1, y, 0, w - 2, h, top, top, bot, bot);
  C2D_DrawRectangle(x, y + 1, 0, 1, h - 2, top, top, bot, bot);
  C2D_DrawRectangle(x + w - 1, y + 1, 0, 1, h - 2, top, top, bot, bot);
}
inline void rframe(float x, float y, float w, float h, u32 c) {
  C2D_DrawRectSolid(x + 1, y, 0, w - 2, 1, c);
  C2D_DrawRectSolid(x + 1, y + h - 1, 0, w - 2, 1, c);
  C2D_DrawRectSolid(x, y + 1, 0, 1, h - 2, c);
  C2D_DrawRectSolid(x + w - 1, y + 1, 0, 1, h - 2, c);
}
inline void corners(float x, float y, float w, float h, u32 c) {
  const float L = 5;
  C2D_DrawRectSolid(x, y, 0, L, 1, c); C2D_DrawRectSolid(x, y, 0, 1, L, c);
  C2D_DrawRectSolid(x + w - L, y, 0, L, 1, c); C2D_DrawRectSolid(x + w - 1, y, 0, 1, L, c);
  C2D_DrawRectSolid(x, y + h - 1, 0, L, 1, c); C2D_DrawRectSolid(x, y + h - L, 0, 1, L, c);
  C2D_DrawRectSolid(x + w - L, y + h - 1, 0, L, 1, c); C2D_DrawRectSolid(x + w - 1, y + h - L, 0, 1, L, c);
}

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
  // Hilo que lee de la SD los sprites que se piden durante la partida.
  void init();
  // Devuelve el juego de sprites. Si no está y `async` está activo, lo pide al
  // hilo y devuelve null hasta que llega (unos fotogramas: el Pokémon no se ve
  // mientras); sin `async` (precarga) lo carga en el momento.
  SpriteSet* get(int dex, bool shiny);
  // ¿Se está leyendo en el hilo? (para no dibujar el respaldo mientras)
  bool loading(int dex, bool shiny) const;
  // Crea las texturas de lo que ya ha leído el hilo (una vez por fotograma).
  void poll();
  // Imagen de un fotograma y su desplazamiento dentro del fotograma.
  bool frame(SpriteSet* s, int anim, int row, int f, C2D_Image& img, int& ox, int& oy, int& w, int& h);
  void touch_all_free();
  uint32_t bytes() const;
  int loaded() const;
  uint32_t loads = 0;
  bool async = false;
  // Tiempos de carga en segundo plano (para el autotest): cuántas, lo que
  // tarda el hilo en leerlas y lo que cuesta al hilo principal crear la textura.
  uint32_t late_loads = 0, late_max_us = 0, late_total_us = 0, read_max_us = 0;

 private:
  // Archivos leídos: el .fr y cada página ya descomprimida (lista para la GPU).
  struct Files { uint8_t* buf[3] = {}; uint32_t len[3] = {}; uint16_t w[3] = {}, h[3] = {}; uint8_t fmt[3] = {}; int n = 0; };
  enum { RQ_FREE, RQ_QUEUED, RQ_READY, RQ_FAILED };
  struct Req { int16_t dex = -1; bool shiny = false; int state = RQ_FREE; Files f; };
  static constexpr int kReqs = 6;
  static bool fetch(int dex, bool shiny, Files& f);
  static void drop(Files& f);
  bool build(SpriteSet& s, int dex, bool shiny, const Files& f);
  SpriteSet* victim();
  void unload(SpriteSet& s);
  static void worker(void* arg);
  SpriteSet slots_[kSlots];
  uint32_t stamp_ = 0;
  Req reqs_[kReqs];
  mutable LightLock lock_;
  LightEvent ev_;
  Thread thr_ = nullptr;
};

// ---------------- chunks del suelo ----------------
class ChunkCache {
 public:
  // (32: con el anillo de alrededor y el bioma viejo durante el cambio de
  // bioma se piden hasta 26 a la vez; con 24 se echaban unos a otros)
  static constexpr int kSlots = 32;
  // El pintado va en un hilo en otro núcleo (el 2 en la New 3DS, el 1 en la
  // vieja); si no se puede crear, se pinta a trozos en el hilo principal.
  void init();
  void shutdown();
  void clear();          // al cambiar de mundo o de bioma
  // Textura del chunk si ya está pintado (si no, lo pide y devuelve null).
  C2D_Image* get(World& w, int32_t cx, int32_t cy, int biome);
  // Pinta un poco (hasta `rows` filas de casillas) del chunk pendiente.
  void work(World& w, int rows);
  int pending() const;
  // Depuración: el terreno con el que se pintó el chunk (si está listo).
  const uint8_t* painted_terrain(int32_t cx, int32_t cy, int biome) const;
  bool debug_marks = false;   // marca arriba (magenta), izquierda (amarillo) y la esquina (blanco)

 private:
  struct Slot {
    int32_t cx, cy; int biome; uint32_t gen; bool used, ready, painting; int row; uint32_t stamp;
    uint8_t terr[16 * 16];   // el terreno con el que se pintó (depuración: rejilla.txt)
    C3D_Tex tex; Tex3DS_SubTexture sub; C2D_Image img;
  };
  static void worker(void* arg);
  Slot slots_[kSlots];
  uint32_t stamp_ = 0;
  uint32_t* buf_ = nullptr;     // RGBA8 de un chunk (256×256)
  ChunkPainter painter_;
  int painting_ = -1;
  // hilo
  Thread thr_ = nullptr;
  LightEvent ev_;
  volatile int job_ = -1;       // hueco que pinta el hilo
  volatile bool job_done_ = false, quit_ = false;
  uint32_t job_gen_ = 0;
public:
  bool threaded() const { return thr_ != nullptr; }
};

// Copia una imagen RGBA8 lineal (de arriba abajo) a una textura de la GPU
// (formato en mosaico de 8×8, RGB565).
void upload_rgb565(C3D_Tex& tex, const uint32_t* rgba, int w, int h);

}  // namespace ps

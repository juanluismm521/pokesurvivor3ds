// tiles_paint.h — pixel art del suelo, pintado en código (paintChunk de
// tiles.js). Se pinta a resolución de arte (16 px por casilla) y sale idéntico
// al de la web (mismos hashes y umbrales). La plataforma lo sube como textura.
#pragma once
#include <cstdint>

namespace ps {

class World;

constexpr int kChunkPx = 16 * 16;   // 256×256 px de arte por chunk

// Pinta el chunk (cx, cy) con la paleta del bioma `bi` en `out` (RGBA8,
// kChunkPx × kChunkPx, fila a fila). Se puede pintar por tiras de casillas
// (row0..row1, en casillas 0..16) para repartir el trabajo entre fotogramas;
// la máscara de tierra se calcula en la primera tira (row0 == 0).
struct ChunkPainter {
  void begin(World& w, int32_t cx, int32_t cy, int biome);
  // Pinta las filas de casillas [row0, row1). Devuelve true al terminar.
  bool paint_rows(int row0, int row1, uint32_t* out);
  void paint_all(uint32_t* out) { paint_rows(0, 16, out); }
  void paint_origin(int row0, int row1, uint32_t* out);   // la Sala del Origen (la Sentencia)

  World* w = nullptr;
  int32_t cx = 0, cy = 0;
  int biome = 0;
  uint8_t terr[18 * 18];            // terreno con un borde de 1 casilla (autotiling)
  uint8_t dirt[256 * 257];          // máscara de tierra (una fila extra arriba)
};

// Color empaquetado como lo hace la web (ABGR, little-endian = bytes R,G,B,A).
inline uint32_t pack_rgb(uint32_t rgb, uint32_t a = 255) {
  return (a << 24) | ((rgb & 0xff) << 16) | (rgb & 0xff00) | ((rgb >> 16) & 0xff);
}

}  // namespace ps

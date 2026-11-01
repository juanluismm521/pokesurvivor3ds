// delta.h — actualizaciones por diferencias (01-11-2026): en vez de bajar el
// .cia/.3dsx entero (≈40 MB, casi todo sprites y música que no cambian), la
// consola baja un parche contra la versión que ya tiene en la SD y rehace la
// nueva. La nueva se comprueba después con su huella FIRMADA (core/update.h),
// así que un parche malo o una base distinta sólo hacen que se baje entera.
//
// Formato «PSD1»: 4 bytes "PSD1", tamaño de la base y de la nueva (u64, little
// endian) y luego, comprimido con zlib (deflate), una lista de órdenes:
//   0, desplazamiento (varint), largo (varint)   copiar ese trozo de la base
//   1, largo (varint), bytes                      añadir esos bytes
//   2                                             fin
// Las órdenes van en el orden de la nueva (se escribe del tirón, sin volver
// atrás); la base se lee a saltos. Aplicarlo usa poca memoria (la 3DS vieja).
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ps {

// Crea el parche de `base` a `neu` (en el VPS: los dos archivos en memoria).
// `block`: el trozo mínimo que se busca en la base.
std::vector<uint8_t> delta_make(const uint8_t* base, size_t nbase, const uint8_t* neu, size_t nneu, int block = 32);

// Aplicarlo leyendo y escribiendo por trozos. Las funciones devuelven false si
// algo falla (y entonces se para). `ud` es para quien las llama.
struct DeltaIo {
  void* ud = nullptr;
  // Lee hasta n bytes del parche; devuelve cuántos (0 = se acabó).
  size_t (*read_patch)(void* ud, uint8_t* buf, size_t n) = nullptr;
  // Lee exactamente n bytes de la base en ese desplazamiento.
  bool (*read_base)(void* ud, uint64_t off, uint8_t* buf, size_t n) = nullptr;
  // Escribe n bytes de la nueva (siempre seguidos).
  bool (*write_out)(void* ud, const uint8_t* buf, size_t n) = nullptr;
  // Progreso (puede ser null): lo escrito y el total.
  void (*progress)(void* ud, uint64_t done, uint64_t total) = nullptr;
};
enum DeltaResult { DELTA_OK, DELTA_BAD, DELTA_BASE_SIZE, DELTA_IO };
// `base_size`: el tamaño de la base que se tiene (si no es el del parche, no vale).
DeltaResult delta_apply(const DeltaIo& io, uint64_t base_size);

// Comodidad (pruebas y herramientas): todo en memoria.
bool delta_apply_mem(const std::vector<uint8_t>& patch, const uint8_t* base, size_t nbase, std::vector<uint8_t>& out);

}  // namespace ps

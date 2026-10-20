// update.h — el juego se actualiza solo (el programa entero, no sólo los datos).
//
// El VPS publica cada versión (tools/publicar_programa.sh) en
// .../datos/programa/ultimo.json con la versión actual y la anterior. Cada una
// va FIRMADA (Ed25519 de Monocypher) con una clave que sólo tiene el VPS; la
// consola lleva la pública (kUpdateKey) y no acepta nada sin firma buena, ni
// un archivo cuya huella (BLAKE2b) no cuadre.
//
// «serie» es el número de publicación (sube siempre, también al volver atrás
// en el servidor); «build» es el número de la build (commits de main). La
// consola instala una publicación si es de otra build que la suya y es más
// nueva que la última que ya instaló o decidió no instalar (así, volver atrás
// —en la consola o en el servidor— no se deshace solo).
#pragma once
#include <cstdint>
#include <string>

namespace ps {

extern const uint8_t kUpdateKey[32];   // clave pública de las publicaciones

struct UpdateFile {
  std::string url;          // relativa a .../datos/programa/
  uint64_t size = 0;
  uint8_t hash[32] = {};    // BLAKE2b-256 del archivo
};

struct UpdateEntry {
  bool ok = false;
  int serie = 0, build = 0;
  std::string version, commit, fecha, notas;
  UpdateFile f3dsx, cia;
};

// Lo que se firma de cada publicación (texto fijo, igual en el VPS).
std::string update_message(const UpdateEntry& e);
// Lee ultimo.json y comprueba las firmas de las dos entradas con `key`.
// Devuelve false (y why) si la actual no vale; la anterior puede faltar.
bool update_parse(const std::string& json, const uint8_t key[32], UpdateEntry& actual, UpdateEntry& anterior, std::string& why);
// ¿Hay que instalar `e`? (otra build y más nueva que la última decidida)
bool update_wanted(const UpdateEntry& e, int my_build, int decided_serie);

// Huella de un archivo por trozos (para comprobarlo mientras se descarga).
struct UpdateHasher {
  UpdateHasher();
  void add(const void* data, size_t n);
  void done(uint8_t out[32]);
  alignas(8) uint8_t ctx[256];
};

}  // namespace ps

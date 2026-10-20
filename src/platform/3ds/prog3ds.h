// prog3ds.h — el juego se actualiza solo (core/update tiene las reglas).
//
//   - Con «Comprobar actualizaciones automáticamente», al encender busca una
//     versión nueva en el servidor del proyecto y, si la hay, la descarga en
//     segundo plano a la SD comprobando firma y huella.
//   - Se instala AL SALIR del juego (con el juego abierto no se puede: lee sus
//     recursos del propio archivo). .3dsx: se cambia el archivo; .cia: se
//     instala como hace FBI. Si algo falla, se queda la versión que había.
//   - Volver atrás: la versión anterior se guarda en la SD (anterior.3dsx /
//     anterior.cia) y, si no está, se baja la anterior del servidor. Volver
//     atrás no se deshace solo: hasta la siguiente publicación no se vuelve a
//     actualizar.
#pragma once
#include <string>

#include "core/update.h"

namespace ps {
namespace prog3ds {

enum State { ST_IDLE, ST_CHECKING, ST_UPTODATE, ST_DOWNLOADING, ST_READY, ST_FAILED };

// dir: carpeta del juego en la SD; argv0: de dónde se ha cargado (.3dsx).
void init(const char* dir, const char* argv0);
bool supported();            // se sabe qué archivo cambiar (.3dsx) o es el .cia
bool is_cia();
void start_check(bool download);   // en segundo plano
int state();
int pct();                   // avance de la descarga
std::string status();        // para la pantalla y el log
const UpdateEntry& latest(); // la publicada (si se ha podido leer)
bool can_rollback();
bool start_rollback();       // prepara la anterior (puede descargar)
bool pending();              // hay algo que instalar al salir
std::string pending_text();
// Al salir: instala lo pendiente. show(texto, %) pinta la pantalla mientras.
// Devuelve el resultado (para el log).
std::string apply(void (*show)(const char* msg, int pct));
void shutdown();

}  // namespace prog3ds
}  // namespace ps

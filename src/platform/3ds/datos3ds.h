// datos3ds.h — los datos del juego (números de equilibrio, nombres, textos) van SIEMPRE
// con cada versión del port: desde la 0.26.2 (31-10-2026, decisión del dueño) ya no se
// bajan aparte de la web. Así todos los que tienen la misma versión tienen los mismos
// datos (si no, en grupo no se ven: la huella de los datos va en cada paquete del
// lockstep) y cuando hay datos nuevos en la web, el dueño decide si van en la siguiente.
// Al arrancar, si en la SD quedaba un paquete de antes (datos.json), se borra.
#pragma once
#include <string>

namespace ps {
namespace datos3ds {

// Quita el paquete de datos que pudiera quedar en la SD. Devuelve el texto para el log.
std::string init(const char* dir);
// Dirección del servidor del proyecto (…/datos/, o la de datos_url.txt en pruebas):
// de ahí salen las versiones del programa (prog3ds).
const std::string& base();

}  // namespace datos3ds
}  // namespace ps

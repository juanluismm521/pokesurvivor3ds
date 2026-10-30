// hw3ds.h — ¿es Halloween? (core/halloween.h) en la consola. Como la web, con la
// hora del servidor (la cabecera Date de cualquier respuesta: el juego busca
// actualizaciones al encender); sin conexión, con la fecha de la consola.
#pragma once
#include <cstdint>

namespace ps {
namespace hw3ds {

bool on();                                     // ¿es Halloween ahora (hora de España)?
bool countdown(bool& on_now, int64_t& ms);     // cuenta atrás del menú (core/halloween.h)
void force(int v);                             // pruebas (autotest): 1 sí, 0 no, -1 lo de verdad
bool forced();

}  // namespace hw3ds
}  // namespace ps

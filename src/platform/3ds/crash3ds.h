// crash3ds.h — si el juego se rompe en la consola, dónde y por qué.
//   - Cada hilo instala un manejador de excepciones (guard): si falla, escribe
//     fallo.log en la SD (build, hilo, tipo, registros y las direcciones de
//     código que hay en su pila, para sacar la línea con addr2line y el .elf
//     de esa build) y cierra el juego.
//   - step(): las etapas del arranque, en arranque.log (si se rompe antes de
//     llegar al menú, la última línea dice dónde).
//   - Al arrancar, si hay un fallo.log de la vez anterior, last() lo da (para
//     enseñarlo en pantalla) y pasa a fallo_anterior.log.
#pragma once

namespace crash3ds {

void init(const char* dir);       // (lo primero de main)
void guard(const char* name);     // al empezar cada hilo
void step(const char* what);      // etapa del arranque
void boot_done();                 // ya en el menú: deja de apuntar etapas
const char* last();               // resumen del fallo de la vez anterior ("" si no hubo)

}  // namespace crash3ds

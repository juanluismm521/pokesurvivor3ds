// datos3ds.h — la consola se actualiza sola los datos del juego (core/datapack):
// números de equilibrio, nombres, textos y traducciones de la web.
//
//   - Al arrancar, SIN red: si en la SD hay un paquete (datos.json) que vale
//     para este programa, se usa. Sin conexión, se juega con el último bajado
//     (o con los datos de fábrica).
//   - Con wifi, en segundo plano: baja el índice y el paquete de su versión del
//     servidor del proyecto (los publica tools/web/publicar_datos.sh) y, si es nuevo y vale,
//     lo deja listo. Se aplica en cuanto no hay una partida en marcha.
//   - Si la web ya pide un programa nuevo (otro esquema), lo dice: los datos
//     siguen llegando mientras la forma sea la misma.
#pragma once
#include <string>

namespace ps {
namespace datos3ds {

// Lee y aplica el paquete de la SD (sin red). Devuelve el texto para el log.
std::string init(const char* dir);
// Comprobación en segundo plano (una a la vez).
void start_check();
bool busy();
// Hay un paquete nuevo bajado y comprobado: aplicarlo (fuera de la partida).
bool pending();
bool apply_pending(std::string& msg);
// Lo último que ha pasado (para Ajustes y el autotest).
std::string status();
// La web tiene cambios que piden un programa nuevo.
bool newer_program();
void shutdown();

}  // namespace datos3ds
}  // namespace ps

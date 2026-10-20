// datapack.h — paquete de datos: lo que la consola puede actualizar sola, sin
// cambiar de programa (números de equilibrio, nombres, textos y traducciones).
//
// Lo genera tools/gen_data.js (PS_PAQUETE) desde la web y lo publica
// tools/web/publicar_datos.sh; la 3DS lo baja (platform/3ds/datos3ds) y lo
// guarda en la SD, así que sin conexión usa el último que bajó (o los de
// fábrica). Sólo se acepta si:
//   - su esquema es el de este programa (kDataSchema): mismos movimientos,
//     familias, Pokémon, mejoras, misiones y logros. Si la web cambia eso,
//     hace falta un programa nuevo;
//   - su huella cuadra con los datos (no está cortado ni estropeado);
//   - todos los números son razonables (finitos y dentro de su rango).
// Se aplica entero o nada, y nunca en plena partida (lo decide quien llama).
#pragma once
#include <cstdint>
#include <string>

namespace ps {

struct DataPackInfo {
  bool on = false;          // hay un paquete aplicado (si no, los de fábrica)
  std::string web, fecha;   // commit y fecha de la web de la que salen
  uint32_t huella = 0;
};

// Comprueba el paquete sin aplicarlo. false + motivo si no vale.
bool datapack_check(const std::string& text, std::string& why, DataPackInfo* info = nullptr);
// Lo aplica (si vale). Cambia kDataHash. false + motivo si no vale (y no toca nada).
bool datapack_apply(const std::string& text, std::string& why);
// El que está aplicado ahora.
const DataPackInfo& datapack_current();
// Vuelve a los datos de fábrica (pruebas).
void datapack_reset();

}  // namespace ps

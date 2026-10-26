// version.h — versión del port y de los datos de la web de los que parte.
#pragma once

namespace ps {

constexpr const char* kGameTitle = "PokéSurvivor 3DS";
constexpr const char* kVersion = "0.13.3";   // (se sube en cada commit con cambios: ver CHANGELOG.md y las normas del proyecto)
// El commit de la web del que salen los datos (kWebBase, kWebBaseDate) lo
// escribe gen_data desde reference/web/UPSTREAM (ver gamedata.h).

}  // namespace ps

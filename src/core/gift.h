// gift.h — regalos firmados (del port, 02-11-2026): para devolver a un jugador lo que
// perdió por un fallo del juego (la victoria del modo rápido de la 0.35.0, que se
// cerraba). El VPS publica .../datos/regalos.json con regalos FIRMADOS con la misma
// clave que las actualizaciones (core/update.h): nadie más puede crearlos. La consola
// los baja al buscar versiones nuevas y da los que son para ella (por su nombre o el de
// su cuenta de pokesurvivor.com) una sola vez: el id queda en la partida ("regalos") y
// viaja con ella por la nube, así que tampoco se repite en otra consola.
#pragma once
#include <cstdint>
#include <string>

namespace ps {

struct Save;

struct Gift {
  std::string id, para, nota, nota_en;   // para: nombre del jugador o de su cuenta («*»: para todos)
  int coins = 0, t1 = 0, t10 = 0, candy = 0;
  int medal = 0;                     // Medalla de Arceus para ese Pokémon (0 ninguna)
  int shiny[8] = {};                 // shinies que se dan (con el normal si no lo tiene)
  int nshiny = 0;
};

constexpr int kMaxGiftsFile = 32;
// Lo que se firma (igual en tools/regalo.py).
std::string gift_message(const Gift& g);
// Lee regalos.json: los regalos con firma buena (los malos se saltan). Devuelve cuántos.
int gift_parse(const std::string& json, const uint8_t key[32], Gift* out, int max);
// ¿Es para esta partida? (nombre del jugador o de la cuenta, sin mayúsculas; «*»: para todos)
bool gift_for(const Gift& g, const char* save_name, const char* account_name);
bool gift_received(const Save& s, const std::string& id);
// Lo da (si no se dio ya). true si lo ha dado.
bool gift_apply(Save& s, const Gift& g, int64_t now_ms);

}  // namespace ps

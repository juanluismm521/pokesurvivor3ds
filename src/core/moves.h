// moves.h — movimientos en una partida (moves.js de la web): instancia con las
// mejoras de nivel aplicadas, subir de nivel, evolución y pool por tipos.
#pragma once
#include <cstdint>

#include "core/gamedata.h"

namespace ps {

struct MoveInst {
  int16_t def = -1;          // índice en kMoves
  int8_t lvl = 1;
  bool evolved = false;
  uint32_t has = 0;          // qué números existen (ver MoveDef::has)
  float v[MS_NUM] = {};
  float t = 0;               // recarga restante
  float cast_at = -1;        // cuándo se lanzó (destello del icono en el HUD)
  float since = 0;           // desde cuándo lo tienes (segundos de partida; resumen de la pausa)
  mutable double dealt = 0;  // daño hecho con él (resumen de la pausa; lo apunta combat al golpear)

  const MoveDef& d() const { return kMoves[def]; }
  int cat() const { return kMoves[def].cat; }
  int kind() const { return kMoves[def].kind; }
  int type() const { return kMoves[def].type; }
  bool has_(int s) const { return (has >> s) & 1u; }
  float get(int s) const { return has_(s) ? v[s] : 0.0f; }   // `m.x || 0`
  int geti(int s) const { return int(get(s)); }               // `m.x | 0`
  const char* name() const;  // nombre (el evolucionado si ha evolucionado)
};

MoveInst move_instance(int def, int lvl = 1);
bool move_level_up(MoveInst& m);               // false si ya está al máximo
const char* move_next_up_text(const MoveInst& m);  // null si está al máximo
inline int move_max_lvl() { return 5; }

// Evolución: ¿puede evolucionar? (null si no). `ready` = condición cumplida.
struct EvoInfo { const char* name; int need; const char* need_name; const char* text; bool ready; };
bool move_evo_info(const MoveInst& m, const uint8_t stats_taken[LS_COUNT], EvoInfo& out);
bool move_evolve(MoveInst& m);

// Movimientos que puede aprender un Pokémon (sus tipos + normal), en el orden
// de la web (Set: primero el pool normal, luego los de cada tipo).
int move_pool_for(const int8_t types[2], int16_t* out, int max);
// Lo mismo más los movimientos extra de ese Pokémon (del port: los que en los
// juegos aprende por MT o tutor y su tipo no le da; hoy, Darkrai). Es lo que
// sale en las cartas de nivel, Repertorio, Atajos y Vetos.
int move_pool_for_dex(int dex, const int8_t types[2], int16_t* out, int max);
int move_extras_for(int dex, int16_t* out, int max);
// Movimiento inicial de serie (G.baseStartOf): el del starter o el de su tipo.
int move_base_start_for(int dex);
// Ataques que se pueden elegir para empezar (G.startChoices): los suyos, sin
// potenciadores, y siempre el de serie. Devuelve cuántos.
int move_start_choices(int dex, int16_t* out, int max);
// Con el que empieza (G.startMoveOf): el elegido (pick, de save.pstart) si
// tiene comprada la mejora "Movimiento inicial" y sigue siendo de los suyos; si
// no, el de serie.
int move_start_for(int dex, int pick = -1, bool bought = false);
// Mejora «Repertorio» (empiezas con 2): los que pueden ser el segundo (de sus tipos,
// sin potenciadores ni el inicial), por orden de la tabla. Devuelve cuántos.
int move_second_choices(int dex, int start, int16_t* out, int max);

}  // namespace ps

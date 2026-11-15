// evo.h — evolución en plena partida (web, Gran Actualización 13-11-2026: «Evolución en plena
// partida»), con los niveles de los juegos (del port, el dueño 15-11-2026: Charmander al 16,
// Charmeleon al 36; la web usa siempre el 20 y el 40).
//
// Tu Pokémon empieza con los stats del primero de su línea y se hace más fuerte en dos
// etapas, como mucho:
//   - si ya está evolucionado (Charizard), toma solo los stats del siguiente de la línea al
//     nivel en que evoluciona en los juegos (Charmeleon al 16, Charizard al 36);
//   - si puede evolucionar (Charmander), a ese nivel eliges: evolucionar (cambian su aspecto y
//     sus tipos) o mantener su aspecto. Sus stats suben igual. Con varias (Eevee), 3 al azar;
//   - si no evoluciona nunca, empieza al 65 % y sube al 82 % y al 100 % (niveles 20 y 40).
// Las que no son por nivel (piedra, intercambio, amistad…): niveles 20 y 40, como la web.
#pragma once
#include <cstdint>

namespace ps {

struct Game;
struct Player;

constexpr int kEvoLv1 = 20, kEvoLv2 = 40;   // las que no son por nivel (y los que no evolucionan)
constexpr float kEvoSolo[3] = {0.65f, 0.82f, 1.0f};

int evo_next(int dex, int16_t* out, int max);   // a quién evoluciona (los que existen)
int evo_prev(int dex);                            // de quién viene (0: de nadie)
int evo_level(int from, int to);                  // nivel en los juegos (0: no es por nivel)

void evo_begin(Game& g, Player& p);               // al empezar la partida
int evo_due_level(const Player& p);               // nivel de la siguiente etapa (0: no hay más)
void evo_update(Game& g, Player& p);              // tras subir de nivel: etapa automática o abre la elección
void evo_choose(Game& g, Player& p, int pick);    // pick < evo_nopts: evolucionar a esa; si no, mantener su aspecto

}  // namespace ps

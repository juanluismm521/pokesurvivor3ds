// runclaim.h — el «reclamo» de una partida para pokesurvivor.com (como runEnded de la web).
//
// Desde el 15-11-2026 el servidor de la web sólo suma a tu cuenta lo que le llega con un reclamo
// (POST /run/end): monedas, tickets, caramelos, maestría y los Pokémon conseguidos en la partida
// (rescatados y shinies salvajes). Cada partida que acaba deja su reclamo en una cola de la
// consola (la plataforma la guarda en la SD) y se cobra al sincronizar (cloud::send_claims),
// también horas después (el servidor recibe cuánto hace que acabó).
#pragma once
#include <cstdint>

#include "core/json.h"

namespace ps {

struct Game;

constexpr int kRunQueueMax = 50;   // (la web guarda las 20 últimas)

// El reclamo de la partida que acaba de terminar (con g.summary ya hecho: game_end_run).
Json run_claim(const Game& g);
// Lo pone al final de la cola ([{id, end, claim}]; end = la hora de la consola, en ms).
void runq_push(Json& queue, const Json& claim, int64_t end_ms);

}  // namespace ps

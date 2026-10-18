// gacha.h — un banner por generación (gacha.js de la web) y el test de
// personalidad (starters.js).
//   Probabilidades: 1★ 45% · 2★ 30% · 3★ 17% · 4★ 6,5% · 5★ 1,5%
//   la ×10 trae al menos un 4★ · a las 70 tiradas sin 5★, el siguiente es 5★
//   1 de cada 100 sale shiny · repetidos → Pokémonedas (shiny repetido, ×3)
#pragma once
#include <cstdint>

#include "core/rng.h"
#include "core/save.h"

namespace ps {

constexpr int kPity5 = 70;
struct PullResult { int16_t dex; int8_t rarity; bool shiny, is_new; int refund; };

// ¿Qué gasta una tirada? false si no te llega. use_t10: un ticket ×10 (sólo para la ×10).
bool gacha_cost(const Save& s, int n, bool use_t10, int& t1, int& t10);
// Hace n tiradas (1 o 10) en el banner de la generación `gen`. Devuelve cuántas.
int gacha_pull(Save& s, int n, int gen, bool use_t10, Rng& rng, int64_t now_ms, PullResult* out);
int gacha_pool(int rarity, int gen, int16_t* out);   // Pokémon de una rareza en un banner
int gacha_featured(int gen, int16_t* out);            // los 3 más fuertes (cartel)
extern int gacha_force_shiny;                        // pruebas: las N próximas salen shiny

// Test de personalidad: puntuaciones por rasgo (en el orden en que se
// sumaron) → rasgo principal y compañero que mejor encaja.
struct TraitScores { int8_t order[32]; int16_t pts[32]; int n = 0; void add(int trait, int p); };
void personality_result(const TraitScores& s, Rng& rng, int& trait, int& dex);

}  // namespace ps

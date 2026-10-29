// gacha.h — un banner por generación (gacha.js de la web) y el test de
// personalidad (starters.js).
//   Probabilidades: 1★ 45% · 2★ 30% · 3★ 17% · 4★ 6,5% · 5★ 1,5%
//   la ×10 trae al menos un 4★ · a las 70 tiradas sin 5★, el siguiente es 5★
//   1 de cada 100 sale shiny (2 de cada 100 con el Amuleto Iris de esa región)
//   repetidos → Pokémonedas (shiny repetido, ×3)
// Amuleto Iris (web, 23-10-2026): uno por región; se tiene al completar su
// Pokédex (legendarios incluidos). No se guarda aparte: sale de lo que tienes.
// ×100 (web, 22-10): 100 tickets o 10 tickets ×10; son diez ×10 seguidas (cada
// una con su Épica asegurada). Desde entonces no hay ×1 (en el núcleo sigue).
#pragma once
#include <cstdint>

#include "core/rng.h"
#include "core/gamedata.h"
#include "core/save.h"

namespace ps {

constexpr int kPity5 = 70;
constexpr float kShinyRate = 1.0f / 100, kShinyRateIris = 2.0f / 100;
struct PullResult { int16_t dex; int8_t rarity; bool shiny, is_new; int refund; };

// ¿Qué gasta una tirada? false si no te llega. use_t10: un ticket ×10 (sólo para la ×10).
bool gacha_cost(const Save& s, int n, bool use_t10, int& t1, int& t10);
// Hace n tiradas (1, 10 o 100) en el banner de la generación `gen` (out: n
// huecos). Devuelve cuántas (0 si no te llegan los tickets).
int gacha_pull(Save& s, int n, int gen, bool use_t10, Rng& rng, int64_t now_ms, PullResult* out);
// Amuleto Iris: cuántos tienes de una región y si está completa.
void gacha_dex_of(const Save& s, int gen, int& have, int& total);
bool gacha_has_iris(const Save& s, int gen);
float gacha_shiny_rate(const Save& s, int gen);
int gacha_pool(int rarity, int gen, int16_t* out);   // Pokémon de una rareza en un banner
int gacha_featured(int gen, int16_t* out);            // los 3 legendarios del cartel
// Banner «Todas» (del port, 2026-10-30): gen 0 = todas las regiones juntas (su
// pity va en pity[0]; su Amuleto Iris, la Pokédex entera).
constexpr int kGachaAll = 0;
const BannerDef& gacha_banner(int gen);
inline int gacha_next(int gen) { return gen == 9 ? kGachaAll : gen == kGachaAll ? 1 : gen + 1; }   // (… Paldea, Todas, Kanto …)
inline int gacha_prev(int gen) { return gen == 1 ? kGachaAll : gen == kGachaAll ? 9 : gen - 1; }
extern int gacha_force_shiny;                        // pruebas: las N próximas salen shiny

// Test de personalidad: puntuaciones por rasgo (en el orden en que se
// sumaron) → rasgo principal y compañero que mejor encaja.
struct TraitScores { int8_t order[32]; int16_t pts[32]; int n = 0; void add(int trait, int p); };
void personality_result(const TraitScores& s, Rng& rng, int& trait, int& dex);

}  // namespace ps

#pragma once
// Desafío semanal (web, Gran Actualización 13-11-2026: challenge.js).
//
// Cada semana (la de las misiones: lunes a domingo, «w2026-47») todos juegan lo mismo:
// el mismo Pokémon (sin mejoras, naturaleza neutra, sin shiny), el mismo mapa (semilla)
// y una condición (lluvia, sol, niebla, nieve o arena toda la partida, enemigos veloces,
// cañón de cristal, sin curaciones, noche cerrada o todos de un tipo) y a veces «sin
// objetos». Se sale de una fórmula con la semana (FNV-1a + mulberry32): es la misma
// que en la web sin preguntar a ningún servidor.
//
// Un intento por semana (wtry, también los de la web al sincronizar) y, si aguantas
// 3 minutos, un ticket ×10 (una vez por semana: wch). No suma maestría.
#include <cstdint>

namespace ps {

enum WeeklyCond : int8_t {
  WC_RAIN, WC_SUN, WC_NOITEMS, WC_FAST, WC_GLASS, WC_FOG, WC_SNOW, WC_SAND, WC_NOHEAL, WC_NIGHT, WC_MONO, WC_COUNT
};
struct WeeklyCondDef { const char* id; const char* name; const char* desc; int8_t wx; float w; };
extern const WeeklyCondDef kWeeklyConds[WC_COUNT];

struct Weekly {
  char week[12] = {};
  uint32_t seed = 0;
  int dex = 0;            // (número del port)
  int8_t cond = WC_RAIN;
  bool no_items = false;  // también sin objetos (además de su condición)
  int8_t type = -1;       // WC_MONO: el tipo
};
constexpr int kWeeklyPrizeT = 180;   // segundos para el ticket ×10

Weekly weekly_of(const char* week);
// Nombre («Siempre llueve», «Todos de tipo Fuego»…) y explicación, en español
void weekly_label(const Weekly& w, char* out, int cap);
void weekly_desc(const Weekly& w, char* out, int cap);
// Milisegundos que le quedan a la semana (hora de Madrid, como la web; now_ms en UTC)
int64_t weekly_ends_in(int64_t now_utc_ms);

}  // namespace ps

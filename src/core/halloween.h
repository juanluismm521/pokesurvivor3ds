// halloween.h — el evento de Halloween (systems/halloween.js de la web, 25-28-10-2026).
// Del 30 de octubre al 2 de noviembre (hora de España, de cualquier año) el juego
// cambia solo; el 3 vuelve todo a la normalidad:
//   - mapa de noche: 3 biomas con calabazas (World::set_halloween, run_biome)
//   - dos Pokémon Fantasma más en cada tramo de enemigos (enemies.cpp)
//   - caramelos: los sueltan los Fantasma y las calabazas, al ritmo del tiempo
//     aguantado (como mucho kCandyRate por minuto) y kCandyJudge de golpe al llegar
//     al gran Gengar del minuto 30
//   - Tienda Kecleon: disfraces de bruja por kSkinPrice caramelos (para siempre)
//   - gacha: banner Noche Espectral (Fantasma de todas las regiones, shiny al 2 %)
//   - la Sentencia: el gran Gengar bruja en vez de Arceus (judgment.cpp)
// Una partida empezada durante el evento lo es hasta que acaba (RunConfig::hw; en
// grupo lo decide el anfitrión). La fecha la pone la plataforma: la hora del
// servidor si la sabe (la cabecera Date, como la web) y si no, la de la consola.
#pragma once
#include <cstdint>

namespace ps {

struct Game;
struct Save;

constexpr int kSkinPrice = 300;          // caramelos por disfraz
constexpr float kCandyRate = 1.25f;      // como mucho, por minuto aguantado
constexpr int kCandyJudge = 60;          // al llegar al gran Gengar (minuto 30)

// ¿Es Halloween ese día (mes 1-12, día del mes; hora de España)?
bool halloween_date(int month, int day);
// Con la hora UTC (ms desde 1970): la fecha de España (con su horario de verano).
void madrid_date(int64_t utc_ms, int& year, int& month, int& day);
bool halloween_at(int64_t utc_ms);
// Cuenta atrás del menú: desde 14 días antes, cuánto falta (on = false); durante
// el evento, cuánto queda (on = true). false el resto del año.
bool halloween_countdown(int64_t utc_ms, bool& on, int64_t& ms);
// La medianoche de España de ese día, en ms UTC.
int64_t madrid_midnight(int year, int month, int day);

// ¿Puede soltarse otro caramelo a los `t` segundos? (y lo cuenta). `need`: los
// Fantasma en cuanto toca (1); los demás, sólo si se han ido acumulando (3).
bool candy_take(Game& g, float t, int need = 1);

// Disfraces (core/skins.js): índice en kSkins o -1; el que lleva puesto un Pokémon
// (-1 si ninguno); comprar (sólo en Halloween, con caramelos) y ponérselo/quitárselo.
int skin_index(const char* id);
int skin_worn(const Save& s, int dex);
enum SkinBuy { SKIN_OK, SKIN_NOT_EVENT, SKIN_OWNED, SKIN_NO_CANDY, SKIN_BAD };
SkinBuy skin_buy(Save& s, int k, bool event_on, int64_t now_ms);
void skin_wear(Save& s, int dex, int k, int64_t now_ms);   // k = -1: quitárselo

}  // namespace ps

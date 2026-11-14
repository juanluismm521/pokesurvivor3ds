// mons.h — naturaleza, habilidad y MT de cada Pokémon, y la Tienda Kecleon
// (systems/mons.js y data/shop.js de la web, 20-10-2026, del PR #236 de TheAdro7).
//
// Cada Pokémon de tu colección tiene:
//   - NATURALEZA (kNatures): +10 % a un stat y −10 % a otro (5 son neutras). Se
//     cambia con una Menta.
//   - HABILIDAD (kAbils; lo que hace, core/abilities.h): una de las suyas; la
//     oculta sale 1 de cada 20. Se cambia con una Cápsula Habilidad (la oculta,
//     con la Cápsula Habilidad Oculta).
//   - MT aprendidas: ataques que no son de sus tipos pero sí aprende en los
//     juegos. Salen en sus cartas de nivel y se pueden elegir de ataque inicial.
//
// Lo que le TOCÓ (naturaleza y habilidad) no se guarda: sale "al azar" pero fijo
// para cada cuenta y Pokémon (semilla: fecha de creación de la partida + nº de la
// Pokédex), igual que en la web: todos los aparatos de la misma cuenta ven lo mismo.
// Lo cambiado con Menta o Cápsula sí se guarda (Save::pnat / pabil, con la fecha).
//
// La tienda vende con Pokémonedas y lo comprado va a la Mochila (Save::bag) o a
// las MT (Save::tms); se gasta al usarlo en la ficha del Pokémon (Colección).
// Cada día hay 8 MT a la venta, las mismas que en la web (semilla: la fecha en España).
#pragma once
#include <cstdint>

#include "core/save.h"

namespace ps {

// Número fijo entre 0 y 1 para un texto (hash de mons.js).
double mons_hash01(const char* s);

// ---------------- naturaleza ----------------
int nature_rolled(int64_t created, int dex);           // la que le tocó (índice de kNatures)
int nature_of(const Save& s, int dex);                 // la de la Menta o la que le tocó
void nature_set(Save& s, int dex, int nat, int64_t now_ms);
const char* nature_name(int nat);                      // en el idioma del juego
// El Pokémon con su naturaleza aplicada (pa/sa: daño de sus ataques físicos/
// especiales; df/sd: defensas; spd: velocidad; atk = el mayor de pa y sa).
// Copia: kDex no se toca. Con nat < 0, igual que el base.
void mon_with_nature(const Mon& base, int nat, Mon& out);

// ---------------- habilidad ----------------
struct AbilPick { int id = -1; int slot = 0; };        // id: índice de kAbils (-1: no tiene)
const int16_t* abil_slots(int dex);                    // [1, 2 o -1, oculta o -1]
int abil_rolled_slot(int64_t created, int dex);
AbilPick abil_of(const Save& s, int dex);
// Huecos a los que se puede cambiar (la oculta, sólo con la Cápsula Oculta). Devuelve cuántos.
int abil_choices(const Save& s, int dex, bool hidden, int out[3]);
void abil_set(Save& s, int dex, int slot, int64_t now_ms);
const char* abil_name(int id);                         // en el idioma del juego ("" si -1)

// ---------------- MT ----------------
int tm_index_of_move(int move);                        // índice de kTms o -1
bool tm_natural(int dex, int tm);                      // ya lo sabe sin MT (es de sus tipos)
TmBits tm_natural_bits(int dex);                     // (todas a la vez: bit = kTms)
bool tm_taught(const Save& s, int dex, int tm);
TmBits tm_taught_bits(const Save& s, int dex);       // las aprendidas (bit = kTms), para las listas de ataques
bool tm_can_learn(const Save& s, int dex, int tm);     // la aprende en los juegos y aún no la sabe
int tm_learnable(const Save& s, int dex, int* out);    // las que puede aprender (índices de kTms)
bool tm_teach(Save& s, int dex, int tm);               // gasta una de la Mochila; false si no puede

// ---------------- Mochila y tienda ----------------
struct ShopItemDef { const char* id; const char* name; const char* desc; const char* icon; int price; };
extern const ShopItemDef kShopItems[BAG_COUNT];
bool bag_take(Save& s, int item);
bool tm_take(Save& s, int tm);
// Fecha de España "AAAAMMDD" (la semilla de las MT del día).
void shop_day(int64_t utc_ms, char out[9]);
// Las 8 MT a la venta ese día (índices de kTms). Si ninguna le sirve al compañero,
// la última se cambia por una que sí aprenda. Devuelve cuántas (8).
constexpr int kShopDaily = 8;
int shop_daily(const Save& s, const char* day, int out[kShopDaily]);
int tm_price(int tm);
enum ShopBuy { BUY_OK, BUY_NO_COINS, BUY_GONE, BUY_FULL, BUY_BAD };
ShopBuy shop_buy_item(Save& s, int item);
ShopBuy shop_buy_tm(Save& s, int tm, const char* day);

}  // namespace ps

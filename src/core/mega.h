// mega.h — megaevoluciones (propias del port: la web no las tiene).
//
//   - La megapiedra es un objeto más (sale en cofres, cofres con candado y
//     jefes, donde salen los objetos) y sólo la de TU Pokémon: si llevas a
//     Gengar, la Gengarita. Con dos megas (Charizard X/Y, Absol/Absol Z…)
//     sale una u otra.
//   - Probabilidad: kMegaChanceBase y +kMegaChanceUp por nivel de la mejora
//     «Megapiedra» (sólo en los Pokémon con mega; Save::pmega).
//   - Con la piedra puesta, Y megaevoluciona durante kMegaTime s (las
//     estadísticas y el tipo de la mega; su sprite si SpriteCollab lo tiene) y
//     luego hay kMegaCooldown s de recarga. La transformación dura
//     kMegaTransform s (invulnerable, con su animación).
// Todo en el núcleo: en grupo, todas las consolas lo simulan igual.
#pragma once
#include "core/gamedata.h"

namespace ps {

struct Game;
struct Player;

// Megapiedras: objetos del port, después de los de la web (kItems).
constexpr int IT_MEGA_A = IT_COUNT, IT_MEGA_B = IT_COUNT + 1;
constexpr int kItemTotal = IT_COUNT + 2;
inline bool is_mega_stone(int id) { return id == IT_MEGA_A || id == IT_MEGA_B; }

constexpr float kMegaTime = 20, kMegaCooldown = 45, kMegaTransform = 1.2f;
constexpr float kMegaChanceBase = 0.10f, kMegaChanceUp = 0.06f;
constexpr int kMegaUpMax = 5;
// Coste de la mejora «Megapiedra» (nivel actual → siguiente).
int mega_up_cost(int level);

int mega_index(int dex, int slot);   // índice en kMegas o -1
int mega_count(int dex);             // 0, 1 o 2
inline bool has_mega(int dex) { return mega_count(dex) > 0; }
float mega_chance(int up_level);

// Nombre y descripción de un objeto (los de la web o la megapiedra de `dex`).
const char* item_name(int id, int dex);
const char* item_desc(int id, int dex);
const char* item_icon(int id);       // id del icono en el atlas (nullptr: megapiedra, se dibuja)

// El objeto que sale (cofres, jefes…): a veces la megapiedra de alguno.
int roll_item(Game& g);
// Al cogerla: la de tu Pokémon (o, si no tiene mega, otro objeto).
int mega_stone_for(Game& g, const Player& p, int id);

bool mega_ready(const Player& p);    // tiene la piedra puesta y no está en recarga
void mega_start(Game& g, Player& p); // (Y)
void mega_end(Game& g, Player& p);
void mega_update(Game& g, Player& p, float dt);

}  // namespace ps

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

constexpr float kMegaTime = 20, kMegaCooldown = 40, kMegaTransform = 1.2f;
constexpr float kMegaChanceBase = 0.10f, kMegaChanceUp = 0.06f;
// Además de sus estadísticas, mientras dura (se nota, pero sin romper el juego):
constexpr float kMegaAtk = 1.25f;      // ataque (físico y especial)
constexpr float kMegaSpd = 1.15f;      // velocidad
constexpr float kMegaCd = 0.75f;       // recarga de los ataques (un 25 % más rápida)
constexpr float kMegaRegen = 0.015f;   // vida por segundo (fracción de la vida máxima)
constexpr float kMegaArmor = 0.15f;    // menos daño recibido
// Las megas Z (Leyendas Z-A: Mega-Lucario Z, Mega-Garchomp Z, Mega-Absol Z…)
// duran 5 s menos pero empujan más, como en Z-A (del port; el dueño, 2026-10-26).
constexpr float kMegaZTime = 15;
constexpr float kMegaZAtk = 1.40f, kMegaZSpd = 1.25f, kMegaZCd = 0.65f, kMegaZRegen = 0.02f, kMegaZArmor = 0.20f;
constexpr int kMegaUpMax = 5;
// Coste de la mejora «Megapiedra» (nivel actual → siguiente).
int mega_up_cost(int level);

int mega_index(int dex, int slot);   // índice en kMegas o -1
bool mega_is_z(int k);               // ¿es una mega Z? (k: índice en kMegas)
// Lo que da cada mega mientras dura (las Z, más y menos rato).
struct MegaBoost { float time, atk, spd, cd, regen, armor; };
MegaBoost mega_boost(int k);
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

// orbs.h — esferas de Mundo Misterioso (orbs.js de la web, 23-10-2026): de un
// solo uso, se encuentran en la partida (cofres, jefes y, a veces, al derrotar
// a un Pokémon) y se pierden al acabarla (no se guardan).
//   4 huecos, hasta 3 de cada. Si no te cabe, se queda en el suelo.
//   En la 3DS (del port): A usa la elegida, B elige la siguiente y tocar su
//   casilla (pantalla de abajo) la usa. 1 s de recarga entre una y otra.
//   foe   a los enemigos de tu pantalla (dormir, frenar, empujar)
//   buff  a ti y a los compañeros a menos de media pantalla
// A los jefes y legendarios les afectan menos. Van por la entrada (CMD_ORB):
// en grupo todas las consolas las simulan igual.
#pragma once
#include <cstdint>

namespace ps {

struct Game;
struct Player;

// ORB_HEAL (del port, 2026-10-30): cura a todo el equipo, esté donde esté.
enum OrbId : int8_t { ORB_SLUMBER, ORB_SLOW, ORB_BLOWBACK, ORB_FOCUS, ORB_LUMINOUS, ORB_QUICK, ORB_HEAL, ORB_COUNT };
constexpr float kOrbHeal = 0.3f;   // (Curaesfera) de la vida máxima
struct OrbDef {
  const char* id;
  const char* name;
  bool foe;
  float dur;             // (buff) segundos
  uint32_t col;          // color (0xRRGGBB)
  const char* desc;
};
extern const OrbDef kOrbs[ORB_COUNT];

constexpr int kOrbSlots = 4, kOrbStack = 3;
constexpr float kOrbCd = 1;
constexpr float kOrbNear = 384;              // "media pantalla"
constexpr float kOrbSphereRate = 1.0f / 300; // al derrotar a un Pokémon normal
// El tesoro de la Luminosfera brilla 40 s (luego se apaga) y lejos, a unas 4-6
// pantallas: hay que ir a por él sin entretenerse (el dueño, 2026-10-30: «que sea
// desafiante llegar»). A la velocidad de un Pokémon normal (~106), 17-23 s en línea
// recta; con los enemigos en medio, justo.
constexpr float kTreasureTime = 40;
constexpr float kTreasureMin = 1800, kTreasureMax = 2400;
// La Luminosfera hace brillar el tesoro (game.h, Game::treasure).
void orb_place_treasure(Game& g, const Player& p);
void treasure_update(Game& g, float dt);

int orb_roll(Game& g);
bool orb_can_take(const Player& p, int id);
bool orb_add(Player& p, int id);
// Recogida: la añade, la anuncia y, la primera vez en la partida, explica qué hace.
void orb_got(Game& g, Player& p, int id);
// Usar la del hueco `slot` (0-3). → false si no se ha podido (vacío, recarga…).
bool orb_use(Game& g, Player& p, int slot);
void orbs_update(Game& g, Player& p, float dt);
int orb_count(const Player& p);   // cuántas lleva en total
const char* orb_effect(int id);   // lo que hace, corto («+25 % de crítico»)

}  // namespace ps

// abilities.h — qué hace cada habilidad en la partida (data/ability-fx.js de la
// web, 20-10-2026, del PR #236 de TheAdro7, adaptado a este juego).
//
// Las que tienen sentido en un survivor tienen su efecto; las demás dan una
// pasiva sencilla de su tipo principal (+8 % de daño con sus movimientos), con
// su nombre de siempre. Las que llevan «plus» suman además esa pasiva.
//
// Ganchos (los llama el juego en el sitio de la web):
//   abil_setup       al empezar la partida (stats fijos, recarga)
//   abil_out         × al daño que hace un movimiento a un enemigo (combat.cpp)
//   abil_after_hit   ha golpeado a un enemigo (después del daño)
//   abil_in_mul      × al daño que recibe (cat: físico, especial o ninguno)
//   abil_absorbs     ese tipo no le hace daño: le cura un poco
//   abil_dodge       probabilidad de esquivar un golpe
//   abil_hurt        le acaban de golpear
//   abil_contact     un enemigo le ha golpeado cuerpo a cuerpo
//   abil_tick        cada paso
//   abil_kill        ha caído un enemigo
//   abil_speed       × a la velocidad
// Lo aleatorio usa el azar de la partida (g.rng): en grupo, igual en todas las consolas.
#pragma once
#include <cstdint>

#include "core/gamedata.h"

namespace ps {

struct Game;
struct Player;
struct Enemy;
struct MoveInst;

// El efecto de una habilidad (kAbils) o -1 (sólo la pasiva genérica).
int abil_fx(int abil);
// (pruebas) las filas de la tabla de efectos y su id
int abil_fx_rows();
const char* abil_fx_id(int row);
// ¿Lleva además la pasiva genérica de su tipo?
bool abil_plus(int abil);
// Descripción (en español; tr() la traduce): `d1` lo que hace y, si suma la pasiva
// de su tipo, `d2` esa pasiva (vacío si no). Con abil < 0, la pasiva genérica en d1.
void abil_desc(int abil, const Mon* mon, char* d1, int n1, char* d2, int n2);

void abil_setup(Game& g, Player& p, int abil);
float abil_out(Game& g, Player& p, const MoveInst* m, const Enemy* e);
void abil_after_hit(Game& g, Player& p, Enemy& e, const MoveInst* m);
float abil_crit_mul(const Player& p);       // daño del crítico (normal ×1,5)
float abil_status_k(const Player& p);       // × a quemaduras, venenos, frenazos y empujones (0, 1 o 2)
float abil_status_cd(const Player& p);      // × a la recarga de los potenciadores
float abil_heal_mul(const Player& p);
bool abil_absorbs(const Player& p, int type);
float abil_dodge(Game& g, const Player& p, int type, int cat);
float abil_in_mul(Game& g, const Player& p, int type, int cat);
void abil_hurt(Game& g, Player& p, int type, int cat);
bool abil_sturdy(const Player& p);
void abil_contact(Game& g, Player& p, Enemy& e);
void abil_tick(Game& g, Player& p, float dt);
void abil_kill(Player& p);
float abil_speed(Game& g, const Player& p);
bool abil_steady(const Player& p);          // no le frenan las trampas pegajosas ni el terreno
bool abil_levitate(const Player& p);        // flota: ni agua, ni lava, ni trampas
bool abil_lava_proof(const Player& p);
bool abil_trap_proof(const Player& p);
bool abil_damp(const Player& p);            // las explosiones de los kamikaze no le dañan

}  // namespace ps

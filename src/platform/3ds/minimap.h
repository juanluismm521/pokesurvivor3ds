// minimap.h — minimapa de la pantalla de abajo: el terreno de verdad (suelo,
// paredes y agua con los colores del bioma), lo que ves arriba (un marco),
// tú (una flecha), tus compañeros, enemigos, jefes, cofres, altares,
// manantiales, la grieta, el Pokémon perdido y su casa, y Arceus; lo
// importante que queda fuera, con una flecha en el borde.
//
// El terreno va en una textura de 128×64 casillas (3 px por casilla en
// pantalla) que se repinta poco a poco (unas filas por fotograma) en otra
// textura y se cambia al terminar: así no hay tirones ni en la 3DS vieja.
#pragma once
#include <3ds.h>
#include <citro2d.h>

#include <cstdint>

#include "core/game.h"

namespace ps {

struct Renderer;

class Minimap {
 public:
  static constexpr int kTexW = 128, kTexH = 64;   // casillas
  static constexpr float kPxPerTile = 3;          // en pantalla

  void init();
  // Repinta `rows` filas de la textura de reserva (centrada en el jugador
  // local); al acabar, la cambia por la que se ve.
  void work(Game& g, int rows);
  void draw(Game& g, Renderer& r, float x, float y, float w, float h, float now);
  // Qué es cada icono (al tocar el mapa).
  void legend(Renderer& r, float x, float y, float w, float h);

 private:
  C3D_Tex tex_[2];
  Tex3DS_SubTexture sub_;
  bool ready_[2] = {false, false};
  int front_ = 0;
  int32_t ox_[2] = {}, oy_[2] = {};   // casilla de la esquina de cada textura
  int row_ = -1;                      // fila que se pinta en la de reserva (-1: empezar)
  int biome_[2] = {-1, -1};
  float ang_ = kPi / 2;               // hacia dónde mira la flecha
};

}  // namespace ps

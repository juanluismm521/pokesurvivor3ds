// anim.h — reproduce las animaciones de un Pokémon (Animator de sprites.js).
//   loop(anim)        animación de fondo (Idle / Walk); no reinicia si ya suena
//   play(anim, ...)   animación de una vez (Attack, Shoot, Hurt...) que tapa a
//                     la de fondo hasta que termina
// El dibujo lo hace la plataforma con dex, cur, frame, dir y shiny.
#pragma once
#include <cstdint>

#include "core/gamedata.h"
#include "core/util.h"

namespace ps {

// Geometría de una animación (la shiny sólo difiere en casos raros).
const AnimMeta* anim_meta(int dex, int anim, bool shiny = false);
int anim_name(int dex, int anim, bool shiny = false);     // la que existe (o Idle/Walk)
bool has_shiny(int dex);
bool hovers(int dex);
int shadow_size(int dex);
// Radio de colisión y alto visual a partir del tamaño del sprite.
float body_radius(int dex, float scale = 1);
float body_height(int dex, float scale = 1);
// Tu Pokémon, si es de los grandes (18-10-2026): se dibuja algo más pequeño y
// choca como uno normal (playerScale / playerRadius de sprites.js).
float player_scale(int dex);
float player_radius(int dex);

struct Animator {
  int16_t dex = 0;
  bool shiny = false;
  float scale = 1;
  int8_t base = A_IDLE, cur = A_IDLE;
  int16_t frame = 0;
  float t = 0;            // tiempo dentro del fotograma, en 1/60 s
  float speed = 1;
  bool once = false, hold = false, done = false;
  int8_t dir = 0;

  void init(int d, float sc = 1, bool sh = false) {
    dex = int16_t(d); shiny = sh && has_shiny(d); scale = sc;
    base = cur = A_IDLE; frame = 0; t = 0; speed = 1; once = hold = done = false; dir = 0;
  }
  void loop(int anim, float spd = 1) {
    base = int8_t(anim);
    if (!once && cur != anim_name(dex, anim, shiny)) start(anim, false);
    if (!once) speed = spd;
  }
  void play(int anim, float spd = 1, bool hold_last = false) {
    start(anim, true);
    speed = spd > 0 ? spd : 1;
    hold = hold_last;
  }
  bool busy() const { return once && !done; }
  float progress() const;
  void start(int anim, bool o);
  void update(float dt);
};

}  // namespace ps

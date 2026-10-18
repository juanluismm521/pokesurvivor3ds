// fx.h — partículas, números de daño y efectos (fx.js de la web). Todo lo
// puramente visual: nada de esto cambia la partida. Usa su propia
// aleatoriedad (vrng) para que la partida sea igual con o sin efectos.
#pragma once
#include <cstdint>

#include "core/gamedata.h"
#include "core/pool.h"
#include "core/rng.h"

namespace ps {

struct Particle { float x, y, vx, vy, t, life, s, grav, drag; uint32_t color; };
struct FxSprite {
  float x, y, vx, vy, t, life, scale, angle, spin, grav, fr;
  int8_t shape, pal;
  bool fade, twinkle;
};
struct FxText { float x, y, vy, t, life; uint32_t color; bool big; char s[28]; };
struct FxRing { float x, y, r0, r1, life, t, width; uint32_t color; };
struct FxSlash { float x, y, angle, radius, arc, t, life; int8_t pal; };
struct FxBeam { float x, y, angle, len, width, t, life, seed; int8_t pal, shape; };

struct PxOpts { float vx = 0, vy = 0, life = 0.4f, size = 1, grav = 0, drag = 0.9f; };
struct SpOpts { float vx = 0, vy = 0, life = 0.4f, scale = 1, angle = 0, spin = 0, grav = 0, frame_rate = 10; bool fade = true; };

class Fx {
 public:
  // Calidad (Ajustes → Gráficos): 0 bajo, 1 medio, 2 alto.
  int quality = 2;
  bool dmg_numbers = true;
  Rng* vr = nullptr;

  void clear();
  void px(float x, float y, uint32_t color, const PxOpts& o = PxOpts());
  void burst(float x, float y, uint32_t color, int n = 8, float power = 110);
  void spark(float x, float y, uint32_t color, float dir_x, float dir_y, int n = 4);
  void sp(int shape, int pal, float x, float y, const SpOpts& o = SpOpts());
  void twinkle(float x, float y);
  void trail(int kind, int pal, float x, float y, float vx, float vy);
  // value: texto ya formado. is_number: se oculta con "Números de daño" apagado.
  void dmg_text(float x, float y, const char* value, uint32_t color, bool big = false, bool is_number = false);
  void dmg_num(float x, float y, int value, uint32_t color, bool big = false);
  void ring(float x, float y, float r0, float r1, uint32_t color, float life = 0.3f, float width = 3);
  void slash(float x, float y, float angle, float radius, float arc, uint32_t color, const MoveVis* vis);
  void beam(float x, float y, float angle, float len, float width, uint32_t color, int pal, int shape);
  void update(float dt);

  FixedList<Particle, 1400> parts;
  FixedList<FxSprite, 320> sprites;
  FixedList<FxText, 70> texts;
  FixedList<FxRing, 128> rings;
  FixedList<FxSlash, 48> slashes;
  FixedList<FxBeam, 64> beams;
  int count() const { return parts.n + sprites.n + texts.n; }
};

// Paleta de 5 colores de un tipo (o PAL_AURA).
inline const uint32_t* pal_colors(int pal) { return kPal[(pal >= 0 && pal < PAL_COUNT) ? pal : T_NORMAL]; }

}  // namespace ps

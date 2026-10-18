// util.h — ayudas compartidas (util.js de la web).
#pragma once
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace ps {

constexpr float kPi = 3.14159265358979f;
constexpr float kTau = 6.2832f;     // la web usa 6.2832 a menudo (no 2π exacto)
constexpr float kLift = 18.0f;      // G.LIFT: altura de proyectiles y efectos
constexpr float kPX = 2.0f;         // G.Sprites.PX: unidades de mundo por píxel de arte

template <class T> inline T clamp(T v, T a, T b) { return v < a ? a : v > b ? b : v; }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float dist(float ax, float ay, float bx, float by) { return std::hypot(bx - ax, by - ay); }
inline float dist2(float ax, float ay, float bx, float by) {
  const float dx = bx - ax, dy = by - ay;
  return dx * dx + dy * dy;
}
// U.norm: normaliza; (0,0) si es nulo.
inline void norm(float x, float y, float& ox, float& oy) {
  const float m = std::hypot(x, y);
  if (m > 1e-6f) { ox = x / m; oy = y / m; } else { ox = 0; oy = 0; }
}
// U.damp: interpolación independiente del framerate.
inline float damp(float a, float b, float rate, float dt) { return b + (a - b) * std::exp(-rate * dt); }

// U.hash: hash determinista 2D → [0,1). Idéntico al de la web (Math.imul en
// 32 bits). Se devuelve en double, como en JS, para que las comparaciones con
// umbrales den exactamente lo mismo al generar el mapa.
inline double hash(int32_t x, int32_t y, int32_t seed = 0) {
  uint32_t h = (uint32_t(x) * 374761393u) ^ (uint32_t(y) * 668265263u) ^ (uint32_t(seed) * 2246822519u);
  h = (h ^ (h >> 13)) * 1274126177u;
  h ^= h >> 16;
  return double(h) / 4294967296.0;
}

// Math.floor de JS a entero de 32 bits (para coordenadas de casillas).
inline int32_t ifloor(double v) { return int32_t(std::floor(v)); }

// Math.round de JS (las mitades redondean hacia +∞).
inline double jsround(double v) { return std::floor(v + 0.5); }

// Ángulo de pantalla (atan2(dy,dx)) → fila de dirección 0..7 de los sprites.
inline int dir_from_angle(float a) {
  const int r = int(jsround((kPi / 2 - a) / (kPi / 4)));
  return ((r % 8) + 8) % 8;
}

// U.mmss: "3:07"
void mmss(float s, char* out, int n);

}  // namespace ps

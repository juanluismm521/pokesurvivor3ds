// camera.h — vista y transformación mundo↔pantalla (camera.js de la web).
// En la 3DS la vista mide 800×480 unidades de mundo en la pantalla de arriba
// (400×240 px a escala 0,5): cada píxel de arte de los sprites cae justo en un
// píxel de la pantalla, y se ve casi lo mismo que en la web (768 de ancho).
#pragma once
#include <cmath>

#include "core/rng.h"
#include "core/util.h"

namespace ps {

struct Camera {
  float x = 0, y = 0;
  float w = 800, h = 480;
  float scale = 0.5f;
  float shake = 0;
  float sx = 0, sy = 0;
  bool calm = false;          // Ajustes → "menos sacudidas"

  void follow(float tx, float ty, float dt, Rng& vr) {
    x = damp(x, tx, 9, dt);
    y = damp(y, ty, 9, dt);
    update_shake(dt, vr);
  }
  // Sacudida (sólo se ve: usa la aleatoriedad visual).
  void update_shake(float dt, Rng& vr) {
    if (shake > 0) {
      shake = std::fmax(0.0f, shake - dt * 2.4f);
      const float a = shake * 7;
      sx = vr.range(-a, a);
      sy = vr.range(-a, a);
    } else { sx = sy = 0; }
  }
  void kick(float amount) {
    if (calm) amount *= 0.25f;
    shake = std::fmin(1.2f, shake + amount);
  }
  float left() const { return x - w / 2; }
  float top() const { return y - h / 2; }
  bool sees(float px, float py, float pad = 60) const { return sees_from(x, y, px, py, pad); }
  // ¿Se ve (px, py) desde una vista centrada en (cx, cy)? (vistas de cada jugador)
  bool sees_from(float cx, float cy, float px, float py, float pad = 60) const {
    const float l = cx - w / 2, t = cy - h / 2;
    return px > l - pad && px < l + w + pad && py > t - pad && py < t + h + pad;
  }
  float outer_radius() const { return std::hypot(w, h) / 2 + 40; }
};

}  // namespace ps

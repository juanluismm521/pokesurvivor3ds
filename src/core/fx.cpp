// fx.cpp — ver fx.h (fx.js de la web).
#include "core/fx.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/util.h"

namespace ps {

namespace {
const int kCapParts[3] = {350, 800, 1400};
const int kCapSprites[3] = {110, 210, 320};
const int kCapTexts[3] = {25, 45, 70};
const float kDens[3] = {0.35f, 0.65f, 1.0f};
}  // namespace

void Fx::clear() { parts.clear(); sprites.clear(); texts.clear(); rings.clear(); slashes.clear(); beams.clear(); }

void Fx::px(float x, float y, uint32_t color, const PxOpts& o) {
  if (parts.n >= kCapParts[quality]) return;
  Particle* p = parts.push();
  if (!p) return;
  *p = {x, y, o.vx, o.vy, 0, o.life, o.size * kPX, o.grav, o.drag, color};
}

void Fx::burst(float x, float y, uint32_t color, int n, float power) {
  n = std::max(1, int(jsround(n * kDens[quality])));
  for (int i = 0; i < n; ++i) {
    const float a = vr->next() * kTau, s = vr->range(power * 0.35f, power);
    PxOpts o;
    o.vx = std::cos(a) * s; o.vy = std::sin(a) * s;
    o.life = vr->range(0.22f, 0.5f);
    o.size = vr->next() < 0.35f ? 2 : 1;
    px(x, y, color, o);
  }
}

void Fx::spark(float x, float y, uint32_t color, float dir_x, float dir_y, int n) {
  n = std::max(1, int(jsround(n * kDens[quality])));
  const float base = std::atan2(dir_y, dir_x);
  for (int i = 0; i < n; ++i) {
    const float a = base + vr->range(-0.9f, 0.9f), s = vr->range(60, 170);
    PxOpts o;
    o.vx = std::cos(a) * s; o.vy = std::sin(a) * s;
    o.life = vr->range(0.14f, 0.32f);
    px(x, y, color, o);
  }
}

void Fx::sp(int shape, int pal, float x, float y, const SpOpts& o) {
  if (shape < 0 || shape >= SH_COUNT) return;
  if (sprites.n >= kCapSprites[quality]) return;
  FxSprite* s = sprites.push();
  if (!s) return;
  *s = {x, y, o.vx, o.vy, 0, o.life, o.scale, o.angle, o.spin, o.grav, o.frame_rate, int8_t(shape), int8_t(pal), o.fade, false};
}

void Fx::twinkle(float x, float y) {
  if (sprites.n >= kCapSprites[quality]) return;
  FxSprite* s = sprites.push();
  if (!s) return;
  *s = {x, y, 0, -8, 0, vr->range(0.45f, 0.7f), 1, 0, 0, 0, 0, SH_STAR, 0, false, true};
}

void Fx::trail(int kind, int pal, float x, float y, float vx, float vy) {
  if (vr->next() > kDens[quality]) return;
  const uint32_t* P = pal_colors(pal);
  const float back = std::atan2(-vy, -vx);
  auto j = [&]() { return vr->range(-3, 3); };
  PxOpts o;
  switch (kind) {
    case TR_EMBER: o.vx = std::cos(back) * 30; o.vy = -40; o.life = 0.35f; px(x + j(), y + j(), P[vr->next() < 0.5f ? 3 : 2], o); break;
    case TR_DRIP: o.vx = std::cos(back) * 20; o.vy = 10; o.grav = 260; o.life = 0.35f; px(x + j(), y + j(), P[3], o); break;
    case TR_SPARK: {
      const float jx = j() * 2, jy = j() * 2;
      const uint32_t c = vr->next() < 0.5f ? P[4] : P[3];
      o.vx = vr->range(-60, 60); o.vy = vr->range(-60, 60); o.life = 0.16f;
      px(x + jx, y + jy, c, o);
      break;
    }
    case TR_BUBBLES: {
      const float jx = j(), jy = j();
      o.vy = -35; o.life = 0.45f; o.size = vr->next() < 0.3f ? 2 : 1;
      px(x + jx, y + jy, P[3], o);
      break;
    }
    case TR_DIRT: o.vx = std::cos(back) * 25; o.vy = -20; o.grav = 300; o.life = 0.35f; px(x + j(), y + j(), P[vr->next() < 0.5f ? 2 : 1], o); break;
    case TR_WISP: o.vy = -30; o.life = 0.5f; o.size = 2; px(x + j() * 2, y + j() * 2, P[vr->next() < 0.5f ? 1 : 2], o); break;
    default: break;
  }
}

void Fx::dmg_text(float x, float y, const char* value, uint32_t color, bool big, bool is_number) {
  if (is_number && !dmg_numbers) return;
  if (texts.n >= kCapTexts[quality]) {
    if (!big) return;
    texts.remove_ordered(0);
  }
  FxText* t = texts.push();
  if (!t) return;
  t->x = x + vr->range(-6, 6); t->y = y; t->vy = -46; t->t = 0;
  t->life = big ? 0.95f : 0.62f; t->color = color; t->big = big;
  std::snprintf(t->s, sizeof(t->s), "%s", value);
}

void Fx::dmg_num(float x, float y, int value, uint32_t color, bool big) {
  char b[16];
  std::snprintf(b, sizeof(b), "%d", value);
  dmg_text(x, y, b, color, big, true);
}

void Fx::ring(float x, float y, float r0, float r1, uint32_t color, float life, float width) {
  FxRing* r = rings.push();
  if (!r) return;
  *r = {x, y, r0, r1, life, 0, width, color};
}

void Fx::slash(float x, float y, float angle, float radius, float arc, uint32_t color, const MoveVis* fx) {
  (void)color;
  const int pal = fx ? int(fx->pal) : int(T_NORMAL);
  FxSlash* s = slashes.push();
  if (s) *s = {x, y, angle, radius, arc, 0, 0.22f, int8_t(pal)};
  const float mx = x + std::cos(angle) * radius * 0.7f, my = y + std::sin(angle) * radius * 0.7f;
  if (!fx) return;
  SpOpts o;
  switch (fx->melee) {
    case ME_IMPACT: o.life = 0.2f; o.scale = 1.5f; o.frame_rate = 12; o.fade = false; sp(SH_IMPACT, pal, mx, my, o); break;
    case ME_JAWS: o.life = 0.24f; o.scale = 1.5f; o.frame_rate = 9; o.fade = false; sp(SH_JAWS, pal, mx, my, o); break;
    case ME_CLAW: o.life = 0.2f; o.scale = 1.5f; o.angle = angle - kPi / 4; sp(SH_CLAW, pal, mx, my, o); break;
    case ME_VINE:
    case ME_FEATHER:
      for (int i = 0; i < 4; ++i) {
        const float a = angle + vr->range(-arc / 2, arc / 2), r = radius * vr->range(0.5f, 1);
        SpOpts l;
        l.vx = std::cos(a) * 40; l.vy = std::sin(a) * 40 + 20; l.life = 0.45f; l.spin = vr->range(-8, 8); l.grav = 60;
        sp(SH_LEAF, fx->melee == ME_FEATHER ? T_FLYING : pal, x + std::cos(a) * r, y + std::sin(a) * r, l);
      }
      break;
    default: break;
  }
}

void Fx::beam(float x, float y, float angle, float len, float width, uint32_t color, int pal, int shape) {
  (void)color;
  FxBeam* b = beams.push();
  if (b) *b = {x, y, angle, len, width, 0, 0.2f, vr->next() * 100, int8_t(pal), int8_t(shape)};
  SpOpts o;
  o.life = 0.12f; o.scale = 1;
  sp(SH_IMPACT, pal, x + std::cos(angle) * 10, y + std::sin(angle) * 10, o);
  o.life = 0.18f; o.scale = 1.5f;
  sp(SH_IMPACT, pal, x + std::cos(angle) * len, y + std::sin(angle) * len, o);
}

void Fx::update(float dt) {
  for (int i = parts.n - 1; i >= 0; --i) {
    Particle& p = parts[i];
    p.t += dt;
    if (p.t >= p.life) { parts.remove_swap(i); continue; }
    p.vy += p.grav * dt;
    p.x += p.vx * dt; p.y += p.vy * dt;
    const float k = std::pow(p.drag, dt * 60);
    p.vx *= k; p.vy *= k;
  }
  for (int i = sprites.n - 1; i >= 0; --i) {
    FxSprite& s = sprites[i];
    s.t += dt;
    if (s.t >= s.life) { sprites.remove_swap(i); continue; }
    s.vy += s.grav * dt;
    s.x += s.vx * dt; s.y += s.vy * dt;
    if (s.spin != 0) s.angle += s.spin * dt;
  }
  for (int i = texts.n - 1; i >= 0; --i) {
    FxText& t = texts[i];
    t.t += dt;
    if (t.t >= t.life) { texts.remove_ordered(i); continue; }
    t.y += t.vy * dt; t.vy *= 0.94f;
  }
  for (int i = rings.n - 1; i >= 0; --i) { rings[i].t += dt; if (rings[i].t >= rings[i].life) rings.remove_swap(i); }
  for (int i = slashes.n - 1; i >= 0; --i) { slashes[i].t += dt; if (slashes[i].t >= slashes[i].life) slashes.remove_swap(i); }
  for (int i = beams.n - 1; i >= 0; --i) { beams[i].t += dt; if (beams[i].t >= beams[i].life) beams.remove_swap(i); }
}

}  // namespace ps

// anim.cpp — ver anim.h (sprites.js de la web).
#include "core/anim.h"

namespace ps {

namespace {
const SpriteMeta* meta(int dex) {
  const Mon* m = mon_by_dex(dex);
  return m ? &kSpriteMeta[m - kDex] : nullptr;
}
bool has(const SpriteMeta* m, int anim) { return m->a[anim].n > 0; }
}  // namespace

bool has_shiny(int dex) { const SpriteMeta* m = meta(dex); return m && m->shiny; }
bool hovers(int dex) { const SpriteMeta* m = meta(dex); return m && m->hover; }
int shadow_size(int dex) { const SpriteMeta* m = meta(dex); return m ? m->shadow : 1; }

int anim_name(int dex, int anim, bool shiny) {
  (void)shiny;   // (ningún shiny de la web tiene animaciones de menos)
  const SpriteMeta* m = meta(dex);
  if (!m) return A_IDLE;
  if (has(m, anim)) return anim;
  return has(m, A_IDLE) ? A_IDLE : A_WALK;
}

const AnimMeta* anim_meta(int dex, int anim, bool shiny) {
  const SpriteMeta* m = meta(dex);
  if (!m) return nullptr;
  shiny = shiny && m->shiny;
  const int name = anim_name(dex, anim, shiny);
  if (shiny && m->shiny_alt >= 0 && kShinyAlt[m->shiny_alt].a[name].n > 0) return &kShinyAlt[m->shiny_alt].a[name];
  return m->a[name].n > 0 ? &m->a[name] : nullptr;
}

float body_radius(int dex, float scale) {
  const AnimMeta* m = anim_meta(dex, A_IDLE);
  const float w = m ? m->w : 32;
  return clamp(w * 0.22f, 5.0f, 26.0f) * kPX * scale;
}

float body_height(int dex, float scale) {
  const AnimMeta* m = anim_meta(dex, A_IDLE);
  return (m ? m->ay : 24) * kPX * scale;
}

float Animator::progress() const {
  const AnimMeta* m = anim_meta(dex, cur, shiny);
  if (!m) return 1;
  float total = 0, before = 0;
  for (int i = 0; i < m->n; ++i) {
    const float d = kAnimDur[m->dur + i];
    if (i < frame) before += d;
    total += d;
  }
  return total > 0 ? (before + t) / total : 1;
}

void Animator::start(int anim, bool o) {
  cur = int8_t(anim_name(dex, anim, shiny));
  frame = 0; t = 0;
  once = o; done = false;
}

void Animator::update(float dt) {
  const AnimMeta* m = anim_meta(dex, cur, shiny);
  if (!m || done) return;
  t += dt * 60 * speed;
  while (t >= kAnimDur[m->dur + frame]) {
    t -= kAnimDur[m->dur + frame];
    frame++;
    if (frame >= m->n) {
      if (once) {
        if (hold) { frame = int16_t(m->n - 1); done = true; return; }
        // Vuelve a la animación de fondo.
        start(base, false);
        speed = 1;
        return;
      }
      frame = 0;
    }
  }
}

}  // namespace ps

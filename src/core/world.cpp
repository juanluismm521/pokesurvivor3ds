// world.cpp — ver world.h. Las funciones siguen a world.js línea a línea; el
// ruido y los umbrales usan double para que el mapa salga idéntico al de la web.
#include "core/world.h"

#include <cmath>
#include <cstring>

#include "core/game.h"
#include "core/util.h"

namespace ps {

namespace {
struct PropDef { bool solid; float hp; float r; bool flat; };
constexpr float INF = 1e30f;
const PropDef kPropDef[PR_COUNT] = {
  {true, INF, 13, false},    // tree
  {true, 34, 12, false},     // rock
  {true, 22, 12, false},     // chest
  {false, 1, 14, false},     // grass
  {true, INF, 9, false},     // altar
  {false, INF, 22, true},    // spring
  {true, INF, 14, false},    // bigchest
  {false, INF, 9, true},     // trap
};
inline double H(int32_t x, int32_t y, int32_t s) { return hash(x, y, s); }
inline uint64_t tkey(int32_t tx, int32_t ty) { return (uint64_t(uint32_t(tx)) << 32) | uint32_t(ty); }
constexpr uint64_t kEmpty = ~0ull;
}  // namespace

// ---------------- control ----------------

void World::reset(int32_t s) {
  seed_ = s ? s : 1;
  for (auto& c : cache_) c.valid = false;
  last_ = nullptr;
  for (auto& b : broken_) b = kEmpty;
  nbroken_ = 0;
  fade_t_ = -1;
  arena_ = ArenaInfo();
  biome_gen_++;
}

void World::set_biome(int i, bool instant) {
  const int b = ((i % kBiomeCount) + kBiomeCount) % kBiomeCount;
  if (b == biome_) return;
  fade_from_ = biome_;
  fade_t_ = instant ? -1 : 0;
  biome_ = b;
  biome_gen_++;
}

float World::fade_k() const {
  if (fade_t_ < 0) return -1;
  const float x = std::fmin(1.0f, fade_t_ / 2.6f);
  return x * x * (3 - 2 * x);
}

void World::set_arena(bool on, int32_t tx, int32_t ty, float r, int theme) {
  if (!on) { arena_ = ArenaInfo(); return; }
  arena_.on = true; arena_.tx = tx; arena_.ty = ty; arena_.r = r; arena_.theme = theme;
  // Olvida los chunks de esa zona (si se hubieran generado antes).
  const float R = r + 9;
  for (auto& c : cache_) {
    if (!c.valid) continue;
    if (std::fabs(float(c.cx * kChunkTiles + kChunkTiles / 2 - tx)) < R + kChunkTiles &&
        std::fabs(float(c.cy * kChunkTiles + kChunkTiles / 2 - ty)) < R + kChunkTiles) {
      c.valid = false;
      if (last_ == &c) last_ = nullptr;
    }
  }
}

// ---------------- ruido ----------------

double World::vnoise(double x, double y, int32_t s) const {
  const double xi = std::floor(x), yi = std::floor(y);
  const double xf = x - xi, yf = y - yi;
  const double u = xf * xf * (3 - 2 * xf), v = yf * yf * (3 - 2 * yf);
  const int32_t ix = int32_t(xi), iy = int32_t(yi);
  const double a = H(ix, iy, s), b = H(ix + 1, iy, s), c = H(ix, iy + 1, s), d = H(ix + 1, iy + 1, s);
  return (a + (b - a) * u) * (1 - v) + (c + (d - c) * u) * v;
}

double World::fbm(double x, double y, int32_t s) const {
  return vnoise(x, y, s) * 0.65 + vnoise(x * 2.13 + 17, y * 2.13 - 9, s + 1) * 0.35;
}

double World::arena_dist(int32_t tx, int32_t ty) const {
  return arena_.on ? std::hypot(double(tx - arena_.tx), double(ty - arena_.ty)) : 1e300;
}

bool World::arena_safe(int32_t tx, int32_t ty) const {
  const int32_t lx = tx - arena_.tx, ly = ty - arena_.ty;
  const ArenaTheme& th = kArenaThemes[arena_.theme];
  return (std::abs(lx) <= 2 && ly >= -6 && ly <= 5) || std::hypot(double(lx), double(ly)) < 2.5 ||
         (th.clear > 0 && std::hypot(double(lx), double(ly + 5)) < th.clear);
}

uint8_t World::terrain_raw(int32_t tx, int32_t ty) const {
  const double ad = arena_dist(tx, ty);
  if (ad < arena_outer()) {
    if (ad >= arena_.r) return TR_WALL;
    if (arena_safe(tx, ty)) return TR_FLOOR;
    const ArenaTheme& th = kArenaThemes[arena_.theme];
    // Columnas de 2×2 (cobertura) y charcos según el tipo de la arena.
    if (th.pillars > 0 && ad < arena_.r - 2 && H(tx >> 1, ty >> 1, seed_ + 31 + arena_.tx) < double(th.pillars)) return TR_WALL;
    if (th.pools > 0 && ad < arena_.r - 1.5 && fbm(tx / 3.2, ty / 3.2, seed_ + 37 + arena_.tx) < double(th.pools)) return TR_LIQUID;
    return TR_FLOOR;
  }
  const double d = std::hypot(double(tx), double(ty));
  if (d < 9) return TR_FLOOR;                              // claro inicial
  const double ramp = clamp((d - 9) / 8, 0.0, 1.0);        // aparición suave
  if (fbm(tx / 13.0, ty / 13.0, seed_ + 101) < 0.04 + 0.23 * ramp) return TR_LIQUID;
  if (fbm(tx / 8.0, ty / 8.0, seed_ + 202) > 1 - 0.27 * ramp) return TR_WALL;
  return TR_FLOOR;
}

// ---------------- datos de chunk ----------------

Chunk& World::chunk(int32_t cx, int32_t cy) {
  if (last_ && last_->cx == cx && last_->cy == cy && last_->valid) return *last_;
  Chunk* victim = nullptr;
  for (auto& c : cache_)
    if (c.valid && c.cx == cx && c.cy == cy) { c.stamp = ++stamp_; last_ = &c; return c; }
  // No está: se genera en el hueco libre o en el más viejo que no tenga cambios.
  Chunk* oldest = nullptr;
  for (auto& c : cache_) {
    if (!c.valid) { victim = &c; break; }
    if (!oldest || c.stamp < oldest->stamp) oldest = &c;
    // Un chunk con algo cambiado (roca tocada, altar usado, manantial gastado,
    // cofre a medio abrir) no se descarta: al regenerarlo volvería como nuevo.
    // Así el estado del mundo no depende de qué chunks se han mirado (cada
    // consola del cooperativo dibuja una zona distinta).
    if (victim && c.stamp >= victim->stamp) continue;
    if (chunk_modified(c)) continue;
    victim = &c;
  }
  if (!victim) victim = oldest;   // (todos cambiados: no pasa con 160 chunks en caché)
  Chunk& c = *victim;
  c.valid = true;
  c.cx = cx; c.cy = cy;
  c.stamp = ++stamp_;
  c.nprops = 0;
  for (int j = 0; j < kChunkTiles; ++j)
    for (int i = 0; i < kChunkTiles; ++i) {
      c.terr[j * kChunkTiles + i] = terrain_raw(cx * kChunkTiles + i, cy * kChunkTiles + j);
      c.prop_at[j * kChunkTiles + i] = -1;
    }
  generated_++;
  last_ = &c;
  place_props(c);
  return c;
}

bool World::chunk_modified(const Chunk& c) {
  for (int i = 0; i < c.nprops; ++i) {
    const Prop& p = c.props[i];
    if (p.used || p.charge < 1 || p.open > 0 || (p.destructible && p.hp < p.max_hp)) return true;
  }
  return false;
}

uint8_t World::terrain_at(int32_t tx, int32_t ty) {
  const int32_t cx = int32_t(std::floor(tx / double(kChunkTiles))), cy = int32_t(std::floor(ty / double(kChunkTiles)));
  Chunk& c = chunk(cx, cy);
  return c.terr[(ty - cy * kChunkTiles) * kChunkTiles + (tx - cx * kChunkTiles)];
}

void World::make_prop(Chunk& c, int type, int32_t tx, int32_t ty, int kind) {
  if (c.nprops >= kMaxPropsPerChunk) return;   // (no pasa con los datos de la web; ver pruebas)
  const PropDef& d = kPropDef[type];
  Prop& p = c.props[c.nprops];
  // Id fijo según el sitio (no según el orden en que se generan los chunks).
  p.id = int32_t(((uint32_t(c.cx) & 0x7FF) << 19 | (uint32_t(c.cy) & 0x7FF) << 8 | uint32_t(c.nprops)) & 0x3FFFFFFF) + 1;
  p.type = int8_t(type); p.kind = int8_t(kind);
  p.tx = tx; p.ty = ty;
  p.x = tx * kTS + kTS / 2; p.y = ty * kTS + kTS - 4;
  p.r = d.r; p.hp = d.hp; p.max_hp = d.hp;
  p.solid = d.solid; p.flat = d.flat;
  p.destructible = d.hp < INF;
  p.flash = 0;
  p.wiggle = float(H(tx, ty, 9) * 6.28);
  p.used = false; p.charge = 1; p.open = 0;
  p.gone = false;
  if (p.solid) c.prop_at[(ty - c.cy * kChunkTiles) * kChunkTiles + (tx - c.cx * kChunkTiles)] = c.nprops;
  c.nprops++;
}

void World::place_props(Chunk& c) {
  for (int j = 0; j < kChunkTiles; ++j)
    for (int i = 0; i < kChunkTiles; ++i) {
      if (c.terr[j * kChunkTiles + i] != TR_FLOOR) continue;
      const int32_t tx = c.cx * kChunkTiles + i, ty = c.cy * kChunkTiles + j;
      if (std::hypot(double(tx), double(ty)) < 7) continue;
      if (is_broken(tx, ty)) continue;

      // Necesita suelo alrededor para no pegarse a paredes / agua.
      bool open = true;
      for (int b = -1; b <= 1 && open; ++b)
        for (int a = -1; a <= 1; ++a)
          if (terrain_raw(tx + a, ty + b) != TR_FLOOR) { open = false; break; }

      // Dentro de la arena: la decoración de su tipo (y nada en el pasillo).
      const double ad = arena_dist(tx, ty);
      if (ad < arena_outer() + 2) {
        if (!arena_.on || ad >= arena_.r - 1.2 || arena_safe(tx, ty)) continue;
        const ArenaTheme& th = kArenaThemes[arena_.theme];
        const double hh = H(tx, ty, seed_ + 777);
        int at = -1;
        if (open && hh < double(th.tree)) at = PR_TREE;
        else if (open && hh < double(th.tree) + double(th.rock)) at = PR_ROCK;
        else if (hh > 1 - double(th.grass)) at = PR_GRASS;
        if (at >= 0) make_prop(c, at, tx, ty);
        continue;
      }

      const double h = H(tx, ty, seed_ + 404);
      const bool grove = fbm(tx / 10.0, ty / 10.0, seed_ + 505) > 0.6;
      int type = -1, kind = 0;
      // Interactivos (raros), lejos del claro del principio.
      const double hs = H(tx, ty, seed_ + 909);
      if (open && std::hypot(double(tx), double(ty)) > 12 && hs < 0.0032) {
        type = hs < 0.0004 ? PR_ALTAR : hs < 0.0008 ? PR_SPRING : hs < 0.0011 ? PR_BIGCHEST : PR_TRAP;
        kind = int(std::floor(H(tx, ty, seed_ + 910) * (type == PR_ALTAR ? 4 : 3)));
      }
      if (type >= 0) { /* ya elegido */ }
      else if (open && h < (grove ? 0.07 : 0.006)) type = PR_TREE;
      else if (open && h > 0.992) type = PR_ROCK;
      else if (open && h > 0.985 && h <= 0.992 && H(tx, ty, seed_ + 606) < 0.07) type = PR_CHEST;
      else if (fbm(tx / 5.0, ty / 5.0, seed_ + 707) > 0.66 && H(tx, ty, seed_ + 808) < 0.75) type = PR_GRASS;
      if (type < 0) continue;
      make_prop(c, type, tx, ty, kind);
    }
}

// ---------------- consultas ----------------

LiquidFx World::liquid_effect(const int8_t types[2]) const {
  LiquidFx e;
  int imm[2] = {-1, -1};
  switch (liquid_kind()) {
    case LQ_SWAMP: e.slow = 0.45f; imm[0] = T_WATER; break;
    case LQ_LAVA: e.dmg = 0.06f; e.flat = 5; e.every = 0.5f; e.color = 0xff8a3d; imm[0] = T_FIRE; break;
    case LQ_POISON: e.dmg = 0.035f; e.flat = 3; e.every = 0.5f; e.color = 0xc86bdc; imm[0] = T_POISON; imm[1] = T_STEEL; break;
    case LQ_SPARK: e.dmg = 0.045f; e.flat = 4; e.every = 0.7f; e.color = 0xffe14d; imm[0] = T_ELECTRIC; imm[1] = T_GROUND; break;
    case LQ_ICE: e.slide = true; imm[0] = T_ICE; break;
    default: e.slow = 0.6f; imm[0] = T_WATER; break;
  }
  if (types) for (int k = 0; k < 2; ++k)
    if (types[k] >= 0 && (types[k] == imm[0] || types[k] == imm[1])) return LiquidFx();
  return e;
}

Prop* World::prop_at_tile(int32_t tx, int32_t ty) {
  const int32_t cx = int32_t(std::floor(tx / double(kChunkTiles))), cy = int32_t(std::floor(ty / double(kChunkTiles)));
  Chunk& c = chunk(cx, cy);
  const int k = c.prop_at[(ty - cy * kChunkTiles) * kChunkTiles + (tx - cx * kChunkTiles)];
  return k < 0 ? nullptr : &c.props[k];
}

bool World::walkable(int32_t tx, int32_t ty) {
  const uint8_t t = terrain_at(tx, ty);
  if (t == TR_WALL) return false;
  if (t == TR_LIQUID) {
    const int lk = liquid_kind();
    if (lk == LQ_LAVA || lk == LQ_POISON || lk == LQ_SPARK) return false;   // lava, veneno, descarga
  }
  const Prop* p = prop_at_tile(tx, ty);
  return !(p && p->solid);
}

bool World::is_free(float x, float y, float r, bool allow_liquid) {
  const int32_t t0x = tile_of(x - r), t1x = tile_of(x + r), t0y = tile_of(y - r), t1y = tile_of(y + r);
  for (int32_t ty = t0y; ty <= t1y; ++ty)
    for (int32_t tx = t0x; tx <= t1x; ++tx) {
      const uint8_t t = terrain_at(tx, ty);
      if (t == TR_WALL || (!allow_liquid && t == TR_LIQUID)) return false;
      const Prop* p = prop_at_tile(tx, ty);
      if (p && dist2(x, y, p->x, p->y) < (r + p->r) * (r + p->r)) return false;
    }
  return true;
}

int World::props_in_circle(float x, float y, float r, Prop** out, int max) {
  int n = 0;
  for_props_in_rect(x - r, y - r, x + r, y + r, [&](Prop& p) {
    if (!p.destructible || p.hp <= 0 || n >= max) return;
    if (dist2(x, y, p.x, p.y) <= (r + p.r) * (r + p.r)) out[n++] = &p;
  });
  return n;
}

int World::specials_near(float x, float y, float r, Prop** out, int max) {
  int n = 0;
  for_props_in_rect(x - r, y - r, x + r, y + r, [&](Prop& p) {
    if (n >= max) return;
    if (p.type < PR_ALTAR) return;   // altar, spring, bigchest, trap
    if (dist2(x, y, p.x, p.y) <= (r + p.r) * (r + p.r)) out[n++] = &p;
  });
  return n;
}

Prop* World::nearest_breakable(float x, float y, float r) {
  Prop* list[64];
  const int n = props_in_circle(x, y, r, list, 64);
  Prop* best = nullptr;
  float bd = r * r;
  for (int i = 0; i < n; ++i) {
    if (!list[i]->solid) continue;
    const float d = dist2(x, y, list[i]->x, list[i]->y);
    if (d < bd) { bd = d; best = list[i]; }
  }
  return best;
}

bool World::collide(float& ex, float& ey, float r) {
  bool hit = false;
  for (int pass = 0; pass < 2; ++pass) {
    const int32_t t0x = tile_of(ex - r), t1x = tile_of(ex + r), t0y = tile_of(ey - r), t1y = tile_of(ey + r);
    for (int32_t ty = t0y; ty <= t1y; ++ty)
      for (int32_t tx = t0x; tx <= t1x; ++tx) {
        if (terrain_at(tx, ty) == TR_WALL) {
          // Punto más cercano de la caja del tile al centro del círculo.
          const float bx0 = tx * kTS, by0 = ty * kTS, bx1 = bx0 + kTS, by1 = by0 + kTS;
          const float qx = clamp(ex, bx0, bx1), qy = clamp(ey, by0, by1);
          const float dx = ex - qx, dy = ey - qy;
          const float d2 = dx * dx + dy * dy;
          if (d2 < r * r) {
            hit = true;
            if (d2 > 1e-6f) {
              const float d = std::sqrt(d2);
              ex += dx / d * (r - d); ey += dy / d * (r - d);
            } else {
              // Centro dentro de la caja: sal por el lado más cercano.
              const float l = ex - bx0, rr = bx1 - ex, u = ey - by0, b = by1 - ey;
              const float m = std::fmin(std::fmin(l, rr), std::fmin(u, b));
              if (m == l) ex = bx0 - r; else if (m == rr) ex = bx1 + r;
              else if (m == u) ey = by0 - r; else ey = by1 + r;
            }
          }
        }
        const Prop* p = prop_at_tile(tx, ty);
        if (p && p->solid) {
          const float dx = ex - p->x, dy = ey - p->y;
          const float min = r + p->r, d2 = dx * dx + dy * dy;
          if (d2 < min * min) {
            hit = true;
            const float d = d2 > 0 ? std::sqrt(d2) : 0.01f;
            ex = p->x + dx / d * min; ey = p->y + dy / d * min;
          }
        }
      }
  }
  return hit;
}

int World::projectile_block(float x, float y, float r, Prop** prop) {
  const int32_t tx = tile_of(x), ty = tile_of(y);
  if (terrain_at(tx, ty) == TR_WALL) return 1;
  for (int b = -1; b <= 1; ++b)
    for (int a = -1; a <= 1; ++a) {
      Prop* p = prop_at_tile(tx + a, ty + b);
      if (p && p->solid && dist2(x, y, p->x, p->y) < (r + p->r) * (r + p->r)) { if (prop) *prop = p; return 2; }
    }
  return 0;
}

// ---------------- daño a objetos ----------------

bool World::damage_prop(Prop* p, float amount, uint32_t color) {
  if (!p->destructible || p->hp <= 0) return false;
  p->hp -= amount;
  p->flash = 0.08f;
  if (p->type != PR_GRASS && game_) game_->fx.spark(p->x, p->y - 10, color, 0, -1, 3);
  if (p->hp <= 0) { destroy_prop(p); return true; }
  return false;
}

bool World::is_broken(int32_t tx, int32_t ty) const {
  const uint64_t k = tkey(tx, ty);
  uint32_t i = uint32_t((k * 0x9E3779B97F4A7C15ull) >> 49) & (kBrokenCap - 1);
  for (int n = 0; n < kBrokenCap; ++n) {
    if (broken_[i] == kEmpty) return false;
    if (broken_[i] == k) return true;
    i = (i + 1) & (kBrokenCap - 1);
  }
  return false;
}

void World::mark_broken(int32_t tx, int32_t ty) {
  if (nbroken_ >= kBrokenCap * 3 / 4) return;   // tope (nunca se llega en una partida)
  const uint64_t k = tkey(tx, ty);
  uint32_t i = uint32_t((k * 0x9E3779B97F4A7C15ull) >> 49) & (kBrokenCap - 1);
  while (broken_[i] != kEmpty) {
    if (broken_[i] == k) return;
    i = (i + 1) & (kBrokenCap - 1);
  }
  broken_[i] = k;
  nbroken_++;
}

void World::destroy_prop(Prop* p) {
  mark_broken(p->tx, p->ty);
  p->gone = true;
  p->hp = std::fmin(p->hp, 0.0f);
  const int32_t cx = int32_t(std::floor(p->tx / double(kChunkTiles))), cy = int32_t(std::floor(p->ty / double(kChunkTiles)));
  Chunk& c = chunk(cx, cy);
  int16_t& at = c.prop_at[(p->ty - cy * kChunkTiles) * kChunkTiles + (p->tx - cx * kChunkTiles)];
  if (at >= 0 && &c.props[at] == p) at = -1;
  if (game_) game_->on_prop_destroyed(*p);
}

void World::update(float dt, float cam_l, float cam_t, float cam_w, float cam_h) {
  if (fade_t_ >= 0 && (fade_t_ += dt) >= 2.6f) fade_t_ = -1;
  const float pad = 60;
  for_props_in_rect(cam_l - pad, cam_t - pad, cam_l + cam_w + pad, cam_t + cam_h + pad, [&](Prop& p) {
    if (p.flash > 0) p.flash -= dt;
  });
}

}  // namespace ps

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
  {true, 26, 11, false},     // pumpkin (Halloween)
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
  nmods_ = mod_next_ = 0;
  fade_t_ = -1;
  arena_ = ArenaInfo();
  biome_gen_++;
}

void World::set_halloween(bool on) {
  if (on == halloween_) return;
  halloween_ = on;
  for (auto& c : cache_) c.valid = false;   // (sus props se vuelven a colocar)
  last_ = nullptr;
  terrain_gen_++;
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

void World::set_arena(bool on, int32_t tx, int32_t ty, float r, int theme, bool bare) {
  terrain_gen_++;   // (el terreno cambia: lo pintado de antes ya no vale)
  if (!on) {
    // Fuera la arena: también se olvidan sus chunks (si no, sus paredes seguían).
    const ArenaInfo a = arena_;
    arena_ = ArenaInfo();
    if (a.on)
      for (auto& c : cache_) {
        if (!c.valid) continue;
        if (std::fabs(float(c.cx * kChunkTiles + kChunkTiles / 2 - a.tx)) < a.r + 9 + kChunkTiles &&
            std::fabs(float(c.cy * kChunkTiles + kChunkTiles / 2 - a.ty)) < a.r + 9 + kChunkTiles) {
          save_mods(c);
          c.valid = false;
          if (last_ == &c) last_ = nullptr;
        }
      }
    return;
  }
  arena_.on = true; arena_.tx = tx; arena_.ty = ty; arena_.r = r; arena_.theme = theme; arena_.bare = bare;
  // Olvida los chunks de esa zona (si se hubieran generado antes).
  const float R = r + 9;
  for (auto& c : cache_) {
    if (!c.valid) continue;
    if (std::fabs(float(c.cx * kChunkTiles + kChunkTiles / 2 - tx)) < R + kChunkTiles &&
        std::fabs(float(c.cy * kChunkTiles + kChunkTiles / 2 - ty)) < R + kChunkTiles) {
      save_mods(c);
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
  // No está: se genera en el hueco libre o en el más viejo (sus cambios se
  // guardan aparte y vuelven al generarlo otra vez: el estado del mundo no
  // depende de qué chunks se han mirado, y cada consola del grupo dibuja una zona).
  for (auto& c : cache_) {
    if (!c.valid) { victim = &c; break; }
    if (!victim || c.stamp < victim->stamp) victim = &c;
  }
  if (victim->valid) save_mods(*victim);
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
  apply_mods(c);
  return c;
}

bool World::prop_modified(const Prop& p) {
  return !p.gone && (p.used || p.charge < 1 || p.open > 0 || (p.destructible && p.hp < p.max_hp));
}

static inline bool in_chunk(int32_t tx, int32_t ty, int32_t cx, int32_t cy) {
  return tx >= cx * kChunkTiles && tx < (cx + 1) * kChunkTiles && ty >= cy * kChunkTiles && ty < (cy + 1) * kChunkTiles;
}

void World::save_mods(const Chunk& c) {
  // Fuera lo apuntado de este chunk (pudo recuperarse) y se apunta lo de ahora.
  for (int i = 0; i < nmods_; ++i)
    if (mods_[i].type >= 0 && in_chunk(mods_[i].tx, mods_[i].ty, c.cx, c.cy)) mods_[i].type = -1;
  for (int i = 0; i < c.nprops; ++i) {
    const Prop& p = c.props[i];
    if (!prop_modified(p)) continue;
    int k = -1;
    for (int j = 0; j < nmods_ && k < 0; ++j) if (mods_[j].type < 0) k = j;   // (un hueco libre)
    if (k < 0 && nmods_ < kModCap) k = nmods_++;
    if (k < 0) { k = mod_next_; mod_next_ = (mod_next_ + 1) % kModCap; }
    mods_[k] = PropMod{p.tx, p.ty, p.type, p.used, p.hp, p.charge, p.open};
  }
}

void World::apply_mods(Chunk& c) {
  for (int i = 0; i < nmods_; ++i) {
    const PropMod& m = mods_[i];
    if (m.type < 0 || !in_chunk(m.tx, m.ty, c.cx, c.cy)) continue;
    for (int k = 0; k < c.nprops; ++k) {
      Prop& p = c.props[k];
      if (p.tx != m.tx || p.ty != m.ty || p.type != m.type || p.gone) continue;
      p.used = m.used; p.hp = m.hp; p.charge = m.charge; p.open = m.open;
      break;
    }
  }
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
  // El terreno del chunk con un borde de 1 casilla, calculado una vez: la
  // comprobación de «suelo alrededor» de cada casilla miraba 9 vecinas con
  // ruido cada vez (lo más caro de generar un chunk; al abrirse la arena de la
  // Sentencia se generan muchos de golpe y el juego daba un tirón).
  constexpr int R = kChunkTiles + 2;
  uint8_t ring[R * R];
  for (int b = -1; b <= kChunkTiles; ++b)
    for (int a = -1; a <= kChunkTiles; ++a)
      ring[(b + 1) * R + (a + 1)] = (a >= 0 && b >= 0 && a < kChunkTiles && b < kChunkTiles)
                                        ? c.terr[b * kChunkTiles + a]
                                        : terrain_raw(c.cx * kChunkTiles + a, c.cy * kChunkTiles + b);
  for (int j = 0; j < kChunkTiles; ++j)
    for (int i = 0; i < kChunkTiles; ++i) {
      if (c.terr[j * kChunkTiles + i] != TR_FLOOR) continue;
      const int32_t tx = c.cx * kChunkTiles + i, ty = c.cy * kChunkTiles + j;
      // (Halloween, en el fondo del menú: también calabazas en el claro del centro, que es donde mira la cámara)
      const bool menu_hw = halloween_ && seed_ == kMenuSeed;
      if (std::hypot(double(tx), double(ty)) < 7 && !menu_hw) continue;
      if (is_broken(tx, ty)) continue;

      // Necesita suelo alrededor para no pegarse a paredes / agua.
      bool open = true;
      for (int b = -1; b <= 1 && open; ++b)
        for (int a = -1; a <= 1; ++a)
          if (ring[(j + b + 1) * R + (i + a + 1)] != TR_FLOOR) { open = false; break; }
      if (std::hypot(double(tx), double(ty)) < 7) {   // (sólo llega aquí el menú de Halloween)
        if (open && H(tx, ty, seed_ + 404) > 0.96) make_prop(c, PR_PUMPKIN, tx, ty);
        continue;
      }

      // Dentro de la arena: la decoración de su tipo (y nada en el pasillo).
      const double ad = arena_dist(tx, ty);
      if (ad < arena_outer() + 2) {
        if (!arena_.on || ad >= arena_.r - 1.2 || arena_safe(tx, ty) || arena_.bare) continue;   // (la Sala del Origen: limpia)
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
      // Halloween: calabazas por todo el mapa (mismo sitio para todos: va con la semilla).
      else if (open && halloween_ && h > (menu_hw ? 0.955 : 0.972) && h <= 0.985) type = PR_PUMPKIN;
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
    if (p.type < PR_ALTAR || p.type > PR_TRAP) return;   // altar, spring, bigchest, trap
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
    if (!list[i]->solid || list[i]->type == PR_CHEST) continue;   // (los cofres pequeños se abren al pasar, no a golpes)
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

int World::projectile_block(float x, float y, float r, Prop** prop, bool through_chests) {
  const int32_t tx = tile_of(x), ty = tile_of(y);
  if (terrain_at(tx, ty) == TR_WALL) return 1;
  for (int b = -1; b <= 1; ++b)
    for (int a = -1; a <= 1; ++a) {
      Prop* p = prop_at_tile(tx + a, ty + b);
      if (!p || !p->solid) continue;
      if (through_chests && (p->type == PR_CHEST || p->type == PR_BIGCHEST)) continue;   // (del port: tus ataques los atraviesan)
      if (dist2(x, y, p->x, p->y) < (r + p->r) * (r + p->r)) { if (prop) *prop = p; return 2; }
    }
  return 0;
}

// ---------------- daño a objetos ----------------

bool World::damage_prop(Prop* p, float amount, uint32_t color) {
  if (!p->destructible || p->hp <= 0) return false;
  if (p->type == PR_CHEST) return false;   // (del port, 2026-10-30: se abre al pasar, no a golpes; interact.cpp)
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

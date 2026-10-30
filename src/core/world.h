// world.h — arena abierta infinita con obstáculos (world.js de la web).
//
// Terreno por casilla, determinista (ruido de valor con semilla): suelo, pared
// (rocas: bloquean a quien camina y a los proyectiles) y líquido (agua, lava...).
// Objetos del escenario (props): árbol, roca, cofre, hierba, altar, manantial,
// cofre con candado y trampa. Los datos de cada chunk (16×16 casillas) se
// generan al pedirlos y se guardan en una caché de tamaño fijo.
//
// Coordenadas: (x, y) de una entidad = sus PIES, en el plano del suelo.
// Una casilla mide 32 unidades (16 px de arte × 2).
#pragma once
#include <cmath>
#include <cstdint>

#include "core/gamedata.h"

namespace ps {

struct Game;

constexpr int kTileArt = 16;               // px de arte por casilla (G.Tiles.T)
constexpr int kChunkTiles = 16;            // casillas por chunk (N)
constexpr float kTS = kTileArt * 2.0f;     // casilla en mundo (32)
constexpr float kCW = kChunkTiles * kTS;   // chunk en mundo (512)

enum Terrain : uint8_t { TR_FLOOR = 0, TR_WALL = 1, TR_LIQUID = 2 };
enum PropType : int8_t {
  PR_TREE, PR_ROCK, PR_CHEST, PR_GRASS, PR_ALTAR, PR_SPRING, PR_BIGCHEST, PR_TRAP,
  PR_PUMPKIN,   // Halloween (world.js de la web, 25-10-2026): se rompe a golpes; experiencia, monedas y caramelos
  PR_COUNT
};

struct Prop {
  int32_t id;
  int32_t tx, ty;
  float x, y, r;
  float hp, max_hp;
  float flash, wiggle;
  float charge, open;      // manantial / cofre con candado
  int8_t type, kind;
  bool solid, flat, destructible;
  bool used;               // altar
  bool gone;               // roto (el hueco se queda en el chunk)
};

constexpr int kMaxPropsPerChunk = 192;
struct Chunk {
  int32_t cx, cy;
  uint32_t stamp;          // para la caché (el más viejo se va)
  bool valid;
  uint8_t terr[kChunkTiles * kChunkTiles];
  int16_t prop_at[kChunkTiles * kChunkTiles];   // índice del prop SÓLIDO de la casilla o -1
  int16_t nprops;
  Prop props[kMaxPropsPerChunk];
};

// Efecto de un charco sobre un Pokémon (LIQ de world.js).
struct LiquidFx {
  float slow = 0;     // 0 = no frena
  float dmg = 0;      // % de vida máxima
  float flat = 0;
  float every = 0;
  uint32_t color = 0;
  bool slide = false;
  bool none() const { return slow == 0 && dmg == 0 && !slide; }
};

// Arena de la grieta (o de la Sentencia): zona redonda cerrada por paredes.
struct ArenaInfo {
  bool on = false;
  int32_t tx = 0, ty = 0;
  float r = 0;
  int theme = 0;           // índice en kArenaThemes
};

class World {
 public:
  static constexpr int kCacheChunks = 160;

  void init(Game* g) { game_ = g; }
  void reset(int32_t seed);
  void set_biome(int i, bool instant = false);
  int biome() const { return biome_; }
  const char* biome_name() const { return kBiomes[biome_].name; }
  int32_t seed() const { return seed_; }
  // Halloween (world.js de la web): calabazas por el mapa. Va con la partida (o el
  // menú); cambiarlo vacía los chunks (los props se colocan al generarlos).
  static constexpr int32_t kMenuSeed = 4242;   // el mundo del fondo de los menús
  void set_halloween(bool on);
  bool halloween() const { return halloween_; }
  void set_arena(bool on, int32_t tx = 0, int32_t ty = 0, float r = 0, int theme = 0);
  const ArenaInfo& arena() const { return arena_; }

  // --- generación (determinista, igual que la web) ---
  double vnoise(double x, double y, int32_t s) const;
  double fbm(double x, double y, int32_t s) const;
  uint8_t terrain_raw(int32_t tx, int32_t ty) const;
  uint8_t terrain_at(int32_t tx, int32_t ty);
  Chunk& chunk(int32_t cx, int32_t cy);

  // --- consultas ---
  static int32_t tile_of(float v) { return int32_t(std::floor(v / kTS)); }
  bool is_wall(float x, float y) { return terrain_at(tile_of(x), tile_of(y)) == TR_WALL; }
  bool is_liquid(float x, float y) { return terrain_at(tile_of(x), tile_of(y)) == TR_LIQUID; }
  int liquid_kind() const { return kBiomes[biome_].liquid; }
  LiquidFx liquid_effect(const int8_t types[2]) const;
  Prop* prop_at_tile(int32_t tx, int32_t ty);
  bool walkable(int32_t tx, int32_t ty);
  bool is_free(float x, float y, float r = 12, bool allow_liquid = false);
  // Objetos destructibles vivos que tocan el círculo. Devuelve cuántos.
  int props_in_circle(float x, float y, float r, Prop** out, int max);
  int specials_near(float x, float y, float r, Prop** out, int max);
  Prop* nearest_breakable(float x, float y, float r);
  // Empuja un círculo fuera de paredes y objetos sólidos; true si ha chocado.
  bool collide(float& x, float& y, float r);
  // 0 = nada, 1 = pared, 2 = prop sólido (en *prop)
  int projectile_block(float x, float y, float r, Prop** prop, bool through_chests = false);

  // --- daño a objetos (los efectos van a Game) ---
  bool damage_prop(Prop* p, float amount, uint32_t color);
  void break_prop(Prop* p) { if (!is_broken(p->tx, p->ty)) destroy_prop(p); }

  // Recorre los props de los chunks que tocan el rectángulo (para dibujar e
  // interactuar). fn(Prop&) → nada.
  template <class F> void for_props_in_rect(float x0, float y0, float x1, float y1, F fn) {
    const int32_t c0x = int32_t(std::floor(x0 / kCW)), c1x = int32_t(std::floor(x1 / kCW));
    const int32_t c0y = int32_t(std::floor(y0 / kCW)), c1y = int32_t(std::floor(y1 / kCW));
    for (int32_t cy = c0y; cy <= c1y; ++cy)
      for (int32_t cx = c0x; cx <= c1x; ++cx) {
        Chunk& c = chunk(cx, cy);
        for (int i = 0; i < c.nprops; ++i) if (!c.props[i].gone) fn(c.props[i]);
      }
  }

  void update(float dt, float cam_l, float cam_t, float cam_w, float cam_h);

  // Fundido de bioma: 0..1 o -1 si no hay.
  float fade_k() const;
  int fade_from() const { return fade_from_; }
  uint32_t biome_gen() const { return biome_gen_; }   // cambia al cambiar de bioma
  uint32_t terrain_gen() const { return terrain_gen_; }   // cambia si el terreno cambia (arenas)

  int chunks_generated() const { return generated_; }

 private:
  void place_props(Chunk& c);
  bool arena_safe(int32_t tx, int32_t ty) const;
  double arena_dist(int32_t tx, int32_t ty) const;
  float arena_outer() const { return arena_.on ? arena_.r + 7 : 0; }
  void make_prop(Chunk& c, int type, int32_t tx, int32_t ty, int kind = 0);
  static bool prop_modified(const Prop& p);
  void save_mods(const Chunk& c);    // (antes de descartar un chunk)
  void apply_mods(Chunk& c);         // (al generarlo otra vez)
  void destroy_prop(Prop* p);
  bool is_broken(int32_t tx, int32_t ty) const;
  void mark_broken(int32_t tx, int32_t ty);

  Game* game_ = nullptr;
  int32_t seed_ = 1;
  bool halloween_ = false;
  int biome_ = 0;
  ArenaInfo arena_;
  float fade_t_ = -1;
  int fade_from_ = 0;
  uint32_t biome_gen_ = 0;
  uint32_t terrain_gen_ = 0;
  uint32_t stamp_ = 0;
  int generated_ = 0;
  Chunk* last_ = nullptr;
  Chunk cache_[kCacheChunks];
  // Props rotos: casillas "tx,ty" (tabla hash de tamaño fijo).
  static constexpr int kBrokenCap = 1 << 15;
  uint64_t broken_[kBrokenCap];
  int nbroken_ = 0;
  // Props cambiados de los chunks que han salido de la caché (roca golpeada,
  // altar usado, manantial gastado, cofre a medio abrir): al volver a
  // generarse el chunk, se les devuelve su estado. Antes esos chunks no se
  // podían descartar y, tras unos minutos rompiendo cosas por el mapa, la
  // caché se llenaba de ellos y el juego regeneraba chunks sin parar en cada
  // fotograma (en la consola, de 60 a 4 FPS hacia el minuto 8).
  struct PropMod { int32_t tx, ty; int8_t type; bool used; float hp, charge, open; };
  static constexpr int kModCap = 4096;
  PropMod mods_[kModCap];
  int nmods_ = 0, mod_next_ = 0;   // (lleno: se pisa el más antiguo, igual en todas las consolas)
};

}  // namespace ps

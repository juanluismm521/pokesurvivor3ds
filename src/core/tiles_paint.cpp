// tiles_paint.cpp — paintChunk, paintFloor, paintWall y paintLiquid de tiles.js,
// línea a línea. Todo en double, como en JS, para que cada píxel salga igual.
#include "core/tiles_paint.h"

#include <cmath>

#include "core/gamedata.h"
#include "core/util.h"
#include "core/world.h"

namespace ps {

namespace {
constexpr int T = 16;
constexpr int N = 16;
constexpr int W = N * T;

inline double H(int32_t x, int32_t y, int32_t s) { return hash(x, y, s); }

uint32_t mix_pack(uint32_t h1, uint32_t h2, double t) {
  const int a[3] = {int(h1 >> 16) & 255, int(h1 >> 8) & 255, int(h1) & 255};
  const int b[3] = {int(h2 >> 16) & 255, int(h2 >> 8) & 255, int(h2) & 255};
  int c[3];
  for (int i = 0; i < 3; ++i) c[i] = int(jsround(a[i] + (b[i] - a[i]) * t));
  return (255u << 24) | (uint32_t(c[2]) << 16) | (uint32_t(c[1]) << 8) | uint32_t(c[0]);
}

struct ChunkPal {
  uint32_t floor[3], dirt[3], wallTop[3], wallFace[2], flowers[3];
  uint32_t speck, light, wallLight, wallEdge, wallFaceDark, liq, liqDeep, foam, shore, liqLight;
};

ChunkPal make_pal(int bi) {
  const Biome& B = kBiomes[bi];
  ChunkPal C;
  for (int i = 0; i < 3; ++i) { C.floor[i] = pack_rgb(B.floor[i]); C.dirt[i] = pack_rgb(B.dirt[i]); C.wallTop[i] = pack_rgb(B.wallTop[i]); C.flowers[i] = pack_rgb(B.flowers[i]); }
  for (int i = 0; i < 2; ++i) C.wallFace[i] = pack_rgb(B.wallFace[i]);
  C.speck = pack_rgb(B.speck); C.light = pack_rgb(B.light); C.wallLight = pack_rgb(B.wallLight);
  C.wallEdge = pack_rgb(B.wallEdge); C.wallFaceDark = pack_rgb(B.wallFaceDark);
  C.liq = pack_rgb(B.liq); C.liqDeep = pack_rgb(B.liqDeep); C.foam = pack_rgb(B.foam); C.shore = pack_rgb(B.shore);
  C.liqLight = mix_pack(B.liq, B.foam, 0.28);
  return C;
}

struct Canvas {
  uint32_t* buf;
  void set(int x, int y, uint32_t c) { if (x >= 0 && y >= 0 && x < W && y < W) buf[y * W + x] = c; }
};
}  // namespace

void ChunkPainter::begin(World& world, int32_t x, int32_t y, int bi) {
  w = &world; cx = x; cy = y; biome = bi;
  const int32_t tx0 = cx * N, ty0 = cy * N;
  for (int j = -1; j <= N; ++j)
    for (int i = -1; i <= N; ++i) terr[(j + 1) * 18 + (i + 1)] = world.terrain_at(tx0 + i, ty0 + j);
}

bool ChunkPainter::paint_rows(int row0, int row1, uint32_t* out) {
  World& world = *w;
  const int32_t seed = world.seed();
  const int32_t tx0 = cx * N, ty0 = cy * N;
  const int32_t gx0 = tx0 * T, gy0 = ty0 * T;
  // Máscara de tierra: el ruido se evalúa cada 4 px y se interpola.
  if (row0 == 0) {
    const int S = 4, gw = W / S + 3;
    static double grid[(W / 4 + 3) * (W / 4 + 3)];
    const double S6 = 6.0 * T;
    for (int j = 0; j < gw; ++j)
      for (int i = 0; i < gw; ++i)
        grid[j * gw + i] = world.fbm((gx0 + (i - 1) * S) / S6, (gy0 + (j - 1) * S) / S6, seed + 303);
    for (int yy = -1; yy < W; ++yy) {
      const double fy = double(yy + S) / S;
      const int j = int(std::floor(fy));
      const double v = fy - j;
      for (int xx = 0; xx < W; ++xx) {
        const double fx = double(xx + S) / S;
        const int i = int(std::floor(fx));
        const double u = fx - i;
        const double a = grid[j * gw + i], b = grid[j * gw + i + 1];
        const double c = grid[(j + 1) * gw + i], d = grid[(j + 1) * gw + i + 1];
        double val = (a + (b - a) * u) * (1 - v) + (c + (d - c) * u) * v;
        val += (H((gx0 + xx) >> 1, (gy0 + yy) >> 1, seed + 9) - 0.5) * 0.05;
        dirt[(yy + 1) * W + xx] = val > 0.64 ? 1 : 0;
      }
    }
  }
  auto dirt_at = [&](int x, int y) { return dirt[(y + 1) * W + x] == 1; };
  auto at = [&](int32_t tx, int32_t ty) { return terr[(ty - ty0 + 1) * 18 + (tx - tx0 + 1)]; };
  const ChunkPal C = make_pal(biome);
  Canvas P{out};

  for (int j = row0; j < row1; ++j) {
    for (int i = 0; i < N; ++i) {
      const int32_t tx = tx0 + i, ty = ty0 + j;
      const int ox = i * T, oy = j * T;
      const uint8_t kind = at(tx, ty);

      // 1) Suelo bajo todo (las esquinas redondeadas de las paredes lo dejan ver).
      {
        int dirt_count = 0;
        for (int y = 0; y < T; ++y)
          for (int x = 0; x < T; ++x) {
            const int32_t gx = tx * T + x, gy = ty * T + y;
            const int lx = ox + x, ly = oy + y;
            const bool d = dirt_at(lx, ly);
            if (d) dirt_count++;
            const uint32_t* pal = d ? C.dirt : C.floor;
            const double band = H(gx >> 3, gy >> 3, 11) * 0.7 + H(gx, gy, 12) * 0.5;
            uint32_t col = pal[std::min(2, int(std::floor(band * 3 / 1.2)))];
            const double r = H(gx, gy, 13);
            if (r < 0.035) col = C.speck;
            else if (r > 0.985) col = C.light;
            // Borde de la tierra: labio oscuro arriba, hierba que la invade abajo.
            if (d && !dirt_at(lx, ly - 1)) col = C.speck;
            else if (!d && dirt_at(lx, ly - 1) && r < 0.5) col = C.light;
            P.set(ox + x, oy + y, col);
          }
        const bool dirt_tile = dirt_count > T * T / 2;
        // Sombra de contacto bajo las paredes que hay justo al norte.
        if (at(tx, ty - 1) == TR_WALL) {
          for (int x = 0; x < T; ++x) { P.set(ox + x, oy, C.wallFaceDark); if ((x + ty) % 2) P.set(ox + x, oy + 1, C.speck); }
        }
        // Detalles: matojo, piedrecita o flor.
        const double dd = H(tx, ty, 14);
        const int dx = ox + 3 + int(std::floor(H(tx, ty, 15) * 10)), dy = oy + 4 + int(std::floor(H(tx, ty, 16) * 9));
        if (dirt_tile) {
          if (dd < 0.22) { P.set(dx, dy, C.light); P.set(dx + 1, dy, C.light); P.set(dx, dy + 1, C.speck); P.set(dx + 1, dy + 1, C.speck); }
        } else if (dd < 0.18) {                         // matojo
          P.set(dx, dy, C.light); P.set(dx - 1, dy - 1, C.light); P.set(dx + 1, dy - 1, C.light);
          P.set(dx, dy - 2, C.light); P.set(dx, dy + 1, C.speck);
        } else if (dd < 0.26) {                         // flor
          const uint32_t fc = C.flowers[int(std::floor(H(tx, ty, 17) * 3))];
          P.set(dx, dy - 1, fc); P.set(dx - 1, dy, fc); P.set(dx + 1, dy, fc); P.set(dx, dy + 1, fc);
          P.set(dx, dy, C.light);
        } else if (dd < 0.32) {                         // piedrecita
          P.set(dx, dy, C.light); P.set(dx + 1, dy, C.light); P.set(dx, dy + 1, C.speck); P.set(dx + 1, dy + 1, C.speck);
        }
      }

      if (kind == TR_WALL) {
        auto Wl = [&](int32_t x, int32_t y) { return at(x, y) == TR_WALL; };
        const bool n = Wl(tx, ty - 1), s = Wl(tx, ty + 1), wv = Wl(tx - 1, ty), e = Wl(tx + 1, ty);
        const int FACE = s ? 0 : 6;
        for (int y = 0; y < T; ++y)
          for (int x = 0; x < T; ++x) {
            // Esquinas redondeadas exteriores: dejan ver el suelo.
            if (!n && !wv && x + y < 3) continue;
            if (!n && !e && (T - 1 - x) + y < 3) continue;
            if (!s && !wv && x + (T - 1 - y) < 2) continue;
            if (!s && !e && (T - 1 - x) + (T - 1 - y) < 2) continue;
            const int32_t gx = tx * T + x, gy = ty * T + y;
            uint32_t col;
            if (y >= T - FACE) {
              // Cara frontal: vetas verticales y base oscura.
              const int fy = y - (T - FACE);
              col = C.wallFace[(x + (H(gx, 0, 21) < 0.5 ? 0 : 1)) % 3 == 0 ? 1 : 0];
              if (fy == 0) col = C.wallEdge;
              else if (y == T - 1) col = C.wallFaceDark;
              else if (H(gx, gy, 22) < 0.08) col = C.wallFaceDark;
            } else {
              // Cara superior con tramado.
              col = C.wallTop[int(std::floor(H(gx >> 1, gy >> 1, 23) * 2.2 + H(gx, gy, 24) * 0.8)) % 3];
              if (H(gx, gy, 25) < 0.05) col = C.wallLight;
              // Bordes: luz al norte, contorno oscuro a los lados.
              if (!n && y == 0) col = C.wallEdge;
              else if (!n && y == 1) col = C.wallLight;
              if (!wv && x == 0) col = C.wallEdge;
              if (!e && x == T - 1) col = C.wallEdge;
            }
            P.set(ox + x, oy + y, col);
          }
      } else if (kind == TR_LIQUID) {
        auto L = [&](int32_t x, int32_t y) { return at(x, y) == TR_LIQUID; };
        const bool n = L(tx, ty - 1), s = L(tx, ty + 1), wv = L(tx - 1, ty), e = L(tx + 1, ty);
        const bool nw = L(tx - 1, ty - 1), ne = L(tx + 1, ty - 1), sw = L(tx - 1, ty + 1), se = L(tx + 1, ty + 1);
        const double BIG = 99;
        for (int y = 0; y < T; ++y)
          for (int x = 0; x < T; ++x) {
            // Esquinas exteriores redondeadas: dejan ver el suelo de debajo.
            if (!n && !wv && x + y < 3) continue;
            if (!n && !e && (T - 1 - x) + y < 3) continue;
            if (!s && !wv && x + (T - 1 - y) < 3) continue;
            if (!s && !e && (T - 1 - x) + (T - 1 - y) < 3) continue;
            // Distancia a la orilla dentro del tile (incluye esquinas interiores).
            double d = std::fmin(std::fmin(n ? BIG : y, s ? BIG : T - 1 - y), std::fmin(wv ? BIG : x, e ? BIG : T - 1 - x));
            if (n && wv && !nw) d = std::fmin(d, std::hypot(double(x), double(y)) - 0.5);
            if (n && e && !ne) d = std::fmin(d, std::hypot(double(T - 1 - x), double(y)) - 0.5);
            if (s && wv && !sw) d = std::fmin(d, std::hypot(double(x), double(T - 1 - y)) - 0.5);
            if (s && e && !se) d = std::fmin(d, std::hypot(double(T - 1 - x), double(T - 1 - y)) - 0.5);
            const int32_t gx = tx * T + x, gy = ty * T + y;
            const double r = H(gx, gy, 31);
            uint32_t col;
            if (d < 1) col = C.shore;                       // labio de tierra
            else if (d < 2) col = C.foam;                   // espuma
            else if (d < 4) col = C.liqLight;               // agua somera
            else if (d < 6) col = r < 0.5 ? C.liq : C.liqLight;
            else col = d < 8 && r < 0.5 ? C.liq : C.liqDeep;
            if (d >= 6 && d < BIG && r < 0.08) col = C.liq;
            // Olas sueltas: rayitas horizontales de 3 px, pocas.
            if (d >= 3 && H(gx >> 2, gy, 33) < 0.045 && (gx & 3) != 3) col = C.liqLight;
            if (d >= 3 && H(gx, gy, 34) < 0.004) col = C.foam;
            P.set(ox + x, oy + y, col);
          }
      }
    }
  }
  return row1 >= N;
}

}  // namespace ps

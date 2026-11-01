// Parches de las actualizaciones (core/delta.h): rehacen la nueva exacta, son
// pequeños si cambia poco y fallan (sin escribir basura de más) si algo no cuadra.
#include <doctest/doctest.h>

#include <cstring>
#include <vector>

#include "core/delta.h"

using namespace ps;

namespace {
std::vector<uint8_t> random_bytes(size_t n, uint32_t seed) {
  std::vector<uint8_t> v(n);
  for (auto& b : v) { seed = seed * 1664525u + 1013904223u; b = uint8_t(seed >> 24); }
  return v;
}

// Aplicar por trozos raros (el parche llega de a 1..7 bytes), como un archivo.
struct Slow { const std::vector<uint8_t>* p; size_t pos; int k; const std::vector<uint8_t>* base; std::vector<uint8_t> out; };
size_t slow_read(void* ud, uint8_t* b, size_t n) {
  Slow& s = *static_cast<Slow*>(ud);
  size_t k = size_t(1 + (s.k++ % 7));
  if (k > n) k = n;
  if (k > s.p->size() - s.pos) k = s.p->size() - s.pos;
  if (k) std::memcpy(b, s.p->data() + s.pos, k);
  s.pos += k;
  return k;
}
bool slow_base(void* ud, uint64_t off, uint8_t* b, size_t n) {
  Slow& s = *static_cast<Slow*>(ud);
  if (off + n > s.base->size()) return false;
  if (n) std::memcpy(b, s.base->data() + off, n);
  return true;
}
bool slow_write(void* ud, const uint8_t* b, size_t n) { static_cast<Slow*>(ud)->out.insert(static_cast<Slow*>(ud)->out.end(), b, b + n); return true; }
DeltaResult apply_slow(const std::vector<uint8_t>& patch, const std::vector<uint8_t>& base, std::vector<uint8_t>& out) {
  Slow s{&patch, 0, 0, &base, {}};
  DeltaIo io;
  io.ud = &s; io.read_patch = slow_read; io.read_base = slow_base; io.write_out = slow_write;
  const DeltaResult r = delta_apply(io, base.size());
  out = s.out;
  return r;
}
}  // namespace

TEST_CASE("parches: rehacen la nueva exacta (cambios, inserciones que desplazan, igual, vacía)") {
  const std::vector<uint8_t> base = random_bytes(300000, 1);
  std::vector<std::vector<uint8_t>> news;
  news.push_back(base);                                   // igual
  std::vector<uint8_t> a = base;                          // unos bytes cambiados
  for (int i = 0; i < 40; ++i) a[size_t(i) * 7001 + 13] ^= 0x5a;
  news.push_back(a);
  std::vector<uint8_t> b = base;                          // insertado al principio (todo se desplaza 3 bytes)
  b.insert(b.begin() + 10, {1, 2, 3});
  news.push_back(b);
  std::vector<uint8_t> c(base.begin() + 1000, base.end()); // quitado el principio y añadido algo nuevo
  const std::vector<uint8_t> extra = random_bytes(5000, 9);
  c.insert(c.begin() + 50000, extra.begin(), extra.end());
  news.push_back(c);
  news.push_back(random_bytes(20000, 77));                // nada en común
  news.push_back({});                                     // vacía
  for (size_t k = 0; k < news.size(); ++k) {
    INFO("caso " << k);
    const std::vector<uint8_t> p = delta_make(base.data(), base.size(), news[k].data(), news[k].size());
    REQUIRE(!p.empty());
    std::vector<uint8_t> out;
    CHECK(delta_apply_mem(p, base.data(), base.size(), out));
    CHECK(out == news[k]);
    CHECK(apply_slow(p, base, out) == DELTA_OK);
    CHECK(out == news[k]);
    if (k <= 3) CHECK(p.size() < 12000);   // (poco que cambia: parche pequeño)
  }
  // Base vacía: todo va dentro.
  const std::vector<uint8_t> none;
  const std::vector<uint8_t> p = delta_make(none.data(), 0, base.data(), base.size());
  std::vector<uint8_t> out;
  CHECK(delta_apply_mem(p, none.data(), 0, out));
  CHECK(out == base);
}

TEST_CASE("parches: si algo no cuadra, fallan (base distinta, cortado, estropeado)") {
  const std::vector<uint8_t> base = random_bytes(200000, 3);
  std::vector<uint8_t> neu = base;
  neu.insert(neu.begin() + 777, {9, 9, 9, 9});
  const std::vector<uint8_t> p = delta_make(base.data(), base.size(), neu.data(), neu.size());
  std::vector<uint8_t> out;
  // Otra base (otro tamaño): ni se empieza.
  std::vector<uint8_t> other(base.begin(), base.end() - 1);
  CHECK(apply_slow(p, other, out) == DELTA_BASE_SIZE);
  CHECK(out.empty());
  // Cortado en cualquier sitio: falla.
  for (size_t cut : {size_t(0), size_t(3), size_t(19), size_t(20), p.size() / 2, p.size() - 1}) {
    INFO("cortado en " << cut);
    std::vector<uint8_t> q(p.begin(), p.begin() + long(cut));
    CHECK(apply_slow(q, base, out) != DELTA_OK);
  }
  // Estropeado: o falla o sale otra cosa (la consola lo descarta con la huella firmada).
  std::vector<uint8_t> q = p;
  q[0] = 'X';
  CHECK(apply_slow(q, base, out) == DELTA_BAD);
  q = p;
  q[q.size() / 2] ^= 0xff;
  CHECK((apply_slow(q, base, out) != DELTA_OK || out != neu));
  // Misma base del mismo tamaño pero con otro contenido: sale otra cosa (y la huella lo ve).
  std::vector<uint8_t> same_size = base;
  same_size[5000] ^= 1;
  if (apply_slow(p, same_size, out) == DELTA_OK) CHECK(out != neu);
}

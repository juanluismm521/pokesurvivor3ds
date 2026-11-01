// delta.cpp — ver delta.h.
#include "core/delta.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <unordered_map>

#include <zlib.h>

namespace ps {

namespace {
constexpr uint8_t kMagic[4] = {'P', 'S', 'D', '1'};
enum : uint8_t { OP_COPY = 0, OP_ADD = 1, OP_END = 2 };
constexpr uint32_t kMul = 0x01000193u;   // hash que rueda (FNV): h = h·M + byte

void put_u64(std::vector<uint8_t>& v, uint64_t x) { for (int i = 0; i < 8; ++i) v.push_back(uint8_t(x >> (8 * i))); }
void put_var(std::vector<uint8_t>& v, uint64_t x) {
  while (x >= 0x80) { v.push_back(uint8_t(x | 0x80)); x >>= 7; }
  v.push_back(uint8_t(x));
}

// Órdenes → deflate.
std::vector<uint8_t> deflate_all(const std::vector<uint8_t>& raw) {
  z_stream z{};
  std::vector<uint8_t> out;
  if (deflateInit(&z, 9) != Z_OK) return out;
  out.resize(deflateBound(&z, uLong(raw.size())) + 64);
  z.next_in = const_cast<Bytef*>(raw.data());
  z.avail_in = uInt(raw.size());
  z.next_out = out.data();
  z.avail_out = uInt(out.size());
  const int r = deflate(&z, Z_FINISH);
  out.resize(r == Z_STREAM_END ? z.total_out : 0);
  deflateEnd(&z);
  return out;
}
}  // namespace

std::vector<uint8_t> delta_make(const uint8_t* base, size_t nbase, const uint8_t* neu, size_t nneu, int block) {
  const size_t B = size_t(block < 8 ? 8 : block);
  uint32_t mul_b = 1;   // M^B (para quitar el byte que sale de la ventana)
  for (size_t i = 0; i < B; ++i) mul_b *= kMul;
  // Índice de la base: el hash de cada trozo alineado a B (el primero que sale).
  std::unordered_map<uint32_t, uint32_t> index;
  if (nbase >= B) {
    index.reserve(nbase / B + 1);
    for (size_t off = 0; off + B <= nbase; off += B) {
      uint32_t h = 0;
      for (size_t i = 0; i < B; ++i) h = h * kMul + base[off + i];
      index.emplace(h, uint32_t(off));
    }
  }
  std::vector<uint8_t> ops;
  size_t lit = 0;        // donde empiezan los bytes nuevos aún sin escribir
  auto flush_add = [&](size_t upto) {
    if (upto <= lit) return;
    ops.push_back(OP_ADD);
    put_var(ops, upto - lit);
    ops.insert(ops.end(), neu + lit, neu + upto);
    lit = upto;
  };
  size_t i = 0;
  uint32_t h = 0;
  bool have = false;     // ¿h es el hash de neu[i, i+B)?
  uint64_t next_base = 0;   // donde acabó la última copia (lo más probable es que siga)
  while (i + B <= nneu) {
    if (!have) { h = 0; for (size_t k = 0; k < B; ++k) h = h * kMul + neu[i + k]; have = true; }
    size_t from = SIZE_MAX;
    // 1. ¿sigue igual que justo después de la última copia?
    if (next_base + B <= nbase && std::memcmp(base + next_base, neu + i, B) == 0) from = size_t(next_base);
    else {
      auto it = index.find(h);
      if (it != index.end() && std::memcmp(base + it->second, neu + i, B) == 0) from = it->second;
    }
    if (from != SIZE_MAX) {
      // Se alarga hacia atrás (sobre los bytes nuevos pendientes) y hacia delante.
      size_t s = i, f = from;
      while (s > lit && f > 0 && neu[s - 1] == base[f - 1]) { --s; --f; }
      size_t len = i - s + B;
      while (s + len < nneu && f + len < nbase && neu[s + len] == base[f + len]) ++len;
      flush_add(s);
      ops.push_back(OP_COPY);
      put_var(ops, f);
      put_var(ops, len);
      lit = s + len;
      next_base = f + len;
      i = lit;
      have = false;
      continue;
    }
    // Un byte más: la ventana rueda.
    if (i + B < nneu) h = h * kMul + neu[i + B] - mul_b * neu[i];
    ++i;
  }
  flush_add(nneu);
  ops.push_back(OP_END);
  std::vector<uint8_t> out(kMagic, kMagic + 4);
  put_u64(out, nbase);
  put_u64(out, nneu);
  const std::vector<uint8_t> z = deflate_all(ops);
  if (z.empty()) return {};
  out.insert(out.end(), z.begin(), z.end());
  return out;
}

namespace {
// Lector de las órdenes: va inflando el parche por trozos.
struct ZReader {
  const DeltaIo& io;
  z_stream z{};
  uint8_t in[16 * 1024];
  uint8_t buf[32 * 1024];
  size_t pos = 0, len = 0;
  bool ended = false, failed = false, ok_init = false;
  explicit ZReader(const DeltaIo& io_) : io(io_) { ok_init = inflateInit(&z) == Z_OK; }
  ~ZReader() { if (ok_init) inflateEnd(&z); }
  bool fill() {
    if (ended || failed) return false;
    z.next_out = buf;
    z.avail_out = sizeof(buf);
    while (z.avail_out == sizeof(buf)) {
      if (z.avail_in == 0) {
        const size_t n = io.read_patch(io.ud, in, sizeof(in));
        if (n == 0) { failed = true; return false; }   // (se ha cortado antes del fin)
        z.next_in = in;
        z.avail_in = uInt(n);
      }
      const int r = inflate(&z, Z_NO_FLUSH);
      if (r == Z_STREAM_END) { ended = true; break; }
      if (r != Z_OK) { failed = true; return false; }
    }
    pos = 0;
    len = sizeof(buf) - z.avail_out;
    return len > 0;
  }
  bool byte(uint8_t& b) {
    if (pos >= len && !fill()) return false;
    b = buf[pos++];
    return true;
  }
  bool var(uint64_t& x) {
    x = 0;
    for (int sh = 0; sh < 64; sh += 7) {
      uint8_t b;
      if (!byte(b)) return false;
      x |= uint64_t(b & 0x7f) << sh;
      if (!(b & 0x80)) return true;
    }
    return false;
  }
};
}  // namespace

DeltaResult delta_apply(const DeltaIo& io, uint64_t base_size) {
  uint8_t head[20];
  size_t got = 0;
  while (got < sizeof(head)) {
    const size_t n = io.read_patch(io.ud, head + got, sizeof(head) - got);
    if (n == 0) return DELTA_BAD;
    got += n;
  }
  if (std::memcmp(head, kMagic, 4) != 0) return DELTA_BAD;
  uint64_t nb = 0, nn = 0;
  for (int i = 0; i < 8; ++i) { nb |= uint64_t(head[4 + i]) << (8 * i); nn |= uint64_t(head[12 + i]) << (8 * i); }
  if (nb != base_size) return DELTA_BASE_SIZE;
  std::unique_ptr<ZReader> zp(new ZReader(io));   // (48 KB: fuera de la pila)
  ZReader& zr = *zp;
  if (!zr.ok_init) return DELTA_IO;
  static uint8_t chunk[64 * 1024];   // (no cabe en la pila de los hilos de la 3DS)
  uint64_t done = 0;
  for (;;) {
    uint8_t op;
    if (!zr.byte(op)) return DELTA_BAD;
    if (op == OP_END) break;
    if (op == OP_COPY) {
      uint64_t off, n;
      if (!zr.var(off) || !zr.var(n) || off > nb || n > nb - off || done + n > nn) return DELTA_BAD;
      while (n) {
        const size_t k = size_t(n < sizeof(chunk) ? n : sizeof(chunk));
        if (!io.read_base(io.ud, off, chunk, k) || !io.write_out(io.ud, chunk, k)) return DELTA_IO;
        off += k; n -= k; done += k;
        if (io.progress) io.progress(io.ud, done, nn);
      }
    } else if (op == OP_ADD) {
      uint64_t n;
      if (!zr.var(n) || done + n > nn) return DELTA_BAD;
      while (n) {
        if (zr.pos >= zr.len && !zr.fill()) return DELTA_BAD;
        const size_t k = size_t(n < zr.len - zr.pos ? n : zr.len - zr.pos);
        if (!io.write_out(io.ud, zr.buf + zr.pos, k)) return DELTA_IO;
        zr.pos += k; n -= k; done += k;
        if (io.progress) io.progress(io.ud, done, nn);
      }
    } else {
      return DELTA_BAD;
    }
  }
  // Después del fin no puede haber nada más, y el deflate tiene que acabar entero
  // (con su suma de comprobación): si no, el parche está cortado o estropeado.
  if (zr.pos < zr.len) return DELTA_BAD;
  while (!zr.ended) {
    if (zr.fill()) return DELTA_BAD;     // (más órdenes después del fin)
    if (!zr.ended) return DELTA_BAD;     // (cortado)
  }
  return done == nn ? DELTA_OK : DELTA_BAD;
}

namespace {
struct MemIo { const std::vector<uint8_t>* patch; size_t pp; const uint8_t* base; size_t nbase; std::vector<uint8_t>* out; };
size_t mem_read_patch(void* ud, uint8_t* b, size_t n) {
  MemIo& m = *static_cast<MemIo*>(ud);
  const size_t k = std::min(n, m.patch->size() - m.pp);
  if (k) std::memcpy(b, m.patch->data() + m.pp, k);
  m.pp += k;
  return k;
}
bool mem_read_base(void* ud, uint64_t off, uint8_t* b, size_t n) {
  MemIo& m = *static_cast<MemIo*>(ud);
  if (off > m.nbase || n > m.nbase - off) return false;
  if (n) std::memcpy(b, m.base + off, n);
  return true;
}
bool mem_write(void* ud, const uint8_t* b, size_t n) {
  static_cast<MemIo*>(ud)->out->insert(static_cast<MemIo*>(ud)->out->end(), b, b + n);
  return true;
}
}  // namespace

bool delta_apply_mem(const std::vector<uint8_t>& patch, const uint8_t* base, size_t nbase, std::vector<uint8_t>& out) {
  out.clear();
  MemIo m{&patch, 0, base, nbase, &out};
  DeltaIo io;
  io.ud = &m;
  io.read_patch = mem_read_patch;
  io.read_base = mem_read_base;
  io.write_out = mem_write;
  return delta_apply(io, nbase) == DELTA_OK;
}

}  // namespace ps

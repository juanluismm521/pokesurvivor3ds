// ps_delta — parches de las actualizaciones (core/delta.h), para el publicador.
//   ps_delta crear BASE NUEVA PARCHE      crea el parche (y lo comprueba aplicándolo)
//   ps_delta aplicar BASE PARCHE SALIDA   lo aplica por trozos, como la consola
#include <cstdio>
#include <cstring>
#include <vector>

#include "core/delta.h"

using namespace ps;

namespace {
bool read_all(const char* p, std::vector<uint8_t>& out) {
  FILE* f = std::fopen(p, "rb");
  if (!f) return false;
  std::fseek(f, 0, SEEK_END);
  const long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  out.resize(size_t(n > 0 ? n : 0));
  const bool ok = n >= 0 && std::fread(out.data(), 1, out.size(), f) == out.size();
  std::fclose(f);
  return ok;
}

struct Files { FILE* patch; FILE* base; FILE* out; };
size_t rp(void* ud, uint8_t* b, size_t n) { return std::fread(b, 1, n, static_cast<Files*>(ud)->patch); }
bool rb(void* ud, uint64_t off, uint8_t* b, size_t n) {
  FILE* f = static_cast<Files*>(ud)->base;
  return std::fseek(f, long(off), SEEK_SET) == 0 && std::fread(b, 1, n, f) == n;
}
bool wo(void* ud, const uint8_t* b, size_t n) { return std::fwrite(b, 1, n, static_cast<Files*>(ud)->out) == n; }

int apply_files(const char* base, const char* patch, const char* out) {
  Files f{std::fopen(patch, "rb"), std::fopen(base, "rb"), std::fopen(out, "wb")};
  if (!f.patch || !f.base || !f.out) { std::fprintf(stderr, "no se pueden abrir los archivos\n"); return 2; }
  std::fseek(f.base, 0, SEEK_END);
  const uint64_t nb = uint64_t(std::ftell(f.base));
  DeltaIo io;
  io.ud = &f; io.read_patch = rp; io.read_base = rb; io.write_out = wo;
  const DeltaResult r = delta_apply(io, nb);
  std::fclose(f.patch); std::fclose(f.base);
  const bool closed = std::fclose(f.out) == 0;
  if (r != DELTA_OK || !closed) { std::fprintf(stderr, "el parche no vale (%d)\n", int(r)); std::remove(out); return 1; }
  return 0;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc == 5 && !std::strcmp(argv[1], "crear")) {
    std::vector<uint8_t> a, b;
    if (!read_all(argv[2], a) || !read_all(argv[3], b)) { std::fprintf(stderr, "no se pueden leer\n"); return 2; }
    const std::vector<uint8_t> p = delta_make(a.data(), a.size(), b.data(), b.size());
    std::vector<uint8_t> back;
    if (p.empty() || !delta_apply_mem(p, a.data(), a.size(), back) || back != b) { std::fprintf(stderr, "el parche sale mal\n"); return 1; }
    FILE* f = std::fopen(argv[4], "wb");
    if (!f || std::fwrite(p.data(), 1, p.size(), f) != p.size() || std::fclose(f) != 0) { std::fprintf(stderr, "no se puede escribir\n"); return 2; }
    std::printf("parche: %zu bytes (la nueva: %zu)\n", p.size(), b.size());
    return 0;
  }
  if (argc == 5 && !std::strcmp(argv[1], "aplicar")) return apply_files(argv[2], argv[3], argv[4]);
  std::fprintf(stderr, "uso: ps_delta crear BASE NUEVA PARCHE | aplicar BASE PARCHE SALIDA\n");
  return 2;
}

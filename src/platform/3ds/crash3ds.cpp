// crash3ds.cpp — ver crash3ds.h.
#include "platform/3ds/crash3ds.h"

#include <3ds.h>

#include <cstdio>
#include <cstring>

#include "core/version.h"
#include "gen/build.h"

namespace crash3ds {
namespace {

char g_dir[64] = "sdmc:/3ds/pokesurvivor-3ds";
char g_last[160] = "";
bool g_booting = true;
// Pila del manejador (una para todos: dos fallos a la vez no se van a dar, y
// con un desbordamiento de pila la del hilo no sirve).
alignas(8) u8 g_stack[16 * 1024];
__thread const char* t_name = "principal";

const char* type_name(int t) {
  switch (t) {
    case ERRF_EXCEPTION_PREFETCH_ABORT: return "prefetch abort";
    case ERRF_EXCEPTION_DATA_ABORT: return "data abort";
    case ERRF_EXCEPTION_UNDEFINED: return "instrucción no válida";
    case ERRF_EXCEPTION_VFP: return "coma flotante (VFP)";
    default: return "?";
  }
}

// Escribe con el FS directamente: si el fallo fue dentro de malloc o de la
// biblioteca de C, fopen podría quedarse bloqueado.
void write_file(const char* sd_path, const char* data, u32 n) {
  Handle h;
  if (R_FAILED(FSUSER_OpenFileDirectly(&h, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, sd_path),
                                       FS_OPEN_WRITE | FS_OPEN_CREATE, 0)))
    return;
  FSFILE_SetSize(h, 0);
  u32 w = 0;
  FSFILE_Write(h, &w, 0, data, n, FS_WRITE_FLUSH);
  FSFILE_Close(h);
}

bool mapped(u32 a) {
  MemInfo mi;
  PageInfo pi;
  return R_SUCCEEDED(svcQueryMemory(&mi, &pi, a)) && mi.state != MEMSTATE_FREE && (mi.perm & MEMPERM_READ);
}

void handler(ERRF_ExceptionInfo* e, CpuRegisters* r) {
  static char b[2048];
  int n = std::snprintf(b, sizeof(b),
                        "PokéSurvivor 3DS %s · build %d\nhilo: %s\nfallo: %s\npc %08lx · lr %08lx · sp %08lx · cpsr %08lx\n"
                        "dirección %08lx · fsr %08lx\n",
                        ps::kVersion, PS_BUILD, t_name, type_name(e->type), (unsigned long)r->pc, (unsigned long)r->lr,
                        (unsigned long)r->sp, (unsigned long)r->cpsr, (unsigned long)e->far, (unsigned long)e->fsr);
  for (int i = 0; i < 13 && n < int(sizeof(b)) - 32; ++i) n += std::snprintf(b + n, sizeof(b) - n, "r%d %08lx%s", i, (unsigned long)r->r[i], i % 4 == 3 ? "\n" : " · ");
  // Direcciones de código en la pila (las llamadas por las que iba).
  n += std::snprintf(b + n, sizeof(b) - n, "\npila:");
  int found = 0;
  if (!mapped(r->sp)) n += std::snprintf(b + n, sizeof(b) - n, " (sin pila: ¿desbordada?)");
  else for (u32 a = r->sp & ~3u; found < 24 && a < r->sp + 4096 && n < int(sizeof(b)) - 16; a += 4) {
    if ((a & 0xfff) == 0 && !mapped(a)) break;
    const u32 v = *reinterpret_cast<const u32*>(a);
    if (v >= 0x00100000 && v < 0x00800000) { n += std::snprintf(b + n, sizeof(b) - n, " %08lx", (unsigned long)v); found++; }
  }
  n += std::snprintf(b + n, sizeof(b) - n, "\n");
  char path[96];
  std::snprintf(path, sizeof(path), "%s/fallo.log", g_dir + 5);   // (sin «sdmc:»)
  write_file(path, b, u32(n));
  svcExitProcess();
}

}  // namespace

void guard(const char* name) {
  t_name = name;
  threadOnException(handler, g_stack + sizeof(g_stack), WRITE_DATA_TO_HANDLER_STACK);
}

void init(const char* dir) {
  std::snprintf(g_dir, sizeof(g_dir), "%s", dir);
  guard("principal");
  // ¿Se rompió la vez anterior?
  char path[96], old[96];
  std::snprintf(path, sizeof(path), "%s/fallo.log", g_dir);
  std::snprintf(old, sizeof(old), "%s/fallo_anterior.log", g_dir);
  if (FILE* f = std::fopen(path, "r")) {
    char line[128];
    int k = 0;
    while (std::fgets(line, sizeof(line), f) && k < 4) {
      line[std::strcspn(line, "\n")] = 0;
      if (k == 0) { const char* p = std::strstr(line, "build"); std::snprintf(g_last, sizeof(g_last), "%s", p ? p : line); }
      else if (k == 3) { const char* p = std::strstr(line, " · sp"); if (p) line[p - line] = 0; std::strncat(g_last, " · ", sizeof(g_last) - std::strlen(g_last) - 1); std::strncat(g_last, line, sizeof(g_last) - std::strlen(g_last) - 1); }
      else { std::strncat(g_last, " · ", sizeof(g_last) - std::strlen(g_last) - 1); std::strncat(g_last, line, sizeof(g_last) - std::strlen(g_last) - 1); }
      k++;
    }
    std::fclose(f);
    std::remove(old);
    std::rename(path, old);
  }
  std::snprintf(path, sizeof(path), "%s/arranque.log", g_dir);
  if (FILE* f = std::fopen(path, "w")) { std::fprintf(f, "PokéSurvivor 3DS %s · build %d\n", ps::kVersion, PS_BUILD); std::fclose(f); }
}

void step(const char* what) {
  if (!g_booting) return;
  char path[96];
  std::snprintf(path, sizeof(path), "%s/arranque.log", g_dir);
  if (FILE* f = std::fopen(path, "a")) { std::fprintf(f, "%llu ms · %s\n", (unsigned long long)osGetTime() % 100000000ull, what); std::fclose(f); }
}

void boot_done() {
  step("en el menú");
  g_booting = false;
}

const char* last() { return g_last; }

}  // namespace crash3ds

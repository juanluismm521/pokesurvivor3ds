// datos3ds.cpp — ver datos3ds.h.
#include "platform/3ds/datos3ds.h"

#include <3ds.h>

#include <cstdio>
#include <string>

#include "core/datapack.h"
#include "core/gamedata.h"
#include "core/json.h"
#include "platform/3ds/cloud3ds.h"

namespace ps {
namespace datos3ds {

namespace {
// Dónde se publican: el servidor del proyecto (tools/web/publicar_datos.sh y
// tools/web/servidor-datos). Sólo se leen; la consola comprueba cada paquete.
const char kBaseDefault[] = "https://pokesurvivor3ds.duckdns.org/datos/";
std::string g_base = kBaseDefault;   // (pruebas: datos_url.txt en la carpeta del juego la cambia)

std::string g_dir;
Thread g_thr = nullptr;
volatile bool g_busy = false, g_ready = false, g_newer = false;
std::string g_text;               // paquete bajado y comprobado (esperando a aplicarse)
std::string g_status = "Datos de fábrica";
LightLock g_lock;

bool read_file(const std::string& p, std::string& out) {
  FILE* f = std::fopen(p.c_str(), "rb");
  if (!f) return false;
  out.clear();
  char b[16384];
  size_t n;
  while ((n = std::fread(b, 1, sizeof(b), f)) > 0) out.append(b, n);
  std::fclose(f);
  return true;
}
bool write_file(const std::string& p, const std::string& s) {
  const std::string tmp = p + ".tmp";
  FILE* f = std::fopen(tmp.c_str(), "wb");
  if (!f) return false;
  const bool ok = std::fwrite(s.data(), 1, s.size(), f) == s.size();
  std::fclose(f);
  if (!ok) { std::remove(tmp.c_str()); return false; }
  std::remove(p.c_str());
  return std::rename(tmp.c_str(), p.c_str()) == 0;
}
void set_status(const std::string& s) { LightLock_Lock(&g_lock); g_status = s; LightLock_Unlock(&g_lock); }

std::string describe(const DataPackInfo& i) { return "web del " + i.fecha + " (" + i.web + ")"; }

void worker(void*) {
  std::string body, err;
  int st = 0;
  // ¿Pide la web ya un programa nuevo? (el índice dice el esquema más reciente)
  if (cloud3ds::http_get(g_base + "ultimo.json", body, st, err)) {
    Json idx;
    if (idx.parse(body) && idx["esquema"].is_str() && idx["esquema"].as_str() != kDataSchema) g_newer = true;
  }
  // El paquete de esta versión del programa.
  body.clear();
  if (!cloud3ds::http_get(g_base + kDataSchema + ".json", body, st, err)) {
    set_status(st == 404 ? "Sin datos nuevos para esta versión" : st == 0 ? "Sin conexión: datos guardados" : "No se han podido bajar los datos");
    g_busy = false;
    return;
  }
  std::string why;
  DataPackInfo info;
  if (!datapack_check(body, why, &info)) { set_status("Paquete descartado: " + why); g_busy = false; return; }
  if (info.huella == kDataHash) { set_status("Datos al día"); g_busy = false; return; }
  LightLock_Lock(&g_lock);
  g_text.swap(body);
  g_ready = true;
  LightLock_Unlock(&g_lock);
  set_status("Datos nuevos listos: " + describe(info));
  g_busy = false;
}
}  // namespace

std::string init(const char* dir) {
  LightLock_Init(&g_lock);
  g_dir = dir;
  std::string text, why;
  if (read_file(g_dir + "/datos_url.txt", text)) {
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) text.pop_back();
    if (text.compare(0, 8, "https://") == 0 && text.size() < 200) g_base = text;
  }
  if (!read_file(g_dir + "/datos.json", text)) return "datos de fábrica";
  if (!datapack_apply(text, why)) {
    // De otra versión del programa (o estropeado): se quita para no volver a leerlo.
    std::remove((g_dir + "/datos.json").c_str());
    return "datos de fábrica (el paquete de la SD no vale: " + why + ")";
  }
  g_status = "Datos de la " + describe(datapack_current());
  return g_status;
}

void start_check() {
  if (g_busy || g_thr) return;
  g_busy = true;
  set_status("Buscando datos nuevos…");
  s32 prio = 0x30;
  svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
  g_thr = threadCreate(worker, nullptr, 128 * 1024, prio + 2, -2, false);
  if (!g_thr) { g_busy = false; set_status("No se ha podido buscar"); }
}

bool busy() { return g_busy; }
bool pending() { return g_ready; }
bool newer_program() { return g_newer; }

bool apply_pending(std::string& msg) {
  if (!g_ready) return false;
  LightLock_Lock(&g_lock);
  std::string text;
  text.swap(g_text);
  g_ready = false;
  LightLock_Unlock(&g_lock);
  std::string why;
  if (!datapack_apply(text, why)) { msg = "Datos nuevos descartados: " + why; set_status(msg); return false; }
  write_file(g_dir + "/datos.json", text);   // para la próxima vez (también sin conexión)
  msg = "Datos actualizados: " + describe(datapack_current());
  set_status("Datos de la " + describe(datapack_current()));
  return true;
}

std::string status() {
  LightLock_Lock(&g_lock);
  std::string s = g_status;
  LightLock_Unlock(&g_lock);
  return s;
}

void shutdown() {
  if (g_thr) { threadJoin(g_thr, 30000000000ull); threadFree(g_thr); g_thr = nullptr; }
}

}  // namespace datos3ds
}  // namespace ps

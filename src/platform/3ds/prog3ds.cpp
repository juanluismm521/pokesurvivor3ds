// prog3ds.cpp — ver prog3ds.h.
#include "platform/3ds/crash3ds.h"
#include "platform/3ds/prog3ds.h"

#include <3ds.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "core/json.h"
#include "gen/build.h"
#include "platform/3ds/cloud3ds.h"
#include "platform/3ds/datos3ds.h"

namespace ps {
namespace prog3ds {

namespace {
std::string g_dir, g_self;     // carpeta del juego · el .3dsx que se está usando ("" si es el .cia)
bool g_cia = false;
LightLock g_lock;
Thread g_thr = nullptr;
volatile int g_state = ST_IDLE, g_pct = 0;
volatile bool g_cancel = false, g_busy = false;
std::string g_status = "Sin buscar todavía";
UpdateEntry g_latest, g_prev;
// Lo que se recuerda en la SD (programa.json).
int g_decided = 0;             // última publicación instalada o descartada (volver atrás)
struct Pending { bool on = false; bool back = false; std::string file; int serie = 0, build = 0; } g_pend;
// Trabajo del hilo: 1 buscar, 2 buscar y descargar, 3 volver atrás.
int g_job = 0;

std::string path(const char* f) { return g_dir + "/" + f; }
bool exists(const std::string& p) { FILE* f = std::fopen(p.c_str(), "rb"); if (!f) return false; std::fclose(f); return true; }
void set_status(const std::string& s) { LightLock_Lock(&g_lock); g_status = s; LightLock_Unlock(&g_lock); }

void save_state() {
  Json o = Json::object();
  o.set("decidida", Json::num(g_decided));
  if (g_pend.on) {
    Json p = Json::object();
    p.set("archivo", Json::str(g_pend.file));
    p.set("serie", Json::num(g_pend.serie));
    p.set("build", Json::num(g_pend.build));
    p.set("vuelta", Json::boolean(g_pend.back));
    o.set("pendiente", p);
  }
  const std::string t = o.dump(), tmp = path("programa.tmp"), cur = path("programa.json");
  if (FILE* f = std::fopen(tmp.c_str(), "wb")) {
    std::fwrite(t.data(), 1, t.size(), f);
    std::fclose(f);
    std::remove(cur.c_str());
    std::rename(tmp.c_str(), cur.c_str());
  }
}

void load_state() {
  FILE* f = std::fopen(path("programa.json").c_str(), "rb");
  if (!f) return;
  std::string t;
  char b[1024];
  size_t n;
  while ((n = std::fread(b, 1, sizeof(b), f)) > 0) t.append(b, n);
  std::fclose(f);
  Json o;
  if (!o.parse(t) || !o.is_obj()) return;
  g_decided = int(o["decidida"].as_num(0));
  const Json& p = o["pendiente"];
  if (p.is_obj() && p["archivo"].is_str()) {
    g_pend.on = true;
    g_pend.file = p["archivo"].as_str();
    g_pend.serie = int(p["serie"].as_num(0));
    g_pend.build = int(p["build"].as_num(0));
    g_pend.back = p["vuelta"].as_bool();
    // (sólo nombres de la carpeta del juego, y que el archivo siga ahí)
    if (g_pend.file.find('/') != std::string::npos || !exists(path(g_pend.file.c_str()))) g_pend = Pending();
  }
}

const char* ext() { return g_cia ? "cia" : "3dsx"; }
const UpdateFile& file_of(const UpdateEntry& e) { return g_cia ? e.cia : e.f3dsx; }

bool fetch_manifest(std::string& why) {
  std::string body, err;
  int st = 0;
  // (el DNS de la consola a veces falla justo al encender: unos reintentos)
  bool ok = false;
  for (int k = 0; k < 4 && !ok && !g_cancel; ++k) {
    if (k) svcSleepThread(3000000000LL);
    body.clear();
    ok = cloud3ds::http_get(datos3ds::base() + "programa/ultimo.json", body, st, err);
    if (st != 0) break;   // (el servidor ha contestado: no hace falta insistir)
  }
  if (!ok) {
    why = st == 404 ? "Todavía no hay versiones publicadas" : st == 0 ? "Sin conexión" : "El servidor no contesta";
    return false;
  }
  UpdateEntry a, b;
  if (!update_parse(body, kUpdateKey, a, b, why)) { why = "Versión del servidor rechazada: " + why; return false; }
  LightLock_Lock(&g_lock);
  g_latest = a; g_prev = b;
  LightLock_Unlock(&g_lock);
  return true;
}

// Descarga la entrada a `name` (en la carpeta del juego) y la deja pendiente.
bool download(const UpdateEntry& e, const char* name, bool back) {
  const UpdateFile& uf = file_of(e);
  g_state = ST_DOWNLOADING;
  g_pct = 0;
  char msg[96];
  std::snprintf(msg, sizeof(msg), "Descargando la versión %s (build %d)…", e.version.c_str(), e.build);
  set_status(msg);
  std::string err;
  if (!cloud3ds::http_download(datos3ds::base() + "programa/" + uf.url, path(name), uf.size, uf.hash, &g_pct, &g_cancel, err)) {
    set_status("No se ha podido descargar: " + err);
    g_state = ST_FAILED;
    return false;
  }
  g_pend.on = true; g_pend.back = back; g_pend.file = name; g_pend.serie = e.serie; g_pend.build = e.build;
  if (back) g_decided = std::max(g_decided, g_latest.serie);
  save_state();
  if (back) std::snprintf(msg, sizeof(msg), "Lista para volver a la build %d: se instala al salir del juego", e.build);
  else std::snprintf(msg, sizeof(msg), "Versión %s (build %d) lista: se instala al salir del juego", e.version.c_str(), e.build);
  set_status(msg);
  g_state = ST_READY;
  return true;
}

void worker(void*) {
  crash3ds::guard("actualizar");
  std::string why;
  const int job = g_job;
  g_state = ST_CHECKING;
  set_status("Buscando versiones nuevas…");
  const bool got = fetch_manifest(why);
  if (job == 3) {   // volver atrás con la anterior del servidor
    if (!got) { set_status("No se puede volver atrás: " + why); g_state = ST_FAILED; g_busy = false; return; }
    const UpdateEntry prev = g_latest.build == PS_BUILD || !g_latest.ok ? g_prev : g_latest;   // (si llevas otra, la publicada)
    if (!prev.ok || prev.build == PS_BUILD) { set_status("No hay versión anterior publicada"); g_state = ST_FAILED; g_busy = false; return; }
    std::string name = std::string("vuelta.") + ext();
    download(prev, name.c_str(), true);
    g_busy = false;
    return;
  }
  if (!got) { set_status(why); g_state = g_pend.on ? ST_READY : ST_FAILED; g_busy = false; return; }
  if (g_pend.on && g_pend.serie >= g_latest.serie) { g_state = ST_READY; g_busy = false; return; }   // (ya descargada)
  if (!update_wanted(g_latest, PS_BUILD, g_decided)) {
    set_status(g_latest.build == PS_BUILD ? "Tienes la última versión" : "Al día (volviste atrás: se actualizará con la siguiente)");
    g_state = ST_UPTODATE;
    g_busy = false;
    return;
  }
  if (job == 2) {
    std::string name = std::string("actualizacion.") + ext();
    download(g_latest, name.c_str(), false);
  } else {
    char msg[96];
    std::snprintf(msg, sizeof(msg), "Hay una versión nueva: %s (build %d)", g_latest.version.c_str(), g_latest.build);
    set_status(msg);
    g_state = ST_IDLE;
  }
  g_busy = false;
}

bool start(int job) {
  if (g_busy || !supported()) return false;
  if (g_thr) { threadJoin(g_thr, U64_MAX); threadFree(g_thr); g_thr = nullptr; }
  g_job = job;
  g_busy = true;
  g_cancel = false;
  s32 prio = 0x30;
  svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
  g_thr = threadCreate(worker, nullptr, 128 * 1024, prio + 2, -2, false);
  if (!g_thr) { g_busy = false; return false; }
  return true;
}

// Instala un .cia de la SD con el servicio del sistema (como FBI).
bool install_cia(const std::string& file, void (*show)(const char*, int), std::string& err) {
  FILE* f = std::fopen(file.c_str(), "rb");
  if (!f) { err = "no se encuentra el archivo"; return false; }
  std::fseek(f, 0, SEEK_END);
  const long total = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (R_FAILED(amInit())) { std::fclose(f); err = "sin permiso para instalar"; return false; }
  Handle h;
  Result r = AM_StartCiaInstall(MEDIATYPE_SD, &h);
  bool ok = R_SUCCEEDED(r);
  static uint8_t buf[128 * 1024];
  u64 off = 0;
  size_t n;
  int last = -1;
  while (ok && (n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
    u32 w = 0;
    r = FSFILE_Write(h, &w, off, buf, u32(n), 0);
    ok = R_SUCCEEDED(r) && w == n;
    off += n;
    const int p = int(off * 100 / u64(total > 0 ? total : 1));
    if (show && p != last) { show("Instalando la versión nueva…", p); last = p; }
  }
  std::fclose(f);
  if (ok) ok = R_SUCCEEDED(AM_FinishCiaInstall(h));
  else AM_CancelCIAInstall(h);
  amExit();
  if (!ok) { char b[48]; std::snprintf(b, sizeof(b), "el sistema no la ha aceptado (0x%08lX)", (unsigned long)r); err = b; }
  return ok;
}
}  // namespace

void init(const char* dir, const char* argv0) {
  LightLock_Init(&g_lock);
  g_dir = dir;
  // Cargado desde un .3dsx de la SD (el Homebrew Launcher lo dice en argv[0]):
  // se cambia ese archivo. Si no, y no es homebrew, es el .cia instalado.
  if (argv0 && std::strncmp(argv0, "sdmc:/", 6) == 0 && std::strstr(argv0, ".3dsx")) g_self = argv0;
  g_cia = g_self.empty() && !envIsHomebrew();
  load_state();
  if (g_pend.on) {
    g_state = ST_READY;
    set_status(g_pend.back ? "Vuelta a la versión anterior pendiente: se instala al salir" : "Versión nueva descargada: se instala al salir del juego");
  }
}

bool supported() { return g_cia || !g_self.empty(); }
bool is_cia() { return g_cia; }
void start_check(bool dl) { start(dl ? 2 : 1); }
int state() { return g_state; }
int pct() { return g_pct; }
std::string status() {
  LightLock_Lock(&g_lock);
  std::string s = g_status;
  LightLock_Unlock(&g_lock);
  return s;
}
const UpdateEntry& latest() { return g_latest; }

bool can_rollback() {
  if (!supported()) return false;
  if (exists(path(g_cia ? "anterior.cia" : "anterior.3dsx"))) return true;
  return g_prev.ok || (g_latest.ok && g_latest.build != PS_BUILD) || !g_latest.ok;   // (se mira en el servidor)
}

bool start_rollback() {
  if (!supported() || g_busy) return false;
  const std::string local = g_cia ? "anterior.cia" : "anterior.3dsx";
  if (exists(path(local.c_str()))) {   // la de la SD: sin red
    g_pend.on = true; g_pend.back = true; g_pend.file = local; g_pend.serie = 0; g_pend.build = 0;
    g_decided = std::max(g_decided, g_latest.serie);
    save_state();
    set_status("Se vuelve a la versión anterior al salir del juego");
    g_state = ST_READY;
    return true;
  }
  return start(3);
}

bool pending() { return g_pend.on && !g_busy; }
std::string pending_text() { return g_pend.back ? "volver a la versión anterior" : "instalar la versión nueva"; }

std::string apply(void (*show)(const char*, int)) {
  if (!g_pend.on) return "";
  g_cancel = true;
  if (g_thr) { threadJoin(g_thr, 10000000000ULL); threadFree(g_thr); g_thr = nullptr; }
  const std::string src = path(g_pend.file.c_str());
  std::string res;
  if (show) show(g_pend.back ? "Volviendo a la versión anterior…" : "Instalando la versión nueva…", 0);
  if (!g_cia) {
    // .3dsx: el que se usa pasa a anterior.3dsx y el nuevo ocupa su sitio.
    const std::string prev = path("anterior.3dsx"), swap = g_self + ".cambio";
    if (g_pend.file == "anterior.3dsx") {   // volver con la de la SD: se intercambian
      if (std::rename(g_self.c_str(), swap.c_str()) == 0) {
        if (std::rename(prev.c_str(), g_self.c_str()) == 0) { std::rename(swap.c_str(), prev.c_str()); res = "vuelta a la anterior hecha"; }
        else { std::rename(swap.c_str(), g_self.c_str()); res = "no se ha podido volver atrás"; }
      } else res = "no se ha podido volver atrás";
    } else {
      std::remove(swap.c_str());
      if (std::rename(g_self.c_str(), swap.c_str()) == 0) {
        if (std::rename(src.c_str(), g_self.c_str()) == 0) { std::remove(prev.c_str()); std::rename(swap.c_str(), prev.c_str()); res = "versión nueva instalada"; }
        else { std::rename(swap.c_str(), g_self.c_str()); res = "no se ha podido instalar"; }
      } else res = "no se ha podido instalar";
    }
  } else {
    std::string err;
    if (install_cia(src, show, err)) {
      // La instalada se guarda (para volver atrás sin red); la de antes, como anterior.
      const std::string inst = path("instalada.cia"), prev = path("anterior.cia");
      if (g_pend.file == "anterior.cia") {   // se intercambian
        const std::string swap = path("cambio.cia");
        std::rename(inst.c_str(), swap.c_str());
        std::rename(prev.c_str(), inst.c_str());
        std::rename(swap.c_str(), prev.c_str());
      } else {
        if (exists(inst)) { std::remove(prev.c_str()); std::rename(inst.c_str(), prev.c_str()); }
        std::rename(src.c_str(), inst.c_str());
      }
      res = g_pend.back ? "vuelta a la anterior instalada" : "versión nueva instalada";
    } else {
      res = "no se ha podido instalar: " + err;
    }
  }
  if (res.find("no se ha podido") != 0) {
    g_decided = std::max(g_decided, g_pend.serie);
    g_pend = Pending();
  }
  save_state();
  return res;
}

void shutdown() {
  g_cancel = true;
  if (g_thr) { threadJoin(g_thr, 10000000000ULL); threadFree(g_thr); g_thr = nullptr; }
}

}  // namespace prog3ds
}  // namespace ps

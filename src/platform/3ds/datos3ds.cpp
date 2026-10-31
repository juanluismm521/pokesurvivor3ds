// datos3ds.cpp — ver datos3ds.h.
#include "platform/3ds/datos3ds.h"

#include <cstdio>
#include <string>

namespace ps {
namespace datos3ds {

namespace {
const char kBaseDefault[] = "https://pokesurvivor3ds.duckdns.org/datos/";
std::string g_base = kBaseDefault;   // (pruebas: datos_url.txt en la carpeta del juego la cambia)

bool read_file(const std::string& p, std::string& out) {
  FILE* f = std::fopen(p.c_str(), "rb");
  if (!f) return false;
  out.clear();
  char b[512];
  size_t n;
  while ((n = std::fread(b, 1, sizeof(b), f)) > 0) out.append(b, n);
  std::fclose(f);
  return true;
}
}  // namespace

std::string init(const char* dir) {
  const std::string d = dir;
  std::string text;
  if (read_file(d + "/datos_url.txt", text)) {
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) text.pop_back();
    if (text.compare(0, 8, "https://") == 0 && text.size() < 200) g_base = text;
  }
  // Los paquetes de antes de la 0.26.2 (se bajaban de la web): fuera, los datos son los de esta versión.
  if (std::remove((d + "/datos.json").c_str()) == 0) return "datos de esta versión (quitado el paquete viejo de la SD)";
  return "datos de esta versión";
}

const std::string& base() { return g_base; }

}  // namespace datos3ds
}  // namespace ps

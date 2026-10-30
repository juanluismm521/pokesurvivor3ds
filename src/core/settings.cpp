// settings.cpp
#include "core/settings.h"

#include <cstdio>
#include <cstring>

#include "core/json.h"

namespace ps {

std::string settings_to_json(const Settings& s) {
  Json o = Json::object();
  o.set("lang", Json::str(s.lang == 1 ? "en" : "es"));
  o.set("quality", Json::str(s.quality < 0 ? "auto" : s.quality == 0 ? "low" : s.quality == 1 ? "mid" : "high"));
  o.set("dmgNumbers", Json::boolean(s.dmg_numbers));
  o.set("calm", Json::boolean(s.calm));
  o.set("fps", Json::boolean(s.fps));
  o.set("music", Json::num(s.music));
  o.set("sfx", Json::num(s.sfx));
  o.set("stereo3d", Json::boolean(s.stereo));
  o.set("server", Json::str(s.server));
  o.set("serverPort", Json::num(s.server_port));
  o.set("cloudNever", Json::boolean(s.cloud_never));
  o.set("datosAuto", Json::boolean(s.datos_auto));
  o.set("novedades", Json::str(s.news_seen));
  o.set("modo", Json::num(s.play_mode));
  return o.dump();
}

bool settings_from_json(Settings& s, const std::string& text) {
  Json o;
  if (!o.parse(text) || !o.is_obj()) return false;
  s = Settings();
  s.lang = o["lang"].as_str() == "en" ? 1 : 0;
  s.lang_set = o["lang"].is_str();
  const std::string& q = o["quality"].as_str();
  s.quality = q == "low" ? 0 : q == "mid" ? 1 : q == "high" ? 2 : -1;
  if (o.has("dmgNumbers")) s.dmg_numbers = o["dmgNumbers"].as_bool();
  s.calm = o["calm"].as_bool();
  s.fps = o["fps"].as_bool();
  if (o.has("music")) s.music = int(o["music"].as_int(70));
  if (o.has("sfx")) s.sfx = int(o["sfx"].as_int(80));
  // (desde el 30-10-2026 el 3D va apagado de serie: el «stereo» de antes, que casi
  // nadie tocó, no cuenta; sólo «stereo3d», lo que elijas a partir de ahora)
  if (o.has("stereo3d")) s.stereo = o["stereo3d"].as_bool();
  if (o["server"].is_str() && settings_valid_host(o["server"].as_str().c_str())) std::snprintf(s.server, sizeof(s.server), "%s", o["server"].as_str().c_str());
  // El servidor por defecto era la IP del VPS; ahora es su dominio (si cambia
  // de máquina, sólo hay que cambiar el dominio).
  if (!std::strcmp(s.server, "217.154.98.19")) std::snprintf(s.server, sizeof(s.server), "pokesurvivor3ds.duckdns.org");
  if (o.has("serverPort")) {
    const int p = int(o["serverPort"].as_int(24873));
    s.server_port = p > 0 && p < 65536 ? p : 24873;
  }
  s.cloud_never = o["cloudNever"].as_bool();
  s.datos_auto = o["datosAuto"].as_bool(false);
  std::snprintf(s.news_seen, sizeof(s.news_seen), "%s", o["novedades"].as_str().c_str());
  if (o.has("modo")) s.play_mode = int(o["modo"].as_int(0)) == 1 ? 1 : 0;
  if (s.music < 0 || s.music > 100) s.music = 70;
  if (s.sfx < 0 || s.sfx > 100) s.sfx = 80;
  return true;
}

bool settings_valid_host(const char* h) {
  if (!h || !*h) return false;
  int n = 0;
  for (const char* p = h; *p; ++p, ++n) {
    const char c = *p;
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-')) return false;
  }
  return n < 64 && h[0] != '.' && h[0] != '-';
}

bool settings_read(const char* dir, Settings& s) {
  char path[256];
  std::snprintf(path, sizeof(path), "%s/settings.json", dir);
  FILE* f = std::fopen(path, "rb");
  if (!f) return false;
  std::string t;
  char b[1024];
  size_t n;
  while ((n = std::fread(b, 1, sizeof(b), f)) > 0) t.append(b, n);
  std::fclose(f);
  return settings_from_json(s, t);
}

bool settings_write(const char* dir, const Settings& s) {
  char path[256], tmp[256];
  std::snprintf(path, sizeof(path), "%s/settings.json", dir);
  std::snprintf(tmp, sizeof(tmp), "%s/settings.tmp", dir);
  FILE* f = std::fopen(tmp, "wb");
  if (!f) return false;
  const std::string t = settings_to_json(s);
  const bool ok = std::fwrite(t.data(), 1, t.size(), f) == t.size();
  if (std::fclose(f) != 0 || !ok) return false;
  std::remove(path);
  return std::rename(tmp, path) == 0;
}

}  // namespace ps

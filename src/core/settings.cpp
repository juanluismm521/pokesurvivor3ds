// settings.cpp
#include "core/settings.h"

#include <cstdio>

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
  o.set("stereo", Json::boolean(s.stereo));
  return o.dump();
}

bool settings_from_json(Settings& s, const std::string& text) {
  Json o;
  if (!o.parse(text) || !o.is_obj()) return false;
  s = Settings();
  s.lang = o["lang"].as_str() == "en" ? 1 : 0;
  const std::string& q = o["quality"].as_str();
  s.quality = q == "low" ? 0 : q == "mid" ? 1 : q == "high" ? 2 : -1;
  if (o.has("dmgNumbers")) s.dmg_numbers = o["dmgNumbers"].as_bool();
  s.calm = o["calm"].as_bool();
  s.fps = o["fps"].as_bool();
  if (o.has("music")) s.music = int(o["music"].as_int(70));
  if (o.has("sfx")) s.sfx = int(o["sfx"].as_int(80));
  if (o.has("stereo")) s.stereo = o["stereo"].as_bool();
  if (s.music < 0 || s.music > 100) s.music = 70;
  if (s.sfx < 0 || s.sfx > 100) s.sfx = 80;
  return true;
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

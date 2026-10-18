// settings.h — ajustes de esta consola (settings.js de la web + volumen y 3D).
// Se guardan aparte de la partida (settings.json en la SD).
#pragma once
#include <string>

namespace ps {

struct Settings {
  int lang = 0;               // 0 español, 1 inglés
  int quality = -1;           // -1 auto, 0 baja, 1 media, 2 alta
  bool dmg_numbers = true;
  bool calm = false;          // menos sacudidas y destellos
  bool fps = false;           // contador de FPS
  int music = 70, sfx = 80;   // volumen 0..100
  bool stereo = true;         // 3D (si el deslizador está subido)
};

std::string settings_to_json(const Settings& s);
bool settings_from_json(Settings& s, const std::string& text);
bool settings_read(const char* dir, Settings& s);
bool settings_write(const char* dir, const Settings& s);

}  // namespace ps

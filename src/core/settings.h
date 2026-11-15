// settings.h — ajustes de esta consola (settings.js de la web + volumen y 3D).
// Se guardan aparte de la partida (settings.json en la SD).
#pragma once
#include <string>

namespace ps {

struct Settings {
  int lang = 0;               // 0 español, 1 inglés
  bool lang_set = false;      // (leído del archivo; si no, la plataforma pone el de la consola)
  int quality = -1;           // -1 auto, 0 baja, 1 media, 2 alta
  bool dmg_numbers = true;
  bool calm = false;          // menos sacudidas y destellos
  bool fps = false;           // contador de FPS
  int music = 70, sfx = 80;   // volumen 0..100
  bool stereo = false;        // 3D (si el deslizador está subido). Desactivado de serie (el dueño, 2026-10-30)
  // Servidor para jugar online (repositorio pokesurvivor-relay): IP o dominio y puerto.
  char server[64] = "pokesurvivor3ds.duckdns.org";
  int server_port = 24873;
  bool cloud_never = false;   // «Nunca»: no preguntar al abrir por la cuenta de pokesurvivor.com
  // «Comprobar actualizaciones automáticamente»: al encender (con wifi) baja los
  // datos nuevos de la web (datos3ds). Desactivado de serie (decisión del dueño).
  char news_seen[16] = "";    // últimas novedades vistas (las de ahora se abren solas una vez)
  int play_mode = 0;          // último modo elegido en Jugar (0 normal, 1 rápido)
  int diff = 0;               // dificultad elegida en Jugar (core/diff.h; si está desbloqueada)
  char shop_seen[12] = "";    // día (España, AAAAMMDD) de las últimas MT de la Tienda Kecleon vistas
};
// ¿Vale como IP o dominio? (letras, cifras, '.', '-'; sin espacios)
bool settings_valid_host(const char* h);

std::string settings_to_json(const Settings& s);
bool settings_from_json(Settings& s, const std::string& text);
bool settings_read(const char* dir, Settings& s);
bool settings_write(const char* dir, const Settings& s);

}  // namespace ps

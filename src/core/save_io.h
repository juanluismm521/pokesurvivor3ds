// save_io.h — partida guardada ↔ JSON y archivos (save_json.cpp).
#pragma once
#include <cstdint>
#include <string>

#include "core/json.h"

#include "core/save.h"

namespace ps {

std::string save_to_json(const Save& s);
bool save_from_json(Save& s, const std::string& text);
void new_save(Save& s, const char* name, int64_t now_ms);
// 0 = no hay partida, 1 = leída, 2 = leída de la copia de seguridad.
int save_read_file(const char* dir, Save& s);
// Escritura atómica (temporal + renombrar) con copia de seguridad.
bool save_write_file(const char* dir, const Save& s);
// Escribe ya convertida (save.tmp → save.json, el anterior a save.bak), en
// trozos de 16 KB: otros que lean archivos a la vez (la música) no esperan a
// que acabe todo.
bool save_write_text(const char* dir, const std::string& text);
// La partida web completa tal como se leyó (o se sincronizó): al escribir el
// archivo se pone encima lo de la 3DS y se conserva lo que la 3DS no usa
// (la libreta de monedas de cada aparato, campos nuevos de la web...).
Json& save_raw();
// Pone la partida web completa y apunta lo que la 3DS entiende de ella (`s`,
// ya leída de `raw`): al escribir sólo se cambia lo que la 3DS haya cambiado.
void save_set_raw(const Json& raw, const Save& s);
// Lo de la 3DS sobre la partida web completa (sólo lo que ha cambiado).
Json save_full_json(const Save& s);
int clamp_from(int f);

}  // namespace ps

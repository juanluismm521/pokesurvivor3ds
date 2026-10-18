// save_io.h — partida guardada ↔ JSON y archivos (save_json.cpp).
#pragma once
#include <cstdint>
#include <string>

#include "core/save.h"

namespace ps {

std::string save_to_json(const Save& s);
bool save_from_json(Save& s, const std::string& text);
void new_save(Save& s, const char* name, int64_t now_ms);
// 0 = no hay partida, 1 = leída, 2 = leída de la copia de seguridad.
int save_read_file(const char* dir, Save& s);
// Escritura atómica (temporal + renombrar) con copia de seguridad.
bool save_write_file(const char* dir, const Save& s);
int clamp_from(int f);

}  // namespace ps

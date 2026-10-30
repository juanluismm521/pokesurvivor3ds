// app_ui.h — colores y ayudantes de las pantallas (app.cpp, app_coop.cpp).
#pragma once
#include <3ds.h>
#include <citro2d.h>

#include <cstdint>

#include "platform/3ds/app.h"

namespace ps {

namespace {  // (cada archivo, su copia)
constexpr float BW = 320, BH = 240, TW = 400, TH = 240;
const u32 C_BG = C2D_Color32(16, 22, 34, 255);
const u32 C_PANEL = C2D_Color32(30, 40, 62, 255);
const u32 C_SEL = C2D_Color32(52, 70, 110, 255);
const u32 C_TEXT = C2D_Color32(234, 242, 255, 255);
const u32 C_MUTED = C2D_Color32(0x8f, 0xa3, 0xc4, 255);
const u32 C_GOLD = C2D_Color32(0xff, 0xcb, 0x3d, 255);
const char* const GEN_NAMES[9] = {"Kanto", "Johto", "Hoenn", "Sinnoh", "Teselia", "Kalos", "Alola", "Galar", "Paldea"};
// Pokémon que pasean por el menú (CAST de game.js).
const int16_t CAST[20] = {1, 4, 7, 25, 133, 52, 39, 35, 16, 43, 60, 69, 74, 92, 129, 147, 113, 143, 54, 79};
// Halloween: paseando por la noche, sobre todo Fantasma (game.js de la web).
const int16_t CAST_HW[20] = {92, 93, 94, 200, 355, 353, 425, 429, 442, 479, 562, 592, 607, 708, 710, 778, 25, 197, 302, 351};

inline int64_t now_ms() { return int64_t(osGetTime()) - 2208988800000LL; }   // (osGetTime cuenta desde 1900)

inline bool in_rect(const UiIn* in, float x, float y, float w, float h) {
  return in && in->tap && in->tx >= x && in->tx < x + w && in->ty >= y && in->ty < y + h;
}
}  // namespace

}  // namespace ps

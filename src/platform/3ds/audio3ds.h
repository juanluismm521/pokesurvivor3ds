// audio3ds.h — sonido en la 3DS (ndsp): música en streaming (Ogg Vorbis con
// tremor, en un hilo) y efectos en PCM, grabados del sintetizador de la web
// por tools/gen_audio.js. Si el DSP no arranca (falta dspfirm.cdc), el juego
// sigue en silencio.
#pragma once
#include <cstdint>

#include "core/game.h"

namespace ps {

namespace audio {

bool init();
void exit();
bool ok();
void set_volume(int music, int sfx);   // 0..100
// Tema de música (MU_*); MU_NONE = silencio. Cambia con fundido.
void music(int track);
// Efecto por nombre (ver sfx.bin) con separación mínima entre repeticiones.
void play(const char* name, float vol = 1, float pitch = 1);
// Efecto de la partida (Game::sfx_queue).
void play_game(const SfxEvent& e);
void update(float dt);   // fundidos

}  // namespace audio

}  // namespace ps

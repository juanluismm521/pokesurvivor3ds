// saver3ds.h — guardar la partida sin parar el juego.
//
// Convertir la partida a JSON (unos 160 KB con toda la colección) y escribirla
// en la SD tarda en la consola de 150 ms (New 3DS) a casi medio segundo (la
// vieja): hecho en el hilo principal, el juego se quedaba «pillado» al comprar
// una mejora y la música se cortaba. Ahora el hilo principal sólo copia la
// partida (un instante) y otro hilo la convierte y la escribe. Si llegan varios
// guardados seguidos, se escribe sólo el último.
#pragma once
#include "core/save.h"

namespace ps {
namespace saver3ds {

void init(const char* dir);
void save(const Save& s);   // copia y sigue (se escribe en segundo plano)
void flush();               // espera a que esté todo escrito (al salir)
void shutdown();
// La partida web original (save_raw) la leen la conversión y la nube, y la
// cambia la nube al sincronizar: quien la cambie, con este cerrojo.
void lock_raw();
void unlock_raw();
// Para el log del autotest: cuántos guardados y lo que tardó el más lento (ms).
int saves_done();
int slowest_ms();

}  // namespace saver3ds
}  // namespace ps

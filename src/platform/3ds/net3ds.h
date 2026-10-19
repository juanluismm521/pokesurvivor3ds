// net3ds.h — juego local de la 3DS (UDS, servicio nwm::UDS de libctru) para
// el cooperativo: una consola crea la sala y las demás (hasta 3) se unen.
// Sólo transporte: lo que viaja lo decide core/lockstep (y la sala, app.cpp).
#pragma once
#include <cstdint>

namespace ps {
namespace net3ds {

constexpr int kMaxNodes = 4;
constexpr uint16_t kBroadcast = 0xFFFF;
constexpr uint16_t kHostNode = 1;

struct Room {
  char host[16];                 // nombre de quien la ha creado
  int players = 0;               // consolas dentro
  int index = 0;                 // (posición en la última búsqueda)
};

// Por internet (servidor de relevo, server/relay.py) en vez del juego local.
// Cambiar de modo cierra lo que hubiera abierto.
void set_online(bool online);
bool online();
bool init();                     // al entrar en el menú del cooperativo
void exit();
bool ready();

bool create(const char* host_name);
// Busca salas (tarda ~1 s). Devuelve cuántas.
int scan(Room* out, int max);
bool join(int index);            // índice de la última búsqueda
void leave();                    // sale de la sala (o la cierra, si es tuya)
bool in_room();
bool is_host();

bool send(uint16_t to_node, const void* data, int len);
// Saca un paquete recibido (sin esperar). Devuelve su tamaño o 0.
int poll(void* buf, int cap, uint16_t* from_node);
uint16_t node_mask();            // bit 0 = nodo 1 (el anfitrión), bit 1 = nodo 2...
uint16_t my_node();
void set_room_info(int players); // anfitrión: cuántos hay (lo ven al buscar)
void close_room();               // anfitrión: al empezar, que no entre nadie más
int rtt_ms();                    // ping con el servidor (por internet; 0 en local)
const char* room_code();         // código de la sala (por internet)
const char* last_error();
void set_name(const char* name); // (por internet: con qué nombre se entra)
void set_server(const char* host, int port);   // IP o dominio y puerto (Ajustes)

}  // namespace net3ds
}  // namespace ps

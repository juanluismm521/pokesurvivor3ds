// relay_client.h — cliente del servidor de relevo (relay.py del repositorio pokesurvivor-relay) para jugar
// online: crear sala, ver las salas abiertas, entrar, mandar y recibir
// paquetes de la sala y mantenerla viva (PING cada segundo, que además mide el
// ping). Sockets BSD (UDP): igual en la 3DS (soc:u, tras socInit) y en Linux,
// así se prueba en PC contra el servidor de verdad.
#pragma once
#include <cstdint>
#include <string>

namespace ps {
namespace relay {

constexpr int kDefaultPort = 24873;
constexpr uint16_t kBroadcast = 0xFFFF;

struct RoomInfo {
  char code[5] = {};
  char host[16] = {};
  int players = 0;
};

class Client {
 public:
  ~Client() { close(); }
  bool open(const char* host, int port);
  void close();
  bool is_open() const { return fd_ >= 0; }

  // Esperan la respuesta (como mucho `timeout_ms`).
  bool create(const char* name, int timeout_ms = 3000);
  int list(RoomInfo* out, int max, int timeout_ms = 2000);
  bool join(const char* code, const char* name, int timeout_ms = 3000);
  void leave();

  bool in_room() const { return node_ != 0; }
  bool is_host() const { return node_ == 1; }
  uint16_t node() const { return node_; }
  uint16_t mask() const { return mask_; }
  const char* code() const { return code_; }
  int rtt_ms() const { return rtt_; }
  void set_open(bool open);     // anfitrión: cerrar la sala al empezar
  bool closed_by_host() const { return kicked_; }

  bool send(uint16_t to, const void* data, int len);
  // Un paquete de la sala (sin esperar): su tamaño, o 0. Atiende también lo
  // del servidor (nodos, ping) y manda el PING de cada segundo.
  int poll(void* buf, int cap, uint16_t* from);

  std::string error;

 private:
  bool raw_send(uint8_t type, const void* body, int len);
  int raw_recv(uint8_t* buf, int cap, int timeout_ms);   // un paquete del servidor (o 0)
  bool wait_for(uint8_t type, uint8_t* buf, int cap, int timeout_ms, int& len);
  void handle_ctrl(const uint8_t* p, int n);
  int fd_ = -1;
  uint8_t addr_[16] = {};        // sockaddr_in
  uint16_t node_ = 0, mask_ = 0;
  char code_[5] = {};
  int rtt_ = 0;
  uint64_t last_ping_ = 0;
  bool kicked_ = false;
  // Paquetes de datos que llegan mientras se espera otra cosa.
  static constexpr int kQueue = 32;
  uint8_t q_[kQueue][1300];
  int qlen_[kQueue] = {};
  uint16_t qfrom_[kQueue] = {};
  int qhead_ = 0, qn_ = 0;
};

uint64_t now_ms();

}  // namespace relay
}  // namespace ps

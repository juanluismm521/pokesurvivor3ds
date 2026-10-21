// lockstep.h — cooperativo local por "lockstep" con anfitrión de relevo.
//
// Cada consola simula la partida entera; lo único que viaja son las entradas
// de cada paso (8 bytes por jugador). Como el núcleo es determinista, con las
// mismas entradas en el mismo orden todas llegan al mismo estado.
//
//   invitado -> anfitrión   sus entradas para los pasos que vienen (con
//                           kDelay pasos de adelanto; cada envío repite las
//                           últimas por si se pierde alguno) y la huella de
//                           su estado cada kHashEvery pasos
//   anfitrión -> todos      el "paquete de paso": las entradas de TODOS para
//                           cada paso (también repetidas) y su huella
//
// Nadie simula el paso N sin tener las entradas de todos para N: el
// anfitrión las junta y las reparte, así que todos simulan lo mismo. Si un
// invitado deja de responder, el anfitrión pone por él la orden de irse
// (CMD_QUIT) en un paso y todos lo quitan en ese mismo paso. Si las huellas
// no coinciden, se avisa (desincronización) y se acaba.
//
// Sin nada de la 3DS: el transporte (UDS) lo pone la plataforma, que pasa los
// paquetes que llegan (on_packet) y manda los que salen (outbox). Así se
// prueba en PC con varias "consolas" en el mismo programa.
#pragma once
#include <cstdint>

#include "core/game.h"

namespace ps {
namespace net {

constexpr int kDelay = 6;            // pasos de adelanto de las entradas (100 ms): más que la ida y vuelta por radio
constexpr int kResend = 8;           // pasos que repite cada envío
constexpr int kRing = 256;           // pasos guardados (más que kDelay + kResend)
constexpr int kHashEvery = 60;       // huella cada segundo
constexpr int kTimeoutFrames = 60 * 20;   // 20 s sin noticias: se le da por ido
constexpr int kMaxPacket = 1024;
constexpr uint32_t kMagic = 0x50535338;   // 'PSS8' + versión del protocolo (8: la mega da más; con la 7 se desincronizarían)
// Lo que va en cada paquete: la versión del protocolo mezclada con la huella
// de los datos del juego (gen_data). Dos 3DS con datos distintos (otra versión
// de la web) se ignoran: si jugaran juntas, sus partidas se separarían.
inline uint32_t wire_magic() { return kMagic ^ kDataHash; }
constexpr int kInputBytes = 8;

// ---------------- formato ----------------
void pack_input(const Input& in, uint8_t* out);     // 8 bytes
Input unpack_input(const uint8_t* b);
// La entrada tal como la verán los demás (el movimiento va en 1/127): la
// consola local también usa ésta, para simular exactamente lo mismo.
inline Input quantize(const Input& in) { uint8_t b[kInputBytes]; pack_input(in, b); return unpack_input(b); }

// Sala: lo que cada jugador aporta (su Pokémon y sus mejoras) y el arranque.
int write_member(const RunConfig::Member& m, uint8_t* out, int cap);
bool read_member(const uint8_t* b, int len, RunConfig::Member& m);
int write_config(const RunConfig& c, uint8_t* out, int cap);
bool read_config(const uint8_t* b, int len, RunConfig& c);

enum MsgType : uint8_t {
  MSG_HELLO = 1,     // invitado -> anfitrión: quiero entrar (Member)
  MSG_ROSTER,        // anfitrión -> todos: quién está en la sala (nombres y Pokémon)
  MSG_START,         // anfitrión -> todos: empieza (RunConfig + tu número)
  MSG_INPUT,         // invitado -> anfitrión: entradas
  MSG_BUNDLE,        // anfitrión -> todos: paquetes de paso
  MSG_BYE,           // me voy de la sala
};

// ---------------- lockstep ----------------
struct Packet {
  uint8_t data[kMaxPacket];
  int len = 0;
  int to = -1;                       // -1 = a todos (anfitrión) / al anfitrión (invitado)
};

class Lockstep {
 public:
  // me: tu número (0 = anfitrión); n: jugadores al empezar; delay: pasos de
  // adelanto (en local kDelay; por internet, según el ping).
  void begin(int me, int n, int delay = kDelay);
  int delay() const { return delay_; }
  // Anfitrión: el retraso que aconseja a cada jugador (lo mide él: con cuánta
  // antelación le llegan sus entradas). Va en cada paquete de paso.
  int advised(int p) const { return want_[p & (kMaxPlayers - 1)]; }
  bool active() const { return on_; }
  bool host() const { return me_ == 0; }
  int me() const { return me_; }

  // ¿Se puede dar ya el paso siguiente? (hay entradas de todos)
  bool ready() const;
  // Da el paso siguiente con las entradas de todos (y saca la huella cada
  // kHashEvery pasos). Devuelve false si no se podía (falta alguien).
  bool step(Game& g);
  // Tu entrada para el siguiente paso pendiente de pedir. Llamar una vez por
  // paso simulado (o al empezar, hasta llenar el adelanto): ver want_local().
  bool want_local() const;
  void push_local(const Input& in);

  // Red: lo que llega y lo que hay que mandar (una vez por fotograma).
  void on_packet(const uint8_t* data, int len, int from);
  int outbox(Packet* out, int max);
  void tick_frame();                 // cuenta fotogramas (tiempos de espera)
  // Anfitrión: la radio dice que se ha desconectado: se le quita ya (sin
  // esperar kTimeoutFrames).
  void drop(int p) { if (host() && p > 0 && p < n_ && !gone_[p]) { heard_[p] = kTimeoutFrames + 1; host_collect(); } }
  bool gone(int p) const { return gone_[p]; }

  // Estado para la pantalla.
  int64_t tick() const { return sim_; }
  uint8_t waiting_mask() const;      // de quién falta entrada (si llevamos rato esperando)
  int stall_frames() const { return stall_; }
  bool desync() const { return desync_; }
  bool lost_host() const { return lost_host_; }
  uint32_t last_hash() const { return last_hash_tick_ < 0 ? 0 : my_hash_[(last_hash_tick_ / kHashEvery) % kHashRing]; }
  int64_t last_hash_tick() const { return last_hash_tick_; }

 private:
  static constexpr int kHashRing = 16;
  bool on_ = false;
  int me_ = 0, n_ = 1, delay_ = kDelay;
  int64_t sim_ = 0;                  // siguiente paso a simular
  int64_t local_ = 0;                // siguiente paso para el que falta tu entrada
  Input in_[kRing][kMaxPlayers];
  bool have_[kRing][kMaxPlayers];    // anfitrión: entrada de cada uno · invitado: [t][0] = paquete recibido
  Input mine_[kRing];                // tus entradas (para repetirlas)
  uint8_t host_wait_ = 0;            // invitado: a quién espera el anfitrión
  bool gone_[kMaxPlayers] = {};      // anfitrión: ya no se le esperan entradas
  int64_t bundled_ = 0;              // anfitrión: pasos ya repartidos; invitado: pasos recibidos
  int64_t acked_[kMaxPlayers] = {};  // anfitrión: hasta dónde ha recibido cada uno (para no reenviar sin fin)
  int heard_[kMaxPlayers] = {};      // fotogramas desde su último mensaje
  int frames_ = 0, stall_ = 0;
  uint32_t my_hash_[kHashRing] = {};
  int64_t last_hash_tick_ = -1;
  // huellas que han llegado de los demás (anfitrión) o del anfitrión
  // (invitado), por segundo: se comparan cuando están la suya y la tuya
  int64_t peer_tick_[kMaxPlayers][kHashRing];
  uint32_t peer_hash_[kMaxPlayers][kHashRing];
  int64_t my_tick_[kHashRing];
  bool desync_ = false, lost_host_ = false;
  // Retraso de cada uno (lo decide el anfitrión): el que tiene mucho ping se
  // adelanta más con sus entradas y sólo él nota el retraso; los demás no le
  // esperan. lead_min_: la menor antelación vista en el último segundo.
  int want_[kMaxPlayers] = {};
  int lead_min_[kMaxPlayers] = {};
  int calm_[kMaxPlayers] = {};
  int waited_[kMaxPlayers] = {};     // fotogramas que se le ha esperado en el último segundo

  void host_collect();               // anfitrión: pasa a "repartido" lo que ya está completo
  void check_hash(int who, int64_t t);
  void got_hash(int who, int64_t t, uint32_t h);
};

}  // namespace net
}  // namespace ps

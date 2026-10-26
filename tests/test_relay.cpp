// Juego online: el servidor de relevo de verdad (relay.py del repo pokesurvivor-relay) y varias
// "consolas" en este programa que crean, buscan y entran en la sala por UDP y
// juegan con el lockstep a través del servidor.
#include <doctest/doctest.h>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/bot.h"
#include "core/game.h"
#include "core/lockstep.h"
#include "core/relay_client.h"

using namespace ps;

namespace {
// El servidor está en su propio repositorio (pokesurvivor-relay), al lado de
// éste; si no está, estas pruebas se saltan (con aviso).
bool relay_available() {
  if (FILE* f = std::fopen(PS_RELAY_PY, "r")) { std::fclose(f); return true; }
  MESSAGE("No encuentro el servidor (" << PS_RELAY_PY << "): clona github.com/juanluismm521/pokesurvivor3ds-server en ../servidor (o -DPS_RELAY_PY=...). Se salta la prueba.");
  return false;
}

struct Server {
  pid_t pid = -1;
  int port = 0;
  bool start() {
    port = 25000 + int(getpid() % 3000);
    pid = fork();
    if (pid == 0) {
      const std::string p = std::to_string(port);
      const std::string py = PS_RELAY_PY;
      execlp("python3", "python3", py.c_str(), "--port", p.c_str(), "--host", "127.0.0.1", (char*)nullptr);
      _exit(127);
    }
    usleep(700 * 1000);   // (que arranque)
    return pid > 0;
  }
  ~Server() { if (pid > 0) { kill(pid, SIGTERM); waitpid(pid, nullptr, 0); } }
};

void sleep_ms(int ms) { usleep(useconds_t(ms) * 1000); }
}  // namespace

TEST_CASE("online: sala, lista, entrar y paquetes por el servidor de relevo") {
  if (!relay_available()) return;
  Server srv;
  REQUIRE(srv.start());
  relay::Client a, b;
  REQUIRE(a.open("127.0.0.1", srv.port));
  REQUIRE(b.open("127.0.0.1", srv.port));
  REQUIRE(a.create("Ana"));
  CHECK(a.is_host());
  CHECK(std::strlen(a.code()) == 4);
  relay::RoomInfo rooms[8];
  const int n = b.list(rooms, 8);
  REQUIRE(n >= 1);
  CHECK(std::string(rooms[0].host) == "Ana");
  REQUIRE(b.join(rooms[0].code, "Beto"));
  CHECK(b.node() == 2);
  // a -> todos, b -> anfitrión
  const char hola[] = "hola";
  CHECK(a.send(relay::kBroadcast, hola, 4));
  CHECK(b.send(1, "adios", 5));
  char buf[64];
  uint16_t from = 0;
  int got = 0;
  for (int i = 0; i < 100 && !got; ++i) { got = b.poll(buf, sizeof(buf), &from); sleep_ms(10); }
  CHECK(got == 4);
  CHECK(from == 1);
  got = 0;
  for (int i = 0; i < 100 && !got; ++i) { got = a.poll(buf, sizeof(buf), &from); sleep_ms(10); }
  CHECK(got == 5);
  CHECK(from == 2);
  // Los dos ven a los dos (PEERS / PONG) y miden el ping.
  for (int i = 0; i < 120; ++i) { a.poll(buf, sizeof(buf), &from); b.poll(buf, sizeof(buf), &from); sleep_ms(10); }
  CHECK(a.mask() == 0x3);
  CHECK(b.mask() == 0x3);
  CHECK(a.rtt_ms() >= 0);
  // Cerrada al empezar: ya no sale en la lista y no se puede entrar.
  a.set_open(false);
  sleep_ms(100);
  relay::Client c;
  REQUIRE(c.open("127.0.0.1", srv.port));
  CHECK(c.list(rooms, 8) == 0);
  CHECK(c.error.empty());   // (ninguna sala no es un error: el servidor sí contesta)
  CHECK(!c.join(a.code(), "Cris"));
  // Se va el anfitrión: el invitado se entera.
  a.leave();
  for (int i = 0; i < 50 && !b.closed_by_host(); ++i) { b.poll(buf, sizeof(buf), &from); sleep_ms(10); }
  CHECK(b.closed_by_host());
}

TEST_CASE("online: 3 consolas juegan por el servidor y no se separan") {
  if (!relay_available()) return;
  Server srv;
  REQUIRE(srv.start());
  const int N = 3;
  relay::Client cl[N];
  for (int i = 0; i < N; ++i) REQUIRE(cl[i].open("127.0.0.1", srv.port));
  REQUIRE(cl[0].create("J0"));
  for (int i = 1; i < N; ++i) REQUIRE(cl[i].join(cl[0].code(), ("J" + std::to_string(i)).c_str()));
  RunConfig cfg;
  cfg.seed = 77; cfg.judged = 1; cfg.nplayers = N;
  for (int i = 0; i < N; ++i) { cfg.members[i].dex = i == 0 ? 25 : i == 1 ? 4 : 7; std::snprintf(cfg.members[i].name, 16, "J%d", i); }
  std::vector<std::unique_ptr<Game>> g;
  net::Lockstep ls[N];
  Bot bot[N];
  std::map<int64_t, uint32_t> hashes[N];
  for (int i = 0; i < N; ++i) {
    g.emplace_back(new Game());
    g[i]->local = i;
    g[i]->debug_immortal = true;
    g[i]->start_run(cfg);
    ls[i].begin(i, N, 8);
    bot[i].init(100 + i);
  }
  // nodo del servidor -> jugador (en la sala: 1 = anfitrión, luego por orden)
  auto idx_of = [&](uint16_t node) { return int(node) - 1; };
  uint8_t buf[1400];
  const int frames = 60 * 25;
  for (int f = 0; f < frames; ++f) {
    for (int i = 0; i < N; ++i) {
      uint16_t from;
      int n;
      while ((n = cl[i].poll(buf, sizeof(buf), &from)) > 0) ls[i].on_packet(buf, n, idx_of(from));
      while (ls[i].want_local()) { Input in; bot[i].decide(*g[i], in, i); ls[i].push_local(in); }
      for (int s = 0; s < 2 && ls[i].ready(); ++s) {
        ls[i].step(*g[i]);
        if (ls[i].tick() % net::kHashEvery == 0) hashes[i][ls[i].tick()] = g[i]->state_hash();
      }
      net::Packet pk[2];
      const int np = ls[i].outbox(pk, 2);
      for (int k = 0; k < np; ++k) cl[i].send(pk[k].to < 0 ? relay::kBroadcast : 1, pk[k].data, pk[k].len);
      ls[i].tick_frame();
    }
    usleep(12000);  // (casi un fotograma de verdad: el lockstep va al ritmo de la red)
  }
  int compared = 0;
  bool same = true;
  for (auto& kv : hashes[0])
    for (int i = 1; i < N; ++i) {
      auto it = hashes[i].find(kv.first);
      if (it == hashes[i].end()) continue;
      compared++;
      if (it->second != kv.second) same = false;
    }
  CHECK(same);
  CHECK(compared > 40);
  for (int i = 0; i < N; ++i) CHECK(!ls[i].desync());
  CHECK(ls[N - 1].tick() > frames * 0.85);
}

TEST_CASE("online: buscar el servidor por su nombre y avisos claros") {
  relay::Client c;
  // Un nombre que no existe: lo dice con el nombre (y no se queda colgado).
  CHECK(!c.open("no-existe.invalid", 24873));
  CHECK(c.error.find("no-existe.invalid") != std::string::npos);
  // Un nombre de verdad (localhost) se resuelve.
  REQUIRE(c.open("localhost", 9));
  // Nadie escuchando: «no contesta», no «no se encuentra».
  relay::RoomInfo rooms[8];
  CHECK(c.list(rooms, 8, 600) == 0);
  CHECK(c.error.find("contesta") != std::string::npos);
  if (!relay_available()) return;
  Server srv;
  REQUIRE(srv.start());
  relay::Client d;
  REQUIRE(d.open("localhost", srv.port));
  CHECK(d.list(rooms, 8) == 0);
  CHECK(d.error.empty());
}

TEST_CASE("relay: la sala anuncia candado y modo (banderas); el nombre llega limpio") {
  if (!relay_available()) return;
  Server srv;
  REQUIRE(srv.start());
  relay::Client a, b, c;
  REQUIRE(a.open("127.0.0.1", srv.port));
  REQUIRE(b.open("127.0.0.1", srv.port));
  REQUIRE(c.open("127.0.0.1", srv.port));
  REQUIRE(a.create("Ana", 3000, relay::ROOM_LOCKED | relay::ROOM_QUICK));
  REQUIRE(b.create("Beto"));
  relay::RoomInfo rooms[8];
  const int n = c.list(rooms, 8);
  REQUIRE(n == 2);
  int seen = 0;
  for (int i = 0; i < n; ++i) {
    if (std::string(rooms[i].host) == "Ana") { CHECK(rooms[i].flags == (relay::ROOM_LOCKED | relay::ROOM_QUICK)); seen++; }
    if (std::string(rooms[i].host) == "Beto") { CHECK(rooms[i].flags == 0); seen++; }
  }
  CHECK(seen == 2);
}

// Juego online: el servidor de relevo de verdad (relay.py del repo pokesurvivor-relay) y varias
// "consolas" en este programa que crean, buscan y entran en la sala por UDP y
// juegan con el lockstep a través del servidor.
#include <doctest/doctest.h>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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

// ---------------- el servidor como anfitrión (relay.py LOCK, 31-10-2026) ----------------
namespace {
// Una consola «en su casa»: todo lo que manda y recibe tarda la mitad de su ping
// (más un poco de vaivén), por encima de lo que tarde el servidor de verdad.
struct Laggy {
  relay::Client c;
  int half_ms = 0, jitter_ms = 0;
  uint32_t rng = 1;
  struct Pk { uint64_t due; uint16_t node; std::vector<uint8_t> d; };
  std::vector<Pk> out, in;
  int jit() { rng = rng * 1103515245u + 12345u; return jitter_ms ? int((rng >> 16) % uint32_t(jitter_ms + 1)) : 0; }
  void send(uint16_t to, const uint8_t* d, int n) { out.push_back({relay::now_ms() + uint64_t(half_ms + jit()), to, std::vector<uint8_t>(d, d + n)}); }
  void pump() {
    const uint64_t now = relay::now_ms();
    for (size_t k = 0; k < out.size();) {
      if (out[k].due <= now) { c.send(out[k].node, out[k].d.data(), int(out[k].d.size())); out.erase(out.begin() + long(k)); }
      else ++k;
    }
    uint8_t b[1400];
    uint16_t from;
    int n;
    while ((n = c.poll(b, sizeof(b), &from)) > 0) in.push_back({now + uint64_t(half_ms + jit()), from, std::vector<uint8_t>(b, b + n)});
  }
  // Lo que ya ha llegado (en orden de llegada).
  template <class F> void deliver(F f) {
    const uint64_t now = relay::now_ms();
    for (size_t k = 0; k < in.size();) {
      if (in[k].due <= now) { f(in[k].node, in[k].d.data(), int(in[k].d.size())); in.erase(in.begin() + long(k)); }
      else ++k;
    }
  }
};

struct OnlineResult {
  bool same = true, desync = false;
  int compared = 0;
  double delay_ms[kMaxPlayers] = {};   // latencia media de los controles de cada uno (lo que pulsa va tantos ms por delante)
  double stall[kMaxPlayers] = {};      // fracción de fotogramas parado esperando
  int64_t ticks[kMaxPlayers] = {};
};

// N consolas por el servidor de verdad, cada una con su ping. relay: el servidor
// junta las entradas; si no, el anfitrión (como hasta la 0.26.0).
// leave_at: el jugador 0 (el anfitrión) se va en ese fotograma (-1: nadie).
OnlineResult play_online(int port, int N, const int* rtt, bool relay, int frames, int leave_at = -1) {
  OnlineResult res;
  std::unique_ptr<Laggy> cl[kMaxPlayers];
  for (int i = 0; i < N; ++i) {
    cl[i].reset(new Laggy());
    cl[i]->half_ms = rtt[i] / 2; cl[i]->jitter_ms = rtt[i] / 8; cl[i]->rng = 7u + uint32_t(i);
    REQUIRE(cl[i]->c.open("127.0.0.1", port));
  }
  REQUIRE(cl[0]->c.create("J0"));
  for (int i = 1; i < N; ++i) REQUIRE(cl[i]->c.join(cl[0]->c.code(), ("J" + std::to_string(i)).c_str()));
  RunConfig cfg;
  cfg.seed = 4242; cfg.judged = 1; cfg.nplayers = N;
  static const int DEX[4] = {25, 4, 7, 1};
  for (int i = 0; i < N; ++i) { cfg.members[i].dex = DEX[i]; std::snprintf(cfg.members[i].name, 16, "J%d", i); }
  int worst = 0, maxrtt = 0;
  for (int i = 0; i < N; ++i) { if (i) worst = std::max(worst, rtt[i]); maxrtt = std::max(maxrtt, rtt[i]); }
  // (como coop_host_start)
  int delay = std::min(30, std::max(net::kDelay, int(std::ceil((rtt[0] + worst) / 16.7)) + 3));
  if (relay) {
    delay = std::min(30, std::max(net::kDelay / 2 + 1, int(std::ceil(maxrtt / 16.7)) + 3));
    uint16_t nodes[kMaxPlayers];
    for (int i = 0; i < N; ++i) nodes[i] = uint16_t(i + 1);
    REQUIRE(cl[0]->c.lock(net::wire_magic(), N, delay, nodes));
  }
  std::vector<std::unique_ptr<Game>> g;
  net::Lockstep ls[kMaxPlayers];
  Bot bot[kMaxPlayers];
  std::map<int64_t, uint32_t> hashes[kMaxPlayers];
  for (int i = 0; i < N; ++i) {
    g.emplace_back(new Game());
    g[size_t(i)]->local = i;
    g[size_t(i)]->debug_immortal = true;
    g[size_t(i)]->start_run(cfg);
    ls[i].begin(i, N, delay, relay);
    bot[i].init(300 + uint32_t(i));
  }
  bool left = false;
  int counted[kMaxPlayers] = {};
  double acc[kMaxPlayers] = {};
  const uint64_t t0 = relay::now_ms();
  for (int f = 0; f < frames; ++f) {
    if (f == leave_at) { cl[0]->c.leave(); left = true; }
    for (int i = 0; i < N; ++i) {
      if (left && i == 0) continue;
      Laggy& L = *cl[i];
      ls[i].set_rtt(rtt[i] + 2);   // (como el PING de la consola: su ping más el del servidor)
      L.pump();
      L.deliver([&](uint16_t from, const uint8_t* d, int n) { ls[i].on_packet(d, n, relay ? 0 : int(from) - 1); });
      // A 60 pasos por segundo de reloj de verdad (se pone al día hasta 3 por fotograma).
      acc[i] += 1;
      const double due = double(relay::now_ms() - t0) / (1000.0 / 60);
      int steps = 0;
      for (int s = 0; s < 3 && ls[i].tick() < due; ++s) {
        while (ls[i].want_local()) { Input in; bot[i].decide(*g[size_t(i)], in, i); ls[i].push_local(in); }
        if (!ls[i].step(*g[size_t(i)])) break;
        steps++;
        if (ls[i].tick() % net::kHashEvery == 0) hashes[i][ls[i].tick()] = g[size_t(i)]->state_hash();
      }
      // Tirón: un fotograma entero sin avanzar (esperando a alguien).
      if (!steps && f > 240 && ls[i].tick() < due - 1) res.stall[i] += 1;
      while (ls[i].want_local()) { Input in; bot[i].decide(*g[size_t(i)], in, i); ls[i].push_local(in); }
      net::Packet pk[2];
      const int np = ls[i].outbox(pk, 2);
      for (int k = 0; k < np; ++k) L.send(pk[k].to < 0 ? relay::kBroadcast : relay ? 0 : 1, pk[k].data, pk[k].len);
      L.pump();
      ls[i].tick_frame();
      // (la latencia de verdad: cuántos pasos por delante va lo que pulsa ahora)
      if (f > 240) { res.delay_ms[i] += ls[i].ahead() * (1000.0 / 60); counted[i]++; }
      if (std::getenv("PS_ONLINE_DEBUG") && i == 0 && f % 60 == 59)
        std::printf("  f %4d · J0 retraso %2d · por delante %2d · tick %lld · paradas J0 %d\n", f, ls[0].delay(), ls[0].ahead(), (long long)ls[0].tick(), ls[0].starved());
    }
    usleep(16000);
  }
  for (int i = 0; i < N; ++i) {
    if (counted[i]) res.delay_ms[i] /= counted[i];
    res.stall[i] /= std::max(1, frames - 240);
    res.ticks[i] = ls[i].tick();
    if (ls[i].desync()) res.desync = true;
  }
  for (int i = 1; i < N; ++i)
    for (auto& kv : hashes[i]) {
      for (int j = 0; j < i; ++j) {
        if (left && j == 0) continue;
        auto it = hashes[j].find(kv.first);
        if (it == hashes[j].end()) continue;
        res.compared++;
        if (it->second != kv.second) res.same = false;
      }
    }
  return res;
}
}  // namespace

TEST_CASE("online: el servidor junta las entradas (LOCK): 3 consolas no se separan y cada una su retraso") {
  if (!relay_available()) return;
  Server srv;
  REQUIRE(srv.start());
  const int rtt[3] = {0, 0, 0};
  const OnlineResult r = play_online(srv.port, 3, rtt, true, 60 * 12);
  CHECK(r.same);
  CHECK(!r.desync);
  CHECK(r.compared > 15);
  for (int i = 0; i < 3; ++i) CHECK(r.ticks[i] > 60 * 12 * 0.85);
  // (en la misma máquina: el servidor baja el retraso de todos al mínimo)
  for (int i = 0; i < 3; ++i) CHECK(r.delay_ms[i] < 8 * 16.7);
}

TEST_CASE("online: con el servidor de anfitrión, si se va el anfitrión los demás siguen") {
  if (!relay_available()) return;
  Server srv;
  REQUIRE(srv.start());
  const int rtt[3] = {0, 0, 0};
  const int frames = 60 * 12;
  const OnlineResult r = play_online(srv.port, 3, rtt, true, frames, 60 * 4);
  CHECK(r.same);
  CHECK(!r.desync);
  CHECK(r.ticks[1] > frames * 0.85);   // (siguen hasta el final, sin el anfitrión)
  CHECK(r.ticks[2] > frames * 0.85);
  CHECK(r.ticks[0] < frames * 0.5);
}

TEST_CASE("online: retraso de los controles, el anfitrión del juego contra el servidor de anfitrión (con ping)") {
  if (!relay_available()) return;
  Server srv;
  REQUIRE(srv.start());
  // Lo normal: dos, cada uno en su casa con su wifi (60 ms de ping).
  {
    const int rtt2[2] = {60, 60};
    const OnlineResult a = play_online(srv.port, 2, rtt2, false, 60 * 14);
    const OnlineResult b = play_online(srv.port, 2, rtt2, true, 60 * 14);
    MESSAGE("2 consolas (60/60 ms) · latencia de los controles (ms) · anfitrión: " << int(a.delay_ms[0]) << " / " << int(a.delay_ms[1])
            << " · servidor: " << int(b.delay_ms[0]) << " / " << int(b.delay_ms[1])
            << " · tirones (%): " << int(a.stall[0] * 100) << " / " << int(a.stall[1] * 100) << " -> " << int(b.stall[0] * 100) << " / " << int(b.stall[1] * 100));
    CHECK(a.same); CHECK(b.same); CHECK(!b.desync);
    CHECK(b.delay_ms[1] < a.delay_ms[1] * 0.75);              // (el invitado, mucho mejor)
    CHECK(std::abs(b.delay_ms[0] - b.delay_ms[1]) < 40);      // (y los dos, iguales)
    CHECK(b.stall[0] < 0.05); CHECK(b.stall[1] < 0.05);
  }
  // Dos en casa con wifi normal (70 ms de ping) y uno con wifi mala (140 ms).
  const int rtt[3] = {70, 70, 140};
  const int frames = 60 * 14;
  const OnlineResult a = play_online(srv.port, 3, rtt, false, frames);
  const OnlineResult b = play_online(srv.port, 3, rtt, true, frames);
  MESSAGE("retraso de los controles (ms) · anfitrión: " << int(a.delay_ms[0]) << " / " << int(a.delay_ms[1]) << " / " << int(a.delay_ms[2])
          << " · servidor: " << int(b.delay_ms[0]) << " / " << int(b.delay_ms[1]) << " / " << int(b.delay_ms[2]));
  MESSAGE("parado esperando (%) · anfitrión: " << int(a.stall[0] * 100) << " / " << int(a.stall[1] * 100) << " / " << int(a.stall[2] * 100)
          << " · servidor: " << int(b.stall[0] * 100) << " / " << int(b.stall[1] * 100) << " / " << int(b.stall[2] * 100));
  CHECK(a.same); CHECK(b.same);
  CHECK(!a.desync); CHECK(!b.desync);
  // Los invitados, mucho menos retraso; el que tiene buen ping, menos que el de mal ping.
  CHECK(b.delay_ms[1] < a.delay_ms[1] * 0.8);
  CHECK(b.delay_ms[2] < a.delay_ms[2] * 0.85);
  CHECK(b.delay_ms[1] < b.delay_ms[2]);
  for (int i = 0; i < 3; ++i) CHECK(b.stall[i] < 0.05);
}

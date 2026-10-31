// online_sim.h — el juego en grupo por internet, lo más parecido a la vida real sin
// consolas: el servidor de verdad (relay.py, por UDP) y varias «consolas» en este
// programa con el mismo bucle de fotogramas que la 3DS (app_coop.cpp: reloj, reloj
// elástico, hasta 4 pasos por fotograma), cada una con su red de casa (ping, vaivén,
// picos de wifi y paquetes perdidos) y su reloj, que nunca va exacto al de las demás.
// Lo usan tests/test_relay.cpp y la herramienta ps_red (en un build sin sanitizers,
// para que la CPU no meta ruido en las medidas).
#pragma once
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/bot.h"
#include "core/game.h"
#include "core/lockstep.h"
#include "core/relay_client.h"

namespace ps {
namespace online_sim {

// El servidor de verdad, en otro proceso.
struct RelayServer {
  pid_t pid = -1;
  int port = 0;
  bool start(const char* py, int p) {
    port = p;
    pid = fork();
    if (pid == 0) {
      const std::string ps = std::to_string(port);
      execlp("python3", "python3", py, "--port", ps.c_str(), "--host", "127.0.0.1", (char*)nullptr);
      _exit(127);
    }
    usleep(700 * 1000);
    return pid > 0;
  }
  ~RelayServer() { if (pid > 0) { kill(pid, SIGTERM); waitpid(pid, nullptr, 0); } }
};

// La casa de cada consola.
struct Home {
  int rtt = 0;          // ping con el servidor (ms)
  int jitter = 0;       // vaivén de cada paquete, en cada sentido (0..jitter ms)
  double spike = 0;     // probabilidad de un pico de wifi (+4 × vaivén)
  double loss = 0;      // paquetes perdidos
  double clock = 1.0;   // su reloj (1,003: un 0,3 % más deprisa)
};

// Todo lo que manda y recibe una consola pasa por su casa.
struct Laggy {
  relay::Client c;
  Home h;
  uint64_t rng = 88172645463325252ull;
  struct Pk { uint64_t due; uint16_t node; std::vector<uint8_t> d; };
  std::vector<Pk> out, in;
  double rnd() { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return double(rng % 1000000) / 1000000.0; }
  bool lost() { return rnd() < h.loss; }
  uint64_t lag() {
    double d = h.rtt / 2.0 + rnd() * h.jitter;
    if (rnd() < h.spike) d += 4.0 * h.jitter;
    return uint64_t(d);
  }
  void send(uint16_t to, const uint8_t* d, int n) { if (!lost()) out.push_back({relay::now_ms() + lag(), to, std::vector<uint8_t>(d, d + n)}); }
  void pump() {
    const uint64_t now = relay::now_ms();
    for (size_t k = 0; k < out.size();) {
      if (out[k].due <= now) { c.send(out[k].node, out[k].d.data(), int(out[k].d.size())); out.erase(out.begin() + long(k)); }
      else ++k;
    }
    uint8_t b[1400];
    uint16_t from;
    int n;
    while ((n = c.poll(b, sizeof(b), &from)) > 0)
      if (!lost()) in.push_back({now + lag(), from, std::vector<uint8_t>(b, b + n)});
  }
  template <class F> void deliver(F f) {
    const uint64_t now = relay::now_ms();
    for (size_t k = 0; k < in.size();) {
      if (in[k].due <= now) { f(in[k].node, in[k].d.data(), int(in[k].d.size())); in.erase(in.begin() + long(k)); }
      else ++k;
    }
  }
};

struct Result {
  bool same = true, desync = false;
  int compared = 0;
  double ahead_ms[kMaxPlayers] = {};   // latencia de los controles: lo que pulsas va tantos ms por delante
  double hitches[kMaxPlayers] = {};    // tirones por minuto (fotogramas sin avanzar ni un paso)
  double bursts[kMaxPlayers] = {};     // saltos por minuto (fotogramas con 2 pasos o más)
  double speed[kMaxPlayers] = {};      // pasos por segundo de verdad (60 = a su velocidad)
  int64_t ticks[kMaxPlayers] = {};
  int delay[kMaxPlayers] = {};
};

// N consolas jugando `seconds` segundos. relay: el servidor junta las entradas (si
// no, el anfitrión, como hasta la 0.26.0). leave_s: el anfitrión se va a esos
// segundos (-1: nadie). cushion: colchón del reloj elástico (0: apagado).
inline Result play(int port, int N, const Home* home, bool relay, int seconds, int leave_s = -1, int cushion = 3, uint32_t seed = 4242) {
  constexpr double kStep = 1000.0 / 60;
  Result res;
  std::unique_ptr<Laggy> cl[kMaxPlayers];
  for (int i = 0; i < N; ++i) {
    cl[i].reset(new Laggy());
    cl[i]->h = home[i];
    cl[i]->rng ^= uint64_t(seed) * 2654435761ull + uint64_t(i) * 97;
    if (!cl[i]->c.open("127.0.0.1", port)) return res;
  }
  if (!cl[0]->c.create("J0")) return res;
  for (int i = 1; i < N; ++i) if (!cl[i]->c.join(cl[0]->c.code(), ("J" + std::to_string(i)).c_str())) return res;
  RunConfig cfg;
  cfg.seed = seed; cfg.judged = 1; cfg.nplayers = N;
  static const int DEX[4] = {25, 4, 7, 1};
  for (int i = 0; i < N; ++i) { cfg.members[i].dex = DEX[i]; std::snprintf(cfg.members[i].name, 16, "J%d", i); }
  int worst = 0, maxrtt = 0;
  for (int i = 0; i < N; ++i) { if (i) worst = std::max(worst, home[i].rtt); maxrtt = std::max(maxrtt, home[i].rtt); }
  // (como coop_host_start)
  int delay = std::min(30, std::max(net::kDelay, int(std::ceil((home[0].rtt + worst) / 16.7)) + 3));
  if (relay) {
    delay = std::min(30, std::max(net::kDelay / 2 + 1, int(std::ceil(maxrtt / 16.7)) + 3));
    uint16_t nodes[kMaxPlayers];
    for (int i = 0; i < N; ++i) nodes[i] = uint16_t(i + 1);
    if (!cl[0]->c.lock(net::wire_magic(), N, delay, nodes)) return res;
  }
  std::vector<std::unique_ptr<Game>> g;
  net::Lockstep ls[kMaxPlayers];
  Bot bot[kMaxPlayers];
  std::map<int64_t, uint32_t> hashes[kMaxPlayers];
  double acc[kMaxPlayers] = {};
  for (int i = 0; i < N; ++i) {
    g.emplace_back(new Game());
    g[size_t(i)]->local = i;
    g[size_t(i)]->debug_immortal = true;
    g[size_t(i)]->start_run(cfg);
    ls[i].begin(i, N, delay, relay);
    ls[i].set_cushion(cushion);
    bot[i].init(300 + uint32_t(i));
  }
  const int frames = seconds * 60, warm = 5 * 60;
  bool left = false;
  int64_t tick_warm[kMaxPlayers] = {};
  int st_warm[kMaxPlayers] = {}, bu_warm[kMaxPlayers] = {}, counted[kMaxPlayers] = {};
  const uint64_t t0 = relay::now_ms();
  uint64_t last = t0, t_warm = 0;
  for (int f = 0; f < frames; ++f) {
    // Un fotograma cada 16,7 ms de verdad (sin acumular retraso).
    const uint64_t due = t0 + uint64_t(f * kStep);
    const uint64_t now0 = relay::now_ms();
    if (due > now0) usleep(useconds_t((due - now0) * 1000));
    const uint64_t now = relay::now_ms();
    const double dt = double(now - last);
    last = now;
    if (f == warm) {
      t_warm = now;
      for (int i = 0; i < N; ++i) { tick_warm[i] = ls[i].tick(); st_warm[i] = ls[i].starved(); bu_warm[i] = ls[i].bursts(); }
    }
    if (leave_s >= 0 && f == leave_s * 60) { cl[0]->c.leave(); left = true; }
    for (int i = 0; i < N; ++i) {
      if (left && i == 0) continue;
      Laggy& L = *cl[i];
      // (app_coop.cpp: App::advance)
      L.pump();
      L.deliver([&](uint16_t from, const uint8_t* d, int n) {
        if (relay) { if (from == 0) ls[i].on_packet(d, n, 0); }
        else ls[i].on_packet(d, n, int(from) - 1);
      });
      if (relay) ls[i].set_rtt(L.h.rtt + L.h.jitter);
      acc[i] += dt * L.h.clock;
      acc[i] -= dt * L.h.clock * (1.0 - ls[i].clock_rate());
      int steps = 0;
      while (acc[i] >= kStep && steps < 4) {
        while (ls[i].want_local()) { Input in; bot[i].decide(*g[size_t(i)], in, i); ls[i].push_local(in); }
        if (!ls[i].step(*g[size_t(i)])) break;
        if (ls[i].tick() % net::kHashEvery == 0) hashes[i][ls[i].tick()] = g[size_t(i)]->state_hash();
        acc[i] -= kStep;
        steps++;
      }
      if (acc[i] > kStep * 4) acc[i] = kStep * 4;
      net::Packet pk[2];
      const int np = ls[i].outbox(pk, 2);
      for (int k = 0; k < np; ++k) L.send(pk[k].to < 0 ? relay::kBroadcast : relay ? 0 : 1, pk[k].data, pk[k].len);
      L.pump();
      ls[i].tick_frame();
      if (f >= warm) { res.ahead_ms[i] += ls[i].ahead() * kStep; counted[i]++; }
    }
  }
  const double mins = double(relay::now_ms() - t_warm) / 60000.0;
  for (int i = 0; i < N; ++i) {
    if (counted[i]) res.ahead_ms[i] /= counted[i];
    res.hitches[i] = (ls[i].starved() - st_warm[i]) / mins;
    res.bursts[i] = (ls[i].bursts() - bu_warm[i]) / mins;
    res.speed[i] = double(ls[i].tick() - tick_warm[i]) / (mins * 60);
    res.ticks[i] = ls[i].tick();
    res.delay[i] = ls[i].delay();
    if (ls[i].desync()) res.desync = true;
  }
  for (int i = 1; i < N; ++i)
    for (auto& kv : hashes[i])
      for (int j = 0; j < i; ++j) {
        if (left && j == 0) continue;
        auto it = hashes[j].find(kv.first);
        if (it == hashes[j].end()) continue;
        res.compared++;
        if (it->second != kv.second) res.same = false;
      }
  return res;
}

}  // namespace online_sim
}  // namespace ps

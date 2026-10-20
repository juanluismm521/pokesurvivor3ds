// Lockstep: formato de los mensajes y varias consolas por una red falsa con
// retraso, pérdidas, duplicados y desorden.
#include <doctest/doctest.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <map>
#include <memory>
#include <vector>

#include "core/bot.h"
#include "core/game.h"
#include "core/lockstep.h"

using namespace ps;
using namespace ps::net;

TEST_CASE("lockstep: entradas y sala ida y vuelta") {
  Input in;
  in.ax = 0.5f; in.ay = -1; in.select = 2; in.cycle = -1; in.cmd = CMD_BOND; in.arg = 2; in.moves[0] = 3; in.moves[1] = 5;
  const Input q = quantize(in);
  CHECK(q.ax == doctest::Approx(64 / 127.0f));
  CHECK(q.ay == -1.0f);
  CHECK(q.select == 2); CHECK(q.cycle == -1); CHECK(q.cmd == CMD_BOND); CHECK(q.arg == 2);
  CHECK(q.moves[0] == 3); CHECK(q.moves[1] == 5);
  CHECK(quantize(q).ax == q.ax);   // estable
  RunConfig c;
  c.seed = 123456; c.judged = 2; c.nplayers = 3;
  c.legends_beaten[5] = 0x81;
  for (int i = 0; i < 3; ++i) { c.members[i].dex = 100 + i; c.members[i].upgrades[1] = uint8_t(i); std::snprintf(c.members[i].name, 16, "Jugador%d", i); }
  c.members[2].shiny = true; c.members[1].pbind[2] = 7;
  uint8_t buf[1024];
  const int n = write_config(c, buf, sizeof(buf));
  REQUIRE(n > 0);
  RunConfig d;
  REQUIRE(read_config(buf, n, d));
  CHECK(d.seed == 123456u); CHECK(d.judged == 2); CHECK(d.nplayers == 3);
  CHECK(d.legends_beaten[5] == 0x81);
  CHECK(d.members[1].dex == 101); CHECK(d.members[2].shiny); CHECK(d.members[1].pbind[2] == 7);
  CHECK(std::string(d.members[2].name) == "Jugador2");
  CHECK(!read_config(buf, n - 5, d));   // cortado: no vale
}

namespace {

struct Wire { int due; int from, to; std::vector<uint8_t> data; };

struct Net {
  Rng rng{99};
  float loss = 0, dup = 0;
  int lat_min = 1, lat_max = 1;
  std::vector<Wire> q;
  bool cut[kMaxPlayers] = {};    // consola desconectada: ni manda ni recibe
  int lag[kMaxPlayers] = {};     // fotogramas de más en todo lo que va o viene de esa consola (su ping)
  void send(int frame, int from, int to, const uint8_t* d, int len) {
    if (cut[from] || cut[to]) return;
    const int copies = rng.next() < dup ? 2 : 1;
    for (int c = 0; c < copies; ++c) {
      if (rng.next() < loss) continue;
      q.push_back({frame + rng.range_int(lat_min, lat_max) + lag[from] + lag[to], from, to, std::vector<uint8_t>(d, d + len)});
    }
  }
};

struct Console {
  std::unique_ptr<Game> g{new Game()};
  Lockstep ls;
  Bot bot;
  std::map<int64_t, uint32_t> hashes;
};

struct Result { bool same = true; int compared = 0; int64_t min_tick = 0; bool desync = false; };

// Juega `frames` fotogramas con n consolas. `on_frame` puede tocar la red.
template <class F>
Result play(int n, uint32_t seed, int frames, Net& net, std::vector<Console>& cs, F on_frame) {
  RunConfig cfg;
  cfg.seed = seed; cfg.judged = 1; cfg.nplayers = n;
  static const int DEX[4] = {25, 4, 7, 1};
  for (int i = 0; i < n; ++i) { cfg.members[i].dex = DEX[i]; std::snprintf(cfg.members[i].name, 16, "J%d", i); }
  cs.clear();
  cs.resize(size_t(n));
  for (int i = 0; i < n; ++i) {
    Console& c = cs[size_t(i)];
    c.g->local = i;
    c.g->debug_immortal = true;
    c.g->start_run(cfg);
    c.ls.begin(i, n);
    c.bot.init(seed + 17 * i);
  }
  for (int f = 0; f < frames; ++f) {
    on_frame(f);
    // Llega lo que toca.
    std::vector<Wire> later;
    std::vector<Wire> now;
    for (auto& w : net.q) (w.due <= f ? now : later).push_back(w);
    net.q.swap(later);
    // (desordenados entre sí)
    for (size_t k = 1; k < now.size(); ++k) std::swap(now[k], now[size_t(net.rng.range_int(0, int(k)))]);
    for (auto& w : now) cs[size_t(w.to)].ls.on_packet(w.data.data(), int(w.data.size()), w.from);
    for (int i = 0; i < n; ++i) {
      Console& c = cs[size_t(i)];
      if (net.cut[i]) continue;   // (consola "apagada")
      while (c.ls.want_local()) { Input in; c.bot.decide(*c.g, in, i); c.ls.push_local(in); }
      // A 60 pasos por segundo, como la consola (se pone al día si va atrasada, hasta 3 por fotograma).
      for (int s = 0; s < 3 && c.ls.ready() && c.ls.tick() <= f; ++s) {
        c.ls.step(*c.g);
        if (c.ls.tick() % kHashEvery == 0) c.hashes[c.ls.tick()] = c.g->state_hash();
        if (c.ls.want_local()) { Input in; c.bot.decide(*c.g, in, i); c.ls.push_local(in); }
      }
      Packet pk[2];
      const int np = c.ls.outbox(pk, 2);
      for (int k = 0; k < np; ++k) {
        if (pk[k].to < 0) { for (int j = 1; j < n; ++j) net.send(f, i, j, pk[k].data, pk[k].len); }
        else net.send(f, i, pk[k].to, pk[k].data, pk[k].len);
      }
      c.ls.tick_frame();
    }
  }
  Result r;
  r.min_tick = 1 << 30;
  for (int i = 0; i < n; ++i) {
    if (net.cut[i]) continue;
    r.min_tick = std::min<int64_t>(r.min_tick, cs[size_t(i)].ls.tick());
    if (cs[size_t(i)].ls.desync()) r.desync = true;
    for (auto& kv : cs[size_t(i)].hashes) {
      for (int j = 0; j < i; ++j) {
        if (net.cut[j]) continue;
        auto it = cs[size_t(j)].hashes.find(kv.first);
        if (it == cs[size_t(j)].hashes.end()) continue;
        r.compared++;
        if (it->second != kv.second) r.same = false;
      }
    }
  }
  return r;
}

}  // namespace

TEST_CASE("lockstep: 2 consolas en red perfecta van a la par") {
  Net net;
  std::vector<Console> cs;
  const Result r = play(2, 7, 60 * 90, net, cs, [](int) {});
  CHECK(r.same);
  CHECK(!r.desync);
  CHECK(r.compared > 80);
  CHECK(r.min_tick > 60 * 90 - 30);   // casi sin esperas
}

TEST_CASE("lockstep: 4 consolas con retraso, pérdidas, duplicados y desorden") {
  {  // radio local normal (1-3 fotogramas) con muchas pérdidas: casi a velocidad normal
    Net net;
    net.loss = 0.2f; net.dup = 0.1f; net.lat_min = 1; net.lat_max = 3;
    std::vector<Console> cs;
    const Result r = play(4, 13, 60 * 150, net, cs, [](int) {});
    CHECK(r.same);
    CHECK(!r.desync);
    CHECK(r.compared > 300);
    CHECK(r.min_tick > 60 * 150 * 0.85);
  }
  {  // red terrible (hasta 100 ms por trayecto): va más lenta, pero no se separa ni se para
    Net net;
    net.loss = 0.3f; net.dup = 0.2f; net.lat_min = 1; net.lat_max = 6;
    std::vector<Console> cs;
    const Result r = play(3, 14, 60 * 120, net, cs, [](int) {});
    CHECK(r.same);
    CHECK(!r.desync);
    CHECK(r.min_tick > 60 * 120 * 0.3);
  }
}

TEST_CASE("lockstep: uno con mucho ping no frena a los demás (retraso de cada uno)") {
  // Tres por internet; el invitado 2 con 200 ms por trayecto (400 ms ida y vuelta).
  Net net;
  net.lat_min = 2; net.lat_max = 4;
  net.lag[2] = 12;
  std::vector<Console> cs;
  const Result r = play(3, 21, 60 * 120, net, cs, [](int) {});
  CHECK(r.same);
  CHECK(!r.desync);
  MESSAGE("anfitrión " << cs[0].ls.tick() << " · invitado 1 " << cs[1].ls.tick() << " · invitado 2 " << cs[2].ls.tick()
          << " pasos de " << 60 * 120 << " · retraso: anfitrión " << cs[0].ls.delay() << ", invitado 1 " << cs[1].ls.delay() << ", invitado 2 " << cs[2].ls.delay());
  CHECK(cs[0].ls.tick() > 60 * 120 * 0.95);   // el anfitrión casi a velocidad normal
  CHECK(cs[1].ls.tick() > 60 * 120 * 0.95);   // y el otro invitado también
  CHECK(cs[2].ls.delay() >= 24);              // el del ping alto se adelanta más (su retraso, sólo suyo: ~400 ms)
  CHECK(cs[1].ls.delay() <= 14);              // los demás, con el suyo pequeño
  CHECK(cs[0].ls.delay() == kDelay);          // y el anfitrión, el mínimo
}

TEST_CASE("lockstep: si una consola se desconecta, el anfitrión la quita y los demás siguen") {
  Net net;
  net.lat_max = 3;
  std::vector<Console> cs;
  const int cut_at = 60 * 20;
  const Result r = play(3, 21, cut_at + kTimeoutFrames + 60 * 30, net, cs, [&](int f) { if (f == cut_at) net.cut[2] = true; });
  CHECK(r.same);
  CHECK(!r.desync);
  CHECK(!cs[0].g->players[2].active);
  CHECK(!cs[1].g->players[2].active);
  CHECK(cs[0].g->players[1].active);
  CHECK(r.min_tick > cut_at + 60 * 25);   // tras la espera, siguen jugando
  CHECK(cs[1].ls.tick() == doctest::Approx(double(cs[0].ls.tick())).epsilon(0.01));
}

TEST_CASE("lockstep: una consola que se separa se detecta") {
  Net net;
  std::vector<Console> cs;
  bool touched = false;
  const Result r = play(2, 5, 60 * 30, net, cs, [&](int f) {
    // (sólo en una; algo que no se borra solo: un empujón de la posición
    // desaparece si en ese momento el Pokémon está pegado a una pared)
    if (f == 60 * 10 && !touched) { cs[1].g->players[0].kills++; touched = true; }
  });
  CHECK(!r.same);
  CHECK(cs[0].ls.desync());
  CHECK(cs[1].ls.desync());
}

TEST_CASE("lockstep: si se apaga el anfitrión, los invitados lo saben") {
  Net net;
  std::vector<Console> cs;
  play(2, 9, 60 * 5 + kTimeoutFrames + 30, net, cs, [&](int f) { if (f == 60 * 5) net.cut[0] = true; });
  CHECK(cs[1].ls.lost_host());
  CHECK(!cs[1].ls.desync());
}

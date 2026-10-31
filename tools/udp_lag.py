#!/usr/bin/env python3
"""udp_lag.py — red de casa de mentira entre una consola (emulador) y el servidor de
relevo, para probar el juego en grupo como en la vida real: ping, vaivén (jitter, con
algún pico como el wifi), paquetes perdidos y alguno desordenado.

    python3 tools/udp_lag.py --listen 25001 --to 127.0.0.1:24874 --rtt 70 --jitter 15 --loss 0.01

La consola se conecta a 127.0.0.1:25001 (COOP_SERVER_A=127.0.0.1:25001 en emu_coop.sh) y
este programa reenvía cada paquete al servidor y las respuestas a la consola, cada uno
tras la mitad del ping más su vaivén. Al acabar (SIGTERM) dice cuántos pasaron.
"""
import argparse
import asyncio
import random
import signal


class Lag:
    def __init__(self, a, loop):
        self.a, self.loop = a, loop
        self.client = None                     # (dirección de la consola)
        self.down = None                       # socket hacia la consola
        self.up = None                         # socket hacia el servidor
        self.n = {"sube": 0, "baja": 0, "perdidos": 0}

    def delay(self):
        d = self.a.rtt / 2 + random.uniform(0, self.a.jitter)
        if random.random() < self.a.spike:     # (un pico de wifi de vez en cuando)
            d += self.a.jitter * 4
        return d / 1000.0

    def later(self, fn):
        if random.random() < self.a.loss:
            self.n["perdidos"] += 1
            return
        self.loop.call_later(self.delay(), fn)


class Down(asyncio.DatagramProtocol):
    def __init__(self, lag): self.lag = lag
    def connection_made(self, t): self.lag.down = t
    def datagram_received(self, data, addr):
        self.lag.client = addr
        self.lag.n["sube"] += 1
        self.lag.later(lambda: self.lag.up.sendto(data))


class Up(asyncio.DatagramProtocol):
    def __init__(self, lag): self.lag = lag
    def connection_made(self, t): self.lag.up = t
    def datagram_received(self, data, addr):
        if self.lag.client:
            self.lag.n["baja"] += 1
            c = self.lag.client
            self.lag.later(lambda: self.lag.down.sendto(data, c))


async def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--listen", type=int, required=True)
    ap.add_argument("--to", required=True)
    ap.add_argument("--rtt", type=float, default=70)
    ap.add_argument("--jitter", type=float, default=15)
    ap.add_argument("--spike", type=float, default=0.01)
    ap.add_argument("--loss", type=float, default=0.01)
    a = ap.parse_args()
    host, port = a.to.rsplit(":", 1)
    loop = asyncio.get_running_loop()
    lag = Lag(a, loop)
    await loop.create_datagram_endpoint(lambda: Down(lag), local_addr=("0.0.0.0", a.listen))
    await loop.create_datagram_endpoint(lambda: Up(lag), remote_addr=(host, int(port)))
    stop = loop.create_future()
    loop.add_signal_handler(signal.SIGTERM, stop.set_result, None)
    print(f"udp_lag: :{a.listen} -> {a.to} · ping {a.rtt} ms ± {a.jitter} · pérdidas {a.loss:.0%}", flush=True)
    await stop
    print(f"udp_lag: {lag.n}", flush=True)


if __name__ == "__main__":
    asyncio.run(main())

#!/usr/bin/env python3
"""relay.py — servidor de relevo para jugar online con la 3DS (cooperativo).

La 3DS no puede hablar directamente con otra por internet (cada una está
detrás de su router), así que las dos le mandan sus paquetes a este servidor,
que los reenvía a los demás de la misma sala. Lo que viaja es lo mismo que en
el juego local (sala + lockstep): el servidor no entiende la partida, sólo
reenvía.

Mensajes (UDP; todos empiezan por b"PSR1" y un byte de tipo):
  consola -> servidor
    HOST  nombre          crear sala            -> ROOM código, nodo 1
    JOIN  código nombre   entrar en una sala    -> JOINED código, nodo (2..4)
    LIST  (relleno)       salas abiertas        -> ROOMS [(código, anfitrión, jugadores)]
    SEND  nodo, datos     a un nodo (0xFFFF: a todos los demás)
    PING  t               (cada segundo; mantiene la sala)  -> PONG t, máscara de nodos
    INFO  abierta, n      el anfitrión cierra la sala al empezar
    BYE
  servidor -> consola
    DATA  nodo de origen, datos
    PEERS máscara de nodos (bit 0 = nodo 1)
    ERR   texto

Seguridad: el servidor nunca contesta más grande de lo que recibe (LIST se
pide con relleno), límites de paquetes por dirección, de tamaño, de salas, y
quien no da noticias en 20 s sale de la sala.

    python3 server/relay.py [--port 24873]
"""
import argparse
import asyncio
import random
import struct
import time

MAGIC = b"PSR1"
HOST, JOIN, LIST, SEND, PING, INFO, BYE = 1, 2, 3, 4, 5, 6, 7
ROOM, JOINED, ROOMS, DATA, PEERS, PONG, ERR = 101, 102, 103, 104, 105, 106, 107
BROADCAST = 0xFFFF
MAX_PAYLOAD = 1200
MAX_ROOMS = 500
MAX_NODES = 4
TIMEOUT = 20.0
RATE = 400            # paquetes por segundo y dirección
LETTERS = "ABCDEFGHJKLMNPQRSTUVWXYZ"   # (sin I ni O, que se confunden)


class Room:
    def __init__(self, code, host_name):
        self.code = code
        self.host_name = host_name
        self.members = {}        # nodo -> dirección
        self.seen = {}           # nodo -> hora
        self.open = True


class Relay(asyncio.DatagramProtocol):
    def __init__(self):
        self.rooms = {}          # código -> Room
        self.where = {}          # dirección -> (código, nodo)
        self.rate = {}           # ip -> (segundo, cuenta)
        self.transport = None
        self.stats = {"in": 0, "out": 0, "drop": 0}

    def connection_made(self, transport):
        self.transport = transport

    # ---------- ayudas ----------
    def out(self, addr, typ, payload=b"", limit=None):
        pk = MAGIC + bytes([typ]) + payload
        if limit is not None and len(pk) > limit:   # nunca más grande que lo recibido
            pk = pk[:limit]
        self.transport.sendto(pk, addr)
        self.stats["out"] += 1

    def mask(self, room):
        m = 0
        for n in room.members:
            m |= 1 << (n - 1)
        return m

    def peers(self, room):
        for addr in room.members.values():
            self.out(addr, PEERS, struct.pack("<H", self.mask(room)))

    def leave(self, addr):
        w = self.where.pop(addr, None)
        if not w:
            return
        room = self.rooms.get(w[0])
        if not room:
            return
        room.members.pop(w[1], None)
        room.seen.pop(w[1], None)
        if w[1] == 1 or not room.members:      # sin anfitrión no hay sala
            for a in list(room.members.values()):
                self.where.pop(a, None)
                self.out(a, ERR, "El anfitrión ha cerrado la sala".encode())
            del self.rooms[room.code]
        else:
            self.peers(room)

    def allowed(self, addr):
        now = int(time.monotonic())
        sec, n = self.rate.get(addr[0], (now, 0))
        if sec != now:
            sec, n = now, 0
        n += 1
        self.rate[addr[0]] = (sec, n)
        return n <= RATE

    # ---------- entrada ----------
    def datagram_received(self, data, addr):
        self.stats["in"] += 1
        if len(data) < 5 or data[:4] != MAGIC or len(data) > MAX_PAYLOAD + 64 or not self.allowed(addr):
            self.stats["drop"] += 1
            return
        typ, body = data[4], data[5:]
        try:
            self.handle(typ, body, addr, len(data))
        except (struct.error, UnicodeDecodeError, IndexError):
            self.stats["drop"] += 1

    def handle(self, typ, body, addr, size):
        now = time.monotonic()
        w = self.where.get(addr)
        if w and w[0] in self.rooms:
            self.rooms[w[0]].seen[w[1]] = now
        if typ == HOST:
            self.leave(addr)
            if len(self.rooms) >= MAX_ROOMS:
                self.out(addr, ERR, b"Servidor lleno")
                return
            code = "".join(random.choice(LETTERS) for _ in range(4))
            while code in self.rooms:
                code = "".join(random.choice(LETTERS) for _ in range(4))
            room = Room(code, body[:15].decode("utf-8", "replace"))
            room.members[1] = addr
            room.seen[1] = now
            self.rooms[code] = room
            self.where[addr] = (code, 1)
            self.out(addr, ROOM, code.encode() + bytes([1]))
            self.peers(room)
        elif typ == JOIN:
            code = body[:4].decode("ascii", "replace").upper()
            room = self.rooms.get(code)
            if w and w[0] == code:                       # (ya está: se repite la respuesta)
                self.out(addr, JOINED, code.encode() + bytes([w[1]]))
                return
            if not room or not room.open:
                self.out(addr, ERR, "Esa sala no existe o ya ha empezado".encode(), limit=size)
                return
            free = [n for n in range(2, MAX_NODES + 1) if n not in room.members]
            if not free:
                self.out(addr, ERR, b"Sala llena", limit=size)
                return
            self.leave(addr)
            node = free[0]
            room.members[node] = addr
            room.seen[node] = now
            self.where[addr] = (code, node)
            self.out(addr, JOINED, code.encode() + bytes([node]))
            self.peers(room)
        elif typ == LIST:
            rooms = [r for r in self.rooms.values() if r.open][:20]
            payload = bytes([len(rooms)])
            for r in rooms:
                name = r.host_name.encode()[:15].ljust(16, b"\0")
                payload += r.code.encode() + name + bytes([len(r.members)])
            self.out(addr, ROOMS, payload, limit=size)   # (la consola rellena la petición)
        elif typ == SEND:
            if not w or w[0] not in self.rooms:
                return
            room = self.rooms[w[0]]
            to = struct.unpack_from("<H", body)[0]
            payload = struct.pack("<H", w[1]) + body[2:]
            if to == BROADCAST:
                for n, a in room.members.items():
                    if n != w[1]:
                        self.out(a, DATA, payload)
            elif to in room.members and to != w[1]:
                self.out(room.members[to], DATA, payload)
        elif typ == PING:
            m = self.mask(self.rooms[w[0]]) if w and w[0] in self.rooms else 0
            self.out(addr, PONG, body[:8] + struct.pack("<H", m), limit=max(size, 15))
        elif typ == INFO:
            if w and w[1] == 1 and w[0] in self.rooms:
                self.rooms[w[0]].open = bool(body[0])
        elif typ == BYE:
            self.leave(addr)

    # ---------- limpieza ----------
    async def sweep(self):
        while True:
            await asyncio.sleep(2)
            now = time.monotonic()
            for room in list(self.rooms.values()):
                for n, t in list(room.seen.items()):
                    if now - t > TIMEOUT and n in room.members:
                        self.leave(room.members[n])
            if len(self.rate) > 10000:
                self.rate.clear()


async def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=24873)
    ap.add_argument("--host", default="0.0.0.0")
    a = ap.parse_args()
    loop = asyncio.get_running_loop()
    relay = Relay()
    await loop.create_datagram_endpoint(lambda: relay, local_addr=(a.host, a.port))
    print(f"relay: escuchando en UDP {a.host}:{a.port}", flush=True)
    await relay.sweep()


if __name__ == "__main__":
    asyncio.run(main())

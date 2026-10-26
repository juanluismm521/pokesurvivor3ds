#!/usr/bin/env bash
# Prueba del cooperativo con DOS emuladores Azahar en la misma sala local:
#   - servidor de salas oficial de Azahar (imagen Docker azahar-room)
#   - dos Azahar, cada uno en su Xvfb (:97 y :98) con su configuración y su
#     SD (nombre de consola, MAC e id de consola distintos: si no, la sala
#     no deja entrar al segundo)
#   - cada uno entra en la sala con xdotool (Multiplayer → Direct Connect)
#   - el juego arranca en modo autotest cooperativo: uno crea la sala del
#     juego, el otro la busca y se une; los bots juegan y cada uno escribe
#     la huella del estado cada 10 s en su autotest.log
# Al acabar compara las huellas (tienen que ser iguales) y saca capturas.
#
# Uso: tools/emu_coop.sh [archivo.3dsx] [segundos de partida] [carpeta]
# Con COOP_ONLINE=1, por internet (servidor de relevo del VPS, repo pokesurvivor-relay):
# sin sala de Azahar (la red de la consola emulada sale directa).
set -uo pipefail
cd "$(dirname "$0")/.."
ROM=$(realpath "${1:-pokesurvivor-3ds.3dsx}")
SECS=${2:-120}
OUT=${3:-captures/coop}
W=$(mktemp -d /tmp/ps-coop.XXXX)
mkdir -p "$OUT"
APP=/opt/azahar/squashfs-root/AppRun
ROOM_IMG=azahar-room:2126.2

ONLINE=${COOP_ONLINE:-}
# --- servidor de salas (sólo para el juego local) ---
if [ -z "$ONLINE" ] && ! docker image inspect $ROOM_IMG >/dev/null 2>&1; then
  curl -sL -o "$W/room.img" https://github.com/azahar-emu/azahar/releases/download/2126.2/azahar-room-x86_64-2126.2.dockerimage
  docker load -i "$W/room.img" >/dev/null
fi
docker rm -f ps-room >/dev/null 2>&1
if [ -z "$ONLINE" ]; then
  docker run -d --name ps-room --network host $ROOM_IMG azahar-room --room-name prueba --port 24872 --max_members 4 --preferred-app PokeSurvivor --preferred-app-id 0 >/dev/null
  sleep 2
fi

# --- dos "consolas" ---
cp "$ROM" "$W/game.3dsx"
setup() {   # $1 = A|B, $2 = rol, $3 = dex
  local D="$W/$1"
  mkdir -p "$D/config" "$D/data"
  cp -r ~/.config/azahar-emu "$D/config/"
  cp -r ~/.local/share/azahar-emu "$D/data/"
  sed -i 's/^Multiplayer\\ip=.*/Multiplayer\\ip=127.0.0.1/; s/^Multiplayer\\ip\\default=true/Multiplayer\\ip\\default=false/' "$D/config/azahar-emu/qt-config.ini"
  # COOP_DUMP=carpeta: graba las dos consolas en vídeo (RGB sin pérdidas) en carpeta/A.mkv y B.mkv
  if [ -n "${COOP_DUMP:-}" ]; then
    python3 - "$D/config/azahar-emu/qt-config.ini" <<'PY'
import re, sys
p = sys.argv[1]; s = open(p).read()
# (la tasa alta hace que salga casi sin pérdidas: Azahar no deja pasar opciones con comas al códec)
for k, v in {'output_format': 'matroska', 'video_encoder': 'libx264rgb', 'video_bitrate': '400000000',
             'audio_encoder': 'pcm_s16le', 'audio_encoder_options': ''}.items():
    s = re.sub(r'(?m)^' + k + r'=.*$', k + '=' + v, s).replace(k + '\\default=true', k + '\\default=false')
open(p, 'w').write(s)
PY
  fi
  local SD="$D/data/azahar-emu/sdmc/3ds/pokesurvivor-3ds"
  mkdir -p "$SD"; rm -f "$SD/autotest.log"
  # COOP_MORIR=1: el invitado no es inmortal y se deja matar (para ver al caído
  # mirando a sus compañeros con L/R).
  if [ "${COOP_MORIR:-0}" = 1 ] && [ "$2" = join ]; then echo "coop $2 $SECS $3 0${ONLINE:+ online} morir" > "$SD/autotest.txt"
  else
    local own=""   # (COOP_EXTRA_A / COOP_EXTRA_B: sólo para el anfitrión / el invitado)
    if [ "$1" = A ]; then own="${COOP_EXTRA_A:-}"; else own="${COOP_EXTRA_B:-}"; fi
    echo "coop $2 $SECS $3 1${ONLINE:+ online}${COOP_MEGA:+ mega}${COOP_EXTRA:+ $COOP_EXTRA}${own:+ $own}" > "$SD/autotest.txt"
  fi
}
# COOP_EXTRA="desde=1300 nivel=40": reloj adelantado y nivel (para grabar; SECS es el tiempo
# total de partida, así que súmale el «desde»). COOP_EXTRA_A / COOP_EXTRA_B: sólo para el
# anfitrión / el invitado (p. ej. COOP_EXTRA_A="pass=abc rapido" COOP_EXTRA_B="pass=abc").
# COOP_MEGA=1: Gengar y Charizard, cada uno con su megapiedra (el bot megaevoluciona)
if [ -n "${COOP_MEGA:-}" ]; then DA=94; DB=6; else DA=25; DB=4; fi
setup A host ${COOP_DEX_A:-$DA}
setup B join ${COOP_DEX_B:-$DB}
# B: otro nombre, otra MAC y otro id de consola.
echo "40:F4:07:12:34:56" > "$W/B/data/azahar-emu/sysdata/mac.txt"
python3 - "$W/B/data/azahar-emu/nand/data/00000000000000000000000000000000/sysdata/00010017/00000000/config" <<'EOF'
import sys, struct
p = sys.argv[1]; d = bytearray(open(p, 'rb').read())
n, _ = struct.unpack_from('<HH', d, 0)
for i in range(n):
    bid, o, sz, fl = struct.unpack_from('<IIHH', d, 4 + i * 12)
    if bid in (0x90000, 0x90001): d[o:o + 4] = bytes([0x13, 0x37, 0xbe, 0xef])
    if bid == 0xA0000: d[o:o + 20] = 'BETO'.encode('utf-16-le').ljust(20, b'\0')
open(p, 'wb').write(d)
EOF

launch() {   # $1 = A|B, $2 = display
  pgrep -f "Xvfb :$2" >/dev/null || { Xvfb :$2 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 2; }
  DISPLAY=:$2 XDG_CONFIG_HOME="$W/$1/config" XDG_DATA_HOME="$W/$1/data" LIBGL_ALWAYS_SOFTWARE=1 \
    "$APP" ${COOP_DUMP:+-d "$COOP_DUMP/$1.mkv"} "$W/game.3dsx" >"$W/$1.log" 2>&1 &
}
join_room() {   # $1 = display
  DISPLAY=:$1 xdotool mousemove 410 188 click 1; sleep 1          # Multiplayer
  DISPLAY=:$1 xdotool mousemove 470 253 click 1; sleep 1.5        # Direct Connect to Room
  DISPLAY=:$1 xdotool mousemove 806 461 click 1; sleep 3          # Connect
  DISPLAY=:$1 import -window root "$OUT/sala_$1.png" 2>/dev/null   # (para ver si ha entrado)
  for w in $(DISPLAY=:$1 xdotool search --name "members" 2>/dev/null); do DISPLAY=:$1 xdotool windowmove $w 1300 900; done
}
launch A 97; launch B 98
sleep 9
[ -z "$ONLINE" ] && { join_room 97; join_room 98; }

# --- esperar y comparar ---
LA="$W/A/data/azahar-emu/sdmc/3ds/pokesurvivor-3ds/autotest.log"
LB="$W/B/data/azahar-emu/sdmc/3ds/pokesurvivor-3ds/autotest.log"
end=$((SECONDS + SECS * 3 + 120)); shot=0
[ -n "${COOP_QUICK:-}" ] && end=$((SECONDS + 40))
while [ $SECONDS -lt $end ]; do
  if grep -q FIN "$LA" 2>/dev/null && grep -q FIN "$LB" 2>/dev/null; then break; fi
  sleep 10
  if grep -q "^t=" "$LA" 2>/dev/null && [ $shot -lt ${COOP_SHOTS:-3} ]; then
    shot=$((shot + 1))
    for d in 97 98; do   # (fuera las ventanas de la sala, que tapan el juego)
      for w in $(DISPLAY=:$d xdotool search --name "members|Direct Connect" 2>/dev/null); do DISPLAY=:$d xdotool windowmove $w 1300 900; done
    done
    DISPLAY=:97 import -window root "$W/a_full.png" 2>/dev/null; DISPLAY=:98 import -window root "$W/b_full.png" 2>/dev/null
    convert "$W/a_full.png" -crop 400x480+440+199 +repage "$W/a.png" 2>/dev/null
    convert "$W/b_full.png" -crop 400x480+440+199 +repage "$W/b.png" 2>/dev/null
    convert "$W/a.png" "$W/b.png" +append "$OUT/dos_$shot.png" 2>/dev/null
  fi
done
cp "$LA" "$OUT/anfitrion.log" 2>/dev/null; cp "$LB" "$OUT/invitado.log" 2>/dev/null
for p in $(pgrep -x AppRun.wrapped); do
  if tr '\0' '\n' < /proc/$p/environ 2>/dev/null | grep -q "XDG_CONFIG_HOME=$W/"; then kill -9 $p; fi
done
docker rm -f ps-room >/dev/null 2>&1
grep -o "huella paso [0-9]* = [0-9a-f]*" "$OUT/anfitrion.log" > "$W/ha" 2>/dev/null
grep -o "huella paso [0-9]* = [0-9a-f]*" "$OUT/invitado.log" > "$W/hb" 2>/dev/null
n=$(comm -12 <(sort "$W/ha") <(sort "$W/hb") | wc -l)
total=$(cut -d= -f1 "$W/ha" | sort | comm -12 - <(cut -d= -f1 "$W/hb" | sort) | wc -l)
echo "huellas comparadas: $total · iguales: $n"
grep FIN "$OUT/anfitrion.log" "$OUT/invitado.log"
rm -rf "$W"
[ "$total" -gt 0 ] && [ "$n" -eq "$total" ]

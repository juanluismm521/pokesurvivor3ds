#!/usr/bin/env bash
# make_cia.sh — PokéSurvivor 3DS como .cia: se instala con FBI y sale en el
# menú HOME de la 3DS como cualquier juego (con su icono, su banner y su
# sonido). Hace falta una 3DS con custom firmware (Luma3DS), como para el .3dsx.
#
#   tools/make_cia.sh        → pokesurvivor-3ds.cia
#
# Herramientas libres que se bajan solas a tools/cache/cia si no están:
# makerom (github.com/3DSGuy/Project_CTR) y bannertool (github.com/diasurgical/bannertool).
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
C="$ROOT/tools/cache/cia"
OUT="$ROOT/build/cia"
mkdir -p "$C" "$OUT"
cd "$C"
if [ ! -x makerom ]; then
  curl -sLO https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v0.19.0/makerom-v0.19.0-ubuntu_x86_64.zip
  unzip -oq makerom-v0.19.0-ubuntu_x86_64.zip && chmod +x makerom
fi
if [ ! -x bt/linux-x86_64/bannertool ]; then
  curl -sLO https://github.com/diasurgical/bannertool/releases/download/1.2.0/bannertool.zip
  unzip -oq bannertool.zip -d bt && chmod +x bt/linux-x86_64/bannertool
fi
cd "$ROOT"
# El programa (.elf) y sus recursos (romfs/), como para el .3dsx.
. tools/env.sh
make > /dev/null
# La versión del título (la que mira el sistema al instalar) sale de la build:
# así cada .cia es MÁS NUEVO que el anterior. makerom 0.19 deja siempre el
# «minor» a 0 (antes pasábamos 0.4.3 y quedaba «3»; la 0.5.0, «0»: la consola
# rechazaba la actualización como si fuera volver atrás). Por eso va en
# major = build / 16 y micro = build % 16 (crece siempre hasta la build 1023).
N=$(sed -n 's/#define PS_BUILD \([0-9]*\)/\1/p' src/gen/build.h)
N=${N:-0}
if [ "$N" -gt 1023 ]; then echo "make_cia: la build $N no cabe en la versión del título (máx. 1023): hay que cambiar el esquema" >&2; exit 1; fi
python3 tools/cia/gen_banner.py "$OUT" > /dev/null
"$C/bt/linux-x86_64/bannertool" makebanner -i "$OUT/banner.png" -a "$OUT/banner.wav" -o "$OUT/banner.bnr" > /dev/null
"$C/bt/linux-x86_64/bannertool" makesmdh -s "PokéSurvivor 3DS" -l "Bullet-heaven con Pokémon (proyecto de fans)" \
  -p "juanluismm520" -i assets-src/icon-48.png -o "$OUT/icon.icn" > /dev/null
"$C/makerom" -f cia -o pokesurvivor-3ds.cia -target t -exefslogo -elf pokesurvivor-3ds.elf -rsf tools/cia/app.rsf \
  -icon "$OUT/icon.icn" -banner "$OUT/banner.bnr" -major "$((N >> 4))" -micro "$((N & 15))"
ls -l pokesurvivor-3ds.cia

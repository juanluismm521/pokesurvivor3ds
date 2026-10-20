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
VERSION=$(grep -o 'kVersion\[\] = "[^"]*"\|kVersion = "[^"]*"' -r src/core src/platform/3ds | head -1 | grep -o '"[^"]*"' | tr -d '"')
VERSION=${VERSION:-0.1.0}
IFS=. read -r MAJ MIN MIC <<< "$VERSION"
python3 tools/cia/gen_banner.py "$OUT" > /dev/null
"$C/bt/linux-x86_64/bannertool" makebanner -i "$OUT/banner.png" -a "$OUT/banner.wav" -o "$OUT/banner.bnr" > /dev/null
"$C/bt/linux-x86_64/bannertool" makesmdh -s "PokéSurvivor 3DS" -l "Bullet-heaven con Pokémon (proyecto de fans)" \
  -p "juanluismm520" -i assets-src/icon-48.png -o "$OUT/icon.icn" > /dev/null
"$C/makerom" -f cia -o pokesurvivor-3ds.cia -target t -exefslogo -elf pokesurvivor-3ds.elf -rsf tools/cia/app.rsf \
  -icon "$OUT/icon.icn" -banner "$OUT/banner.bnr" -major "${MAJ:-0}" -minor "${MIN:-1}" -micro "${MIC:-0}"
ls -l pokesurvivor-3ds.cia

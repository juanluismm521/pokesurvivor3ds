#!/usr/bin/env bash
# build.sh — compila jsbench.3dsx: el juego web de reference/web (o de la
# carpeta que se diga) corriendo con QuickJS en la 3DS, para medir si su JS
# cabría en la consola. Ver tools/web/jsbench/main.c.
#
#   tools/web/jsbench/build.sh [carpeta_web]   → build/jsbench/jsbench.3dsx
#
# QuickJS-ng se baja a tools/cache/quickjs-ng (versión fija). Ojo: en ARM con
# -O2 hay que compilarlo con -fno-strict-aliasing -fwrapv (sin eso, el
# operador ... da "out of memory"; comprobado el 2026-10-19).
set -euo pipefail
cd "$(dirname "$0")/../../.."
ROOT=$(pwd)
WEB=${1:-$ROOT/reference/web}
QJS=$ROOT/tools/cache/quickjs-ng
QJS_COMMIT=081d50c2792c16763f0e4a62ee4fdfb902b15a67
OUT=$ROOT/build/jsbench
source tools/env.sh
if [ ! -d "$QJS/.git" ]; then git clone -q https://github.com/quickjs-ng/quickjs.git "$QJS"; fi
git -C "$QJS" checkout -q "$QJS_COMMIT" 2>/dev/null || { git -C "$QJS" fetch -q origin; git -C "$QJS" checkout -q "$QJS_COMMIT"; }
mkdir -p "$OUT/romfs"
A="-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft"
CF="$A -O2 -fno-strict-aliasing -fwrapv -funsigned-char -D_GNU_SOURCE -D__3DS__ -I$DEVKITPRO/libctru/include -I$QJS"
for f in quickjs libregexp libunicode dtoa; do
  [ "$OUT/$f.o" -nt "$QJS/$f.c" ] || arm-none-eabi-gcc $CF -w -c "$QJS/$f.c" -o "$OUT/$f.o"
done
arm-none-eabi-gcc $CF -Wall -c tools/web/jsbench/main.c -o "$OUT/main.o"
arm-none-eabi-gcc -specs=3dsx.specs $A "$OUT"/main.o "$OUT"/quickjs.o "$OUT"/libregexp.o "$OUT"/libunicode.o "$OUT"/dtoa.o \
  -L"$DEVKITPRO/libctru/lib" -lctru -lm -o "$OUT/jsbench.elf"
PS_QJS="$OUT/romfs/bundle.js" node tools/web/partida.js "$WEB" > /dev/null
smdhtool --create "jsbench PokéSurvivor" "El juego web con QuickJS: ¿cabe en la 3DS?" "pruebas" "$DEVKITPRO/libctru/default_icon.png" "$OUT/jsbench.smdh"
3dsxtool "$OUT/jsbench.elf" "$OUT/jsbench.3dsx" --smdh="$OUT/jsbench.smdh" --romfs="$OUT/romfs"
echo "listo: $OUT/jsbench.3dsx ($(du -h "$OUT/jsbench.3dsx" | cut -f1))"

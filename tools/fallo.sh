#!/usr/bin/env bash
# fallo.sh — traduce un fallo.log de la consola (sdmc:/3ds/pokesurvivor-3ds/
# fallo.log o fallo_anterior.log) a funciones y líneas del código, con el .elf
# de esa build (el publicador los guarda en /root/docker/pokesurvivor/elfs/bN.elf).
#   tools/fallo.sh fallo.log            (o pegando el texto: tools/fallo.sh -)
#   tools/fallo.sh fallo.log mi.elf     (con otro .elf)
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
. "$ROOT/tools/env.sh"
LOG=$(cat "${1:--}")
B=$(grep -o 'build [0-9]*' <<< "$LOG" | head -1 | grep -o '[0-9]*')
ELF=${2:-$HOME/pokesurvivor-elfs/b$B.elf}
[ -f "$ELF" ] || { echo "no está el .elf de la build $B ($ELF)"; exit 1; }
echo "$LOG" | head -4
echo "--- con $ELF"
ADDRS=$(grep -oE '(pc|lr) [0-9a-f]{8}' <<< "$LOG" | awk '{print $2}')
ADDRS="$ADDRS $(grep '^pila:' <<< "$LOG" | grep -oE '[0-9a-f]{8}')"
for a in $ADDRS; do
  printf '%s  ' "$a"
  "$DEVKITARM/bin/arm-none-eabi-addr2line" -f -C -p -e "$ELF" "0x$a"
done

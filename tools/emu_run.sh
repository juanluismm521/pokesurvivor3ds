#!/usr/bin/env bash
# Arranca un .3dsx en Azahar bajo Xvfb, espera y saca capturas.
# Uso: tools/emu_run.sh [archivo.3dsx] [segundos] [carpeta_capturas] [n_capturas]
# La SD del emulador está en ~/.local/share/azahar-emu/sdmc/.
set -uo pipefail
cd "$(dirname "$0")/.."
ROM=$(realpath "${1:-pokesurvivor-3ds.3dsx}")
SECS=${2:-20}
OUT=${3:-captures}
SHOTS=${4:-1}
mkdir -p "$OUT"
export DISPLAY=:99
# (el Xvfb de ESTA pantalla: en el VPS hay otros servicios con el suyo)
if ! pgrep -f "Xvfb :99( |$)" >/dev/null; then
  Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 &
  sleep 2
fi
pkill -9 -x AppRun.wrapped 2>/dev/null; sleep 1
LIBGL_ALWAYS_SOFTWARE=1 /opt/azahar/squashfs-root/AppRun "$ROM" >"$OUT/azahar.log" 2>&1 &
EMU=$!
STEP=$(( SECS / SHOTS ))
for i in $(seq 1 "$SHOTS"); do
  sleep "$STEP"
  import -window root "$OUT/emu_$i.png"
  # Recortes de las dos pantallas (disposición por defecto a 1280x800)
  convert "$OUT/emu_$i.png" -crop 400x240+440+199 +repage "$OUT/emu_${i}_top.png" 2>/dev/null
  convert "$OUT/emu_$i.png" -crop 320x240+480+439 +repage "$OUT/emu_${i}_bottom.png" 2>/dev/null
done
pkill -9 -x AppRun.wrapped 2>/dev/null
wait $EMU 2>/dev/null
echo "Capturas en $OUT/"

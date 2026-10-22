#!/usr/bin/env bash
# grabar.sh NOMBRE "autotest" MAX_SEGUNDOS [rom] — graba el juego en Azahar (RGB, 60 fps, casi sin pérdidas).
# Espera a que la partida acabe («FIN» en el log) o a MAX_SEGUNDOS reales.
set -u
N=$1; AT=$2; S=$3; ROM=${4:-/root/docker/pokesurvivor/juego/pokesurvivor-3ds.3dsx}
D=~/.local/share/azahar-emu/sdmc/3ds/pokesurvivor-3ds
export DISPLAY=:99
pgrep -x Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 &>/dev/null & sleep 2; }
pkill -9 -x AppRun.wrapped 2>/dev/null; sleep 1
echo "$AT" > $D/autotest.txt; rm -f $D/autotest.log
rm -f /root/docker/pokesurvivor/trailer/raw/$N.mkv
XDG_CONFIG_HOME=/root/docker/pokesurvivor/trailer/cfg LIBGL_ALWAYS_SOFTWARE=1 /opt/azahar/squashfs-root/AppRun -d /root/docker/pokesurvivor/trailer/raw/$N.tmp.mkv "$ROM" > /root/docker/pokesurvivor/trailer/raw/$N.log 2>&1 &
t=0
while [ $t -lt "$S" ]; do
  grep -q "^FIN" $D/autotest.log 2>/dev/null && { sleep 2; break; }
  sleep 2; t=$((t + 2))
done
pkill -9 -x AppRun.wrapped 2>/dev/null; sleep 1
cp $D/autotest.log /root/docker/pokesurvivor/trailer/raw/$N.autotest.log 2>/dev/null
rm -f $D/autotest.txt
ffmpeg -v error -y -i /root/docker/pokesurvivor/trailer/raw/$N.tmp.mkv -c copy /root/docker/pokesurvivor/trailer/raw/$N.mkv 2>/dev/null; rm -f /root/docker/pokesurvivor/trailer/raw/$N.tmp.mkv
echo "$N: $(ffprobe -v error -show_entries format=duration -of csv=p=0 /root/docker/pokesurvivor/trailer/raw/$N.mkv) s (en $t s reales)"

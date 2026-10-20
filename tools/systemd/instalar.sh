#!/usr/bin/env bash
# Instala (o actualiza) el temporizador que publica las versiones nuevas del juego.
#   tools/systemd/instalar.sh          instalar y arrancar
#   tools/systemd/instalar.sh parar    pararlo (las consolas se quedan con lo último publicado)
set -e
D=$(cd "$(dirname "$0")" && pwd)
if [ "${1:-}" = parar ]; then systemctl disable --now pokesurvivor-publicar.timer; exit 0; fi
cp "$D/pokesurvivor-publicar.service" "$D/pokesurvivor-publicar.timer" /etc/systemd/system/
systemctl daemon-reload
systemctl enable --now pokesurvivor-publicar.timer
systemctl list-timers pokesurvivor-publicar.timer --no-pager

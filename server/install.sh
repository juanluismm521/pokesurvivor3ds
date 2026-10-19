#!/usr/bin/env bash
# Instala (o actualiza) el servidor de relevo como servicio del sistema.
set -euo pipefail
cd "$(dirname "$0")"
install -d /opt/pokesurvivor-relay
install -m 644 relay.py /opt/pokesurvivor-relay/relay.py
install -m 644 pokesurvivor-relay.service /etc/systemd/system/pokesurvivor-relay.service
systemctl daemon-reload
systemctl enable --now pokesurvivor-relay
systemctl restart pokesurvivor-relay
sleep 1
systemctl --no-pager --lines=3 status pokesurvivor-relay | head -6

#!/usr/bin/env bash
# google_qr_emu.sh — «Entrar con Google» por QR en Azahar, de punta a punta:
#   - el servicio «vincular» del servidor (../docker/pokesurvivor-relay/vincular) en local (8126)
#   - el emulador de Firebase (tools/firebase-emu, ya arrancado: 9099/8089)
#   - la consola en Azahar con el autotest «nube google -» y nube.json apuntando a todo eso
# Cuando la consola enseña el QR (lo apunta en el log), hace de pokesurvivor.com:
# entra con una cuenta de Google en el emulador y entrega la sesión con el código.
# Uso: tools/google_qr_emu.sh /ruta/absoluta/pokesurvivor-3ds.3dsx [carpeta_capturas]
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
APP=${1:?falta el .3dsx (ruta absoluta)}
OUT=${2:-$ROOT/captures/google_qr}
SD=~/.local/share/azahar-emu/sdmc/3ds/pokesurvivor-3ds
VPORT=8126
mkdir -p "$SD" "$OUT"
LOG=$SD/autotest.log

VINCULAR_PUERTO=$VPORT VINCULAR_BASE=http://127.0.0.1:$VPORT python3 "$ROOT/../docker/pokesurvivor-relay/vincular/vincular.py" > /dev/null 2>&1 &
VPID=$!
trap 'kill $VPID 2>/dev/null; rm -f "$SD/nube.json" "$SD/autotest.txt"' EXIT
cat > "$SD/nube.json" <<EOF
{"auth":"http://127.0.0.1:9099","fs":"http://127.0.0.1:8089","project":"demo-pokesurvivor","key":"fake-api-key","vincular":"http://127.0.0.1:$VPORT"}
EOF
rm -f "$SD/cuenta.json" "$LOG"
echo "nube google -" > "$SD/autotest.txt"

"$ROOT/tools/emu_run.sh" "$APP" 60 "$OUT" 6 &
EPID=$!
# Espera al QR.
for _ in $(seq 1 60); do grep -q "^google:" "$LOG" 2>/dev/null && break; sleep 1; done
CODE=$(grep -o "vincular/[A-Z0-9]\{8\}" "$LOG" | head -1 | cut -d/ -f2)
echo "la consola enseña el código: ${CODE:-NINGUNO}"
sleep 20   # (para que las capturas salgan con el QR)
# «pokesurvivor.com»: una cuenta de Google en el emulador y su sesión.
N=$RANDOM$RANDOM
REFRESH=$(curl -s -X POST "http://127.0.0.1:9099/identitytoolkit.googleapis.com/v1/accounts:signInWithIdp?key=fake-api-key" \
  -H 'Content-Type: application/json' \
  -d "{\"postBody\":\"id_token={\\\"sub\\\":\\\"qr-emu-$N\\\",\\\"email\\\":\\\"ana$N@gmail.com\\\",\\\"name\\\":\\\"Ana\\\"}&providerId=google.com\",\"requestUri\":\"http://localhost\",\"returnSecureToken\":true}" |
  python3 -c "import json,sys;print(json.load(sys.stdin).get('refreshToken',''))")
curl -s -X POST "http://127.0.0.1:$VPORT/vincular/api/entregar" -H 'Content-Type: application/json' \
  -d "{\"code\":\"$CODE\",\"refresh\":\"$REFRESH\",\"nombre\":\"Ana\"}"; echo
wait $EPID
cat "$LOG"
# El QR de la captura se lee (zbar-tools) y lleva a la página de ese código.
QR=$(for f in "$OUT"/emu_*_bottom.png; do zbarimg -q --raw "$f" 2>/dev/null; done | head -1)
echo "QR de la captura: ${QR:-ilegible}"
[ "$QR" = "http://127.0.0.1:$VPORT/vincular/$CODE" ] && grep -q "nube: BIEN · Ana" "$LOG" && echo "google_qr_emu: TODO BIEN" || { echo "google_qr_emu: FALLA"; exit 1; }

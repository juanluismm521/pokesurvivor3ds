#!/usr/bin/env bash
# prueba_real.sh — la sincronización contra los servidores DE VERDAD
# (pokesurvivor.com: Firebase Auth de pokesurvivor-free y la API de la partida
# ice.pokesurvivor.com/v1/d (desde noviembre de 2026; antes Firestore), y el
# servidor del proyecto pokesurvivor3ds.duckdns.org), con la cuenta de prueba
# del dueño. Lo pidió el dueño (2026-10-20): las pruebas, siempre también con
# el servidor real; el emulador no se porta igual en todo.
#
# La cuenta va en /root/.config/pokesurvivor/prueba_real.env (fuera del repo,
# NUNCA en git): PS_REAL_USUARIO=... y PS_REAL_PASS=...
# La partida local es VACÍA: la fusión no añade ni quita nada en la cuenta.
#   1. usuario y contraseña → sincronizar
#   2. «Entrar con Google» por QR: código del servicio vincular real, se entrega
#      la sesión (como el trocito de la web) y la «3DS» entra y sincroniza
#      (justo después de la 1: comprueba también la espera del tope de ~20 s).
#   3. el servidor online responde.
#   4. (AZAHAR=1 y la ruta absoluta del .3dsx como 1er argumento) la consola
#      en Azahar entra y sincroniza con una SD sin partida (la de pruebas se
#      aparta y se devuelve).
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
CFG=/root/.config/pokesurvivor/prueba_real.env
[ -f "$CFG" ] || { echo "falta $CFG (cuenta de prueba del dueño)"; exit 2; }
. "$CFG"
KEY=$(grep -o 'kWebApiKey = "[^"]*"' "$ROOT/src/gen/gamedata.cpp" | cut -d'"' -f2)
API=https://ice.pokesurvivor.com/v1/d
export PS_AUTH="https:/" PS_API="$API" PS_PROJECT=pokesurvivor-free PS_KEY="$KEY"
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
echo "{\"v\":3,\"name\":\"$PS_REAL_USUARIO\",\"coins\":0,\"owned\":{},\"updated\":1}" > "$T/vacia.json"
fallos=0

# 1. usuario y contraseña
if out=$(timeout 60 "$ROOT/build-pc/ps_cloud" "$PS_REAL_USUARIO" "$PS_REAL_PASS" "$T/vacia.json" ctrvpstest "$T/a.json" 2>&1) && [[ $out == *"ok uid="* ]]; then
  echo "  ✔ usuario y contraseña: $(echo "$out" | tail -1)"
else echo "  ✘ usuario y contraseña: $out"; fallos=$((fallos+1)); fi

# 1b. un guardado de la web (la misma partida, rev + 1, como PUT /save de db.js).
# Después de eso la 3DS tiene que seguir pudiendo subir (409: se junta y se repite).
sleep 21   # (la API no deja guardar dos veces en menos de 20 s)
EMAIL0=$(python3 -c "import sys;print(sys.argv[1].lower().encode().hex()+'@jugadores.pokesurvivor.net')" "$PS_REAL_USUARIO")
TOK0=$(curl -s -m 15 -X POST "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=$KEY" -H "Referer: https://pokesurvivor.com/" \
  -H 'Content-Type: application/json' -d "{\"email\":\"$EMAIL0\",\"password\":\"$PS_REAL_PASS\",\"returnSecureToken\":true}" |
  python3 -c "import json,sys;print(json.load(sys.stdin).get('idToken',''))")
curl -s -m 15 "$API/save" -H "Authorization: Bearer $TOK0" > "$T/web.json"
REV0=$(python3 -c "import json,sys;print(json.load(open(sys.argv[1])).get('rev',0))" "$T/web.json" 2>/dev/null)
python3 -c "import json,sys;d=json.load(open(sys.argv[1]));json.dump({'save':d['save'],'rev':int(d['rev'])+1},open(sys.argv[2],'w'))" "$T/web.json" "$T/put.json" 2>/dev/null
if curl -s -m 15 -X PUT "$API/save" -H "Authorization: Bearer $TOK0" -H 'Content-Type: application/json' --data @"$T/put.json" | grep -q '"rev"'; then
  echo "  ✔ guardado de la web simulado (rev $REV0 → $((REV0 + 1)))"
else echo "  ✘ no se ha podido simular el guardado de la web (rev $REV0)"; fallos=$((fallos+1)); fi
sleep 21
# 2. por QR con el servidor vincular real (y con la partida ya guardada desde la web nueva)
timeout 60 "$ROOT/build-pc/ps_cloud" --qr - "$T/vacia.json" ctrvpstest "$T/b.json" > "$T/qr.log" 2>&1 &
QPID=$!
for _ in $(seq 1 20); do grep -q "QR:" "$T/qr.log" && break; sleep 0.5; done
CODE=$(grep -o "vincular/[A-Z0-9]\{8\}" "$T/qr.log" | cut -d/ -f2)
EMAIL=$(python3 -c "import sys;print(sys.argv[1].lower().encode().hex()+'@jugadores.pokesurvivor.net')" "$PS_REAL_USUARIO")
R=$(curl -s -m 15 -X POST "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=$KEY" \
  -H "Referer: https://pokesurvivor.com/" -H 'Content-Type: application/json' \
  -d "{\"email\":\"$EMAIL\",\"password\":\"$PS_REAL_PASS\",\"returnSecureToken\":true}" |
  python3 -c "import json,sys;print(json.load(sys.stdin).get('refreshToken',''))")
curl -s -o /dev/null -X POST https://pokesurvivor3ds.duckdns.org/vincular/api/entregar -H 'Origin: https://pokesurvivor.com' \
  -H 'Content-Type: application/json' -d "{\"code\":\"$CODE\",\"refresh\":\"$R\",\"nombre\":\"$PS_REAL_USUARIO\"}"
wait $QPID
if grep -q "ok uid=" "$T/qr.log"; then echo "  ✔ QR (vincular real): $(tail -1 "$T/qr.log")"
else echo "  ✘ QR (vincular real): $(cat "$T/qr.log")"; fallos=$((fallos+1)); fi

# 3. el servidor online responde
if python3 /root/docker/pokesurvivor/servidor/check.py pokesurvivor3ds.duckdns.org > "$T/relay.log" 2>&1; then echo "  ✔ servidor online: responde"
else echo "  ✘ servidor online: $(tail -2 "$T/relay.log")"; fallos=$((fallos+1)); fi

# 4. la consola en Azahar, contra la nube real
if [ "${AZAHAR:-0}" = 1 ]; then
  SD=/root/.local/share/azahar-emu/sdmc/3ds/pokesurvivor-3ds
  mkdir -p "$T/sd" "$SD" && cp -a "$SD/." "$T/sd/"
  rm -f "${SD:?}"/*.json "${SD:?}/save.bak" "${SD:?}/autotest.log"
  echo "nube $PS_REAL_USUARIO $PS_REAL_PASS" > "$SD/autotest.txt"
  "$ROOT/tools/emu_run.sh" "${1:?ruta absoluta del .3dsx}" 45 "$ROOT/captures/real" 3 > /dev/null 2>&1
  line=$(grep "^nube:" "$SD/autotest.log" 2>/dev/null)
  rm -f "${SD:?}"/*.json "${SD:?}/save.bak" "${SD:?}/autotest.log" "${SD:?}/autotest.txt"
  cp -a "$T/sd/." "$SD/"
  if [[ $line == "nube: BIEN"* ]]; then echo "  ✔ Azahar: $line"; else echo "  ✘ Azahar: ${line:-sin respuesta}"; fallos=$((fallos+1)); fi
fi

[ $fallos -eq 0 ] && echo "prueba_real: TODO BIEN" || { echo "prueba_real: $fallos FALLOS"; exit 1; }

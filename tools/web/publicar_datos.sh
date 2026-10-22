#!/usr/bin/env bash
# publicar_datos.sh — publica el paquete de datos de la web que tiene ahora el
# port (reference/web) para que las consolas lo bajen solas (core/datapack,
# platform/3ds/datos3ds). Se llama tras cada actualización (auto.sh) o a mano.
#
#   tools/web/publicar_datos.sh [carpeta]   (por defecto $PS_DATOS_DIR o
#                                            /root/docker/pokesurvivor/servidor/datos/www/datos)
# El servidor que los sirve está en el repositorio del servidor
# (github.com/juanluismm521/pokesurvivor3ds-server, perfil «datos»).
#
# Deja <esquema>.json (el paquete para los programas que entienden esa forma de
# datos) y ultimo.json (el esquema más reciente: a los programas viejos les
# dice que hay uno nuevo). Los paquetes de esquemas viejos se quedan, así los
# programas viejos siguen recibiendo lo último que les vale.
set -euo pipefail
cd "$(dirname "$0")/../.."
DEST=${1:-${PS_DATOS_DIR:-/root/docker/pokesurvivor/servidor/datos/www/datos}}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
PS_PAQUETE="$TMP/paquete.json" node tools/gen_data.js "$TMP/gamedata.cpp" > /dev/null
ESQ=$(node -e "const p=require('$TMP/paquete.json');console.log(p.esquema)")
INFO=$(node -e "const p=require('$TMP/paquete.json');console.log(JSON.stringify({esquema:p.esquema,web:p.web,fecha:p.fecha,huella:p.huella}))")
mkdir -p "$DEST"
if [ -f "$DEST/$ESQ.json" ] && cmp -s "$TMP/paquete.json" "$DEST/$ESQ.json"; then
  echo "publicar_datos: sin cambios (esquema $ESQ)"; exit 0
fi
cp "$TMP/paquete.json" "$DEST/.$ESQ.json.tmp" && mv "$DEST/.$ESQ.json.tmp" "$DEST/$ESQ.json"
echo "$INFO" > "$DEST/.ultimo.json.tmp" && mv "$DEST/.ultimo.json.tmp" "$DEST/ultimo.json"
echo "publicar_datos: $DEST/$ESQ.json ($(du -h "$DEST/$ESQ.json" | cut -f1)) · $INFO"

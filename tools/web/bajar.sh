#!/bin/bash
# bajar.sh — baja la web publicada (pokesurvivor.com: sólo hay app.min.js ofuscado desde
# nov-2026, el repo es privado), la deja legible con webcrack y la parte en módulos con
# nombre (tools/web/partir.js) en una carpeta como la de reference/web.
#
#   tools/web/bajar.sh [carpeta_salida]        (por defecto build/web/gran)
#
# Para ponerla como referencia del port: mover la carpeta a reference/web (la anterior,
# a reference/web-AAAAMMDD) y escribir UPSTREAM a mano (fecha de la web).
set -euo pipefail
cd "$(dirname "$0")/../.."
OUT="${1:-build/web/gran}"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
curl -fsSL -A "Mozilla/5.0" https://pokesurvivor.com/js/app.min.js -o "$TMP/app.min.js"
tools/node_modules/.bin/webcrack "$TMP/app.min.js" -o "$TMP/dec" >/dev/null
mkdir -p "$OUT"
node tools/web/partir.js "$TMP/dec/deobfuscated.js" "$OUT"
cp "$TMP/app.min.js" "$OUT/app.min.js"
# (lo que la web sirve sin ofuscar aparte del paquete)
for f in js/core/locales.js js/boot.js js/core/beta.js js/noframe.js css/style.css; do
  mkdir -p "$OUT/$(dirname "$f")"; curl -fsSL -A "Mozilla/5.0" "https://pokesurvivor.com/$f" -o "$OUT/$f" || echo "aviso: no se pudo bajar $f"
done
date -u +"%Y-%m-%d web publicada (app.min.js, $(sha1sum "$TMP/app.min.js" | cut -c1-12))" > "$OUT/BAJADA"
echo "web en $OUT"

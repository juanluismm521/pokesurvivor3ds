#!/bin/bash
# comparar.sh — qué ha cambiado en un archivo de la web entre la copia con fuentes y la
# ofuscada (sin nombres de variables: ver normal.js). Deja el diff en build/web/cmp/.
#   tools/web/comparar.sh js/systems/combat.js [vieja=reference/web-1910] [nueva=reference/web]
set -euo pipefail
cd "$(dirname "$0")/../.."
F="$1"; OLD="${2:-reference/web-1910}"; NEW="${3:-reference/web}"
OUT=build/web/cmp; mkdir -p "$OUT"; B="$(echo "$F" | tr / _)"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
tools/node_modules/.bin/webcrack "$OLD/$F" -o "$TMP/o" >/dev/null 2>&1
(cd tools && node web/normal.js "$TMP/o/deobfuscated.js") > "$OUT/$B.vieja.js"
(cd tools && node web/normal.js "../$NEW/$F") > "$OUT/$B.nueva.js"
diff -U2 -b "$OUT/$B.vieja.js" "$OUT/$B.nueva.js" > "$OUT/$B.diff" || true
echo "$OUT/$B.diff: $(grep -c '^[+-]' "$OUT/$B.diff") líneas cambiadas ($(wc -l < "$OUT/$B.vieja.js") → $(wc -l < "$OUT/$B.nueva.js"))"

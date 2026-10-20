#!/usr/bin/env bash
# gen_build.sh — src/gen/build.h con el número de build (commits de main) y el
# commit, para que la consola sepa qué versión lleva (actualizaciones: core/update).
# Sólo se reescribe si cambia (para no recompilar sin motivo).
OUT=${1:-src/gen/build.h}
N=${PS_BUILD_N:-$(git rev-list --count HEAD 2>/dev/null || echo 0)}   # (PS_BUILD_N: sólo para pruebas)
C=$(git rev-parse --short=7 HEAD 2>/dev/null || echo desconocido)
mkdir -p "$(dirname "$OUT")"
printf '// Generado por tools/gen_build.sh (no se sube a git).\n#pragma once\n#define PS_BUILD %s\n#define PS_COMMIT "%s"\n' "$N" "$C" > "$OUT.tmp"
if cmp -s "$OUT.tmp" "$OUT"; then rm -f "$OUT.tmp"; else mv "$OUT.tmp" "$OUT"; fi

#!/usr/bin/env bash
# actualizar.sh — trae una versión nueva de la web (github.com/JPTER78/pokesurvivor)
# al port. La web se baja a tools/cache/web (un clon aparte, sólo para leer:
# nunca se añade como remoto de este repo ni se sube nada allí).
#
#   tools/web/actualizar.sh                  informe de lo nuevo (no toca nada)
#   tools/web/actualizar.sh aplicar [commit] copia la web nueva a reference/web,
#                                            regenera los datos y pasa TODAS las pruebas
#   tools/web/actualizar.sh pruebas          sólo las pruebas (tras portar a mano)
#
# reference/web/UPSTREAM dice de qué commit de la web es la copia actual.
# Resultado en build/web/: informe.md, informe.json y resultado.txt (una línea:
# "nada", "automatico", "portar", "pruebas-ok" o "pruebas-fallan").
# Sale con 0 si todo va bien, 3 si hay que portar lógica y 1 si fallan las pruebas.
set -uo pipefail
cd "$(dirname "$0")/../.."
ROOT=$(pwd)
MODE=${1:-informe}
WEB_URL=${WEB_REPO:-https://github.com/JPTER78/pokesurvivor.git}
CACHE=$ROOT/tools/cache/web
OUT=$ROOT/build/web
mkdir -p "$OUT"
log() { echo "== $*"; }
result() { echo "$1" > "$OUT/resultado.txt"; log "resultado: $1"; }

# ---------------- la web, al día ----------------
if [ "$MODE" != pruebas ]; then
  if [ -d "$CACHE/.git" ]; then git -C "$CACHE" fetch -q origin || { log "no se puede bajar la web"; exit 1; }
  else mkdir -p "$(dirname "$CACHE")"; git clone -q "$WEB_URL" "$CACHE" || { log "no se puede clonar la web"; exit 1; }
  fi
  BASE=$(head -1 reference/web/UPSTREAM)
  TARGET=$(git -C "$CACHE" rev-parse "${2:-origin/main}")
  log "web: copia actual ${BASE:0:7}, última ${TARGET:0:7}"
  node tools/web/informe.js --repo "$CACHE" --desde "$BASE" --hasta "$TARGET" --md "$OUT/informe.md" --json "$OUT/informe.json" || exit 1
  VEREDICTO=$(node -e "console.log(require('$OUT/informe.json').veredicto)")
  log "informe: $OUT/informe.md (veredicto: $VEREDICTO)"
  if [ "$MODE" = informe ]; then result "$VEREDICTO"; exit 0; fi
  if [ "$VEREDICTO" = nada ]; then result nada; exit 0; fi

  # ---------------- copiar la versión nueva a reference/web ----------------
  log "copiando la web ${TARGET:0:7} a reference/web"
  TMP=$(mktemp -d)
  git -C "$CACHE" archive "$TARGET" js css dev.html tools GDD.md CREDITS.md | tar -x -C "$TMP"
  rsync -a --delete --exclude app.min.js "$TMP/js/" reference/web/js/
  rsync -a --delete "$TMP/css/" reference/web/css/
  rsync -a --delete "$TMP/tools/" reference/web/tools/
  cp "$TMP/dev.html" reference/web/dev.html
  cp "$TMP/GDD.md" GDD.md
  # Los créditos de los sprites (CC BY-NC) son obligatorios: si la web los
  # cambia, hay que juntarlos con los nuestros (CREDITS.md empieza con las
  # gracias de la 3DS). Se avisa; no se pisa.
  if ! git -C "$CACHE" diff --quiet "$BASE" "$TARGET" -- CREDITS.md; then
    cp "$TMP/CREDITS.md" "$OUT/CREDITS.web.md"
    log "AVISO: la web ha cambiado CREDITS.md (copia en $OUT/CREDITS.web.md): júntalo con CREDITS.md"
  fi
  rm -rf "$TMP"
  { echo "$TARGET"; git -C "$CACHE" log -1 --format='%ad %s' --date=short "$TARGET" | cut -c1-200; } > reference/web/UPSTREAM

  # ---------------- datos ----------------
  log "datos (gen_data)"
  if ! node tools/gen_data.js src/gen/gamedata.cpp; then
    node tools/gen_data.js --informe > "$OUT/gen_data.json" || true
    log "gen_data no sabe leer la versión nueva: hay que portar (ver $OUT/informe.md)"
    result portar; exit 3
  fi
  CHANGED=$(node -e "const j=require('$OUT/informe.json');console.log(j.archivos.map(a=>a.f).join(' '))")
  if [ "${PS_SIN_3DS:-0}" != 1 ]; then   # (recursos de la 3DS: no hacen falta para un paquete de datos)
  if echo "$CHANGED" | grep -qE 'js/(ui/icons|entities/vfx|world/tiles|data/(moves|items|pokedex))\.js'; then log "arte (gen_art)"; node tools/gen_art.js || { result pruebas-fallan; exit 1; }; fi
  if echo "$CHANGED" | grep -qE 'js/data/sprites-meta\.js'; then log "sprites (gen_sprites, ≈4 min)"; python3 tools/gen_sprites.py || { result pruebas-fallan; exit 1; }; fi
  if echo "$CHANGED" | grep -qE 'js/(core/audio|data/music)\.js'; then log "sonido (gen_audio, ≈8 min)"; node tools/gen_audio.js || { result pruebas-fallan; exit 1; }; fi
  fi
  if [ "$VEREDICTO" = portar ]; then
    log "la web cambia lógica del juego: los datos ya están, falta portar (ver $OUT/informe.md)"
    result portar; exit 3
  fi
fi

# ---------------- pruebas ----------------
# PS_SIN_3DS=1: sin compilar para la 3DS (el actualizador de Docker: para un
# paquete de datos basta con las pruebas de PC, que sacan los dorados del JS nuevo).
if [ "${PS_SIN_3DS:-0}" != 1 ]; then
  log "compilar para la 3DS"
  # shellcheck disable=SC1091
  source tools/env.sh
  if ! make -j2 > "$OUT/make.log" 2>&1; then tail -20 "$OUT/make.log"; result pruebas-fallan; exit 1; fi
  if grep -q "warning:" "$OUT/make.log"; then log "AVISO: make da avisos (ver $OUT/make.log)"; fi
fi
log "pruebas en PC (casos dorados con la web nueva, simulación con ASan/UBSan)"
cmake -S . -B build-pc > /dev/null && cmake --build build-pc -j2 > "$OUT/cmake.log" 2>&1 || { tail -20 "$OUT/cmake.log"; result pruebas-fallan; exit 1; }
if ! (cd build-pc && ctest --output-on-failure > "$OUT/ctest.log" 2>&1); then
  grep -E "ERROR|FAILED|failed" "$OUT/ctest.log" | head -30
  result pruebas-fallan; exit 1
fi
grep -E "tests passed|test cases" "$OUT/ctest.log" | tail -2
result pruebas-ok

#!/usr/bin/env bash
# auto.sh — seguir a la web solo (para cron). Mira si la web tiene commits
# nuevos y, según tools/web/auto.conf:
#
#   MODO=informe  sólo escribe el informe (build/web/informe.md) y avisa.
#   MODO=datos    además, si la web sólo cambia datos (veredicto "automatico")
#                 y TODAS las pruebas pasan, lo sube a main (commit + push).
#                 Además publica el paquete de datos (tools/web/publicar_datos.sh):
#                 las consolas lo bajan solas al encender, sin cambiar de programa.
#                 Si hay lógica que portar, prepara una rama web/<commit> con
#                 la web nueva y los datos ya regenerados, y avisa: portarla es
#                 cosa de una persona (o de alguien que alguien
#                 lance a mano), siguiendo tools/web/PORTAR.md.
#
# Todo se hace en un worktree aparte (WORKTREE), nunca en la copia de trabajo
# de main: aunque algo salga mal, main no se toca hasta que pasa todo.
#
# Cron (una vez al día, a las 6:30 UTC):
#   30 6 * * * /root/docker/pokesurvivor/juego/tools/web/auto.sh >> /var/log/pokesurvivor-web.log 2>&1
set -uo pipefail
REPO=$(cd "$(dirname "$0")/../.." && pwd)
CONF=$REPO/tools/web/auto.conf
MODO=informe; WORKTREE=$REPO/../pokesurvivor-3ds-auto; AVISAR=""
# shellcheck disable=SC1090
[ -f "$CONF" ] && source "$CONF"
LOCK=/tmp/pokesurvivor-web-auto.lock
exec 9> "$LOCK"; flock -n 9 || { echo "auto.sh ya está en marcha"; exit 0; }
stamp() { date '+%F %T'; }
avisar() {   # avisar "texto" [archivo]
  echo "[$(stamp)] $1"
  if [ -n "$AVISAR" ]; then $AVISAR "$1" "${2:-}" || true; fi
}
echo "[$(stamp)] auto.sh (MODO=$MODO)"

# ---------------- worktree al día con main ----------------
git -C "$REPO" fetch -q origin main || { avisar "auto.sh: no se puede bajar origin/main"; exit 1; }
if [ ! -d "$WORKTREE" ]; then git -C "$REPO" worktree add -q --detach "$WORKTREE" origin/main || exit 1; fi
cd "$WORKTREE" || exit 1
git checkout -q --detach origin/main && git reset -q --hard origin/main && git clean -fdq -e tools/cache -e tools/node_modules -e romfs -e build -e build-pc
# Lo que no está en git (dependencias y recursos) se toma de la copia principal.
[ -e tools/node_modules ] || ln -s "$REPO/tools/node_modules" tools/node_modules
[ -e romfs ] || cp -r "$REPO/romfs" romfs

# ---------------- ¿algo nuevo? ----------------
tools/web/actualizar.sh informe > build-auto.log 2>&1
VER=$(cat build/web/resultado.txt 2>/dev/null || echo error)
case "$VER" in
  nada) echo "[$(stamp)] la web no tiene nada nuevo"; exit 0 ;;
  error) avisar "auto.sh: el informe ha fallado" build-auto.log; exit 1 ;;
esac
TARGET=$(git -C tools/cache/web rev-parse origin/main); SHORT=${TARGET:0:7}
# Si ya se preparó esta versión de la web, no se repite cada día.
if git -C "$REPO" rev-parse -q --verify "refs/heads/web/$SHORT" > /dev/null; then echo "[$(stamp)] web/$SHORT ya existe: nada que hacer"; exit 0; fi
if [ "$MODO" = informe ]; then avisar "La web tiene cambios nuevos ($SHORT, veredicto: $VER)" build/web/informe.md; exit 0; fi

# ---------------- aplicar en una rama ----------------
git checkout -q -b "web/$SHORT"
tools/web/actualizar.sh aplicar "$TARGET" >> build-auto.log 2>&1
RES=$(cat build/web/resultado.txt)
git add -A
if [ "$RES" = portar ]; then
  git commit -qm "Web $SHORT: copia de la web y datos (falta portar la lógica, ver build/web/informe.md)" || true
  cp build/web/informe.md "$REPO/../informe-web-$SHORT.md" 2>/dev/null || true
  avisar "La web $SHORT cambia lógica del juego: rama web/$SHORT preparada en el VPS (datos listos, falta portar según tools/web/PORTAR.md)" build/web/informe.md
  exit 3
fi
if [ "$RES" != pruebas-ok ]; then
  git commit -qm "Web $SHORT: actualización a medias (las pruebas no pasan)" || true
  avisar "Actualización a la web $SHORT a medias: las pruebas no pasan (rama web/$SHORT en el VPS)" build-auto.log
  exit 1
fi
# Sólo datos y todo pasa: commit (sin firmas) y a main.
git commit -qm "Web $SHORT: $(head -2 reference/web/UPSTREAM | tail -1 | cut -c1-150)" || true
if git push -q origin "HEAD:main"; then
  # Y el paquete de datos para las consolas (lo bajan solas al encender).
  tools/web/publicar_datos.sh >> build-auto.log 2>&1 || avisar "Web $SHORT: no se ha podido publicar el paquete de datos" build-auto.log
  avisar "Port actualizado a la web $SHORT y subido a main (sólo datos; todas las pruebas pasan); las consolas lo bajarán solas" build/web/informe.md
else
  avisar "Port actualizado a la web $SHORT pero no se ha podido subir a main (¿main ha cambiado?): rama web/$SHORT" build-auto.log
fi

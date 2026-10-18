#!/usr/bin/env bash
# Regenera todos los recursos de la 3DS en romfs/ desde reference/ y assets-src/.
#   tools/build_assets.sh          todo
#   tools/build_assets.sh rapido   sin sprites ni sonido (lo que más tarda)
set -euo pipefail
cd "$(dirname "$0")/.."
source tools/env.sh
mkdir -p romfs/gfx romfs/spr romfs/snd
echo "== arte (objetos, efectos, iconos)"; node tools/gen_art.js
echo "== fuente";                         python3 tools/gen_font.py
echo "== créditos";                       python3 tools/gen_credits.py
if [ "${1:-}" != "rapido" ]; then
  echo "== sprites de los 966 Pokémon (≈4 min)"; python3 tools/gen_sprites.py
  echo "== música y efectos (≈8 min)";           node tools/gen_audio.js
fi
echo "== listo: $(du -sh romfs | cut -f1) en romfs/"

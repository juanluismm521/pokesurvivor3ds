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
  # Sonido: sólo si ha cambiado lo que lo genera. Chromium redondea distinto en algún
  # que otro bit cada vez y eso, aunque no se oye, agranda los parches de las actualizaciones.
  AUDIO_IN=$(cat tools/gen_audio.js reference/web/js/core/audio.js reference/web/js/core/util.js reference/web/js/data/music.js | sha256sum | cut -c1-16)
  if [ -f romfs/snd/sfx.bin ] && [ "$(cat build/audio.huella 2>/dev/null)" = "$AUDIO_IN" ]; then
    echo "== música y efectos: sin cambios (no se vuelven a grabar)"
  else
    echo "== música y efectos (≈8 min)";           node tools/gen_audio.js
    mkdir -p build && echo "$AUDIO_IN" > build/audio.huella
  fi
fi
# Certificados raíz para HTTPS (sincronizar con pokesurvivor.com).
cp /etc/ssl/certs/ca-certificates.crt romfs/cacert.pem
echo "== listo: $(du -sh romfs | cut -f1) en romfs/"

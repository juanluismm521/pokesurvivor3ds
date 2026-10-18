#!/usr/bin/env bash
# Instala en el VPS (Ubuntu 24.04) todo lo necesario para compilar y probar.
# pkg.devkitpro.org bloquea las IP de centros de datos (Cloudflare, 403), así
# que devkitPro se saca de la imagen oficial de Docker (devkitpro/devkitarm).
set -euo pipefail

# Paquetes del sistema
apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y \
  build-essential cmake ninja-build git curl python3 python3-pil \
  doctest-dev libsdl2-dev xvfb xauth ffmpeg opus-tools vorbis-tools \
  mesa-vulkan-drivers libgl1-mesa-dri fuse libfuse2t64 imagemagick \
  docker.io nodejs npm

# devkitPro (devkitARM, libctru, citro2d/3d, tex3ds, mkbcfnt, portlibs con opus)
if [ ! -x /opt/devkitpro/devkitARM/bin/arm-none-eabi-gcc ]; then
  docker pull devkitpro/devkitarm:latest
  docker run --rm -v /opt:/host devkitpro/devkitarm:latest cp -a /opt/devkitpro /host/
fi

# Azahar (emulador), extraído para no depender de FUSE
if [ ! -x /opt/azahar/squashfs-root/AppRun ]; then
  mkdir -p /opt/azahar && cd /opt/azahar
  curl -fsSL -o azahar.AppImage \
    https://github.com/azahar-emu/azahar/releases/download/2126.2/azahar.AppImage
  chmod +x azahar.AppImage && ./azahar.AppImage --appimage-extract >/dev/null
  cd - >/dev/null
fi

# Playwright + Chromium (para grabar arte y sonido de la web)
cd "$(dirname "$0")"
npm install --no-audit --no-fund
npx playwright install chromium

echo "Listo. Ahora: source tools/env.sh"

#!/usr/bin/env python3
"""gen_banner.py — el banner del menú HOME (256×128) y su sonido, para el .cia.

Fondo rojo de Pokéball, el título con la fuente del juego (Pixelify), unos
Pokémon de la colección (sprites de SpriteCollab, ver CREDITS.md) y el autor
del port. El sonido: tres notas cortas (como al abrir un cofre).
Uso: gen_banner.py <carpeta de salida>  → banner.png, banner.wav
"""
import math, os, struct, sys, wave
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = sys.argv[1] if len(sys.argv) > 1 else '.'
W, H = 256, 128


sys.path.insert(0, os.path.join(ROOT, 'tools'))
from gen_sprites import load_meta  # noqa: E402  (SPRITE_META de la web: tamaño de cada fotograma)
META = load_meta()


def sprite(dex, scale):
    """Primer fotograma de Idle (mirando abajo), recortado y escalado sin suavizar."""
    sheet = Image.open(os.path.join(ROOT, 'assets-src', 'pokemon', str(dex), 'Idle.png')).convert('RGBA')
    a = META[str(dex)]['a']['Idle']
    fr = sheet.crop((0, 0, a['w'], a['h']))
    bb = fr.getbbox()
    fr = fr.crop(bb)
    return fr.resize((fr.width * scale, fr.height * scale), Image.NEAREST)


im = Image.new('RGBA', (W, H))
d = ImageDraw.Draw(im)
# Mitad de arriba roja (degradado), franja negra y mitad de abajo blanca: una Pokéball.
for y in range(H):
    if y < 70:
        k = y / 70
        c = (int(232 - 40 * k), int(56 - 24 * k), int(20 - 10 * k), 255)
    elif y < 76:
        c = (17, 17, 17, 255)
    else:
        k = (y - 76) / (H - 76)
        c = (int(244 - 30 * k), int(244 - 30 * k), int(250 - 26 * k), 255)
    d.line([(0, y), (W, y)], fill=c)
# El botón de la Pokéball, a la derecha.
cx, cy = 222, 73
d.ellipse([cx - 17, cy - 17, cx + 17, cy + 17], fill=(17, 17, 17, 255))
d.ellipse([cx - 11, cy - 11, cx + 11, cy + 11], fill=(250, 250, 250, 255))
d.ellipse([cx - 6, cy - 6, cx + 6, cy + 6], fill=(220, 226, 236, 255))

# Pokémon: los iniciales de Kanto y Pikachu, sobre la franja.
x = 12
for dex in (1, 4, 7, 25):
    s = sprite(dex, 2)
    im.alpha_composite(s, (x, 74 - s.height + 6))
    x += s.width + 6

font = ImageFont.truetype(os.path.join(ROOT, 'assets-src', 'fonts', 'PixelifyPS-700.ttf'), 24)
small = ImageFont.truetype(os.path.join(ROOT, 'assets-src', 'fonts', 'PixelifyPS-400.ttf'), 12)
title = 'PokéSurvivor 3DS'
for dx, dy in ((2, 2), (1, 1)):
    d.text((10 + dx, 6 + dy), title, font=font, fill=(90, 14, 0, 255))
d.text((10, 6), title, font=font, fill=(255, 214, 64, 255))
d.text((10, 104), 'port de juanluismm520', font=small, fill=(40, 48, 70, 255))

os.makedirs(OUT, exist_ok=True)
im.convert('RGB').save(os.path.join(OUT, 'banner.png'))

# Sonido: do-mi-sol cortitos, 16 bits mono a 22050 Hz (< 3 s, lo que pide el banner).
rate = 22050
samples = []
for f, dur in ((523.25, 0.12), (659.25, 0.12), (783.99, 0.35)):
    n = int(rate * dur)
    for i in range(n):
        env = min(1.0, i / 200) * math.exp(-3.0 * i / n)
        v = math.sin(2 * math.pi * f * i / rate) + 0.3 * math.sin(4 * math.pi * f * i / rate)
        samples.append(int(9000 * env * v))
with wave.open(os.path.join(OUT, 'banner.wav'), 'wb') as w:
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(rate)
    w.writeframes(b''.join(struct.pack('<h', s) for s in samples))
print('banner: banner.png y banner.wav en', OUT)

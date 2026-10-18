#!/usr/bin/env python3
"""gen_font.py — Pixelify Sans → romfs/gfx/font.bcfnt (mkbcfnt).

Sólo con los caracteres que usa el juego: ASCII imprimible más todo lo que no
sea ASCII en los textos (tablas generadas de la web, traducciones y el código
de src/). Así entran á é í ó ú ñ ü ¡ ¿ « » · → × ★ … y la fuente pesa poco."""
import glob
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MKBCFNT = os.path.join(os.environ.get('DEVKITPRO', '/opt/devkitpro'), 'tools', 'bin', 'mkbcfnt')


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'romfs', 'gfx', 'font.bcfnt')
    size = sys.argv[2] if len(sys.argv) > 2 else '12'
    chars = set(chr(c) for c in range(0x20, 0x7f))
    files = glob.glob(os.path.join(ROOT, 'src', '**', '*.cpp'), recursive=True) + glob.glob(os.path.join(ROOT, 'src', '**', '*.h'), recursive=True)
    for f in files:
        with open(f, encoding='utf-8') as fh:
            chars.update(ch for ch in fh.read() if ord(ch) > 0x7f)
    with open(os.path.join(ROOT, 'CREDITS.md'), encoding='utf-8') as fh:
        chars.update(ch for ch in fh.read() if ord(ch) > 0x7f)
    # Textos de la web (nombres, descripciones, traducciones...)
    for f in glob.glob(os.path.join(ROOT, 'reference', 'web', 'js', '**', '*.js'), recursive=True):
        with open(f, encoding='utf-8') as fh:
            chars.update(ch for ch in fh.read() if 0x7f < ord(ch) < 0x3000)
    chars.discard('﻿')
    wl = os.path.join(ROOT, 'build', 'font_whitelist.txt')
    os.makedirs(os.path.dirname(wl), exist_ok=True)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(wl, 'w') as fh:
        fh.write(' '.join(str(ord(c)) for c in sorted(chars)))
    font = os.path.join(ROOT, 'assets-src', 'fonts', 'PixelifyPS-700.ttf')
    # Lo que no tiene Pixelify (★ → ♀ ♂) sale de DejaVu Sans.
    extra = [p for p in ['/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'] if os.path.exists(p)]
    subprocess.check_call([MKBCFNT, '-s', size, '-w', wl, '-o', out, font] + extra)
    print(f'gen_font: {len(chars)} caracteres, {os.path.getsize(out)} bytes')


if __name__ == '__main__':
    main()

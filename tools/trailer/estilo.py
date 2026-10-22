# estilo.py — letras del tráiler: las de Pokémon (amarillo con borde azul) y las de cine.
import math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

F = '/root/trailer/fonts/'
POKE = F + 'Pokemon Solid.ttf'
ANTON = F + 'Anton.ttf'
PIXEL = '/root/pokesurvivor-3ds/assets-src/fonts/PixelifyPS-700.ttf'

_cache = {}
def font(path, size):
    k = (path, size)
    if k not in _cache: _cache[k] = ImageFont.truetype(path, size)
    return _cache[k]


def _mask(text, fnt, stroke=0, pad=0):
    l, t, r, b = fnt.getbbox(text, stroke_width=stroke)
    w, h = r - l + 2 * pad, b - t + 2 * pad
    m = Image.new('L', (w, h), 0)
    ImageDraw.Draw(m).text((pad - l, pad - t), text, font=fnt, fill=255, stroke_width=stroke, stroke_fill=255)
    return m


def poke(text, size, arco=0.0, top=(255, 236, 90), bot=(255, 190, 0), borde=(42, 74, 170), sombra=(16, 26, 80)):
    """Letras como el logo de Pokémon: relleno amarillo en degradado, borde azul
    grueso, sombra azul oscura desplazada y un brillo arriba. `arco`: curva."""
    s = 2   # (supermuestreo)
    fnt = font(POKE, size * s)
    sw = max(2, int(size * s * 0.11))
    pad = sw * 3
    fill0 = _mask(text, fnt, 0, pad)
    edge = _mask(text, fnt, sw, pad)
    fill = Image.new('L', edge.size, 0); fill.paste(fill0, (sw, sw))
    W, H = edge.size
    out = Image.new('RGBA', (W + sw, H + sw), (0, 0, 0, 0))
    # sombra (abajo a la derecha) y borde
    out.paste(Image.new('RGBA', edge.size, sombra + (255,)), (sw * 2 // 3, sw), edge)
    out.paste(Image.new('RGBA', edge.size, borde + (255,)), (0, 0), edge)
    # relleno con degradado vertical y un brillo arriba
    ys = np.linspace(0, 1, H)[:, None]
    yf = np.clip((ys * H - pad - sw) / max(1, H - 2 * (pad + sw)), 0, 1)
    g = np.zeros((H, W, 4), np.uint8)
    for c in range(3): g[..., c] = (top[c] * (1 - yf) + bot[c] * yf).astype(np.uint8)
    g[..., 3] = 255
    out.paste(Image.fromarray(g, 'RGBA'), (0, 0), fill)
    hi = fill.filter(ImageFilter.MinFilter(max(3, (sw // 2) | 1)))
    a = np.array(hi, np.float32) / 255 * np.clip(1 - yf * 2.4, 0, 1) * 0.5
    hl = np.zeros((H, W, 4), np.uint8); hl[..., :3] = 255; hl[..., 3] = (a * 255).astype(np.uint8)
    o2 = Image.new('RGBA', out.size, (0, 0, 0, 0)); o2.paste(Image.fromarray(hl, 'RGBA'), (0, 0))
    out.alpha_composite(o2)
    if arco:   # curva hacia arriba en el centro, como el logo
        arr = np.array(out); h, w = arr.shape[:2]
        res = np.zeros_like(arr)
        lift = (arco * size * s * (1 - ((np.arange(w) - w / 2) / (w / 2)) ** 2)).astype(int)
        pad2 = int(arco * size * s) + 1
        res = np.zeros((h + pad2, w, 4), np.uint8)
        for x in range(w): res[pad2 - lift[x]:pad2 - lift[x] + h, x] = arr[:, x]
        out = Image.fromarray(res, 'RGBA')
    return out.resize((out.width // s, out.height // s), Image.LANCZOS)


def cine(text, size, color=(255, 255, 255), glow=None, track=0.04):
    """Rótulo de tráiler (Anton), con espaciado y un resplandor opcional."""
    fnt = font(ANTON, size)
    chars = list(text)
    ws = [fnt.getlength(c) for c in chars]
    sp = size * track
    W = int(sum(ws) + sp * (len(chars) - 1)) + 8
    l, t, r, b = fnt.getbbox(text)
    H = b + 8
    m = Image.new('L', (W, H), 0); d = ImageDraw.Draw(m); x = 4
    for c, w in zip(chars, ws): d.text((x, 4), c, font=fnt, fill=255); x += w + sp
    bbox = m.getbbox(); m = m.crop(bbox)
    pad = int(size * 0.4) if glow else 4
    out = Image.new('RGBA', (m.width + 2 * pad, m.height + 2 * pad), (0, 0, 0, 0))
    if glow:
        gm = Image.new('L', out.size, 0); gm.paste(m, (pad, pad)); gm = gm.filter(ImageFilter.GaussianBlur(size * 0.12))
        out.paste(Image.new('RGBA', out.size, glow + (255,)), (0, 0), gm.point(lambda v: min(255, v * 2)))
    sh = Image.new('L', out.size, 0); sh.paste(m, (pad + 3, pad + 5)); sh = sh.filter(ImageFilter.GaussianBlur(3))
    out.paste(Image.new('RGBA', out.size, (0, 0, 0, 255)), (0, 0), sh.point(lambda v: v * 0.6))
    out.paste(Image.new('RGBA', m.size, color + (255,)), (pad, pad), m)
    return out


def pixel(text, size, color=(255, 255, 255), borde=(0, 0, 0)):
    fnt = font(PIXEL, size)
    st = max(2, size // 10)
    m = _mask(text, fnt, st, 4)
    f = _mask(text, fnt, 0, 4)
    out = Image.new('RGBA', m.size, (0, 0, 0, 0))
    out.paste(Image.new('RGBA', m.size, borde + (255,)), (0, 0), m)
    out.paste(Image.new('RGBA', f.size, color + (255,)), (st, st), f)
    return out


if __name__ == '__main__':
    bg = Image.new('RGBA', (1080, 1300), (20, 30, 50, 255))
    y = 30
    for t, sz, a in [('PokéSurvivor', 170, 0.12), ('¡Cooperativo Local!', 92, 0), ('¡Cooperativo Online!', 92, 0),
                     ('Sincronización', 92, 0), ('con PokéSurvivor', 80, 0), ('¡Megaevoluciones!', 100, 0.06)]:
        im = poke(t, sz, a); bg.alpha_composite(im, ((1080 - im.width) // 2, y)); y += im.height + 10
    im = cine('LA SENTENCIA', 150, glow=(255, 60, 40)); bg.alpha_composite(im, ((1080 - im.width) // 2, y)); y += im.height
    im = pixel('MEGA-CHARIZARD X', 64, (255, 230, 120)); bg.alpha_composite(im, ((1080 - im.width) // 2, y))
    bg.save('/tmp/estilo.png')

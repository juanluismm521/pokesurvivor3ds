# trailer.py — monta el tráiler vertical (1080x1920, 60 fps) con las grabaciones del juego.
#   python trailer.py [desde] [hasta] [salida.mp4]     (prueba: un trozo; sin nada, entero)
#   python trailer.py --foto T                         (un fotograma suelto, para revisar)
import math, subprocess, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageEnhance, ImageChops
import fuente
from fuente import clip, frame, top, bottom, zoom
from estilo import poke, cine, pixel
from consola import consola
from tiempos import *

W, H, FPS = 1080, 1920, 60
OUT = '/root/trailer/'

# ---------------------------------------------------------------- utilidades
_txt = {}
def T(kind, text, size, **kw):
    k = (kind, text, size, tuple(sorted(kw.items())))
    if k not in _txt: _txt[k] = {'poke': poke, 'cine': cine, 'pixel': pixel}[kind](text, size, **kw)
    return _txt[k]

def ease_out(x): x = min(1, max(0, x)); return 1 - (1 - x) ** 3
def ease_io(x): x = min(1, max(0, x)); return x * x * (3 - 2 * x)
def clamp(x, a=0, b=1): return max(a, min(b, x))

def put(c, im, cx, cy, scale=1.0, alpha=1.0, rot=0):
    """Pega `im` (RGBA) centrado en (cx, cy) con escala, transparencia y giro."""
    if alpha <= 0.01 or scale <= 0.01: return
    if scale != 1: im = im.resize((max(1, int(im.width * scale)), max(1, int(im.height * scale))), Image.BICUBIC)
    if rot: im = im.rotate(rot, Image.BICUBIC, expand=True)
    if alpha < 1:
        a = im.getchannel('A').point(lambda v: int(v * alpha)); im = im.copy(); im.putalpha(a)
    c.paste(im, (int(cx - im.width / 2), int(cy - im.height / 2)), im)

def slam(c, im, cx, cy, lt, dur=0.22, big=1.6, out_at=None, out_dur=0.18):
    """Rótulo que entra de golpe (grande -> su tamaño, con rebote) y, si se dice, sale."""
    if lt < 0: return
    k = clamp(lt / dur)
    s = 1 + (big - 1) * (1 - ease_out(k)) - 0.06 * math.sin(k * math.pi) * (k > 0.6)
    a = clamp(lt / (dur * 0.6))
    if out_at is not None and lt > out_at:
        o = clamp((lt - out_at) / out_dur); a *= 1 - o; s *= 1 + 0.15 * o
    put(c, im, cx, cy, s, a)

_vig = None
def vignette(c, k=0.55):
    global _vig
    if _vig is None:
        y, x = np.mgrid[0:H, 0:W]
        d = np.sqrt(((x - W / 2) / (W * 0.62)) ** 2 + ((y - H / 2) / (H * 0.58)) ** 2)
        _vig = Image.fromarray((np.clip(d - 0.55, 0, 1) * 255).astype(np.uint8))
    c.paste((0, 0, 0), (0, 0), _vig.point(lambda v: int(v * k)))

def flash(c, a, col=(255, 255, 255)):
    if a > 0.01: c.paste(Image.blend(c, Image.new('RGB', c.size, col), clamp(a)))

_bgbase = None
def background(c, t, tint=(0, 0, 0)):
    """Fondo azul noche con luces que se mueven despacio."""
    global _bgbase
    if _bgbase is None:
        y = np.linspace(0, 1, H)[:, None, None]
        top_, bot_ = np.array([10, 14, 34]), np.array([26, 14, 46])
        _bgbase = Image.fromarray(np.repeat((top_ * (1 - y) + bot_ * y).astype(np.uint8), W, 1))
    c.paste(_bgbase)
    d = ImageDraw.Draw(c, 'RGBA')
    for i in range(5):
        x = W / 2 + math.sin(t * 0.4 + i * 1.7) * 420; yy = 300 + i * 340 + math.cos(t * 0.3 + i) * 80
        r = 260
        d.ellipse((x - r, yy - r, x + r, yy + r), fill=(60 + i * 20, 70, 160, 14))

def blurred_game(c, arr, dark=0.45):
    """La pantalla del juego, ampliada y desenfocada, de fondo."""
    im = Image.fromarray(arr).resize((135, 240), Image.BILINEAR).filter(ImageFilter.GaussianBlur(5))
    im = im.resize((W, H), Image.BILINEAR)
    c.paste(ImageEnhance.Brightness(im).enhance(dark))

_shadow = {}
def panel(c, im, cx, cy, border=6, glow=(255, 214, 80), radius=18):
    """Pantalla del juego como panel: borde, sombra y esquinas redondeadas."""
    w, h = im.size
    k = (w, h, border, glow, radius)
    if k not in _shadow:
        pad = 60
        s = Image.new('L', (w + 2 * pad, h + 2 * pad), 0)
        ImageDraw.Draw(s).rounded_rectangle((pad - 4, pad + 10, pad + w + 4, pad + h + 18), radius + 6, fill=200)
        s = s.filter(ImageFilter.GaussianBlur(22))
        m = Image.new('L', (w, h), 0); ImageDraw.Draw(m).rounded_rectangle((0, 0, w - 1, h - 1), radius, fill=255)
        b = Image.new('L', (w + 2 * border, h + 2 * border), 0)
        ImageDraw.Draw(b).rounded_rectangle((0, 0, w + 2 * border - 1, h + 2 * border - 1), radius + border, fill=255)
        _shadow[k] = (s, m, b, pad)
    s, m, b, pad = _shadow[k]
    x0, y0 = int(cx - w / 2), int(cy - h / 2)
    c.paste((0, 0, 0), (x0 - pad, y0 - pad), s)
    if border: c.paste(glow, (x0 - border, y0 - border), b)
    c.paste(im, (x0, y0), m)

def shake(t, t0, amp=18, dur=0.35):
    if t < t0 or t > t0 + dur: return 0, 0
    k = 1 - (t - t0) / dur
    return math.sin(t * 91) * amp * k, math.cos(t * 77) * amp * k

def chroma(c, amt):
    if amt < 0.5: return c
    r, g, b = c.split()
    r = ImageChops.offset(r, int(amt), 0); b = ImageChops.offset(b, -int(amt), 0)
    return Image.merge('RGB', (r, g, b))

def rainbow_burst(c, cx, cy, lt, n=24, speed=1400):
    d = ImageDraw.Draw(c, 'RGBA')
    cols = [(255, 95, 109), (255, 210, 63), (95, 224, 138), (95, 200, 255), (196, 123, 255), (255, 138, 216)]
    for i in range(n):
        a = i / n * 2 * math.pi + lt * 0.6
        r0, r1 = lt * speed * 0.55, lt * speed
        al = int(255 * clamp(1 - lt / 0.9))
        if al <= 0: continue
        col = cols[i % 6] + (al,)
        x0, y0 = cx + math.cos(a) * r0, cy + math.sin(a) * r0
        x1, y1 = cx + math.cos(a) * r1, cy + math.sin(a) * r1
        d.line((x0, y0, x1, y1), fill=col, width=10)
    for k in range(3):
        rr = lt * speed * (0.5 + k * 0.25)
        al = int(200 * clamp(1 - lt / 0.7))
        if al > 0: d.ellipse((cx - rr, cy - rr, cx + rr, cy + rr), outline=cols[k * 2] + (al,), width=12)

def sparkles(c, t, seed, n=40, area=(0, 0, W, H), col=(255, 240, 160)):
    rng = np.random.default_rng(seed)
    d = ImageDraw.Draw(c, 'RGBA')
    for i in range(n):
        x = area[0] + rng.random() * (area[2] - area[0]); y = area[1] + rng.random() * (area[3] - area[1])
        ph = rng.random() * 6.28; sp = 1 + rng.random() * 2
        y -= (t * 60 * sp) % (area[3] - area[1])
        if y < area[1]: y += area[3] - area[1]
        a = 0.5 + 0.5 * math.sin(t * 5 * sp + ph)
        r = 2 + 4 * a
        d.line((x - r * 2, y, x + r * 2, y), fill=col + (int(220 * a),), width=2)
        d.line((x, y - r * 2, x, y + r * 2), fill=col + (int(220 * a),), width=2)

def chip(text, size=44, bg=(255, 203, 5), fg=(20, 30, 80)):
    from estilo import font, ANTON
    f = font(ANTON, size)
    l, tt, r, b = f.getbbox(text)
    w, h = r - l + size, int(size * 1.45)
    im = Image.new('RGBA', (w, h), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    d.rounded_rectangle((0, 0, w - 1, h - 1), h // 2, fill=bg + (255,))
    d.text((w / 2, h / 2), text, font=f, fill=fg + (255,), anchor='mm')
    return im

# ---------------------------------------------------------------- escenas
# Momentos de las grabaciones (segundos del vídeo): MEGA = el destello de la transformación.
MEGA = {'gengar': 12.85, 'charx': 12.817, 'darkrai': 12.85, 'absolz': 12.85,
        'gengar_paz': 12.983, 'charx_paz': 11.933, 'darkrai_paz': 11.933}   # (las «_paz»: sin enemigos)
CX, CY = 200, 132      # dónde suele estar tu Pokémon en la pantalla de arriba

def track(name, st, dur, lt, cx=CX, cy=CY):
    p = fuente.posicion(name, st, dur, int(lt * 60))
    return (cx, cy) if p is None else (float(p[0]), float(p[1]))

def game_wide(name, st, lt, cw=300, zoom0=1.0, zoom1=1.0, dur=1.0, cx=None, cy=None, out=(1080, 864)):
    fr = clip(name, st, dur + 0.2)
    f = frame(fr, lt)
    tx, ty = track(name, st, dur + 0.2, lt)
    cx = (tx * 0.6 + 200 * 0.4) if cx is None else cx   # (sigue a tu Pokémon, sin pegarse del todo)
    cy = (ty * 0.6 + 120 * 0.4) if cy is None else cy
    z = zoom0 + (zoom1 - zoom0) * ease_io(lt / dur)
    w, h = cw / z, cw / z * out[1] / out[0]
    cx = min(400 - w / 2, max(w / 2, cx)) if w < 400 else 200
    cy = min(240 - h / 2, max(h / 2, cy)) if h < 240 else 120
    return zoom(top(f), cx, cy, w, h, out[0], out[1]), f

def s_intro(c, t):
    lines = [('SOBREVIVE', 'horda', 6.0), ('DERROTA A ARCEUS', 'sentencia', 33.45), ('TODOS LOS POKÉMON DISPONIBLES', 'horda', 20.0)]
    k = int(t // B_J)
    if k < 3:
        lt = t - k * B_J
        txt, name, st = lines[k]
        fr = clip(name, st, B_J + 0.2); f = frame(fr, lt)
        z = 1 + 0.06 * ease_io(lt / B_J)
        im = zoom(top(f), 200, 120, 400 / z, 240 / z, 1040, 624)
        ib = zoom(bottom(f), 160, 120, 320, 240, 832, 624)
        c.paste((4, 6, 14), (0, 0, W, H))
        br = 0.8 * clamp(lt / 0.25)
        im = ImageEnhance.Brightness(ImageEnhance.Color(im).enhance(0.7)).enhance(br)
        ib = ImageEnhance.Brightness(ib).enhance(br)
        dx, dy = shake(t, k * B_J, 10, 0.25)
        panel(c, im, W / 2 + dx, 640 + dy, border=0)
        panel(c, ib, W / 2 + dx, 1300 + dy, border=0)
        size = 150 if k == 0 else (110 if k == 1 else 76)
        slam(c, T('cine', txt, size, glow=(90, 120, 255)), W / 2, 640, lt, 0.18, 1.35, B_J - 0.2)
        flash(c, 0.6 * clamp(1 - lt / 0.12))
    else:   # el logo
        lt = t - 3 * B_J
        fr = clip('charx', MEGA['charx'] + 3.0, B_J + 0.3)
        blurred_game(c, top(frame(fr, lt)), 0.55)
        rays(c, W / 2, 900, t, clamp(lt / 0.3))
        sparkles(c, t, 7, 60)
        logo = T('poke', 'PokéSurvivor', 138, arco=0.12)
        s = 1.0 + 0.04 * lt
        slam(c, logo, W / 2, 860, lt, 0.25, 1.8)
        put(c, logo3ds(360), W / 2, 1080, (1 + 0.04 * lt) * ease_out(clamp((lt - 0.25) / 0.2)))
        flash(c, 0.9 * clamp(1 - lt / 0.25))
        if lt > B_J - 0.25: flash(c, clamp((lt - (B_J - 0.25)) / 0.25))

def rays(c, cx, cy, t, a=1.0, n=14):
    """Rayos de luz que giran detrás de un rótulo."""
    lay = Image.new('L', (W // 4, H // 4), 0); d = ImageDraw.Draw(lay)
    for i in range(n):
        ang = t * 0.35 + i * 2 * math.pi / n
        r = 2400 / 4
        p0 = (cx / 4, cy / 4)
        p1 = (cx / 4 + math.cos(ang - 0.09) * r, cy / 4 + math.sin(ang - 0.09) * r)
        p2 = (cx / 4 + math.cos(ang + 0.09) * r, cy / 4 + math.sin(ang + 0.09) * r)
        d.polygon([p0, p1, p2], fill=int(70 * a))
    lay = lay.filter(ImageFilter.GaussianBlur(3)).resize((W, H), Image.BILINEAR)
    c.paste((255, 225, 120), (0, 0), lay)

def s_montage(c, t):
    lt0 = t - T_DROP
    cuts = [('horda', 24.0), ('charx', MEGA['charx'] + 6), ('absolz', MEGA['absolz'] + 5), ('darkrai', MEGA['darkrai'] + 4)]
    k = min(3, int(lt0 // (B_R / 2)))
    lt = lt0 - k * B_R / 2
    name, st = cuts[k]
    background(c, t)
    im, f = game_wide(name, st, lt, 260, 1.25, 1.0, B_R / 2, out=(1080, 1080))
    dx, dy = shake(t, T_DROP + k * B_R / 2, 22, 0.3)
    panel(c, im, W / 2 + dx, 900 + dy, border=8, glow=(255, 255, 255))
    flash(c, 0.75 * clamp(1 - lt / 0.1))
    return chroma(c, 14 * clamp(1 - lt / 0.15))

def s_mega_title(c, t):
    lt = t - r(2)
    background(c, t)
    fr = clip('novedades', 6.9, 3.2)
    f = frame(fr, lt)
    im = zoom(top(f), 200, 158, 300, 160, 1000, 533)
    if lt < B_R:   # «Y ahora...»
        c.paste((4, 6, 14), (0, 0, W, H))
        put(c, T('cine', 'ADEMÁS...', 130, glow=(150, 90, 255)), W / 2, 940, 1 + 0.05 * lt, clamp(lt / 0.3) * clamp((B_R - lt) / 0.15))
        return c
    l2 = lt - B_R
    panel(c, im, W / 2, 1180, border=6, glow=(196, 123, 255))
    rainbow_burst(c, W / 2, 560, l2)
    slam(c, T('poke', 'Megaevoluciones', 118, arco=0.08), W / 2, 560, l2, 0.22, 1.9)
    sparkles(c, t, 11, 40, (0, 300, W, 900), (255, 255, 255))
    flash(c, 0.9 * clamp(1 - l2 / 0.18))
    return chroma(c, 18 * clamp(1 - l2 / 0.2))

def y_button(c, cx, cy, press):
    d = ImageDraw.Draw(c, 'RGBA')
    r = 70; p = 8 * press
    d.ellipse((cx - r, cy - r + 10, cx + r, cy + r + 10), fill=(10, 20, 30, 255))
    d.ellipse((cx - r, cy - r + p, cx + r, cy + r + p), fill=(70, 190, 100, 255))
    d.ellipse((cx - r + 10, cy - r + 8 + p, cx + r - 10, cy + r - 14 + p), fill=(95, 215, 125, 255))
    put(c, T('cine', 'Y', 96), cx, cy + p - 4)

MEGA_INFO = {'gengar': ('Mega-Gengar', (123, 98, 163), None), 'charx': ('Mega-Charizard X', (40, 90, 220), None),
             'darkrai': ('Mega-Darkrai', (60, 50, 90), 'NUEVA · LEYENDAS Z-A'),
             'gengar_paz': ('Mega-Gengar', (123, 98, 163), None), 'charx_paz': ('Mega-Charizard X', (40, 90, 220), None),
             'darkrai_paz': ('Mega-Darkrai', (90, 70, 140), 'NUEVA · LEYENDAS Z-A')}

def mega_closeup(c, t, name, lt, dur, pre=0.35, card_at=0.55, speed=1.0, cw=150):
    """Primer plano de tu Pokémon (sin enemigos) megaevolucionando, y su nombre."""
    st = MEGA[name] - pre * speed
    fr = clip(name, st, dur + 0.3, speed=speed)
    f = frame(fr, lt)
    z = 1.0 + 0.12 * ease_io(lt / dur)
    w, h = cw / z, cw * 1.125 / z
    px, py = track(name, st, dur + 0.3, lt)
    px = min(400 - w / 2, max(w / 2, px)); py = min(240 - h / 2, max(h / 2, py))
    im = zoom(top(f), px, py, w, h, 960, 1080)
    background(c, t)
    sparkles(c, t, 31, 30, (0, 400, W, 1700), (255, 255, 255))
    dx, dy = shake(t, t - lt + pre, 16, 0.4)
    panel(c, im, W / 2 + dx, 1040 + dy, border=8, glow=MEGA_INFO[name][1])
    title, col, tag = MEGA_INFO[name]
    l2 = lt - card_at
    if l2 > 0:
        slam(c, T('poke', title, 104), W / 2, 290, l2, 0.2, 1.7)
        if tag: put(c, chip(tag, 40, (196, 123, 255), (255, 255, 255)), W / 2, 418, ease_out(clamp((l2 - 0.2) / 0.2)))
    flash(c, 0.85 * clamp(1 - abs(lt - pre) / 0.12))

def s_how(c, t):
    """Cómo se megaevoluciona: las dos pantallas con el aviso «Y», la Y que se pulsa
    (justo cuando la pulsa el juego) y la transformación a cámara lenta."""
    lt = t - r(4)
    PRESS = MEGA['gengar_paz']
    if lt < 2 * B_R:
        st = PRESS - 2 * B_R
        fr = clip('gengar_paz', st, 2 * B_R + 0.2); f = frame(fr, lt)
        background(c, t)
        tx, ty = track('gengar_paz', st, 2 * B_R + 0.2, lt)
        im_t = zoom(top(f), min(250, max(150, tx)), min(150, max(90, ty)), 300, 180, 900, 540)
        panel(c, im_t, W / 2, 650, border=6, glow=(123, 98, 163))
        im_b = zoom(bottom(f), 160, 120, 320, 240, 800, 600)
        panel(c, im_b, W / 2, 1300, border=6, glow=(95, 215, 125))
        # el aviso de abajo («Y: ¡Megaevolucionar!» y la megapiedra), resaltado
        k = 800 / 320; x0, y0 = W / 2 - 400, 1000
        pul = 0.5 + 0.5 * math.sin(t * 9)
        d = ImageDraw.Draw(c, 'RGBA')
        d.rounded_rectangle((x0 + 6 * k, y0 + 146 * k, x0 + 314 * k, y0 + 173 * k), 14, outline=(255, 214, 80, int(140 + 115 * pul)), width=7)
        put(c, T('cine', 'SI TE SALE SU MEGAPIEDRA...', 70), W / 2, 250, 1, clamp(lt / 0.2))
        l2 = lt - B_R
        if l2 > 0:
            put(c, T('cine', 'Y PULSA', 80), W / 2 - 90, 1700, 1, clamp(l2 / 0.15))
            press = clamp(1 - abs(lt - 2 * B_R) / 0.1)
            y_button(c, W / 2 + 170, 1705, press)
        flash(c, clamp((lt - (2 * B_R - 0.1)) / 0.1))
    else:
        # la transformación, a media velocidad, y su nombre
        mega_closeup(c, t, 'gengar_paz', lt - 2 * B_R, 2 * B_R, pre=0.1, card_at=1.4, speed=0.5, cw=130)

def s_mega(name, bar):
    def fn(c, t):
        mega_closeup(c, t, name, t - r(bar), 2 * B_R, pre=0.35, card_at=0.5, speed=0.75)
    return fn

# Megas con dibujo propio para «y muchas más» (n.º en kMegas; la galería las enseña de 8 en 8)
MORE = [51, 20, 50, 80, 59, 29, 56, 33, 17, 79, 74, 81]
ZA = {80, 79, 74, 81}
def s_darkrai_anda(c, t):
    """Mega-Darkrai ya transformado, andando (para ver su sprite en movimiento)."""
    lt = t - r(12)
    st, sp = 15.0, 1.4
    fr = clip('darkrai_paz', st, B_R + 0.3, speed=sp); f = frame(fr, lt)
    w, h = 150, 168.75
    px, py = track('darkrai_paz', st, B_R + 0.3, lt)
    px = min(400 - w / 2, max(w / 2, px)); py = min(240 - h / 2, max(h / 2, py))
    background(c, t)
    sparkles(c, t, 31, 30, (0, 400, W, 1700), (255, 255, 255))
    panel(c, zoom(top(f), px, py, w, h, 960, 1080), W / 2, 1040, border=8, glow=MEGA_INFO['darkrai_paz'][1])
    put(c, T('poke', 'Mega-Darkrai', 104), W / 2, 290)
    put(c, chip('NUEVA · LEYENDAS Z-A', 40, (196, 123, 255), (255, 255, 255)), W / 2, 418)

def s_more(c, t):
    lt = t - r(13)
    background(c, t)
    sparkles(c, t, 41, 50)
    cols, rows = 3, 4
    cw, ch = 300, 288
    for j, i in enumerate(MORE):
        page, k = i // 8, i % 8
        x, y = 50 + (k % 4) * 100, 98 + (k // 4) * 112
        st = 1.75 + 3.134 * page   # (la página 1 empieza en 1,583 s y cada una dura 3,134 s)
        fr = clip('galeria', st, 1.6)
        f = frame(fr, (lt * 0.8) % 1.5)
        im = zoom(top(f), x - 3, y - 26, 92, 84, cw, int(cw * 0.913))
        a = ease_out(clamp((lt - 0.08 * j) / 0.25))
        if a <= 0: continue
        cx = W / 2 + (j % cols - 1) * (cw + 30); cy = 620 + (j // cols) * (ch + 22)
        s = 0.6 + 0.4 * a
        im = im.resize((int(im.width * s), int(im.height * s)), Image.BILINEAR)
        panel(c, im, cx, cy, border=5, glow=(196, 123, 255) if i in ZA else (255, 214, 80), radius=14)
    slam(c, T('poke', 'Y muchas más', 112), W / 2, 270, lt, 0.2, 1.7)
    put(c, chip('91 MEGAS · TAMBIÉN DE LEYENDAS Z-A', 44, (196, 123, 255), (255, 255, 255)), W / 2, 395, ease_out(clamp((lt - 0.3) / 0.25)))
    flash(c, 0.7 * clamp(1 - lt / 0.12))

def s_stone(c, t):
    lt = t - r(15)
    background(c, t)
    fr = clip('novedades', 14.6, 2 * B_R + 0.2); f = frame(fr, lt)
    z = 1 + 0.15 * ease_io(lt / (2 * B_R))
    im = zoom(top(f), 282, 168, 228 / z, 150 / z, 1000, 658)
    panel(c, im, W / 2, 1000, border=6, glow=(196, 123, 255))
    slam(c, T('poke', 'Cada una con', 96), W / 2, 330, lt, 0.18, 1.5)
    slam(c, T('poke', 'su megapiedra', 96), W / 2, 455, lt - 0.12, 0.18, 1.5)
    put(c, T('cine', 'Y SU PROBABILIDAD, QUE PUEDES MEJORAR', 46), W / 2, 1420, 1, clamp((lt - 1.0) / 0.25))
    flash(c, 0.6 * clamp(1 - lt / 0.1))

def s_power(c, t):
    lt = t - r(17)
    im, f = game_wide('charx', MEGA['charx'] + 2.0, lt, 250, 1.1, 1.0, 2 * B_R, out=(1080, 1080))
    background(c, t)
    panel(c, im, W / 2, 940, border=8, glow=(40, 90, 220))
    chips = ['+25% ATAQUE', '+15% VELOCIDAD', '-25% RECARGA', '+REGENERACIÓN']
    for i, s in enumerate(chips):
        l2 = lt - i * B_R / 2
        if l2 > 0:
            y = 300 + (i % 2) * 120 if i < 2 else 1560 - ((i % 2) * 120)
            x = W / 2 + (-220 if i % 2 == 0 else 220)
            slam(c, chip(s, 50), x, y, l2, 0.16, 1.5)

def s_sentencia(c, t):
    lt = t - T_SENT
    if lt < B_J:
        c.paste((0, 0, 0), (0, 0, W, H))
        put(c, T('cine', 'A LOS 30 MINUTOS...', 84, color=(220, 220, 230)), W / 2, 960, 1 + 0.04 * lt, clamp(lt / 0.4) * clamp((B_J - lt) / 0.2))
        return c
    l2 = lt - B_J
    k = min(2, int(l2 // B_J)); l3 = l2 - k * B_J
    fr = clip('sentencia', SENT_T[k], B_J + 0.2)
    f = frame(fr, l3)
    w = 330 - 40 * ease_io(l3 / B_J); h = min(240, w)
    im = zoom(top(f), 200, 120, w, h, 1080, int(1080 * h / w))
    c.paste((20, 0, 0), (0, 0, W, H))
    blurred_game(c, top(f), 0.3)
    c.paste(Image.blend(c, Image.new('RGB', c.size, (120, 0, 0)), 0.35))
    dx, dy = shake(t, T_SENT + B_J + k * B_J, 26 if k != 1 else 12, 0.6)
    panel(c, im, W / 2 + dx, 980 + dy, border=6, glow=(255, 60, 40))
    slam(c, T('cine', 'LA SENTENCIA', 170, glow=(255, 40, 20)), W / 2 + dx, 360 + dy, l2, 0.25, 1.8)
    put(c, T('cine', '¿SOBREVIVIRÁS?', 70, color=(255, 210, 200)), W / 2, 1520, 1, clamp((l2 - 2.2) / 0.3))
    flash(c, 0.9 * clamp(1 - l3 / 0.2), (255, 220, 200))
    vignette(c, 0.8)
    return chroma(c, 16 * clamp(1 - l2 / 0.3))

def feature_title(c, lines, lt, y0=300):
    for i, s in enumerate(lines):
        slam(c, T('poke', s, 104 if len(lines) == 1 else 96), W / 2, y0 + i * 125, lt - i * 0.1, 0.2, 1.7)

def s_coop_local(c, t):
    lt = t - f(0)
    background(c, t)
    fa = frame(clip('coop_A', COOP_L, 2 * B_B + 0.3), lt); fb = frame(clip('coop_B', COOP_LB, 2 * B_B + 0.3), lt)
    for i, (fx, lab) in enumerate(((fa, 'JUGADOR 1'), (fb, 'JUGADOR 2'))):
        im = zoom(top(fx), 200, 120, 400, 240, 840, 504)
        k = ease_out(clamp((lt - 0.1 - i * 0.12) / 0.3))
        x = W / 2 + (1 - k) * (-900 if i == 0 else 900)
        panel(c, im, x, 790 + i * 600, border=6, glow=(255, 214, 80) if i == 0 else (95, 200, 255))
        put(c, chip(lab, 34, (255, 214, 80) if i == 0 else (95, 200, 255)), x - 300, 790 + i * 600 - 270, k)
    feature_title(c, ['Cooperativo Local'], lt, 300)
    flash(c, 0.6 * clamp(1 - lt / 0.12))

def globe(c, cx, cy, r, t):
    d = ImageDraw.Draw(c, 'RGBA')
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(30, 80, 170, 230), outline=(140, 200, 255, 255), width=5)
    for k in range(-2, 3):
        ry = r * math.cos(k * 0.5)
        d.ellipse((cx - r, cy + r * math.sin(k * 0.5) - 3, cx + r, cy + r * math.sin(k * 0.5) + 3), outline=(140, 200, 255, 120))
    for k in range(4):
        ph = (t * 0.8 + k / 4) % 1
        w = r * abs(math.cos(ph * math.pi))
        d.ellipse((cx - w, cy - r, cx + w, cy + r), outline=(140, 200, 255, 160), width=3)
    for k in range(3):   # ondas
        rr = r + ((t * 120 + k * 60) % 180)
        al = int(200 * (1 - (rr - r) / 180))
        d.ellipse((cx - rr, cy - rr, cx + rr, cy + rr), outline=(255, 214, 80, al), width=4)

def s_coop_online(c, t):
    lt = t - f(2)
    background(c, t)
    fa = frame(clip('online_A', COOP_O, 2 * B_B + 0.3), lt)
    im = zoom(top(fa), 200, 125, 300, 240, 1000, 800)
    panel(c, im, W / 2, 1000, border=6, glow=(95, 200, 255))
    fb = frame(clip('online_B', COOP_O, 2 * B_B + 0.3), lt)
    im2 = zoom(top(fb), 200, 125, 400, 240, 420, 252)
    k = ease_out(clamp((lt - 0.4) / 0.3))
    panel(c, im2, W - 260 + (1 - k) * 600, 1430, border=5, glow=(255, 214, 80))
    globe(c, 200, 1430, 110 * k, t)
    feature_title(c, ['Cooperativo Online'], lt, 300)
    put(c, T('cine', 'CON TUS AMIGOS, ESTÉS DONDE ESTÉS', 46), W / 2, 450, 1, clamp((lt - 0.4) / 0.2))
    flash(c, 0.6 * clamp(1 - lt / 0.12))

def sync_icon(c, cx, cy, r, t):
    d = ImageDraw.Draw(c, 'RGBA')
    a0 = t * 200
    for k in (0, 180):
        d.arc((cx - r, cy - r, cx + r, cy + r), a0 + k, a0 + k + 140, fill=(255, 214, 80, 255), width=16)
        a = math.radians(a0 + k + 140)
        x, y = cx + r * math.cos(a), cy + r * math.sin(a)
        d.polygon([(x + 22 * math.cos(a + 2.6), y + 22 * math.sin(a + 2.6)), (x + 26 * math.cos(a - 0.2), y + 26 * math.sin(a - 0.2)),
                   (x - 30 * math.sin(a), y + 30 * math.cos(a))], fill=(255, 214, 80, 255))

def s_sync(c, t):
    lt = t - f(4)
    background(c, t)
    fr = clip('qr', QR_T, 2 * B_B + 0.3); fq = frame(fr, lt)
    im = zoom(bottom(fq), 160, 120, 320, 240, 880, 660)
    panel(c, im, W / 2, 1010, border=6, glow=(255, 255, 255))
    feature_title(c, ['Sincronización', 'con PokéSurvivor'], lt, 290)
    k = ease_out(clamp((lt - 0.5) / 0.3))
    put(c, chip('pokesurvivor.com', 52, (255, 255, 255), (30, 40, 90)), W / 2 + 120, 1440, k)
    sync_icon(c, 230, 1440, 46 * k + 0.01, t)
    put(c, T('cine', 'TU PARTIDA, EN LA WEB Y EN LA 3DS', 46), W / 2, 1560, 1, clamp((lt - 0.8) / 0.2))
    flash(c, 0.6 * clamp(1 - lt / 0.12))

def s_updates(c, t):
    lt = t - f(6)
    background(c, t)
    fr = clip('actualizar', UPD_T, 2 * B_B + 0.3, speed=6.6); fu = frame(fr, lt)
    im = zoom(bottom(fu), 160, 120, 320, 240, 880, 660)
    panel(c, im, W / 2, 1000, border=6, glow=(95, 224, 138))
    feature_title(c, ['Actualizaciones', 'automáticas'], lt, 290)
    vers = ['0.1', '0.2', '0.3', '0.4']
    for i, v in enumerate(vers):
        l2 = lt - 0.5 - i * 0.35
        if l2 > 0:
            slam(c, chip('v' + v, 50, (95, 224, 138) if i == 3 else (255, 255, 255), (20, 40, 30)), 190 + i * 235, 1440, l2, 0.15, 1.5)
    put(c, T('cine', 'Y CONSTANTES', 60), W / 2, 1560, 1, clamp((lt - 2.0) / 0.2))
    flash(c, 0.6 * clamp(1 - lt / 0.12))

import consola_real
_logo3 = None
def logo3ds(width):
    """El logo original de 3DS (el «3» rojo y «DS» en blanco, para fondo oscuro)."""
    global _logo3
    if _logo3 is None: _logo3 = Image.open('/root/trailer/real/logo_3ds_blanco.png').convert('RGBA')
    k = width / _logo3.width
    return _logo3.resize((int(_logo3.width * k), int(_logo3.height * k)), Image.LANCZOS)

def s_final(c, t):
    lt = t - T_FIN
    name, st = 'darkrai', MEGA['darkrai'] + 4.0
    fr = clip(name, st, T_TOTAL - T_FIN + 0.3)
    fx = frame(fr, lt)
    background(c, t)
    sparkles(c, t, 21, 40)
    CWID, CHEI = consola_real.size()
    tq, bq = consola_real.quads()
    # De la pantalla de arriba a todo lo ancho a la consola entera (980 de ancho).
    k = ease_io(clamp((lt - 1.2) / 3.2))
    tw = (tq[1][0] - tq[0][0] + tq[2][0] - tq[3][0]) / 2
    s_start, s_end = 1080 / tw, 980 / CWID
    sc = math.exp(math.log(s_start) * (1 - k) + math.log(s_end) * k)   # (zoom suave: en escala logarítmica)
    tcx, tcy = tq[:, 0].mean(), tq[:, 1].mean()
    cx0 = W / 2 - (tcx - CWID / 2) * s_start; cy0 = 960 - (tcy - CHEI / 2) * s_start
    cx = cx0 * (1 - k) + (W / 2) * k; cy = cy0 * (1 - k) + 1010 * k
    arr = np.array(c)
    consola_real.compose(arr, sc, cx, cy, np.ascontiguousarray(top(fx)), np.ascontiguousarray(bottom(fx)))
    c.paste(Image.fromarray(arr))
    # rótulos finales
    l2 = lt - 3.8
    if l2 > 0:
        slam(c, T('poke', 'PokéSurvivor', 150, arco=0.12), W / 2, 250, l2, 0.25, 1.6)
        put(c, logo3ds(300), W / 2, 420, ease_out(clamp((l2 - 0.3) / 0.25)))
    l3 = lt - 5.2
    if l3 > 0:
        put(c, T('cine', 'GRATIS · HOMEBREW PARA NINTENDO 3DS', 50), W / 2, 1530, 1, clamp(l3 / 0.3))
    lg = t - T_GOLPE
    if lg > 0:
        put(c, T('cine', 'por juanluismm520', 56, color=(255, 214, 80)), W / 2, 1620, 1, clamp(lg / 0.3))
        put(c, T('cine', 'Proyecto de fans sin ánimo de lucro · Pokémon © Nintendo, Game Freak y The Pokémon Company', 23,
                 color=(185, 190, 215)), W / 2, 1715, 1, clamp((lg - 0.3) / 0.5))
        put(c, T('cine', 'Foto de la New 3DS XL: Antoine Turmel (CC BY 2.0) · Música y juego: PokéSurvivor', 21,
                 color=(150, 155, 185)), W / 2, 1755, 1, clamp((lg - 0.5) / 0.5))
        flash(c, 0.85 * clamp(1 - lg / 0.3))
    if t > T_TOTAL - 0.9: flash(c, (t - (T_TOTAL - 0.9)) / 0.9, (0, 0, 0))
    if lt < 0.12: flash(c, 0.7 * (1 - lt / 0.12))

# Momentos (se ajustan al ver las grabaciones)
SENT_T = (32.30, 33.55, 98.60)   # Arceus desciende, cae a tu lado, los rayos
COOP_L = 33.0
COOP_LB = 36.0   # (la consola 2 espera al anfitrión entre el 30 y el 33)
COOP_O = 18.0
QR_T = 5.6
UPD_T = 4.5

SCENES = [
    (0, T_DROP, s_intro),
    (T_DROP, r(2), s_montage),
    (r(2), r(4), s_mega_title),
    (r(4), r(8), s_how),
    (r(8), r(10), s_mega('charx_paz', 8)),
    (r(10), r(12), s_mega('darkrai_paz', 10)),
    (r(12), r(13), s_darkrai_anda),
    (r(13), r(15), s_more),
    (r(15), r(17), s_stone),
    (r(17), T_SENT, s_power),
    (T_SENT, T_FEAT, s_sentencia),
    (f(0), f(2), s_coop_local),
    (f(2), f(4), s_coop_online),
    (f(4), f(6), s_sync),
    (f(6), T_FIN, s_updates),
    (T_FIN, T_TOTAL + 1, s_final),
]

def render(t):
    c = Image.new('RGB', (W, H), (0, 0, 0))
    for a, b, fn in SCENES:
        if a <= t < b:
            out = fn(c, t)
            if out is not None: c = out
            break
    vignette(c, 0.45)
    return c

def main():
    if sys.argv[1:2] == ['--foto']:
        for ts in sys.argv[2:]:
            render(float(ts)).save(OUT + 'foto_%s.png' % ts); print('foto', ts)
        return
    a = float(sys.argv[1]) if len(sys.argv) > 1 else 0
    b = float(sys.argv[2]) if len(sys.argv) > 2 else T_TOTAL
    out = sys.argv[3] if len(sys.argv) > 3 else OUT + 'video_sin_audio.mp4'
    enc = subprocess.Popen(['ffmpeg', '-v', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', '%dx%d' % (W, H), '-r', str(FPS),
                            '-i', '-', '-c:v', 'libx264', '-preset', 'medium', '-crf', '16', '-pix_fmt', 'yuv420p', out], stdin=subprocess.PIPE)
    n0, n1 = int(a * FPS), int(b * FPS)
    for n in range(n0, n1):
        enc.stdin.write(render(n / FPS).tobytes())
        if n % 120 == 0: print('%.1f s' % (n / FPS), flush=True)
    enc.stdin.close(); enc.wait()
    print('hecho', out)

if __name__ == '__main__':
    main()

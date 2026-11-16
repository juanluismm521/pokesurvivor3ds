# trailer_v1.py — tráiler vertical de la versión 1.0 (1080x1920, 60 fps, ~39 s).
#   python trailer_v1.py [desde] [hasta] [salida.mp4]     (prueba: un trozo; sin nada, entero)
#   python trailer_v1.py --foto T [T…]                    (fotogramas sueltos, para revisar)
# Reutiliza las piezas de trailer.py (letras, paneles, destellos, la consola real) con la escaleta
# de tiempos_v1.py y las grabaciones v1_* (grabar.sh, ver README).
import math, sys, subprocess
import numpy as np
from PIL import Image, ImageDraw, ImageEnhance
import trailer as tr
from trailer import (W, H, FPS, OUT, T, L, EN, ease_out, ease_io, clamp, put, slam, vignette, flash, background,
                     blurred_game, panel, shake, chroma, rainbow_burst, sparkles, chip, rays, y_button, logo3ds)
from fuente import clip, frame, top, bottom, zoom
import fuente, consola_real
from tiempos_v1 import *

EN.update({
    'LLEGA LA 1.0': 'VERSION 1.0 IS HERE', 'LA GRAN ACTUALIZACIÓN': 'THE BIG UPDATE', 'MÁS ÉPICA QUE NUNCA': 'MORE EPIC THAN EVER',
    'VERSIÓN 1.0': 'VERSION 1.0', 'MINUTO 20': 'MINUTE 20', 'Eternatus': 'Eternatus', 'ETERNAMAX': 'ETERNAMAX',
    'TRES FASES · NÚCLEOS · RAYO ETERNAMAX': 'THREE PHASES · CORES · ETERNABEAM',
    'VÉNCELO Y ELIGE TU PODER': 'BEAT IT AND CHOOSE YOUR POWER', 'Dinamax': 'Dynamax', 'Teracristal': 'Terastal',
    'Pulsera Z': 'Z-Ring', 'Megaevolución': 'Mega Evolution', 'Pasivas a la vista': 'Passives at a glance',
    'CADA CARTA, CON SU ICONO Y CUÁNTAS LLEVAS': 'EVERY CARD, WITH ITS ICON AND HOW MANY YOU HAVE',
    'Tienda Kecleon': 'Kecleon Shop', 'MT · MENTAS · HABILIDADES': 'TMs · MINTS · ABILITIES',
    'Pesadilla': 'Nightmare', 'NUEVAS DIFICULTADES: DIFÍCIL Y PESADILLA': 'NEW DIFFICULTIES: HARD AND NIGHTMARE',
    'Desafío semanal': 'Weekly Challenge', 'EL MISMO RETO PARA TODOS, CADA SEMANA': 'THE SAME CHALLENGE FOR EVERYONE, EVERY WEEK',
    'Gacha ×100': 'Gacha ×100', 'CON NATURALEZA Y HABILIDAD EN CADA POKÉMON': 'EVERY POKÉMON WITH ITS NATURE AND ABILITY',
    'Y mucho más': 'And much more', 'NUEVO MENÚ · MAESTRÍA · EVOLUCIONES': 'NEW MENU · MASTERY · EVOLUTIONS',
    'VERSIÓN 1.0 · GRATIS · HOMEBREW PARA 3DS': 'VERSION 1.0 · FREE · 3DS HOMEBREW',
})

# Momentos de las grabaciones (segundos del vídeo; se ajustan mirando las hojas de fotogramas)
TT = {
    'eterna_in': 14.6,      # «¡Eternatus Eternamax! El cielo se tiñe de rojo…»
    'eterna_laser': 30.9,   # el Rayo Infinito (rayos rojos)
    'dinamax': 9.95, 'tera': 9.93, 'z': 9.95, 'mega': 10.0,   # el destello del poder
    'intro': (('v1_eterna', 74.6), ('v1_pesadilla', 25.0), ('v1_eterna', 90.8)),
    'cortes': (('v1_eterna', 31.6), ('v1_pesadilla', 41.0), ('v1_semanal', 52.0), ('v1_eterna', 91.6)),
    'final': ('v1_eterna', 29.6),
}
RED = (255, 60, 70)
PURPLE = (196, 123, 255)

def track(name, st, dur, lt, cx=200, cy=132):
    p = fuente.posicion(name, st, dur, int(lt * 60))
    return (cx, cy) if p is None else (float(p[0]), float(p[1]))

def wide(name, st, lt, dur, cw=280, z0=1.15, z1=1.0, out=(1080, 1080)):
    """La pantalla de arriba siguiendo a tu Pokémon, con un zoom que se abre."""
    f = frame(clip(name, st, dur + 0.2), lt)
    tx, ty = track(name, st, dur + 0.2, lt)
    z = z0 + (z1 - z0) * ease_io(lt / dur)
    w, h = cw / z, cw / z * out[1] / out[0]
    cx = min(400 - w / 2, max(w / 2, tx * 0.6 + 80)) if w < 400 else 200
    cy = min(240 - h / 2, max(h / 2, ty * 0.6 + 48)) if h < 240 else 120
    return zoom(top(f), cx, cy, w, h, out[0], out[1]), f

def red_tint(c, k):
    if k > 0.01: c.paste(Image.blend(c, Image.new('RGB', c.size, (140, 0, 20)), clamp(k)))

def badge(text, size=64):
    """«VERSIÓN 1.0» en una chapa con el arcoíris de la web."""
    im = chip(text, size, (255, 255, 255), (30, 20, 70))
    w, h = im.size
    out = Image.new('RGBA', (w + 16, h + 16), (0, 0, 0, 0)); d = ImageDraw.Draw(out)
    cols = [(255, 95, 109), (255, 210, 63), (95, 224, 138), (95, 200, 255), (196, 123, 255)]
    for i, col in enumerate(cols):
        x0 = i * (w + 16) / len(cols)
        d.rounded_rectangle((0, 0, w + 15, h + 15), (h + 16) // 2, fill=None)
        d.rectangle((x0, 0, x0 + (w + 16) / len(cols) + 1, h + 15), fill=col + (255,))
    m = Image.new('L', out.size, 0); ImageDraw.Draw(m).rounded_rectangle((0, 0, w + 15, h + 15), (h + 16) // 2, fill=255)
    o2 = Image.new('RGBA', out.size, (0, 0, 0, 0)); o2.paste(out, (0, 0), m)
    o2.alpha_composite(im, (8, 8))
    return o2

# ---------------------------------------------------------------- escenas
def s_intro(c, t):
    lines = ['LLEGA LA 1.0', 'LA GRAN ACTUALIZACIÓN', 'MÁS ÉPICA QUE NUNCA']
    k = int(t // B_J)
    if k < 3:
        lt = t - k * B_J
        name, st = TT['intro'][k]
        f = frame(clip(name, st, B_J + 0.2), lt)
        z = 1 + 0.07 * ease_io(lt / B_J)
        im = zoom(top(f), 200, 120, 400 / z, 240 / z, 1040, 624)
        ib = zoom(bottom(f), 160, 120, 320, 240, 832, 624)
        c.paste((4, 6, 14), (0, 0, W, H))
        br = 0.75 * clamp(lt / 0.25)
        im = ImageEnhance.Brightness(ImageEnhance.Color(im).enhance(0.7)).enhance(br)
        ib = ImageEnhance.Brightness(ib).enhance(br * 0.9)
        dx, dy = shake(t, k * B_J, 10, 0.25)
        panel(c, im, W / 2 + dx, 640 + dy, border=0)
        panel(c, ib, W / 2 + dx, 1300 + dy, border=0)
        size = 140 if k == 0 else (96 if k == 1 else 92)
        glow = [(90, 120, 255), PURPLE, RED][k]
        slam(c, T('cine', lines[k], size, glow=glow), W / 2, 640, lt, 0.18, 1.35, B_J - 0.2)
        flash(c, 0.6 * clamp(1 - lt / 0.12))
        return
    lt = t - 3 * B_J   # el logo y la chapa de la 1.0
    name, st = TT['final']
    blurred_game(c, top(frame(clip(name, st, B_J + 0.3), lt)), 0.5)
    rays(c, W / 2, 900, t, clamp(lt / 0.3))
    sparkles(c, t, 7, 60)
    slam(c, T('poke', 'PokéSurvivor', 138, arco=0.12), W / 2, 820, lt, 0.25, 1.8)
    put(c, logo3ds(320), W / 2, 1020, (1 + 0.04 * lt) * ease_out(clamp((lt - 0.2) / 0.2)))
    if lt > 0.45:
        slam(c, badge('VERSIÓN 1.0', 70), W / 2, 1200, lt - 0.45, 0.2, 1.8)
        if lt < 1.1: rainbow_burst(c, W / 2, 1200, lt - 0.45, 24, 1600)
    flash(c, 0.9 * clamp(1 - lt / 0.25))
    if lt > B_J - 0.25: flash(c, clamp((lt - (B_J - 0.25)) / 0.25))

def s_cortes(c, t):
    lt0 = t - T_DROP
    k = min(3, int(lt0 // (B_R / 2)))
    lt = lt0 - k * B_R / 2
    name, st = TT['cortes'][k]
    background(c, t)
    im, f = wide(name, st, lt, B_R / 2, 260, 1.3, 1.0)
    dx, dy = shake(t, T_DROP + k * B_R / 2, 22, 0.3)
    panel(c, im, W / 2 + dx, 960 + dy, border=8, glow=[RED, PURPLE, (255, 214, 80), RED][k])
    flash(c, 0.75 * clamp(1 - lt / 0.1))
    return chroma(c, 14 * clamp(1 - lt / 0.15))

def s_eterna(c, t):
    lt = t - r(2)
    dur = 3 * B_R
    half = 1.5 * B_R
    st = TT['eterna_in'] if lt < half else TT['eterna_laser']
    l2 = lt if lt < half else lt - half
    f = frame(clip('v1_eterna', st, half + 0.2), l2)
    c.paste((20, 0, 6), (0, 0, W, H))
    blurred_game(c, top(f), 0.3)
    red_tint(c, 0.35)
    w = 340 - 50 * ease_io(l2 / half); h = min(240, w * 1.0)
    im = zoom(top(f), 200, 120, w, h, 1080, int(1080 * h / w))
    dx, dy = shake(t, r(2) + (0 if lt < half else half), 28, 0.6)
    panel(c, im, W / 2 + dx, 1000 + dy, border=6, glow=RED)
    put(c, chip('MINUTO 20', 44, RED, (255, 255, 255)), W / 2, 230, ease_out(clamp(lt / 0.2)))
    slam(c, T('poke', 'Eternatus', 140, top=(255, 120, 140), bot=(200, 20, 60), borde=(60, 0, 30), sombra=(20, 0, 10)), W / 2 + dx, 370 + dy, lt - 0.1, 0.25, 1.8)
    slam(c, T('cine', 'ETERNAMAX', 110, glow=(255, 30, 60)), W / 2 + dx, 520 + dy, lt - 0.3, 0.2, 1.6)
    if lt > half:
        put(c, T('cine', 'TRES FASES · NÚCLEOS · RAYO ETERNAMAX', 46, color=(255, 210, 210)), W / 2, 1560, 1, clamp((lt - half) / 0.2))
        flash(c, 0.8 * clamp(1 - (lt - half) / 0.15), (255, 200, 200))
    flash(c, 0.9 * clamp(1 - lt / 0.2), (255, 220, 220))
    vignette(c, 0.8)
    return chroma(c, 16 * clamp(1 - lt / 0.3))

POWERS = [('dinamax', 'Dinamax', (255, 70, 120)), ('tera', 'Teracristal', (120, 230, 255)),
          ('z', 'Pulsera Z', (255, 214, 80)), ('mega', 'Megaevolución', PURPLE)]

def s_power(i):
    key, title, col = POWERS[i]
    def fn(c, t):
        lt = t - r(5 + i)
        pre = 0.25
        st = TT[key] - pre
        f = frame(clip('v1_' + key, st, B_R + 0.3), lt)
        px, py = track('v1_' + key, st, B_R + 0.3, lt)
        z = 1.0 + 0.15 * ease_io(lt / B_R)
        w, h = 300 / z, 240 / z
        px = min(400 - w / 2, max(w / 2, px)); py = min(240 - h / 2, max(h / 2, py - 10))
        background(c, t)
        sparkles(c, t, 31 + i, 30, (0, 400, W, 1700), (255, 255, 255))
        dx, dy = shake(t, r(5 + i) + pre, 18, 0.4)
        panel(c, zoom(top(f), px, py, w, h, 1000, 800), W / 2 + dx, 1060 + dy, border=8, glow=col)
        put(c, T('cine', 'VÉNCELO Y ELIGE TU PODER', 50, color=(230, 230, 245)), W / 2, 200, 1, clamp((t - r(5)) / 0.2))
        slam(c, T('poke', title, 120 if len(L(title)) < 12 else 104), W / 2, 380, lt - 0.05, 0.2, 1.7)
        y_button(c, W - 150, 1700, clamp(1 - abs(lt - pre) / 0.12))
        if lt > pre: rainbow_burst(c, W / 2 + dx, 1060 + dy, lt - pre, 20, 1500)
        flash(c, 0.85 * clamp(1 - abs(lt - pre) / 0.12))
    return fn

def feature(c, t, bar, title, sub, name, st, screen='top', col=(255, 214, 80), cut=None, speed=1.0, title_kw=None):
    """Una novedad en un compás: su pantalla (arriba o abajo), el título y una línea."""
    lt = t - r(bar)
    f = frame(clip(name, st, B_R + 0.3, speed=speed), lt)
    background(c, t)
    z = 1 + 0.08 * ease_io(lt / B_R)
    if screen == 'top':
        cx, cy, w, h = cut or (200, 120, 300, 240)
        im = zoom(top(f), cx, cy, w / z, h / z, 1000, int(1000 * h / w))
    else:
        cx, cy, w, h = cut or (160, 120, 320, 240)
        im = zoom(bottom(f), cx, cy, w / z, h / z, 960, int(960 * h / w))
    k = ease_out(clamp(lt / 0.25))
    panel(c, im, W / 2 + (1 - k) * 700, 1060, border=7, glow=col)
    slam(c, T('poke', title, 104 if len(L(title)) < 15 else 90, **(title_kw or {})), W / 2, 350, lt, 0.2, 1.7)
    put(c, T('cine', sub, 44), W / 2, 1620, 1, clamp((lt - 0.25) / 0.2))
    flash(c, 0.6 * clamp(1 - lt / 0.1))
    return chroma(c, 10 * clamp(1 - lt / 0.12))

def s_pasivas(c, t):
    return feature(c, t, 9, 'Pasivas a la vista', 'CADA CARTA, CON SU ICONO Y CUÁNTAS LLEVAS', 'v1_eterna', 48.5, 'bottom', (95, 224, 138), cut=(150, 72, 230, 125))
def s_tienda(c, t):
    return feature(c, t, 10, 'Tienda Kecleon', 'MT · MENTAS · HABILIDADES', 'v1_tienda', 7.6, 'top', (95, 224, 138), cut=(200, 120, 400, 240))
def s_pesadilla(c, t):
    out = feature(c, t, 11, 'Pesadilla', 'NUEVAS DIFICULTADES: DIFÍCIL Y PESADILLA', 'v1_pesadilla', 56.6, 'top', RED,
                  title_kw=dict(top=(255, 120, 140), bot=(200, 20, 60), borde=(60, 0, 30), sombra=(20, 0, 10)))
    return out
def s_semanal(c, t):
    return feature(c, t, 12, 'Desafío semanal', 'EL MISMO RETO PARA TODOS, CADA SEMANA', 'v1_semanal', 3.0, 'top', (95, 200, 255), cut=(200, 110, 400, 240))
def s_gacha(c, t):
    return feature(c, t, 13, 'Gacha ×100', 'CON NATURALEZA Y HABILIDAD EN CADA POKÉMON', 'v1_gacha', 22.4, 'bottom', PURPLE)
def s_mas(c, t):
    return feature(c, t, 14, 'Y mucho más', 'NUEVO MENÚ · MAESTRÍA · EVOLUCIONES', 'v1_tienda', 23.6, 'bottom', (255, 214, 80))

def s_final(c, t):
    lt = t - T_FIN
    name, st = TT['final']
    fx = frame(clip(name, st, T_TOTAL - T_FIN + 0.3), lt)
    background(c, t)
    sparkles(c, t, 21, 40)
    CWID, CHEI = consola_real.size()
    tq, bq = consola_real.quads()
    k = ease_io(clamp((lt - 0.6) / 2.6))
    tw = (tq[1][0] - tq[0][0] + tq[2][0] - tq[3][0]) / 2
    s_start, s_end = 1080 / tw, 980 / CWID
    sc = math.exp(math.log(s_start) * (1 - k) + math.log(s_end) * k)
    tcx, tcy = tq[:, 0].mean(), tq[:, 1].mean()
    cx0 = W / 2 - (tcx - CWID / 2) * s_start; cy0 = 960 - (tcy - CHEI / 2) * s_start
    cx = cx0 * (1 - k) + (W / 2) * k; cy = cy0 * (1 - k) + 1010 * k
    arr = np.array(c)
    consola_real.compose(arr, sc, cx, cy, np.ascontiguousarray(top(fx)), np.ascontiguousarray(bottom(fx)))
    c.paste(Image.fromarray(arr))
    l2 = lt - 3.0
    if l2 > 0:
        slam(c, T('poke', 'PokéSurvivor', 150, arco=0.12), W / 2, 250, l2, 0.25, 1.6)
        put(c, logo3ds(300), W / 2, 420, ease_out(clamp((l2 - 0.3) / 0.25)))
    lg = t - T_GOLPE
    if lg > 0:
        slam(c, badge('VERSIÓN 1.0', 64), W / 2, 1530, lg, 0.22, 1.8)
        if lg < 1.0: rainbow_burst(c, W / 2, 1530, lg, 24, 1600)
        put(c, T('cine', 'por juanluismm520', 56, color=(255, 214, 80)), W / 2, 1650, 1, clamp((lg - 0.3) / 0.3))
        put(c, T('cine', 'Proyecto de fans sin ánimo de lucro · Pokémon © Nintendo, Game Freak y The Pokémon Company', 23,
                 color=(185, 190, 215)), W / 2, 1740, 1, clamp((lg - 0.5) / 0.5))
        put(c, T('cine', 'Foto de la New 3DS XL: Antoine Turmel (CC BY 2.0) · Música y juego: PokéSurvivor', 21,
                 color=(150, 155, 185)), W / 2, 1780, 1, clamp((lg - 0.7) / 0.5))
        flash(c, 0.85 * clamp(1 - lg / 0.3))
    if t > T_TOTAL - 0.9: flash(c, (t - (T_TOTAL - 0.9)) / 0.9, (0, 0, 0))
    if lt < 0.12: flash(c, 0.7 * (1 - lt / 0.12))

SCENES = [
    (0, T_DROP, s_intro),
    (T_DROP, r(2), s_cortes),
    (r(2), r(5), s_eterna),
    (r(5), r(6), s_power(0)), (r(6), r(7), s_power(1)), (r(7), r(8), s_power(2)), (r(8), r(9), s_power(3)),
    (r(9), r(10), s_pasivas), (r(10), r(11), s_tienda), (r(11), r(12), s_pesadilla),
    (r(12), r(13), s_semanal), (r(13), r(14), s_gacha), (r(14), T_FIN, s_mas),
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
            render(float(ts)).save(OUT + 'v1_foto_%s.png' % ts); print('foto', ts)
        return
    a = float(sys.argv[1]) if len(sys.argv) > 1 else 0
    b = float(sys.argv[2]) if len(sys.argv) > 2 else T_TOTAL
    out = sys.argv[3] if len(sys.argv) > 3 else OUT + 'video_v1_sin_audio.mp4'
    enc = subprocess.Popen(['ffmpeg', '-v', 'error', '-y', '-threads', '2', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', '%dx%d' % (W, H),
                            '-r', str(FPS), '-i', '-', '-c:v', 'libx264', '-preset', 'medium', '-crf', '16', '-pix_fmt', 'yuv420p', out],
                           stdin=subprocess.PIPE)
    for n in range(int(a * FPS), int(b * FPS)):
        enc.stdin.write(render(n / FPS).tobytes())
        if n % 120 == 0: print('%.1f s' % (n / FPS), flush=True)
    enc.stdin.close(); enc.wait()
    print('hecho', out)

if __name__ == '__main__':
    main()

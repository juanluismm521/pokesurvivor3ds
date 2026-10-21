# consola.py — una New 3DS XL dibujada (azul metalizado), con las dos pantallas
# huecas para meter el juego. consola(ancho) -> (imagen RGBA, rect_arriba, rect_abajo)
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

def _grad(w, h, c0, c1, vertical=True):
    t = np.linspace(0, 1, h if vertical else w)
    a = np.array(c0, np.float32)[None] * (1 - t[:, None]) + np.array(c1, np.float32)[None] * t[:, None]
    if vertical: arr = np.repeat(a[:, None, :], w, 1)
    else: arr = np.repeat(a[None, :, :], h, 0)
    return Image.fromarray(arr.astype(np.uint8), 'RGBA' if len(c0) == 4 else 'RGB')

def _rr(size, box, r, fill=255):
    m = Image.new('L', size, 0); ImageDraw.Draw(m).rounded_rectangle(box, r, fill=fill); return m

def consola(W=1000):
    s = W / 1000.0
    S = lambda v: int(round(v * s))
    # medidas base (ancho 1000): pantalla de arriba 400x240 -> 760x456; abajo 320x240 -> 600x450
    TW, TH = 760, 456; BW_, BH_ = 600, 450
    top_h, bot_h, hinge = 620, 640, 56
    H = top_h + hinge + bot_h
    img = Image.new('RGBA', (S(1000), S(H) + S(40)), (0, 0, 0, 0))
    # sombra suave
    sh = Image.new('L', img.size, 0); d = ImageDraw.Draw(sh)
    d.rounded_rectangle((S(20), S(40), S(990), S(H) + S(30)), S(70), fill=150)
    sh = sh.filter(ImageFilter.GaussianBlur(S(30)))
    img.paste(Image.new('RGBA', img.size, (0, 0, 0, 255)), (0, 0), sh)
    navy0, navy1 = (40, 52, 104, 255), (18, 24, 56, 255)
    def shell(y0, h, flip=False):
        g = _grad(S(1000), S(h), navy1 if flip else navy0, navy0 if flip else navy1)
        m = _rr(g.size, (0, 0, S(1000) - 1, S(h) - 1), S(64))
        img.paste(g, (0, S(y0)), m)
        # brillo metalizado (motas) y borde claro
        rng = np.random.default_rng(3)
        n = (rng.random((S(h), S(1000))) * 26).astype(np.uint8)
        sp = Image.fromarray(np.dstack([n * 0 + 170, n * 0 + 180, n * 0 + 230, (n > 24).astype(np.uint8) * 255]), 'RGBA')
        img.paste(sp, (0, S(y0)), Image.fromarray((np.array(m) // 255 * (n > 24) * 38).astype(np.uint8)))
        # reflejo en diagonal
        yy, xx = np.mgrid[0:S(h), 0:S(1000)]
        band = np.clip(1 - np.abs((xx + yy * 1.4) / S(1000) - 0.55) / 0.25, 0, 1) ** 2 * 26
        gl = Image.fromarray(np.dstack([np.full_like(band, 255), np.full_like(band, 255), np.full_like(band, 255), band]).astype(np.uint8), 'RGBA')
        img.paste(gl, (0, S(y0)), Image.fromarray((np.array(m) / 255 * band).astype(np.uint8)))
        e = Image.new('L', g.size, 0); ImageDraw.Draw(e).rounded_rectangle((1, 1, S(1000) - 2, S(h) - 2), S(64), outline=255, width=max(2, S(4)))
        img.paste(Image.new('RGBA', g.size, (110, 130, 200, 255)), (0, S(y0)), e.point(lambda v: v // 2))
    shell(0, top_h)
    shell(top_h + hinge, bot_h, True)
    d = ImageDraw.Draw(img)
    # bisagra
    d.rounded_rectangle((S(40), S(top_h - 6), S(960), S(top_h + hinge + 6)), S(26), fill=(14, 18, 40, 255))
    d.rounded_rectangle((S(60), S(top_h + 6), S(940), S(top_h + hinge - 6)), S(20), fill=(30, 38, 80, 255))
    # pantalla de arriba: marco negro
    bx0, by0 = S(500 - TW / 2 - 70), S(60)
    d.rounded_rectangle((bx0, by0, S(1000) - bx0, by0 + S(TH + 80)), S(30), fill=(8, 9, 14, 255))
    top = (S(500 - TW / 2), S(100), S(500 + TW / 2), S(100 + TH))
    # altavoces y cámara
    for k in range(5):
        for side in (-1, 1):
            cx = S(500 + side * (TW / 2 + 38)); cy = S(170 + k * 26)
            d.ellipse((cx - S(4), cy - S(4), cx + S(4), cy + S(4)), fill=(25, 28, 40, 255))
    d.ellipse((S(492), S(70), S(508), S(86)), fill=(20, 24, 40, 255)); d.ellipse((S(497), S(75), S(503), S(81)), fill=(60, 80, 140, 255))
    # pantalla de abajo
    yb = top_h + hinge
    bot = (S(500 - BW_ / 2), S(yb + 70), S(500 + BW_ / 2), S(yb + 70 + BH_))
    d.rounded_rectangle((bot[0] - S(26), bot[1] - S(26), bot[2] + S(26), bot[3] + S(26)), S(22), fill=(8, 9, 14, 255))
    # circle pad
    cx, cy = S(105), S(yb + 150)
    d.ellipse((cx - S(62), cy - S(62), cx + S(62), cy + S(62)), fill=(12, 14, 26, 255))
    d.ellipse((cx - S(48), cy - S(48), cx + S(48), cy + S(48)), fill=(170, 175, 190, 255))
    d.ellipse((cx - S(40), cy - S(40), cx + S(40), cy + S(40)), fill=(200, 204, 214, 255))
    # cruceta
    cx, cy = S(105), S(yb + 330)
    for (w, h) in ((S(32), S(108)), (S(108), S(32))):
        d.rounded_rectangle((cx - w // 2, cy - h // 2, cx + w // 2, cy + h // 2), S(6), fill=(26, 32, 70, 255))
    # ABXY (New 3DS: X azul, Y verde, A rojo, B amarillo) y la palanca C
    try: f = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', S(26))
    except Exception: f = ImageFont.load_default()
    bx, by = S(895), S(yb + 300)
    for (dx, dy, col, L) in ((0, -58, (70, 110, 230), 'X'), (-58, 0, (60, 180, 90), 'Y'), (58, 0, (220, 60, 60), 'A'), (0, 58, (235, 190, 40), 'B')):
        x, y = bx + S(dx), by + S(dy)
        d.ellipse((x - S(27), y - S(27), x + S(27), y + S(27)), fill=(14, 18, 40, 255))
        d.ellipse((x - S(23), y - S(25), x + S(23), y + S(21)), fill=(30, 40, 80, 255))
        d.text((x, y - S(2)), L, font=f, fill=col + (255,), anchor='mm')
    d.ellipse((S(895) - S(14), S(yb + 175) - S(14), S(895) + S(14), S(yb + 175) + S(14)), fill=(190, 194, 205, 255))
    # START / SELECT / HOME
    for k, yy in enumerate((yb + 450, yb + 500)):
        d.rounded_rectangle((S(880), S(yy), S(930), S(yy + 22)), S(11), fill=(26, 32, 70, 255))
    d.rounded_rectangle((S(440), S(yb + 560), S(560), S(yb + 600)), S(20), fill=(26, 32, 70, 255))
    d.ellipse((S(484), S(yb + 568), S(516), S(yb + 592)), outline=(110, 130, 200, 255), width=max(1, S(3)))
    # luces
    for k, col in enumerate(((60, 160, 255), (40, 40, 40), (255, 120, 40))):
        d.ellipse((S(820 + k * 30), S(yb + 610), S(832 + k * 30), S(yb + 622)), fill=col + (255,))
    return img, top, bot

if __name__ == '__main__':
    im, t, b = consola(1000)
    bg = Image.new('RGBA', im.size, (230, 230, 240, 255)); bg.alpha_composite(im)
    d = ImageDraw.Draw(bg); d.rectangle(t, fill=(40, 120, 60)); d.rectangle(b, fill=(30, 40, 70))
    bg.convert('RGB').resize((im.width // 2, im.height // 2)).save('/tmp/consola.png')
    print(im.size, t, b)

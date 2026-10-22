# consola_real.py — la New 3DS XL de la foto (Antoine Turmel, CC BY 2.0, Wikimedia Commons),
# recortada, con el juego metido en sus pantallas con perspectiva y el reflejo del cristal encima.
import cv2, numpy as np
from PIL import Image

REAL = '/root/trailer/real/'
_d = {}

def _load():
    if _d: return _d
    im = cv2.imread(REAL + 'consola_recorte.png', cv2.IMREAD_UNCHANGED)   # BGRA, 3100x2788
    im = im[:-40]                                                       # (el borde de abajo de la foto)
    rgba = cv2.cvtColor(im, cv2.COLOR_BGRA2RGBA)
    # esquinas de las pantallas en el recorte (ver las cuentas en el README)
    top = np.float32([(635, 229), (2434, 235), (2364, 1295), (687, 1291)])
    bot = np.float32([(836, 1539), (2223, 1539), (2416, 2505), (679, 2505)])
    # reflejo del cristal de arriba: lo claro de la foto dentro de la pantalla
    L = cv2.cvtColor(im[..., :3], cv2.COLOR_BGR2GRAY).astype(np.float32)
    m = np.zeros(L.shape, np.uint8); cv2.fillPoly(m, [top.astype(np.int32)], 255)
    g = np.clip((L - 45) * 1.6, 0, 255) * (m / 255.0) * 0.38
    glare = np.dstack([np.full_like(L, 255)] * 3 + [g]).astype(np.uint8)
    _d.update(rgba=rgba, top=top, bot=bot, glare=glare)
    return _d

def size():
    d = _load(); return d['rgba'].shape[1], d['rgba'].shape[0]

def quads():
    d = _load(); return d['top'], d['bot']

def _blend(dst, src_rgba, x, y):
    """Mezcla src (RGBA uint8) sobre dst (RGB uint8) en (x, y), recortando."""
    H, W = dst.shape[:2]; h, w = src_rgba.shape[:2]
    x0, y0, x1, y1 = max(0, x), max(0, y), min(W, x + w), min(H, y + h)
    if x1 <= x0 or y1 <= y0: return
    s = src_rgba[y0 - y:y1 - y, x0 - x:x1 - x].astype(np.float32)
    a = s[..., 3:4] / 255.0
    dst[y0:y1, x0:x1] = (s[..., :3] * a + dst[y0:y1, x0:x1].astype(np.float32) * (1 - a)).astype(np.uint8)

def _screen(dst, src_rgb, quad, ox, oy, s, up=4):
    sh, sw = src_rgb.shape[:2]
    big = cv2.resize(src_rgb, (sw * up, sh * up), interpolation=cv2.INTER_NEAREST)
    q = quad * s + np.float32([ox, oy])
    x0, y0 = np.floor(q.min(0)).astype(int); x1, y1 = np.ceil(q.max(0)).astype(int)
    w, h = x1 - x0 + 1, y1 - y0 + 1
    if w < 3 or h < 3: return
    M = cv2.getPerspectiveTransform(np.float32([(0, 0), (sw * up, 0), (sw * up, sh * up), (0, sh * up)]), q - np.float32([x0, y0]))
    warped = cv2.warpPerspective(big, M, (w, h), flags=cv2.INTER_AREA if s < 0.5 else cv2.INTER_LINEAR)
    mask = np.zeros((h, w), np.uint8); cv2.fillPoly(mask, [np.round((q - np.float32([x0, y0]))).astype(np.int32)], 255)
    _blend(dst, np.dstack([warped, mask]), x0, y0)

def compose(canvas_rgb, s, cx, cy, top_rgb, bot_rgb):
    """Pinta la consola a escala s centrada en (cx, cy) sobre canvas (array RGB) con el juego."""
    d = _load()
    H0, W0 = d['rgba'].shape[:2]
    w, h = max(1, int(W0 * s)), max(1, int(H0 * s))
    con = cv2.resize(d['rgba'], (w, h), interpolation=cv2.INTER_AREA)
    gl = cv2.resize(d['glare'], (w, h), interpolation=cv2.INTER_AREA)
    ox, oy = int(round(cx - w / 2)), int(round(cy - h / 2))
    sx, sy = w / W0, h / H0
    _blend(canvas_rgb, con, ox, oy)
    _screen(canvas_rgb, top_rgb, d['top'], ox, oy, sx)
    _screen(canvas_rgb, bot_rgb, d['bot'], ox, oy, sx)
    _blend(canvas_rgb, gl, ox, oy)

if __name__ == '__main__':
    can = np.full((1920, 1080, 3), (20, 20, 50), np.uint8)
    t = np.zeros((240, 400, 3), np.uint8); t[:] = (40, 140, 60); t[::20] = 255
    b = np.zeros((240, 320, 3), np.uint8); b[:] = (30, 40, 90); b[:, ::20] = 255
    compose(can, 980 / size()[0], 540, 1010, t, b)
    Image.fromarray(can).resize((540, 960)).save('/tmp/cr.png')

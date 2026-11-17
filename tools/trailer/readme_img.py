# readme_img.py — imágenes de la 1.0 para el README (docs/img): las dos pantallas como en la consola, ×2,
import subprocess, numpy as np
from PIL import Image
RAW = '/root/trailer/raw/'; OUT = '/root/docker/pokesurvivor/juego/docs/img/'
DES = {'v1_eterna': 0.233, 'v1_dinamax': 0.167, 'v1_tera': 0.167, 'v1_z': 0.100, 'v1_mega': 0.200, 'v1_tienda': 0.033, 'v1_gacha': 0.067}
BG = (14, 22, 48)
def fr(name, t, lang):
    src = RAW + ('en_' if lang == 'en' else '') + name + '.mkv'
    if lang == 'en': t += DES.get(name, 0)
    o = subprocess.run(['ffmpeg', '-v', 'error', '-ss', '%.3f' % t, '-i', src, '-frames:v', '1', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], capture_output=True).stdout
    a = np.frombuffer(o, np.uint8).reshape(480, 400, 3).copy()
    a[240 + 216:240 + 225, 40 + 246:40 + 318] = a[240 + 226, 40 + 246]      # (fuera «60 FPS · AUTOTEST»)
    a[476:480, 40:44] = a[470, 40]                                         # (fuera el marcador del tráiler)
    return Image.fromarray(a)
def consola(im, gap=8):
    c = Image.new('RGB', (400, 480 + gap), BG)
    c.paste(im.crop((0, 0, 400, 240)), (0, 0)); c.paste(im.crop((40, 240, 360, 480)), (40, 240 + gap))
    return c
def x2(im): return im.resize((im.width * 2, im.height * 2), Image.NEAREST)
def guarda(im, n): x2(im).save(OUT + n + '.webp', quality=92, method=6); print(n)
def fila(ims, gap=12):
    c = Image.new('RGB', (sum(i.width for i in ims) + gap * (len(ims) - 1), max(i.height for i in ims)), BG); x = 0
    for i in ims: c.paste(i, (x, 0)); x += i.width + gap
    return c
for lang in ('es', 'en'):
    s = '' if lang == 'es' else '-en'
    guarda(fila([consola(fr('v1_menu', 10, lang)), consola(fr('v1_novedades', 3, lang))]), 'menu-1.0' + s)
    guarda(fila([consola(fr('v1_eterna', 19.0, lang)), consola(fr('v1_eterna', 48.5, lang))]), 'eternatus' + s)
    tops = [fr(n, t, lang).crop((0, 0, 400, 240)) for n, t in (('v1_dinamax', 10.4), ('v1_tera', 10.4), ('v1_z', 10.4), ('v1_mega', 10.5))]
    g = Image.new('RGB', (812, 492), BG)
    for i, tp in enumerate(tops): g.paste(tp, ((i % 2) * 412, (i // 2) * 252))
    guarda(g, 'poderes' + s)
    guarda(fila([consola(fr('v1_tienda', 7.6, lang)), consola(fr('v1_gacha', 22.4, lang))]), 'tienda-gacha' + s)

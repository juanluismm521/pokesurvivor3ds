# web_media.py — recursos de la web de descarga (servidor/web/media, no van en git):
# sprites andando, capturas de las dos pantallas, logo con las letras de Pokémon,
# logo 3DS, QR para FBI, piedras y el tráiler.
#   /root/docker/pokesurvivor/trailer/venv/bin/python tools/trailer/web_media.py
import json, os, shutil, subprocess, sys
import numpy as np
from PIL import Image, ImageDraw

JUEGO = '/root/docker/pokesurvivor/juego'
TR = '/root/docker/pokesurvivor/trailer'
OUT = '/root/docker/pokesurvivor/servidor/web/media'
sys.path.insert(0, TR + '/py')
from estilo import poke  # noqa: E402

os.makedirs(OUT + '/sprites', exist_ok=True)
os.makedirs(OUT + '/capturas', exist_ok=True)

# ---------------- sprites andando (cabecera) ----------------
meta = json.loads(subprocess.check_output(['node', '-e', '''
global.G={};eval(require('fs').readFileSync('%s/reference/web/js/data/sprites-meta.js','utf8'));
const o={};for(const d of [25,6,94,448,133,445,150,1,4,7,143,149]) o[d]=G.SPRITE_META[d].a.Walk;
process.stdout.write(JSON.stringify(o));''' % JUEGO]))
megas = json.load(open(JUEGO + '/assets-src/megas/megas.json'))['megas']
for i in (1, 12, 51, 62, 20, 56):          # Charizard X, Gengar, Rayquaza, Darkrai, Mewtwo Y, Lucario
    meta[str(3000 + i)] = megas[i]['sprite']['a']['Walk']
sprites = []
for k, m in meta.items():
    shutil.copyfile('%s/assets-src/pokemon/%s/Walk.png' % (JUEGO, k), '%s/sprites/%s.png' % (OUT, k))
    sprites.append({'id': int(k), 'w': m['w'], 'h': m['h'], 'n': m['n'], 'd': sum(m['d']) / 60, 'ay': m['ay']})
json.dump(sprites, open(OUT + '/sprites.json', 'w'))

# ---------------- capturas (las dos pantallas, como en la consola) ----------------
def frame(name, t):
    p = subprocess.run(['ffmpeg', '-v', 'error', '-ss', str(t), '-i', '%s/raw/%s.mkv' % (TR, name), '-frames:v', '1',
                        '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], capture_output=True)
    a = np.frombuffer(p.stdout, np.uint8).reshape(480, 400, 3).copy()
    a[240 + 216:240 + 225, 40 + 246:40 + 318] = a[240 + 226, 40 + 246]   # (etiqueta del autotest)
    a[476:480, 40:44] = a[470, 40]                                       # (marcador del tráiler)
    return a

CAPS = [('horda', 'horda', 24.0), ('mega-gengar', 'gengar_paz', 14.6), ('mega-charizard', 'charx', 21.0),
        ('sentencia', 'sentencia', 99.3), ('arceus', 'sentencia', 32.4), ('cooperativo', 'coop_A', 34.5),
        ('online', 'online_A', 19.0), ('novedades', 'novedades', 9.0), ('megapiedras', 'novedades', 16.0),
        ('mega-darkrai', 'darkrai_paz', 13.6), ('sincronizar', 'qr', 8.0), ('actualizar', 'actualizar', 26.0)]
for slug, name, t in CAPS:
    a = frame(name, t)
    top = Image.fromarray(a[:240]).resize((800, 480), Image.NEAREST)
    bot = Image.fromarray(a[240:, 40:360]).resize((640, 480), Image.NEAREST)
    im = Image.new('RGBA', (800, 480 + 24 + 480), (0, 0, 0, 0))
    im.paste(top, (0, 0)); im.paste(bot, (80, 504))
    im.save('%s/capturas/%s.webp' % (OUT, slug), lossless=True, quality=90)
    # miniatura para la galería (4 por fila): reducida con Lanczos, más legible que dejar que la encoja el navegador
    im.resize((560, round(im.height * 560 / 800)), Image.LANCZOS).save('%s/capturas/%s-mini.webp' % (OUT, slug), quality=92)
    top.save('%s/capturas/%s-arriba.webp' % (OUT, slug), lossless=True, quality=90)

# ---------------- logos, piedras, QR, tráiler ----------------
poke('PokéSurvivor', 200, arco=0.12).save(OUT + '/logo.png', optimize=True)
for t, f in (('Descargar', 'tit-descargar'), ('Novedades', 'tit-novedades'), ('Capturas', 'tit-capturas'),
             ('Cómo instalarlo', 'tit-instalar'), ('Apoya el proyecto', 'tit-apoya'), ('El tráiler', 'tit-trailer'),
             ('Qué trae', 'tit-que')):
    poke(t, 96).save('%s/%s.png' % (OUT, f), optimize=True)
for t, f in (('Download', 'tit-descargar-en'), ('What\'s new', 'tit-novedades-en'), ('Screenshots', 'tit-capturas-en'),
             ('How to install', 'tit-instalar-en'), ('Support the project', 'tit-apoya-en'), ('The trailer', 'tit-trailer-en'),
             ('Features', 'tit-que-en')):
    poke(t, 96).save('%s/%s.png' % (OUT, f), optimize=True)
shutil.copyfile(TR + '/real/logo_3ds_blanco.png', OUT + '/logo-3ds.png')

# ---------------- tarjetas para compartir (Open Graph / X, 1200x630) e icono de móvil ----------------
from PIL import ImageFilter, ImageFont  # noqa: E402
def tarjeta(lema, sub, out):
    W, H = 1200, 630
    fondo = Image.open('%s/capturas/sentencia-arriba.webp' % OUT).convert('RGB').resize((W, int(W * 0.6)), Image.LANCZOS)
    fondo = fondo.crop((0, (fondo.height - H) // 2, W, (fondo.height - H) // 2 + H)).filter(ImageFilter.GaussianBlur(6))
    c = Image.blend(fondo, Image.new('RGB', (W, H), (10, 16, 40)), 0.62).convert('RGBA')
    cap = Image.open('%s/capturas/mega-charizard-arriba.webp' % OUT).convert('RGBA').resize((560, 336), Image.LANCZOS)
    m = Image.new('L', cap.size, 0); ImageDraw.Draw(m).rounded_rectangle((0, 0, 559, 335), 18, fill=255)
    ImageDraw.Draw(c).rounded_rectangle((592, 178, 1168, 530), 24, fill=(255, 203, 5, 255))
    c.paste(cap, (600, 186), m)
    lg = Image.open(OUT + '/logo.png'); lg = lg.resize((600, int(lg.height * 600 / lg.width)), Image.LANCZOS)
    c.alpha_composite(lg, (20, 40))
    l3 = Image.open(OUT + '/logo-3ds.png'); l3 = l3.resize((200, int(l3.height * 200 / l3.width)), Image.LANCZOS)
    c.alpha_composite(l3, (220, 40 + lg.height - 10))
    d = ImageDraw.Draw(c)
    f1 = ImageFont.truetype(TR + '/fonts/Anton.ttf', 44); f2 = ImageFont.truetype(TR + '/fonts/Anton.ttf', 30)
    y = 330
    for linea in lema:
        d.text((40, y), linea, font=f1, fill=(255, 255, 255)); y += 58
    d.text((40, y + 18), sub, font=f2, fill=(255, 203, 5))
    c.convert('RGB').save(out, quality=88)
tarjeta(['Sobrevive a la horda.', 'Derrota a Arceus.'], 'GRATIS · HOMEBREW PARA NINTENDO 3DS', OUT + '/og-es.jpg')
tarjeta(['Survive the horde.', 'Defeat Arceus.'], 'FREE · NINTENDO 3DS HOMEBREW', OUT + '/og-en.jpg')
ic = Image.open(JUEGO + '/assets-src/icon-48.png').convert('RGBA')
for n in (180, 192, 512):
    b = Image.new('RGBA', (n, n), (14, 22, 48, 255)); k = int(n * 0.84)
    b.alpha_composite(ic.resize((k, k), Image.NEAREST), ((n - k) // 2, (n - k) // 2))
    b.save('%s/icono-%d.png' % (OUT, n))
os.makedirs(OUT + '/piedras', exist_ok=True)
for i in (1, 12, 51, 62, 2, 20):
    s = megas[i].get('stone_item')
    if s: shutil.copyfile('%s/assets-src/megas/piedras/%s.png' % (JUEGO, s), '%s/piedras/%d.png' % (OUT, i))

import qrcode, qrcode.image.svg  # noqa: E402
URL_CIA = 'http://pokesurvivor3ds.duckdns.org/datos/programa/ultimo.cia'
qr = qrcode.QRCode(error_correction=qrcode.constants.ERROR_CORRECT_M, border=2, box_size=10)
qr.add_data(URL_CIA); qr.make(fit=True)
qr.make_image(fill_color='#101830', back_color='white').save(OUT + '/qr-fbi.png')

shutil.copyfile(TR + '/PokeSurvivor3DS_trailer_ligero.mp4', OUT + '/trailer.mp4')
if os.path.exists(TR + '/PokeSurvivor3DS_trailer_en_ligero.mp4'):
    shutil.copyfile(TR + '/PokeSurvivor3DS_trailer_en_ligero.mp4', OUT + '/trailer-en.mp4')
subprocess.run(['ffmpeg', '-v', 'error', '-y', '-ss', '17.0', '-i', OUT + '/trailer.mp4', '-frames:v', '1', '-vf', 'scale=540:960',
                '-q:v', '3', OUT + '/trailer.jpg'], check=True)
# El tráiler de la 1.0 (trailer_v1.py), en español y en inglés, con su portada (el logo con «VERSIÓN 1.0»).
for sufijo, src in (('', 'PokeSurvivor3DS_trailer_v1_ligero.mp4'), ('-en', 'PokeSurvivor3DS_trailer_v1_en_ligero.mp4')):
    if os.path.exists(TR + '/' + src):
        shutil.copyfile(TR + '/' + src, OUT + '/trailer-1.0%s.mp4' % sufijo)
        subprocess.run(['ffmpeg', '-v', 'error', '-y', '-ss', '6.4', '-i', OUT + '/trailer-1.0%s.mp4' % sufijo, '-frames:v', '1',
                        '-vf', 'scale=540:960', '-q:v', '3', OUT + '/trailer-1.0%s.jpg' % sufijo], check=True)

# favicon: una pokéball
fv = Image.new('RGBA', (64, 64), (0, 0, 0, 0)); d = ImageDraw.Draw(fv)
d.ellipse((2, 2, 61, 61), fill=(230, 50, 60), outline=(20, 20, 30), width=4)
d.chord((2, 2, 61, 61), 0, 180, fill=(245, 245, 245), outline=(20, 20, 30), width=4)
d.line((4, 32, 59, 32), fill=(20, 20, 30), width=5); d.ellipse((22, 22, 41, 41), fill=(245, 245, 245), outline=(20, 20, 30), width=4)
fv.save(OUT + '/favicon.png')
print('recursos en', OUT, ':', len(sprites), 'sprites,', len(CAPS), 'capturas')

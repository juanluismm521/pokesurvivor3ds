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
    top.save('%s/capturas/%s-arriba.webp' % (OUT, slug), lossless=True, quality=90)

# ---------------- logos, piedras, QR, tráiler ----------------
poke('PokéSurvivor', 200, arco=0.12).save(OUT + '/logo.png', optimize=True)
for t, f in (('Descargar', 'tit-descargar'), ('Novedades', 'tit-novedades'), ('Capturas', 'tit-capturas'),
             ('Cómo instalarlo', 'tit-instalar'), ('Apoya el proyecto', 'tit-apoya'), ('El tráiler', 'tit-trailer'),
             ('Qué trae', 'tit-que')):
    poke(t, 96).save('%s/%s.png' % (OUT, f), optimize=True)
shutil.copyfile(TR + '/real/logo_3ds_blanco.png', OUT + '/logo-3ds.png')
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
subprocess.run(['ffmpeg', '-v', 'error', '-y', '-ss', '16.2', '-i', OUT + '/trailer.mp4', '-frames:v', '1', '-vf', 'scale=540:960',
                '-q:v', '3', OUT + '/trailer.jpg'], check=True)

# favicon: una pokéball
fv = Image.new('RGBA', (64, 64), (0, 0, 0, 0)); d = ImageDraw.Draw(fv)
d.ellipse((2, 2, 61, 61), fill=(230, 50, 60), outline=(20, 20, 30), width=4)
d.chord((2, 2, 61, 61), 0, 180, fill=(245, 245, 245), outline=(20, 20, 30), width=4)
d.line((4, 32, 59, 32), fill=(20, 20, 30), width=5); d.ellipse((22, 22, 41, 41), fill=(245, 245, 245), outline=(20, 20, 30), width=4)
fv.save(OUT + '/favicon.png')
print('recursos en', OUT, ':', len(sprites), 'sprites,', len(CAPS), 'capturas')

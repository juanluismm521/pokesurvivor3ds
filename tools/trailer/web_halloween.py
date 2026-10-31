# web_halloween.py — recursos de Halloween de la web de descarga (servidor/web/media/hw, no
# van en git): los disfraces de bruja y unos Fantasma andando (desfile), las capturas de
# Halloween (de trailer/raw_hw: arriba y abajo, sacadas de Azahar con los autotests
# «… halloween disfraz», «tienda halloween», «gacha100 halloween», «menu halloween» y
# «novedades 94 halloween»), el logo y los títulos en naranja y morado.
# La web sólo los enseña del 30-10 al 2-11 (web.js / generar.py: tema de Halloween).
#   /root/docker/pokesurvivor/trailer/venv/bin/python tools/trailer/web_halloween.py
import json, os, shutil, subprocess, sys
import numpy as np
from PIL import Image

JUEGO = '/root/docker/pokesurvivor/juego'
TR = '/root/docker/pokesurvivor/trailer'
OUT = '/root/docker/pokesurvivor/servidor/web/media/hw'
sys.path.insert(0, TR + '/py')
import estilo  # noqa: E402
from estilo import poke  # noqa: E402
estilo.POKE = TR + '/fonts/Pokemon Solid.ttf'   # (estilo.py aún apunta a la ruta vieja del tráiler)

os.makedirs(OUT + '/sprites', exist_ok=True)
os.makedirs(OUT + '/capturas', exist_ok=True)

# ---------------- desfile: disfraces de bruja y Fantasma ----------------
FANTASMA = [92, 93, 94, 200, 355, 425, 609, 710, 711, 778]
meta = json.loads(subprocess.check_output(['node', '-e', '''
global.G={};eval(require('fs').readFileSync('%s/reference/web/js/data/sprites-meta.js','utf8'));
const o={};for(const d of %s) if (G.SPRITE_META[d]) o[d]=G.SPRITE_META[d].a.Walk;
process.stdout.write(JSON.stringify(o));''' % (JUEGO, json.dumps(FANTASMA))]))
for sk in json.load(open(JUEGO + '/assets-src/skins/skins.json'))['skins']:
    meta[str(sk['sprite_id'])] = sk['sprite']['a']['Walk']
sprites = []
for k, m in meta.items():
    src = '%s/assets-src/pokemon/%s/Walk.png' % (JUEGO, k)
    if not os.path.exists(src):
        continue
    shutil.copyfile(src, '%s/sprites/%s.png' % (OUT, k))
    sprites.append({'id': int(k), 'w': m['w'], 'h': m['h'], 'n': m['n'], 'd': sum(m['d']) / 60, 'ay': m['ay']})
json.dump(sprites, open(OUT + '/sprites.json', 'w'))

# ---------------- capturas (las dos pantallas, como las de web_media.py) ----------------
CAPS = ['hw-horda', 'hw-gengar', 'hw-disfraz', 'hw-tienda', 'hw-gacha', 'hw-menu', 'hw-novedades']
for slug in CAPS:
    top = np.array(Image.open('%s/raw_hw/%s-top.png' % (TR, slug)).convert('RGB'))
    bot = np.array(Image.open('%s/raw_hw/%s-bottom.png' % (TR, slug)).convert('RGB'))
    bot[216:226, 246:318] = bot[226, 246]   # (la etiqueta «60 FPS · AUTOTEST»)
    t = Image.fromarray(top).resize((800, 480), Image.NEAREST)
    b = Image.fromarray(bot).resize((640, 480), Image.NEAREST)
    im = Image.new('RGBA', (800, 480 + 24 + 480), (0, 0, 0, 0))
    im.paste(t, (0, 0)); im.paste(b, (80, 504))
    im.save('%s/capturas/%s.webp' % (OUT, slug), lossless=True, quality=90)
    im.resize((560, round(im.height * 560 / 800)), Image.LANCZOS).save('%s/capturas/%s-mini.webp' % (OUT, slug), quality=92)
    t.save('%s/capturas/%s-arriba.webp' % (OUT, slug), lossless=True, quality=90)

# ---------------- logo y títulos: naranja calabaza con borde morado ----------------
NAR = dict(top=(255, 196, 92), bot=(255, 120, 20), borde=(88, 36, 140), sombra=(30, 10, 56))
poke('PokéSurvivor', 200, arco=0.12, **NAR).save(OUT + '/logo.png', optimize=True)
poke('¡Noche de Halloween!', 96, **NAR).save(OUT + '/tit-halloween.png', optimize=True)
poke('Halloween night!', 96, **NAR).save(OUT + '/tit-halloween-en.png', optimize=True)
for t, f in (('Descargar', 'tit-descargar'), ('Novedades', 'tit-novedades'), ('Capturas', 'tit-capturas'),
             ('Cómo instalarlo', 'tit-instalar'), ('Apoya el proyecto', 'tit-apoya'), ('El tráiler', 'tit-trailer'),
             ('Qué trae', 'tit-que'), ('Download', 'tit-descargar-en'), ("What's new", 'tit-novedades-en'),
             ('Screenshots', 'tit-capturas-en'), ('How to install', 'tit-instalar-en'), ('Support the project', 'tit-apoya-en'),
             ('The trailer', 'tit-trailer-en'), ('Features', 'tit-que-en')):
    poke(t, 96, **NAR).save('%s/%s.png' % (OUT, f), optimize=True)
print('Halloween en', OUT, ':', len(sprites), 'sprites,', len(CAPS), 'capturas')

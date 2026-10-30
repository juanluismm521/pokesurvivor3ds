#!/usr/bin/env python3
"""fetch_skins.py — los disfraces de bruja de Halloween (core/skins.js de la web) →
assets-src/pokemon/39xx/ (un número de sprite propio por disfraz, como las megas) y
assets-src/skins/skins.json (geometría y datos para gen_data y gen_sprites).

Las hojas salen del repo de la web (git, tools/cache/web; por defecto la rama
fuentes-28-10 = d667c216, el último commit con las fuentes): assets/pokemon/{dex}/
k-{id}/{Anim}.png (y s/ las shiny). Llevan un margen para el sombrero: PAD_X a los
lados y PAD_T arriba (tools/make_skins.py de la web), así que la geometría es la del
Pokémon con ese margen.

    python3 tools/fetch_skins.py [ref]
"""
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WEB = os.path.join(ROOT, 'tools', 'cache', 'web')
REF = sys.argv[1] if len(sys.argv) > 1 else 'fuentes-28-10'
PAD_X, PAD_T = 8, 16
BASE = 3900
ANIMS = ['Idle', 'Walk', 'Attack', 'Shoot', 'Charge', 'Hurt', 'Faint']
# (id, dex, nombre, se vende en la Tienda Kecleon)
SKINS = [('bruja-gengar', 94, 'Gengar bruja', True),
         ('bruja-pikachu', 25, 'Pikachu bruja', True),
         ('bruja-umbreon', 197, 'Umbreon bruja', True),
         ('hat', 352, 'Kecleon bruja', False)]


def git(*a):
    return subprocess.check_output(['git', '-C', WEB] + list(a))


def load_meta():
    js = ("const fs=require('fs');global.G={};"
          "eval(fs.readFileSync(process.argv[1],'utf8').replace(/\\r\\n/g,'\\n'));"
          "process.stdout.write(JSON.stringify(G.SPRITE_META))")
    return json.loads(subprocess.check_output(['node', '-e', js, os.path.join(ROOT, 'reference', 'web', 'js', 'data', 'sprites-meta.js')]))


def padded(geo):
    return {a: dict(g, w=g['w'] + 2 * PAD_X, h=g['h'] + PAD_T, ax=g['ax'] + PAD_X, ay=g['ay'] + PAD_T) for a, g in geo.items()}


def main():
    meta = load_meta()
    files = set(git('ls-tree', '-r', '--name-only', REF, '--', 'assets/pokemon').decode().split('\n'))
    out = []
    for i, (sid, dex, name, sell) in enumerate(SKINS):
        num = BASE + i
        m = meta[str(dex)]
        dst = os.path.join(ROOT, 'assets-src', 'pokemon', str(num))
        shiny = False
        for sub in ('', 's/'):
            for a in ANIMS:
                src = f'assets/pokemon/{dex}/k-{sid}/{sub}{a}.png'
                if src not in files:
                    continue
                os.makedirs(os.path.join(dst, sub), exist_ok=True)
                open(os.path.join(dst, sub, a + '.png'), 'wb').write(git('show', f'{REF}:{src}'))
                shiny = shiny or sub == 's/'
        sprite = {'s': m.get('s', 1), 'a': padded(m['a'])}
        if shiny and m.get('sh'):
            sprite['sh'] = True
            if m.get('sa'):
                sprite['sa'] = padded(m['sa'])
        out.append({'id': sid, 'dex': dex, 'name': name, 'sell': sell, 'sprite_id': num, 'sprite': sprite})
        print(f'{sid}: sprite {num}' + (' (con shiny)' if sprite.get('sh') else ''))
    json.dump({'web': REF, 'pad': [PAD_X, PAD_T], 'skins': out},
              open(os.path.join(ROOT, 'assets-src', 'skins', 'skins.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)


if __name__ == '__main__':
    main()

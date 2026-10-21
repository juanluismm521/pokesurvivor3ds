#!/usr/bin/env python3
"""fetch_megas.py — las megaevoluciones (propias del port: la web no las tiene).

  python3 tools/fetch_megas.py

Baja de PokeAPI todas las formas Mega (también las de Leyendas Z-A y su
expansión) con sus estadísticas, calculadas con LA MISMA fórmula que la web
(reference/web/tools/fetch_pokedex.py), sus tipos y el nombre de su
megapiedra; y de SpriteCollab sus sprites (con las funciones de
reference/web/tools/fetch_sprites.py: mismo formato y anclas) y sus autores.

Escribe assets-src/megas/megas.json (lo leen gen_data.js y gen_sprites.py) y
los sprites en assets-src/pokemon/<id>/ (id = 3000 + número de la mega). Las
megapiedras, cada una con su dibujo (los objetos de PokeAPI), van a
assets-src/megas/piedras/ (las mete gen_art.js en el atlas de iconos):
  python3 tools/fetch_megas.py piedras   sólo las piedras (nombres y dibujos)
Las
megas sin sprite en SpriteCollab funcionan igual (el juego usa el de la forma
normal, más grande y con aura); al volver a lanzarlo se recogen las nuevas.
"""
import importlib.util
import json
import os
import re
import sys
import urllib.request
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'assets-src', 'megas', 'megas.json')
SPR = os.path.join(ROOT, 'assets-src', 'pokemon')
API = 'https://pokeapi.co/api/v2'
SPRITE_BASE = 3000

# Las funciones de la web para bajar sprites de SpriteCollab.
spec = importlib.util.spec_from_file_location('fs', os.path.join(ROOT, 'reference', 'web', 'tools', 'fetch_sprites.py'))
fs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fs)


def get(url):
    for _ in range(4):
        try:
            req = urllib.request.Request(url, headers={'User-Agent': 'pokesurvivor3ds/1.0'})
            with urllib.request.urlopen(req, timeout=40) as r:
                return json.load(r)
        except Exception:
            pass
    return None


def mul(base):   # (fetch_pokedex.py de la web)
    return round(0.70 + base / 100 * 0.55, 2)


TYPES_ES = {'normal': 'normal', 'fire': 'fire', 'water': 'water', 'electric': 'electric', 'grass': 'grass', 'ice': 'ice',
            'fighting': 'fighting', 'poison': 'poison', 'ground': 'ground', 'flying': 'flying', 'psychic': 'psychic',
            'bug': 'bug', 'rock': 'rock', 'ghost': 'ghost', 'dragon': 'dragon', 'dark': 'dark', 'steel': 'steel', 'fairy': 'fairy'}

# Forma de PokeAPI -> nombre del subgrupo de SpriteCollab (dentro del Pokémon).
SPECIAL = {'magearna-original-mega': 'Original_Mega', 'tatsugiri-droopy-mega': 'Droopy_Mega',
           'tatsugiri-stretchy-mega': 'Stretchy_Mega', 'meowstic-female-mega': None}


def sc_name(form):
    if form in SPECIAL:
        return SPECIAL[form]
    m = re.search(r'-mega(?:-([xyz]))?$', form)
    return 'Mega' + ('_' + m.group(1).upper() if m and m.group(1) else '')


def find_sub(node, want, path):
    for k, v in node.get('subgroups', {}).items():
        if v.get('name') == want:
            return path + [k], v
        r = find_sub(v, want, path + [k])
        if r[0]:
            return r
    return None, None


def main():
    pokedex = json.loads(open(os.path.join(ROOT, 'tools', 'cache', 'dex_es.json')).read()) if os.path.exists(os.path.join(ROOT, 'tools', 'cache', 'dex_es.json')) else None
    if pokedex is None:   # nombres en español de la web (js/data/pokedex.js)
        import subprocess
        js = ("const fs=require('fs');global.G={};eval(fs.readFileSync(process.argv[1],'utf8'));"
              "process.stdout.write(JSON.stringify(Object.fromEntries(G.DEX.map(p=>[p.dex,p.name]))))")
        pokedex = json.loads(subprocess.check_output(['node', '-e', js, os.path.join(ROOT, 'reference', 'web', 'js', 'data', 'pokedex.js')]))
    pokedex = {int(k): v for k, v in pokedex.items()}
    print('PokeAPI: formas Mega…', flush=True)
    names = [x['name'] for x in get(f'{API}/pokemon?limit=3000')['results'] if '-mega' in x['name']]
    tracker = json.loads(fs.fetch(fs.RAW + '/tracker.json').decode('utf-8'))
    credit_names = {}
    raw = fs.fetch(fs.RAW + '/credit_names.txt')
    if raw:
        for line in raw.decode('utf-8', 'replace').splitlines()[1:]:
            parts = line.split('\t')
            if len(parts) >= 2:
                credit_names[parts[1].strip()] = parts[0].strip()

    def one(form):
        p = get(f'{API}/pokemon/{form}')
        if not p:
            return None
        sp = get(p['species']['url'])
        dex = sp['id']
        b = {st['stat']['name']: int(st['base_stat']) for st in p['stats']}
        types = [TYPES_ES[t['type']['name']] for t in sorted(p['types'], key=lambda t: t['slot'])]
        suf = re.search(r'-mega(?:-([xyz]))?$', form)
        letter = suf.group(1).upper() if suf and suf.group(1) else ''
        stone = None
        stone_id = form.split('-')[0] + 'ite' + ('-' + letter.lower() if letter else '')
        it = get(f'{API}/item/{stone_id}')
        if it:
            stone = next((n['name'] for n in it.get('names', []) if n['language']['name'] == 'es'), None)
        return {'form': form, 'dex': dex, 'letter': letter, 'types': types,
                'hp': round(70 + b['hp'] * 0.45), 'spd': round(78 + b['speed'] * 0.42),
                'atk': mul(max(b['attack'], b['special-attack'])), 'pa': mul(b['attack']), 'sa': mul(b['special-attack']),
                'df': b['defense'], 'sd': b['special-defense'], 'bst': sum(b.values()), 'stone': stone}

    with ThreadPoolExecutor(max_workers=8) as ex:
        rows = [r for r in ex.map(one, names) if r]
    # Orden fijo (por Pokédex y forma) y como mucho dos megas por Pokémon (A y B).
    rows.sort(key=lambda r: (r['dex'], r['form']))
    out, per = [], {}
    for r in rows:
        if r['dex'] not in pokedex:
            continue   # (no está en el juego)
        k = per.get(r['dex'], 0)
        # Como mucho dos, y la segunda sólo si es otra de verdad (X/Y/Z): las
        # de Meowstic macho/hembra, Magearna original o los Tatsugiri son iguales.
        if k >= 2 or (k == 1 and not r['letter']):
            continue
        per[r['dex']] = k + 1
        r['slot'] = k
        base = pokedex[r['dex']]
        r['name'] = f"Mega-{base}" + (f" {r['letter']}" if r['letter'] else '')
        if not r['stone']:
            r['stone'] = f"Megapiedra de {base}" + (f" {r['letter']}" if r['letter'] else '')
        r['id'] = SPRITE_BASE + len(out)
        out.append(r)
    # Sprites de SpriteCollab (los que tengan Idle y Walk como mínimo).
    print(f'{len(out)} megas · sprites de SpriteCollab…', flush=True)

    def sprite(r):
        node = tracker.get(f"{r['dex']:04d}")
        want = sc_name(r['form'])
        if not node or not want:
            return r, None
        path, sub = find_sub(node, want, [f"{r['dex']:04d}"])
        if not sub or not {'Idle', 'Walk'} <= set(sub.get('sprite_files', {})):
            return r, None
        url = fs.BASE + '/' + '/'.join(path)
        xml = fs.fetch(url + '/AnimData.xml')
        if not xml:
            return r, None
        anims, shadow = fs.parse_animdata(xml.decode('utf-8', 'replace'))
        meta = fs.download_set(url, os.path.join(SPR, str(r['id'])), anims)
        if not {'Idle', 'Walk'} <= set(meta):
            return r, None
        out_m = {'s': shadow, 'a': meta}
        # Su shiny (en SpriteCollab cuelga de la forma: …/<forma>/0001 «Shiny»).
        sh = sub.get('subgroups', {}).get('0001', {})
        if sh.get('name') == 'Shiny' and {'Idle', 'Walk'} <= set(sh.get('sprite_files', {})):
            sxml = fs.fetch(url + '/0001/AnimData.xml')
            sanims = fs.parse_animdata(sxml.decode('utf-8', 'replace'))[0] if sxml else anims
            shiny = fs.download_set(url + '/0001', os.path.join(SPR, str(r['id']), 's'), sanims, meta)
            if {'Idle', 'Walk'} <= set(shiny) and set(meta) <= set(shiny):
                out_m['sh'] = 1
                diff = {k: v for k, v in shiny.items() if meta.get(k) != v}
                if diff:
                    out_m['sa'] = diff
        cr = sub.get('sprite_credit', {})
        who = [cr.get('primary', '')] + list(cr.get('secondary', []))
        authors = [credit_names.get(w, w) for w in who if w]
        out_m.update({'path': '/'.join(path), 'autores': authors})
        return r, out_m

    with ThreadPoolExecutor(max_workers=8) as ex:
        for r, m in ex.map(sprite, out):
            if m:
                r['sprite'] = m
    piedras(out)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w', encoding='utf-8') as f:
        json.dump({'fuente': 'PokeAPI (estadísticas con la fórmula de la web) + SpriteCollab (CC BY-NC 4.0)', 'megas': out},
                  f, ensure_ascii=False, indent=1)
    con = sum(1 for r in out if 'sprite' in r)
    print(f'megas.json: {len(out)} megas ({con} con sprite propio)')
    creditos(out)


def creditos(megas):
    """La sección de CREDITS.md con los autores de los sprites de las megas."""
    path = os.path.join(ROOT, 'CREDITS.md')
    txt = open(path, encoding='utf-8').read()
    ini, fin = '<!-- megas -->', '<!-- /megas -->'
    filas = [f"| {m['name']} | {', '.join(m['sprite']['autores']) or '—'} |" for m in megas if 'sprite' in m]
    sec = (ini + '\n## Sprites de las megaevoluciones\n\nDe [SpriteCollab](https://github.com/PMDCollab/SpriteCollab) '
           '(CC BY-NC 4.0), por sus autores:\n\n| Mega | Autores |\n|---|---|\n' + '\n'.join(filas) +
           '\n\nLas megas sin sprite en SpriteCollab usan el de su forma normal. Las megapiedras: dibujos de los objetos de\n[PokeAPI](https://github.com/PokeAPI/sprites) (de los juegos, © Nintendo / Game Freak).\n' + fin)
    if ini in txt:
        txt = txt[:txt.index(ini)] + sec + txt[txt.index(fin) + len(fin):]
    else:
        txt = txt.replace('## Datos', sec + '\n\n## Datos', 1)
    open(path, 'w', encoding='utf-8').write(txt)


PIEDRAS = os.path.join(ROOT, 'assets-src', 'megas', 'piedras')


def piedras(megas=None):
    """Cada megapiedra con su nombre en español y su dibujo (categoría
    «mega-stones» de PokeAPI). Rayquaza no tiene: en los juegos megaevoluciona
    con Ascenso del Dragón; aquí, con el Meteorito."""
    from PIL import Image
    import io
    guardar = megas is None
    if megas is None:
        megas = json.load(open(OUT, encoding='utf-8'))
    lista = megas['megas'] if isinstance(megas, dict) else megas
    items = [i['name'] for i in get(f'{API}/item-category/mega-stones')['items']]
    with ThreadPoolExecutor(max_workers=8) as ex:
        info = {n: d for n, d in zip(items, ex.map(lambda n: get(f'{API}/item/{n}'), items)) if d}

    def partes(n):
        m = re.match(r'(.*?)(?:-([xyz]))?$', n)
        return m.group(1), m.group(2) or ''

    def comun(a, b):
        i = 0
        while i < min(len(a), len(b)) and a[i] == b[i]:
            i += 1
        return i
    os.makedirs(PIEDRAS, exist_ok=True)
    for r in lista:
        especie = r['form'].split('-mega')[0].split('-')[0]
        letra = r['letter'].lower()
        mejor = max(((comun(especie, partes(n)[0]), n) for n in info if partes(n)[1] == letra), default=(0, None))
        item = mejor[1] if mejor[0] >= 4 else None
        if especie == 'rayquaza':
            item = 'meteorite'
            info[item] = get(f'{API}/item/{item}')
        if not item or not info.get(item):
            print('sin piedra:', r['name'])
            continue
        d = info[item]
        es = next((n['name'] for n in d.get('names', []) if n['language']['name'] == 'es'), None)
        if es:
            r['stone'] = es
        url = (d.get('sprites') or {}).get('default')
        if not url:
            continue
        req = urllib.request.Request(url, headers={'User-Agent': 'pokesurvivor3ds/1.0'})
        img = Image.open(io.BytesIO(urllib.request.urlopen(req, timeout=40).read())).convert('RGBA')
        img = img.crop(img.getbbox())   # (sin el borde transparente)
        # Todas a 28×28: las pixeladas (14 px) a tamaño entero, las grandes suavizadas.
        T = 28
        if max(img.size) <= T // 2:
            k = T // max(img.size)
            img = img.resize((img.width * k, img.height * k), Image.NEAREST)
        elif max(img.size) > T:
            k = T / max(img.size)
            img = img.resize((max(1, round(img.width * k)), max(1, round(img.height * k))), Image.LANCZOS)
        lienzo = Image.new('RGBA', (T, T), (0, 0, 0, 0))
        lienzo.alpha_composite(img, ((T - img.width) // 2, (T - img.height) // 2))
        lienzo.save(os.path.join(PIEDRAS, item + '.png'))
        r['stone_item'] = item
    if guardar:
        with open(OUT, 'w', encoding='utf-8') as f:
            json.dump(megas, f, ensure_ascii=False, indent=1)
    print(f"piedras: {sum(1 for r in lista if r.get('stone_item'))} de {len(lista)} con dibujo")


if __name__ == '__main__':
    if sys.argv[1:] == ['piedras']:
        piedras()
    else:
        main()

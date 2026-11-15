#!/usr/bin/env python3
"""fetch_gran.py — sprites que trajo la Gran Actualización de la web (13-11-2026).

    python3 tools/fetch_gran.py [--web reference/web]

Baja de pokesurvivor.com (assets/pokemon/<id>/[s/]<Anim>.png, con permiso de su autor;
son los de PMD SpriteCollab y los de su comunidad, CC BY-NC) lo que al port le falta:
  - los Pokémon que no estaban (59: Miraidon, Kingambit…)        → assets-src/pokemon/<dex>
  - las formas regionales (Vulpix de Alola…): en la web son 500037…; aquí llevan un
    número de Pokédex propio a partir de 1026, que NO cambia nunca (los guardados lo
    usan): la tabla está en assets-src/formas/formas.json y sólo se le añade al final.
  - las megas que SpriteCollab no tiene (49 de las del port)      → assets-src/pokemon/<3000+i>
    y su geometría en megas.json («sprite», con «web» = id de la web).
No vuelve a bajar lo que ya está.
"""
import argparse, json, os, subprocess, sys, time, urllib.request
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'assets-src', 'pokemon')
BASE = 'https://pokesurvivor.com/assets/pokemon/'
ANIMS = ['Idle', 'Walk', 'Attack', 'Shoot', 'Charge', 'Hurt', 'Faint']


def get(url, dst):
    if os.path.exists(dst) and os.path.getsize(dst) > 0:
        return True
    for i in range(4):
        try:
            req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
            data = urllib.request.urlopen(req, timeout=30).read()
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            with open(dst + '.tmp', 'wb') as f:
                f.write(data)
            os.replace(dst + '.tmp', dst)
            return True
        except Exception as e:  # noqa
            err = e
            time.sleep(1 + i * 2)
    print('  no se pudo bajar', url, err)
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--web', default=os.path.join(ROOT, 'reference', 'web'))
    a = ap.parse_args()
    T = json.loads(subprocess.check_output(['node', os.path.join(ROOT, 'tools', 'web', 'volcar.js'), a.web], maxbuffer=None) if False else
                   subprocess.check_output(['node', os.path.join(ROOT, 'tools', 'web', 'volcar.js'), a.web]))
    meta = T['meta']
    jobs = []   # (id web, carpeta)

    # Formas regionales: número propio, estable
    ff = os.path.join(ROOT, 'assets-src', 'formas', 'formas.json')
    formas = json.load(open(ff))['formas'] if os.path.exists(ff) else []
    by_id = {f['id']: f for f in formas}
    nxt = max([f['dex'] for f in formas] + [1025]) + 1
    for p in T['dex']:
        if p['dex'] <= 1025:
            if not os.path.isdir(os.path.join(SRC, str(p['dex']))):
                jobs.append((p['dex'], str(p['dex'])))
            continue
        f = by_id.get(p['dex'])
        if not f:
            f = {'id': p['dex'], 'dex': nxt, 'num': p['num'], 'slug': p['slug'], 'name': p['name'], 'reg': p.get('reg', '')}
            nxt += 1
            formas.append(f)
            by_id[f['id']] = f
        f.update(num=p['num'], slug=p['slug'], name=p['name'], reg=p.get('reg', ''))
        jobs.append((p['dex'], str(f['dex'])))
    os.makedirs(os.path.dirname(ff), exist_ok=True)
    json.dump({'_': 'Formas regionales: id de la web → número de Pokédex del port (estable; sólo se añade al final). tools/fetch_gran.py',
               'formas': formas}, open(ff, 'w'), ensure_ascii=False, indent=1)

    # Megas sin sprite en SpriteCollab
    mf = os.path.join(ROOT, 'assets-src', 'megas', 'megas.json')
    MJ = json.load(open(mf))
    for m in MJ['megas']:
        w = T['megas'].get(m['form'])
        if not w or (m.get('sprite') and not m.get('web')):
            continue
        mm = meta.get(str(w['id']))
        if not mm:
            continue
        m['web'] = w['id']
        m['sprite'] = {'s': mm.get('s', 1), 'sh': bool(mm.get('sh')), 'a': mm['a'], **({'sa': mm['sa']} if mm.get('sa') else {})}
        jobs.append((w['id'], str(m['id'])))
    json.dump(MJ, open(mf, 'w'), ensure_ascii=False, indent=1)

    # Sprites especiales (del port: 3800 + i, assets-src/extra/extra.json): Eternatus Eternamax…
    EXTRA = [(200890, 'eternamax', 'Eternatus Eternamax')]
    xf = os.path.join(ROOT, 'assets-src', 'extra', 'extra.json')
    extra = []
    for i, (wid, slug, name) in enumerate(EXTRA):
        mm = meta.get(str(wid))
        if not mm:
            continue
        extra.append({'id': 3800 + i, 'web': wid, 'slug': slug, 'name': name,
                      'sprite': {'s': mm.get('s', 1), 'sh': bool(mm.get('sh')), 'a': mm['a'], **({'sa': mm['sa']} if mm.get('sa') else {})}})
        jobs.append((wid, str(3800 + i)))
    os.makedirs(os.path.dirname(xf), exist_ok=True)
    json.dump({'_': 'Sprites especiales de la web (3800 + i): tools/fetch_gran.py', 'extra': extra}, open(xf, 'w'), ensure_ascii=False, indent=1)

    urls = []
    for wid, folder in jobs:
        mm = meta.get(str(wid))
        if not mm:
            print('sin SPRITE_META:', wid)
            continue
        for an in ANIMS:
            if an in mm['a']:
                urls.append((f'{BASE}{wid}/{an}.png', os.path.join(SRC, folder, an + '.png')))
                if mm.get('sh'):
                    urls.append((f'{BASE}{wid}/s/{an}.png', os.path.join(SRC, folder, 's', an + '.png')))
    print(f'{len(jobs)} sprites, {len(urls)} hojas')
    with ThreadPoolExecutor(4) as ex:
        ok = list(ex.map(lambda u: get(*u), urls))
    print('bajadas:', sum(ok), 'fallos:', len(ok) - sum(ok))
    return 0 if all(ok) else 1


if __name__ == '__main__':
    sys.exit(main())

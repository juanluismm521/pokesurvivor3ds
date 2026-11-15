#!/usr/bin/env python3
"""fetch_evos.py — a qué nivel evoluciona cada Pokémon en los juegos (PokeAPI).

    python3 tools/fetch_evos.py

Escribe assets-src/evos/niveles.json: {"niveles": {"<dex al que evoluciona>": nivel}}.
Sólo las evoluciones por nivel (Charmander → Charmeleon: 16). Las de piedra, intercambio,
amistad… no tienen: el juego les pone el suyo (core/evo.cpp). Lo usa tools/gen_data.js
(kEvos). Guarda lo bajado en tools/cache/pokeapi/ para no repetir.
"""
import json, os, sys, time, urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CACHE = os.path.join(ROOT, 'tools', 'cache', 'pokeapi', 'evolution-chain')
OUT = os.path.join(ROOT, 'assets-src', 'evos', 'niveles.json')


def get(i):
    f = os.path.join(CACHE, f'{i}.json')
    if os.path.exists(f):
        t = open(f).read()
        return json.loads(t) if t.strip() else None
    os.makedirs(CACHE, exist_ok=True)
    for k in range(4):
        try:
            req = urllib.request.Request(f'https://pokeapi.co/api/v2/evolution-chain/{i}/', headers={'User-Agent': 'Mozilla/5.0'})
            t = urllib.request.urlopen(req, timeout=30).read().decode()
            open(f, 'w').write(t)
            return json.loads(t)
        except urllib.error.HTTPError as e:
            if e.code == 404:
                open(f, 'w').write('')
                return None
            time.sleep(2 + k * 2)
        except Exception:
            time.sleep(2 + k * 2)
    return None


def dex_of(sp):
    return int(sp['url'].rstrip('/').split('/')[-1])


def walk(node, out):
    for nxt in node.get('evolves_to', []):
        lv = [d.get('min_level') for d in nxt.get('evolution_details', []) if d.get('trigger', {}).get('name') == 'level-up' and d.get('min_level')]
        d = dex_of(nxt['species'])
        if lv and d <= 1025:
            out[str(d)] = min(lv)
        walk(nxt, out)


def main():
    out = {}
    miss = 0
    for i in range(1, 560):
        c = get(i)
        if not c:
            miss += 1
            continue
        walk(c['chain'], out)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    json.dump({'_': 'Nivel al que se evoluciona A cada Pokémon en los juegos (PokeAPI, sólo por nivel). tools/fetch_evos.py',
               'niveles': dict(sorted(out.items(), key=lambda kv: int(kv[0])))}, open(OUT, 'w'), indent=0)
    print(f'{len(out)} evoluciones por nivel ({miss} cadenas que no existen)')


if __name__ == '__main__':
    sys.exit(main())

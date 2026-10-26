#!/usr/bin/env python3
"""megas_derivadas.py — sprites de megas que SpriteCollab aún no tiene, hechos
en el port a partir de otro de SpriteCollab (CC BY-NC 4.0: se permiten obras
derivadas con su crédito). Hoy: Mega-Lucario Z, recoloreado desde Mega-Lucario
con la paleta de la forma Z de Leyendas Z-A (cian, amarillo claro y negro en vez
de rojo y negro). Es una aproximación: la forma Z de verdad cambia también la
silueta. Cuando SpriteCollab publique la suya, fetch_megas.py la usará en vez de
esta (sólo se hace si la mega no tiene sprite propio).

  python3 tools/megas_derivadas.py      (lo llama también fetch_megas.py)
"""
import json
import os
import shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MEGAS = os.path.join(ROOT, 'assets-src', 'megas', 'megas.json')
SPR = os.path.join(ROOT, 'assets-src', 'pokemon')

# forma → (forma de la que sale, cambios de color RGB, nota para los créditos)
DERIVADAS = {
    'lucario-mega-z': ('lucario-mega', {
        (0xe9, 0x30, 0x02): (0x3c, 0xe6, 0xf0), (0x9f, 0x2f, 0x00): (0x1a, 0x8f, 0xb4),   # rojo → cian
        (0xff, 0xd7, 0x87): (0xfb, 0xf3, 0xa8), (0xc7, 0x8f, 0x4f): (0xd8, 0xc6, 0x5e),   # crema → amarillo claro
        (0xff, 0xf7, 0xaf): (0xff, 0xff, 0xd8),
        (0x50, 0x7f, 0x9e): (0x2e, 0x5c, 0x8c), (0x6d, 0xb4, 0xe0): (0x4f, 0x9c, 0xe0),   # azul, algo más oscuro
    }, 'recoloreado en el port desde el de Mega-Lucario'),
}


def recolor(src, dst, cambios):
    from PIL import Image
    im = Image.open(src).convert('RGBA')
    px = im.load()
    for y in range(im.size[1]):
        for x in range(im.size[0]):
            r, g, b, a = px[x, y]
            if a and (r, g, b) in cambios:
                px[x, y] = cambios[(r, g, b)] + (a,)
    im.save(dst)


def aplicar(datos=None):
    guardar = datos is None
    if datos is None:
        datos = json.load(open(MEGAS, encoding='utf-8'))
    por_forma = {m['form']: m for m in datos['megas']}
    hechas = 0
    for forma, (origen, cambios, nota) in DERIVADAS.items():
        m, o = por_forma.get(forma), por_forma.get(origen)
        if not m or not o or 'sprite' not in o:
            continue
        if 'sprite' in m and not m['sprite'].get('derivado'):
            continue   # (ya tiene uno de SpriteCollab)
        dst = os.path.join(SPR, str(m['id']))
        shutil.rmtree(dst, ignore_errors=True)
        os.makedirs(dst)
        src = os.path.join(SPR, str(o['id']))
        for f in sorted(os.listdir(src)):
            if f.endswith('.png'):
                recolor(os.path.join(src, f), os.path.join(dst, f), cambios)
        sp = {k: v for k, v in o['sprite'].items() if k not in ('sh', 'sa')}   # (sin shiny: el shiny usa el suyo)
        sp['autores'] = list(o['sprite'].get('autores', [])) + [nota]
        sp['derivado'] = origen
        m['sprite'] = sp
        hechas += 1
    if guardar:
        with open(MEGAS, 'w', encoding='utf-8') as f:
            json.dump(datos, f, ensure_ascii=False, indent=1)
    print(f'megas derivadas: {hechas}')
    return datos


if __name__ == '__main__':
    d = aplicar()
    import importlib.util
    spec = importlib.util.spec_from_file_location('fm', os.path.join(ROOT, 'tools', 'fetch_megas.py'))
    fm = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(fm)
    fm.creditos(d['megas'])

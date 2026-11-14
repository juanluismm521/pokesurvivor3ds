#!/usr/bin/env python3
"""gen_sprites.py — hojas de sprites de assets-src/pokemon → formato de la 3DS.

    python3 tools/gen_sprites.py [--out romfs/spr] [--only 1,4,25] [--jobs 2]

Por cada Pokémon y variante (normal y shiny):
  - recorta cada fotograma a su contenido y deduplica las imágenes iguales,
    también en espejo (Mundo Misterioso reutiliza muchísimo las poses),
  - empaqueta las imágenes únicas en un atlas (potencia de 2, ≤1024) y lo
    convierte con tex3ds a .t3x (RGBA5551 comprimido: el alfa es 0 o 255),
  - escribe la tabla de fotogramas (.fr): por animación, fila y fotograma,
    qué imagen va, si va en espejo y dónde cae dentro del fotograma.

La geometría (tamaño de fotograma, filas) sale de SPRITE_META (sprites-meta.js);
la del shiny, de `sa` cuando difiere. Al final escribe un informe del tamaño.

Formato .fr (little-endian):
  'PSF1' · u8 npages · u8 0 · u16 nimages
  por página: u16 ancho, u16 alto (de la textura)
  por imagen: u8 página, u8 0, u16 x, u16 y, u16 w, u16 h
  por animación (7, orden de kAnimName): u8 n, u8 r, y r*n fotogramas:
      u16 imagen (0xFFFF = vacío), u8 espejo, u8 0, i16 ox, i16 oy
"""
import argparse
import hashlib
import json
import os
import struct
import subprocess
import sys
from multiprocessing import Pool

from PIL import Image, ImageOps

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'assets-src', 'pokemon')
ANIMS = ['Idle', 'Walk', 'Attack', 'Shoot', 'Charge', 'Hurt', 'Faint']
TEX3DS = os.path.join(os.environ.get('DEVKITPRO', '/opt/devkitpro'), 'tools', 'bin', 'tex3ds')


def load_meta():
    """SPRITE_META de la web nueva (reference/web-gran, tools/web/volcar.js): los 1025 y las
    formas regionales, éstas con su número del port (assets-src/formas/formas.json)."""
    gran = os.path.join(ROOT, 'reference', 'web-gran')
    T = json.loads(subprocess.check_output(['node', os.path.join(ROOT, 'tools', 'web', 'volcar.js'), gran]))
    ff = os.path.join(ROOT, 'assets-src', 'formas', 'formas.json')
    por_id = {f['id']: f['dex'] for f in json.load(open(ff, encoding='utf-8'))['formas']} if os.path.exists(ff) else {}
    meta = {}
    for p in T['dex']:
        d = p['dex']
        m = T['meta'].get(str(d))
        if not m:
            continue
        meta[str(por_id.get(d, d) if d > 1025 else d)] = m
    return meta


def pack(sizes):
    """Empaqueta rectángulos (w, h) en estantes. Devuelve (W, H, posiciones) o None."""
    order = sorted(range(len(sizes)), key=lambda i: (-sizes[i][1], -sizes[i][0]))
    best = None
    for W in (64, 128, 256, 512, 1024):
        if any(w > W for w, h in sizes):
            continue
        pos = [None] * len(sizes)
        x = y = shelf = 0
        for i in order:
            w, h = sizes[i]
            if x + w > W:
                x, y, shelf = 0, y + shelf, 0
            pos[i] = (x, y)
            x += w
            shelf = max(shelf, h)
        used = y + shelf
        H = 8
        while H < used:
            H *= 2
        if H > 1024:
            continue
        if best is None or W * H < best[0] * best[1] or (W * H == best[0] * best[1] and max(W, H) < max(best[0], best[1])):
            best = (W, H, pos)
    return best


def split_pages(images):
    """Reparte las imágenes en páginas de ≤1024×1024 (casi siempre una)."""
    pages, cur = [], []
    for i in range(len(images)):
        trial = cur + [i]
        if pack([images[k].size for k in trial]) is None:
            pages.append(cur)
            cur = [i]
        else:
            cur = trial
    if cur:
        pages.append(cur)
    return pages


def build(dex, meta, shiny, out_dir, tmp_dir):
    d = str(dex)
    m = meta[d]
    folder = os.path.join(SRC, d, 's') if shiny else os.path.join(SRC, d)
    if shiny and not m.get('sh'):
        return None
    geo = dict(m['a'])
    if shiny and m.get('sa'):
        geo.update(m['sa'])
    images, keys = [], {}
    anims = []
    for a in ANIMS:
        am = geo.get(a)
        path = os.path.join(folder, a + '.png')
        if not am or not os.path.exists(path):
            anims.append((0, 0, []))
            continue
        sheet = Image.open(path).convert('RGBA')
        w, h, n, r = am['w'], am['h'], am['n'], am['r']
        frames = []
        for row in range(r):
            for f in range(n):
                c = sheet.crop((f * w, row * h, f * w + w, row * h + h))
                bb = c.getbbox()
                if not bb:
                    frames.append((0xFFFF, 0, 0, 0))
                    continue
                img = c.crop(bb)
                k = hashlib.sha1(img.tobytes() + bytes(str(img.size), 'ascii')).digest()
                km = hashlib.sha1(ImageOps.mirror(img).tobytes() + bytes(str(img.size), 'ascii')).digest()
                if k in keys:
                    idx, flip = keys[k], 0
                elif km in keys:
                    idx, flip = keys[km], 1
                else:
                    idx, flip = len(images), 0
                    keys[k] = idx
                    images.append(img)
                frames.append((idx, flip, bb[0], bb[1]))
        anims.append((n, r, frames))
    if not images:
        return None

    name = d + ('s' if shiny else '')
    pages = split_pages(images)
    rects = [None] * len(images)
    page_dims = []
    raw = 0
    for p, idxs in enumerate(pages):
        W, H, pos = pack([images[k].size for k in idxs])
        atlas = Image.new('RGBA', (W, H), (0, 0, 0, 0))
        for j, k in enumerate(idxs):
            atlas.paste(images[k], pos[j])
            rects[k] = (p, pos[j][0], pos[j][1], images[k].size[0], images[k].size[1])
        png = os.path.join(tmp_dir, f'{name}_{p}.png')
        atlas.save(png)
        t3x = os.path.join(out_dir, f'{name}_{p}.t3x')
        subprocess.check_call([TEX3DS, '-f', 'rgba5551', '-z', 'auto', '-o', t3x, png], stdout=subprocess.DEVNULL)
        os.remove(png)
        page_dims.append((W, H))
        raw += W * H * 2

    buf = bytearray(b'PSF1')
    buf += struct.pack('<BBH', len(pages), 0, len(images))
    for W, H in page_dims:
        buf += struct.pack('<HH', W, H)
    for p, x, y, w, h in rects:
        buf += struct.pack('<BBHHHH', p, 0, x, y, w, h)
    for n, r, frames in anims:
        buf += struct.pack('<BB', n, r)
        for idx, flip, ox, oy in frames:
            buf += struct.pack('<HBBhh', idx, flip, 0, ox, oy)
    with open(os.path.join(out_dir, name + '.fr'), 'wb') as f:
        f.write(buf)
    disk = sum(os.path.getsize(os.path.join(out_dir, f'{name}_{p}.t3x')) for p in range(len(pages))) + len(buf)
    return (dex, shiny, len(images), raw, disk, len(pages))


def job(args):
    dex, meta, out_dir, tmp_dir = args
    res = []
    for shiny in (False, True):
        r = build(dex, meta, shiny, out_dir, tmp_dir)
        if r:
            res.append(r)
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default=os.path.join(ROOT, 'romfs', 'spr'))
    ap.add_argument('--only', default='')
    ap.add_argument('--jobs', type=int, default=os.cpu_count() or 2)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    tmp = os.path.join(ROOT, 'build', 'spr_tmp')
    os.makedirs(tmp, exist_ok=True)
    meta = load_meta()
    # Megaevoluciones (del port): sus sprites, con el número 3000 + i (megas.json).
    mf = os.path.join(ROOT, 'assets-src', 'megas', 'megas.json')
    if os.path.exists(mf):
        for m in json.load(open(mf, encoding='utf-8'))['megas']:
            if 'sprite' in m:
                meta[str(m['id'])] = {k: m['sprite'][k] for k in ('s', 'a', 'sh', 'sa') if k in m['sprite']}
    # Disfraces de Halloween (del port): 3900 + i (skins.json, tools/fetch_skins.py).
    sf = os.path.join(ROOT, 'assets-src', 'skins', 'skins.json')
    if os.path.exists(sf):
        for k in json.load(open(sf, encoding='utf-8'))['skins']:
            meta[str(k['sprite_id'])] = {x: k['sprite'][x] for x in ('s', 'a', 'sh', 'sa') if x in k['sprite']}
    dexes = sorted(int(d) for d in meta) if not a.only else [int(x) for x in a.only.split(',')]
    with Pool(a.jobs) as pool:
        results = [r for rs in pool.imap_unordered(job, [(d, meta, a.out, tmp) for d in dexes]) for r in rs]
    raw = sum(r[3] for r in results)
    disk = sum(r[4] for r in results)
    normal = [r for r in results if not r[1]]
    heavy = max(results, key=lambda r: r[3])
    multi = [r for r in results if r[5] > 1]
    report = {
        'especies': len(normal), 'variantes': len(results),
        'memoria_MB': round(raw / 1e6, 1), 'disco_MB': round(disk / 1e6, 1),
        'mas_pesado': {'dex': heavy[0], 'shiny': heavy[1], 'imagenes': heavy[2], 'KB': round(heavy[3] / 1024)},
        'mediana_KB': round(sorted(r[3] for r in normal)[len(normal) // 2] / 1024) if normal else 0,
        'varias_paginas': len(multi),
    }
    with open(os.path.join(a.out, 'report.json'), 'w') as f:
        json.dump(report, f, indent=1)
    print('gen_sprites:', json.dumps(report, ensure_ascii=False))


if __name__ == '__main__':
    sys.exit(main())

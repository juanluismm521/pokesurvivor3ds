#!/usr/bin/env python3
"""gen_credits.py — CREDITS.md → romfs/credits.txt (los créditos que salen en el
juego). Una línea por renglón; las que empiezan por '#' son títulos.
De la tabla de artistas se queda el nombre y cuántos Pokémon dibujó."""
import os
import re
import textwrap
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def plain(s):
    s = re.sub(r'\[([^\]]+)\]\([^)]+\)', r'\1', s)     # enlaces → texto
    s = re.sub(r'[*`_]', '', s)
    return s.strip()


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'romfs', 'credits.txt')
    lines = ['# Autores', 'PokéSurvivor 3DS: juanluismm520 (creador del port)',
             'github.com/juanluismm521/pokesurvivor3ds', '',
             'PokéSurvivor: JPTER78 (creador del juego original)',
             'github.com/JPTER78/pokesurvivor', '',
             '# PokéSurvivor 3DS', 'Port para Nintendo 3DS de PokéSurvivor (pokesurvivor.com).',
             'Proyecto de fans sin ánimo de lucro, sin relación con',
             'Nintendo, Game Freak ni The Pokémon Company. Gratis siempre.',
             'Código: PolyForm Noncommercial 1.0.0 (© 2026 JPTER78).', '']
    para = []
    skip = False   # (la sección «Gracias» de CREDITS.md ya va arriba)
    with open(os.path.join(ROOT, 'CREDITS.md'), encoding='utf-8') as f:
        for raw in f:
            l = raw.rstrip('\n')
            if l.startswith('## '): skip = l.strip() == '## Gracias'
            if skip: continue
            if l.startswith('|'):
                cells = [c.strip() for c in l.strip('|').split('|')]
                if len(cells) < 3 or cells[0] in ('Artista', '---') or set(cells[0]) <= set('-'):
                    continue
                n = len([x for x in cells[2].split(',') if x.strip()])
                lines.append(f'{plain(cells[0])} · {n} Pokémon')
                continue
            if l.startswith('#'):
                if para: lines.append(' '.join(para)); para = []
                lines.append('')
                lines.append('# ' + plain(l.lstrip('#')))
                continue
            if not l.strip():
                if para: lines.append(' '.join(para)); para = []
                continue
            para.append(plain(l))
    if para: lines.append(' '.join(para))
    # Renglones cortos: caben en la pantalla de abajo (320 px).
    wrapped = []
    for l in lines:
        if l.startswith('#') or len(l) <= 62: wrapped.append(l)
        else: wrapped.extend(textwrap.wrap(l, 62))
    lines = wrapped
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')
    print(f'gen_credits: {len(lines)} líneas')


if __name__ == '__main__':
    main()

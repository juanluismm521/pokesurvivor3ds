#!/usr/bin/env python3
"""gen_credits.py — CREDITS.md → romfs/credits.txt y romfs/credits_en.txt (los
créditos que salen en el juego, en español y en inglés). Una línea por renglón;
las que empiezan por '#' son títulos. De la tabla de artistas se queda el nombre
y cuántos Pokémon dibujó. El inglés sale de EN (abajo): si CREDITS.md cambia y
falta algo por traducir, se para y lo dice."""
import os
import re
import textwrap
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def plain(s):
    s = re.sub(r'\[([^\]]+)\]\([^)]+\)', r'\1', s)     # enlaces → texto
    s = re.sub(r'[*`]', '', s)
    s = re.sub(r'(?<![\w])_|_(?![\w])', '', s)   # (cursiva con _; no los de dentro de un nombre)
    return s.strip()


# Inglés: títulos y párrafos de CREDITS.md (en texto plano, como salen de plain()).
EN = {
    'Créditos': 'Credits',
    'Sprites': 'Sprites',
    'Sprites de las megaevoluciones': 'Mega Evolution sprites',
    'Datos': 'Data',
    'Fuente': 'Font',
    'Bibliotecas (3DS)': 'Libraries (3DS)',
    'Marca': 'Trademark',
    'Los sprites estilo Pokémon Mundo Misterioso vienen de SpriteCollab, bajo licencia CC BY-NC 4.0: uso no comercial y con atribución a sus artistas.':
        'The Pokémon Mystery Dungeon style sprites come from SpriteCollab, under the CC BY-NC 4.0 license: non-commercial use with attribution to their artists.',
    'De SpriteCollab (CC BY-NC 4.0), por sus autores:': 'From SpriteCollab (CC BY-NC 4.0), by their authors:',
    'Mega-Lucario Z aún no está en SpriteCollab: el port recolorea el de Mega-Lucario (tools/megas_derivadas.py). Las megas sin sprite en SpriteCollab usan el de su forma normal. Las megapiedras: dibujos de los objetos de PokeAPI (de los juegos, © Nintendo / Game Freak).':
        'Mega Lucario Z is not on SpriteCollab yet: the port recolors Mega Lucario\'s (tools/megas_derivadas.py). Megas without a SpriteCollab sprite use their normal form\'s. Mega Stones: item artwork from PokeAPI (from the games, © Nintendo / Game Freak).',
    'Stats base y nombres en español: PokeAPI, descargados una vez y guardados en js/data/pokedex.js.':
        'Base stats and Spanish names: PokeAPI, downloaded once and stored in js/data/pokedex.js.',
    'Pixelify Sans (SIL Open Font License 1.1, licencia en assets/fonts/OFL.txt). El juego usa una versión modificada, "Pixelify PS", con los números redibujados para distinguirlos mejor (tools/make_font.py).':
        'Pixelify Sans (SIL Open Font License 1.1, license in assets/fonts/OFL.txt). The game uses a modified version, "Pixelify PS", with redrawn numbers so they are easier to tell apart (tools/make_font.py).',
    'En la 3DS, los textos pequeños usan Tiny5 de Stefan Schmidt (SIL Open Font License 1.1, licencia en assets-src/fonts/OFL-Tiny5.txt), que a 8 px se ve nítida en la pantalla.':
        'On the 3DS, small text uses Tiny5 by Stefan Schmidt (SIL Open Font License 1.1, license in assets-src/fonts/OFL-Tiny5.txt), which looks sharp on screen at 8 px.',
    'El QR de «Entrar con Google» se hace con QR Code generator de Project Nayuki (licencia MIT, en src/platform/3ds/qrcodegen.c). La conexión con la web usa curl y Mbed TLS de los paquetes de devkitPro. Las actualizaciones del juego van firmadas con Monocypher de Loup Vaillant (BSD-2 / CC0, en src/core/monocypher.c).':
        'The "Sign in with Google" QR code is made with QR Code generator by Project Nayuki (MIT license, in src/platform/3ds/qrcodegen.c). The connection to the website uses curl and Mbed TLS from the devkitPro packages. Game updates are signed with Monocypher by Loup Vaillant (BSD-2 / CC0, in src/core/monocypher.c).',
    'Pokémon es propiedad de Nintendo, Creatures Inc. y GAME FREAK. Este es un proyecto de fan sin ánimo de lucro.':
        'Pokémon is the property of Nintendo, Creatures Inc. and GAME FREAK. This is a non-profit fan project.',
}
HEAD = {
    'es': ['# Autores', 'PokéSurvivor 3DS: juanluismm520 (creador del port)',
           'github.com/juanluismm521/pokesurvivor3ds', '',
           'PokéSurvivor: JPTER78 (creador del juego original)',
           'github.com/JPTER78/pokesurvivor', '',
           '# PokéSurvivor 3DS', 'Port para Nintendo 3DS de PokéSurvivor (pokesurvivor.com).',
           'Proyecto de fans sin ánimo de lucro, sin relación con',
           'Nintendo, Game Freak ni The Pokémon Company. Gratis siempre.',
           'Código: PolyForm Noncommercial 1.0.0 (© 2026 JPTER78).', ''],
    'en': ['# Authors', 'PokéSurvivor 3DS: juanluismm520 (creator of the port)',
           'github.com/juanluismm521/pokesurvivor3ds', '',
           'PokéSurvivor: JPTER78 (creator of the original game)',
           'github.com/JPTER78/pokesurvivor', '',
           '# PokéSurvivor 3DS', 'Nintendo 3DS port of PokéSurvivor (pokesurvivor.com).',
           'Non-profit fan project, not affiliated with Nintendo,',
           'Game Freak or The Pokémon Company. Always free.',
           'Code: PolyForm Noncommercial 1.0.0 (© 2026 JPTER78).', ''],
}


def tr(s, lang, falta):
    if lang == 'es': return s
    if s in EN: return EN[s]
    falta.append(s)
    return s


def mega_en(s):
    """«Mega-Lucario Z: autores, recoloreado en el port desde el de Mega-Lucario» en inglés."""
    s = re.sub(r'\bMega-', 'Mega ', s)
    return s.replace('recoloreado en el port desde el de', 'recolored in the port from')


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'romfs', 'credits.txt')
    falta = []
    for lang in ('es', 'en'):
        build(lang, out if lang == 'es' else out.replace('credits.txt', 'credits_en.txt'), falta)
    if falta:
        print('gen_credits: falta traducir al inglés (EN en tools/gen_credits.py):')
        for f in falta: print('  ', f)
        sys.exit(1)


def build(lang, out, falta):
    lines = list(HEAD[lang])
    para = []
    skip = False   # (la sección «Gracias» de CREDITS.md ya va arriba)
    with open(os.path.join(ROOT, 'CREDITS.md'), encoding='utf-8') as f:
        for raw in f:
            l = raw.rstrip('\n')
            if l.startswith('## '): skip = l.strip() == '## Gracias'
            if skip: continue
            if '<!--' in l:   # (marcadores de secciones generadas)
                l = re.sub(r'<!--.*?-->', '', l)
                if not l.strip():
                    continue
            if l.startswith('|'):
                cells = [c.strip() for c in l.strip('|').split('|')]
                if not cells or cells[0] in ('Artista', 'Mega', '---') or set(cells[0]) <= set('-'):
                    continue
                if len(cells) == 2:   # (sprites de las megas: «Mega-X: autores»)
                    m = f'{plain(cells[0])}: {plain(cells[1])}'
                    lines.append(mega_en(m) if lang == 'en' else m)
                    continue
                if len(cells) < 3:
                    continue
                n = len([x for x in cells[2].split(',') if x.strip()])
                lines.append(f'{plain(cells[0])} · {n} Pokémon')
                continue
            if l.startswith('#'):
                if para: lines.append(tr(' '.join(para), lang, falta)); para = []
                lines.append('')
                lines.append('# ' + tr(plain(l.lstrip('#')), lang, falta))
                continue
            if not l.strip():
                if para: lines.append(tr(' '.join(para), lang, falta)); para = []
                continue
            para.append(plain(l))
    if para: lines.append(tr(' '.join(para), lang, falta))
    # Renglones cortos: caben en la pantalla de abajo (320 px).
    wrapped = []
    for l in lines:
        if l.startswith('#') or len(l) <= 62: wrapped.append(l)
        else: wrapped.extend(textwrap.wrap(l, 62))
    lines = wrapped
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')
    print(f'gen_credits: {os.path.basename(out)}, {len(lines)} líneas')


if __name__ == '__main__':
    main()

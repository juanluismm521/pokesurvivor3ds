#!/usr/bin/env python3
"""extraer.py — saca los textos en español del código del port (los que se ven en
pantalla) a build/i18n/textos.tsv: archivo, línea y texto (los %d/%s de printf, como {x}).
Se queda con lo que parece texto para el jugador: con letras y espacio, tilde o
mayúscula; fuera rutas, ids, registros (fprintf a los logs) y cosas de depurar."""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = [os.path.join(ROOT, d) for d in ("src/platform/3ds", "src/core")]
SKIP_FILES = {"lang_en_port.cpp", "gamedata.cpp", "i18n.cpp", "crash3ds.cpp"}   # (crash3ds: fallo.log, para el autor)
LIT = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
FMT = re.compile(r'%[-+#0]*\d*(?:\.\d+)?(?:ll|l|h|z)?[sdifuxXgc]')
SKIP_LINE = re.compile(r'fprintf|std::printf|\bprintf\(|svcOutputDebugString|#include|log_line|LOG\(|static_assert|dbg\(|perror|fopen|"rb"|"wb"|\.log"|crash3ds::guard')

def literals(text):
    """Literales de C con su línea, juntando los que van seguidos ("a" "b")."""
    out = []
    i, n, line = 0, len(text), 1
    while i < n:
        c = text[i]
        if c == '\n': line += 1; i += 1; continue
        if text.startswith('//', i):
            j = text.find('\n', i); i = n if j < 0 else j; continue
        if text.startswith('/*', i):
            j = text.find('*/', i); seg = text[i:j]; line += seg.count('\n'); i = j + 2; continue
        if c == "'":
            j = i + 1
            while j < n and text[j] != "'": j += 2 if text[j] == '\\' else 1
            i = j + 1; continue
        if c == '"' and not (i > 0 and text[i - 1] == 'R'):
            start_line = line
            parts = []
            while True:
                m = LIT.match(text, i)
                if not m: break
                parts.append(m.group(1)); i = m.end()
                k = i
                while k < n and text[k] in ' \t\n': k += 1
                if k < n and text[k] == '"':
                    line += text[i:k].count('\n'); i = k; continue
                break
            out.append((start_line, ''.join(parts)))
            continue
        i += 1
    return out

def unescape(s):
    return s.replace('\\"', '"').replace('\\n', '\n').replace('\\\\', '\\')

def is_text(s):
    if not re.search(r'[A-Za-zÁÉÍÓÚáéíóúÑñ¡¿]', s): return False
    if re.search(r'(^|[^ ])/[a-z]|\.(json|txt|log|cia|3dsx|png|bin|t3x)\b|^https?:|sdmc:|romfs:|^[a-z0-9_\-.]+$|^[A-Z_0-9]+$|%[0-9]*[sd]/', s): return False
    letters = re.sub(r'\{x\}', '', s)
    if not re.search(r'[A-Za-zÁÉÍÓÚáéíóúÑñ]{2,}', letters): return False
    return bool(re.search(r'[ ÁÉÍÓÚáéíóúÑñ¡¿]', s) or re.match(r'^[A-ZÁÉÍÓÚÑ¡¿]', s))

rows, seen = [], set()
for d in SRC:
    for f in sorted(os.listdir(d)):
        if not f.endswith(('.cpp', '.h')) or f in SKIP_FILES: continue
        p = os.path.join(d, f)
        text = open(p, encoding='utf-8').read()
        lines = text.split('\n')
        for ln, raw in literals(text):
            src = lines[ln - 1] if ln - 1 < len(lines) else ''
            if SKIP_LINE.search(src): continue
            s = FMT.sub('{x}', unescape(raw)).replace('%%', '%')
            if not is_text(s): continue
            key = s
            if key in seen: continue
            seen.add(key)
            rows.append((os.path.relpath(p, ROOT), ln, s))
os.makedirs(os.path.join(ROOT, 'build', 'i18n'), exist_ok=True)
out = os.path.join(ROOT, 'build', 'i18n', 'textos.tsv')
with open(out, 'w', encoding='utf-8') as fo:
    for f, ln, s in rows:
        s = s.replace('\t', ' ').replace('\n', '\\n')
        fo.write(f + '\t' + str(ln) + '\t' + s + '\n')
print(len(rows), 'textos →', out)

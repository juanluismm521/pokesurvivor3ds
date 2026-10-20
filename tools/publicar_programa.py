#!/usr/bin/env python3
"""publicar_programa.py — publica versiones del juego para que las consolas se
actualicen solas (core/update, platform/3ds/prog3ds).

  publicar_programa.py publicar        compila (.3dsx y .cia), pasa las pruebas
                                       de PC, firma y publica la build de HEAD
  publicar_programa.py auto            (para el temporizador) baja main de GitHub
                                       en la copia de publicación y, si hay
                                       commits nuevos, la publica
  publicar_programa.py volver [build]  vuelve a publicar una versión anterior
                                       como actual (para todas las consolas)
  publicar_programa.py estado          qué hay publicado

Lo publicado va a PS_PROGRAMA_DIR (por defecto el nginx de datos del servidor:
https://pokesurvivor3ds.duckdns.org/datos/programa/): ultimo.json (actual y
anterior, cada una firmada), historial.json y una carpeta por build. La clave
privada está en /root/.config/pokesurvivor/firma.key (NUNCA en el repo).
"""
import datetime, json, os, shutil, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIR = os.environ.get('PS_PROGRAMA_DIR', '/root/docker/pokesurvivor-relay/datos/www/datos/programa')
KEY = os.environ.get('PS_FIRMA', '/root/.config/pokesurvivor/firma.key')
CLONE = os.environ.get('PS_CLON', '/root/pokesurvivor-publicar')   # copia sólo para publicar (modo auto)
KEEP = int(os.environ.get('PS_GUARDAR', '6'))                      # builds que se guardan en el servidor
FIRMAR = os.path.join(ROOT, 'tools', 'cache', 'firmar')


def sh(cmd, cwd=None, check=True):
    print('$', cmd, flush=True)
    r = subprocess.run(['bash', '-c', cmd], cwd=cwd or ROOT)
    if check and r.returncode != 0:
        raise SystemExit(f'falla: {cmd}')
    return r.returncode


def out(cmd, cwd=None):
    return subprocess.run(['bash', '-c', cmd], cwd=cwd or ROOT, capture_output=True, text=True, check=True).stdout.strip()


def firmar_bin(repo):
    if not os.path.exists(FIRMAR) or os.path.getmtime(FIRMAR) < os.path.getmtime(os.path.join(repo, 'tools', 'firma', 'firmar.c')):
        os.makedirs(os.path.dirname(FIRMAR), exist_ok=True)
        sh(f'gcc -O2 -Isrc/core tools/firma/firmar.c src/core/monocypher.c -o {FIRMAR}', cwd=repo)
    return FIRMAR


def mensaje(e):
    # El mismo texto que firma/comprueba core/update.cpp (update_message).
    return (f"pokesurvivor3ds|{e['serie']}|{e['build']}|{e['version']}|{e['commit']}|"
            f"{e['3dsx']['huella']}|{e['3dsx']['tam']}|{e['cia']['huella']}|{e['cia']['tam']}")


def firma(e, repo=ROOT):
    return out(f"{firmar_bin(repo)} firma {KEY} '{mensaje(e)}'")


def leer(nombre, defecto):
    try:
        with open(os.path.join(DIR, nombre), encoding='utf-8') as f:
            return json.load(f)
    except (OSError, ValueError):
        return defecto


def escribir(nombre, obj):
    tmp = os.path.join(DIR, nombre + '.tmp')
    with open(tmp, 'w', encoding='utf-8') as f:
        json.dump(obj, f, ensure_ascii=False, indent=1)
    os.replace(tmp, os.path.join(DIR, nombre))


def publicar_entrada(e, repo=ROOT):
    """e sin serie ni firma → la hace actual (la de antes pasa a anterior)."""
    ult = leer('ultimo.json', {})
    hist = leer('historial.json', [])
    antes = ult.get('actual')
    e = dict(e)
    e['serie'] = max([h.get('serie', 0) for h in hist] + [antes['serie'] if antes else 0]) + 1
    e['firma'] = firma(e, repo)
    nuevo = {'actual': e, 'anterior': antes if antes and antes['build'] != e['build'] else ult.get('anterior')}
    hist.append({k: v for k, v in e.items()})
    escribir('historial.json', hist)
    escribir('ultimo.json', nuevo)
    # Fuera las carpetas viejas (nunca la actual ni la anterior).
    vivas = {f"b{e['build']}"} | ({f"b{nuevo['anterior']['build']}"} if nuevo.get('anterior') else set())
    builds = sorted({h['build'] for h in hist}, reverse=True)
    vivas |= {f'b{b}' for b in builds[:KEEP]}
    for d in os.listdir(DIR):
        if d.startswith('b') and d[1:].isdigit() and d not in vivas:
            shutil.rmtree(os.path.join(DIR, d), ignore_errors=True)
    print(f"publicada: serie {e['serie']} · build {e['build']} ({e['commit']}) · anterior: "
          f"{nuevo['anterior']['build'] if nuevo.get('anterior') else '-'}")
    return e


def publicar(repo=ROOT, notas=''):
    os.makedirs(DIR, exist_ok=True)
    build = int(os.environ.get('PS_BUILD_N') or out('git rev-list --count HEAD', repo))   # (PS_BUILD_N: sólo pruebas)
    commit = out('git rev-parse --short=7 HEAD', repo)
    version = out("grep -o 'kVersion = \"[^\"]*\"' src/core/version.h | cut -d'\"' -f2", repo)
    ult = leer('ultimo.json', {})
    if ult.get('actual', {}).get('build') == build:
        print(f'la build {build} ya está publicada')
        return
    # Recursos (romfs/) si faltan o han cambiado sus fuentes desde la última.
    last = ult.get('actual', {}).get('commit')
    tocar = not os.path.isdir(os.path.join(repo, 'romfs', 'spr'))
    if not tocar and last:
        cambios = subprocess.run(['git', 'diff', '--name-only', last, 'HEAD'], cwd=repo, capture_output=True, text=True).stdout
        tocar = any(p.startswith(('assets-src/', 'tools/gen_', 'tools/build_assets', 'reference/web/')) for p in cambios.split())
    # Las librerías de Node de tools/ (no van en git): las del repo de trabajo o npm.
    nm = os.path.join(repo, 'tools', 'node_modules')
    if not os.path.exists(nm):
        if repo != ROOT and os.path.isdir(os.path.join(ROOT, 'tools', 'node_modules')):
            os.symlink(os.path.join(ROOT, 'tools', 'node_modules'), nm)
        else:
            sh('PLAYWRIGHT_SKIP_BROWSER_DOWNLOAD=1 npm install --no-audit --no-fund', cwd=os.path.join(repo, 'tools'))
    if tocar:
        sh('tools/build_assets.sh', cwd=repo)
    # Pruebas de PC: si no pasan, no se publica nada.
    sh('cmake -S . -B build-pc > /dev/null && cmake --build build-pc -j2 && cd build-pc && ctest --output-on-failure', cwd=repo)
    # El juego: .3dsx y .cia (make_cia.sh también hace make).
    sh('source tools/env.sh && tools/make_cia.sh', cwd=repo)
    dest = os.path.join(DIR, f'b{build}')
    os.makedirs(dest, exist_ok=True)
    e = {'build': build, 'version': version, 'commit': commit, 'fecha': datetime.date.today().isoformat(), 'notas': notas}
    for ext in ('3dsx', 'cia'):
        src = os.path.join(repo, f'pokesurvivor-3ds.{ext}')
        shutil.copyfile(src, os.path.join(dest, f'pokesurvivor-3ds.{ext}'))
        e[ext] = {'url': f'b{build}/pokesurvivor-3ds.{ext}', 'tam': os.path.getsize(src),
                  'huella': out(f'{firmar_bin(repo)} huella {src}')}
    publicar_entrada(e, repo)


def auto():
    if not os.path.isdir(os.path.join(CLONE, '.git')):
        url = out('git remote get-url origin')
        sh(f'git clone {url} {CLONE}')
    sh('git fetch -q origin main && git checkout -q -B main origin/main', cwd=CLONE)
    publicar(CLONE, notas=out('git log -1 --format=%s', CLONE)[:200])


def volver(build=None):
    ult = leer('ultimo.json', {})
    hist = leer('historial.json', [])
    cur = ult.get('actual')
    if build is None:
        objetivo = ult.get('anterior')
    else:
        objetivo = next((h for h in reversed(hist) if h['build'] == int(build)), None)
    if not objetivo:
        raise SystemExit('no hay versión anterior a la que volver')
    if not os.path.exists(os.path.join(DIR, objetivo['3dsx']['url'])):
        raise SystemExit(f"los archivos de la build {objetivo['build']} ya no están en el servidor")
    e = {k: v for k, v in objetivo.items() if k not in ('serie', 'firma')}
    e['notas'] = f"vuelta a la build {objetivo['build']} (desde la {cur['build'] if cur else '-'})"
    publicar_entrada(e)


def estado():
    ult = leer('ultimo.json', {})
    for k in ('actual', 'anterior'):
        e = ult.get(k)
        print(f"{k}: " + (f"serie {e['serie']} · build {e['build']} · {e['version']} · {e['commit']} · {e['fecha']} · {e.get('notas', '')}" if e else '-'))
    print('historial:', ', '.join(f"{h['serie']}:b{h['build']}" for h in leer('historial.json', [])))


if __name__ == '__main__':
    modo = sys.argv[1] if len(sys.argv) > 1 else 'estado'
    if modo == 'publicar': publicar()
    elif modo == 'auto': auto()
    elif modo == 'volver': volver(sys.argv[2] if len(sys.argv) > 2 else None)
    elif modo == 'estado': estado()
    else: raise SystemExit(__doc__)

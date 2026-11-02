#!/usr/bin/env python3
"""regalo.py — regalos firmados para un jugador (core/gift.h): para devolverle lo que
perdió por un fallo del juego. Las consolas los bajan al buscar versiones nuevas
(.../datos/regalos.json) y dan los que son para ellas una sola vez.

  regalo.py dar ID PARA [monedas=N] [tickets=N] [tickets10=N] [caramelos=N]
                        [medalla=DEX] [shinies=DEX,DEX] [nota="texto"]
  regalo.py lista
  regalo.py quitar ID

PARA es el nombre del jugador en la 3DS o el de su cuenta de pokesurvivor.com.
Firmado con la clave de las publicaciones (/root/.config/pokesurvivor/firma.key, NUNCA
en el repo): nadie más puede crear regalos.
"""
import json, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import publicar_programa as pp  # noqa: E402  (firmar y escribir, igual que las versiones)

ARCHIVO = os.path.join(os.path.dirname(pp.DIR.rstrip('/')), 'regalos.json')   # .../datos/regalos.json


def mensaje(r):
    # El mismo texto que firma/comprueba core/gift.cpp (gift_message).
    return (f"pokesurvivor3ds-regalo|{r['id']}|{r['para']}|{r['monedas']}|{r['tickets']}|{r['tickets10']}|"
            f"{r['caramelos']}|{r['medalla']}|" + ','.join(str(d) for d in r['shinies']))


def leer():
    try:
        with open(ARCHIVO, encoding='utf-8') as f:
            return json.load(f)
    except (OSError, ValueError):
        return {'regalos': []}


def escribir(d):
    tmp = ARCHIVO + '.tmp'
    with open(tmp, 'w', encoding='utf-8') as f:
        json.dump(d, f, ensure_ascii=False, indent=1)
    os.replace(tmp, ARCHIVO)


def dar(rid, para, opts):
    if not rid or len(rid) >= 32 or not para or "'" in rid + para:
        raise SystemExit('id (menos de 32 letras) y para, sin comillas')
    r = {'id': rid, 'para': para, 'nota': opts.get('nota', ''), 'monedas': int(opts.get('monedas', 0)),
         'tickets': int(opts.get('tickets', 0)), 'tickets10': int(opts.get('tickets10', 0)),
         'caramelos': int(opts.get('caramelos', 0)), 'medalla': int(opts.get('medalla', 0)),
         'shinies': [int(x) for x in opts.get('shinies', '').split(',') if x.strip()][:8]}
    r['firma'] = pp.out(f"{pp.firmar_bin(pp.ROOT)} firma {pp.KEY} '{mensaje(r)}'")
    d = leer()
    d['regalos'] = [x for x in d['regalos'] if x.get('id') != rid] + [r]
    escribir(d)
    print('regalo publicado:', json.dumps(r, ensure_ascii=False))


def main(a):
    if len(a) >= 3 and a[0] == 'dar':
        dar(a[1], a[2], dict(x.split('=', 1) for x in a[3:] if '=' in x))
    elif a[:1] == ['lista']:
        for r in leer()['regalos']:
            print(f"{r['id']} → {r['para']}: {r['monedas']} monedas · {r['tickets']} tickets · {r['tickets10']} ×10 · "
                  f"{r['caramelos']} caramelos · medalla {r['medalla']} · shinies {r['shinies']} · {r.get('nota', '')}")
    elif len(a) == 2 and a[0] == 'quitar':
        d = leer()
        d['regalos'] = [x for x in d['regalos'] if x.get('id') != a[1]]
        escribir(d)
        print('quitado', a[1])
    else:
        print(__doc__)
        sys.exit(2)


if __name__ == '__main__':
    main(sys.argv[1:])

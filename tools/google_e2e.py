#!/usr/bin/env python3
"""google_e2e.py — entrar con Google desde la 3DS, de punta a punta, en local:
emulador de Firebase (tools/firebase-emu, puertos 9099/8089) + un Google falso
(flujo de dispositivo) + build-pc/ps_cloud (el mismo código que la consola).

Comprueba lo que le pasa a la gente de verdad:
  1. Una cuenta de NOMBRE y contraseña con su partida; luego le vincula su
     Gmail (lo que hace la web en «¿Te da miedo perder tu cuenta?»). Al entrar
     con Google desde la 3DS es la MISMA cuenta y tiene su partida.
  2. Una cuenta creada con Google en la web (sin contraseña): la 3DS entra con
     Google, ve su partida y sube lo suyo.
"""
import base64, http.server, json, os, subprocess, sys, tempfile, threading, urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AUTH = 'http://127.0.0.1:9099/identitytoolkit.googleapis.com/v1'
FS = 'http://127.0.0.1:8089/v1/projects/demo-pokesurvivor/databases/(default)/documents'
KEY = 'fake-api-key'
CUR = {'jwt': None}

def jwt(sub, email):
    b = lambda o: base64.urlsafe_b64encode(json.dumps(o).encode()).decode().rstrip('=')
    return b({'alg': 'none', 'typ': 'JWT'}) + '.' + b({'sub': sub, 'email': email, 'email_verified': True, 'name': 'Juan Luis',
                                                     'iss': 'https://accounts.google.com', 'aud': 'cliente-tv'}) + '.'

class FakeGoogle(http.server.BaseHTTPRequestHandler):
    polls = 0
    wait = 2           # preguntas antes de "aceptado"
    def log_message(self, *a): pass
    def do_POST(self):
        n = int(self.headers.get('Content-Length', 0)); self.rfile.read(n)
        if self.path.startswith('/device/code'):
            FakeGoogle.polls = 0
            out, st = {'device_code': 'DC', 'user_code': 'PKMN-3DSX', 'verification_url': 'https://www.google.com/device', 'interval': 1, 'expires_in': 60}, 200
        else:
            FakeGoogle.polls += 1
            out, st = ({'error': 'authorization_pending'}, 428) if FakeGoogle.polls < FakeGoogle.wait else ({'access_token': 'x', 'id_token': CUR['jwt']}, 200)
        body = json.dumps(out).encode()
        self.send_response(st); self.send_header('Content-Type', 'application/json'); self.end_headers(); self.wfile.write(body)

def post(url, data):
    r = urllib.request.Request(url, json.dumps(data).encode(), {'Content-Type': 'application/json'})
    return json.loads(urllib.request.urlopen(r).read())

def name_to_email(n):
    return n.strip().lower().encode().hex() + '@jugadores.pokesurvivor.net'

def ps_cloud(args, save, out):
    env = dict(os.environ, PS_GOOGLE_DEVICE='http://127.0.0.1:8765/device/code', PS_GOOGLE_TOKEN='http://127.0.0.1:8765/token')
    r = subprocess.run([os.path.join(ROOT, 'build-pc', 'ps_cloud')] + args + [save, 'ctr3ds', out], capture_output=True, text=True, env=env)
    print('   ', (r.stdout + r.stderr).strip().replace('\n', ' · '))
    return r.returncode == 0

def check(ok, what):
    print(('  ✔ ' if ok else '  ✘ ') + what)
    if not ok: sys.exit(1)

def main():
    srv = http.server.HTTPServer(('127.0.0.1', 8765), FakeGoogle)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    tmp = tempfile.mkdtemp()
    save = os.path.join(tmp, 'save.json')
    json.dump({'v': 3, 'name': 'x', 'coins': 700, 'owned': {'7': {'at': 5, 'from': 'starter'}}, 'updated': 1}, open(save, 'w'))
    import random
    tag = str(random.randint(1000, 99999))

    print('1. Cuenta de nombre + Gmail vinculado')
    name = 'Juanlu' + tag
    acc = post(f'{AUTH}/accounts:signUp?key={KEY}', {'email': name_to_email(name), 'password': 'secreta1', 'returnSecureToken': True})
    out1 = os.path.join(tmp, 'm1.json')
    check(ps_cloud([name, 'secreta1'], save, out1), 'la 3DS entra con nombre y sube su partida (Squirtle)')
    g = jwt('google-' + tag, f'juan{tag}@gmail.com')
    link = post(f'{AUTH}/accounts:signInWithIdp?key={KEY}', {'idToken': acc['idToken'], 'postBody': f'id_token={g}&providerId=google.com',
                                                            'requestUri': 'http://localhost', 'returnSecureToken': True})
    check(link['localId'] == acc['localId'], 'la web le vincula el Gmail (misma cuenta)')
    CUR['jwt'] = g
    save2 = os.path.join(tmp, 'save2.json')
    json.dump({'v': 3, 'name': 'x', 'coins': 500, 'owned': {'1': {'at': 9, 'from': 'starter'}}, 'updated': 2}, open(save2, 'w'))
    out2 = os.path.join(tmp, 'm2.json')
    check(ps_cloud(['--google', '-'], save2, out2), 'la 3DS entra con Google (código en la consola)')
    m2 = json.load(open(out2))
    check('7' in m2['owned'] and '1' in m2['owned'], 'es la MISMA cuenta: tiene el Squirtle de antes y el Bulbasaur de esta consola')
    check(ps_cloud([name, 'secreta1'], save, os.path.join(tmp, 'm3.json')), 'y con nombre y contraseña sigue entrando')

    print('2. Cuenta creada con Google en la web (sin contraseña)')
    g2 = jwt('solo-google-' + tag, f'ana{tag}@gmail.com')
    web = post(f'{AUTH}/accounts:signInWithIdp?key={KEY}', {'postBody': f'id_token={g2}&providerId=google.com', 'requestUri': 'http://localhost', 'returnSecureToken': True})
    CUR['jwt'] = g2
    out4 = os.path.join(tmp, 'm4.json')
    check(ps_cloud(['--google', '-'], save, out4), 'la 3DS entra con Google')
    check(json.load(open(out4))['owned'].get('7') is not None, 'sube lo suyo a esa cuenta')
    doc = json.loads(urllib.request.urlopen(urllib.request.Request(f"{FS}/saves/{web['localId']}", headers={'Authorization': 'Bearer owner'})).read())
    check('save' in doc['fields'], 'la partida está en la cuenta de Google de la web (' + web['localId'][:8] + '…)')
    print('google_e2e: TODO BIEN')

main()

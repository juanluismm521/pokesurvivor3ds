# fuente.py — fotogramas de las grabaciones (400x480: arriba 0..240, abajo 240..480, abajo con 40 px de margen a cada lado)
import os, subprocess, numpy as np
from PIL import Image

RAW = '/root/trailer/raw/'
_cache = {}
# En inglés (TRAILER_LANG=en) se usa la toma «en_NOMBRE» si existe (grabada con idioma.txt en la SD).
_EN = os.environ.get('TRAILER_LANG', 'es') == 'en'
# Desfase de cada toma en inglés respecto a la española (s; medido comparando fotogramas).
EN_DESFASE = {'v1_eterna': 0.233, 'v1_dinamax': 0.167, 'v1_tera': 0.167, 'v1_z': 0.100, 'v1_mega': 0.200,
              'v1_pesadilla': 0.167, 'v1_semanal': 0.167, 'v1_tienda': 0.033, 'v1_gacha': 0.067}
def _mkv(name):
    if _EN and os.path.exists(RAW + 'en_' + name + '.mkv'): return RAW + 'en_' + name + '.mkv'
    return RAW + name + '.mkv'

def clip(name, start, dur, fps=60, speed=1.0):
    """Fotogramas [start, start+dur) de la grabación (a 60 fps), como arrays 480x400x3.
    `speed` > 1: acelerado (dur es el tiempo del tráiler)."""
    k = (name, round(start, 3), round(dur, 3))
    if k in _cache: return _cache[k]
    n = int(round(dur * fps)) + 2
    if _mkv(name) != RAW + name + '.mkv': start += EN_DESFASE.get(name, 0)
    vf = ['-vf', 'setpts=PTS/%.4f,fps=60' % speed] if speed != 1 else []
    p = subprocess.run(['ffmpeg', '-v', 'error', '-ss', '%.3f' % start, '-i', _mkv(name)] + vf + ['-frames:v', str(n),
                        '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], capture_output=True)
    a = np.frombuffer(p.stdout, np.uint8)
    fr = a.reshape(-1, 480, 400, 3)
    if len(fr) == 0: raise RuntimeError('sin fotogramas: %s %.2f' % (name, start))
    # Tapa «60 FPS · AUTOTEST» (abajo a la derecha de la pantalla de abajo).
    fr = fr.copy()
    fr[:, 240 + 216:240 + 225, 40 + 246:40 + 318] = fr[:, 240 + 226:240 + 227, 40 + 246:40 + 247]
    # El marcador del tráiler (abajo a la izquierda de la pantalla de abajo): posición de tu Pokémon.
    m = fr[:, 476:480, 40:44].reshape(len(fr), -1, 3).astype(np.float32)
    ok = np.median(m[:, :, 2], 1) > 200
    pos = np.stack([np.median(m[:, :, 0], 1) * 2, np.median(m[:, :, 1], 1)], 1)   # (R = x/2, G = y)
    if ok.any():
        pos[~ok] = pos[ok].mean(0)
        # suavizado (media móvil de ±8 fotogramas) para que la cámara no tiemble
        kk = 8; pp = np.pad(pos, ((kk, kk), (0, 0)), mode='edge')
        cs = np.cumsum(pp, 0); pos = (cs[2 * kk:] - np.concatenate([np.zeros((1, 2)), cs[:-2 * kk - 1]])) / (2 * kk + 1)
        pos = pos[:len(fr)]
        fr[:, 476:480, 40:44] = fr[:, 470:471, 40:41]   # (tapado)
    _pos[k] = pos if ok.any() else None
    while len(_cache) >= 3: _cache.pop(next(iter(_cache)))   # (las 3 últimas: en grupo se usan dos a la vez)
    _cache[k] = fr
    return fr

_pos = {}
def posicion(name, start, dur, i):
    """(x, y) suavizado de tu Pokémon en la pantalla de arriba, o None si la grabación no tiene marcador."""
    k = (name, round(start, 3), round(dur, 3))
    p = _pos.get(k)
    if p is None: return None
    return p[min(len(p) - 1, max(0, i))]

def frame(fr, t, fps=60):
    i = min(len(fr) - 1, max(0, int(t * fps)))
    return fr[i]

def top(f): return f[0:240]
def bottom(f): return f[240:480, 40:360]

def zoom(arr, cx, cy, w, h, out_w, out_h, pre=4):
    """Recorte (cx,cy,w,h en píxeles de la consola, decimales vale) escalado a out_w x out_h:
    primero por píxeles enteros (nítido) y luego suave (para que el zoom se mueva fino)."""
    H, W = arr.shape[:2]
    x0, y0 = cx - w / 2, cy - h / 2
    ix0, iy0 = int(np.floor(x0)), int(np.floor(y0))
    ix1, iy1 = int(np.ceil(x0 + w)) + 1, int(np.ceil(y0 + h)) + 1
    pad = np.zeros((iy1 - iy0, ix1 - ix0, 3), np.uint8)
    sx0, sy0, sx1, sy1 = max(0, ix0), max(0, iy0), min(W, ix1), min(H, iy1)
    if sx1 > sx0 and sy1 > sy0: pad[sy0 - iy0:sy1 - iy0, sx0 - ix0:sx1 - ix0] = arr[sy0:sy1, sx0:sx1]
    im = Image.fromarray(pad).resize((pad.shape[1] * pre, pad.shape[0] * pre), Image.NEAREST)
    fx, fy = (x0 - ix0) * pre, (y0 - iy0) * pre
    return im.resize((out_w, out_h), Image.BILINEAR, box=(fx, fy, fx + w * pre, fy + h * pre))

# audio.py — la banda sonora del tráiler (48 kHz, estéreo): música del juego por compases y diseño de sonido.
import subprocess, numpy as np
from tiempos import *
SR = 48000
SND = '/root/pokesurvivor-3ds/romfs/snd/'

def load(name):
    p = subprocess.run(['ffmpeg', '-v', 'error', '-i', SND + 'music_%s.ogg' % name, '-f', 'f32le', '-ac', '2', '-ar', str(SR), '-'], capture_output=True)
    return np.frombuffer(p.stdout, np.float32).reshape(-1, 2).copy()

def env(n, a=0.002, d=0.3):
    t = np.arange(n) / SR
    return np.minimum(t / a, 1) * np.exp(-t / d)

def lowpass(x, fc):
    # un polo, aplicado dos veces
    a = np.exp(-2 * np.pi * fc / SR); y = x.copy()
    for _ in range(2):
        from scipy.signal import lfilter
        y = lfilter([1 - a], [1, -a], y, axis=0)
    return y

def boom(dur=2.2, f0=120, f1=34, vol=1.0):
    vol *= 0.75
    n = int(dur * SR); t = np.arange(n) / SR
    f = f1 + (f0 - f1) * np.exp(-t * 9)
    ph = 2 * np.pi * np.cumsum(f) / SR
    s = np.sin(ph) * env(n, 0.003, 0.55) * 1.2
    nz = np.random.default_rng(1).standard_normal(n) * env(n, 0.001, 0.05) * 0.5
    s = np.tanh((s + lowpass(nz[:, None], 2500)[:, 0]) * 1.6) * vol
    return np.stack([s, s], 1)

def braam(dur=2.6, vol=0.8):
    n = int(dur * SR); t = np.arange(n) / SR; s = np.zeros(n)
    for f in (36.7, 55, 73.4, 110):
        for det in (-0.6, 0.6):
            ph = 2 * np.pi * (f + det) * t
            s += (2 * ((ph / (2 * np.pi)) % 1) - 1) / (1 + f / 55)
    s = np.tanh(s * 0.9) * env(n, 0.02, 0.9)
    s = lowpass(s[:, None], 900)[:, 0] * vol
    return np.stack([s, s * 0.97], 1)

def whoosh(dur=0.6, vol=0.5, up=True):
    n = int(dur * SR); t = np.arange(n) / SR
    nz = np.random.default_rng(2).standard_normal((n, 2))
    k = t / dur if up else 1 - t / dur
    from scipy.signal import butter, sosfilt
    out = np.zeros((n, 2))
    for i in range(8):   # barrido de un filtro paso banda
        a, b = i * n // 8, (i + 1) * n // 8
        fc = 300 + 5000 * (k[(a + b) // 2] ** 2)
        sos = butter(2, [fc * 0.6, fc * 1.4], 'band', fs=SR, output='sos')
        out[a:b] = sosfilt(sos, nz[a:b], axis=0)
    e = np.sin(np.pi * np.clip(t / dur, 0, 1)) ** 2
    return out * e[:, None] * vol * 3

def riser(dur=3.4, vol=0.5):
    n = int(dur * SR); t = np.arange(n) / SR
    f = 200 * (2 ** (t / dur * 3))
    s = np.sin(2 * np.pi * np.cumsum(f) / SR) * 0.3
    nz = np.random.default_rng(3).standard_normal(n)
    from scipy.signal import butter, sosfilt
    nz = sosfilt(butter(2, 1500, 'high', fs=SR, output='sos'), nz) * 0.4
    e = (t / dur) ** 2.2
    s = (s + nz) * e * vol
    return np.stack([s, s], 1)

def chime(dur=1.4, base=880, vol=0.35):
    n = int(dur * SR); t = np.arange(n) / SR; s = np.zeros(n)
    for i, m in enumerate((1, 1.5, 2, 3, 4)):
        f = base * m * (1 + 0.3 * (1 - np.exp(-t * 6)))
        s += np.sin(2 * np.pi * np.cumsum(f) / SR) * env(n, 0.005, 0.5 / (1 + i * 0.3)) / (1 + i)
    s *= vol
    return np.stack([s * 0.8, s], 1)

def drone(dur, vol=0.35):
    n = int(dur * SR); t = np.arange(n) / SR
    s = (np.sin(2 * np.pi * 55 * t) + 0.5 * np.sin(2 * np.pi * 82.4 * t + 0.3 * np.sin(2 * np.pi * 0.3 * t))) * 0.5
    s += 0.25 * np.sin(2 * np.pi * 110 * t) * (0.5 + 0.5 * np.sin(2 * np.pi * 0.7 * t))
    e = np.minimum(t / 1.0, 1)
    s = s * e * vol
    return np.stack([s, s], 1)

def build(path='/root/trailer/banda.wav'):
    N = int(T_TOTAL * SR) + SR
    mix = np.zeros((N, 2))
    def add(x, t, g=1.0):
        i = int(t * SR); j = min(N, i + len(x)); mix[i:j] += x[:j - i] * g
    rift, jud, boss = load('rift'), load('judgment'), load('boss')
    def seg(track, a, b, t, g=1.0, fade=0.01):
        x = track[int(a * SR):int(b * SR)].copy()
        k = int(fade * SR)
        if k: x[:k] *= np.linspace(0, 1, k)[:, None]; x[-k:] *= np.linspace(1, 0, k)[:, None]
        add(x, t, g)
    # --- intro: dron, golpes en cada rótulo, subida y el logo ---
    add(drone(T_DROP), 0, 0.55)
    for k in range(3): add(boom(2.0, 110, 32), k * B_J, 0.6)
    jl = lowpass(jud[:int(4 * B_J * SR)], 600)
    x = jl * np.linspace(0.2, 0.8, len(jl))[:, None]; add(x, 0, 0.7)
    add(braam(2.6), 3 * B_J, 0.9)
    add(riser(B_J * 1.0, 0.6), 3 * B_J)
    add(whoosh(0.5, 0.35), T_DROP - 0.5)
    # --- la Grieta (18 compases, la pista da la vuelta): megas ---
    seg(rift, 0, 16 * B_R, T_DROP, 1.3)
    seg(rift, 0, (N_R - 16) * B_R, T_DROP + 16 * B_R, 1.3)
    add(boom(2.4, 140, 30), T_DROP, 1.0)
    add(whoosh(0.7, 0.4), r(3) - 0.7); add(boom(2.0, 130, 30), r(3), 0.9); add(chime(1.6, 660, 0.4), r(3))
    add(chime(0.6, 1320, 0.25), r(5))                       # «Y pulsa»
    add(whoosh(0.5, 0.4), r(6) - 0.5); add(boom(2.6, 150, 28), r(6), 1.0); add(chime(2.2, 880, 0.45), r(6))   # ¡se pulsa la Y!
    add(chime(1.2, 1100, 0.3), r(6) + 1.4)                  # su nombre
    for b in (8, 10):                                       # Charizard X y Darkrai
        add(whoosh(0.4, 0.3), r(b) - 0.4); add(boom(1.6, 160, 40), r(b) + 0.35, 0.7); add(chime(1.4, 880, 0.35), r(b) + 0.35)
    add(whoosh(0.3, 0.25), r(12) - 0.3)   # Darkrai echa a andar
    add(whoosh(0.5, 0.3), r(13) - 0.5); add(boom(1.4, 150, 40), r(13), 0.6)
    for k in range(12): add(chime(0.25, 1200 + 60 * k, 0.08), r(13) + 0.08 * k)   # las casillas
    add(whoosh(0.4, 0.3), r(15) - 0.4); add(boom(1.2, 160, 45), r(15), 0.5)
    for k in range(4): add(whoosh(0.25, 0.25, False), r(17) + k * B_R * 0.5)
    # --- la Sentencia ---
    add(whoosh(0.9, 0.5), T_SENT - 0.9)
    add(boom(3.0, 100, 26), T_SENT, 1.2); add(braam(3.2, 1.0), T_SENT + B_J)
    seg(jud, 0, 4 * B_J, T_SENT, 1.3)
    # --- funciones (Jefe, 8 compases) ---
    seg(boss, 0, 8 * B_B, T_FEAT, 1.25)
    for b in (0, 2, 4, 6):
        add(whoosh(0.45, 0.35), f(b) - 0.45); add(boom(1.2, 160, 45), f(b), 0.6)
    # --- final (Grieta, compases 8-13), golpe y cola con los créditos ---
    add(boom(2.4, 140, 30), T_FIN, 1.0)
    seg(rift, 8 * B_R, 14 * B_R, T_FIN, 1.3, 0.02)
    add(riser(B_R * 2, 0.45), T_GOLPE - 2 * B_R)
    add(boom(3.5, 120, 25), T_GOLPE, 1.3); add(braam(3.6, 0.8), T_GOLPE); add(chime(2.4, 523, 0.4), T_GOLPE + 0.05)
    cola = lowpass(rift[:int(4 * B_R * SR)], 900)
    cola *= np.linspace(0.55, 0.0, len(cola))[:, None]
    add(cola, T_GOLPE + 0.4, 1.0)
    # --- máster ---
    mix = mix[:int(T_TOTAL * SR)]
    fade = int(0.9 * SR); mix[-fade:] *= np.linspace(1, 0, fade)[:, None]
    mix = np.tanh(mix * 1.1) / np.tanh(1.1)
    mix /= max(1e-6, np.abs(mix).max()) / 0.95
    import wave
    w = wave.open(path, 'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
    w.writeframes((mix * 32767).astype('<i2').tobytes()); w.close()
    print('banda:', path, '%.2f s' % (len(mix) / SR))

if __name__ == '__main__':
    build()

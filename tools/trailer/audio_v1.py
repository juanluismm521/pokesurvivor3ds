# audio_v1.py — la banda sonora del tráiler de la 1.0 (48 kHz, estéreo): música del juego por compases
# (Sentencia en la intro, Grieta en el cuerpo y el final) y el diseño de sonido de audio.py.
import os, sys, numpy as np
import audio
from audio import SR, env, lowpass, boom, braam, whoosh, riser, chime, drone
from tiempos_v1 import *

audio.SND = os.environ.get('PS_SND', '/root/docker/pokesurvivor/juego/romfs/snd/')

def build(path='/root/trailer/banda_v1.wav'):
    N = int(T_TOTAL * SR) + SR
    mix = np.zeros((N, 2))
    def add(x, t, g=1.0):
        i = int(t * SR); j = min(N, i + len(x)); mix[i:j] += x[:j - i] * g
    rift, jud = audio.load('rift'), audio.load('judgment')
    def seg(track, a, b, t, g=1.0, fade=0.01):
        x = track[int(a * SR):int(b * SR)].copy()
        k = int(fade * SR)
        if k: x[:k] *= np.linspace(0, 1, k)[:, None]; x[-k:] *= np.linspace(1, 0, k)[:, None]
        add(x, t, g)
    # --- intro: dron, la Sentencia apagada que crece, golpes en cada rótulo y el logo ---
    add(drone(T_DROP), 0, 0.5)
    jl = lowpass(jud[:int(4 * B_J * SR)], 700)
    add(jl * np.linspace(0.25, 0.9, len(jl))[:, None], 0, 0.75)
    for k in range(3): add(boom(2.0, 110, 32), k * B_J, 0.65)
    add(braam(2.6), 3 * B_J, 0.95)
    add(chime(2.0, 523, 0.35), 3 * B_J + 0.05)
    add(riser(B_J * 1.0, 0.6), 3 * B_J)
    add(whoosh(0.5, 0.35), T_DROP - 0.5)
    # --- la Grieta ---
    seg(rift, 0, N_R * B_R, T_DROP, 1.3)
    add(boom(2.4, 140, 30), T_DROP, 1.0)
    for k in range(1, 4): add(whoosh(0.3, 0.25), r(k * 0.5) - 0.3); add(boom(1.0, 160, 45), r(k * 0.5), 0.45)
    # Eternatus: golpe grave y braam
    add(whoosh(0.7, 0.45), r(2) - 0.7); add(boom(3.0, 90, 24), r(2), 1.2); add(braam(3.4, 0.9), r(2) + 0.05)
    add(boom(1.6, 120, 30), r(3.5), 0.7)
    # Poderes: uno por compás, con su destello
    for b in (5, 6, 7, 8):
        add(whoosh(0.4, 0.35), r(b) - 0.4); add(boom(1.6, 160, 40), r(b), 0.8); add(chime(1.3, 660 + (b - 5) * 110, 0.35), r(b) + 0.03)
    # Ráfaga de novedades
    for b in range(9, 15):
        add(whoosh(0.3, 0.3), r(b) - 0.3); add(boom(1.1, 170, 50), r(b), 0.55)
    add(riser(B_R * 1.0, 0.5), r(14))
    # --- final (Grieta, compases 8-11), golpe y cola con los créditos ---
    add(boom(2.4, 140, 30), T_FIN, 1.0)
    seg(rift, 8 * B_R, 12 * B_R, T_FIN, 1.3, 0.02)
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
    build(*sys.argv[1:2])

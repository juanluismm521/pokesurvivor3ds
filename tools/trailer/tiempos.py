# tiempos.py — la escaleta del tráiler, cuadrada con los compases de la música del juego.
B_J = 60 / 140 * 4      # compás de «La Sentencia» (140 BPM)
B_R = 60 / 164 * 4      # compás de «Grieta» (164 BPM)
B_B = 60 / 150 * 4      # compás de «Jefe» (150 BPM)
T_DROP = 4 * B_J                    # 6.857: entra la Grieta
T_SENT = T_DROP + 16 * B_R          # 30.272: la Sentencia
T_FEAT = T_SENT + 4 * B_J           # 37.129: cooperativo, nube, actualizaciones (Jefe)
T_FIN = T_FEAT + 8 * B_B            # 49.929: final (Grieta, compases 8-13)
T_GOLPE = T_FIN + 6 * B_R           # 58.710: golpe final
T_TOTAL = T_GOLPE + 1.9             # 60.6
def r(bar): return T_DROP + bar * B_R          # inicio del compás `bar` de la Grieta
def f(bar): return T_FEAT + bar * B_B          # del Jefe
def e(bar): return T_FIN + bar * B_R           # del final

# tiempos.py — la escaleta del tráiler, cuadrada con los compases de la música del juego.
B_J = 60 / 140 * 4      # compás de «La Sentencia» (140 BPM)
B_R = 60 / 164 * 4      # compás de «Grieta» (164 BPM)
B_B = 60 / 150 * 4      # compás de «Jefe» (150 BPM)
N_R = 19                            # compases de Grieta en la parte de las megas (la pista da la vuelta a los 16)
T_DROP = 4 * B_J                    # 6.857: entra la Grieta
T_SENT = T_DROP + N_R * B_R         # la Sentencia
T_FEAT = T_SENT + 4 * B_J           # cooperativo, nube, actualizaciones (Jefe)
T_FIN = T_FEAT + 8 * B_B            # final (Grieta, compases 8-13)
T_GOLPE = T_FIN + 6 * B_R           # golpe final
T_TOTAL = T_GOLPE + 5.5             # y se queda con los créditos
def r(bar): return T_DROP + bar * B_R          # inicio del compás `bar` de la Grieta
def f(bar): return T_FEAT + bar * B_B          # del Jefe
def e(bar): return T_FIN + bar * B_R           # del final

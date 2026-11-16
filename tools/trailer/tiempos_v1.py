# tiempos_v1.py — la escaleta del tráiler de la 1.0 (~39 s), cuadrada con la música del juego.
B_J = 60 / 140 * 4      # compás de «La Sentencia» (140 BPM)
B_R = 60 / 164 * 4      # compás de «Grieta» (164 BPM)
T_DROP = 4 * B_J                    # 6.857: entra la Grieta
N_R = 15                            # compases de Grieta del cuerpo
T_FIN = T_DROP + N_R * B_R          # final (Grieta, compases 8-11)
T_GOLPE = T_FIN + 4 * B_R           # golpe final
T_TOTAL = T_GOLPE + 4.6             # y se queda con los créditos
def r(bar): return T_DROP + bar * B_R          # inicio del compás `bar` de la Grieta

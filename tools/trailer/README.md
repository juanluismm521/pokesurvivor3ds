# Tráiler (TikTok, 1080x1920, 60 fps, ~60 s)

Cómo se hizo `PokeSurvivor3DS_trailer.mp4` (carpeta de trabajo: `/root/trailer`, fuera del repo).

1. **Grabar el juego** en Azahar con `grabar.sh NOMBRE "autotest" MAX_SEG [rom]` (RGB, 60 fps,
   casi sin pérdidas: en `cfg/azahar-emu/qt-config.ini` de la carpeta de trabajo, `[VideoDumping]`
   con `matroska`, `libx264rgb` y `video_bitrate=400000000`, cada uno con `\default=false`; Azahar
   borra las opciones con comas, por eso la tasa alta). Autotest para grabar:
   `"SEG_TOTAL dex 1 DESDE mega nivel=35 megaen=10 [piedra=b] marca"` (reloj adelantado, nivel,
   la piedra a los N s, la segunda mega, y `marca` = posición de tu Pokémon en un cuadradito de la
   pantalla de abajo para que los primeros planos le sigan); `"novedades 6"`; `"nube google -"`
   (QR real); `"actualizar"` con una build `PS_BUILD_N=60` (descarga de verdad la nueva).
   Grupo: `COOP_DUMP=carpeta COOP_MEGA=1 COOP_EXTRA="desde=1300 nivel=40" tools/emu_coop.sh rom 1350 …`
   (el reloj y el nivel van con la sala: `RunConfig::demo_time/demo_level`).
2. `detectar.py` saca el segundo de cada megaevolución; `hoja.py` hace hojas de fotogramas.
3. **Montaje:** `trailer.py` (escenas en `tiempos.py`, cuadradas con los compases de la música del
   juego: Grieta 164 BPM, Sentencia 140, Jefe 150), letras en `estilo.py` (la fuente «Pokemon Solid»
   de dafont, gratuita, no va en git: se baja a `fonts/`; Anton es OFL), consola en `consola.py`.
4. **Sonido:** `audio.py` (música del juego por compases + golpes, subidas y destellos sintetizados).
5. `unir.sh`: vídeo + sonido con compresor y -14 LUFS.

Necesita: ffmpeg, Python con numpy, pillow, scipy (y librosa para medir), Azahar bajo Xvfb.

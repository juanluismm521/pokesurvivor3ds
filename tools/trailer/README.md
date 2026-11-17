# Tráiler (TikTok, 1080x1920, 60 fps, ~60 s)

Cómo se hizo `PokeSurvivor3DS_trailer.mp4` (carpeta de trabajo: `/root/docker/pokesurvivor/trailer`, fuera del repo).

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
5. `unir.sh`: vídeo + sonido con compresor y -14 LUFS. `empalme.sh`: cambiar una escena sin
   montarlo entero (3 trozos y concat sin recodificar). **Ojo con la RAM** (el VPS tiene ~4 GB):
   nunca leer el mismo vídeo por dos ramas de un filtro (guarda cientos de fotogramas 1080x1920),
   ffmpeg con `-threads 2` y dentro de `systemd-run --scope -p MemoryMax=1G`.
6. v2: la consola es la foto real de una New 3DS XL (Antoine Turmel, CC BY 2.0, Wikimedia
   Commons; `consola_real.py`, juego con perspectiva y el reflejo del cristal), el logo 3DS es el
   original de Commons (CC0) en blanco, y las megas se graban sin enemigos (`paz`) esperando
   con la piedra antes de pulsar Y (`megaespera=2.5`).

Necesita: ffmpeg, Python con numpy, pillow, scipy (y librosa para medir), Azahar bajo Xvfb.

7. v3 (1:08,6): las megas quietas mirando abajo (`quieto`: hasta 3 s después de megaevolucionar) y un compás más de Mega-Darkrai andando.

8. **Tráiler de la 1.0** (39 s): `trailer_v1.py` (escaleta en `tiempos_v1.py`, banda en `audio_v1.py`), con las
   grabaciones `v1_*`: Eternatus (`"1300 6 1 1188 nivel=45 marca"`), los poderes (`"95 94 1 60 nivel=35
   poder=dinamax poderen=7 marca"`: el poder salta a los 7 s de partida, para que se vea; desde el minuto 1
   para no caer en una grieta), `pesadilla`, `semanal`, `tienda` y `gacha100`. Salida
   `PokeSurvivor3DS_trailer_v1.mp4` (vídeo + `banda_v1.wav` como en `unir.sh`).
   **En inglés:** `tanda_v1_en.sh` graba las mismas tomas con el juego en inglés (`idioma.txt` en la SD) como
   `en_v1_*`; con `TRAILER_LANG=en`, `fuente.py` usa esas tomas (con su desfase, `EN_DESFASE`) y los rótulos
   salen en inglés → `PokeSurvivor3DS_trailer_v1_en.mp4`.

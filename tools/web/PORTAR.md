# Portar una actualización de la web al port de 3DS

Guía para quien porte (una persona o alguien lanzada a mano)
una versión nueva de la web cuando `tools/web/actualizar.sh` dice **portar**.
Las normas de `las normas del proyecto` siguen mandando: español, sin firmas de IA en los
commits, `reference/web` sólo se lee, y nada de la 3DS en `src/core`.

## 0. Punto de partida
- Rama aparte (`web/<commit>`), nunca directamente en `main`.
- `tools/web/actualizar.sh aplicar` ya ha copiado la web nueva a `reference/web`
  (y `reference/web/UPSTREAM` dice a qué commit). La versión anterior se ve con
  `git -C tools/cache/web show <commit_viejo>:js/...`, y el cambio exacto con
  `git -C tools/cache/web diff <viejo> <nuevo> -- js/...`.
- **`build/web/informe.md`** es la lista de tareas: commits, archivos, funciones
  nuevas, cambiadas y quitadas (con su línea), campos y módulos nuevos (`G.*`) y lo
  que `gen_data` no sabe leer.

## 1. Datos primero
Si el informe trae «Lo que gen_data todavía no sabe leer» o campos nuevos en la
forma (`G.DEX[].pa`, `G.Moves.ALL[].cat`...):
1. Enseña a `tools/gen_data.js` a sacarlos y añádelos a `src/core/gamedata.h`
   (estructuras y enumerados en el mismo orden que la web).
2. Las comprobaciones de `gen_data` (familias, campos, listas de mejoras...) se
   actualizan a la versión nueva: son las alarmas para la próxima vez.
3. `node tools/gen_data.js --informe` no debe dar ningún problema.

## 2. Lógica, función a función
Por cada archivo de «Lógica que hay que portar»:
1. Lee la función en la web nueva **y el diff** (no sólo el resultado): casi
   siempre el cambio es pequeño dentro de una función grande.
2. Pórtala al archivo C++ que dice el informe (`tools/web/mapa.json`), con el
   mismo nombre que en el JS en los comentarios, como el resto del port.
3. Igual que la web en fórmulas, tiempos y probabilidades. Diferencias
   obligadas por el port, sin excepción:
   - paso fijo de 60 Hz y azar con semilla (`Rng` de la partida, nunca `rand()`),
   - sin memoria dinámica en la partida (pools de tamaño fijo, topes de la web),
   - el núcleo no dibuja ni lee botones (entra `Input`, sale estado),
   - cooperativo en lockstep: todo lo que cambie la partida tiene que salir igual
     en todas las consolas (nada de relojes reales ni del orden de punteros).
4. Las funciones nuevas de dibujo (`draw...`) van a `src/platform/3ds/render.cpp`;
   las de interfaz web (`js/ui/...`) sólo si traen algo jugable (una mejora nueva
   en la tienda, un ajuste...): se hace a la manera de la 3DS en `app.cpp`.
5. Archivos «sin port todavía»: un sistema nuevo de la web. Créalo en `src/core`
   (y en `CMakeLists.txt` y el `Makefile` si hace falta) siguiendo `mapa.json`, y
   añade su regla a `tools/web/mapa.json`.

## 3. Casos dorados
Cada fórmula nueva o cambiada (daño, recargas, probabilidades, escalados...) se
comprueba contra el JS: añádela a `tools/golden/golden.js` (ejecuta el JS
original) y a `tests/test_golden.cpp` (compara el C++). Si un dorado viejo deja
de cuadrar con la web nueva, es que hay que portar algo más: no se toca el
dorado, se arregla el port.

## 4. Pruebas (obligatorias, como en las normas del proyecto)
```bash
tools/web/actualizar.sh pruebas      # make (sin avisos nuevos) + ctest (dorados y simulación con ASan/UBSan)
./build-pc/ps_sim 7 25 1800 1         # partida de 30 min con el bot (topes, NaN...)
tools/emu_run.sh pokesurvivor-3ds.3dsx 30 captures/web 3   # capturas de lo que se vea distinto
```
Mira las capturas. Lo que sólo se puede probar en la consola va a `PROGRESS.md`
> *Para probar en la consola*.

## 5. Cerrar
- `PROGRESS.md`: qué trae la versión de la web, qué se ha portado, pruebas con
  números y lo que falta. Decisiones técnicas en `PLAN.md` > *Decisiones*.
- Si algo de la web no tiene sentido en la 3DS (móvil, ratón, ranking...),
  apúntalo en `PROGRESS.md` en vez de portarlo.
- Commit en español y sin firmas. A `main` sólo con todas las pruebas pasando.

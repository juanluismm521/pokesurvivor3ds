# Seguir a la web: cómo mantener el port al día

*Investigación del 2026-10-19. Las herramientas están en `tools/web/` y el plan
en `PLAN.md` > Fase 11.*

## En resumen
- La web cambia **muy deprisa**: 98 commits en 10 días y 12 commits en los 4
  días desde la copia del port (`5bbc3b2`, 15-oct → `803125d`, 19-oct).
- La mayoría de los cambios que se notan al jugar **tocan lógica** (código), no
  sólo números: ataques con formas nuevas, Ataque/Defensa físicos y especiales,
  mejoras nuevas... Sólo con regenerar datos no basta.
- **Correr el JS de la web en la 3DS no es viable para jugar**: medido con
  QuickJS dentro de la consola emulada, la lógica de la web va de 5 a 10 veces
  más lenta de lo necesario en la New 3DS (y peor en la vieja). Ver *Opción A*.
- Lo elegido: **port nativo + tubería de actualización**. Los datos entran solos.
  La lógica se detecta sola, hasta la función exacta, y se porta. Las pruebas,
  con los casos dorados sacados del JS nuevo, dicen si el port está bien.
  `main` no se toca hasta que todo pasa.
- El juego cruzado con jugadores de la web **es posible sin cambiar la web**
  (con una pasarela en el VPS), pero es un proyecto grande: va después.

## 1. Cómo cambia la web
Clasificando los 45 últimos commits por lo que tocan:

| Tipo de cambio | Ejemplos | ¿Entra solo? |
|---|---|---|
| Datos (números, listas, textos) | Pokédex, movimientos, objetos, mejoras, misiones, logros, traducciones, biomas | **Sí** (gen_data, gen_art, gen_audio) |
| Lógica del juego | `combat.js` (formas de atacar), `enemy.js`, `player.js`, `spawner.js`, `judgment.js`, `db.js`… | No: hay que portarla |
| Interfaz de la web | `menu.js`, `run-ui.js`, `hud.js` | Revisar: la 3DS tiene sus pantallas |
| Sólo web | Firebase, ranking, moderación, CSS, SEO, móvil, mando, PWA, idiomas nuevos | No afecta a la 3DS |

Unos dos tercios de los commits no afectan a la partida (nube, ranking, web).
Pero casi todos los que sí la cambian mezclan datos y lógica en el mismo commit
(p. ej. «38 ataques nuevos» = 236 líneas de `moves.js` + 215 de `combat.js`).

## 2. Opciones estudiadas

### Opción A — Ejecutar el JS de la web en la 3DS (medido: NO para jugar)
La única forma de que la lógica se actualizara sola de verdad. Se probó:
1. `tools/web/navegador.js` + `tools/web/partida.js`: el juego web ENTERO (sus
   71 archivos, sin tocar) corre sin navegador con un bot que juega.
2. Con **QuickJS-ng** (intérprete JS pequeño, sin JIT, como iría en la consola)
   en el VPS: ~2,0 ms de lógica + 0,7 ms de dibujo por fotograma con 64-70
   enemigos; 17 MB de memoria. En node (V8 con JIT): ~1,2 + 0,6 ms.
3. QuickJS compilado para la 3DS (`tools/web/jsbench/`, `jsbench.3dsx`) y
   ejecutado en Azahar (modo New 3DS):

| Momento de la partida | Lógica | Dibujo (sólo el JS) | Total por fotograma |
|---|---|---|---|
| 1-2 enemigos | 26 ms | 10-15 ms | ~40 ms |
| 10 enemigos | 45 ms | 12 ms | ~58 ms |
| 64 enemigos (estimado ×65 del VPS) | ~130 ms | ~45 ms | ~175 ms |

Para 60 FPS hay **16,7 ms** en total (y eso sin dibujar de verdad ni sonido).
Además, cargar el JS tarda 5-6 s y el reloj del emulador cuenta casi una
instrucción por ciclo: en la consola real (cachés pequeñas) iría igual o peor.
**Conclusión:** de 5 a 10 veces demasiado lento en la New 3DS y unas 20 en la
vieja. Se descarta para la partida. (Con `jsbench.3dsx` el dueño puede medirlo
en su consola: ver `PROGRESS.md`.)

Hallazgo útil: QuickJS-ng en ARM con `-O2` necesita `-fno-strict-aliasing
-fwrapv`. Sin eso, el operador `...` da «out of memory».

### Opción B — Traducir el JS a C++ con un programa (descartada)
El JS de la web usa objetos dinámicos, cierres, `Map`/`Set`, propiedades que
aparecen y desaparecen... Un traductor automático daría C++ lento o no
determinista (el cooperativo lockstep necesita que todas las consolas calculen
exactamente lo mismo). Traducir bien cada cambio es justo lo que hace un port.

### Opción C — Port nativo + tubería de actualización (ELEGIDA)
Tres capas, de más automática a menos:

1. **Datos: automáticos.** `gen_data` carga los `.js` de datos de la web en node
   y genera las tablas C++; `gen_art` y `gen_audio` sacan el arte y el sonido.
   Un cambio de equilibrio (daño, vida, probabilidades, precios, textos) entra
   sin tocar C++. Tiene **alarmas**: si la web añade algo que el port no sabe
   leer (una familia de ataques nueva, un campo nuevo), para y lo dice.
2. **Detección de lógica: automática.** `tools/web/informe.js` dice qué
   funciones exactas han cambiado en cada archivo, a qué archivo C++
   corresponden (`tools/web/mapa.json`) y qué módulos y campos nuevos tiene la
   web (`tools/web/forma.js` compara la «forma» de `G.*` entre versiones).
3. **Portar la lógica: con una persona en el bucle.** La hace alguien, o una
   persona que se lance a mano, siguiendo `tools/web/PORTAR.md`
   y la lista del informe. Las pruebas deciden: los casos dorados se regeneran
   del JS nuevo, así que si el port no hace lo mismo que la web, fallan. Encima
   van la simulación de 30 min con ASan/UBSan y el emulador.

   (Se intentó que `auto.sh` lanzara un asistente él solo desde cron, sin nadie
   delante. Los permisos de un asistente no dejan crear eso sin que el dueño lo
   decida, y es razonable: es un agente con acceso total trabajando sin
   supervisión. Ver *Preguntas* en `PROGRESS.md`.)

## 3. Las herramientas (`tools/web/`)
| Archivo | Qué hace |
|---|---|
| `actualizar.sh` | `informe` (por defecto): baja la web a `tools/cache/web` (clon sólo para leer) y escribe `build/web/informe.md`. `aplicar [commit]`: copia la web nueva a `reference/web`, regenera datos/arte/sonido y pasa todas las pruebas. `pruebas`: sólo las pruebas. |
| `auto.sh` + `auto.conf` | Para cron: cada día mira si hay web nueva. `MODO=informe` avisa; `MODO=datos` sube a `main` lo que sea sólo datos con todas las pruebas pasando, y para lo demás prepara una rama `web/<commit>` y avisa. Siempre en un worktree aparte. |
| `informe.js` | El informe: commits, archivos por tipo, funciones cambiadas, forma y lo que `gen_data` no sabe leer. Veredicto `automatico` / `portar` / `nada`. |
| `funciones.js` | Diff por funciones de dos versiones de un `.js` (con acorn), comparando el código propio de cada función. |
| `forma.js` | La forma de `G.*` (módulos, funciones exportadas, campos de los datos) cargando toda la web en un navegador falso. |
| `mapa.json` | Qué es cada archivo de la web (datos, lógica, interfaz, web) y su archivo C++. |
| `navegador.js`, `bot.js`, `partida.js` | El juego web sin navegador, con un bot. Sirve para medir y comprobar que una versión nueva arranca. Más adelante, para comparar partidas web/port. |
| `jsbench/` | El banco de pruebas de QuickJS en la 3DS (Opción A). |
| `PORTAR.md` | La guía para portar una actualización (datos → lógica → dorados → pruebas). |
| `reference/web/UPSTREAM` | De qué commit de la web es la copia del port. |

`gen_data --informe` (nuevo) lista TODO lo que no sabe leer de una vez, en vez
de pararse en lo primero.

## 4. Prueba real: la web de hoy (`5bbc3b2` → `803125d`)
`tools/web/actualizar.sh` sobre los 12 commits pendientes:
- **77 archivos**: 7 de datos, 15 de lógica, 8 de interfaz, 47 sólo web.
- `gen_data` avisa de **8 familias de ataques nuevas** (`homing`, `bounce`,
  `charge`, `spin`, `quake`, `vortex`, `shove`, `stun`), campos nuevos en los
  movimientos (`cat` físico/especial, `charge`, `pull`, `bounce`, `stun`) y una
  mejora nueva (`start`, «Movimiento inicial»).
- `forma.js` destapa lo que **se colaba en silencio**: la Pokédex ganó Ataque,
  At. Esp., Defensa y Def. Esp. (`pa`, `sa`, `df`, `sd`), y `gen_data` los
  ignoraba sin avisar. También salen un módulo nuevo (`G.Stats`) y funciones
  nuevas (`Player#atkFor`, `Player#monoType`, `Enemy#applyStun`…).
- Lista de tareas: en `combat.js`, 9 funciones nuevas (`spin`, `quake`,
  `vortex`, `shove`, `stunZone`, `rebound`…) y 24 cambiadas; en `enemy.js`, 4
  nuevas y 13 cambiadas; también `player.js`, `levelup.js`, `judgment.js`,
  `spawner.js`, `rift.js`, `db.js`…

Veredicto: **portar**. Es la primera actualización que se hará con este método
(Fase 11, B).

## 5. Jugar con jugadores de la web (juego cruzado)
Cómo es el cooperativo de la web (`net.js`, `coop.js`):
- Estrella: el anfitrión habla con cada invitado por **WebRTC** (canal de datos
  entre navegadores, STUN de Google; sin TURN). La sala y la señalización van
  por la **Realtime Database** de Firebase (`rooms/{sala}/q/{uid}`).
- El **anfitrión manda**: enemigos, objetos, XP y reloj. Cada uno manda en su
  Pokémon. Unos 15 mensajes JSON por segundo: el invitado manda su estado y sus
  golpes; el anfitrión manda los enemigos cercanos (`e:[[id,x,y,vida,anim,…]]`).
- La 3DS juega en **lockstep** (todas calculan lo mismo y sólo se mandan los
  botones). Son modelos distintos.

Lo que haría falta, **sin cambiar la web**:
1. **Pasarela en el VPS** (node, con una librería WebRTC libre): entra en la sala
   de la web como si fuera el navegador de ese jugador (con su sesión, la que la
   3DS ya tiene para sincronizar) y pasa los mensajes a la 3DS por el servidor
   de relevo (UDP) que ya existe.
2. En la 3DS, un **segundo modo de cooperativo «como la web»**: de invitado,
   dibujar los enemigos que manda el anfitrión y mandar los golpes; de anfitrión,
   mandar el estado en el formato de la web.
3. **Misma versión**: sólo se puede jugar con la versión de la web a la que
   esté portada la 3DS (mismos ataques e ids). Por eso va DESPUÉS de tener la
   tubería de actualización funcionando.

Coste estimado: varias semanas. Cuidado también con el gasto de la Realtime
Database de la web (la señalización son pocos mensajes, pero es de JPTER78).
Propuesta: Fase 12, cuando el port esté al día con la web.

## 6. Que la consola se actualice sola (propuesta)
El último paso de «cargar automáticamente»: al arrancar, el juego miraría si
hay una versión nueva publicada en GitHub (Releases del repo; ya tiene HTTPS) y
ofrecería «Actualizar». Bajaría el `.3dsx` nuevo a la SD y se reiniciaría.
Junto con `auto.sh`, quedaría: commit en la web → port (solo si son datos) →
versión publicada → la consola se actualiza. Necesita publicar *releases* en
GitHub (Fase 8), así que es decisión del dueño.

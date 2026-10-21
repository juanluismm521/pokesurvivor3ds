# Versiones de PokéSurvivor 3DS

Cada commit con cambios para el jugador sube la versión (`src/core/version.h`):
**parche** (0.2.1) para arreglos, **menor** (0.3.0) para cosas nuevas, **mayor**
(1.0.0) sólo cuando lo decida el dueño. La build (número que sale en
Ajustes → Actualizaciones) es el número de commits de `main`.

## 0.3.3 — 2026-10-21
- **Arreglado el .cia** (instalado en el menú HOME se rompía nada más abrirlo,
  en la New 3DS y en la vieja): le faltaba el permiso para usar la memoria del
  sonido. Hay que instalarlo a mano una vez (el que tenías no llegaba a
  actualizarse); desde ahí se actualiza solo.

## 0.3.2 — 2026-10-21
- **La mega se nota más.** Además de sus estadísticas, mientras dura: +25 % de
  ataque (físico y especial), +15 % de velocidad, recarga de los ataques un
  25 % más rápida, regeneras un 1,5 % de tu vida por segundo y recibes un 15 %
  menos de daño. Sigue durando 20 s con 45 s de recarga. Medido con el bot
  (`ps_balance`): antes la megapiedra rendía menos que la Cinta Elección; ahora
  queda a la par del mejor objeto.
- Textos que se salían de la pantalla: nombres y descripciones de movimientos
  en las cartas de nivel (la descripción larga pasa a dos líneas), en la pausa,
  los vínculos, los atajos, el movimiento inicial y las mejoras. Todos medidos
  con la letra de la consola (autotest `textos`: 1328 textos, ninguno se sale).
- Si el juego se rompe en la consola, guarda en la SD qué pasó (`fallo.log`) y
  hasta dónde llegó al arrancar (`arranque.log`); al abrirlo otra vez lo
  enseña en pantalla. Para encontrar el fallo del .cia.
- (Juego en grupo: protocolo nuevo; actualizad todos.)

## 0.3.1 — 2026-10-21
- Si cambias la megapiedra por la otra (de la X a la Y) con la mega puesta, la
  mega se acaba (antes seguías con la X llevando la Y).
- Las megas de tu Pokémon se cargan al empezar la partida: al megaevolucionar
  ya no desaparece un instante mientras carga su sprite.
- (Juego en grupo: protocolo nuevo; actualizad todos.)

## 0.3.0 — 2026-10-21
- **Megaevoluciones** (91, también las de Leyendas Z-A y su expansión: Mega-Darkrai,
  Mega-Zeraora, Mega-Raichu X/Y, Mega-Garchomp Z…).
  - La megapiedra sale como un objeto más (cofres, cofres con candado, jefes)
    y sólo la de tu Pokémon (con Gengar, la Gengarita). Con dos megas
    (Charizard X/Y, Absol/Absol Z…) sale una u otra.
  - Con ella puesta, **Y** megaevoluciona durante **20 s** (estadísticas y tipo
    de la mega) y luego hay **45 s** de recarga. Transformación con luces de
    colores y destello (1,2 s invulnerable), aura mientras dura, y su barra y
    la recarga en la pantalla de abajo.
  - Mejora nueva **Megapiedra** (sólo en los que tienen mega): 10 % de que un
    objeto sea su megapiedra, +6 % por nivel (hasta 40 %).
  - Sprites de SpriteCollab (41 megas, 37 con shiny). Las que aún no tienen
    dibujo usan el de su forma normal, más grande y con aura. Los shinys
    siguen siendo shiny al megaevolucionar.
  - En grupo también: cada uno con su mega.

## 0.2.1 — 2026-10-21
- En el menú, debajo del nombre de tu compañero, su tipo (o sus dos tipos).

## 0.2.0 — 2026-10-21
- Cuenta en el menú: el logo de Google si entraste con Google, «pokesurvivor.com»
  en amarillo y el botón X, que lleva a la cuenta (sincronizar, entrar, salir).
- Con Google, el nombre es tu apodo de jugador de pokesurvivor.com (si lo
  tienes), no el de la cuenta de Google.
- Al coger el primer objeto, eliges si te lo quedas.
- Jugar en grupo: «Crear sala» y debajo las salas, que se buscan solas.
- Créditos: primero el port (juanluismm520) y luego el juego original (JPTER78).
- El botón Y (normal/shiny) del menú ya no se sale de la pantalla.

## 0.1.0 — 2026-10-20 y antes
- El port: el juego entero de pokesurvivor.com en la 3DS, juego en grupo local
  y por internet, sincronización con la web (usuario o Google por QR), datos y
  versiones que se actualizan solos (con vuelta atrás), .cia para el menú HOME.

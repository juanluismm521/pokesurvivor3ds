# Versiones de PokéSurvivor 3DS

Cada commit con cambios para el jugador sube la versión (`src/core/version.h`):
**parche** (0.2.1) para arreglos, **menor** (0.3.0) para cosas nuevas, **mayor**
(1.0.0) sólo cuando lo decida el dueño. La build (número que sale en
Ajustes → Actualizaciones) es el número de commits de `main`.

## 0.16.1 — 2026-10-29
- **Ajustes → Actualizaciones**: el aviso «La versión nueva del juego se busca y se
  baja sola al encender…» ya no se monta encima del último botón.

## 0.16.0 — 2026-10-29
- **En el modo rápido, Arceus se puede derrotar.** Acuden sólo 8 legendarios
  (arriba pone «LEGENDARIOS x/8»); tumbados todos, Arceus **baja la guardia**: sale su
  barra de vida, recibe el daño entero… y se enfurece (ataca más a menudo y corre
  más). Es difícil: tiene mucha vida y, si tardas, su toque acaba siendo mortal.
- **Si le ganas**: «¡HAS DERROTADO A ARCEUS!», la partida acaba en victoria y te da
  2500 Pokémonedas y 3 tickets ×10.
- **Medalla de Arceus** para el Pokémon que lo consiga: en su casilla y su ficha de la
  Colección, junto a su nombre en el menú principal y en tus estadísticas.
- La pantalla de modo y la lista de salas avisan de que en el rápido se puede ganar.
- **El modo normal sigue igual**: Arceus es invencible.
- Novedades con su título: «¡Derrota a Arceus!».
- (Juego en grupo: protocolo nuevo; actualizad todos.)

## 0.15.0 — 2026-10-27
- **La Luminosfera ahora lleva a un tesoro**: al usarla brilla un tesoro escondido a
  cierta distancia; una flecha dorada («TESORO · 25 m») y un punto en el minimapa te
  guían y, al llegar, suelta 2-3 tickets, monedas y a veces un ticket ×10. Si nadie lo
  coge en 2 minutos, se apaga. (Ya no aleja la vista: con el minimapa no hacía falta.)
- **Al usar una esfera, sabes lo que hace**: un aviso grande («¡Focosfera! +25 % de
  crítico durante 15 s») y, mientras dura, una línea en la pantalla de abajo con el
  tiempo que le queda.
- **Los avisos del centro ya no se salen de la pantalla**: si son largos se parten en
  dos líneas (con una banda oscura detrás para leerlos bien).
- **Gacha**: el cartel de cada banner enseña sus legendarios de verdad (antes colaba a
  Dragonite, Tyranitar, Slaking…), y con el Amuleto Iris pone «Shiny 2 %».
- Las esferas van aparte de los objetos: no cambian la probabilidad de que un objeto
  sea tu megapiedra (sigue igual, con su prueba).
- Novedades con su título: «¡Esferas y tesoros!».
- (Juego en grupo: protocolo nuevo; actualizad todos.)

## 0.14.0 — 2026-10-27
- **El juego busca la versión nueva cada vez que lo abres** (antes, sólo si activabas
  «Buscar solas al encender» o desde Ajustes). Se baja sola mientras juegas y se
  instala al salir.
- **Barra de actualización en el menú principal**: «Buscando versión nueva…», «Bajando
  0.14.1 · 45 %» con su barra de progreso y «¡0.14.1 lista! Al salir» en verde.
  Tocándola o con **START** vas a Actualizaciones (y B te devuelve al menú).
- La opción de Ajustes (antes «Solas») queda para los datos de equilibrio de la web.

## 0.13.4 — 2026-10-26
- **Arreglado «La nube ha rechazado la partida (permiso denegado)»** al entrar con
  la cuenta de pokesurvivor.com. Desde el 21-10 la web cuenta las versiones de la
  partida en la nube y la 3DS no lo hacía: en cuanto alguien guardaba desde la web
  nueva, la consola ya no podía subir su partida. Ahora la 3DS lo hace igual que la web.
- Si se acaba de guardar desde otro sitio, la espera de 20 s lleva un margen (a veces
  se volvía a intentar justo antes de tiempo y fallaba).

## 0.13.3 — 2026-10-26
- Por dentro: al pedir actualizaciones, la consola dice su versión, build y si es
  .cia o .3dsx (nada personal). Es para las estadísticas privadas del dueño: cuántas
  consolas lo usan y en qué versión.

## 0.13.2 — 2026-10-26
- **Novedades** al día con todo lo de la 0.6 a la 0.13 (esferas, Arceus, gacha ×100
  y Amuletos Iris, modo rápido y salas con contraseña, resumen, megas Z y los ataques
  en la Colección). Se abren solas una vez.

## 0.13.1 — 2026-10-26
- Gacha: al cerrar una tirada ya no queda elegido el botón de tirar. Pulsando A
  seguido para pasar las animaciones se encadenaban tiradas sin querer (tickets
  gastados); ahora hay que elegir otra vez con la cruceta o tocar (arreglo de la web).
- Juego en grupo: lo que manda cada consola (mejoras, movimientos elegidos) se
  limita a lo que puede ser, para que un juego trucado no pueda romper la partida
  de los demás (arreglo de la web).

## 0.13.0 — 2026-10-26
- **Modo rápido**: Jugar → eliges **Normal** (la Sentencia de Arceus a los 30 min)
  o **Rápido** (a los 20 min), con lo que es cada uno. Se acuerda del último.
- **Salas del grupo con modo y contraseña**: al crear una sala (cerca o por
  internet) eliges el modo y, si quieres, una contraseña (se escribe con el
  teclado de la consola). En la lista de salas sale un **candado** si la tiene y
  «Rápido» si es de ese modo; para entrar hay que escribirla (si no coincide, el
  anfitrión no te deja entrar y se te dice).
- (Juego en grupo: protocolo nuevo; actualizad todos.)

## 0.12.0 — 2026-10-26
- **Gacha ×100**: con 100 tickets o con 10 tickets ×10. Son diez ×10 seguidas (cada
  una con su Épica asegurada): la tragaperras gira 10 veces enseñando lo mejor de
  cada ×10 y al final sale un resumen (nuevos, shiny y Pokémonedas) con las cartas
  de mejor a peor (arriba/abajo para verlas todas). Como en la web, ya no hay tirada
  ×1: se tira de 10 en 10 o de 100 en 100.
- **Amuletos Iris**: uno por región. Al completar la Pokédex de una región
  (legendarios incluidos) tienes su Amuleto Iris y en su banner los shiny salen
  el doble (del 1 % al 2 %). El gacha y la Colección dicen cuántos te faltan; si
  ya habías completado alguna, el suyo ya es tuyo. Se avisa al conseguirlo.

## 0.11.0 — 2026-10-26
- **Esferas de Mundo Misterioso** (como en la web del 23-10): objetos de un solo
  uso que se encuentran en la partida (cofres, jefes y, a veces, al derrotar a un
  Pokémon). Llevas hasta 4 distintas (3 de cada); están en la pantalla de abajo,
  dos a cada lado de tus ataques.
  - **A** usa la elegida, **B** pasa a la siguiente, y **tocar su casilla** la usa.
  - Soporesfera, Ralentosfera y Rafagaesfera duermen, frenan o lanzan lejos a los
    enemigos (a los jefes y legendarios les afectan menos).
  - Focosfera (+25 % de crítico), Luminosfera (ves más campo: el mundo se aleja)
    y Rapidosfera (+40 % de velocidad) son para ti y los compañeros cercanos.
  - La primera vez que coges cada una en la partida te explica qué hace.
- (Juego en grupo: protocolo nuevo; actualizad todos.)

## 0.10.0 — 2026-10-26
- **La Sentencia de Arceus, como en la web del 23-10:**
  - **Ya se le puede pegar.** Sigue siendo invencible, pero recibe una pequeña
    parte del daño (el 5 %, y cada vez menos: el 1 % a los 10 min). Los ataques le
    apuntan sólo cuando no queda ningún legendario («le protegen» arriba).
  - **Tablas:** en cada partida lleva una al azar, que le da su tipo (se avisa al
    empezar la pelea y sale arriba, con el daño que le lleváis; su aura en el suelo
    es de ese color). Ya no hay un tipo de ataque que sea siempre el mejor.
  - El daño a Arceus sale en el resumen del final.
  - Cada legendario sale con un **25 % más de vida y un 15 % más de daño** que el
    anterior (antes, el doble: no había forma de tumbarlos).
  - Veneno y quemaduras quitan a los legendarios poco y siempre lo mismo
    (envenenar y huir ya no sirve).
  - Nadie se puede salir de la arena (yendo muy rápido se podía y se aguantaba
    para siempre).
- (Juego en grupo: protocolo nuevo; actualizad todos.)

## 0.9.0 — 2026-10-26
- **Colección → SELECT: los ataques del Pokémon**, al doble de tamaño en la pantalla
  de abajo (se tocan o se recorren con la cruceta). Arriba, el elegido: tipo, si
  es físico, especial o una mejora, qué hace, si lo aprende por MT o tutor, si
  empieza con él y en qué evoluciona.

## 0.8.0 — 2026-10-26
- **Resumen al acabar la partida** (como en la web): en la pantalla de arriba,
  cada ataque con el daño que ha hecho y su daño por segundo, el daño total y tu
  objeto. En grupo, el de cada compañero también: **L/R** cambia de jugador.
- La línea «Tiempo · Nivel · derrotados · jefes» del final ya no se sale de la
  pantalla.

## 0.7.0 — 2026-10-26
- **Darkrai aprende más movimientos.** Al ser sólo Siniestro tenía 16 (los 11
  normales y 5 de su tipo); ahora también los que aprende en los juegos por MT o
  tutor: Bola Sombra, Viento Aciago, Psíquico, Rayo Hielo, Rayo, Onda Trueno,
  Residuos y Tóxico (24). Salen en las cartas de nivel y se pueden elegir en
  Repertorio, Atajos y Vetos.
- (Juego en grupo: protocolo nuevo; actualizad todos.)

## 0.6.0 — 2026-10-26
- **Megas más ágiles:** la recarga baja de 45 a **40 s**.
- **Megas Z** (Mega-Lucario Z, Mega-Garchomp Z, Mega-Absol Z…), como en Leyendas Z-A:
  duran **15 s** (5 menos) pero empujan más: +40 % de ataque (antes +25 %), +25 %
  de velocidad, recarga de los ataques un 35 % más rápida, más regeneración y
  aguantan más. La descripción de cada megapiedra dice lo suyo.
- **Mega-Lucario Z con su propio sprite**: el de Mega-Lucario con los colores de la
  forma Z (cian, amarillo claro y negro). SpriteCollab aún no tiene el suyo; cuando
  lo publique, se usará ese.
- (Juego en grupo: protocolo nuevo; actualizad todos.)

## 0.5.1 — 2026-10-22
- **Arreglado el bajón a 4 FPS hacia el minuto 8** (también en grupo). El mapa
  guarda en memoria 160 trozos y no podía soltar los que tenían algo cambiado
  (una roca golpeada, un cofre o un altar usado). Tras unos minutos rompiendo
  cosas por el mapa se llenaba de ellos y el juego regeneraba el terreno sin
  parar en cada fotograma. Ahora esos cambios se guardan aparte y cualquier
  trozo se puede soltar. Medido en el emulador en la misma situación: de 1-2 FPS
  a 60. De paso, en grupo las consolas ya no pueden acabar con el mapa distinto
  por mirar zonas diferentes.
- **El .cia se actualiza bien.** El número de versión que lleva el .cia por
  dentro (el que mira la consola al instalar) salía mal: la 0.4.3 llevaba un «3»
  y la 0.5.0 un «0». La consola tomaba la 0.5.0 por una versión más vieja y no
  la dejaba instalar. Ahora el número sale de la build y siempre es mayor que el de la
  anterior, así que esta actualización ya se instala aunque la haga
  tu versión de antes. Además, al volver a una versión anterior se borra antes la
  instalada. Si la instalación falla, se reintenta así una vez y el error dice en
  qué paso y con qué código (también en `programa.log`).
- **Vínculo:** al unir dos movimientos, el primero que eliges queda marcado
  («1.º ELEGIDO»). Púlsalo otra vez o pulsa B para quitar la marca.
- **Novedades** al día: la web oficial **pokesurvivor3ds.com**, cómo apoyar el
  proyecto en Ko-fi para mantener los servidores y las actualizaciones, y lo nuevo
  de la 0.5.
- Si la partida va lenta (menos de 30 FPS), el juego apunta en la SD
  (`rendimiento.log`) qué había en pantalla y en qué se fue el tiempo, para
  encontrar el porqué.

## 0.5.0 — 2026-10-22
- **Elige tu segundo movimiento**: con la mejora «Repertorio» comprada, en Mejoras
  pulsa A sobre ella y elige con qué segundo ataque empiezas (o «Al azar», como
  hasta ahora). Se guarda por Pokémon y se sincroniza con pokesurvivor.com.
- (Juego en grupo: protocolo nuevo; actualizad todos.)

## 0.4.3 — 2026-10-21
- (Juego en grupo: protocolo nuevo; actualizad todos.)
- Por dentro: modos de prueba para grabar el tráiler (partida con nivel y reloj
  adelantado, también en grupo).

## 0.4.2 — 2026-10-21
- En el menú, debajo de las megapiedras de tu compañero: la probabilidad de
  que un objeto sea su megapiedra y el nivel de la mejora «Megapiedra».

## 0.4.1 — 2026-10-21
- **Cada megapiedra con su dibujo de verdad** (Charizardita X azul y negra, la
  Y naranja, Gengarita, Lucarita…), en la partida (suelo, elección de objeto y
  barra de abajo), en el menú, en Mejoras y en Novedades. Rayquaza, como en
  los juegos, megaevoluciona con el **Meteorito**.
- Las megapiedras se llaman como en los juegos (Blastoisita, Alakazamita,
  Lucarita…; antes muchas salían como «Megapiedra de …»).

## 0.4.0 — 2026-10-21
- **Novedades**: pantalla nueva con lo último del juego (SELECT en el menú; se
  abre sola la primera vez). Arriba, un desfile de megas con su aura; abajo,
  qué hay de nuevo.
- En el menú, si tu compañero puede megaevolucionar, abajo a la derecha salen
  sus megapiedras (una por mega: Charizard, Mewtwo, Raichu, Absol, Garchomp y
  Lucario tienen dos).

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

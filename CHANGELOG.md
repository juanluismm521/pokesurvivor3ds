# Versiones de PokéSurvivor 3DS

Cada commit con cambios para el jugador sube la versión (`src/core/version.h`):
**parche** (0.2.1) para arreglos, **menor** (0.3.0) para cosas nuevas, **mayor**
(1.0.0) sólo cuando lo decida el dueño. La build (número que sale en
Ajustes → Actualizaciones) es el número de commits de `main`.

## 1.0.0 — en preparación (sin publicar)
**La Gran Actualización de pokesurvivor.com llega a la 3DS** (con permiso de su autor): la 1.0.
- **Se sube de nivel como antes** (3 veces más despacio que en la web nueva): menos pantallas de
  cartas y cada una cuenta más. Para que la partida siga igual de dura, los enemigos, los jefes y
  Eternatus tienen menos vida (medido con el bot, en los dos modos).
- **Tus pasivas a la vista:** abajo, sobre el minimapa, una chapita por cada carta pasiva que
  llevas (daño, crítico, velocidad, recarga…) con cuántas veces la has cogido (dorada si ya está
  al máximo).
- **Formas regionales en su sección de la Colección**, después de las 9 generaciones: por
  regiones (Alola, Galar, Hisui y Paldea), cada una con su color.
- Los Pokémon cuyo sprite no tiene animación de reposo (Miraidon y otros) ya no se ven
  congelados: se mueven con su animación de andar.
- **Menú principal renovado** (pantalla de abajo): tus números en tarjetas (runs, mejor tiempo,
  mejor nivel, Pokémon), monedas y tickets en cápsulas y botones con su icono y su color.
- **Al vencer a Eternatus con tu megapiedra puesta** puedes elegir **Megaevolución +**: tu mega dura
  10 s más, se recarga 5 s antes y pega un 10 % más (medido con el bot: vale lo mismo que llevar la
  mega y además otro poder, que también se puede elegir). Sin la piedra, la Megaevolución de
  Eternatus. Las megapiedras se siguen encontrando como siempre (con la mejora «Megapiedra»).
- Abajo, junto al ataque elegido, su categoría: físico, especial o de estado.
- **Arreglos:** la Tienda Kecleon volvía a enseñar las MT de hoy (con 92 MT se borraba la lista);
  la Pokédex enseña el número de verdad de las formas regionales y las ordena al final de su
  región; las cartas pasivas nuevas (Golpe Brutal, Esquiva, Poder rápido) tienen su icono; las
  descripciones largas (Protección) ya no se salen de la carta; en las arenas de Eternatus y de la
  Sentencia el minimapa dice dónde estás (antes, «Grieta»); Eternatus llega sin tirón.
- **Pantalla táctil renovada:** insignia de nivel con el color de tu Pokémon, sus tipos, la vida
  en verde, amarillo o rojo según lo que te queda (y latiendo cuando estás en las últimas), el
  reloj en su cápsula, el minimapa enmarcado, los ataques en tarjetas del color de su tipo y las
  cartas de nivel con su color y la elegida brillando.
- **Los 1025 Pokémon:** llegan los 59 que faltaban (Miraidon, Zarude, Pecharunt…).
- **57 formas regionales** como Pokémon nuevos: Vulpix de Alola, Meowth de Galar, Growlithe de
  Hisui, Wooper de Paldea… Salen en el gacha y en la Colección.
- **46 megaevoluciones estrenan sprite propio** (Venusaur, Blastoise, Gyarados, Garchomp…); antes
  se veían como su forma normal.
- **73 ataques nuevos** (176 en total), con 14 formas de atacar nuevas: Terremoto, Surf,
  Llamarada, Trueno, los puños elementales, Tijera X, Velocidad Extrema, Acróbata, Protección,
  Descanso, Reflejo y Pantalla Luz, Doble Equipo, Explosión, Día Soleado, Danza Lluvia…
- **Ataques exclusivos:** 39 Pokémon tienen el suyo (Tacleada de Voltios, Onda Certera, Sentencia
  de Arceus…): su carta dorada sale desde el nivel 10.
- **Eternatus Eternamax, el jefe del minuto 20** (en el modo rápido, del 13): el cielo se tiñe de
  rojo y te lleva a su arena. Tres fases: Despertar (Rayo Infinito, Bomba Lodo con charcos de
  Tóxico, Energía Dinamax), Escudo Dinamax (es inmune hasta que rompes 4 núcleos unidos por una
  barrera de láseres que se apaga a ratos, y las nubes rojas cierran la arena) y Eternamax
  Furioso (Espiral Infinita, Noche Oscura, Remolino Infinito, que te arrastra) y, al final, el
  Rayo Eternamax: rompe el Núcleo Eterno a tiempo y queda aturdido; si no, ¡a los círculos! Si
  caes, se acaba la partida; si ganas, ticket ×10, 600 monedas y un poder.
- **Los legendarios de la grieta pelean de verdad:** tres fases (a 2/3 de vida sacan todo su
  poder y a 1/3 se enfurecen), cinco o seis ataques que se mueven por la arena según sus tipos
  (Llamarada, Surf, Psíquico, Ventisca, Bola Sombra…) y el suyo propio en los más famosos
  (Aerochorro de Lugia, Fuego Sagrado de Ho-Oh, Onda Mental de Mewtwo…). En la tercera fase, su
  ataque definitivo: ¡escóndete detrás de una columna, un árbol o una roca! Las grietas se abren
  ahora a su hora (minutos 4, 8, 12, 16 y 25; antes en el modo rápido) y la vida del legendario
  se ajusta al daño que le haces. La última sólo da vínculo si te dejaste alguna.
- **Poderes** para el resto de la partida tras vencer a Eternatus (se usan con Y): Megaevolución
  sin megapiedra (si tu Pokémon tiene mega), Dinamax (gigante: doble de vida, +40 % de daño y
  aplastas lo que pisas), Pulsera Z (una gran Z rueda hacia los enemigos y el movimiento Z
  arrasa la pantalla) y Teracristal (tus ataques de tu tipo pegan un 80 % más).
- **Tres dificultades, en los dos modos:** Normal, Difícil (enemigos con ×1,4 de vida y ×1,25 de
  daño; ×1,5 monedas y tickets) y Pesadilla (×2 de vida, ×1,5 de daño; ×2 de premio). Se eligen en
  Jugar con izquierda/derecha; vence a Eternatus en una para desbloquear la siguiente (se
  sincroniza con la web).
- **Flechas de los jefes:** cuando no los ves, una flecha en el borde dice dónde están y a
  cuántos metros (también los núcleos de Eternatus).
- **Equilibrio nuevo, el de la web:** se sube de nivel 3 veces más rápido, los enemigos y los
  jefes aguantan según el minuto de la partida, los jefes llegan en los minutos 2, 6, 10, 14 y
  18, las mejoras de nivel se suman en vez de multiplicarse, las mejoras permanentes dan menos
  y un golpe nunca te quita más del 30 % de la vida.
- **Siempre 4 cartas al subir de nivel**, como en la web (antes 3). La mejora del port «Más
  pasivas a niveles altos» se queda con lo suyo: desde el nivel 25 (o con 8 movimientos) al menos
  2 de las 4 son pasivas, y desde el 40, al menos 3.
- **Cartas de nivel nuevas:** Ojo Crítico, Golpe Brutal, Cazador (más daño a jefes), Esquiva,
  Racha de suerte, Bolsa de monedas y Poder rápido (menos recarga de la megaevolución).
- **Vínculos:** los movimientos unidos ya no pegan cada uno entero: entre todos hacen ×1,6
  (dos), ×2 (tres) o ×2,3 (cuatro).
- **Crítico con tope:** 60 % de probabilidad y ×3,5 de daño como mucho.
- **Modo rápido:** todo llega antes en la misma proporción (y la experiencia también), para
  que siga siendo la misma partida en menos tiempo.
- 92 MT en la Tienda Kecleon (antes 58); las 8 del día son las mismas para todos.
- **Evolución en plena partida, en proporción a los niveles de los juegos:** como aquí se sube de
  nivel más despacio, Charmander evoluciona al 10 y Charmeleon al 22 (16 y 36 en los juegos), en
  el mismo momento de la partida que en la web. Los que evolucionan con piedra, intercambio o
  amistad, al 12 y al 25. Eliges evolucionar (cambian su aspecto, sus tipos y sus stats base, que
  pasan a ser los de la evolución) o mantener su aspecto (y sus stats). Eevee, entre 3 de sus
  evoluciones al azar. Todos empiezan con la fuerza del primero de su línea (Charizard, con la de
  Charmander) y la ganan al subir; los que no evolucionan, al 65 %, y suben en los niveles 12 y 25.
- **La Sentencia de Arceus, en la Sala del Origen:** un suelo de mármol con anillos dorados
  flotando entre las nubes del cielo. Arceus aprende ataques nuevos según dura la pelea:
  Velocidad Extrema (cruza la sala embistiendo), Premonición (donde estabas hace un momento),
  Gravedad (todos más lentos y una lluvia de pilares), Hiperrayo (16 rayos anchos en abanico) y
  Sentencia de las Tablas (espirales de tablas de todos los tipos). A los 5 y 7 minutos de pelea
  giran por la sala las **Aspas Doradas**: ¡muévete por los huecos! Como mucho hay 30
  legendarios a la vez en la sala. En el modo rápido Arceus se puede vencer como siempre.
- **Maestría Pokémon:** cada partida suma puntos al Pokémon con el que juegas (1 por cada 6
  derrotados y 25 por minuto aguantado). A los 10 000, 25 000, 50 000 y 100 000 llegan la foto, el
  título «Maestro de…», el marco y el **aura de maestro** (destellos dorados en la partida). Se ve
  en la ficha de la Colección y en el resumen, y se sincroniza con la web, junto con las
  estadísticas de cada Pokémon.
- **Desafío semanal** (en Jugar): cada semana, todos con el mismo Pokémon, el mismo mapa y una
  condición: siempre llueve, sol abrasador, niebla, nevada, tormenta de arena, enemigos veloces,
  cañón de cristal, sin curaciones, noche cerrada o todos de un tipo (y a veces sin objetos).
  Es el mismo que el de la web. Un intento por semana (también cuenta si lo jugaste en la web)
  y, si aguantas 3 minutos, un ticket ×10.

## 0.50.6 — 2026-11-14
Sincronización con pokesurvivor.com al día con su Gran Actualización (13-11):
- **Lo nuevo de la web ya no se pisa desde la consola:** maestría, huevos, estelas, fotos y
  marcos, la foto y el marco del perfil, la dificultad, el Vestidor y la Pokédex de vistos se
  quedan como los tengas en la web (antes, al sincronizar desde la 3DS podían volver a como
  estaban la última vez).
- Una consola recién estrenada ya no borra las misiones del día de la web ni cambia el nombre
  de tu cuenta a «Jugador» (si te pasó, al sincronizar vuelve tu nombre).
- Las MT que borres en la web con la MT vacía ya no vuelven al sincronizar.

## 0.50.5 — 2026-11-08
(Lleva también lo de la 0.50.1, que no llegó a publicarse.)
- Mientras sincroniza, debajo de «Conectando con pokesurvivor.com…» pone que puede tardar un
  poco (con algunas redes va por el puente de nuestro servidor y la primera vez tarda más).
- **Entrar con Google:** el código de la consola sólo lleva letras y cifras que no se confunden
  en su letra (fuera 0/O/C/D, 6/G, W/H, U/V, 1/I/L, 2/Z, 5/S, 8/B y 9) y sale partido en dos
  (KH4P-H7MJ). La página
  a la que lleva el QR sale en el idioma del juego (español o inglés).
- Tienda Kecleon: el nombre de la Cápsula Habilidad Oculta ya no se monta encima de su
  descripción (sale en letra normal, en dos o tres líneas), y la Cápsula dice claro qué hace
  («Cambia la habilidad, incluida la oculta» / «…incluida su habilidad oculta»).
- Si la sincronización falla, el mensaje entero (con qué servidor y por qué) se ve en dos líneas.

## 0.50.1 — 2026-11-08 (sin publicar: va en la 0.50.5)
- **Sincronizar: si falla la conexión, ahora dice con qué servidor y por qué** (en vez de sólo
  «Sin conexión a internet»), y lo apunta en `nube.log` en la SD (cada petición: servidor, resultado,
  lo que tardó y los bytes; sin contraseñas ni claves). Así se puede ver qué pasa en cada consola.
- **Si tu consola no llega al servidor de la partida de pokesurvivor.com** (pasa con algunas redes:
  se quedaba esperando y salía «Sin conexión a internet»), ahora va por un puente de nuestro
  servidor, al que sí llega, y lo recuerda para las siguientes veces.
- Más aguante al sincronizar: hasta 60 s por petición (antes 25; con mucha partida y una wifi lenta
  no llegaba), un reintento si la conexión se corta y, si tu red no encuentra el servidor de la
  partida de pokesurvivor.com, se prueba por su dirección directa.

## 0.50.0 — 2026-11-07 · Tienda Kecleon completa
- **¡Abre la Tienda Kecleon!** (como en pokesurvivor.com; R en el menú). Arriba, el puesto de
  los dos Kecleon (el verde y su hermano morado), que dan las gracias y saltan al comprar.
  Abajo, dos pestañas (L/R):
  - **Comprar**: Menta (1500), Cápsula Habilidad (1500), Cápsula Habilidad Oculta (5000) y
    **8 MT nuevas cada día**, las mismas que en la web (cambian a medianoche, hora de España;
    se ve cuánto falta). Si ninguna le sirve a tu compañero, una de las 8 sí. Cada MT dice
    si tu compañero la puede aprender y cuántos de tus Pokémon la pueden aprender.
  - **Mochila**: lo comprado; con A, a la ficha de tu compañero para usarlo.
  - En el menú, un punto rojo en la tienda cuando hay MT nuevas sin ver.
- **Cada Pokémon tiene su naturaleza y su habilidad**, las mismas que en la web (salen de tu
  cuenta y del Pokémon: en la consola y en la web te tocan las mismas). La naturaleza sube un
  10 % un stat y baja otro (5 son neutras). La habilidad hace cosas en la partida, como en la
  web: más daño con los de un tipo cuando vas mal de vida, esquivar golpes, curarse con la
  lluvia, traer el sol, quemar a quien te toca… (las demás, +8 % con los ataques de su tipo).
  La oculta sale 1 de cada 20. En grupo, cada uno juega con las suyas.
- **Ficha de cada Pokémon** (Colección → SELECT), con pestañas (L/R): Ataques, Naturaleza
  (las 25, con lo que sube y baja; con una Menta se cambia), Habilidad (las suyas, qué hace
  cada una; con una Cápsula se cambia, la oculta con la Oculta) y MT (las que aprende en los
  juegos: las aprendidas y las que tienes para enseñarle). Las MT aprendidas son para siempre:
  salen en sus cartas de nivel y se pueden elegir de ataque inicial y en Repertorio.
- La Colección enseña la naturaleza y la habilidad de cada Pokémon (y sus stats con ella); al
  conseguir uno nuevo en el gacha se ve cuáles le han tocado; en la pausa, tu habilidad.
- **Disfraces con X** en la Colección (antes START; el filtro pasa a START). Los caramelos y
  los disfraces de bruja sólo se ven en Halloween.
- La Mochila, las MT, las naturalezas y las habilidades se sincronizan con pokesurvivor.com
  (en la libreta, como las monedas: nada se duplica ni se pierde).
- **12 ataques de un solo disparo** (Bomba Germen, Energibola, Bola Voltio, Bola Neblina, Bomba
  Ácida, Doble Rayo, Joya de Luz, Canto Helado, Sombra Vil, Beso Drenaje, Disparo Espejo y Golpe
  Bajo): con «+1 proyectil», los disparos se abren en abanico (como en la web) en vez de salir uno
  encima del otro.

## 0.35.3 — 2026-11-06
- **Sincronizar con pokesurvivor.com vuelve a funcionar.** La web ha dejado de guardar las
  partidas directamente en su base de datos y ahora lo hace a través de su propio servidor
  (ice.pokesurvivor.com). La 3DS seguía usando el camino de antes y la nube la rechazaba
  («La nube ha rechazado el acceso (permiso denegado)»). Ahora la consola guarda y recupera
  la partida por el servidor nuevo, con permiso del creador de la web, igual que la web:
  si otro aparato ha guardado entre medias, se juntan las dos partidas sin perder nada, y
  si se acaba de guardar, espera lo que diga el servidor. Lo que el servidor corrige al
  guardar (monedas, tickets…) queda también en la consola.
- Si pokesurvivor.com pide comprobar que no eres un robot (la consola no puede enseñar esa
  prueba), la 3DS lo explica: entra con tu cuenta en pokesurvivor.com, pulsa Guardar y
  vuelve a sincronizar.

## 0.35.2 — 2026-11-02
- Arreglos internos.

## 0.35.1 — 2026-11-01
- **Al ganar a Gengar/Arceus en el modo rápido, el juego ya no se cierra** (en grupo a un
  invitado, y en Reddit también al ganar solo). Si el golpe final era un disparo, el juego
  vaciaba la lista de disparos mientras la estaba recorriendo y la dejaba con -1
  elementos; al dibujarlos, la consola leía memoria que no era suya y se rompía («data
  abort» en Renderer::draw_top, fallo.log de la build 126). Ahora esa limpieza se hace al
  final del paso y las listas no pueden quedar nunca por debajo de 0.
- **En grupo, el cofre grande se abre aunque haya otro jugador cerca.** Se miraba jugador
  por jugador: el que estaba a su lado lo iba abriendo y el que estaba un poco más lejos lo
  iba cerrando (más deprisa), así que no se abría nunca. Ahora se abre si hay alguien al
  lado (al mismo ritmo, haya uno o cuatro) y sólo se cierra si no hay nadie.
- En grupo, los invitados veían 0 caramelos en su pantalla (en su partida guardada sí se
  sumaban): ahora ven los del equipo.
- Pruebas: dos consolas en modo rápido hasta ganar no se separan; autotest «real» (con la
  partida guardada) y «menu» (tras el resumen, al menú); «ganar» también en grupo.

## 0.35.0 — 2026-11-01
- **Actualizaciones exprés: sólo se baja lo que cambia.** Antes cada versión nueva se
  bajaba entera (≈40 MB, casi todo sprites y música que no cambian). Ahora el servidor
  publica también un parche contra las últimas versiones y la consola rehace la nueva
  con la que ya tiene en la SD (el .3dsx que usas, o la copia del .cia instalado): de
  la 0.26.1 a la 0.26.2 habrían sido 1,3 MB del .cia y 0,2 MB del .3dsx. Lo que sale se
  comprueba con la huella firmada; si algo no cuadra, se baja entera como siempre. Se
  nota desde la siguiente actualización a esta.
- Novedades: «Versión 0.35 · Actualizaciones exprés» (y los arreglos de Reddit de la
  0.26.3); de ahí, directamente a la 0.26 de Halloween, sólo con lo de Halloween.
- Si salías del juego mientras buscaba versiones nuevas, la que ya estaba descargada no
  se instalaba hasta la vez siguiente: ahora se instala.

## 0.26.3 — 2026-11-01
- **No se pierde lo de una partida al sincronizar con pokesurvivor.com** (Reddit: «encontró
  un shiny, lo derrotó y no se lo dio»). La sincronización que empieza al acabar una
  partida se hacía con la partida de ese momento; si volvías a jugar en seguida y
  terminaba mientras, al acabar la partida nueva pisaba lo ganado en ella (shinies,
  caramelos, tickets...). Ahora se vuelve a juntar con lo de esa partida.
- **«+1 proyectil» ya dispara uno más** en Bola Neblina, Energibola, Canto Helado, Sombra
  Vil, Beso Drenaje, Golpe Bajo, Bola Voltio, Bomba Germen, Bomba Ácida, Rayo Señal, Joya
  de Luz y Disparo Espejo: esos ataques no decían cuántos disparan y la mejora sumaba a
  cero (también pasa en la web).
- Modo rápido: en la Sentencia (el gran Gengar del minuto 20), la velocidad baja menos.
  El tope (contra la huida sin fin de un Arceus invencible) quitaba un 30-50 % con un
  Pokémon rápido; ahora lo que pasa de 170 cuenta la mitad. El modo normal, como la web.
- Halloween: al llegar al gran Gengar se ve «+60 caramelos», y los caramelos que quedaban
  en el suelo ya no se pierden con el viaje a su arena.

## 0.26.2 — 2026-10-31
- **Los datos del juego van con cada versión** (decisión del dueño): ya no se bajan
  aparte de la web (fuera «Datos de la web, solos» de Ajustes y Actualizaciones). Al
  arrancar se borra el paquete de datos que quedara en la SD, así que todos los que
  tienen la misma versión tienen los mismos datos. Cuando la web oficial tenga datos
  nuevos, el dueño decide si van en la siguiente versión. (Un paquete viejo del 19-10,
  sólo con 91 traducciones de menos, hacía que dos consolas 0.26.1 no se vieran en
  una sala.)
- Sala: si entra alguien con otra versión del juego (o el anfitrión la tiene), se avisa
  («actualizad los dos») en vez de quedarse esperando sin decir nada.

## 0.26.1 — 2026-10-31
- **Grupo por internet más fluido: el servidor hace de anfitrión.** Antes, los botones
  de un invitado iban al servidor, al anfitrión, otra vez al servidor y al invitado
  (cuatro tramos); el anfitrión no notaba retraso y los invitados, mucho, y a saltos.
  Ahora cada consola manda sus botones al servidor, que junta los de todos y se los
  reparte: dos tramos, igual para todos.
  - El servidor calcula el retraso de cada uno con su ping (su pico reciente) y el del
    peor, más un margen que sube si alguien da un tirón o se queda sin colchón y baja
    si a todos les sobra.
  - **Reloj elástico**: el reloj de cada consola nunca va exacto al de las demás; con
    poco colchón, el suyo va un poco más despacio (como mucho un 10 %, no se nota) en
    vez de quedarse parado.
  - Medido con el servidor de verdad, redes de casa de mentira (ping, vaivén, picos,
    pérdidas) y relojes que no van exactos (ps_red): con dos casas a 60 ms,
    el invitado pasa de ~360 ms de retraso de los controles (a saltos) a ~190 ms, igual
    que el anfitrión (antes 83), y 0 tirones; con wifi regular, de ~500 ms a ~230-280.
  - Si el anfitrión sale de una partida por internet, los demás siguen jugando.
  - Con un servidor viejo, todo sigue como antes (el anfitrión junta los botones).
  - Por internet, menos paquetes: si no hay nada nuevo, la consola no repite el mismo
    envío cada fotograma (cada 100 ms).
- Sala: el modo («Modo rápido (20 min)») en su franja, en letra grande y con candado
  si tiene contraseña (antes, letra pequeña encima del cuarto hueco); en la lista de
  salas, «Rápido» en etiqueta naranja; «Esperando a que el anfitrión empiece…» ya no
  se sale de la pantalla.
- **Si el DNS de casa no encuentra el servidor** (en Reddit: «pokesurvivor3ds.duckdns.org
  can't be found»: hay routers, operadores y bloqueadores que no resuelven los
  dominios duckdns.org o los mandan a 0.0.0.0), el juego llega igual por la dirección
  del servidor: el juego en grupo, las actualizaciones y los datos (por HTTPS, con su
  certificado). Probado en PC con un DNS de mentira que dice «no existe», 0.0.0.0 o una
  dirección muerta (build-pc/ps_dns).
- (Juego en grupo: protocolo nuevo; actualizad todos.)

## 0.26.0 — 2026-10-30
- **¡Halloween!** (del 30 de octubre al 2 de noviembre, hora de España, como en
  pokesurvivor.com):
  - Mapa de noche: Huerto de Calabazas, Cementerio Embrujado y Bosque Tenebroso, con
    calabazas encendidas que se rompen (experiencia, monedas y caramelos).
  - Dos Pokémon Fantasma más en cada tramo de enemigos.
  - **Caramelos**: los sueltan los Fantasma y las calabazas, al ritmo del tiempo que
    aguantas, y 60 de golpe al llegar al gran Gengar.
  - **Tienda Kecleon** (arriba a la derecha del menú, con R; Kecleon con sombrero en
    Halloween y normal el resto del año): disfraces de bruja para Gengar, Pikachu y
    Umbreon por 300 caramelos, para siempre. Se ponen y quitan en la tienda o en la
    Colección (START) y se ven en la partida y en grupo. Próximamente, más cosas.
  - Las calabazas se rompen al pasar por encima, con tus ataques o con disparos
    enemigos.
  - **El gran Gengar bruja** en vez de Arceus en la Sentencia («LA NOCHE DE GENGAR»).
  - Banner **Noche Espectral** en el gacha: los Fantasma de todas las regiones, shiny
    al 2 %.
  - Menú de noche con calabazas y Fantasma paseando, cuenta atrás y **música
    tenebrosa**.
  - Caramelos y disfraces se sincronizan con pokesurvivor.com.
- Mejora nueva **«Más pasivas a niveles altos»** (2500 Pokémonedas, por Pokémon):
  desde el nivel 25 (o con 8 movimientos) eliges entre 4 cartas, y al menos 2 son
  pasivas (Vigor, Potencia, Carrera, Reflejos, Coraza…); desde el 40, al menos 3.
- **Tesoro a contrarreloj**: el de la Luminosfera sale más lejos (4-6 pantallas) y
  dura 40 s (la flecha dice los metros y los segundos; en rojo los últimos 10). Trae
  lo de un cofre grande y 10 monedas extra (y en Halloween, un caramelo).
- Al elegir el segundo movimiento (y el inicial), cada ataque dice si es **Físico o
  Especial**; arriba, lo que rinde con tu Pokémon (su lado fuerte, al 100 %).
- Tus ataques atraviesan los cofres (grandes y pequeños) y ya no apuntan a los
  pequeños.
- Al acabar la partida, «Sincronizando» ya no tapa el resumen (sólo sale en el menú).
- La primera vez, el juego sale en el idioma de la consola (español si lo es; si no,
  inglés). El 3D va apagado de serie.
- Por internet, en grupo, ya no se ve ni se cambia el servidor.
- Novedades: «¡Halloween!», con dibujo propio en cada una.
- (Juego en grupo: protocolo nuevo; actualizad todos.)

## 0.25.0 — 2026-10-30
(Salto de versión a la 0.25, por decisión del dueño.)
- **Curaesfera**: nuevo objeto utilizable que cura un 30 % de la vida a todo el
  equipo, esté donde esté.
- **Región «Todas»** en el gacha (después de Paldea): todas las regiones juntas, con
  su propio pity y su Amuleto Iris (la Pokédex entera).
- Se arreglan ciertos bugs del gacha que no aparecían Pokémon representativos: Kanto
  enseña a Dragonite; Paldea, a Koraidon y Gholdengo (Miraidon no está en el juego).
- **Cofres**: los pequeños se abren al pasar por encima (ya no a golpes ni paran tus
  disparos); los grandes se abren igual, desde un poco más lejos, y no se llevan tus
  golpes.
- El tesoro de la Luminosfera trae lo de un cofre grande (objetos, esferas,
  experiencia) y además tickets y monedas.
- Arreglado: al unir movimientos (vínculo) el ataque elegido ya no cambia solo.
- Novedades: «¡Región Todas!».
- (Juego en grupo: protocolo nuevo; actualizad todos.)

## 0.16.2 — 2026-10-29
- **El juego, en inglés de verdad.** Había unos 600 textos del port sin traducir o a
  medias («Se instala al Log out», «La Save of all time»…): menús, avisos, cuenta web,
  grupo, actualizaciones, esferas, Arceus, novedades… Ahora todo sale en inglés, con
  los nombres oficiales (Shiny Charm, Mega Stone, Flame Plate, Slumber Orb…).
- Créditos en inglés, megapiedras con su nombre inglés (Charizardite X, Gengarite…),
  rasgos de personalidad y los Pokémon paradoja con su nombre inglés (Iron Valiant,
  Great Tusk…).
- Arreglado: los avisos largos del centro se cortaban (el de Arceus en el modo rápido
  perdía «¡aquí se le puede derrotar!» y los de las esferas, el final). También en
  español.
- Arreglado: en los créditos, los nombres de archivo perdían los guiones bajos.

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

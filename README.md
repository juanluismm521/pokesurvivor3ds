# PokéSurvivor 3DS

Port **homebrew** para **Nintendo 3DS** de [PokéSurvivor](https://pokesurvivor.com),
el *bullet-heaven* con Pokémon y sprites de *Pokémon Mundo Misterioso*.

La versión 3DS es **para un jugador y 100 % local** (guardado en la SD): sin
cuentas, sin nube, sin ranking y sin cooperativo (por ahora). La partida, el
equilibrio, los 966 Pokémon y la progresión son los de la web.

> Proyecto de fans **sin ánimo de lucro**, sin relación con Nintendo, Game Freak
> ni The Pokémon Company. Gratis siempre: no se puede vender ni poner anuncios.

## Estado
En desarrollo. Mira [`PLAN.md`](PLAN.md) (qué se hace y en qué orden) y
[`PROGRESS.md`](PROGRESS.md) (dónde estamos).

## Qué hay en el repositorio
| Carpeta | Qué es |
|---|---|
| `reference/web/` | Código original de la web (JS, CSS, `dev.html`, herramientas). **Sólo se lee**: es la fuente de verdad de fórmulas y tiempos |
| `assets-src/` | Sprites PNG originales (`pokemon/`), fuentes e icono |
| `tools/` | Generadores de la web a formatos de la 3DS (node / python) y `golden/` para los casos de prueba |
| `src/core/` | Lógica del juego en C++17, sin nada de la 3DS (se compila también en Linux) |
| `src/platform/3ds/` | Gráficos (citro2d), sonido (ndsp), botones y táctil, guardado en la SD |
| `tests/` | Pruebas con doctest (sueltas, casos dorados y simulación sin pantalla) |

Los archivos generados (`build/`, `romfs/`, `dist/`...) no se suben: se
regeneran con los scripts de `tools/`.

## Instalar en la consola
1. Hace falta una 3DS con *custom firmware* y el Homebrew Launcher.
2. Copia `pokesurvivor-3ds.3dsx` a `sd:/3ds/` y ábrelo desde el Homebrew Launcher.
3. Para el sonido hace falta `sd:/3ds/dspfirm.cdc` (se saca una vez con DSP1).
4. La partida se guarda en `sd:/3ds/pokesurvivor-3ds/save.json` (con copia en
   `save.bak`) y los ajustes en `settings.json`.

## Controles
| Botón | Acción |
|---|---|
| Circle Pad / cruceta | Moverse |
| L / R (o X) | Cambiar de movimiento (o tocar el hueco abajo) |
| START | Pausa (SELECT en la pausa: abandonar la partida) |
| Arriba/abajo + A, o tocar | Elegir carta de nivel, objeto o vínculo |
| Y | Cambiar las cartas de nivel |
| START + SELECT | Salir del juego |

## La misma partida en la web y en la 3DS
**Ajustes → Cuenta de pokesurvivor.com → Entrar** con tu usuario y contraseña
de la web. Se juntan las dos partidas (nunca se pierde nada: Pokémon, shinies,
monedas, tickets, mejoras...) y a partir de ahí se sincroniza sola al encender
y al acabar cada partida (o con «Sincronizar ahora»). En la web, recarga la
página para ver lo de la 3DS. Las cuentas de Google no sirven en la consola.
La contraseña no se guarda en la SD (sólo un token de sesión).

## Jugar en grupo (hasta 4 consolas)
Cada consola necesita el juego. Una entra en **En grupo → Crear sala**; las
demás, en **En grupo → Buscar salas** y eligen esa sala. Cuando estéis, el
anfitrión pulsa **¡Empezar!**. Cada uno lleva su Pokémon (el compañero que
tenga elegido, con sus mejoras); el nivel, la experiencia y las monedas son
del equipo; las cartas salen a la vez y se espera a que todos elijan; si
alguien cae, quédate a su lado 3 s para levantarle. En grupo, START no para
la partida (te quedas quieto); SELECT en la pausa te saca de la partida.

## Compilar
Ver `las normas del proyecto` > *Comandos* (instalación de devkitPro, generación de recursos,
compilación de la 3DS y pruebas en PC).

## Licencias y créditos
- Código: PolyForm Noncommercial 1.0.0 (`LICENSE`, `LICENSE-NONCOMMERCIAL.md`).
- Sprites: PMD Sprite Collab, CC BY-NC 4.0. Todos los artistas en
  [`CREDITS.md`](CREDITS.md); también salen en el juego.
- Fuente: Pixelify Sans (SIL OFL, `assets-src/fonts/OFL.txt`).

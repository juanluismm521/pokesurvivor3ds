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

## Compilar
Ver `las normas del proyecto` > *Comandos* (instalación de devkitPro, generación de recursos,
compilación de la 3DS y pruebas en PC).

## Licencias y créditos
- Código: PolyForm Noncommercial 1.0.0 (`LICENSE`, `LICENSE-NONCOMMERCIAL.md`).
- Sprites: PMD Sprite Collab, CC BY-NC 4.0. Todos los artistas en
  [`CREDITS.md`](CREDITS.md); también salen en el juego.
- Fuente: Pixelify Sans (SIL OFL, `assets-src/fonts/OFL.txt`).

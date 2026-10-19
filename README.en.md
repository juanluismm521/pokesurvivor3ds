# PokéSurvivor 3DS

*[Versión en español](README.md) (main)*

A **homebrew** port for the **Nintendo 3DS** of [PokéSurvivor](https://pokesurvivor.com),
the Pokémon *bullet-heaven* with *Pokémon Mystery Dungeon* sprites. It is the
same game as the web version —all 966 Pokémon, the moves, rifts, Arceus'
Judgment, the gacha, missions and achievements— tailored to the console: two
screens, buttons, touch screen and 3D.

> A **non-profit** fan project, not affiliated with Nintendo, Game Freak or
> The Pokémon Company. Always free: it may not be sold or carry ads.

## Features

### The game
- **The whole web game**, ported to C++ and checked number by number against
  the original code: damage, type chart, experience, enemy spawns, terrain
  painting, moves, missions... (golden tests).
- **Complete menus**: personality test, partner, collection (Pokédex with
  shinies), per-Pokémon upgrades with shortcuts and vetoes, **slot-machine
  gacha** with a region selector (L/R), missions, achievements, stats,
  settings and credits.
- **Top screen**: the game at 400×240, pixel-perfect, with optional **3D**.
- **Bottom screen**: health, level, timer, your moves (tap to switch) and a
  **minimap** with the real terrain, your view, you, your teammates, enemies,
  bosses, chests, altars, the rift, Arceus... **Tap the map** for a legend.
- **Crisp text**: Pixelify Sans for big text and Tiny5 (8 px) for small text,
  always at integer scale.
- **Sound**: the web's music and sound effects, recorded from its synth.
- **Automatic quality**: high on New 3DS, medium on the old 3DS.
- **No hitches**: new Pokémon load in the background.
- **Languages**: Spanish and English (the web's translations).

### Play together (up to 4 consoles)
In **En grupo** (Play together), **L/R** switches between:
- **Cerca** (Nearby): 3DS local wireless, no internet needed.
- **Por internet** (Online): with anyone, through a **relay server** (see
  below). The server is set on the console itself (Y): IP or domain and port.

One console creates a room, the others search and join, and the host presses
**¡Empezar!** (Start). Web rules: more and tougher enemies the more players,
team level and experience, level-up cards appear for everyone at once and the
game waits for all, a downed teammate is revived by standing next to them for
3 s, and the run ends when everyone is down.

Under the hood it is **lockstep**: each console only sends its button presses
(8 bytes per step) and they all compute exactly the same game.

### The same save on the web and on the 3DS
**Ajustes (Settings) → Cuenta de pokesurvivor.com → Entrar** with your web
username and password. Both saves are **merged without losing anything**
(Pokémon, shinies, coins, tickets, upgrades, achievements...), the same way the
web merges two computers, and then it syncs by itself on boot and after every
run, or with «Sincronizar ahora» (Sync now). Reload the web page to see what
you did on the 3DS.

- Only **username + password** accounts (Google accounts can't be used on the
  console). The password is **never stored**, only a session token.
- **Safe against web updates**: if the web changes the save format, the 3DS
  touches nothing and asks you to update; anything the 3DS doesn't know (new
  Pokémon, upgrades or fields) is **never deleted**; the 3DS only writes what
  it changed; and if a merge would lose anything, nothing is uploaded. Backups
  are written to the SD card before every sync.

## Install on the console
1. A 3DS with custom firmware and the Homebrew Launcher.
2. Copy `pokesurvivor-3ds.3dsx` to `sd:/3ds/` and open it from the Homebrew Launcher.
3. Sound needs `sd:/3ds/dspfirm.cdc` (dump it once with DSP1).
4. Online play and web sync need Wi-Fi.

Files in `sd:/3ds/pokesurvivor-3ds/`: `save.json` (+ `save.bak`, your save, same
format as the web), `settings.json` (settings, including the online server),
`cuenta.json` (pokesurvivor.com session, no password) and the backups of the
last sync (`save.antes-de-sincronizar.json`, `nube.copia.json`).

## Controls
| Button | Action |
|---|---|
| Circle Pad / D-Pad | Move |
| L / R (or X, ZL/ZR) | Switch move (or tap the slot below) |
| START | Pause (SELECT while paused: quit the run) |
| Up/down + A, or tap | Pick a level-up card, item or bond |
| Y | Reroll level-up cards |
| Tap the minimap | Map legend |
| START + SELECT | Exit the game |

## The online server
It lives in **its own repository, `pokesurvivor-relay`**: a single Python file
with **Docker** and an easy setup guide (VPS, Raspberry Pi or home PC). By
default the console uses the project's server; anyone can set their own in
**En grupo → Por internet → Y**.

## Building
```bash
tools/setup_vps.sh                       # devkitPro (official Docker image), Node, Python, Azahar...
tools/build_assets.sh                    # web assets → romfs/
source tools/env.sh && make              # → pokesurvivor-3ds.3dsx
cmake -S . -B build-pc && cmake --build build-pc -j2 && (cd build-pc && ctest)   # PC tests
```
More commands in [`las normas del proyecto`](las normas del proyecto) → *Comandos*.

## Testing
Golden tests against the original JavaScript, headless simulations with
AddressSanitizer/UBSan, multi-console lockstep tests over a fake network and
over the real relay server, the Azahar emulator (screenshots, autotests, two
emulated consoles playing together locally and online), and web sync tested
end-to-end with the **original web code** in Chromium against the Firebase
emulator.

## Licenses and credits
- Thanks above all to **JPTER78**, creator of PokéSurvivor
  ([github.com/JPTER78/pokesurvivor](https://github.com/JPTER78/pokesurvivor)),
  and to **juanluismm521**, who brought it to the 3DS
  ([github.com/juanluismm521/pokesurvivor3ds](https://github.com/juanluismm521/pokesurvivor3ds)).
- Code: PolyForm Noncommercial 1.0.0 (`LICENSE`, `LICENSE-NONCOMMERCIAL.md`).
- Sprites: PMD Sprite Collab, CC BY-NC 4.0. All artists in
  [`CREDITS.md`](CREDITS.md); they are also shown in the game.
- Fonts: Pixelify Sans and Tiny5 (SIL OFL, `assets-src/fonts/`).

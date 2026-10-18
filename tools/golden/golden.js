#!/usr/bin/env node
/* golden.js — casos dorados: ejecuta las funciones del JS original de la web
 * (reference/web/js) con node y guarda los resultados para las pruebas de PC.
 *
 *   node tools/golden/golden.js <carpeta_salida>
 *
 * Escribe golden.json (números) y chunk_*.rgba (píxeles del suelo pintado).
 * Las pruebas (tests/test_golden.cpp) comparan el port con esto: si no
 * coinciden, el fallo está en el port.
 */
'use strict';
const fs = require('fs'), path = require('path'), vm = require('vm');
const ROOT = path.join(__dirname, '..', '..');
const WEB = path.join(ROOT, 'reference', 'web');
const OUT = process.argv[2] || path.join(ROOT, 'build-pc', 'golden');
fs.mkdirSync(OUT, { recursive: true });

const sandbox = require('../lib/websandbox').create();
const G = sandbox.G;
function load(f, inject) {
  let src = fs.readFileSync(path.join(WEB, f), 'utf8').replace(/\r\n/g, '\n');
  if (inject) for (const [marker, code] of inject) {
    if (!src.includes(marker)) throw new Error(`${f}: no encuentro "${marker}"`);
    src = src.replace(marker, code + '\n' + marker);
  }
  vm.runInContext(src, sandbox, { filename: f });
}
for (const f of ['js/core/util.js', 'js/core/i18n.js', 'js/data/pokedex.js', 'js/data/sprites-meta.js', 'js/core/sprites.js',
                 'js/data/moves.js', 'js/data/types.js', 'js/data/items.js', 'js/data/upgrades.js', 'js/data/starters.js',
                 'js/world/tiles.js']) load(f);
load('js/data/enemies.js', [['return {\n    TIERS', 'G.__EN2 = { behaviorOf, defFor, candidates };']]);
load('js/world/world.js', [['function arenaDist', 'G.__W = { fbm: (x, y, s) => fbm(x, y, s), vnoise: (x, y, s) => vnoise(x, y, s), terrainAt: (x, y) => terrainAt(x, y), chunkData: (x, y) => chunkData(x, y), get seed() { return seed; } };']]);
load('js/entities/player.js');
load('js/systems/spawner.js');
load('js/systems/gacha.js');
load('js/data/missions.js');
load('js/data/achievements.js');
sandbox.G.Ranking = { periodIds: () => G.__periods };
sandbox.G.DB = { save: null, commit() {} };
load('js/systems/progress.js');

const out = {};
const r6 = x => Math.round(x * 1e6) / 1e6;

// ---------------- hash y ruido ----------------
out.hash = [];
for (const [x, y, s] of [[0, 0, 0], [1, 2, 3], [-5, 7, 101], [123456, -654321, 909], [-1, -1, -1], [2147483647, -2147483648, 77]]) out.hash.push([x, y, s, G.U.hash(x, y, s)]);
out.fbm = [];
for (const [x, y, s] of [[0.5, 0.25, 102], [-3.7, 12.2, 203], [100.01, -50.5, 506], [7 / 13, -9 / 13, 102]]) out.fbm.push([x, y, s, G.__W.fbm(x, y, s)]);

// ---------------- mundo ----------------
const SEED = 424242;
G.World.reset(SEED);
G.World.setBiome(0, true);
const R = 48;
let terr = '';
for (let ty = -R; ty < R; ty++) for (let tx = -R; tx < R; tx++) terr += G.__W.terrainAt(tx, ty);
out.world = { seed: SEED, r: R, terrain: terr, props: [] };
for (const [cx, cy] of [[0, 0], [1, 0], [-1, -1], [2, 3], [-3, 2], [5, -4]]) {
  const c = G.__W.chunkData(cx, cy);
  out.world.props.push({ cx, cy, list: c.props.map(p => [p.type, p.tx, p.ty, p.kind]) });
}
// Arena de la grieta (tipo fuego) y de la Sentencia.
for (const [type, tx, ty, r] of [['fire', 6000, 6100, 11], ['judgment', -6500, -7000, 24]]) {
  G.World.reset(SEED);
  G.World.setArena({ tx, ty, r, type });
  let s = '';
  for (let y = ty - r - 3; y <= ty + r + 3; y++) for (let x = tx - r - 3; x <= tx + r + 3; x++) s += G.__W.terrainAt(x, y);
  const props = [];
  for (const [cx, cy] of [[Math.floor(tx / 16), Math.floor(ty / 16)], [Math.floor(tx / 16) - 1, Math.floor(ty / 16)]]) {
    const c = G.__W.chunkData(cx, cy);
    props.push({ cx, cy, list: c.props.map(p => [p.type, p.tx, p.ty, p.kind]) });
  }
  (out.arenas = out.arenas || []).push({ type, tx, ty, r, terrain: s, props });
}

// Pintado de chunks (píxel a píxel) en varios biomas.
out.chunks = [];
G.World.reset(SEED);
const KIND = ['floor', 'wall', 'liquid'];
const at = (tx, ty) => KIND[G.__W.terrainAt(tx, ty)];
const dirtField = (gx, gy) => G.__W.fbm(gx / (6 * 16), gy / (6 * 16), SEED + 303);
let ci = 0;
for (const [cx, cy, bi] of [[0, 0, 0], [3, -2, 1], [-4, 1, 2], [2, 2, 5], [-2, -3, 0]]) {
  const cv = G.Tiles.paintChunk(cx, cy, 16, at, dirtField, bi, SEED);
  const file = `chunk_${ci++}.rgba`;
  fs.writeFileSync(path.join(OUT, file), Buffer.from(cv._img.data.buffer));
  out.chunks.push({ cx, cy, biome: bi, file });
}

// ---------------- tipos ----------------
const TYPES = Object.keys(G.U.TYPE_NAME);
out.typeMult = [];
for (const a of TYPES) for (const d1 of TYPES) for (const d2 of [null, ...TYPES]) {
  if (d2 === d1) continue;
  if (d2 && TYPES.indexOf(d2) % 5 !== TYPES.indexOf(d1) % 5) continue;   // muestra (no las 5.800)
  out.typeMult.push([TYPES.indexOf(a), TYPES.indexOf(d1), d2 ? TYPES.indexOf(d2) : -1, r6(G.Types.mult(a, d2 ? [d1, d2] : [d1]))]);
}

// ---------------- jugador y fórmulas ----------------
out.xpFor = [];
for (let l = 1; l <= 80; l++) out.xpFor.push(G.Player.xpFor(l));
out.scale = [];
for (const t of [0, 30, 60, 95, 180, 333, 600, 900, 1200, 1799]) {
  const s = G.Enemies.scale(t), b = G.Enemies.bossScale(t);
  out.scale.push([t, r6(s.hp), r6(s.dmg), r6(s.spd), r6(b.hp), r6(b.dmg), r6(G.Spawner.rate(t))]);
}
out.dir = [];
for (let a = -3.3; a <= 3.3; a += 0.137) out.dir.push([r6(a), G.Sprites.dirFromAngle(a)]);

// ---------------- Pokédex: comportamiento, cuerpo, movimiento inicial ----------------
out.dex = G.DEX.map(p => [p.dex, G.__EN2.behaviorOf(p), r6(G.Sprites.bodyRadius(p.dex)), r6(G.Sprites.bodyHeight(p.dex)), G.Moves.ALL.findIndex(m => m.id === G.startMoveOf(p.dex))]);
out.defFor = [];
G.Enemies.TIERS.forEach((tier, ti) => {
  for (const p of G.__EN2.candidates(tier).filter((_, i) => i % 7 === 0)) {
    const d = G.__EN2.defFor(p, tier, ti);
    out.defFor.push([ti, p.dex, d.behavior, d.hp, d.spd, d.dmg, d.xp, d.shotDmg || 0, d.shotCd || 0, d.range || 0, d.minion || 0, d.fanN || 0, d.zones || 0]);
  }
});
out.candidates = G.Enemies.TIERS.map(t => G.__EN2.candidates(t).length);

// ---------------- movimientos ----------------
const MSTATS = ['cd', 'dmg', 'speed', 'count', 'spread', 'pierce', 'life', 'size', 'radius', 'arc', 'width', 'knock', 'burn', 'poison', 'slow', 'homing', 'amount', 'dur', 'stacks', 'rate', 'range', 'hex', 'drain'];
out.moves = [];
for (const m of G.Moves.ALL) {
  for (let lvl = 1; lvl <= 5; lvl++) {
    const s = G.Moves.instance(m.id, lvl);
    out.moves.push([m.id, lvl, 0, MSTATS.map(k => (s[k] == null ? null : r6(s[k])))]);
  }
  const s = G.Moves.instance(m.id, 5);
  if (G.Moves.evolve(s)) out.moves.push([m.id, 5, 1, MSTATS.map(k => (s[k] == null ? null : r6(s[k])))]);
}
out.pool = [];
for (const types of [['fire'], ['water', 'flying'], ['grass', 'poison'], ['normal'], ['dragon', 'ghost'], ['bug', 'steel']]) {
  out.pool.push([types.map(t => TYPES.indexOf(t)), G.Moves.poolFor({ types }).map(id => G.Moves.ALL.findIndex(m => m.id === id))]);
}

// ---------------- mejoras ----------------
out.upCost = [];
for (const u of G.Upgrades.LIST) for (let l = 0; l < u.max; l++) out.upCost.push([u.id, l, G.Upgrades.cost(u.id, l)]);
out.perks = [];
for (const lv of [{}, { hp: 3, atk: 10, spd: 99, cd: 4, def: 8, regen: 5, magnet: 2, xp: 8, coins: 1, moves: 1, crit: 10, reroll: 2, ban: 3 }]) {
  const p = G.Upgrades.perks(lv);
  out.perks.push([G.Upgrades.LIST.map(u => lv[u.id] || 0), [p.hp, r6(p.atkMul), r6(p.spdMul), r6(p.cdMul), r6(p.dmgReduce), r6(p.regen), r6(p.magnetMul), r6(p.xpMul), r6(p.coinMul), p.startMoves, r6(p.crit), p.rerolls, p.bans]]);
}

// ---------------- gacha ----------------
out.gachaPools = [];
for (let gen = 1; gen <= 9; gen++) for (let r = 1; r <= 5; r++) out.gachaPools.push([gen, r, G.Gacha.pool(r, gen).length]);

// ---------------- misiones (sorteo con semilla de fecha + partida) ----------------
out.missions = [];
for (const [d, w, created] of [['d20261018', 'w2026-42', 1760000000000], ['d20270101', 'w2026-53', 0], ['d20250315', 'w2025-11', 1234567]]) {
  G.__periods = { d, w };
  const sv = { created, missions: {}, stats: {} };
  G.Progress.ensure(sv);
  const m = sv.missions;
  out.missions.push({ d, w, created, daily: m.daily.map(x => [x.id, x.goal, x.param]), weekly: m.weekly.map(x => [x.id, x.goal, x.param]),
                      texts: m.daily.concat(m.weekly).map(x => G.Progress.text(x)) });
}
out.ach = G.AchDefs.LIST.map(a => [a.id, a.goal]);

fs.writeFileSync(path.join(OUT, 'golden.json'), JSON.stringify(out));
console.log('golden: ' + Object.keys(out).map(k => k + (Array.isArray(out[k]) ? ' ' + out[k].length : '')).join(', '));

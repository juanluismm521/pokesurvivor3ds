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
if (fs.existsSync(path.join(WEB, 'js/systems/stats.js'))) load('js/systems/stats.js');
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
// Desde 2026-10-18/19: físico/especial (stats.js), tamaño de los grandes
// (playerScale/playerRadius) y movimientos para empezar (startChoices).
if (G.Stats) {
  const mi = id => G.Moves.ALL.findIndex(m => m.id === id);
  out.stats = G.DEX.map(p => [p.dex, r6(G.Stats.taken(p, 'physical')), r6(G.Stats.taken(p, 'special')), G.Stats.sideOf(p) === 'special' ? 1 : 0,
                               r6(G.Stats.fit(p, 'physical')), r6(G.Stats.fit(p, 'special')),
                               r6(G.Sprites.playerScale(p.dex)), r6(G.Sprites.playerRadius(p.dex)), G.startChoices(p.dex).map(mi)]);
}
out.defFor = [];
G.Enemies.TIERS.forEach((tier, ti) => {
  for (const p of G.__EN2.candidates(tier).filter((_, i) => i % 7 === 0)) {
    const d = G.__EN2.defFor(p, tier, ti);
    out.defFor.push([ti, p.dex, d.behavior, d.hp, d.spd, d.dmg, d.xp, d.shotDmg || 0, d.shotCd || 0, d.range || 0, d.minion || 0, d.fanN || 0, d.zones || 0]);
  }
});
out.candidates = G.Enemies.TIERS.map(t => G.__EN2.candidates(t).length);

// ---------------- movimientos ----------------
// (mismo orden que MoveStat en src/core/gamedata.h)
const MSTATS = ['cd', 'dmg', 'speed', 'count', 'spread', 'pierce', 'life', 'size', 'radius', 'arc', 'width', 'knock', 'burn', 'poison', 'slow', 'homing', 'amount', 'dur', 'stacks', 'rate', 'range', 'hex', 'drain',
                'pull', 'bounce', 'stun', 'charge'];
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

// ---------------- huecos con atajos y vínculos (player.js) ----------------
{
  const noop = () => {};
  Object.assign(G, { FX: new Proxy({}, { get: () => noop }), Projectiles: { clearOrbs: noop }, Audio: { sfx: noop },
                     Weather: { speedFor: () => 1, mult: () => 1 }, World: G.World, Game: { st: { time: 0 } } });
  const cases = [
    { dex: 25, bind: {}, moves: ['thunder-shock', 'tackle', 'discharge', 'quick-attack'], fuse: [[1, 2], [0]] },
    { dex: 6, bind: { 'ember': 3, 'wing-attack': 0 }, moves: ['ember', 'tackle', 'wing-attack', 'flamethrower'], fuse: [[0, 1], [3]] },
    { dex: 150, bind: { 'psychic': 1, 'tackle': 1, 'confusion': 2 }, moves: ['confusion', 'tackle', 'psychic', 'psybeam'], fuse: [[2, 3]] },
    { dex: 1, bind: { 'vine-whip': 0, 'tackle': 0 }, moves: ['tackle', 'vine-whip'], fuse: [] }
  ];
  out.slots = [];
  for (const c of cases) {
    const pl = new G.Player(G.DEX_BY[c.dex], {}, false);
    for (const [id, i] of Object.entries(c.bind)) pl.bind[id] = i;
    const steps = [];
    const snap = () => ({ slots: pl.slots.map(g => g.map(m => m.id)), active: pl.active });
    for (const id of c.moves) { pl.addMove(id); steps.push(snap()); }
    for (const f of c.fuse) {
      // Fusión: índices en pl.moves de los movimientos que se unen.
      pl.bonds = 1;
      const opt = pl.bondOptions();
      if (!opt) { steps.push({ none: true }); continue; }
      pl.fuse(f.map(i => pl.moves[i]), opt.group);
      steps.push(snap());
    }
    out.slots.push({ dex: c.dex, bind: c.bind, moves: c.moves, fuse: c.fuse, steps });
  }
}

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

// ---------------- traducciones (i18n.js en inglés) ----------------
{
  const sb = require('../lib/websandbox').create('en');
  const GE = sb.G;
  const ld = f => vm.runInContext(fs.readFileSync(path.join(WEB, f), 'utf8').replace(/\r\n/g, '\n'), sb, { filename: f });
  ld('js/core/util.js'); ld('js/core/i18n.js'); ld('js/data/lang-en.js');
  const samples = [
    '¡Un Pikachu shiny anda cerca!', '¡Horda de tipo Fuego!', 'HORDA FUEGO · 23 s', '¡Hora dorada! Doble de experiencia',
    '¡Has conseguido a Pikachu shiny!', '¡Otro Eevee shiny! (+300 monedas)', '¡Marea de Pokémon!', '— Bosque Umbrío —',
    '¡Snorlax salvaje apareció!', '¡Garchomp se ha enfurecido!', 'Has entrado en la grieta…', '¡Huye de Arceus! Es invencible',
    'Bosque Umbrío · 5 derrotados · 1 monedas', 'Nv. 3', 'Pausa', 'Volver', 'LA SENTENCIA', 'DE ARCEUS', 'El tiempo se ha agotado…',
    '¡ARCEUS HA DESCENDIDO!', 'Es invencible: huye y aguanta todo lo que puedas', '¡SE HA ABIERTO UNA GRIETA!',
    'Sigue la flecha morada: dentro te espera un legendario', '¡Nivel 7! Elige una mejora', 'Sólo puedes llevar un objeto',
    '¡Llevas Restos!', 'Ya llevas Vidasfera', '¡Banda Focus!', '¡Pegajoso!', '¡Vínculo!', '+1 ticket', '¡Ticket ×10!',
    'El manantial se ha secado (se recarga solo)', '¡Poder Furia! daño ×2 durante 25 s', 'Te has debilitado', 'Fin de la partida',
    'GRIETA · 12 m · 30 s', 'SENTENCIA · 1:05 · JEFES 2', '¡Mewtwo te esperaba en la grieta!', '¡Has vencido a Lugia! Recoge el premio…',
    'La grieta os ha expulsado…', '¡Un Meowth cargado de monedas! ¡Atrápalo antes de que huya!', '¡Meowth se ha escapado!',
    '¡Un Bulbasaur se ha perdido! Búscalo y llévalo a la marca', '¡Bulbasaur te sigue! Llévalo a la marca verde',
    'Misión completada: Derrota a 300 Pokémon', '¡Logro: Coleccionista!', 'Nuevo título: Coleccionista'];
  for (const m of GE.MissionDefs ? [] : []) samples.push(m);
  out.i18n = samples.map(s => [s, GE.I18n.t(s)]);
  for (const m of G.Moves.ALL) { out.i18n.push([m.name, GE.I18n.t(m.name)]); out.i18n.push([m.desc, GE.I18n.t(m.desc)]); }
  for (const it of G.Items.LIST) { out.i18n.push([it.name, GE.I18n.t(it.name)]); out.i18n.push([it.desc, GE.I18n.t(it.desc)]); }
  for (const a of G.AchDefs.LIST.slice(0, 60)) { out.i18n.push([a.name, GE.I18n.t(a.name)]); out.i18n.push([a.desc, GE.I18n.t(a.desc)]); }
  for (const m of out.missions) for (const t of m.texts) out.i18n.push([t, GE.I18n.t(t)]);
}

// ---------------- Tienda Kecleon: naturalezas, habilidades y MT (web del 20-10-2026) ----------------
// reference/web es del 19-10 y no los tiene: salen de la última web con fuentes
// (tools/cache/web, rama fuentes-28-10 = d667c216), leídos con git sin tocar reference/.
// También db.js: la fusión con la Mochila, las MT, naturalezas y habilidades.
const NEW_REF = process.env.PS_WEB_NEW || 'fuentes-28-10';
function loadNew(f, edits) {
  // (el clon del publicador enlaza reference/ del repo de trabajo: la caché, a su lado)
  const cache = [path.join(ROOT, 'tools', 'cache', 'web'), path.join(fs.realpathSync(path.join(ROOT, 'reference')), '..', 'tools', 'cache', 'web')]
    .find(d => fs.existsSync(d)) || path.join(ROOT, 'tools', 'cache', 'web');
  let src = require('child_process').execFileSync('git', ['-C', cache, 'show', NEW_REF + ':' + f], { encoding: 'utf8', maxBuffer: 1 << 26 }).replace(/\r\n/g, '\n');
  for (const [a, b] of edits || []) { if (!src.includes(a)) throw new Error(`${f}: no encuentro "${a}"`); src = src.replace(a, b); }
  vm.runInContext(src, sandbox, { filename: f });
}
for (const f of ['js/data/natures.js', 'js/data/abilities.js', 'js/data/tms.js', 'js/core/skins.js']) loadNew(f);
sandbox.G.DB = { save: null, commit() {} };
loadNew('js/systems/mons.js');
loadNew('js/data/shop.js', [['r = rng(\'kecleon:\' + today());', 'r = rng(\'kecleon:\' + (G.__DAY || today()));'],
                            ['rng(\'kecleon:\' + today() + \':\' + dex)', 'rng(\'kecleon:\' + (G.__DAY || today()) + \':\' + dex)']]);
{
  // Naturaleza y habilidad que le tocan a cada Pokémon (semilla: fecha de creación + dex).
  const CREATED = [1700000000000, 1792506454241, 1761000000123, 0];
  out.mons = [];
  for (const created of CREATED) {
    G.DB.save = { created, pnat: {}, pabil: {}, ptm: {}, tms: {}, bag: {} };
    const rows = [];
    for (const p of G.DEX) {
      const ab = G.Mons.abilityOf(p.dex);
      rows.push([p.dex, G.NATURES.indexOf(G.Mons.natureOf(p.dex)), ab.slot]);
    }
    out.mons.push({ created, rows });
  }
  // Las MT del día (semilla: la fecha) y las que puede aprender cada uno.
  out.shop = [];
  for (const [day, partner, taught] of [['20261107', 25, {}], ['20261020', 54, {}], ['20251231', 6, {}], ['20261107', 150, { 150: ['shadow-ball'] }], ['20270615', 129, {}]]) {
    G.__DAY = day;
    G.DB.save = { created: 1700000000000, partner, pnat: {}, pabil: {}, ptm: taught, tms: {}, bag: {} };
    out.shop.push({ day, partner, taught, list: G.Shop.dailyTms() });
  }
  G.__DAY = null;
  G.DB.save = { created: 1700000000000, pnat: {}, pabil: {}, ptm: { 150: ['shadow-ball'] }, tms: {}, bag: {} };
  out.learn = G.DEX.filter(p => p.dex !== 491).map(p => [p.dex, G.Mons.learnable(p.dex)]);   // (Darkrai: el port le da más ataques de serie)
  out.monFor = G.DEX.slice(0, 60).map(p => { G.DB.save = { created: 1792506454241, pnat: {}, pabil: {} }; const m = G.Mons.monFor(p.dex); return [p.dex, m.nature, r6(m.pa), r6(m.sa), r6(m.df), r6(m.sd), r6(m.spd), r6(m.atk)]; });
}

// ---------------- partida de la web: migrate, sanitize, ledOf, fold, merge (db.js) ----------------
{
  vm.runInContext('Date.now = () => 1700000000000;', sandbox);
  loadNew('js/core/db.js', [['return {\n    ready,', 'G.__DBX = { migrate, sanitize, ledOf, newSave, merge, fold };\n    return {\n    ready,'],
                            ["let d = read('ps.dev');", "let d = 'ctr3ds01';"]]);
  const X = G.__DBX, NOW = 1700000000000, DEV = 'ctr3ds01';
  const U = G.Upgrades.LIST.map(u => u.id), M = G.Moves.ALL.map(m => m.id);
  const clone = o => JSON.parse(JSON.stringify(o));
  const mk = f => { const s = X.newSave('Ana'); f(s); return s; };
  const cases = [];
  // 1. dos partidas nuevas
  cases.push([mk(s => { s.updated = NOW + 5; }), mk(s => { s.updated = NOW + 9; s.coins = 650; })]);
  // 2. sincronización de verdad: libreta con dos aparatos web; la 3DS jugó con una copia vieja
  const base = mk(s => {
    s.coins = 900; s.tickets = { t1: 4, t10: 1 }; s.updated = NOW + 100;
    s.owned = { 25: { at: 10, from: 'starter' }, 4: { at: 20, from: 'gacha' } }; s.partner = 25;
    s.led = { base: { c: [500, 0], t1: [0, 0], t10: [0, 0] }, dweb1: { c: [600, 200], t1: [4, 0], t10: [1, 0] } };
    s.pupg = { 25: { [U[0]]: 2, [U[1]]: 1 } }; s.stats.runs = 5; s.stats.bestTime = 400;
    s.missions = { day: '2026-10-19', daily: [{ id: 'kill', goal: 300, prog: 100, claimed: false, told: false }, { id: 'time', goal: 5, prog: 2, claimed: false, told: false }], week: '2026-W42', weekly: [] };
  });
  const web = clone(base);
  web.updated = NOW + 300; web.coins = 1000; web.led.dweb1.c = [800, 300]; web.stats.runs = 7; web.owned[1] = { at: 50, from: 'gacha' };
  web.missions.daily[0].prog = 180;
  const ds = clone(base);
  ds.updated = NOW + 200; ds.coins = 1150; ds.tickets.t1 = 6; ds.owned[7] = { at: 60, from: 'wild' }; ds.shiny = { 25: { at: 70, from: 'wild' } };
  ds.pupg[25][U[0]] = 4; ds.stats.bestTime = 900; ds.missions.daily[1].prog = 5; ds.missions.daily[1].claimed = true; ds.legends = { 150: 80 };
  cases.push([ds, web]);
  // 3. basura (partida tocada a mano) y formatos viejos
  cases.push([{
    name: 'Ana', created: NOW - 50, updated: NOW + 1, coins: -5, tickets: { t1: '7', t10: 2.6 }, owned: { 9999: { at: 1 }, '025': { at: 2 }, 6: { at: 3, from: 'gacha' } },
    pupg: { 25: { [U[0]]: 99, nope: 3 }, 4: null }, pity: 6, upgrades: { [U[1]]: 2 }, partner: 25,
    led: { dX: { c: ['5', -2], zz: [1, 2] }, bad: 3 }, stats: { runs: 2 },
    pbind: { 25: [M[0], null, M[1], M[1], M[2]], 4: 'x' }, pban: { 25: [M[3], 'bogus', M[3], M[4], M[5], M[6]] }
  }, mk(s => { s.updated = NOW; s.coins = 10; })]);
  // 4. misiones del mismo día y compañero que el más nuevo no tiene
  cases.push([mk(s => { s.updated = NOW + 1; s.partner = 4; s.partnerShiny = true; s.owned = { 4: { at: 3 } };
                       s.missions = { day: 'd1', daily: [{ id: 'a', prog: 3, claimed: true, told: false }, { id: 'b', prog: 9, claimed: false, told: true }], week: 'w', weekly: [{ id: 'z', prog: 1 }] }; }),
              mk(s => { s.updated = NOW + 2; s.partner = 9; s.owned = { 1: { at: 5 } };
                       s.missions = { day: 'd1', daily: [{ id: 'a', prog: 1, claimed: false, told: true }, { id: 'c', prog: 1, claimed: false, told: false }], week: 'w2', weekly: [{ id: 'z', prog: 7 }] }; })]);
  // 5. una sin libreta y otra con ella (lo que gastó cada uno)
  cases.push([mk(s => { s.updated = NOW + 8; s.coins = 120; s.tickets = { t1: 0, t10: 0 }; }),
              mk(s => { s.updated = NOW + 3; s.coins = 300; s.led = { base: { c: [500, 0] }, dweb9: { c: [0, 200], t1: [3, 3] } }; })]);
  // 6. Tienda Kecleon: la 3DS compró una Menta y dos MT y usó una; la web gastó la
  //    Menta que tenía, cambió la naturaleza y enseñó otra MT. Libreta en negativo (fix).
  const shop = mk(s => {
    s.updated = NOW + 50; s.coins = 3000; s.owned = { 25: { at: 1 }, 150: { at: 2 } }; s.partner = 25;
    s.bag = { mint: 1, caph: 2, candy: 40 }; s.tms = { 'shadow-ball': 1 };
    s.led = { base: { c: [500, 0], t1: [0, 0], t10: [0, 0] }, dweb: { c: [2500, 0], 'b:mint': [1, 0], 'b:caph': [2, 0], 'b:candy': [40, 0], 'm:shadow-ball': [1, 0] } };
    s.ptm = { 150: ['psychic', 'shadow-ball'] }; s.pnat = { 25: ['bold', NOW + 10] }; s.pabil = { 150: [2, NOW + 11] };
    s.skins = { 'bruja-pikachu': NOW }; s.pskin = { 25: ['bruja-pikachu', NOW + 5] };
  });
  const ds6 = clone(shop), web6 = clone(shop);
  ds6.updated = NOW + 90; ds6.coins = 900; ds6.bag.mint = 2; ds6.bag.cap = 1; ds6.tms['thunderbolt'] = 1; ds6.tms['shadow-ball'] = 0;
  ds6.ptm[25] = ['psychic']; ds6.pnat[25] = ['timid', NOW + 20]; ds6.pabil[25] = [1, NOW + 21];
  web6.updated = NOW + 80; web6.bag.mint = 0; web6.pnat[25] = ['modest', NOW + 30]; web6.ptm[150] = ['shadow-ball', 'thunderbolt'];
  web6.led.dweb.c = [2500, 1500]; web6.led.dweb['b:mint'] = [1, 2]; web6.pskin[25] = ['', NOW + 40];
  web6.pstart2 = { 25: 'thunder-shock', 9999: 'x' };
  cases.push([ds6, web6]);
  // 7. basura en lo de la tienda
  cases.push([mk(s => {
    s.updated = NOW + 4; s.bag = { mint: '3', bogus: 2, cap: -1 }; s.tms = { 'shadow-ball': 2.7, nope: 1, 'thunderbolt': 0 };
    s.ptm = { 25: ['psychic', 'psychic', 7, 'nope'], 9999: ['psychic'], 4: 'x' }; s.pnat = { 25: ['nope', 5], 4: ['bold', '7'], 9999: ['bold', 1] };
    s.pabil = { 25: [3, 1], 4: [1, -4] }; s.skins = { 'bruja-gengar': 'x', 'bruja-pikachu': 5, nope: 1 };
    s.pskin = { 25: ['bruja-gengar', 1], 94: ['bruja-gengar', 2], 197: [5, 1] };
  }), mk(s => { s.updated = NOW + 2; })]);
  out.websave = cases.map(([a, b]) => {
    const ma = X.migrate(clone(a)), mb = X.migrate(clone(b));
    const fa = X.fold(clone(ma));
    return { a, b, ma, mb, led: X.ledOf(ma, DEV), fa, m1: X.merge(clone(fa), clone(mb)), m2: X.merge(clone(mb), clone(ma)) };
  });
}

fs.writeFileSync(path.join(OUT, 'golden.json'), JSON.stringify(out));
console.log('golden: ' + Object.keys(out).map(k => k + (Array.isArray(out[k]) ? ' ' + out[k].length : '')).join(', '));

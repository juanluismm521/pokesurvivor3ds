#!/usr/bin/env node
/* gen_data.js — datos del juego de la web → C++ (build/gen/gamedata.cpp)
 *
 *   node tools/gen_data.js [salida.cpp]
 *
 * Carga los mismos .js que la web (reference/web/js) en un contexto aislado de
 * node, como hacía tools/export_data.js en la web, y escribe las tablas que
 * declara src/core/gamedata.h. Las tablas que la web guarda dentro de un
 * cierre (EVO_MODS, START, BIOME_W...) se sacan inyectando una línea de
 * exportación en el texto al cargarlo (el archivo de reference/ no se toca).
 *
 * Al final comprueba que no se pierde nada: cuentas y sumas de control contra
 * el JS (si algo no cuadra, sale con error).
 *
 *   node tools/gen_data.js --informe [salida.json]
 *
 * Modo informe (lo usa tools/web/actualizar.sh): no se para en el primer
 * problema; sigue y apunta TODOS los cambios de la web que el port no sabe
 * leer (familias de movimientos nuevas, campos nuevos...), y los deja en JSON.
 */
'use strict';
const fs = require('fs'), path = require('path'), vm = require('vm');
const ROOT = path.join(__dirname, '..');
// (PS_WEB: otra copia de la web, p. ej. una versión nueva para tools/web/informe.js)
const WEB = process.env.PS_WEB || path.join(ROOT, 'reference', 'web');
const REPORT = process.argv.includes('--informe');
const ARGS = process.argv.slice(2).filter(a => a !== '--informe');
const OUT = REPORT ? null : ARGS[0] || path.join(ROOT, 'build', 'gen', 'gamedata.cpp');
const problems = [];
/** Un cambio de la web que el port no sabe leer: error (o, en modo informe, se apunta y se sigue). */
function fail(msg) {
  if (!REPORT) throw new Error(msg);
  if (!problems.includes(msg)) problems.push(msg);
}

// ---------------- contexto aislado ----------------
const sandbox = require('./lib/websandbox').create();
const G = sandbox.G;
const EN = {};
function load(f, inject) {
  let src = fs.readFileSync(path.join(WEB, f), 'utf8').replace(/\r\n/g, '\n');
  if (inject) for (const [marker, code] of inject) {
    if (!src.includes(marker)) { fail(`${f}: no encuentro "${marker}" para inyectar`); continue; }
    src = src.replace(marker, code + '\n' + marker);
  }
  try { vm.runInContext(src, sandbox, { filename: f }); }
  catch (e) { if (!REPORT) throw e; fail(`${f}: no se puede cargar (${e.message})`); }
}
load('js/core/util.js');
load('js/core/i18n.js');
const add = G.I18n.add;
G.I18n.add = d => { Object.assign(EN, d); add(d); };
load('js/data/lang-en.js');
load('js/data/pokedex.js');
load('js/data/sprites-meta.js');
load('js/data/moves.js', [['G.Moves = {', 'G.__MV = { LADDERS, EVO_MODS, START, MULT, ADD };']]);
load('js/data/types.js');
load('js/data/items.js');
load('js/data/upgrades.js');
load('js/data/enemies.js', [['return {\n    TIERS', 'G.__EN = { HEAL, SHIELD, BOMB, SUMMON };']]);
load('js/data/starters.js');
load('js/data/missions.js');
load('js/data/achievements.js');
load('js/world/tiles.js');
load('js/systems/weather.js', [['let cur = null;', 'G.__WX = { BIOME_W };']]);
load('js/systems/interact.js');
load('js/systems/gacha.js');
load('js/systems/levelup.js', [['/** Construye el conjunto', 'G.__LV = { STATS };']]);
sandbox.G.Sprites = { PX: 2 };
load('js/entities/vfx.js', [['const TYPE_SHAPE', 'G.__VX = { MOVE };']]);

// ---------------- utilidades de salida ----------------
const esc = s => s == null ? 'nullptr' : '"' + String(s).replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/\n/g, '\\n') + '"';
const col = h => { if (!h) return '0x000000u'; const n = parseInt(h.slice(1), 16); return '0x' + n.toString(16).padStart(6, '0') + 'u'; };
const fl = v => { const x = +v; if (!isFinite(x)) return '0.0f'; const s = String(x); return (s.includes('.') || s.includes('e') ? s : s + '.0') + 'f'; };
const out = [];
const w = s => out.push(s);
const arr = (xs, f = x => x) => '{' + xs.map(f).join(', ') + '}';

const TYPES = Object.keys(G.U.TYPE_NAME);
const EXPECT_TYPES = ['normal', 'fire', 'water', 'grass', 'electric', 'ice', 'fighting', 'poison', 'ground', 'flying', 'psychic', 'bug', 'rock', 'ghost', 'dragon', 'dark', 'steel', 'fairy'];
if (TYPES.join() !== EXPECT_TYPES.join()) fail('El orden de los tipos ha cambiado en la web: ' + TYPES.join());
const T = t => (t == null ? -1 : TYPES.indexOf(t));
const ANIMS = ['Idle', 'Walk', 'Attack', 'Shoot', 'Charge', 'Hurt', 'Faint'];
const KINDS = ['projectile', 'melee', 'beam', 'orbit', 'nova', 'aura', 'boomerang', 'chain', 'mine', 'meteor', 'turret', 'dash', 'trail', 'cone',
               'homing', 'bounce', 'charge', 'spin', 'quake', 'vortex', 'shove', 'stun', 'buff'];
if (Object.keys(G.__MV.LADDERS).join() !== KINDS.join()) fail('Las familias de movimientos han cambiado: ' + Object.keys(G.__MV.LADDERS).join());
const MSTATS = ['cd', 'dmg', 'speed', 'count', 'spread', 'pierce', 'life', 'size', 'radius', 'arc', 'width', 'knock', 'burn', 'poison', 'slow', 'homing', 'amount', 'dur', 'stacks', 'rate', 'range', 'hex', 'drain',
                'pull', 'bounce', 'stun', 'charge'];
const BUFFS = ['atk', 'patk', 'satk', 'spd', 'def', 'regen'];
const LSTATS = G.__LV.STATS.map(s => s.id);
if (LSTATS.join() !== 'hp,atk,spd,cd,def,rgn,mag,xp,heal') fail('Las mejoras de nivel han cambiado: ' + LSTATS.join());
const SHAPES = Object.keys(G.VFX.SHAPES);
const EXPECT_SHAPES = ['fire', 'drop', 'bubble', 'leaf', 'bolt', 'shard', 'rock', 'blob', 'needle', 'star', 'streak', 'wind', 'web', 'ring', 'crescent', 'note', 'impact', 'claw', 'jaws', 'arrow', 'ball', 'shadow', 'heart'];
if (SHAPES.join() !== EXPECT_SHAPES.join()) fail('Las formas de vfx.js han cambiado: ' + SHAPES.join());
const shapeIdx = s => (s == null ? -1 : s === 'lightning' ? 100 : (() => { const i = SHAPES.indexOf(s); if (i < 0) fail('forma ' + s); return i; })());
const PALS = Object.keys(G.VFX.PAL);
if (PALS.slice(0, 18).sort().join() !== EXPECT_TYPES.slice().sort().join() || PALS[18] !== 'aura') fail('Paletas de vfx.js: ' + PALS.join());
const palIdx = p => (p === 'aura' ? 18 : T(p));
const LIQ = ['water', 'swamp', 'lava', 'poison', 'spark', 'ice'];
const PROPART = ['tree', 'crystal', 'dead'];
const WX = Object.keys(G.Weather.KINDS);
if (WX.join() !== 'sun,rain,sand,snow,fog') fail('Climas: ' + WX.join());
const EVS = ['kill', 'chest', 'item', 'evolve', 'rift', 'runEnd', 'judge', 'judgeEnd', 'missionDone'];

w('// gamedata.cpp — GENERADO por tools/gen_data.js. No editar a mano.');
w('// Fuente: reference/web/js/data (y tablas de tiles.js, weather.js, vfx.js...).');
w('#include <cstring>');
w('#include "core/gamedata.h"');
w('namespace ps {');

// ---------------- tipos ----------------
w(`const char* const kTypeId[T_COUNT] = ${arr(TYPES, esc)};`);
w(`const char* const kTypeName[T_COUNT] = ${arr(TYPES.map(t => G.U.TYPE_NAME[t]), esc)};`);
w(`const uint32_t kTypeColor[T_COUNT] = ${arr(TYPES.map(t => G.U.TYPE_COLOR[t]), col)};`);
w('float kTypeChart[T_COUNT][T_COUNT] = {');
for (const a of TYPES) w('  ' + arr(TYPES.map(d => { const r = G.Types.CHART[a] || {}; return fl(r[d] == null ? 1 : r[d]); })) + ',');
w('};');

// ---------------- Pokédex ----------------
w('Mon kDex[] = {');
for (const p of G.DEX) {
  for (const k of ['pa', 'sa', 'df', 'sd']) if (typeof p[k] !== 'number') fail(`Pokédex ${p.dex}: falta ${k}`);
  w(`  {${p.dex}, ${esc(p.name)}, {${T(p.types[0])}, ${T(p.types[1])}}, ${p.hp}, ${p.spd}, ${fl(p.atk)}, ${fl(p.pa)}, ${fl(p.sa)}, ${p.df}, ${p.sd}, ${p.rarity}, ${p.bst}, ${p.gen}, ${!!p.starter}, ${!!p.leg}},`);
}
w('};');
w(`const int kDexCount = ${G.DEX.length};`);
const idx = new Array(1026).fill(-1);
G.DEX.forEach((p, i) => { if (p.dex > 1025) fail('dex > 1025'); idx[p.dex] = i; });
w(`const int16_t kDexIndex[kMaxDex + 1] = ${arr(idx)};`);

// ---------------- sprites ----------------
w(`const char* const kAnimName[A_COUNT] = ${arr(ANIMS, esc)};`);
const durs = [];
const animMeta = m => {
  if (!m) return '{0, 0, 0, 0, 0, 0.0f, 0.0f}';
  if (m.w > 255 || m.h > 255 || m.n > 255) fail('fotograma demasiado grande');
  const at = durs.length;
  for (const d of m.d) { if (d > 255) fail('duración > 255'); durs.push(d); }
  if (m.d.length !== m.n) fail('d.length != n');
  return `{${m.w}, ${m.h}, ${m.n}, ${m.r}, ${at}, ${fl(m.ax)}, ${fl(m.ay)}}`;
};
const metaRows = [], altRows = [];
for (const p of G.DEX) {
  const m = G.SPRITE_META[p.dex];
  if (!m) { fail('sin SPRITE_META: ' + p.dex); continue; }
  if (m.sm) fail('sm (animaciones que le faltan al shiny) no soportado todavía: ' + p.dex);
  let alt = -1;
  if (m.sa) { alt = altRows.length; altRows.push('  {' + arr(ANIMS.map(a => animMeta(m.sa[a]))) + '},'); }
  metaRows.push(`  {${m.s}, ${!!m.sh}, ${!!m.hv}, ${alt}, ${arr(ANIMS.map(a => animMeta(m.a[a])))}},`);
}
// Megaevoluciones (del port): sus animaciones (y las de su shiny, si
// SpriteCollab lo tiene) van a las mismas tablas.
const MEGAS_FILE = path.join(ROOT, 'assets-src', 'megas', 'megas.json');
const MEGAS = fs.existsSync(MEGAS_FILE) ? JSON.parse(fs.readFileSync(MEGAS_FILE, 'utf8')).megas : [];
const durDex = durs.length;
const megaMeta = MEGAS.map(m => {
  if (!m.sprite) return `  {1, false, false, -1, ${arr(ANIMS.map(() => animMeta(null)))}},`;
  let alt = -1;
  if (m.sprite.sa) { alt = altRows.length; altRows.push('  {' + arr(ANIMS.map(a => animMeta(m.sprite.sa[a]))) + '},'); }
  return `  {${m.sprite.s}, ${!!m.sprite.sh}, false, ${alt}, ${arr(ANIMS.map(a => animMeta(m.sprite.a[a])))}},`;
});
w('const SpriteMeta kSpriteMeta[] = {'); metaRows.forEach(w); w('};');
w('const ShinyAlt kShinyAlt[] = {'); altRows.forEach(w); if (!altRows.length) w('  {}'); w('};');
w(`const uint8_t kAnimDur[] = ${arr(durs)};`);

// ---------------- megaevoluciones ----------------
w('const SpriteMeta kMegaSpriteMeta[] = {'); megaMeta.forEach(w); if (!megaMeta.length) w('  {}'); w('};');
w('const MegaDef kMegas[] = {');
MEGAS.forEach((m, i) => {
  const b = G.DEX.find(p => p.dex === m.dex);
  if (!b) fail('mega de un Pokémon que no está: ' + m.form);
  if (m.id !== 3000 + i) fail('número de sprite de mega desordenado: ' + m.form);
  w(`  {${m.dex}, ${m.slot}, ${m.sprite ? m.id : m.dex}, ${!!m.sprite}, ${esc(m.stone)}, {${m.dex}, ${esc(m.name)}, {${T(m.types[0])}, ${T(m.types[1])}}, ${m.hp}, ${m.spd}, ${fl(m.atk)}, ${fl(m.pa)}, ${fl(m.sa)}, ${m.df}, ${m.sd}, ${b.rarity}, ${m.bst}, ${b.gen}, false, ${!!b.leg}}},`);
});
if (!MEGAS.length) w('  {}');
w('};');
w(`const int kMegaCount = ${MEGAS.length};`);

// ---------------- movimientos ----------------
w(`const char* const kKindId[K_COUNT] = ${arr(KINDS, esc)};`);
w(`const char* const kMoveStatId[MS_NUM] = ${arr(MSTATS, esc)};`);
w(`const uint8_t kMoveStatOp[MS_NUM] = ${arr(MSTATS.map(k => (G.__MV.MULT.includes(k) ? 1 : G.__MV.ADD.includes(k) ? 2 : 0)))};`);
const known = new Set(['id', 'name', 'type', 'kind', 'desc', 'stat', 'cat', ...MSTATS]);
w('MoveDef kMoves[] = {');
for (const m of G.Moves.ALL) {
  if (m.stat && BUFFS.indexOf(m.stat) < 0) fail(`movimiento ${m.id}: potenciador de «${m.stat}» desconocido`);
  if (![null, 'physical', 'special'].includes(m.cat == null ? null : m.cat)) fail(`movimiento ${m.id}: categoría «${m.cat}» desconocida`);
  for (const k of Object.keys(m)) if (!known.has(k) && k !== 'ups' && k !== 'maxLvl') fail(`movimientos: campo nuevo «${k}» (p. ej. en ${G.Moves.ALL.find(x => k in x).id}; en ${G.Moves.ALL.filter(x => k in x).length} movimientos)`);
  let has = 0;
  const v = MSTATS.map((k, i) => { if (m[k] != null) { has |= 1 << i; return fl(m[k]); } return '0.0f'; });
  const evo = G.Moves.EVO[m.id];
  w(`  {${esc(m.id)}, ${esc(m.name)}, ${esc(m.desc)}, ${T(m.type)}, ${KINDS.indexOf(m.kind)}, ${m.stat ? BUFFS.indexOf(m.stat) : -1}, ${m.cat === 'physical' ? 0 : m.cat === 'special' ? 1 : -1}, 0x${(has >>> 0).toString(16)}u, ${arr(v)}, ${evo ? LSTATS.indexOf(evo[1]) : -1}, ${evo ? esc(evo[0]) : 'nullptr'}},`);
}
w('};');
w(`const int kMoveCount = ${G.Moves.ALL.length};`);
w(`static_assert(${G.Moves.ALL.length} <= kMaxMoves, "la web tiene más movimientos que kMaxMoves (gamedata.h)");`);
w('int move_index(const char* id) { for (int i = 0; i < kMoveCount; ++i) if (!std::strcmp(kMoves[i].id, id)) return i; return -1; }');
const ladder = l => {
  const mods = Object.entries(l.m).map(([k, val]) => { const i = MSTATS.indexOf(k); if (i < 0) fail('mod ' + k); return `{${i}, ${fl(val)}}`; });
  if (mods.length > 5) fail('más de 5 mods');
  return `{${esc(l.t)}, ${mods.length}, ${arr(mods)}}`;
};
w('Ladder kLadder[K_COUNT][4] = {');
for (const k of KINDS) { const L = G.__MV.LADDERS[k]; if (L.length !== 4) fail('escalera de ' + k); w('  ' + arr(L.map(ladder)) + ','); }
w('};');
w('Ladder kEvoMods[K_COUNT] = {');
for (const k of KINDS) w('  ' + (G.__MV.EVO_MODS[k] ? ladder(G.__MV.EVO_MODS[k]) : '{nullptr, 0, {}}') + ',');
w('};');
const mIdx = id => { const i = G.Moves.ALL.findIndex(m => m.id === id); if (i < 0) fail('mov ' + id); return i; };
w(`const int8_t kStartMove[T_COUNT] = ${arr(TYPES.map(t => mIdx(G.__MV.START[t])))};`);

// ---------------- vfx ----------------
w(`const char* const kShapeId[SH_COUNT] = ${arr(SHAPES, esc)};`);
w(`const ShapeInfo kShapeInfo[SH_COUNT] = ${arr(SHAPES.map(s => { const S = G.VFX.SHAPES[s]; return `{${S.w}, ${S.h}, ${S.frames || 1}, ${!!S.dir}, ${!!S.spin}}`; }))};`);
w('const uint32_t kPal[PAL_COUNT][5] = {');
for (const p of [...TYPES, 'aura']) w('  ' + arr(G.VFX.PAL[p].map(col)) + ',');
w('};');
const TRAILS = [null, 'ember', 'drip', 'spark', 'bubbles', 'dirt', 'wisp'];
const MELEES = [null, 'impact', 'jaws', 'claw', 'vine', 'feather'];
w('const MoveVis kMoveVis[] = {');
for (const m of G.Moves.ALL) {
  const v = G.VFX.forMove(m), raw = G.__VX.MOVE[m.id] || {};
  for (const k of Object.keys(raw)) if (!['proj', 'beam', 'melee', 'aura', 'buff', 'pal', 'trail', 'inward', 'rise'].includes(k)) fail('vfx ' + k);
  w(`  {${shapeIdx(v.proj)}, ${shapeIdx(v.beam)}, ${shapeIdx(v.aura)}, ${shapeIdx(v.buff)}, ${palIdx(v.pal)}, ${TRAILS.indexOf(v.trail || null)}, ${MELEES.indexOf(v.melee || null)}, ${!!v.inward}, ${!!v.rise}},`);
}
w('};');
w(`const int8_t kTypeShape[T_COUNT] = ${arr(TYPES.map(t => shapeIdx(G.VFX.forType(t).proj)))};`);

// ---------------- subir de nivel ----------------
w(`LevelStatDef kLevelStats[LS_COUNT] = ${arr(G.__LV.STATS.map(s => `{${esc(s.id)}, ${esc(s.icon)}, ${esc(s.title)}, ${esc(s.desc)}, ${s.max || 0}}`))};`);
w(`const char* const kNeedName[LS_COUNT] = ${arr(LSTATS.map(k => esc(G.Moves.NEED_NAME[k] || null)))};`);

// ---------------- objetos ----------------
const ITEMS = ['leftovers', 'choiceband', 'quickclaw', 'focussash', 'shellbell', 'amuletcoin', 'luckyegg', 'lifeorb', 'choicescarf', 'rockyhelmet', 'assaultvest'];
if (G.Items.LIST.map(i => i.id).join() !== ITEMS.join()) fail('Objetos: ' + G.Items.LIST.map(i => i.id).join());
w(`ItemDef kItems[IT_COUNT] = ${arr(G.Items.LIST.map(i => `{${esc(i.id)}, ${esc(i.name)}, ${esc(i.desc)}}`))};`);

// ---------------- mejoras ----------------
const UPS = ['hp', 'atk', 'spd', 'cd', 'def', 'regen', 'magnet', 'xp', 'coins', 'moves', 'crit', 'reroll', 'ban', 'start'];
if (G.Upgrades.LIST.map(u => u.id).join() !== UPS.join()) fail('Mejoras: ' + G.Upgrades.LIST.map(u => u.id).join());
w(`UpgradeDef kUpgrades[UP_COUNT] = ${arr(G.Upgrades.LIST.map(u => `{${esc(u.id)}, ${esc(u.icon)}, ${esc(u.name)}, ${esc(u.unit)}, ${u.max}, ${u.base}, ${fl(u.grow)}}`))};`);

// ---------------- enemigos ----------------
w(`Tier kTiers[] = ${arr(G.Enemies.TIERS.map(t => `{${t.from}, ${t.bst[0]}, ${t.bst[1]}, ${fl(t.hp)}, ${fl(t.spd)}, ${fl(t.dmg)}, ${fl(t.xp)}, ${t.cast}}`))};`);
w(`const int kTierCount = ${G.Enemies.TIERS.length};`);
w(`BossSlot kBossSlots[] = ${arr(G.Enemies.BOSS_SLOTS.map(b => `{${b.at}, ${b.hp}, ${b.dmg}, ${b.xp}, ${b.bst[0]}, ${b.bst[1]}}`))};`);
w(`const int kBossSlotCount = ${G.Enemies.BOSS_SLOTS.length};`);
for (const [name, set] of [['Heal', G.__EN.HEAL], ['Shield', G.__EN.SHIELD], ['Bomb', G.__EN.BOMB]]) {
  w(`const int16_t k${name}Dex[] = ${arr([...set])};`);
  w(`const int k${name}Count = ${set.size};`);
}
const summ = Object.entries(G.__EN.SUMMON);
w(`const SummonPair kSummon[] = ${arr(summ.map(([a, b]) => `{${a}, ${b}}`))};`);
w(`const int kSummonCount = ${summ.length};`);

// ---------------- biomas y arenas ----------------
const biome = B => {
  const c3 = a => arr(a.map(col));
  return `{${esc(B.name)}, ${LIQ.indexOf(B.liquid)}, ${PROPART.indexOf(B.prop)}, ${c3(B.floor)}, ${c3(B.dirt)}, ${col(B.speck)}, ${col(B.light)}, ${c3(B.flowers)}, ` +
    `${c3(B.wallTop)}, ${col(B.wallLight)}, ${col(B.wallEdge)}, ${c3(B.wallFace)}, ${col(B.wallFaceDark)}, ${col(B.liq)}, ${col(B.liqDeep)}, ${col(B.foam)}, ${col(B.shore)}, ` +
    `${c3(B.leaf)}, ${c3(B.trunk)}, ${c3(B.rock)}, ${c3(B.grass)}}`;
};
for (const B of G.Tiles.BIOMES) { if (LIQ.indexOf(B.liquid) < 0 || PROPART.indexOf(B.prop) < 0) fail('bioma ' + B.name); }
w('const Biome kBiomes[] = {'); for (const B of G.Tiles.BIOMES) w('  ' + biome(B) + ','); w('};');
w(`const int kBiomeCount = ${G.Tiles.BIOMES.length};`);
if (G.Tiles.NORMAL !== 3 || G.Tiles.ARENA !== 3) fail('NORMAL/ARENA de tiles.js han cambiado');
w('const ArenaTheme kArenaThemes[] = {');
for (const id of G.Tiles.ARENA_TYPES) {
  const th = G.Tiles.ARENA_THEMES[id], pr = th.props || {};
  w(`  {${esc(id)}, ${esc(th.name)}, ${LIQ.indexOf(th.liquid)}, ${fl(th.pools || 0)}, ${fl(th.pillars || 0)}, ${fl(pr.tree || 0)}, ${fl(pr.rock || 0)}, ${fl(pr.grass || 0)}, ${th.weather ? WX.indexOf(th.weather) : -1}, ${fl(th.clear || 0)}},`);
}
w('};');
w(`const int kArenaThemeCount = ${G.Tiles.ARENA_TYPES.length};`);
w('int arena_theme_index(const char* id) { for (int i = 0; i < kArenaThemeCount; ++i) if (!std::strcmp(kArenaThemes[i].id, id)) return i; return -1; }');

// ---------------- clima ----------------
w('WeatherDef kWeather[WX_COUNT] = {');
for (const k of WX) {
  const d = G.Weather.KINDS[k];
  let safe = 0; for (const t of d.safe || []) safe |= 1 << T(t);
  w(`  {${esc(k)}, ${esc(d.name)}, ${esc(d.icon)}, ${esc(d.hint)}, ${col(d.color)}, {${d.tint[0]}, ${d.tint[1]}, ${d.tint[2]}}, ${fl(d.tint[3])}, ${arr(TYPES.map(t => fl(d.mult[t] || 1)))}, 0x${safe.toString(16)}u, ${!!d.slow}},`);
}
w('};');
w(`const float kWeatherBiomeW[3][WX_COUNT] = ${arr(G.__WX.BIOME_W.map(b => { for (const k of Object.keys(b)) if (!WX.includes(k)) fail('clima ' + k); return arr(WX.map(k => fl(b[k] || 0))); }))};`);

// ---------------- altares ----------------
w(`const PowerDef kPowers[4] = ${arr(G.Interact.POWERS.map(p => `{${esc(p.id)}, ${esc(p.name)}, ${esc(p.desc)}, ${col(p.color)}}`))};`);

// ---------------- gacha ----------------
w(`const BannerDef kBanners[9] = ${arr(G.Gacha.BANNERS.map(b => `{${b.gen}, ${esc(b.name)}, ${arr(b.color.map(col))}}`))};`);
w(`float kGachaRates[5] = ${arr(G.Gacha.RATES.map(r => fl(r[1])))};`);
w(`int16_t kGachaRefund[6] = ${arr([0, 1, 2, 3, 4, 5].map(r => G.Gacha.REFUND[r] || 0))};`);
w(`const char* const kRarityName[6] = ${arr([0, 1, 2, 3, 4, 5].map(r => esc(G.Gacha.RARITY_NAME[r] || '')))};`);
w(`const uint32_t kRarityColor[6] = ${arr([0, 1, 2, 3, 4, 5].map(r => col(G.Gacha.RARITY_COLOR[r] || '#000000')))};`);

// ---------------- compañeros y test ----------------
const traits = Object.keys(G.TRAIT_TEXT);
w(`const StarterDef kStarters[] = ${arr(G.STARTERS.map(s => `{${s.dex}, ${mIdx(s.start)}, {${esc(s.traits[0])}, ${esc(s.traits[1])}}}`))};`);
w(`const int kStarterCount = ${G.STARTERS.length};`);
w(`const TraitText kTraits[] = ${arr(traits.map(t => `{${esc(t)}, ${esc(G.TRAIT_TEXT[t])}}`))};`);
w(`const int kTraitCount = ${traits.length};`);
w('const TestQuestion kTest[] = {');
for (const q of G.PERSONALITY_TEST) {
  if (q.a.length !== 4) fail('pregunta sin 4 respuestas');
  w(`  {${esc(q.q)}, ${arr(q.a.map(a => {
    const p = Object.entries(a.p);
    if (p.length > 3) fail('respuesta con más de 3 rasgos');
    return `{${esc(a.t)}, ${p.length}, ${arr(p.map(([t, n]) => { if (traits.indexOf(t) < 0) fail('rasgo ' + t); return `{${traits.indexOf(t)}, ${n}}`; }))}}`;
  }))}},`);
}
w('};');
w(`const int kTestCount = ${G.PERSONALITY_TEST.length};`);

// ---------------- misiones y logros ----------------
const rew = r => `{${r.t1 || 0}, ${r.t10 || 0}, ${r.coins || 0}}`;
const missions = [...G.MissionDefs.daily.map(m => [m, false]), ...G.MissionDefs.weekly.map(m => [m, true])];
w(`MissionDef kMissions[] = ${arr(missions.map(([m, wk]) => {
  if (EVS.indexOf(m.ev) < 0) fail('evento ' + m.ev);
  return `{${esc(m.id)}, ${EVS.indexOf(m.ev)}, ${wk}, ${m.mode === 'max'}, ${m.param === 'type'}, ${m.goals.length}, ${arr([0, 1, 2].map(i => m.goals[i] || 0))}, ${rew(m.reward)}}`;
}))};`);
w(`const int kMissionCount = ${missions.length};`);
w('AchDef kAch[] = {');
for (const a of G.AchDefs.LIST) w(`  {${esc(a.id)}, ${esc(a.cat)}, ${a.tier}, ${esc(a.name)}, ${esc(a.desc)}, ${a.goal}, ${rew(a.reward)}, ${esc(a.title || null)}, ${a.dex || 0}},`);
w('};');
w(`const int kAchCount = ${G.AchDefs.LIST.length};`);
w(`const AchCat kAchCats[] = ${arr(G.AchDefs.CATS.map(c => `{${esc(c[0])}, ${esc(c[1])}}`))};`);
w(`const int kAchCatCount = ${G.AchDefs.CATS.length};`);

// ---------------- idioma ----------------
const tr = Object.entries(EN).filter(([es, en]) => en && es !== en);
w('const Tr kLangEn[] = {');
for (const [es, en] of tr) w(`  {${esc(es)}, ${esc(en)}},`);
w('};');
w(`const int kLangEnCount = ${tr.length};`);

w('}  // namespace ps');

// ---------------- comprobaciones ----------------
const sumDex = G.DEX.reduce((a, p) => a + p.dex + p.hp + p.spd + p.bst, 0);
const check = {
  pokemon: G.DEX.length, movimientos: G.Moves.ALL.length, objetos: G.Items.LIST.length,
  mejoras: G.Upgrades.LIST.length, biomas: G.Tiles.BIOMES.length, arenas: G.Tiles.ARENA_TYPES.length,
  logros: G.AchDefs.LIST.length, misiones: missions.length, traducciones: tr.length,
  duraciones: durs.length, sumaDex: sumDex
};
if (check.pokemon !== 966) fail('Se esperaban 966 Pokémon');
const expDur = G.DEX.reduce((a, p) => { const m = G.SPRITE_META[p.dex]; let n = 0; for (const k of ANIMS) if (m.a[k]) n += m.a[k].d.length; if (m.sa) for (const k of ANIMS) if (m.sa[k]) n += m.sa[k].d.length; return a + n; }, 0);
if (expDur !== durDex) fail('duraciones perdidas');

// Configuración pública de Firebase de la web (para sincronizar la partida con
// pokesurvivor.com desde la 3DS; no es secreta: va en la web).
{
  const fb = {};
  vm.runInContext(fs.readFileSync(path.join(WEB, 'js/core/firebase-config.js'), 'utf8').replace(/G\.FIREBASE_CONFIG/g, '__FB.cfg'), Object.assign(sandbox, { __FB: fb }));
  const c = fb.cfg || {};
  w('namespace ps {');
  w(`extern const char* const kWebApiKey; const char* const kWebApiKey = ${JSON.stringify(c.apiKey || '')};`);
  w(`extern const char* const kWebProject; const char* const kWebProject = ${JSON.stringify(c.projectId || '')};`);
  w('}  // namespace ps');
}

if (REPORT) {
  // Modo informe: sólo la lista de problemas (y las cuentas, para ver qué ha crecido).
  const rep = JSON.stringify({ problemas: problems, cuentas: check }, null, 1);
  if (ARGS[0]) fs.writeFileSync(ARGS[0], rep); else console.log(rep);
  process.exit(0);
}
// ---------------- paquete de datos (lo que la 3DS puede actualizar sola) ----------------
// La consola baja este paquete (publicado por tools/web/publicar_datos.sh) y
// cambia con él sus tablas sin cambiar de programa: números de equilibrio,
// nombres, textos y traducciones. Sólo si la FORMA de los datos es la misma que
// la que entiende su programa (kDataSchema): mismos movimientos y familias,
// mismos Pokémon y tipos, mismas mejoras, misiones y logros... Si la web cambia
// algo de eso, hace falta un programa nuevo (y el paquete nuevo no le vale al viejo).
// La huella del cooperativo (kDataHash) es la de los datos del paquete: dos
// consolas con los mismos datos juegan juntas aunque uno los tenga de fábrica.
const fnv = str => { let h = 2166136261; for (const b of Buffer.from(str, 'utf8')) { h ^= b; h = Math.imul(h, 16777619) >>> 0; } return h; };
const n6 = v => (v == null ? 0 : +v);   // (precisión completa: la consola los lee igual que las tablas compiladas)
const forma = JSON.stringify({
  tipos: TYPES, familias: KINDS, numeros: MSTATS, buffs: BUFFS, nivel: LSTATS, objetos: ITEMS, mejoras: UPS, climas: WX, eventos: EVS,
  dex: G.DEX.map(p => [p.dex, p.types]), movs: G.Moves.ALL.map(m => [m.id, m.kind, m.type, m.stat || null, m.cat || null]),
  tiers: G.Enemies.TIERS.length, jefes: G.Enemies.BOSS_SLOTS.length, misiones: missions.map(([m, wk]) => [m.id, m.ev, wk, m.mode || null, m.param || null, m.goals.length]),
  logros: G.AchDefs.LIST.map(a => [a.id, a.cat, a.tier, a.dex || 0]), starters: G.STARTERS.map(x => [x.dex, x.start])
});
const schema = fnv(forma).toString(16).padStart(8, '0');
const ladderOut = l => [l.t, Object.entries(l.m).map(([k, v]) => [MSTATS.indexOf(k), n6(v)])];
const datos = {
  dex: G.DEX.map(p => [p.name, p.hp, p.spd, n6(p.atk), n6(p.pa), n6(p.sa), p.df, p.sd, p.rarity, p.bst]),
  movs: G.Moves.ALL.map(m => { const evo = G.Moves.EVO[m.id]; return [m.name, m.desc, MSTATS.map(k => (m[k] == null ? null : n6(m[k]))), evo ? LSTATS.indexOf(evo[1]) : -1, evo ? evo[0] : null]; }),
  escaleras: KINDS.map(k => G.__MV.LADDERS[k].map(ladderOut)),
  evos: KINDS.map(k => (G.__MV.EVO_MODS[k] ? ladderOut(G.__MV.EVO_MODS[k]) : null)),
  tabla: TYPES.map(a => TYPES.map(d => { const r = G.Types.CHART[a] || {}; return n6(r[d] == null ? 1 : r[d]); })),
  nivel: G.__LV.STATS.map(x => [x.title, x.desc, x.max || 0]),
  objetos: G.Items.LIST.map(i => [i.name, i.desc]),
  mejoras: G.Upgrades.LIST.map(u => [u.name, u.unit, u.max, u.base, n6(u.grow)]),
  tiers: G.Enemies.TIERS.map(t => [t.from, t.bst[0], t.bst[1], n6(t.hp), n6(t.spd), n6(t.dmg), n6(t.xp), t.cast]),
  jefes: G.Enemies.BOSS_SLOTS.map(b => [b.at, b.hp, b.dmg, b.xp, b.bst[0], b.bst[1]]),
  climas: WX.map(k => TYPES.map(t => n6(G.Weather.KINDS[k].mult[t] || 1))),
  gacha: [G.Gacha.RATES.map(r => n6(r[1])), [0, 1, 2, 3, 4, 5].map(r => G.Gacha.REFUND[r] || 0)],
  misiones: missions.map(([m]) => [[0, 1, 2].map(i => m.goals[i] || 0), [m.reward.t1 || 0, m.reward.t10 || 0, m.reward.coins || 0]]),
  logros: G.AchDefs.LIST.map(a => [a.name, a.desc, a.goal, [a.reward.t1 || 0, a.reward.t10 || 0, a.reward.coins || 0], a.title || null]),
  en: tr
};
const datosTxt = JSON.stringify(datos);
const dataHash = fnv(datosTxt);
{
  const up = path.join(WEB, 'UPSTREAM');
  const lines = fs.existsSync(up) ? fs.readFileSync(up, 'utf8').split('\n') : [];
  const sha = (lines[0] || '').trim().slice(0, 7), date = (lines[1] || '').trim().slice(0, 10);
  w('namespace ps {');
  w(`extern const char* const kWebBase; const char* const kWebBase = ${JSON.stringify(sha)};`);
  w(`extern const char* const kWebBaseDate; const char* const kWebBaseDate = ${JSON.stringify(date)};`);
  w(`extern uint32_t kDataHash; uint32_t kDataHash = 0x${dataHash.toString(16).padStart(8, '0')}u;`);
  w(`extern const char* const kDataSchema; const char* const kDataSchema = ${JSON.stringify(schema)};`);
  w('}  // namespace ps');
  // El paquete, si se pide (PS_PAQUETE=archivo.json).
  if (process.env.PS_PAQUETE) {
    const pack = `{"formato":1,"esquema":${JSON.stringify(schema)},"web":${JSON.stringify(sha)},"fecha":${JSON.stringify(date)},"huella":${dataHash},"datos":${datosTxt}}`;
    fs.mkdirSync(path.dirname(path.resolve(process.env.PS_PAQUETE)), { recursive: true });
    fs.writeFileSync(process.env.PS_PAQUETE, pack);
    console.log(`gen_data: paquete ${process.env.PS_PAQUETE} (${(pack.length / 1024).toFixed(0)} KB, esquema ${schema})`);
  }
}
fs.mkdirSync(path.dirname(OUT), { recursive: true });
const text = out.join('\n') + '\n';
// Sólo se reescribe si cambia (así make no recompila sin motivo).
if (!fs.existsSync(OUT) || fs.readFileSync(OUT, 'utf8') !== text) fs.writeFileSync(OUT, text);
fs.writeFileSync(OUT.replace(/\.cpp$/, '.check.json'), JSON.stringify(check, null, 1));
console.log('gen_data:', Object.entries(check).map(([k, v]) => k + ' ' + v).join(', '));

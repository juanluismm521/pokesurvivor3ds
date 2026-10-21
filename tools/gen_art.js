#!/usr/bin/env node
/* gen_art.js — el pixel art que la web pinta en código → atlas de la 3DS.
 *
 *   node tools/gen_art.js [carpeta_salida]      (por defecto romfs/gfx)
 *
 * Ejecuta en node el mismo código de dibujo de la web (tiles.js, vfx.js,
 * icons.js) con un canvas por software (tools/lib/softcanvas.js), así sale
 * idéntico sin reescribirlo. Escribe tres atlas (.t3x con tex3ds) y su índice
 * (.idx, una línea por imagen: "nombre x y w h"):
 *   props   objetos del escenario de los 22 biomas (árbol, roca, cofre...) y la grieta
 *   vfx     formas de los ataques en las 19 paletas (y sus fotogramas)
 *   icons   iconos de la interfaz, emblemas de tipo y de movimiento, y las
 *           megapiedras del port (assets-src/megas/piedras, "stone:<n.º de mega>")
 * Los destellos blancos no se exportan: la consola tiñe la imagen de blanco.
 */
'use strict';
const fs = require('fs'), path = require('path'), vm = require('vm'), zlib = require('zlib');
const { execFileSync } = require('child_process');
const { SoftCanvas } = require('./lib/softcanvas');

const ROOT = path.join(__dirname, '..');
const WEB = path.join(ROOT, 'reference', 'web');
const OUT = process.argv[2] || path.join(ROOT, 'romfs', 'gfx');
const TMP = path.join(ROOT, 'build', 'art_tmp');
const TEX3DS = path.join(process.env.DEVKITPRO || '/opt/devkitpro', 'tools', 'bin', 'tex3ds');
fs.mkdirSync(OUT, { recursive: true });
fs.mkdirSync(TMP, { recursive: true });

const sandbox = require('./lib/websandbox').create();
sandbox.document.createElement = () => new SoftCanvas();
const G = sandbox.G;
const load = f => vm.runInContext(fs.readFileSync(path.join(WEB, f), 'utf8').replace(/\r\n/g, '\n'), sandbox, { filename: f });
for (const f of ['js/core/util.js', 'js/data/pokedex.js', 'js/data/sprites-meta.js', 'js/core/sprites.js', 'js/data/moves.js',
                 'js/world/tiles.js', 'js/entities/vfx.js', 'js/ui/icons.js']) load(f);

// ---------------- PNG ----------------
const CRC = (() => { const t = new Uint32Array(256); for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; t[n] = c >>> 0; } return t; })();
function crc32(b) { let c = 0xffffffff; for (const x of b) c = CRC[(c ^ x) & 255] ^ (c >>> 8); return (c ^ 0xffffffff) >>> 0; }
function chunk(type, data) {
  const len = Buffer.alloc(4); len.writeUInt32BE(data.length);
  const td = Buffer.concat([Buffer.from(type, 'ascii'), data]);
  const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(td));
  return Buffer.concat([len, td, crc]);
}
function writePng(file, w, h, rgba) {
  const raw = Buffer.alloc((w * 4 + 1) * h);
  for (let y = 0; y < h; y++) { raw[y * (w * 4 + 1)] = 0; Buffer.from(rgba.buffer, rgba.byteOffset + y * w * 4, w * 4).copy(raw, y * (w * 4 + 1) + 1); }
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4); ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = ihdr[11] = ihdr[12] = 0;
  fs.writeFileSync(file, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]));
}

// PNG RGBA de 8 bits sin entrelazar (las megapiedras: las guarda así fetch_megas.py).
function readPng(file) {
  const b = fs.readFileSync(file);
  let p = 8, w = 0, h = 0, idat = [];
  while (p < b.length) {
    const len = b.readUInt32BE(p), type = b.toString('ascii', p + 4, p + 8), d = b.subarray(p + 8, p + 8 + len);
    if (type === 'IHDR') {
      w = d.readUInt32BE(0); h = d.readUInt32BE(4);
      if (d[8] !== 8 || d[9] !== 6 || d[12] !== 0) throw new Error(file + ': no es RGBA de 8 bits sin entrelazar');
    } else if (type === 'IDAT') idat.push(d);
    p += 12 + len;
  }
  const raw = zlib.inflateSync(Buffer.concat(idat)), out = new Uint8Array(w * h * 4), st = w * 4;
  for (let y = 0; y < h; y++) {
    const f = raw[y * (st + 1)], row = raw.subarray(y * (st + 1) + 1, (y + 1) * (st + 1));
    for (let x = 0; x < st; x++) {
      const a = x >= 4 ? out[y * st + x - 4] : 0, up = y > 0 ? out[(y - 1) * st + x] : 0, c = x >= 4 && y > 0 ? out[(y - 1) * st + x - 4] : 0;
      let v = row[x];
      if (f === 1) v += a; else if (f === 2) v += up; else if (f === 3) v += (a + up) >> 1;
      else if (f === 4) { const pp = a + up - c, pa = Math.abs(pp - a), pb = Math.abs(pp - up), pc = Math.abs(pp - c); v += pa <= pb && pa <= pc ? a : pb <= pc ? up : c; }
      out[y * st + x] = v & 255;
    }
  }
  return { width: w, height: h, data: out };
}

// ---------------- atlas ----------------
function atlas(name, items) {
  // items: [{ name, cv }] — estantes por alto, en la potencia de 2 más pequeña.
  const order = items.map((it, i) => i).sort((a, b) => items[b].cv.height - items[a].cv.height || items[b].cv.width - items[a].cv.width);
  let best = null;
  for (const W of [128, 256, 512, 1024]) {
    if (items.some(it => it.cv.width + 1 > W)) continue;
    const pos = new Array(items.length);
    let x = 0, y = 0, shelf = 0;
    for (const i of order) {
      const w = items[i].cv.width + 1, h = items[i].cv.height + 1;      // 1 px de separación
      if (x + w > W) { x = 0; y += shelf; shelf = 0; }
      pos[i] = [x, y]; x += w; shelf = Math.max(shelf, h);
    }
    let H = 8; while (H < y + shelf) H *= 2;
    if (H > 1024) continue;
    if (!best || W * H < best.W * best.H) best = { W, H, pos };
  }
  if (!best) throw new Error(name + ': no cabe en 1024×1024');
  const px = new Uint8Array(best.W * best.H * 4);
  const idx = [];
  items.forEach((it, i) => {
    const [x0, y0] = best.pos[i], cv = it.cv;
    for (let y = 0; y < cv.height; y++) for (let x = 0; x < cv.width; x++) {
      const s = (y * cv.width + x) * 4, d = ((y0 + y) * best.W + x0 + x) * 4;
      for (let k = 0; k < 4; k++) px[d + k] = cv.data[s + k];
    }
    idx.push(`${it.name} ${x0} ${y0} ${cv.width} ${cv.height}`);
  });
  const png = path.join(TMP, name + '.png');
  writePng(png, best.W, best.H, px);
  execFileSync(TEX3DS, ['-f', 'rgba8888', '-z', 'auto', '-o', path.join(OUT, name + '.t3x'), png]);
  fs.writeFileSync(path.join(OUT, name + '.idx'), `${best.W} ${best.H}\n` + idx.join('\n') + '\n');
  console.log(`gen_art: ${name} ${items.length} imágenes en ${best.W}×${best.H}`);
  return { W: best.W, H: best.H, n: items.length };
}

// ---------------- objetos del escenario ----------------
const props = [];
const VARIANTS = { tree: [0], rock: [0, 1], chest: [0], grass: [0, 1], altar: [0, 1, 2, 3, 4, 5, 6, 7], spring: [0, 1], bigchest: [0], trap: [0, 1, 2] };
G.Tiles.BIOMES.forEach((B, bi) => {
  for (const [type, vs] of Object.entries(VARIANTS)) for (const v of vs) props.push({ name: `${bi}/${type}/${v}`, cv: G.Tiles.propSprite(bi, type, v) });
});
for (let v = 0; v < 4; v++) props.push({ name: `rift/${v}`, cv: G.Tiles.propSprite(0, 'rift', v) });

// ---------------- formas de los ataques ----------------
const vfx = [];
for (const shape of Object.keys(G.VFX.SHAPES)) {
  const frames = G.VFX.SHAPES[shape].frames || 1;
  for (const pal of Object.keys(G.VFX.PAL)) for (let f = 0; f < frames; f++) vfx.push({ name: `${shape}|${pal}|${f}`, cv: G.VFX.sprite(shape, pal, f) });
}

// ---------------- iconos ----------------
const icons = [];
for (const name of Object.keys(G.Icons.UI)) icons.push({ name, cv: G.Icons.canvas(name) });
for (const t of Object.keys(G.U.TYPE_NAME)) icons.push({ name: 'type:' + t, cv: G.Icons.canvas('type:' + t) });
for (const m of G.Moves.ALL) icons.push({ name: 'move:' + m.id, cv: G.Icons.canvas('move:' + m.id) });

// Megapiedras (del port): cada una con su dibujo, por número de mega (kMegas).
const MEGAS = JSON.parse(fs.readFileSync(path.join(ROOT, 'assets-src', 'megas', 'megas.json'), 'utf8')).megas;
MEGAS.forEach((m, i) => {
  const f = m.stone_item && path.join(ROOT, 'assets-src', 'megas', 'piedras', m.stone_item + '.png');
  if (f && fs.existsSync(f)) icons.push({ name: 'stone:' + i, cv: readPng(f) });
});

atlas('props', props);
atlas('vfx', vfx);
atlas('icons', icons);

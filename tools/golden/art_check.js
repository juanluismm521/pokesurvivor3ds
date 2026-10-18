#!/usr/bin/env node
/* art_check.js — comprueba que el canvas por software de gen_art
 * (tools/lib/softcanvas.js) pinta lo mismo que un navegador de verdad.
 *
 *   node tools/golden/art_check.js
 *
 * Genera todas las imágenes de los atlas (objetos del escenario, formas de
 * los ataques e iconos) dos veces: en node con SoftCanvas y en Chromium
 * (Playwright) con el canvas real, y las compara píxel a píxel. En los
 * píxeles semitransparentes se admite ±2 por canal (el navegador guarda el
 * color premultiplicado y redondea distinto). Sale con código 1 si algo
 * difiere.
 */
'use strict';
const fs = require('fs'), path = require('path'), vm = require('vm');
const { SoftCanvas } = require('../lib/softcanvas');
const { chromium } = require(path.join(__dirname, '..', 'node_modules', 'playwright'));

const ROOT = path.join(__dirname, '..', '..');
const WEB = path.join(ROOT, 'reference', 'web');
const FILES = ['js/core/util.js', 'js/data/pokedex.js', 'js/data/sprites-meta.js', 'js/core/sprites.js', 'js/data/moves.js',
               'js/world/tiles.js', 'js/entities/vfx.js', 'js/ui/icons.js'];
const src = f => fs.readFileSync(path.join(WEB, f), 'utf8').replace(/\r\n/g, '\n');

// La misma lista de imágenes que gen_art.js (se ejecuta igual en los dos lados).
function enumerate(G) {
  const out = [];
  const VARIANTS = { tree: [0], rock: [0, 1], chest: [0], grass: [0, 1], altar: [0, 1, 2, 3, 4, 5, 6, 7], spring: [0, 1], bigchest: [0], trap: [0, 1, 2] };
  G.Tiles.BIOMES.forEach((B, bi) => {
    for (const [type, vs] of Object.entries(VARIANTS)) for (const v of vs) out.push([`${bi}/${type}/${v}`, G.Tiles.propSprite(bi, type, v)]);
  });
  for (let v = 0; v < 4; v++) out.push([`rift/${v}`, G.Tiles.propSprite(0, 'rift', v)]);
  for (const shape of Object.keys(G.VFX.SHAPES)) {
    const frames = G.VFX.SHAPES[shape].frames || 1;
    for (const pal of Object.keys(G.VFX.PAL)) for (let f = 0; f < frames; f++) out.push([`${shape}|${pal}|${f}`, G.VFX.sprite(shape, pal, f)]);
  }
  for (const name of Object.keys(G.Icons.UI)) out.push([name, G.Icons.canvas(name)]);
  for (const t of Object.keys(G.U.TYPE_NAME)) out.push(['type:' + t, G.Icons.canvas('type:' + t)]);
  for (const m of G.Moves.ALL) out.push(['move:' + m.id, G.Icons.canvas('move:' + m.id)]);
  return out;
}

(async () => {
  // --- node + SoftCanvas ---
  const sandbox = require('../lib/websandbox').create();
  sandbox.document.createElement = () => new SoftCanvas();
  for (const f of FILES) vm.runInContext(src(f), sandbox, { filename: f });
  const soft = enumerate(sandbox.G).map(([name, cv]) => ({ name, w: cv.width, h: cv.height, data: Buffer.from(cv.data.buffer, cv.data.byteOffset, cv.data.length) }));

  // --- Chromium ---
  const browser = await chromium.launch();
  const page = await browser.newPage();
  await page.setContent('<!doctype html><html><body></body></html>');
  await page.evaluate(() => { window.G = {}; });
  for (const f of FILES) await page.addScriptTag({ content: src(f) + `\n//# sourceURL=${f}` });
  const real = await page.evaluate(`(${enumerate.toString()})(window.G).map(([name, cv]) => {
    const d = cv.width && cv.height ? cv.getContext('2d').getImageData(0, 0, cv.width, cv.height).data : new Uint8ClampedArray(0);
    let s = ''; for (let i = 0; i < d.length; i += 0x8000) s += String.fromCharCode.apply(null, d.subarray(i, i + 0x8000));
    return { name, w: cv.width, h: cv.height, b64: btoa(s) };
  })`);
  await browser.close();

  // --- comparación ---
  let bad = 0, px = 0, semi = 0;
  if (real.length !== soft.length) { console.log(`art_check: ${soft.length} imágenes en node y ${real.length} en Chromium`); process.exit(1); }
  for (let i = 0; i < soft.length; i++) {
    const a = soft[i], b = real[i], bd = Buffer.from(b.b64, 'base64');
    if (a.name !== b.name || a.w !== b.w || a.h !== b.h) { console.log(`  ${a.name}: tamaño ${a.w}×${a.h} ≠ ${b.name} ${b.w}×${b.h}`); bad++; continue; }
    let diff = 0;
    for (let p = 0; p < a.w * a.h; p++) {
      const o = p * 4;
      if (a.data[o + 3] === 0 && bd[o + 3] === 0) continue;
      px++;
      const tol = a.data[o + 3] === 255 && bd[o + 3] === 255 ? 0 : 2;
      if (tol) semi++;
      for (let k = 0; k < 4; k++) if (Math.abs(a.data[o + k] - bd[o + k]) > tol) { diff++; break; }
    }
    if (diff) { console.log(`  ${a.name}: ${diff} píxeles distintos`); bad++; }
  }
  console.log(`art_check: ${soft.length} imágenes, ${px} píxeles pintados (${semi} semitransparentes), ${bad} imágenes distintas`);
  process.exit(bad ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });

#!/usr/bin/env node
/* volcar.js — tablas de la web partida (tools/web/partir.js) en JSON, para las
 * herramientas en Python (fetch_gran.py, gen_sprites.py).
 *
 *   node tools/web/volcar.js [carpeta_web] > tablas.json
 *
 * dex (los 1025 y las formas regionales, con su id de la web), meta (SPRITE_META),
 * megas (slug → id de sprite y ficha), evos (dex → siguientes) y familia (dex → base).
 */
'use strict';
const fs = require('fs'), path = require('path'), vm = require('vm');
const WEB = process.argv[2] || path.join(__dirname, '..', '..', 'reference', 'web');
process.env.PS_WEB = WEB;
const sb = require('../lib/websandbox').create();
const G = sb.G;
for (const f of ['js/core/util.js', 'js/core/i18n.js', 'js/data/pokedex.js', 'js/data/sprites-meta.js', 'js/data/evos.js', 'js/data/megas.js',
                 'js/data/abilities.js', 'js/data/tms.js', 'js/data/forms.js'])
  vm.runInContext(fs.readFileSync(path.join(WEB, f), 'utf8'), sb, { filename: f });
const megas = {};
for (const d in G.MEGAS) megas[G.MEGAS[d].slug] = Object.assign({ id: 100000 + +d, dex: +d }, G.MEGAS[d]);
for (const d in G.MEGAS_ALT) for (const m of G.MEGAS_ALT[d]) megas[m.slug] = Object.assign({ dex: +d }, m);
process.stdout.write(JSON.stringify({ dex: G.DEX, meta: G.SPRITE_META, megas, evos: G.EVOS, familia: G.FAMILY,
                                      formas: G.FORMS.list.map(f => ({ id: f.id, d: f.d, k: f.k, slug: f.slug, n: f.n })) }));

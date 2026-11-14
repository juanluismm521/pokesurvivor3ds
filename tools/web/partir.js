#!/usr/bin/env node
/* partir.js — la web ofuscada (app.min.js pasado por webcrack) → un árbol de archivos
 * como el de reference/web, con los nombres de los módulos repuestos.
 *
 *   node tools/web/partir.js deobfuscated.js carpeta_salida
 *
 * Desde nov-2026 la web sólo publica js/app.min.js (javascript-obfuscator). webcrack
 * (npm) lo deja legible: se pierden los nombres de las variables locales, pero las
 * claves de los objetos, los textos y la estructura (un bloque por módulo) siguen ahí.
 * Los módulos cuelgan de un objeto global con nombres cambiados (_4596f60 = DEX…):
 * tools/web/nombres.json dice cuál es cada uno y en qué archivo va. Lo que no se
 * conoce cae en js/gran/ con su nombre ofuscado. Escribe también dev.html con el
 * orden de carga (lo usan partida.js y los cargadores) y nombres-vistos.json (los
 * miembros de cada módulo, para volver a emparejarlos si la web cambia los nombres).
 */
'use strict';
const fs = require('fs'), path = require('path');
const [src, out] = process.argv.slice(2);
if (!src || !out) { console.error('uso: partir.js deobfuscated.js carpeta'); process.exit(2); }
const NAMES = JSON.parse(fs.readFileSync(path.join(__dirname, 'nombres.json'), 'utf8'));
const lines = fs.readFileSync(src, 'utf8').split('\n');
const m0 = /^\s*var (_0x[0-9a-f]+) = Object\.create\(null\);/.exec(lines[1]);
if (!m0) throw new Error('no encuentro el objeto global en la línea 2');
const ROOT = m0[1];
// Trozos de primer nivel (dos espacios de sangría)
const stmts = [];
for (let i = 2; i < lines.length - 1; i++) {
  const l = lines[i];
  if (/^  [^ })\]]/.test(l)) stmts.push({ from: i, text: [l] });
  else if (stmts.length) stmts[stmts.length - 1].text.push(l);
}
const reRoot = new RegExp(ROOT.replace(/\$/g, '\\$') + '\\.(_[0-9a-f]{7})\\b', 'g');
const rename = t => t.replace(reRoot, (m, k) => 'G.' + (NAMES[k] && k !== '_' ? NAMES[k][0] : k)).split(ROOT).join('G');
const fileOf = {};   // nombre visible → archivo
const KNOWN_FILE = {
  U: 'js/core/util.js', T: 'js/core/i18n.js', Settings: 'js/core/settings.js', Sprites: 'js/core/sprites.js', Skins: 'js/core/skins.js',
  Marcos: 'js/data/marcos.js', Fotos: 'js/data/fotos.js', Halloween: 'js/systems/halloween.js', Upgrades: 'js/data/upgrades.js',
  Items: 'js/data/items.js', Shop: 'js/data/shop.js', Perfil: 'js/systems/perfil.js', Aura: 'js/entities/aura.js', FX: 'js/entities/fx.js',
  Stats: 'js/systems/stats.js', Gacha: 'js/systems/gacha.js', Judgment: 'js/systems/judgment.js', Rocket: 'js/systems/rocket.js',
  Seen: 'js/systems/seen.js', Eggs: 'js/systems/eggs.js', Ranking: 'js/net/ranking.js', UI: 'js/ui/ui.js', HUD: 'js/ui/hud.js'
};
const files = new Map(), order = [];
let last = 'js/core/util.js';
const put = (f, text) => { if (!files.has(f)) { files.set(f, []); order.push(f); } files.get(f).push(text); last = f; };
for (const s of stmts) {
  const head = s.text[0];
  let m = new RegExp('^  ' + ROOT.replace(/\$/g, '\\$') + '\\.([A-Za-z_$][\\w$]*) = ').exec(head);
  let f = null;
  if (m) {
    const k = m[1];
    if (NAMES[k] && k !== '_') f = NAMES[k][1];
    else if (KNOWN_FILE[k]) f = KNOWN_FILE[k];
    else if (/^_[0-9a-f]{7}$/.test(k)) f = 'js/gran/' + k + '.js';
  }
  // Los diccionarios de inglés (antes js/data/lang-en.js)
  if (new RegExp('^  ' + ROOT.replace(/\$/g, '\\$') + '\\._8a1daf5\\.add\\(').test(head) && NAMES._8a1daf5) f = 'js/data/lang-en.js';
  // Un cierre suelto que define un módulo por dentro (Moves, EnemyMgr…) va a su archivo
  if (!f && /^  \(\(\) => \{/.test(head)) {
    const body = s.text.join('\n');
    for (const mm of body.matchAll(new RegExp('^    ' + ROOT.replace(/\$/g, '\\$') + '\\.([A-Za-z_$][\\w$]*) = ', 'gm'))) {
      const k = mm[1];
      if (NAMES[k]) { f = NAMES[k][1]; break; }
      if (KNOWN_FILE[k]) { f = KNOWN_FILE[k]; break; }
    }
  }
  // (lo que no define un módulo — bucles, I18n.add — va con el anterior)
  put(f || last, rename(s.text.map(l => l.slice(2)).join('\n')));
}
// (lo que la web sirve sin ofuscar y bajar.sh deja al lado se conserva)
const SUELTOS = ['js/core/locales.js', 'js/boot.js', 'js/noframe.js', 'js/core/beta.js'];
const guardados = SUELTOS.filter(f => fs.existsSync(path.join(out, f))).map(f => [f, fs.readFileSync(path.join(out, f))]);
fs.rmSync(path.join(out, 'js'), { recursive: true, force: true });
for (const [f, parts] of files) {
  const p = path.join(out, f);
  fs.mkdirSync(path.dirname(p), { recursive: true });
  fs.writeFileSync(p, "'use strict';\n" + parts.join('\n') + '\n');
}
for (const [f, data] of guardados) { fs.mkdirSync(path.dirname(path.join(out, f)), { recursive: true }); fs.writeFileSync(path.join(out, f), data); }
fs.writeFileSync(path.join(out, 'dev.html'), '<!-- orden de carga (hecho por tools/web/partir.js) -->\n' +
  (fs.existsSync(path.join(out, 'js/core/locales.js')) ? ['js/core/locales.js'] : []).concat(order).map(f => `<script src="${f}"></script>`).join('\n') + '\n');
// Miembros de cada módulo (para reconocerlos si cambian los nombres ofuscados)
const all = lines.join('\n'), seen = {};
for (const m of all.matchAll(new RegExp(ROOT.replace(/\$/g, '\\$') + '\\.(_[0-9a-f]{7})\\.([A-Za-z_$][\\w$]*)', 'g')))
  (seen[m[1]] = seen[m[1]] || new Set()).add(m[2]);
const visto = {};
for (const k in seen) visto[NAMES[k] ? NAMES[k][0] : k] = [...seen[k]].sort();
fs.writeFileSync(path.join(out, 'nombres-vistos.json'), JSON.stringify(visto, null, 1));
console.log(`${stmts.length} trozos → ${order.length} archivos en ${out}`);

#!/usr/bin/env node
/* partida.js — juega una partida del juego web ORIGINAL sin navegador, con un
 * bot (tools/web/bot.js), y dice cada N segundos cuánto cuesta cada fotograma
 * (lógica y dibujo con un canvas que no pinta), enemigos, nivel...
 *
 *   node tools/web/partida.js [carpeta_web] [segundos] [cada] [dex] [semilla]
 *
 * Sirve para: ver si una versión nueva de la web arranca y se juega sin
 * errores, medir cuánto pesa su lógica (¿cabría en la 3DS?) y, más adelante,
 * comparar la marcha de una partida de la web con la del port.
 * Con PS_QJS=1 escribe en vez de jugar el paquete para QuickJS (jsbench).
 */
'use strict';
const fs = require('fs'), path = require('path'), vm = require('vm');
const ROOT = path.join(__dirname, '..', '..');
const [web = path.join(ROOT, 'reference', 'web'), secs = '120', every = '20', dex = '25', seed = '12345'] = process.argv.slice(2);

/** Todo el juego en un solo .js: navegador falso + la web (con medida de lógica y dibujo) + el bot. */
function bundle(webDir) {
  const html = fs.readFileSync(path.join(webDir, 'dev.html'), 'utf8');
  const files = [...html.matchAll(/src="(js\/[^"?]+\.js)/g)].map(m => m[1]).filter(f => f !== 'js/main.js');
  const parts = [fs.readFileSync(path.join(__dirname, 'navegador.js'), 'utf8'), 'var __tu = 0, __tr = 0;'];
  for (const f of files) {
    let src = fs.readFileSync(path.join(webDir, f), 'utf8');
    if (f === 'js/game.js') {
      const a = "if (state === 'playing') update(dt);", b = "if (pl && state !== 'ui') render();";
      if (src.includes(a)) src = src.replace(a, "if (state === 'playing') { var __t0 = __clock(); update(dt); __tu += __clock() - __t0; }");
      if (src.includes(b)) src = src.replace(b, "if (pl && state !== 'ui') { var __t1 = __clock(); render(); __tr += __clock() - __t1; }");
    }
    parts.push(`// ==== ${f}\ntry {\n${src}\n} catch (e) { print('ERROR cargando ${f}: ' + e); }\n`);
  }
  parts.push(fs.readFileSync(path.join(__dirname, 'bot.js'), 'utf8'));
  return parts.join('\n');
}

if (process.env.PS_QJS) {
  // Para QuickJS: lo que no trae (console, Intl) y el paquete del juego.
  const pre = fs.readFileSync(path.join(__dirname, 'jsbench', 'quickjs-pre.js'), 'utf8');
  fs.writeFileSync(process.env.PS_QJS, pre + '\n' + bundle(web));
  console.log('paquete para QuickJS:', process.env.PS_QJS);
} else {
  const ctx = { console, print: s => console.log(s), __clock: () => Number(process.hrtime.bigint()) / 1e6 };
  vm.createContext(ctx);
  vm.runInContext(bundle(web), ctx, { filename: 'web.js' });
  vm.runInContext(`__setup(${+dex}, ${+seed}); __bench(${+secs}, ${+every}, __clock);`, ctx);
}

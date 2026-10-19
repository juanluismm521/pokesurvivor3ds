#!/usr/bin/env node
/* forma.js — la "forma" del juego web: qué módulos tiene (G.*), qué funciones
 * exporta cada uno y qué campos tienen sus datos (Pokédex, movimientos,
 * objetos, mejoras, enemigos, logros...). Comparando la forma de dos versiones
 * de la web se ve qué ha cambiado aunque el port no lo lea todavía (p. ej. el
 * 18-10-2026 la Pokédex ganó pa/sa/df/sd y gen_data los ignoraba sin avisar).
 *
 *   node tools/web/forma.js <carpeta_web> [salida.json]
 *
 * Carga TODOS los .js de dev.html (menos main.js) en un navegador de mentira
 * (tools/web/navegador.js) y recorre G. Salida: { "G.DEX[].pa": "number", ... }
 */
'use strict';
const fs = require('fs'), path = require('path'), vm = require('vm');

function load(webDir) {
  const ctx = { console: { log() {}, warn() {}, error() {}, info() {} }, print() {} };
  vm.createContext(ctx);
  vm.runInContext(fs.readFileSync(path.join(__dirname, 'navegador.js'), 'utf8'), ctx, { filename: 'navegador.js' });
  const html = fs.readFileSync(path.join(webDir, 'dev.html'), 'utf8');
  const files = [...html.matchAll(/src="(js\/[^"?]+\.js)/g)].map(m => m[1]).filter(f => f !== 'js/main.js');
  const errors = [];
  for (const f of files) {
    const p = path.join(webDir, f);
    if (!fs.existsSync(p)) { errors.push(f + ': no existe'); continue; }
    try { vm.runInContext(fs.readFileSync(p, 'utf8'), ctx, { filename: f }); }
    catch (e) { errors.push(f + ': ' + e.message); }
  }
  return { G: vm.runInContext('G', ctx), files, errors };
}

const typeOf = v => (v === null ? 'null' : Array.isArray(v) ? 'array' : typeof v);
const isClass = f => typeof f === 'function' && /^class\b/.test(Function.prototype.toString.call(f));

/** Recorre un valor y apunta la forma de cada camino. */
function walk(v, p, out, depth, seen, record = false) {
  const t = typeOf(v);
  if (t === 'function') {
    out[p] = isClass(v) ? 'clase' : 'función';
    if (isClass(v) && depth < 4) {
      for (const k of Object.getOwnPropertyNames(v.prototype)) if (k !== 'constructor') out[p + '#' + k] = 'método';
      for (const k of Object.getOwnPropertyNames(v)) if (!['length', 'name', 'prototype'].includes(k)) walk(v[k], p + '.' + k, out, depth + 1, seen);
    }
    return;
  }
  if (t !== 'object' && t !== 'array') { out[p] = t; return; }
  if (seen.has(v) || depth > 5) { out[p] = t; return; }
  seen.add(v);
  if (t === 'array') {
    // Lista: la unión de los campos de todos sus elementos.
    const objs = v.filter(x => typeOf(x) === 'object');
    if (objs.length) { out[p] = 'lista'; walk(merge(objs), p + '[]', out, depth + 1, seen, true); }
    else out[p] = 'lista<' + [...new Set(v.map(typeOf))].sort().join('|') + '>';
    return;
  }
  let keys;
  try { keys = Object.keys(v); } catch (e) { out[p] = 'objeto'; return; }
  // Diccionario (muchas claves o números: DEX_BY, CHART...): la unión de los valores.
  if (!record && (keys.length > 24 || (keys.length > 3 && keys.every(k => /^\d+$/.test(k))))) {
    out[p] = 'diccionario';
    const vals = keys.map(k => v[k]);
    const objs = vals.filter(x => typeOf(x) === 'object');
    if (objs.length === vals.length) walk(merge(objs), p + '{}', out, depth + 1, seen, true);
    else out[p + '{}'] = [...new Set(vals.map(typeOf))].sort().join('|');
    return;
  }
  out[p] = 'objeto';
  for (const k of keys) {
    let x;
    try { x = v[k]; } catch (e) { continue; }
    walk(x, p + '.' + k, out, depth + 1, seen);
  }
  // Getters del módulo (get state() {...}) también son parte de su forma.
  for (const k of Object.getOwnPropertyNames(v)) {
    const d = Object.getOwnPropertyDescriptor(v, k);
    if (d && d.get && !(p + '.' + k in out)) out[p + '.' + k] = 'propiedad';
  }
}

/** Un objeto con la unión de los campos de varios (el primer valor de cada uno). */
function merge(objs) {
  const m = {};
  for (const o of objs) for (const k of Object.keys(o)) if (!(k in m) || m[k] == null) m[k] = o[k];
  return m;
}

function shape(webDir) {
  const { G, files, errors } = load(webDir);
  const out = {};
  for (const k of Object.keys(G).sort()) walk(G[k], 'G.' + k, out, 0, new WeakSet());
  return { forma: out, archivos: files, errores: errors };
}

if (require.main === module) {
  const [dir, dst] = process.argv.slice(2);
  if (!dir) { console.error('uso: forma.js <carpeta_web> [salida.json]'); process.exit(2); }
  const s = JSON.stringify(shape(dir), null, 1);
  if (dst) fs.writeFileSync(dst, s); else console.log(s);
}
module.exports = { shape };

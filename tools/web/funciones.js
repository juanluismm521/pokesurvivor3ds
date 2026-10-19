#!/usr/bin/env node
/* funciones.js — qué funciones de un .js de la web han cambiado entre dos
 * versiones (nuevas, quitadas y cambiadas), con su línea en la versión nueva.
 * Cada función se compara por su código PROPIO (sin comentarios, sin espacios
 * y sin el cuerpo de las funciones que tiene dentro): si sólo cambia una
 * función interna, sale ésa y no también la de fuera.
 *
 *   node tools/web/funciones.js <viejo.js> <nuevo.js>
 *
 * Nombres: "G.Combat › tickMove", "Player#hurt", "G.Game › Backdrop › step"...
 */
'use strict';
const fs = require('fs');
const acorn = require('acorn');
const walk = require('acorn-walk');

function parse(src) {
  const comments = [];
  let ast;
  try { ast = acorn.parse(src, { ecmaVersion: 'latest', sourceType: 'script', locations: true, onComment: comments, allowHashBang: true }); }
  catch (e) { return { error: e.message }; }
  return { ast, comments };
}

/** Nombre de lo que hay a la izquierda de un "=" (G.Combat, Foo.prototype.bar...). */
function lhsName(n) {
  if (!n) return null;
  if (n.type === 'Identifier') return n.name;
  if (n.type === 'MemberExpression') {
    const o = lhsName(n.object), p = n.computed ? (n.property.type === 'Literal' ? String(n.property.value) : null) : n.property.name;
    return o && p ? o + '.' + p : null;
  }
  return null;
}
const keyName = k => (!k ? null : k.type === 'Identifier' ? k.name : k.type === 'Literal' ? String(k.value) : k.type === 'PrivateIdentifier' ? '#' + k.name : null);
const isFn = n => n && (n.type === 'FunctionExpression' || n.type === 'ArrowFunctionExpression' || n.type === 'FunctionDeclaration');
// (() => { ... })() — un módulo: G.Combat = (() => { ... return {...} })()
const iife = n => n && n.type === 'CallExpression' && isFn(n.callee) ? n.callee : null;

/** Lista de funciones { name, start, end, line } con nombre jerárquico. */
function functions(src) {
  const p = parse(src);
  if (p.error) return { error: p.error };
  const out = [];
  const visit = (node, scope) => {
    if (!node || typeof node.type !== 'string') return;
    const named = (name, fn, extra, mod = false) => {
      const full = scope ? scope + ' › ' + name : name;
      out.push({ name: full, start: fn.start, end: fn.end, line: fn.loc.start.line, mod });
      visitChildren(fn, full, extra);
    };
    switch (node.type) {
      case 'FunctionDeclaration': if (node.id) return named(node.id.name, node);
        break;
      case 'VariableDeclarator':
        if (isFn(node.init)) return named(node.id.name, node.init);
        if (iife(node.init)) return named(node.id.name, iife(node.init), null, true);
        if (node.init && node.init.type === 'ClassExpression') return visit(Object.assign({}, node.init, { id: node.id }), scope);
        break;
      case 'AssignmentExpression': {
        const n = lhsName(node.left);
        if (n && isFn(node.right)) return named(n, node.right);
        if (n && iife(node.right)) return named(n, iife(node.right), null, true);
        if (n && node.right.type === 'ClassExpression') return visit(Object.assign({}, node.right, { id: { name: n } }), scope);
        if (n && node.right.type === 'ObjectExpression') return visitChildren(node.right, scope ? scope + ' › ' + n : n);
        break;
      }
      case 'Property': case 'MethodDefinition': case 'PropertyDefinition': {
        const k = keyName(node.key);
        const v = node.value;
        if (k && isFn(v)) {
          const nm = node.kind === 'get' ? 'get ' + k : node.kind === 'set' ? 'set ' + k : k;
          return named(nm, v);
        }
        if (k && iife(v)) return named(k, iife(v), null, true);
        if (k && v && v.type === 'ObjectExpression') return visitChildren(v, scope ? scope + ' › ' + k : k);
        break;
      }
      case 'ClassDeclaration': case 'ClassExpression': {
        const cn = node.id ? node.id.name : '(clase)';
        const full = scope ? scope + ' › ' + cn : cn;
        for (const m of node.body.body) {
          const k = keyName(m.key);
          if (m.type === 'MethodDefinition' && k) {
            const nm = (m.kind === 'get' ? 'get ' : m.kind === 'set' ? 'set ' : '') + k;
            out.push({ name: full + '#' + nm, start: m.value.start, end: m.value.end, line: m.value.loc.start.line });
            visitChildren(m.value, full + '#' + nm);
          } else visit(m, full);
        }
        return;
      }
    }
    visitChildren(node, scope);
  };
  const visitChildren = (node, scope) => {
    for (const key of Object.keys(node)) {
      if (key === 'loc' || key === 'start' || key === 'end') continue;
      const v = node[key];
      if (Array.isArray(v)) v.forEach(c => c && typeof c.type === 'string' && visit(c, scope));
      else if (v && typeof v.type === 'string') visit(v, scope);
    }
  };
  visit(p.ast, '');
  // Código de nivel superior (fuera de toda función): también cuenta.
  out.push({ name: '(nivel superior)', start: 0, end: src.length, line: 1, mod: true });
  return { list: out, comments: p.comments };
}

/** El código propio de cada función: sin comentarios, sin espacios y sin las funciones de dentro. */
function ownHashes(src) {
  const r = functions(src);
  if (r.error) return r;
  const blank = new Uint8Array(src.length);   // 1 = comentario
  for (const c of r.comments) blank.fill(1, c.start, c.end);
  const map = new Map();
  for (const f of r.list) {
    const inner = r.list.filter(g => g !== f && g.start >= f.start && g.end <= f.end && !(g.start === f.start && g.end === f.end));
    let s = '';
    let i = f.start;
    const holes = inner.sort((a, b) => a.start - b.start).filter((g, k, arr) => !arr.some(h => h !== g && h.start <= g.start && h.end >= g.end && (h.start !== g.start || h.end !== g.end)));
    for (const g of holes) {
      for (; i < g.start; i++) if (!blank[i]) s += src[i];
      s += '\u0000';
      i = g.end;
    }
    for (; i < f.end; i++) if (!blank[i]) s += src[i];
    let name = f.name, n = 2;
    while (map.has(name)) name = f.name + ' (' + n++ + ')';
    map.set(name, { code: s.replace(/\s+/g, ''), line: f.line, mod: !!f.mod });
  }
  return { map };
}

function diff(oldSrc, newSrc) {
  const a = oldSrc == null ? { map: new Map() } : ownHashes(oldSrc);
  const b = newSrc == null ? { map: new Map() } : ownHashes(newSrc);
  if (a.error || b.error) return { error: a.error || b.error };
  const added = [], removed = [], changed = [];
  for (const [k, v] of b.map) {
    if (!a.map.has(k)) added.push({ name: k, line: v.line, mod: v.mod });
    else if (a.map.get(k).code !== v.code) changed.push({ name: k, line: v.line, mod: v.mod });
  }
  for (const [k, v] of a.map) if (!b.map.has(k)) removed.push({ name: k, line: v.line, mod: v.mod });
  return { added, removed, changed };
}

if (require.main === module) {
  const [o, n] = process.argv.slice(2);
  const read = f => (f && fs.existsSync(f) ? fs.readFileSync(f, 'utf8') : null);
  console.log(JSON.stringify(diff(read(o), read(n)), null, 1));
}
module.exports = { diff, functions };

#!/usr/bin/env node
/* normal.js — deja un .js de la web «sin nombres» para poder comparar la versión con
 * fuentes (reference/web-1910…) con la ofuscada y partida (tools/web/partir.js):
 * todas las variables locales pasan a llamarse «v» y el texto se vuelve a escribir con
 * el mismo estilo. El archivo con fuentes se pasa antes por webcrack (mismas reglas de
 * llaves, declaraciones separadas…).
 *
 *   node tools/web/normal.js archivo.js > archivo.norm.js
 *   tools/web/comparar.sh js/systems/combat.js     (diff de las dos versiones)
 */
'use strict';
const fs = require('fs');
const parser = require('@babel/parser'), traverse = require('@babel/traverse').default, generate = require('@babel/generator').default;
const src = fs.readFileSync(process.argv[2], 'utf8');
const ast = parser.parse(src, { sourceType: 'script', errorRecovery: true });
traverse(ast, {
  Scopable(p) { for (const n of Object.keys(p.scope.bindings)) for (const b of [p.scope.bindings[n]]) {
    b.identifier.name = 'v';
    for (const r of b.referencePaths) r.node.name = 'v';
    for (const c of b.constantViolations) traverse.cheap(c.node, x => { if (x.type === 'Identifier' && x.name === n) x.name = 'v'; });
  } },
  NumericLiteral(p) { delete p.node.extra; }, StringLiteral(p) { delete p.node.extra; },
  noScope: false
});
process.stdout.write(generate(ast, { comments: false, compact: false, retainLines: false }).code + '\n');

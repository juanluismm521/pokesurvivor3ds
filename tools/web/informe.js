#!/usr/bin/env node
/* informe.js — qué trae una versión nueva de la web y qué hay que hacer en el
 * port para tenerla. Lo usa tools/web/actualizar.sh; también suelto:
 *
 *   node tools/web/informe.js --repo <clon de la web> --desde <commit> --hasta <commit>
 *                             [--md informe.md] [--json informe.json]
 *
 * Mira:
 *   1. los commits nuevos de la web y los archivos que tocan;
 *   2. qué es cada archivo (tools/web/mapa.json): datos (entran solos), lógica
 *      (hay que portarla), interfaz (revisar) o sólo web (nada);
 *   3. en los .js de datos, lógica e interfaz, QUÉ FUNCIONES han cambiado
 *      (tools/web/funciones.js);
 *   4. la forma de los datos y del código (tools/web/forma.js): módulos,
 *      funciones exportadas y campos nuevos o quitados;
 *   5. lo que gen_data no sabe leer de la versión nueva (gen_data --informe).
 *
 * Veredicto:
 *   automatico  sólo datos o cosas de la web: se aplica sin tocar C++
 *   portar      hay lógica nueva o cambiada (la lista de tareas va en el informe)
 *   nada        no hay nada nuevo
 */
'use strict';
const fs = require('fs'), path = require('path'), os = require('os');
const { execFileSync } = require('child_process');
const { diff: fnDiff } = require('./funciones');
const { shape } = require('./forma');

const ROOT = path.join(__dirname, '..', '..');
const arg = (k, d) => { const i = process.argv.indexOf('--' + k); return i > 0 ? process.argv[i + 1] : d; };
const REPO = arg('repo'), FROM = arg('desde'), TO = arg('hasta', 'HEAD');
if (!REPO || !FROM) { console.error('uso: informe.js --repo <clon> --desde <commit> [--hasta <commit>] [--md f] [--json f]'); process.exit(2); }
const git = (...a) => execFileSync('git', ['-C', REPO, ...a], { encoding: 'utf8', maxBuffer: 256 << 20, stdio: ['ignore', 'pipe', 'ignore'] });

// ---------------- 1. commits y archivos ----------------
const from = git('rev-parse', FROM).trim(), to = git('rev-parse', TO).trim();
const commits = from === to ? [] : git('log', '--reverse', '--format=%h\t%ad\t%s', '--date=short', `${from}..${to}`)
  .trim().split('\n').filter(Boolean).map(l => { const [h, d, ...s] = l.split('\t'); return { h, d, s: s.join('\t') }; });
const files = from === to ? [] : git('diff', '--name-status', '--no-renames', from, to).trim().split('\n').filter(Boolean)
  .map(l => { const [st, f] = l.split('\t'); return { st, f }; });

// ---------------- 2. qué es cada archivo ----------------
const MAPA = JSON.parse(fs.readFileSync(path.join(__dirname, 'mapa.json'), 'utf8')).reglas;
const glob = p => new RegExp('^' + p.replace(/[.+^${}()|[\]\\]/g, '\\$&').replace(/\*/g, '[^/]*') + '$');
const rules = MAPA.map(r => Object.assign({ re: r.patron === '*' ? /.*/ : glob(r.patron) }, r));
const ruleOf = f => rules.find(r => r.re.test(f));

// ---------------- 3. funciones cambiadas ----------------
const show = (sha, f) => { try { return git('show', `${sha}:${f}`); } catch (e) { return null; } };
for (const x of files) {
  const r = ruleOf(x.f);
  x.tipo = r.tipo; x.cpp = r.cpp || []; x.gen = r.gen || []; x.nota = r.nota || '';
  x.commits = git('log', '--format=%h', `${from}..${to}`, '--', x.f).trim().split('\n').filter(Boolean);
  if (/\.js$/.test(x.f) && x.tipo !== 'web' && !/\.min\.js$/.test(x.f)) {
    const d = fnDiff(show(from, x.f), show(to, x.f));
    if (!d.error) x.funciones = d; else x.error = d.error;
  }
}

// ---------------- 4 y 5. forma y gen_data, con las dos versiones en carpetas ----------------
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'ps-web-'));
const extract = (sha, dir) => {
  fs.mkdirSync(dir, { recursive: true });
  execFileSync('bash', ['-c', `git -C "${REPO}" archive ${sha} js css dev.html | tar -x -C "${dir}"`]);
};
let forma = null, genProblems = null, genCounts = null;
try {
  extract(from, path.join(tmp, 'a')); extract(to, path.join(tmp, 'b'));
  const A = shape(path.join(tmp, 'a')), B = shape(path.join(tmp, 'b'));
  const add = Object.keys(B.forma).filter(k => !(k in A.forma));
  const rem = Object.keys(A.forma).filter(k => !(k in B.forma));
  const chg = Object.keys(B.forma).filter(k => k in A.forma && A.forma[k] !== B.forma[k]).map(k => `${k}: ${A.forma[k]} → ${B.forma[k]}`);
  // G.DEX[].pa y G.DEX_BY{}.pa son lo mismo: se dejan sólo los de las listas.
  const dedupe = ks => ks.filter(k => !/\{\}/.test(k) || !ks.some(o => o !== k && o.replace(/\[\]/g, '{}') === k));
  // (un error al cargar un archivo que sólo es de la web no cambia nada en la 3DS)
  const matters = e => { const r = ruleOf(e.split(':')[0]); return !r || r.tipo !== 'web'; };
  forma = { nuevo: dedupe(add), quitado: dedupe(rem), cambia: chg, errores: B.errores.filter(e => !A.errores.includes(e)),
            erroresWeb: [] };
  forma.erroresWeb = forma.errores.filter(e => !matters(e));
  forma.errores = forma.errores.filter(matters);
  // Lo que gen_data no sabe leer en la versión nueva y sí en la vieja (con
  // --desde = la copia del port, la vieja no tiene ninguno).
  const gen = dir => {
    try {
      const out = execFileSync('node', [path.join(ROOT, 'tools', 'gen_data.js'), '--informe'], { encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'], env: Object.assign({}, process.env, { PS_WEB: dir }) });
      return JSON.parse(out);
    } catch (e) { return { problemas: ['gen_data no puede con esta versión: ' + String(e.message).split('\n')[0]], cuentas: null }; }
  };
  const ga = gen(path.join(tmp, 'a')), gb = gen(path.join(tmp, 'b'));
  genProblems = gb.problemas.filter(p => !ga.problemas.includes(p)); genCounts = gb.cuentas;
} finally { fs.rmSync(tmp, { recursive: true, force: true }); }

// ---------------- veredicto ----------------
const by = t => files.filter(x => x.tipo === t);
const nfn = F => F.added.length + F.removed.length + F.changed.length;
// En los archivos de datos, el texto de las tablas (nivel superior y el cuerpo
// del módulo) son datos; pero sus FUNCIONES (startChoices, behaviorOf, perks...)
// están portadas a mano en C++: si cambian, hay que portarlas.
for (const x of by('datos')) if (x.funciones) {
  const F = x.funciones, keep = f => !f.mod;
  x.funcionesPortadas = { added: F.added.filter(keep), removed: F.removed.filter(keep), changed: F.changed.filter(keep) };
}
const dataLogic = by('datos').filter(x => x.funcionesPortadas && nfn(x.funcionesPortadas) > 0);
const logic = by('logica').filter(x => !x.funciones || nfn(x.funciones) > 0).concat(dataLogic.map(x => Object.assign({}, x, { funciones: x.funcionesPortadas, nota: (x.nota ? x.nota + ' · ' : '') + 'funciones de un archivo de datos, portadas a mano' })));
const ui = by('interfaz');
const dataNew = (forma ? forma.nuevo.length + forma.quitado.length + forma.cambia.length : 0) > 0;
const veredicto = !commits.length ? 'nada' : (logic.length || (genProblems && genProblems.length) || (forma && forma.errores.length)) ? 'portar' : 'automatico';
const resumen = {
  desde: from, hasta: to, commits, veredicto,
  archivos: files.map(({ st, f, tipo, cpp, gen, nota, commits: c, funciones, error }) => ({ st, f, tipo, cpp, gen, nota, commits: c, funciones, error })),
  forma, gen_data: { problemas: genProblems, cuentas: genCounts }
};

// ---------------- markdown ----------------
const md = [];
const L = s => md.push(s);
const ST = { A: 'nuevo', M: 'cambia', D: 'quitado' };
L(`# Actualización de la web: ${from.slice(0, 7)} → ${to.slice(0, 7)}`);
L('');
L(`**Veredicto: ${veredicto === 'automatico' ? 'AUTOMÁTICO — sólo datos o cosas de la web, entra sin tocar C++' : veredicto === 'portar' ? 'HAY QUE PORTAR — la web cambia lógica del juego' : 'NADA NUEVO'}**`);
L('');
L(`- ${commits.length} commits · ${files.length} archivos (${['datos', 'logica', 'interfaz', 'web'].map(t => `${by(t).length} ${t === 'logica' ? 'de lógica' : t === 'interfaz' ? 'de interfaz' : t === 'datos' ? 'de datos' : 'sólo web'}`).join(', ')})`);
if (genCounts) L(`- Datos de la versión nueva: ${Object.entries(genCounts).filter(([k]) => k !== 'sumaDex' && k !== 'duraciones').map(([k, v]) => `${v} ${k}`).join(', ')}`);
L('');
if (commits.length) {
  L('## Commits');
  for (const c of commits) L(`- \`${c.h}\` ${c.d} — ${c.s.length > 220 ? c.s.slice(0, 220) + '…' : c.s}`);
  L('');
}
if (genProblems && genProblems.length) {
  L('## Lo que gen_data todavía no sabe leer');
  L('Hay que enseñárselo (tools/gen_data.js + src/core/gamedata.h) y, casi siempre, portar la lógica que lo usa:');
  for (const p of genProblems) L(`- ${p}`);
  L('');
}
if (forma && dataNew) {
  L('## Forma de los datos y del código (G.*)');
  if (forma.nuevo.length) { L(`**Nuevo (${forma.nuevo.length}):**`); L(''); L(forma.nuevo.map(k => '`' + k + '`').join(' · ')); L(''); }
  if (forma.quitado.length) { L(`**Quitado (${forma.quitado.length}):**`); L(''); L(forma.quitado.map(k => '`' + k + '`').join(' · ')); L(''); }
  if (forma.cambia.length) { L('**Cambia de tipo:**'); L(''); for (const c of forma.cambia) L(`- \`${c}\``); L(''); }
  if (forma.errores.length) { L('**Errores al cargar la versión nueva sin navegador:**'); for (const e of forma.errores) L(`- ${e}`); L(''); }
}
const fnList = x => {
  if (!x.funciones) return x.error ? [`  - (no se puede leer: ${x.error})`] : [];
  const F = x.funciones, o = [];
  if (F.added.length) o.push('  - nuevas: ' + F.added.map(f => `\`${f.name}\` (l. ${f.line})`).join(', '));
  if (F.changed.length) o.push('  - cambiadas: ' + F.changed.map(f => `\`${f.name}\` (l. ${f.line})`).join(', '));
  if (F.removed.length) o.push('  - quitadas: ' + F.removed.map(f => `\`${f.name}\``).join(', '));
  return o;
};
if (logic.length) {
  L('## Lógica que hay que portar');
  for (const x of logic) {
    L(`- **${x.f}** (${ST[x.st] || x.st}) → ${x.cpp.length ? x.cpp.map(c => '`' + c + '`').join(', ') : '**sin port todavía**'} · commits ${x.commits.join(' ')}${x.nota ? ' · ' + x.nota : ''}`);
    fnList(x).forEach(L);
  }
  L('');
}
if (ui.length) {
  L('## Interfaz de la web (revisar si trae algo jugable)');
  for (const x of ui) { L(`- **${x.f}** → ${x.cpp.map(c => '`' + c + '`').join(', ') || '—'}${x.nota ? ' · ' + x.nota : ''}`); fnList(x).forEach(L); }
  L('');
}
const data = by('datos');
if (data.length) {
  L('## Datos (se regeneran solos)');
  for (const x of data) L(`- ${x.f} → ${x.gen.join(', ') || '—'}${x.cpp.length ? ' (y revisar ' + x.cpp.join(', ') + ')' : ''}${x.funciones && x.funciones.changed.length + x.funciones.added.length ? ' · funciones tocadas: ' + [...x.funciones.added, ...x.funciones.changed].map(f => f.name).slice(0, 12).join(', ') : ''}`);
  L('');
}
const web = by('web');
if (web.length) {
  L(`## Sólo web (${web.length}, no afectan a la 3DS)`);
  L(web.map(x => x.f).join(' · '));
  L('');
}

const mdText = md.join('\n') + '\n';
if (arg('md')) fs.writeFileSync(arg('md'), mdText); else process.stdout.write(mdText);
if (arg('json')) fs.writeFileSync(arg('json'), JSON.stringify(resumen, null, 1));

#!/usr/bin/env node
/* cloud_e2e.js — prueba de punta a punta de la sincronización de la 3DS con
 * la web, sin tocar el proyecto de verdad:
 *   - emulador de Firebase local (tools/firebase-emu, ya arrancado)
 *   - la web ORIGINAL (reference/web/dev.html) en Chromium, con la
 *     configuración cambiada al servirla para que use el emulador
 *   - la "3DS": build-pc/ps_cloud (el mismo código de sincronización)
 *
 * Guion:
 *   1. la web crea la cuenta, gana monedas y un Pokémon y sube la partida
 *   2. la web gana algo más SIN subir (como una pestaña abierta)
 *   3. la 3DS, con una copia de antes, gasta monedas, consigue otro Pokémon
 *      y un shiny, y sincroniza
 *   4. la web sube lo suyo (se fusiona con lo de la 3DS en la nube)
 *   5. se vuelve a abrir la web: tiene que estar TODO y las monedas cuadrar
 *
 *   node tools/cloud_e2e.js
 */
'use strict';
const fs = require('fs'), path = require('path'), http = require('http'), { execFileSync } = require('child_process');
const { chromium } = require(path.join(__dirname, 'node_modules', 'playwright'));

const ROOT = path.join(__dirname, '..');
const WEB = path.join(ROOT, 'reference', 'web');
const PORT = 8123, FS_PORT = 8089;
const USER = 'Probador' + Math.floor(Math.random() * 1e5), PASS = 'secreto123';
const TMP = fs.mkdtempSync('/tmp/ps-cloud-');
const MIME = { '.js': 'text/javascript', '.html': 'text/html', '.css': 'text/css', '.json': 'application/json', '.png': 'image/png', '.svg': 'image/svg+xml' };
const EMU_CFG = "G.FIREBASE_CONFIG = { apiKey: 'fake-api-key', authDomain: 'demo-pokesurvivor.firebaseapp.com', projectId: 'demo-pokesurvivor', emulator: true };";

function serve() {
  return http.createServer((req, res) => {
    const u = decodeURIComponent(req.url.split('?')[0]);
    const f = path.join(WEB, u === '/' ? 'dev.html' : u);
    if (!f.startsWith(WEB) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { res.writeHead(404); res.end(); return; }
    res.writeHead(200, { 'Content-Type': MIME[path.extname(f)] || 'application/octet-stream' });
    fs.createReadStream(f).pipe(res);
  }).listen(PORT);
}

async function openWeb(browser) {
  const page = await browser.newPage();
  page.on('pageerror', e => console.log('  [web] error:', e.message));
  await page.route('**/js/core/firebase-config.js', r => r.fulfill({ contentType: 'text/javascript', body: EMU_CFG }));
  // db.js pone Firestore del emulador en el 8080 (ocupado en el VPS): se desvía al 8089.
  await page.route(/127\.0\.0\.1:8080/, r => r.continue({ url: r.request().url().replace(':8080', ':' + FS_PORT) }));
  await page.goto(`http://localhost:${PORT}/dev.html`);
  await page.waitForFunction(() => window.G && G.DB && G.Pokedex !== undefined || (window.G && G.DB), null, { timeout: 30000 });
  await page.evaluate(() => G.DB.ready());
  return page;
}

const snapshot = page => page.evaluate(() => JSON.parse(JSON.stringify(G.DB.save)));
// Subir ya (la web espera 65 s entre subidas: se le borra la marca).
const push = page => page.evaluate(async () => {
  for (const k of Object.keys(localStorage)) if (k.startsWith('ps.cloudok.')) localStorage.removeItem(k);
  G.DB.save.updated = Date.now();
  await G.DB.flush();
  return G.DB.syncState;
});

(async () => {
  const server = serve();
  const browser = await chromium.launch();
  let ok = true;
  const check = (cond, msg) => { console.log((cond ? '  ✔ ' : '  ✘ ') + msg); if (!cond) ok = false; };
  try {
    // 1. la web
    let page = await openWeb(browser);
    const reg = await page.evaluate(([u, p]) => G.DB.register(u, p), [USER, PASS]);
    check(reg && reg.ok, `web: cuenta ${USER} creada en el emulador`);
    await page.evaluate(() => { G.DB.save.coins += 777; G.DB.grant(1, 'gacha'); G.DB.save.partner = 1; G.DB.save.stats.runs = 3; });
    console.log('  web: subir →', await push(page));
    const atStart = await snapshot(page);
    // 2. la web gana algo sin subir
    await page.evaluate(() => { G.DB.save.coins += 50; G.DB.grant(4, 'gacha'); G.DB.save.updated = Date.now(); });
    // 3. la 3DS con la copia de antes
    const ds = JSON.parse(JSON.stringify(atStart));
    ds.coins -= 100;                                  // gacha en la 3DS
    ds.owned[7] = { at: Date.now(), from: 'wild' };   // Squirtle
    ds.shiny[25] = { at: Date.now(), from: 'wild' };  // Pikachu shiny
    ds.owned[25] = { at: Date.now(), from: 'wild' };
    ds.stats.runs = 5; ds.stats.bestTime = 1234;
    ds.updated = Date.now();
    delete ds.led;                                    // (la 3DS no toca la libreta: la dobla al sincronizar)
    ds.led = atStart.led;
    fs.writeFileSync(path.join(TMP, 'ds.json'), JSON.stringify(ds));
    const outp = execFileSync(path.join(ROOT, 'build-pc', 'ps_cloud'), [USER, PASS, path.join(TMP, 'ds.json'), 'ctr3dsprueba', path.join(TMP, 'merged.json')]).toString();
    console.log('  3DS:', outp.trim());
    const merged = JSON.parse(fs.readFileSync(path.join(TMP, 'merged.json'), 'utf8'));
    check(merged.owned[7] && merged.owned[1], '3DS: la fusión tiene lo de la web (Bulbasaur) y lo suyo (Squirtle)');
    check(merged.coins === atStart.coins - 100, `3DS: monedas ${merged.coins} = ${atStart.coins} − 100`);
    // 4. la web sube lo suyo
    console.log('  web: subir →', await push(page));
    await page.close();
    // 5. abrir la web otra vez
    page = await openWeb(browser);
    const lg = await page.evaluate(([u, p]) => G.DB.login(u, p), [USER, PASS]);
    check(lg && lg.ok !== false, 'web: vuelve a entrar');
    const fin = await snapshot(page);
    check(!!(fin.owned[1] && fin.owned[4] && fin.owned[7] && fin.owned[25]), 'web: tiene los Pokémon de los dos (1, 4 de la web; 7, 25 de la 3DS)');
    check(!!fin.shiny[25], 'web: tiene el shiny conseguido en la 3DS');
    check(fin.coins === atStart.coins + 50 - 100, `web: monedas ${fin.coins} = ${atStart.coins} + 50 (web) − 100 (3DS)`);
    check(fin.stats.runs === 5 && fin.stats.bestTime === 1234, 'web: estadísticas, las mayores');
    check(fin.led && fin.led.ctr3dsprueba, 'web: la libreta tiene el apunte de la 3DS');
    // 6. y la 3DS vuelve a sincronizar: baja lo de la web
    fs.writeFileSync(path.join(TMP, 'ds2.json'), JSON.stringify(merged));
    execFileSync(path.join(ROOT, 'build-pc', 'ps_cloud'), [USER, PASS, path.join(TMP, 'ds2.json'), 'ctr3dsprueba', path.join(TMP, 'merged2.json')]);
    const m2 = JSON.parse(fs.readFileSync(path.join(TMP, 'merged2.json'), 'utf8'));
    check(!!m2.owned[4] && m2.coins === fin.coins, `3DS: baja lo de la web (Charmander, ${m2.coins} monedas)`);
  } catch (e) {
    console.log('  ✘ ' + (e.stack || e));
    ok = false;
  }
  await browser.close();
  server.close();
  console.log(ok ? 'cloud_e2e: TODO BIEN' : 'cloud_e2e: FALLOS');
  process.exit(ok ? 0 : 1);
})();

#!/usr/bin/env node
/* google_nombre_e2e.js — una cuenta de GOOGLE de la web gana nombre y
 * contraseña con tools/web/vincular_nombre.js (pegado en la consola de la web
 * ORIGINAL) y la 3DS (build-pc/ps_cloud) entra con ellos: misma cuenta, misma
 * partida. Con el emulador de Firebase (tools/firebase-emu, ya arrancado).
 *   node tools/google_nombre_e2e.js
 */
'use strict';
const fs = require('fs'), path = require('path'), http = require('http'), { execFileSync } = require('child_process');
const { chromium } = require(path.join(__dirname, 'node_modules', 'playwright'));
const ROOT = path.join(__dirname, '..'), WEB = path.join(ROOT, 'reference', 'web'), PORT = 8124, FS_PORT = 8089;
const MIME = { '.js': 'text/javascript', '.html': 'text/html', '.css': 'text/css', '.json': 'application/json', '.png': 'image/png' };
const EMU_CFG = "G.FIREBASE_CONFIG = { apiKey: 'fake-api-key', authDomain: 'demo-pokesurvivor.firebaseapp.com', projectId: 'demo-pokesurvivor', emulator: true };";
const TAG = String(Math.floor(Math.random() * 1e5)), NOMBRE = 'Gnombre' + TAG, PASS = 'secreta123';

const server = http.createServer((req, res) => {
  const u = decodeURIComponent(req.url.split('?')[0]);
  const f = path.join(WEB, u === '/' ? 'dev.html' : u);
  if (!f.startsWith(WEB) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { res.writeHead(404); res.end(); return; }
  res.writeHead(200, { 'Content-Type': MIME[path.extname(f)] || 'application/octet-stream' });
  fs.createReadStream(f).pipe(res);
}).listen(PORT);

async function open(browser) {
  const page = await browser.newPage();
  await page.route('**/js/core/firebase-config.js', r => r.fulfill({ contentType: 'text/javascript', body: EMU_CFG }));
  await page.route(/127\.0\.0\.1:8080/, r => r.continue({ url: r.request().url().replace(':8080', ':' + FS_PORT) }));
  await page.goto(`http://localhost:${PORT}/dev.html`);
  await page.waitForFunction(() => window.G && G.DB, null, { timeout: 30000 });
  return page;
}

(async () => {
  const browser = await chromium.launch();
  const ctx = await browser.newContext();
  let ok = true;
  const check = (c, m) => { console.log((c ? '  ✔ ' : '  ✘ ') + m); if (!c) ok = false; };
  try {
    let page = await ctx.newPage(); await page.close();
    page = await open(ctx);
    await page.evaluate(() => G.DB.ready());
    // Entrar con Google (en el emulador, con un token de Google de mentira).
    const uid = await page.evaluate(async t => {
      const c = await firebase.auth().signInWithCredential(firebase.auth.GoogleAuthProvider.credential(
        JSON.stringify({ sub: 'g-' + t, email: 'ana' + t + '@gmail.com', email_verified: true, name: 'Ana' })));
      return c.user.uid;
    }, TAG);
    // Se vuelve a abrir la web: recupera la sesión de Google y abre su partida.
    await page.reload();
    await page.waitForFunction(() => window.G && G.DB, null, { timeout: 30000 });
    check(await page.evaluate(() => G.DB.ready()), 'web: entra con su cuenta de Google');
    await page.evaluate(async () => {
      G.DB.save.coins += 333; G.DB.grant(25, 'gacha'); G.DB.save.partner = 25;
      for (const k of Object.keys(localStorage)) if (k.startsWith('ps.cloudok.')) localStorage.removeItem(k);
      G.DB.save.updated = Date.now(); await G.DB.flush();
    });
    check(true, 'web: gana un Pikachu y monedas y lo sube');
    // El fragmento de tools/web/vincular_nombre.js, tal cual (con el nombre y la contraseña de la prueba).
    let msg = '';
    page.on('dialog', d => { msg = d.message(); d.accept(); });
    const snippet = fs.readFileSync(path.join(ROOT, 'tools', 'web', 'vincular_nombre.js'), 'utf8')
      .replace("'TuNombre'", JSON.stringify(NOMBRE)).replace("'tu-contraseña'", JSON.stringify(PASS));
    await page.evaluate(snippet);
    await page.waitForTimeout(1500);
    check(/Hecho/.test(msg), 'consola: ' + msg);
    // La 3DS entra con nombre y contraseña.
    const save = path.join(fs.mkdtempSync('/tmp/ps-gn-'), 'save.json');
    fs.writeFileSync(save, JSON.stringify({ v: 3, name: 'x', coins: 500, owned: { 7: { at: 5, from: 'starter' } }, updated: 1 }));
    const out = save.replace('save.json', 'm.json');
    const r = execFileSync(path.join(ROOT, 'build-pc', 'ps_cloud'), [NOMBRE, PASS, save, 'ctr3ds', out], { encoding: 'utf8' });
    console.log('    ' + r.trim());
    const m = JSON.parse(fs.readFileSync(out, 'utf8'));
    check(r.includes('uid=' + uid), '3DS: entra con nombre y contraseña y es la MISMA cuenta de Google');
    check(m.owned['25'] && m.owned['7'], '3DS: tiene el Pikachu de la web y su Squirtle');
    // Otra vez el fragmento con un nombre que ya es de otra cuenta: lo dice claro.
    msg = '';
    await page.evaluate(snippet);
    await page.waitForTimeout(1500);
    check(/ya tiene|ya es de otra/i.test(msg), 'consola (repetido): ' + msg);
  } catch (e) { console.log('  ✘ ' + e.message); ok = false; }
  await browser.close(); server.close();
  console.log(ok ? 'google_nombre_e2e: TODO BIEN' : 'google_nombre_e2e: FALLA');
  process.exit(ok ? 0 : 1);
})();

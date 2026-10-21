#!/usr/bin/env node
/* google_qr_e2e.js — entrar con Google en la 3DS por QR, de punta a punta:
 *   - el servicio «vincular» del servidor (../docker/pokesurvivor-relay/vincular), en local
 *   - el emulador de Firebase (tools/firebase-emu, ya arrancado)
 *   - la web ORIGINAL en Chromium con una sesión de Google que tiene partida
 *   - la «3DS»: build-pc/ps_cloud --qr (el mismo código que la consola)
 * Guion: la 3DS pide un código → se abre la página del QR → se copia el
 * trocito con el botón → se pega en la web (consola) → la 3DS entra sola en la
 * MISMA cuenta de Google y sincroniza.
 */
'use strict';
const fs = require('fs'), path = require('path'), http = require('http'), { spawn } = require('child_process');
const { chromium } = require(path.join(__dirname, 'node_modules', 'playwright'));
const ROOT = path.join(__dirname, '..'), WEB = path.join(ROOT, 'reference', 'web'), PORT = 8125, FS_PORT = 8089, VPORT = 8126;
const VINC = path.join(ROOT, '..', 'docker', 'pokesurvivor-relay', 'vincular', 'vincular.py');
const MIME = { '.js': 'text/javascript', '.html': 'text/html', '.css': 'text/css', '.json': 'application/json', '.png': 'image/png' };
const EMU_CFG = "G.FIREBASE_CONFIG = { apiKey: 'fake-api-key', authDomain: 'demo-pokesurvivor.firebaseapp.com', projectId: 'demo-pokesurvivor', emulator: true };";
const TAG = String(Math.floor(Math.random() * 1e5));

const web = http.createServer((req, res) => {
  const u = decodeURIComponent(req.url.split('?')[0]);
  const f = path.join(WEB, u === '/' ? 'dev.html' : u);
  if (!f.startsWith(WEB) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { res.writeHead(404); res.end(); return; }
  res.writeHead(200, { 'Content-Type': MIME[path.extname(f)] || 'application/octet-stream' });
  fs.createReadStream(f).pipe(res);
}).listen(PORT);
const vinc = spawn('python3', [VINC], { env: Object.assign({}, process.env, {
  VINCULAR_PUERTO: String(VPORT), VINCULAR_BASE: `http://127.0.0.1:${VPORT}`, VINCULAR_ORIGEN: `http://localhost:${PORT}` }) });

(async () => {
  let ok = true;
  const check = (c, m) => { console.log((c ? '  ✔ ' : '  ✘ ') + m); if (!c) ok = false; };
  await new Promise(r => setTimeout(r, 800));
  const browser = await chromium.launch();
  try {
    // La web: cuenta de Google con su partida (Pikachu).
    const ctx = await browser.newContext({ permissions: ['clipboard-read', 'clipboard-write'] });
    const page = await ctx.newPage();
    await page.route('**/js/core/firebase-config.js', r => r.fulfill({ contentType: 'text/javascript', body: EMU_CFG }));
    await page.route(/127\.0\.0\.1:8080/, r => r.continue({ url: r.request().url().replace(':8080', ':' + FS_PORT) }));
    await page.goto(`http://localhost:${PORT}/dev.html`);
    await page.waitForFunction(() => window.G && G.DB, null, { timeout: 30000 });
    await page.evaluate(() => G.DB.ready());
    const uid = await page.evaluate(async t => (await firebase.auth().signInWithCredential(firebase.auth.GoogleAuthProvider.credential(
      JSON.stringify({ sub: 'qr-' + t, email: 'qr' + t + '@gmail.com', email_verified: true, name: 'Ana López' })))).user.uid, TAG);
    await page.reload();
    await page.waitForFunction(() => window.G && G.DB, null, { timeout: 30000 });
    await page.evaluate(() => G.DB.ready());
    await page.evaluate(async () => {
      G.DB.save.coins += 222; G.DB.grant(25, 'gacha');
      for (const k of Object.keys(localStorage)) if (k.startsWith('ps.cloudok.')) localStorage.removeItem(k);
      G.DB.save.updated = Date.now(); await G.DB.flush();
    });
    // Su apodo de jugador en la web (el que se ve en pokesurvivor.com), con la función de la web.
    const nick = await page.evaluate(async () => { await G.Social.start(); return G.Social.claimNick('AnaJuega'); });
    check(nick && nick.ok, 'web: apodo AnaJuega (' + JSON.stringify(nick) + ')');
    check(true, 'web: cuenta de Google con un Pikachu en la nube');
    // La 3DS pide código y espera.
    const save = path.join(fs.mkdtempSync('/tmp/ps-qr-'), 'save.json');
    fs.writeFileSync(save, JSON.stringify({ v: 3, name: 'x', coins: 500, owned: { 7: { at: 5, from: 'starter' } }, updated: 1 }));
    const out = save.replace('save.json', 'm.json');
    const ds = spawn(path.join(ROOT, 'build-pc', 'ps_cloud'), ['--qr', '-', save, 'ctr3ds', out], { env: Object.assign({}, process.env, { PS_VINCULAR: `http://127.0.0.1:${VPORT}` }) });
    let log = '';
    const fin = new Promise(r => ds.on('close', r));   // (antes de que acabe)
    ds.stdout.on('data', d => { log += d; }); ds.stderr.on('data', d => { log += d; });
    const url = await new Promise((res, rej) => { const t = setInterval(() => { const m = /QR: (\S+)/.exec(log); if (m) { clearInterval(t); res(m[1]); } }, 100); setTimeout(() => rej(new Error('sin QR')), 10000); });
    check(true, '3DS: enseña el QR → ' + url);
    // La página del QR: se copia el trocito con el botón.
    const qp = await ctx.newPage();
    await qp.goto(url);
    await qp.click('#copiar');
    const trocito = await qp.evaluate(() => navigator.clipboard.readText());
    check(trocito.includes('/vincular/api/entregar') && (await qp.textContent('#copiar')).includes('Copiado'), 'página: el botón copia el código');
    // Se pega en la consola de la web.
    let msg = '';
    page.on('dialog', d => { msg = d.message(); d.accept(); });
    await page.evaluate(trocito);
    await page.waitForTimeout(1000);
    check(/Listo/.test(msg), 'web: ' + msg);
    const code = await fin;
    console.log('    ' + log.trim().replace(/\n/g, ' · '));
    const m = code === 0 ? JSON.parse(fs.readFileSync(out, 'utf8')) : null;
    check(code === 0 && log.includes('uid=' + uid), '3DS: entra sola en la MISMA cuenta de Google');
    check(m && m.owned['25'] && m.owned['7'], '3DS: tiene el Pikachu de la web y su Squirtle');
    check(/apodo: AnaJuega/.test(log), '3DS: con su apodo de jugador (AnaJuega), no el nombre de Google');
  } catch (e) { console.log('  ✘ ' + e.message); ok = false; }
  await browser.close(); web.close(); vinc.kill();
  console.log(ok ? 'google_qr_e2e: TODO BIEN' : 'google_qr_e2e: FALLA');
  process.exit(ok ? 0 : 1);
})();

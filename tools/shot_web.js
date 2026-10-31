// shot_web.js — capturas de la web (pruebas): node tools/shot_web.js URL salida.png [ancho] [alto] [pagina_entera]
const { chromium } = require('playwright');
(async () => {
  const [url, out, w, h, full] = process.argv.slice(2);
  const b = await chromium.launch();
  const p = await b.newPage({ viewport: { width: +(w || 1280), height: +(h || 800) } });
  await p.goto(url, { waitUntil: 'networkidle' });
  await p.waitForTimeout(1500);
  await p.screenshot({ path: out, fullPage: full === '1' });
  await b.close();
})();

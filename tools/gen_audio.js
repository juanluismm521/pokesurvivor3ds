#!/usr/bin/env node
/* gen_audio.js — graba la música y los efectos del sintetizador de la web
 * (reference/web/js/core/audio.js + data/music.js) para la 3DS.
 *
 *   node tools/gen_audio.js [carpeta_salida]      (por defecto romfs/snd)
 *
 * Se ejecuta el mismo audio.js en Chromium sin pantalla (Playwright) con un
 * OfflineAudioContext: suena idéntico y mucho más rápido que en tiempo real.
 *   música   cada tema se graba 3 veces seguido y se queda la vuelta del medio,
 *            así el bucle no tiene corte (lleva dentro la cola del eco) → .ogg
 *   efectos  uno a uno, recortando el silencio del final → sfx.bin (PCM 16 bits)
 *            (los disparos: shot_<2 letras de la familia>_<tipo>, p. ej. shot_pr_fire)
 * Los niveles son los de la web (mismo compresor y mismas ganancias), con el
 * volumen de música y de efectos al 100 %: la consola aplica el del jugador.
 *
 * sfx.bin: 'PSX1' · u16 n · u16 frecuencia · por efecto: char[24] nombre,
 *          u32 desplazamiento (en muestras), u32 muestras · luego los datos.
 */
'use strict';
const fs = require('fs'), path = require('path');
const { execFileSync } = require('child_process');
const { chromium } = require('playwright');

const ROOT = path.join(__dirname, '..');
const WEB = path.join(ROOT, 'reference', 'web', 'js');
const OUT = process.argv[2] || path.join(ROOT, 'romfs', 'snd');
const TMP = path.join(ROOT, 'build', 'audio_tmp');
const MUSIC_SR = 32000, SFX_SR = 22050;
fs.mkdirSync(OUT, { recursive: true });
fs.mkdirSync(TMP, { recursive: true });

const read = f => fs.readFileSync(path.join(WEB, f), 'utf8').replace(/\r\n/g, '\n');
function inject(src, marker, code) {
  if (!src.includes(marker)) throw new Error('no encuentro ' + marker);
  return src.replace(marker, code + '\n' + marker);
}

function wav(file, ch, sr, int16) {
  const h = Buffer.alloc(44);
  h.write('RIFF', 0); h.writeUInt32LE(36 + int16.length * 2, 4); h.write('WAVE', 8); h.write('fmt ', 12);
  h.writeUInt32LE(16, 16); h.writeUInt16LE(1, 20); h.writeUInt16LE(ch, 22); h.writeUInt32LE(sr, 24);
  h.writeUInt32LE(sr * ch * 2, 28); h.writeUInt16LE(ch * 2, 32); h.writeUInt16LE(16, 34); h.write('data', 36);
  h.writeUInt32LE(int16.length * 2, 40);
  fs.writeFileSync(file, Buffer.concat([h, Buffer.from(int16.buffer, int16.byteOffset, int16.length * 2)]));
}

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage();
  await page.goto('about:blank');
  let audio = read('core/audio.js');
  // Exporta las piezas internas y deja reiniciar el contexto para cada grabación.
  audio = inject(audio, '  return {\n    music, sfx,',
    '  G.__AU = { INST, compile, SFX, init, reset() { ctx = null; }, get ctx() { return ctx; }, get musicBus() { return musicBus; }, get sfxBus() { return sfxBus; } };');
  await page.evaluate(src => {
    window.G = {};
    // (los navegadores sin pantalla no tienen permiso de audio en vivo: no hace falta)
    window.__off = null;
    window.AudioContext = function () { return window.__off; };
    // eslint-disable-next-line no-eval
    (0, eval)(src);
  }, read('core/util.js') + '\n' + read('data/music.js') + '\n' + audio);

  const songs = await page.evaluate(() => Object.keys(G.MUSIC));
  const report = { music: {}, sfx: 0 };

  // ---------------- música ----------------
  for (const name of songs) {
    const res = await page.evaluate(async ({ name, SR }) => {
      const song = G.__AU.compile(G.MUSIC[name]);
      const sd = 60 / song.bpm / 4, loop = song.steps * sd;
      window.__off = new OfflineAudioContext(2, Math.ceil(SR * loop * 3), SR);
      G.__AU.reset(); G.__AU.init();
      await new Promise(r => setTimeout(r, 450));      // la reverberación se rellena con setTimeout
      const ctx = G.__AU.ctx, mb = G.__AU.musicBus;
      mb.gain.cancelScheduledValues(0); mb.gain.value = 0.7;     // música al 100 %
      const bus = ctx.createGain(); bus.connect(mb);
      for (let k = 0; k < song.steps * 3; k++) {
        const i = k % song.steps;
        const t = 0.05 + k * sd + (i % 4 === 2 ? song.swing * sd : 0);
        for (const e of song.ev[i]) {
          if (e.drum) G.__AU.INST[e.inst](t, e.v, bus);
          else G.__AU.INST[e.inst](t, e.f, e.len * sd, e.v, bus);
        }
      }
      const buf = await ctx.startRendering();
      // La vuelta del medio (ya con la cola del eco de la anterior).
      const a = Math.round((0.05 + loop) * SR), n = Math.round(loop * SR);
      const L = buf.getChannelData(0), R = buf.getChannelData(1);
      const out = new Int16Array(n * 2);
      let sum = 0;
      for (let i = 0; i < n; i++) {
        const l = Math.max(-1, Math.min(1, L[a + i])), r = Math.max(-1, Math.min(1, R[a + i]));
        out[i * 2] = l * 32767; out[i * 2 + 1] = r * 32767; sum += l * l + r * r;
      }
      let s = '';
      const u8 = new Uint8Array(out.buffer);
      for (let i = 0; i < u8.length; i += 0x8000) s += String.fromCharCode.apply(null, u8.subarray(i, i + 0x8000));
      return { b64: btoa(s), loop, rms: Math.sqrt(sum / (2 * n)) };
    }, { name, SR: MUSIC_SR });
    const pcm = Buffer.from(res.b64, 'base64');
    const int16 = new Int16Array(pcm.buffer, pcm.byteOffset, pcm.length / 2);
    const w = path.join(TMP, name + '.wav');
    wav(w, 2, MUSIC_SR, int16);
    execFileSync('oggenc', ['-Q', '-q', '3', '-o', path.join(OUT, 'music_' + name + '.ogg'), w]);
    report.music[name] = { s: +res.loop.toFixed(2), rms: +res.rms.toFixed(4), KB: Math.round(fs.statSync(path.join(OUT, 'music_' + name + '.ogg')).size / 1024) };
    process.stdout.write('.');
  }

  // ---------------- efectos ----------------
  const TYPES = ['normal', 'fire', 'water', 'grass', 'electric', 'ice', 'fighting', 'poison', 'ground', 'flying', 'psychic', 'bug', 'rock', 'ghost', 'dragon', 'dark', 'steel', 'fairy'];
  const KINDS = ['projectile', 'melee', 'beam', 'nova', 'aura', 'buff', 'orbit'];
  const names = (await page.evaluate(() => Object.keys(G.__AU.SFX))).filter(n => !['shot', 'burst', 'reveal'].includes(n));
  const jobs = names.map(n => ({ id: n, name: n, opts: null }));
  for (const k of KINDS) for (const t of TYPES) jobs.push({ id: `shot_${k.slice(0, 2)}_${t}`, name: 'shot', opts: { kind: k, type: t } });
  for (let r = 1; r <= 5; r++) { jobs.push({ id: 'burst_' + r, name: 'burst', opts: { rarity: r } }); jobs.push({ id: 'reveal_' + r, name: 'reveal', opts: { rarity: r } }); }

  const clips = [];
  for (const j of jobs) {
    const b64 = await page.evaluate(async ({ j, SR }) => {
      window.__off = new OfflineAudioContext(1, SR * 3, SR);
      G.__AU.reset(); G.__AU.init();
      await new Promise(r => setTimeout(r, 380));
      const sb = G.__AU.sfxBus;
      sb.gain.cancelScheduledValues(0); sb.gain.value = 2.4;     // efectos al 100 %
      G.__AU.SFX[j.name](j.opts || undefined);
      const buf = await G.__AU.ctx.startRendering();
      const d = buf.getChannelData(0);
      let end = d.length;
      while (end > 0 && Math.abs(d[end - 1]) < 3e-4) end--;
      end = Math.min(d.length, end + Math.round(SR * 0.01));
      const out = new Int16Array(end);
      for (let i = 0; i < end; i++) out[i] = Math.max(-1, Math.min(1, d[i])) * 32767;
      let s = '';
      const u8 = new Uint8Array(out.buffer);
      for (let i = 0; i < u8.length; i += 0x8000) s += String.fromCharCode.apply(null, u8.subarray(i, i + 0x8000));
      return btoa(s);
    }, { j, SR: SFX_SR });
    const pcm = Buffer.from(b64, 'base64');
    clips.push({ id: j.id, data: pcm });
    process.stdout.write('·');
  }
  await browser.close();

  // sfx.bin
  const head = Buffer.alloc(8 + clips.length * 32);
  head.write('PSX1', 0); head.writeUInt16LE(clips.length, 4); head.writeUInt16LE(SFX_SR, 6);
  let off = 0;
  clips.forEach((c, i) => {
    const o = 8 + i * 32;
    if (c.id.length > 23) throw new Error('nombre largo: ' + c.id);
    head.write(c.id, o, 'ascii');
    head.writeUInt32LE(off, o + 24); head.writeUInt32LE(c.data.length / 2, o + 28);
    off += c.data.length / 2;
  });
  fs.writeFileSync(path.join(OUT, 'sfx.bin'), Buffer.concat([head, ...clips.map(c => c.data)]));
  report.sfx = clips.length;
  report.sfx_KB = Math.round(fs.statSync(path.join(OUT, 'sfx.bin')).size / 1024);
  fs.writeFileSync(path.join(OUT, 'report.json'), JSON.stringify(report, null, 1));
  console.log('\ngen_audio:', JSON.stringify(report));
})().catch(e => { console.error(e); process.exit(1); });

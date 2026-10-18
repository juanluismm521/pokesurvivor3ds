/* softcanvas.js — un canvas 2D mínimo por software para ejecutar en node el
 * pixel art que la web pinta en código (tiles.js, vfx.js, icons.js). Basta con
 * lo que usan: fillStyle + fillRect (rejilla entera), createImageData /
 * putImageData / getImageData y drawImage de otro canvas sin escalar. */
'use strict';

function parseColor(s) {
  if (typeof s !== 'string') return [0, 0, 0, 255];
  s = s.trim();
  if (s[0] === '#') {
    let h = s.slice(1);
    if (h.length === 3) h = h.split('').map(c => c + c).join('');
    const n = parseInt(h.slice(0, 6), 16);
    const a = h.length === 8 ? parseInt(h.slice(6, 8), 16) : 255;
    return [(n >> 16) & 255, (n >> 8) & 255, n & 255, a];
  }
  const m = /^rgba?\(([^)]+)\)$/.exec(s);
  if (m) {
    const p = m[1].split(',').map(v => parseFloat(v));
    return [p[0], p[1], p[2], p.length > 3 ? Math.round(p[3] * 255) : 255];
  }
  return [0, 0, 0, 255];
}

class SoftCanvas {
  constructor(w = 0, h = 0) {
    this._w = w; this._h = h;
    this.data = new Uint8ClampedArray(w * h * 4);
  }
  get width() { return this._w; }
  set width(v) { this._w = v; this.data = new Uint8ClampedArray(this._w * this._h * 4); }
  get height() { return this._h; }
  set height(v) { this._h = v; this.data = new Uint8ClampedArray(this._w * this._h * 4); }
  getContext() {
    if (!this._ctx) this._ctx = new SoftContext(this);
    return this._ctx;
  }
}

class SoftContext {
  constructor(cv) { this.canvas = cv; this._fill = [0, 0, 0, 255]; this.globalAlpha = 1; this.imageSmoothingEnabled = false; }
  set fillStyle(v) { this._fillStr = v; this._fill = parseColor(v); }
  get fillStyle() { return this._fillStr; }
  _blend(x, y, c) {
    const cv = this.canvas;
    if (x < 0 || y < 0 || x >= cv.width || y >= cv.height) return;
    const i = (y * cv.width + x) * 4, d = cv.data;
    const a = (c[3] / 255) * this.globalAlpha;
    if (a >= 1) { d[i] = c[0]; d[i + 1] = c[1]; d[i + 2] = c[2]; d[i + 3] = 255; return; }
    const da = d[i + 3] / 255, oa = a + da * (1 - a);
    if (oa <= 0) return;
    for (let k = 0; k < 3; k++) d[i + k] = Math.round((c[k] * a + d[i + k] * da * (1 - a)) / oa);
    d[i + 3] = Math.round(oa * 255);
  }
  fillRect(x, y, w, h) {
    const x0 = Math.round(x), y0 = Math.round(y), x1 = Math.round(x + w), y1 = Math.round(y + h);
    for (let yy = y0; yy < y1; yy++) for (let xx = x0; xx < x1; xx++) this._blend(xx, yy, this._fill);
  }
  clearRect() { this.canvas.data.fill(0); }
  createImageData(w, h) { return { width: w, height: h, data: new Uint8ClampedArray(w * h * 4) }; }
  putImageData(img, dx = 0, dy = 0) {
    const cv = this.canvas;
    for (let y = 0; y < img.height; y++) for (let x = 0; x < img.width; x++) {
      const X = x + dx, Y = y + dy;
      if (X < 0 || Y < 0 || X >= cv.width || Y >= cv.height) continue;
      const s = (y * img.width + x) * 4, d = (Y * cv.width + X) * 4;
      for (let k = 0; k < 4; k++) cv.data[d + k] = img.data[s + k];
    }
  }
  getImageData(x, y, w, h) {
    const out = this.createImageData(w, h), cv = this.canvas;
    for (let yy = 0; yy < h; yy++) for (let xx = 0; xx < w; xx++) {
      const s = ((yy + y) * cv.width + xx + x) * 4, d = (yy * w + xx) * 4;
      for (let k = 0; k < 4; k++) out.data[d + k] = cv.data[s + k] || 0;
    }
    return out;
  }
  drawImage(src, dx, dy) {
    if (!src || !src.data) return;
    if (arguments.length > 3) throw new Error('softcanvas: drawImage con escala no soportado');
    for (let y = 0; y < src.height; y++) for (let x = 0; x < src.width; x++) {
      const s = (y * src.width + x) * 4;
      if (!src.data[s + 3]) continue;
      this._blend(Math.round(dx) + x, Math.round(dy) + y, [src.data[s], src.data[s + 1], src.data[s + 2], src.data[s + 3]]);
    }
  }
  save() {} restore() {} translate() {} rotate() {} scale() {} beginPath() {} closePath() {} fill() {} stroke() {}
  arc() {} ellipse() {} moveTo() {} lineTo() {} setTransform() {} setLineDash() {}
  measureText(s) { return { width: String(s).length * 6 }; }
  fillText() {} strokeText() {}
  createLinearGradient() { return { addColorStop() {} }; }
  createRadialGradient() { return { addColorStop() {} }; }
}

module.exports = { SoftCanvas, parseColor };

/* websandbox.js — contexto de node donde se pueden cargar los .js de la web
 * (sin navegador). Lo usan gen_data.js y los casos dorados (tools/golden).
 * Igual que el sandbox de tools/export_data.js de la web, más un canvas falso
 * que guarda los píxeles (para comparar el pintado del suelo con el port). */
'use strict';
const vm = require('vm');

function fakeCanvas() {
  const cv = { width: 0, height: 0, _img: null };
  const ctx = new Proxy({
    createImageData(w, h) { return { width: w, height: h, data: new Uint8ClampedArray(w * h * 4) }; },
    putImageData(img) { cv._img = img; },
    getImageData(x, y, w, h) { return cv._img || { width: w, height: h, data: new Uint8ClampedArray(w * h * 4) }; }
  }, { get: (o, k) => (k in o ? o[k] : () => {}), set: () => true });
  cv.getContext = () => ctx;
  return cv;
}

function create(lang) {
  const noop = () => {};
  const G = {};
  const sandbox = {
    G, console, Math, JSON, Date, Map, Set, WeakMap, Object, Array, Number, String, RegExp, Promise, Symbol,
    Uint8Array, Uint32Array, Int16Array, Int32Array, Float32Array, Uint8ClampedArray,
    Infinity, NaN, isFinite, parseInt, parseFloat,
    location: { pathname: '/', protocol: 'file:' }, navigator: { language: 'es', languages: ['es'] },
    localStorage: { getItem: () => lang || 'es', setItem: noop, removeItem: noop },
    document: {
      documentElement: {}, createElement: () => fakeCanvas(), addEventListener: noop,
      querySelector: () => null, querySelectorAll: () => [], getElementById: () => null, body: {}
    },
    addEventListener: noop, matchMedia: () => ({ matches: false }), CanvasRenderingContext2D: function () {},
    Image: function () {}, performance: { now: () => 0 },
    setTimeout: noop, setInterval: noop, clearTimeout: noop, requestAnimationFrame: noop
  };
  sandbox.window = sandbox;
  vm.createContext(sandbox);
  return sandbox;
}

module.exports = { create };

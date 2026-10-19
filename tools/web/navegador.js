/* navegador.js — un navegador de mentira para correr el juego web ENTERO sin
 * pantalla (node o QuickJS): document, localStorage, temporizadores, un canvas
 * que no dibuja nada... Lo usan tools/web/forma.js (forma de los datos y del
 * código de la web) y tools/web/partida.js (partidas de la web con un bot).
 *
 * Se ejecuta DENTRO del contexto del juego (vm de node), antes que los .js de
 * la web. Deja en el global:
 *   __now (ms del reloj falso), __runTimers(), __raf (el bucle del juego),
 *   __ctx (el canvas falso), __tu / __tr (ms de lógica y de dibujo, si se mide).
 */

var __stubCache = new Map();
function makeStub(name) {
  var f = function () { return makeStub(name + '()'); };
  return new Proxy(f, {
    get: function (t, k) {
      if (k === 'then' || k === Symbol.toPrimitive || k === Symbol.iterator) return undefined;
      if (k === 'length') return 0;
      if (k === 'toString' || k === 'valueOf') return function () { return ''; };
      if (k === 'getPropertyValue') return function () { return '0'; };
      if (k === 'classList') return { add() {}, remove() {}, toggle() {}, contains() { return false; } };
      if (k === 'style' || k === 'dataset') return {};
      if (k === 'children' || k === 'childNodes') return [];
      if (k === 'textContent' || k === 'innerHTML' || k === 'value') return '';
      if (k === 'width' || k === 'height' || k === 'clientWidth' || k === 'clientHeight' || k === 'offsetWidth') return 800;
      if (typeof k === 'symbol') return undefined;
      if (!t.hasOwnProperty(k)) t[k] = makeStub(name + '.' + String(k));
      return t[k];
    },
    set: function (t, k, v) { t[k] = v; return true; },
    apply: function () { return makeStub(name + '()'); },
    construct: function () { return makeStub('new ' + name); }
  });
}
var noop = function () {};
// Azar con semilla: la misma web da siempre lo mismo (forma, partidas de prueba).
(function () { var s = 2463534242; Math.random = function () { s ^= s << 13; s >>>= 0; s ^= s >>> 17; s ^= s << 5; s >>>= 0; return s / 4294967296; }; })();
var store = {};
var G = {};
var __g = (typeof globalThis !== 'undefined') ? globalThis : this;
__g.G = G;
__g.window = __g;
__g.self = __g;
__g.location = { pathname: '/', protocol: 'file:', search: '', hostname: 'localhost', href: 'file:///', origin: 'file://', reload: noop };
Object.defineProperty(__g, 'navigator', { configurable: true, writable: true, value: null });
__g.navigator = { language: 'es', languages: ['es'], userAgent: 'node', maxTouchPoints: 0, getGamepads: function () { return []; } };
__g.localStorage = { getItem: function (k) { return k in store ? store[k] : null; }, setItem: function (k, v) { store[k] = String(v); }, removeItem: function (k) { delete store[k]; } };
__g.sessionStorage = __g.localStorage;
var docStub = makeStub('document');
__g.document = docStub;
__g.getComputedStyle = function () { return makeStub('css'); };
__g.addEventListener = noop; __g.removeEventListener = noop;
__g.matchMedia = function () { return { matches: false, addEventListener: noop }; };
__g.CanvasRenderingContext2D = function () {};
__g.Image = function () { return makeStub('img'); };
__g.Audio = function () { return makeStub('audio'); };
__g.AudioContext = undefined;
__g.performance = { now: function () { return __now; } };
var __now = 0;
var __timers = [];
__g.setTimeout = function (f, ms) { __timers.push({ f: f, at: __now + (ms || 0) }); return __timers.length; };
__g.setInterval = function () { return 0; };
__g.clearTimeout = noop; __g.clearInterval = noop;
__g.requestAnimationFrame = noop;
__g.devicePixelRatio = 1;
__g.innerWidth = 800; __g.innerHeight = 480;
__g.screen = { width: 800, height: 480 };
__g.fetch = function () { return new Promise(function () {}); };
__g.firebase = undefined;
__g.PS_LOCALES = { lang: 'es' };
function __runTimers() {
  var due = __timers.filter(function (t) { return t.at <= __now; });
  __timers = __timers.filter(function (t) { return t.at > __now; });
  due.forEach(function (t) { try { t.f(); } catch (e) {} });
}
// Canvas 2D falso pero barato (sin Proxy).
var __ctx = {};
['save','restore','scale','translate','rotate','setTransform','transform','resetTransform','clearRect','fillRect','strokeRect','beginPath','closePath',
 'moveTo','lineTo','arc','arcTo','ellipse','rect','quadraticCurveTo','bezierCurveTo','fill','stroke','clip','drawImage','fillText','strokeText',
 'putImageData','setLineDash','roundRect'].forEach(function (k) { __ctx[k] = function () {}; });
__ctx.measureText = function (s) { return { width: String(s).length * 6 }; };
__ctx.createLinearGradient = __ctx.createRadialGradient = function () { return { addColorStop: function () {} }; };
__ctx.createPattern = function () { return {}; };
__ctx.getImageData = __ctx.createImageData = function (x, y, w, h) { w = w || x; h = h || y; return { width: w, height: h, data: new Uint8ClampedArray(Math.max(1, (w * h * 4) | 0)) }; };
function __canvas() { return { width: 256, height: 256, clientWidth: 800, clientHeight: 480, offsetWidth: 800, offsetHeight: 480, style: {}, classList: { add() {}, remove() {}, toggle() {}, contains() { return false; } }, getContext: function () { return __ctx; }, addEventListener: function () {}, getBoundingClientRect: function () { return { left: 0, top: 0, width: 800, height: 480 }; } }; }
var __origGet = document.getElementById;
__g.document = new Proxy({}, { get: function (t, k) {
  if (k === 'getElementById') return function (id) { return id === 'game' ? __gameCv : makeStub('#' + id); };
  if (k === 'createElement') return function (tag) { return tag === 'canvas' ? __canvas() : makeStub('<' + tag + '>'); };
  if (k === 'hidden') return false;
  return docStub[k];
} });
var __gameCv = __canvas(); __gameCv.width = 800; __gameCv.height = 480;
var __tu = 0, __tr = 0;
var __raf = null;
__g.requestAnimationFrame = function (f) { __raf = f; return 1; };

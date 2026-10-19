/* bot.js — un bot para jugar al juego web sin pantalla (tools/web/partida.js
 * y el banco de pruebas de QuickJS en la 3DS, tools/web/jsbench). Se ejecuta
 * dentro del contexto del juego, tras navegador.js y los .js de la web.
 *
 * El bot da vueltas, se queda a media distancia del enemigo más cercano (para
 * que sus ataques tengan blanco), elige siempre la primera carta y se pone el
 * objeto nuevo. Con __immortal no puede caer (para medir partidas largas).
 *   __setup(dex, semilla)          prepara la partida (azar con semilla)
 *   __bench(segundos, cada, reloj) la juega e imprime cada `cada` segundos
 */
var __immortal = true;

function __setup(dex, seed) {
  // RNG con semilla para repetir la misma partida.
  var s = seed >>> 0 || 1;
  Math.random = function () { s ^= s << 13; s >>>= 0; s ^= s >>> 17; s ^= s << 5; s >>>= 0; return s / 4294967296; };
  G.DB.playAsGuest();
  var sv = G.DB.save;
  sv.partner = dex; sv.owned = sv.owned || {}; sv.owned[dex] = { at: 1, from: 'starter' };
  G.RunUI.showLevelUp = function (lv, cards, cb) { cb(cards[0]); };
  G.RunUI.showBond = function (opt, cb) { cb(opt.free ? opt.free.slice(0, 2) : []); };
  G.RunUI.showItemChoice = function (cur, next, take) { take(next); };
  G.Ranking.runStarted = function () {};
  var t = 0;
  G.Input.axis = function () {
    t += 1 / 60;
    var pl = G.Game.player; if (!pl) return [0, 0];
    var best = null, bd = 1e18;
    G.EnemyMgr.all().forEach(function (e) { var d = (e.x - pl.x) * (e.x - pl.x) + (e.y - pl.y) * (e.y - pl.y); if (d < bd) { bd = d; best = e; } });
    var ax = Math.cos(t * 0.3), ay = Math.sin(t * 0.3);
    if (best) { var dx = pl.x - best.x, dy = pl.y - best.y, l = Math.sqrt(dx * dx + dy * dy) || 1, k = l < 70 ? 1 : l > 110 ? -1 : 0; ax = ax * 0.5 + k * dx / l; ay = ay * 0.5 + k * dy / l; }
    var L = Math.sqrt(ax * ax + ay * ay) || 1; return [ax / L, ay / L];
  };
}
function __bench(seconds, report, clock) {
  G.Game.init();
  G.Game.startRun();
  if (__immortal) { var pl0 = G.Game.player, hp0 = pl0.hp; Object.defineProperty(pl0, 'hp', { get: function () { return hp0; }, set: function (v) { hp0 = Math.max(1, v); } }); }
  var frames = Math.round(seconds * 60), total = 0, worst = 0, acc = 0, accN = 0;
  for (var i = 0; i < frames; i++) {
    __now += 1000 / 60;
    __runTimers();
    var t0 = clock();
    __raf(__now);
    var dt = clock() - t0;
    total += dt; acc += dt; accN++; if (dt > worst) worst = dt;
    if ((i + 1) % (60 * report) === 0) {
      var pl = G.Game.player;
      print('t=' + ((i + 1) / 60).toFixed(0) + 's  ms/fotograma=' + (acc / accN).toFixed(2) + ' (lógica ' + (__tu / accN).toFixed(2) + ', dibujo ' + (__tr / accN).toFixed(2) + ')  proyectiles=' + (G.Projectiles.all ? G.Projectiles.all().length : '?') + '  enemigos=' + G.EnemyMgr.all().length + '  nivel=' + pl.level + '  vida=' + Math.round(pl.hp) + '  estado=' + G.Game.state + (pl.dead ? ' (caído)' : ''));
      acc = 0; accN = 0; __tu = 0; __tr = 0;
    }
    if (G.Game.state === 'over' || G.Game.state === 'ui') { print('fin de la partida en t=' + (i / 60).toFixed(1)); break; }
  }
  print('media ' + (total / Math.max(1, i)).toFixed(3) + ' ms/fotograma, peor ' + worst.toFixed(1) + ' ms');
}

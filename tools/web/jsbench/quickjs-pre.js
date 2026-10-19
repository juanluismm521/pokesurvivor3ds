// quickjs-pre.js — lo que QuickJS no trae y el juego web usa (va delante del
// paquete de tools/web/partida.js con PS_QJS). print y __clock los pone el
// programa en C (main.c).
var console = { log: function () { print.apply(null, arguments); } };
console.error = console.warn = console.info = console.log;
// QuickJS no trae Intl: lo mínimo que usa el juego (números con separador de miles).
if (typeof Intl === 'undefined') {
  var Intl = {
    NumberFormat: function () { return { format: function (n) { var s = String(Math.round(n)), r = ''; while (s.length > 3) { r = '.' + s.slice(-3) + r; s = s.slice(0, -3); } return s + r; } }; },
    DateTimeFormat: function () { return { format: function (d) { return String(d); }, resolvedOptions: function () { return { timeZone: 'UTC' }; },
      formatToParts: function (d) { d = d || new Date(); var z = function (n) { return (n < 10 ? '0' : '') + n; };
        return [{ type: 'year', value: String(d.getUTCFullYear()) }, { type: 'month', value: z(d.getUTCMonth() + 1) }, { type: 'day', value: z(d.getUTCDate()) },
                { type: 'hour', value: z(d.getUTCHours()) }, { type: 'minute', value: z(d.getUTCMinutes()) }, { type: 'second', value: z(d.getUTCSeconds()) }]; } }; },
    ListFormat: function () { return { format: function (a) { return a.join(', '); } }; },
    PluralRules: function () { return { select: function (n) { return n === 1 ? 'one' : 'other'; } }; }
  };
}

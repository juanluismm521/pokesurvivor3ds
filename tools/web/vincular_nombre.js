// vincular_nombre.js — PÉGALO EN LA CONSOLA DEL NAVEGADOR en pokesurvivor.com,
// con tu cuenta de GOOGLE abierta (F12 → Consola). Cambia las dos primeras
// líneas por el nombre y la contraseña que quieras.
//
// Añade un nombre y una contraseña a TU MISMA cuenta de Google (no crea otra):
// luego puedes entrar con ellos en la 3DS (y en la web), con la misma partida.
// Usa el sistema de cuentas de Firebase de la propia web (linkWithCredential,
// el mismo correo interno que usa la web para las cuentas de nombre).
(async () => {
  const NOMBRE = 'TuNombre';          // 3-16 letras, números o _
  const CONTRASENA = 'tu-contraseña'; // 6 caracteres o más
  if (!/^[A-Za-z0-9_ñÑ]{3,16}$/.test(NOMBRE) || CONTRASENA.length < 6) throw { message: 'Nombre (3-16 letras, números o _) o contraseña (6+) no válidos' };
  const u = firebase.auth().currentUser;
  if (!u) throw { message: 'Primero entra con tu cuenta de Google en pokesurvivor.com' };
  const hex = [...new TextEncoder().encode(NOMBRE.trim().toLowerCase())].map(b => b.toString(16).padStart(2, '0')).join('');
  await u.linkWithCredential(firebase.auth.EmailAuthProvider.credential(hex + '@jugadores.pokesurvivor.net', CONTRASENA));
  alert('¡Hecho! Ya puedes entrar en la 3DS con «' + NOMBRE + '» y tu contraseña: es tu misma cuenta de Google.');
})().catch(e => alert('No se ha podido: ' + ({
  'auth/credential-already-in-use': 'ese nombre ya es de otra cuenta; prueba con otro',
  'auth/email-already-in-use': 'ese nombre ya es de otra cuenta; prueba con otro',
  'auth/provider-already-linked': 'tu cuenta ya tiene nombre y contraseña',
  'auth/requires-recent-login': 'cierra sesión, vuelve a entrar con Google y repite'
}[e && e.code] || (e && (e.code || e.message)))));

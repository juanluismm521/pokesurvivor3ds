# Entrar con Google en la 3DS

## YA, sin esperar a nadie: ponle nombre y contraseña a tu cuenta de Google

Las cuentas creadas con Google en la web no tienen nombre ni contraseña (el
apodo que eliges sólo sirve para los amigos: con él no se puede entrar). Pero
se les pueden **añadir** sin crear otra cuenta, con el propio sistema de
cuentas de la web, y entonces la 3DS entra con ellos (misma cuenta, misma
partida). Hace falta un ordenador:

1. En el ordenador, abre **pokesurvivor.com** y entra con tu cuenta de Google.
2. Pulsa **F12** (o clic derecho → Inspeccionar) y ve a la pestaña **Consola**.
   (Chrome puede pedir que escribas `allow pasting` antes de pegar.)
3. Copia todo `tools/web/vincular_nombre.js`, cambia en las dos primeras líneas
   `TuNombre` y `tu-contraseña` por los que quieras, pégalo y pulsa Enter.
4. Sale «¡Hecho!». En la 3DS: Ajustes → Cuenta web → «Entrar con usuario y
   contraseña» con ese nombre y contraseña. En la web también sirven.

Si dice que el nombre ya es de otra cuenta, elige otro (por ejemplo, si tienes
una cuenta vieja de nombre con ese mismo nombre). Probado con la web original
y el emulador de Firebase (`tools/google_nombre_e2e.js`): la 3DS entra en la
MISMA cuenta de Google y tiene su partida.

## Con el botón «Entrar con Google» de la 3DS (QR, de serie)

Ajustes → Cuenta web → **Entrar con Google**. La consola enseña un **QR** y un
código de 8 letras:

1. Se escanea el QR con el móvil: lleva a una página del servidor del
   proyecto (`https://pokesurvivor3ds.duckdns.org/vincular/<código>`) que
   explica los pasos. Sin móvil: en el ordenador se abre
   `pokesurvivor3ds.duckdns.org/vincular/` y se escribe el código.
2. En el **ordenador**, con la cuenta de Google abierta en pokesurvivor.com, se
   abre la consola del navegador (F12) y se pega el trocito que da la página
   (botón «Copiar el código»). Sale «¡Listo!».
3. La 3DS entra sola en la MISMA cuenta de Google y sincroniza. No hay que
   repetirlo: la consola guarda su sesión como siempre (`cuenta.json`).

Cómo va por dentro: el trocito manda la sesión de Firebase de la web (el token
de renovación) al servicio `vincular` del servidor
(`pokesurvivor3ds-server/vincular`), que la guarda **sólo en memoria** hasta
que la consola (que pregunta cada 2 s con un secreto que sólo ella conoce) la
recoge, o 15 minutos. No se guarda en disco ni en registros. No toca la web ni
Firebase: es la misma sesión que ya tiene el navegador.

Pruebas: `tools/google_qr_e2e.js` (web ORIGINAL en Chromium + emulador de
Firebase + el servicio en local + `ps_cloud --qr`, el mismo código que la
consola) y `tools/google_qr_emu.sh` (lo mismo con la consola en Azahar; lee el
QR de la captura con zbar). En el servidor, `tests/test_vincular.py`.

## Otra forma: google.com/device (necesita un cliente OAuth)

La 3DS también sabe entrar como las teles: enseña un código que se escribe en
**google.com/device** desde el móvil, sin ordenador. Se usa en lugar del QR en
cuanto hay un cliente OAuth configurado (`kGoogleClientId` o `google.json`).
Probado con el emulador de Firebase (`tools/google_e2e.py`).

### Lo que falta para eso (sólo puede hacerlo quien administra pokesurvivor-free)

Google sólo da ese código a un **cliente OAuth** registrado, y Firebase sólo
acepta la cuenta si ese cliente es **del mismo proyecto** que la web. Son 5
minutos en la consola de Google Cloud, sin coste y sin tocar la web ni sus
reglas:

1. https://console.cloud.google.com/ → proyecto **pokesurvivor-free**.
2. *APIs y servicios* → *Credenciales* → *Crear credenciales* → *ID de cliente de OAuth*.
3. Tipo de aplicación: **TV y dispositivos de entrada limitada**. Nombre: `PokéSurvivor 3DS`.
4. Copiar el **ID de cliente** y el **secreto** y pasárselos a juanluismm521.
   (En este tipo de cliente el «secreto» no es secreto de verdad: va dentro de
   la aplicación, como en cualquier tele.)
5. Si la pantalla de consentimiento de OAuth está en modo «Prueba», pasarla a
   «En producción» (con los permisos básicos: correo y perfil no necesitan
   revisión de Google).

Después: se ponen en `src/platform/3ds/cloud3ds.cpp` (`kGoogleClientId`,
`kGoogleClientSecret`) y se compila. Para probar antes sin compilar: archivo
`google.json` en `sd:/3ds/pokesurvivor-3ds/` con
`{"client_id": "...", "client_secret": "..."}`.

## Por qué no hay otra forma sin la web

- Entrar con Google en una página propia (en el VPS) no vale: Firebase sólo
  deja hacerlo desde los dominios autorizados del proyecto (pokesurvivor.com).
- Un cliente OAuth creado en otro proyecto tampoco: Firebase rechaza su token
  («audience is not for this project»).
- Alternativa en la web (también la tendría que hacer JPTER78): en Ajustes,
  «Ponle nombre y contraseña a tu cuenta de Google» (`linkWithCredential` con
  `EmailAuthProvider` y el mismo email interno que `nameToEmail`): la 3DS
  entraría con ese nombre y contraseña sin cambiar nada más.

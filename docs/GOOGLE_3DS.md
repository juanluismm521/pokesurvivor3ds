# Entrar con Google en la 3DS

La 3DS ya sabe entrar con una cuenta de Google y sincronizar su partida con
pokesurvivor.com (la MISMA cuenta que en la web). Lo hace como las teles y las
consolas: enseña un código, y desde el móvil o el ordenador se entra en
**google.com/device**, se escribe y se elige la cuenta de Google.

Probado de punta a punta con el emulador de Firebase (`tools/google_e2e.py`):
una cuenta de nombre con su Gmail vinculado entra con Google y es la misma
(con su partida, y con nombre y contraseña sigue entrando); una cuenta creada
con Google en la web también entra y sincroniza.

## Lo que falta (sólo puede hacerlo quien administra pokesurvivor-free)

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

# Emulador de Firebase (sólo pruebas)

Emulador local de Firebase (Auth + Firestore) para probar la sincronización de
la 3DS con el código de la web sin tocar el proyecto de verdad
(pokesurvivor-free). El proyecto `demo-pokesurvivor` no necesita cuenta.

    cd tools/firebase-emu && ../node_modules/.bin/firebase emulators:start --project demo-pokesurvivor

Lo usa `tools/cloud_e2e.js` (web de verdad en Chromium + cliente de la 3DS).

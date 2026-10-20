// i18n.h — idiomas (i18n.js de la web). El juego está escrito en español; en
// inglés todo texto que se enseña se traduce con el diccionario de la web
// (lang-en.js, generado en kLangEn), con los mismos tres niveles:
//   1. frase exacta                 'Volver' → 'Back'
//   2. patrón con huecos {x}        '¡Has conseguido a {x} shiny!' → 'You got a shiny {x}!'
//                                   (lo que cae en el hueco también se traduce)
//   3. trozos dentro de un texto más largo, sólo palabras enteras
// Los resultados se guardan en una caché (cada texto se traduce una vez).
#pragma once

namespace ps {

enum Lang { LANG_ES, LANG_EN };
void i18n_set(Lang l);
// Otra tabla de traducciones (la de un paquete de datos; null: la de fábrica).
struct Tr;
void i18n_set_table(const Tr* t, int n);
Lang i18n_lang();
// Devuelve el texto traducido (un puntero que vive mientras no se vacíe la caché).
const char* tr(const char* s);

}  // namespace ps

// i18n.h — idiomas (i18n.js de la web). El juego está escrito en español; en
// inglés todo texto que se enseña se traduce con el diccionario de la web
// (lang-en.js, generado en kLangEn) y, antes que él, con el del port
// (lang_en_port.cpp: lo que la web no tiene o el port dice distinto), con los
// mismos tres niveles:
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
// Cómo se traduce un texto (para las pruebas y tools/i18n): 0 = no se traduce,
// 1 = frase exacta, 2 = patrón con huecos, 3 = sólo a trozos (suele quedar a medias).
enum TrKind { TRK_NONE, TRK_EXACT, TRK_PATTERN, TRK_FRAGS };
TrKind tr_kind(const char* s);
// Registro de lo que no se traduce entero (para revisar el inglés en el emulador):
// con on, cada texto nuevo que sale a trozos o sin traducir se apunta (sin repetir).
void tr_record(bool on);
// ¿Le queda español a un texto (palabras como «de», «con», «la» o tildes)? Para
// las pruebas: un patrón puede casar y dejar los huecos a medias.
bool tr_looks_spanish(const char* s);
// Lo apuntado, llamando a fn(texto, cómo queda, ud) por cada uno.
void tr_misses(void (*fn)(const char* es, const char* en, void* ud), void* ud);

}  // namespace ps

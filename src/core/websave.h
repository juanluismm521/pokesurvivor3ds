// websave.h — la partida de la web (db.js) tal cual, para sincronizar con
// pokesurvivor.com: crear, migrar, sanear, la libreta de monedas y tickets de
// cada aparato (led) y fusionar dos versiones sin perder nada. Trabaja sobre el
// JSON entero (también lo que la 3DS no usa: así no se pierde nada de la web).
// Y el formato de valores de Firestore (API REST) de ida y vuelta.
#pragma once
#include <cstdint>
#include <string>

#include "core/json.h"

namespace ps {
namespace web {

constexpr int kVersion = 3;

Json new_save(const std::string& name, int64_t now_ms);
// migrate de db.js (rellena lo que falte, pity antiguo, mejoras v2 → v3, sanitize).
// `strict` = exactamente como db.js (quita Pokémon, mejoras y movimientos que
// no conoce y recorta las mejoras al máximo). Para SINCRONIZAR se usa con
// strict = false: lo que la 3DS no conoce (porque la web se ha actualizado)
// se deja tal cual; sólo se arreglan números imposibles.
Json migrate(Json s, const std::string& name, int64_t now_ms, bool strict = true);
void sanitize(Json& s, bool strict = true);
// Libreta de una partida (sin tocarla); lo que no cuadre, al aparato `dev`.
Json led_of(const Json& s, const std::string& dev);
// Apunta en la libreta de `dev` lo que ha cambiado desde la última vez.
void fold(Json& s, const std::string& dev);
// Fusiona dos versiones de la misma partida (db.js merge).
Json merge(const Json& a, const Json& b, int64_t now_ms);
// Escribe `top` sobre `base` (objetos: clave a clave, en profundidad; lo
// demás se sustituye). Lo que sólo está en `base` se queda.
Json overlay(const Json& base, const Json& top);
// Como overlay, pero sólo con lo que HA CAMBIADO en la 3DS: `was` es lo que la
// 3DS entendía de `raw` al leerla y `now`, lo que tiene ahora. Lo que no ha
// cambiado se queda exactamente como lo dejó la web (también lo que la 3DS
// no sabe leer).
Json overlay_changes(const Json& raw, const Json& was, const Json& now);
// merge para sincronizar: `a` es la partida de la consola y `b`, la de la NUBE. En lo
// que la 3DS no conoce (claves nuevas de una web más nueva, la foto y el marco del
// perfil) manda la de la nube: la consola no lo cambia nunca y su copia es vieja.
Json merge_safe(const Json& a, const Json& b, int64_t now_ms);
// Antes de fusionar: apunta en `local` (ptmOn) las MT que ha enseñado la consola desde
// la última sincronización, como hace la web al enseñarlas (si no, una MT que se borró
// en la web y se ha vuelto a enseñar aquí se borraría otra vez).
void mark_taught(Json& local, const Json& remote, int64_t now_ms);
// Al acabar una sincronización (se pidió con la partida tal como estaba al
// empezarla): ¿hay que juntar otra vez con la de ahora? Sí si se guardó algo
// mientras (`updated` cambió) o si hay cambios SIN guardar (`dirty`: un shiny, un
// caramelo... de una partida que acaba de terminar; si no, se perderían).
inline bool sync_changed_since(int64_t updated_now, int64_t updated_t0, bool dirty) { return dirty || updated_now != updated_t0; }
// ¿Pierde `merged` algo que tenía `prev` (Pokémon, shinies, legendarios,
// logros, mejoras, estadísticas...)? Si sí, dice qué en `what`.
bool loses_progress(const Json& prev, const Json& merged, std::string& what);
// Versión del formato de una partida (0 si no la dice).
int version_of(const Json& s);

// ---- Firestore (REST) ----
// JSON → valor de Firestore ({"integerValue": "5"}, {"mapValue": {...}}...),
// como lo guarda el SDK de la web (los números enteros van como enteros).
Json to_firestore(const Json& v);
Json from_firestore(const Json& fv);

// ¿Son iguales dos JSON (los objetos sin importar el orden de las claves)?
bool same(const Json& a, const Json& b);

}  // namespace web
}  // namespace ps

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
// merge + red de seguridad para versiones nuevas de la web: lo que no es del
// formato conocido (claves nuevas) se UNE en vez de quedarse sólo lo del más
// nuevo.
Json merge_safe(const Json& a, const Json& b, int64_t now_ms);
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

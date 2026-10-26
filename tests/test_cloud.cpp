// Sincronización con la web: que una actualización de la web nunca rompa
// partidas. Con un Firestore falso (sin red) que hace lo mismo que el de verdad.
#include <doctest/doctest.h>

#include <cstring>
#include <ctime>
#include <memory>
#include <string>

#include "core/cloudsync.h"
#include "core/save.h"
#include "core/save_io.h"
#include "core/websave.h"

using namespace ps;

namespace {
struct FakeFs {
  Json doc;                  // campos del documento (formato Firestore) o null
  int commits = 0, no_referer = 0, conflicts = 0;
  int version = 1;           // updateTime del documento (cambia con cada escritura)
  int sneak = 0;             // escrituras de "otro aparato" justo después de leer (para probar el reintento)
  std::string last_commit;
  int64_t now = 1800000000000LL;   // reloj del servidor (avanza al esperar)
  int waited_ms = 0, too_soon = 0;
};

std::string iso(int64_t ms) {
  const time_t t = time_t(ms / 1000);
  tm g;
  gmtime_r(&t, &g);
  char b[40];
  std::snprintf(b, sizeof(b), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", g.tm_year + 1900, g.tm_mon + 1, g.tm_mday, g.tm_hour, g.tm_min, g.tm_sec, int(ms % 1000));
  return b;
}
void fake_sleep(int ms, void* ctx) { FakeFs& fs = *static_cast<FakeFs*>(ctx); fs.now += ms; fs.waited_ms += ms; }

bool fake_http(const cloud::HttpReq& rq, cloud::HttpResp& rs, void* ctx) {
  FakeFs& fs = *static_cast<FakeFs*>(ctx);
  bool ref = false;
  for (const auto& h : rq.headers) if (h == "Referer: https://pokesurvivor.com/") ref = true;
  if (!ref) fs.no_referer++;
  rs.status = 200;
  rs.date_ms = fs.now;
  const std::string& u = rq.url;
  if (u.find("signInWithPassword") != std::string::npos) rs.body = "{\"idToken\":\"t\",\"refreshToken\":\"r\",\"localId\":\"uid1\"}";
  else if (u.find("/v1/token") != std::string::npos) rs.body = "{\"id_token\":\"t\",\"refresh_token\":\"r\",\"user_id\":\"uid1\"}";
  else if (u.find(":beginTransaction") != std::string::npos || u.find(":rollback") != std::string::npos) {
    // Como pokesurvivor.com: las transacciones de la API REST, no.
    rs.status = 403;
    rs.body = "{\"error\":{\"code\":403,\"message\":\"Missing or insufficient permissions.\",\"status\":\"PERMISSION_DENIED\"}}";
  } else if (u.find(":batchGet") != std::string::npos) {
    Json arr = Json::array(), e = Json::object();
    if (fs.doc.is_obj()) {
      Json f = Json::object(); f.set("name", Json::str("x")); f.set("fields", fs.doc);
      f.set("updateTime", Json::str("2027-01-15T10:00:0" + std::to_string(fs.version) + "Z"));
      e.set("found", f);
    } else e.set("missing", Json::str("x"));
    arr.push(e);
    rs.body = arr.dump();
    if (fs.sneak > 0 && fs.doc.is_obj()) { fs.sneak--; fs.version++; }   // otro aparato escribe entre medias
  } else if (u.find(":commit") != std::string::npos) {
    Json b;
    b.parse(rq.body);
    // La condición del SDK: que el documento siga como se leyó.
    const Json& pre = b["writes"][0]["currentDocument"];
    const bool ok = pre["exists"].is_bool() ? (pre["exists"].as_bool() == fs.doc.is_obj())
                  : (fs.doc.is_obj() && pre["updateTime"].as_str() == "2027-01-15T10:00:0" + std::to_string(fs.version) + "Z");
    if (!ok || b.has("transaction")) {
      fs.conflicts++;
      rs.status = 400;
      rs.body = "{\"error\":{\"code\":400,\"message\":\"the stored version does not match the required base version.\",\"status\":\"FAILED_PRECONDITION\"}}";
      return true;
    }
    // Como pokesurvivor.com: no se guarda dos veces en menos de ~20 s.
    if (fs.doc.is_obj()) {
      const int64_t at = cloud::parse_timestamp(fs.doc["updatedAt"]["timestampValue"].as_str());
      if (at && fs.now - at < 20000) {
        fs.too_soon++;
        rs.status = 403;
        rs.body = "{\"error\":{\"code\":403,\"message\":\"Missing or insufficient permissions.\",\"status\":\"PERMISSION_DENIED\"}}";
        return true;
      }
    }
    fs.version++;
    fs.doc = b["writes"][0]["update"]["fields"];
    Json ts = Json::object();
    ts.set("timestampValue", Json::str(iso(fs.now)));
    fs.doc.set("updatedAt", ts);
    fs.commits++;
    fs.last_commit = rq.body;
    rs.body = "{}";
  } else { rs.status = 404; rs.body = "{}"; }
  return true;
}

// Partida de una web "del futuro": cosas que la 3DS de hoy no conoce.
Json future_web_save() {
  Json s;
  REQUIRE(s.parse(R"({
    "v": 3, "name": "Ana", "created": 1700000000000, "updated": 1800000000000, "coins": 900,
    "owned": {"25": {"at": 5, "from": "starter"}, "1100": {"at": 9, "from": "gacha"}},
    "shiny": {"1100": {"at": 10, "from": "gacha"}},
    "partner": 25, "partnerShiny": false, "starter": 25, "personality": null, "upgrades": {}, "pupgDone": true,
    "pupg": {"25": {"spd": 12, "superpoder": 7}},
    "tickets": {"t1": 2, "t10": 0}, "pity": {}, "legends": {}, "arenas": {}, "itemsFound": {}, "evoMoves": {}, "ach": {},
    "missions": {"day": "2027-01-15", "daily": [{"id": "mision-nueva", "goal": 5, "prog": 3, "claimed": false, "told": false}], "week": "w", "weekly": []},
    "profile": {"favs": [1100], "title": "Leyenda", "marco": "oro"},
    "stats": {"runs": 4, "bestTime": 300, "estadisticaNueva": 12},
    "led": {"base": {"c": [500, 0], "t1": [0, 0], "t10": [0, 0]}, "dweb": {"c": [400, 0], "t1": [2, 0], "gemas": [30, 5]}},
    "pbind": {"25": ["movimiento-nuevo", null, null, null]}, "pban": {},
    "dfav": {"1100": 1800000000500}, "pused": {"25": 4, "1100": 2}, "pstart": {"1100": "movimiento-nuevo"},
    "medallas": {"oro": 1700000000500}
  })"));
  return s;
}
}  // namespace

TEST_CASE("sincronizar: una web más nueva no pierde nada al pasar por la 3DS") {
  FakeFs fs;
  fs.doc = Json::object();
  fs.doc.set("save", web::to_firestore(future_web_save()));
  fs.doc.set("v", web::to_firestore(Json::num(3)));
  fs.doc.set("rango", web::to_firestore(Json::num(5)));   // campo nuevo del documento
  // La 3DS: tenía esa partida (de la última sincronización) y ha jugado.
  std::unique_ptr<Save> s(new Save());
  REQUIRE(save_from_json(*s, future_web_save().dump()));
  save_set_raw(future_web_save(), *s);
  s->coins += 100;                          // ha ganado monedas
  s->owned_at[7] = 1800000001000LL; s->owned_from[7] = 3;   // y un Squirtle
  s->updated = 1800000002000LL;
  const Json local = save_full_json(*s);
  // Lo que la 3DS no entiende sigue en su partida completa.
  CHECK(local["owned"].has("1100"));
  CHECK(local["pupg"]["25"]["superpoder"].as_num() == 7);
  CHECK(local["pupg"]["25"]["spd"].as_num() == 12);            // (la 3DS recorta a su máximo, pero no lo ha tocado)
  CHECK(local["pbind"]["25"][0].as_str() == "movimiento-nuevo");
  CHECK(local["missions"]["daily"][0]["id"].as_str() == "mision-nueva");
  CHECK(local["profile"]["marco"].as_str() == "oro");
  CHECK(local["stats"]["estadisticaNueva"].as_num() == 12);
  CHECK(local.has("medallas"));

  cloud::Client c;
  c.http = fake_http; c.ctx = &fs;
  cloud::Session ses;
  REQUIRE(cloud::login(c, "Ana", "x", ses));
  Json merged;
  REQUIRE_MESSAGE(cloud::sync(c, ses, local, "ctrprueba", 1800000002000LL, merged), c.error);
  CHECK(fs.commits == 1);
  CHECK(fs.no_referer == 0);   // (todas las peticiones dicen venir de la web)
  const Json cloud_save = web::from_firestore(fs.doc["save"]);
  // Todo lo de la web "futura" sigue en la nube...
  CHECK(cloud_save["owned"].has("1100"));
  CHECK(cloud_save["shiny"].has("1100"));
  CHECK(cloud_save["pupg"]["25"]["superpoder"].as_num() == 7);
  CHECK(cloud_save["pupg"]["25"]["spd"].as_num() == 12);
  CHECK(cloud_save["pbind"]["25"][0].as_str() == "movimiento-nuevo");
  CHECK(cloud_save["missions"]["daily"][0]["id"].as_str() == "mision-nueva");
  CHECK(cloud_save["profile"]["marco"].as_str() == "oro");
  CHECK(cloud_save["stats"]["estadisticaNueva"].as_num() == 12);
  CHECK(cloud_save["led"]["dweb"]["gemas"][0].as_num() == 30);
  CHECK(cloud_save["medallas"]["oro"].as_num() == 1700000000500.0);
  CHECK(web::from_firestore(fs.doc["rango"]).as_num() == 5);   // campo nuevo del documento
  CHECK(cloud_save["pstart"]["1100"].as_str() == "movimiento-nuevo");   // movimiento inicial de un Pokémon "nuevo"
  CHECK(cloud_save["pused"]["1100"].as_num() == 2);
  CHECK(cloud_save["dfav"]["1100"].as_num() == 1800000000500.0);
  // ...y lo de la 3DS también.
  CHECK(cloud_save["owned"].has("7"));
  CHECK(cloud_save["coins"].as_num() == 1000);
  CHECK(cloud_save["led"].has("ctrprueba"));
  save_raw() = Json();
}

TEST_CASE("sincronizar: fechas de Firestore") {
  CHECK(cloud::parse_timestamp("1970-01-01T00:00:00Z") == 0);
  CHECK(cloud::parse_timestamp("2027-01-15T08:00:00Z") == 1800000000000LL);
  CHECK(cloud::parse_timestamp("2026-10-20T14:36:51.250185Z") == cloud::parse_timestamp("2026-10-20T14:36:51Z") + 250);
  CHECK(cloud::parse_timestamp("2024-02-29T12:00:00.5Z") == 1709208000500LL);
  CHECK(cloud::parse_timestamp("basura") == 0);
}

TEST_CASE("sincronizar: si se acaba de guardar (web u otra consola), se espera al tope de pokesurvivor.com") {
  FakeFs fs;
  fs.doc = Json::object();
  fs.doc.set("save", web::to_firestore(future_web_save()));
  fs.doc.set("v", web::to_firestore(Json::num(3)));
  Json ts = Json::object();
  ts.set("timestampValue", Json::str(iso(fs.now - 5000)));   // la web guardó hace 5 s
  fs.doc.set("updatedAt", ts);
  cloud::Client c;
  c.http = fake_http; c.ctx = &fs;
  cloud::Session ses;
  REQUIRE(cloud::login(c, "Ana", "x", ses));
  Json merged;
  // Sin poder esperar: no se sube nada y se avisa.
  CHECK(!cloud::sync(c, ses, web::new_save("Ana", 1), "ctrprueba", fs.now, merged));
  CHECK(c.error.find("unos segundos") != std::string::npos);
  CHECK(fs.commits == 0);
  // Esperando: se espera lo que falta (20 s hasta los 25) y entra a la primera.
  c.sleep_ms = fake_sleep;
  REQUIRE_MESSAGE(cloud::sync(c, ses, web::new_save("Ana", 1), "ctrprueba", fs.now, merged), c.error);
  CHECK(fs.waited_ms == 21500);
  CHECK(fs.commits == 1);
  CHECK(fs.too_soon == 0);
  // Y sincronizar otra vez enseguida (botón dos veces): espera los 25 s.
  REQUIRE_MESSAGE(cloud::sync(c, ses, web::new_save("Ana", 1), "ctrprueba", fs.now, merged), c.error);
  CHECK(fs.waited_ms == 48000);
  CHECK(fs.commits == 2);
  CHECK(fs.too_soon == 0);
  save_raw() = Json();
}

TEST_CASE("sincronizar: si otro aparato escribe entre medias, se vuelve a leer y se junta") {
  FakeFs fs;
  fs.doc = Json::object();
  fs.doc.set("save", web::to_firestore(future_web_save()));
  fs.doc.set("v", web::to_firestore(Json::num(3)));
  fs.sneak = 1;
  cloud::Client c;
  c.http = fake_http; c.ctx = &fs;
  cloud::Session ses;
  REQUIRE(cloud::login(c, "Ana", "x", ses));
  Json merged;
  REQUIRE_MESSAGE(cloud::sync(c, ses, web::new_save("Ana", 1), "ctrprueba", 1800000002000LL, merged), c.error);
  CHECK(fs.conflicts == 1);
  CHECK(fs.commits == 1);
  CHECK(web::from_firestore(fs.doc["save"])["owned"].has("1100"));
  // Y una cuenta sin partida en la nube: se crea (condición «no existe»).
  FakeFs empty;
  c.ctx = &empty;
  REQUIRE_MESSAGE(cloud::sync(c, ses, web::new_save("Ana", 1), "ctrprueba", 1800000002000LL, merged), c.error);
  CHECK(empty.commits == 1);
  CHECK(empty.conflicts == 0);
  save_raw() = Json();
}

TEST_CASE("sincronizar: una partida de un formato más nuevo no se toca") {
  FakeFs fs;
  Json fut = future_web_save();
  fut.set("v", Json::num(4));
  fs.doc = Json::object();
  fs.doc.set("save", web::to_firestore(fut));
  fs.doc.set("v", web::to_firestore(Json::num(4)));
  const std::string before = fs.doc.dump();
  cloud::Client c;
  c.http = fake_http; c.ctx = &fs;
  cloud::Session ses;
  REQUIRE(cloud::login(c, "Ana", "x", ses));
  Json merged;
  CHECK(!cloud::sync(c, ses, web::new_save("Ana", 1), "ctrprueba", 1800000000000LL, merged));
  CHECK(c.error.find("más nueva") != std::string::npos);
  CHECK(fs.commits == 0);
  CHECK(fs.doc.dump() == before);
}

TEST_CASE("sincronizar: la red de seguridad detecta pérdidas") {
  Json prev = future_web_save(), merged = future_web_save();
  std::string what;
  CHECK(!web::loses_progress(prev, merged, what));
  merged.ref("owned").remove("25");
  CHECK(web::loses_progress(prev, merged, what));
  CHECK(what == "owned.25");
  merged = future_web_save();
  merged.ref("led").ref("dweb").set("c", Json::array());
  CHECK(web::loses_progress(prev, merged, what));
  // Una partida nueva y vacía en la nube: nada que perder.
  CHECK(!web::loses_progress(Json(), merged, what));
}

TEST_CASE("sincronizar: la 3DS sólo escribe lo que ha cambiado") {
  Json raw = future_web_save();
  std::unique_ptr<Save> s(new Save());
  REQUIRE(save_from_json(*s, raw.dump()));
  save_set_raw(raw, *s);
  // Sin cambios: la partida completa es la de la web, idéntica.
  CHECK(web::same(save_full_json(*s), raw));
  // Cambia sus atajos: ahora sí se escriben (los de la 3DS).
  s->pbind[25][1] = 0;
  const Json j = save_full_json(*s);
  CHECK(j["pbind"]["25"][1].is_str());
  save_raw() = Json();
}

// Entrar con Google desde la consola (flujo de dispositivo + signInWithIdp).
namespace {
struct FakeGoogle { int polls = 0; bool deny = false; std::string idp_body; };
bool fake_google(const cloud::HttpReq& rq, cloud::HttpResp& rs, void* ctx) {
  FakeGoogle& g = *static_cast<FakeGoogle*>(ctx);
  rs.status = 200;
  if (rq.url.find("/device/code") != std::string::npos) {
    CHECK(rq.body.find("client_id=cliente-tv") != std::string::npos);
    CHECK(rq.body.find("scope=openid%20email%20profile") != std::string::npos);
    rs.body = R"({"device_code":"DC","user_code":"ABCD-EFGH","verification_url":"https://www.google.com/device","interval":5,"expires_in":1800})";
  } else if (rq.url.find("/token") != std::string::npos && rq.url.find("securetoken") == std::string::npos) {
    CHECK(rq.body.find("device_code=DC") != std::string::npos);
    CHECK(rq.body.find("grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Adevice_code") != std::string::npos);
    g.polls++;
    rs.status = 428;
    if (g.deny) { rs.status = 403; rs.body = R"({"error":"access_denied"})"; }
    else if (g.polls == 1) rs.body = R"({"error":"authorization_pending"})";
    else if (g.polls == 2) rs.body = R"({"error":"slow_down"})";
    else { rs.status = 200; rs.body = R"({"access_token":"AT","id_token":"GOOGLE.ID.TOKEN","expires_in":3599})"; }
  } else if (rq.url.find("signInWithIdp") != std::string::npos) {
    g.idp_body = rq.body;
    bool ref = false;
    for (const auto& h : rq.headers) if (h == "Referer: https://pokesurvivor.com/") ref = true;
    CHECK(ref);
    rs.body = R"({"localId":"uid-google","idToken":"t","refreshToken":"r","displayName":"Juan Luis Martínez","email":"juan@gmail.com"})";
  } else { rs.status = 404; rs.body = "{}"; }
  return true;
}
}  // namespace

TEST_CASE("entrar con Google: código en la consola, se acepta en el móvil y es la misma cuenta") {
  FakeGoogle g;
  cloud::Client c;
  c.http = fake_google; c.ctx = &g;
  cloud::GoogleDevice d;
  cloud::Session s;
  // Sin cliente configurado: lo dice claro.
  CHECK(!cloud::google_start(c, d));
  CHECK(c.error.find("no está activado") != std::string::npos);
  c.cfg.google_client_id = "cliente-tv"; c.cfg.google_client_secret = "secreto";
  REQUIRE(cloud::google_start(c, d));
  CHECK(d.user_code == "ABCD-EFGH");
  CHECK(d.verification_url == "https://www.google.com/device");
  CHECK(cloud::google_poll(c, d, s) == 0);   // todavía no
  CHECK(cloud::google_poll(c, d, s) == 0);   // más despacio
  CHECK(d.interval == 10);
  REQUIRE(cloud::google_poll(c, d, s) == 1);
  CHECK(s.uid == "uid-google");
  CHECK(s.refresh_token == "r");
  CHECK(s.name == "Juan");
  CHECK(g.idp_body.find("providerId=google.com") != std::string::npos);
  CHECK(g.idp_body.find("id_token=GOOGLE.ID.TOKEN") != std::string::npos);
  // Si en Google se dice que no.
  FakeGoogle g2; g2.deny = true;
  c.ctx = &g2;
  REQUIRE(cloud::google_start(c, d));
  CHECK(cloud::google_poll(c, d, s) == -1);
  CHECK(c.error.find("cancelado") != std::string::npos);
}

TEST_CASE("sincronizar: el compañero, de donde se cambió (no el de la copia más nueva)") {
  // En la nube: Pikachu (25). En la consola, más nueva: Squirtle (7).
  auto run = [](int base, int& partner_out) {
    FakeFs fs;
    fs.doc = Json::object();
    fs.doc.set("save", web::to_firestore(future_web_save()));
    fs.doc.set("v", web::to_firestore(Json::num(3)));
    Json local = web::new_save("Ana", 1);
    Json owned = Json::object(), e = Json::object();
    e.set("at", Json::num(5)); e.set("from", Json::str("starter"));
    owned.set("7", e);
    local.set("owned", owned);
    local.set("partner", Json::num(7));
    local.set("updated", Json::num(1900000000000.0));   // (más nueva que la de la nube)
    cloud::Client c;
    c.http = fake_http; c.ctx = &fs;
    c.base_partner = base;
    cloud::Session ses;
    REQUIRE(cloud::login(c, "Ana", "x", ses));
    Json merged;
    REQUIRE_MESSAGE(cloud::sync(c, ses, local, "ctrprueba", 1800000002000LL, merged), c.error);
    partner_out = int(merged["partner"].as_num());
    CHECK(int(web::from_firestore(fs.doc["save"])["partner"].as_num()) == partner_out);   // (y eso es lo que se sube)
    save_raw() = Json();
  };
  int p = 0;
  run(-1, p);     CHECK(p == 25);   // primera vez con esta cuenta: el de la web
  run(25 * 2, p); CHECK(p == 7);    // sólo se cambió en la consola
  run(7 * 2, p);  CHECK(p == 25);   // sólo se cambió en la web
}

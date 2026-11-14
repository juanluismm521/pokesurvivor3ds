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
// Como la API de la partida de pokesurvivor.com (ice.pokesurvivor.com/v1/d).
struct FakeFs {
  Json save;                 // la partida en la nube (null = no hay)
  int64_t rev = 0;
  int commits = 0, no_referer = 0, conflicts = 0;
  int sneak = 0;             // guardados de "otro aparato" justo después de leer (para probar el reintento)
  std::string last_commit;
  int64_t now = 1800000000000LL;   // reloj del servidor (avanza al esperar)
  int64_t saved_at = 0;            // cuándo se guardó por última vez (0 = hace mucho)
  int waited_ms = 0, too_soon = 0;
  bool human = false;        // pide el «no soy un robot»
  int expired = 0;           // cuántas veces contesta 401 (token caducado)
  int refreshes = 0;
  Json eco;                  // lo que el servidor devuelve al guardar
  bool no_direct = false;    // la API directa no responde (red de la consola)
  int bridged = 0;           // peticiones que han ido por el puente
};

void fake_sleep(int ms, void* ctx) { FakeFs& fs = *static_cast<FakeFs*>(ctx); fs.now += ms; fs.waited_ms += ms; }

bool fake_http(const cloud::HttpReq& rq, cloud::HttpResp& rs, void* ctx) {
  FakeFs& fs = *static_cast<FakeFs*>(ctx);
  bool ref = false, bearer = false;
  for (const auto& h : rq.headers) {
    if (h == "Referer: https://pokesurvivor.com/") ref = true;
    if (h.rfind("Authorization: Bearer ", 0) == 0) bearer = true;
  }
  if (!ref) fs.no_referer++;
  rs.status = 200;
  rs.date_ms = fs.now;
  const std::string& u = rq.url;
  std::string api = "https://ice.pokesurvivor.com/v1/d";
  // (la consola que no llega a la API: «Timeout»; por el puente de nuestro servidor, sí)
  if (fs.no_direct && u.rfind(api, 0) == 0) { rs.status = 0; return false; }
  if (u.rfind("https://puente/webapi", 0) == 0) { fs.bridged++; api = "https://puente/webapi"; }
  if (u.find("signInWithPassword") != std::string::npos) { rs.body = "{\"idToken\":\"t\",\"refreshToken\":\"r\",\"localId\":\"uid1\"}"; return true; }
  if (u.find("/v1/token") != std::string::npos) { fs.refreshes++; rs.body = "{\"id_token\":\"t\",\"refresh_token\":\"r\",\"user_id\":\"uid1\"}"; return true; }
  if (u.rfind(api, 0) != 0 || !bearer) { rs.status = 404; rs.body = "Not found"; return true; }
  if (fs.expired > 0) { fs.expired--; rs.status = 401; rs.body = "{\"error\":\"auth\"}"; return true; }
  const std::string path = u.substr(api.size());
  if (path == "/me" && rq.method == "GET") { rs.body = "{\"nick\":\"Ana\"}"; return true; }
  if (path == "/save" && rq.method == "GET") {
    if (!fs.save.is_obj()) { rs.status = 404; rs.body = "{\"error\":\"none\"}"; return true; }
    Json o = Json::object();
    o.set("save", fs.save);
    o.set("rev", Json::num(double(fs.rev)));
    rs.body = o.dump();
    if (fs.sneak > 0) { fs.sneak--; fs.rev++; fs.saved_at = 0; }   // otro aparato guarda entre medias
    return true;
  }
  if (path == "/save" && rq.method == "PUT") {
    if (fs.human) { rs.status = 403; rs.body = "{\"error\":\"human\"}"; return true; }
    Json b;
    b.parse(rq.body);
    if (int64_t(b["rev"].as_num()) != fs.rev + 1) {
      fs.conflicts++;
      rs.status = 409;
      Json o = Json::object();
      o.set("error", Json::str("rev"));
      o.set("save", fs.save);
      o.set("rev", Json::num(double(fs.rev)));
      rs.body = o.dump();
      return true;
    }
    // Como pokesurvivor.com: no se guarda dos veces en menos de 20 s.
    if (fs.saved_at && fs.now - fs.saved_at < 20000) {
      fs.too_soon++;
      rs.status = 429;
      rs.body = "{\"error\":\"gap\",\"wait\":" + std::to_string((20000 - (fs.now - fs.saved_at) + 999) / 1000) + "}";
      return true;
    }
    fs.save = b["save"];
    fs.rev++;
    fs.saved_at = fs.now;
    fs.commits++;
    fs.last_commit = rq.body;
    Json o = Json::object();
    o.set("rev", Json::num(double(fs.rev)));
    if (fs.eco.is_obj()) o.set("eco", fs.eco);
    rs.body = o.dump();
    return true;
  }
  rs.status = 404; rs.body = "Not found";
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
  fs.save = future_web_save();
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
  const Json& cloud_save = fs.save;
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
  CHECK(cloud_save["pstart"]["1100"].as_str() == "movimiento-nuevo");   // movimiento inicial de un Pokémon "nuevo"
  CHECK(cloud_save["pused"]["1100"].as_num() == 2);
  CHECK(cloud_save["dfav"]["1100"].as_num() == 1800000000500.0);
  // ...y lo de la 3DS también.
  CHECK(cloud_save["owned"].has("7"));
  CHECK(cloud_save["coins"].as_num() == 1000);
  CHECK(cloud_save["led"].has("ctrprueba"));
  save_raw() = Json();
}

TEST_CASE("sincronizar: si se acaba de guardar (web u otra consola), se espera lo que diga pokesurvivor.com") {
  FakeFs fs;
  fs.save = future_web_save();
  fs.saved_at = fs.now - 5000;   // la web guardó hace 5 s
  cloud::Client c;
  c.http = fake_http; c.ctx = &fs;
  cloud::Session ses;
  REQUIRE(cloud::login(c, "Ana", "x", ses));
  Json merged;
  // Sin poder esperar: no se sube nada y se avisa.
  CHECK(!cloud::sync(c, ses, web::new_save("Ana", 1), "ctrprueba", fs.now, merged));
  CHECK(c.error.find("unos segundos") != std::string::npos);
  CHECK(fs.commits == 0);
  // Esperando: lo que dice la API (15 s) y un poco más, y entra.
  c.sleep_ms = fake_sleep;
  REQUIRE_MESSAGE(cloud::sync(c, ses, web::new_save("Ana", 1), "ctrprueba", fs.now, merged), c.error);
  CHECK(fs.waited_ms == 16500);
  CHECK(fs.commits == 1);
  // Y sincronizar otra vez enseguida (botón dos veces): espera los 20 s.
  REQUIRE_MESSAGE(cloud::sync(c, ses, web::new_save("Ana", 1), "ctrprueba", fs.now, merged), c.error);
  CHECK(fs.waited_ms == 16500 + 21500);
  CHECK(fs.commits == 2);
  save_raw() = Json();
}

TEST_CASE("sincronizar: si otro aparato escribe entre medias, se vuelve a leer y se junta") {
  FakeFs fs;
  fs.save = future_web_save();
  fs.sneak = 1;
  cloud::Client c;
  c.http = fake_http; c.ctx = &fs;
  cloud::Session ses;
  REQUIRE(cloud::login(c, "Ana", "x", ses));
  Json merged;
  REQUIRE_MESSAGE(cloud::sync(c, ses, web::new_save("Ana", 1), "ctrprueba", 1800000002000LL, merged), c.error);
  CHECK(fs.conflicts == 1);
  CHECK(fs.commits == 1);
  CHECK(fs.save["owned"].has("1100"));
  CHECK(fs.rev == 2);
  // Y una cuenta sin partida en la nube: se crea (rev 1).
  FakeFs empty;
  c.ctx = &empty;
  REQUIRE_MESSAGE(cloud::sync(c, ses, web::new_save("Ana", 1), "ctrprueba", 1800000002000LL, merged), c.error);
  CHECK(empty.commits == 1);
  CHECK(empty.conflicts == 0);
  CHECK(empty.rev == 1);
  save_raw() = Json();
}

TEST_CASE("sincronizar: una partida de un formato más nuevo no se toca") {
  FakeFs fs;
  Json fut = future_web_save();
  fut.set("v", Json::num(4));
  fs.save = fut;
  const std::string before = fs.save.dump();
  cloud::Client c;
  c.http = fake_http; c.ctx = &fs;
  cloud::Session ses;
  REQUIRE(cloud::login(c, "Ana", "x", ses));
  Json merged;
  CHECK(!cloud::sync(c, ses, web::new_save("Ana", 1), "ctrprueba", 1800000000000LL, merged));
  CHECK(c.error.find("más nueva") != std::string::npos);
  CHECK(fs.commits == 0);
  CHECK(fs.save.dump() == before);
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
    fs.save = future_web_save();
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
    CHECK(int(fs.save["partner"].as_num()) == partner_out);   // (y eso es lo que se sube)
    save_raw() = Json();
  };
  int p = 0;
  run(-1, p);     CHECK(p == 25);   // primera vez con esta cuenta: el de la web
  run(25 * 2, p); CHECK(p == 7);    // sólo se cambió en la consola
  run(7 * 2, p);  CHECK(p == 25);   // sólo se cambió en la web
}

TEST_CASE("sincronizar: lo que deja el servidor al guardar (eco) queda en la consola") {
  FakeFs fs;
  fs.save = future_web_save();
  Json eco;
  REQUIRE(eco.parse(R"({"coins": 850, "tickets": {"t1": 1, "t10": 0}, "led": {"base": {"c": [850, 0]}}, "dropOwned": [1100], "dropShiny": ["1100"]})"));
  fs.eco = eco;
  cloud::Client c;
  c.http = fake_http; c.ctx = &fs;
  cloud::Session ses;
  REQUIRE(cloud::login(c, "Ana", "x", ses));
  Json merged;
  REQUIRE_MESSAGE(cloud::sync(c, ses, web::new_save("Ana", 1), "ctrprueba", 1800000002000LL, merged), c.error);
  CHECK(merged["coins"].as_num() == 850);
  CHECK(merged["tickets"]["t1"].as_num() == 1);
  CHECK(!merged["owned"].has("1100"));
  CHECK(!merged["shiny"].has("1100"));
  CHECK(merged["owned"].has("25"));
  CHECK(merged["led"]["base"]["c"][0].as_num() == 850);
  // Como la web del 13-11-2026: el guardado dice qué versión del eco entiende.
  { Json b; REQUIRE(b.parse(fs.last_commit)); CHECK(b["eco"].as_num() == 2); }
  save_raw() = Json();
}

TEST_CASE("sincronizar: si en la nube quedó «Jugador» como nombre, se repone el de la cuenta") {
  FakeFs fs;
  fs.save = future_web_save();
  fs.save.set("name", Json::str("Jugador"));
  cloud::Client c;
  c.http = fake_http; c.ctx = &fs;
  cloud::Session ses;
  REQUIRE(cloud::login(c, "Ana", "x", ses));
  Json merged;
  REQUIRE_MESSAGE(cloud::sync(c, ses, web::new_save("Jugador", 1), "ctrprueba", 1800000002000LL, merged), c.error);
  CHECK(merged["name"].as_str() == "Ana");
  CHECK(fs.save["name"].as_str() == "Ana");
  save_raw() = Json();
}

TEST_CASE("sincronizar: «no soy un robot», token caducado y el apodo") {
  FakeFs fs;
  fs.save = future_web_save();
  fs.human = true;
  cloud::Client c;
  c.http = fake_http; c.ctx = &fs;
  cloud::Session ses;
  REQUIRE(cloud::login(c, "Ana", "x", ses));
  Json merged;
  // La consola no puede enseñar el Turnstile: se explica cómo pasarlo en la web.
  CHECK(!cloud::sync(c, ses, web::new_save("Ana", 1), "ctrprueba", 1800000002000LL, merged));
  CHECK(c.error.find("robot") != std::string::npos);
  CHECK(fs.commits == 0);
  // Token caducado a medias: se renueva una vez y sigue.
  fs.human = false;
  fs.expired = 1;
  const int before = fs.refreshes;
  REQUIRE_MESSAGE(cloud::sync(c, ses, web::new_save("Ana", 1), "ctrprueba", 1800000002000LL, merged), c.error);
  CHECK(fs.refreshes == before + 2);   // (la de sync y la del 401)
  CHECK(fs.commits == 1);
  CHECK(fs.no_referer == 0);
  CHECK(cloud::profile_name(c, ses) == "Ana");
  save_raw() = Json();
}

TEST_CASE("sincronizar: si la consola no llega a la API, va por el puente de nuestro servidor") {
  FakeFs fs;
  fs.save = future_web_save();
  fs.no_direct = true;
  cloud::Client c;
  c.http = fake_http; c.ctx = &fs;
  cloud::Session ses;
  REQUIRE(cloud::login(c, "Ana", "x", ses));
  Json merged;
  // Sin puente: «Sin conexión».
  CHECK(!cloud::sync(c, ses, web::new_save("Ana", 1), "ctrprueba", 1800000002000LL, merged));
  CHECK(c.error.find("Sin conexión") != std::string::npos);
  // Con puente: sincroniza, y se queda con él (para la próxima vez).
  c.error.clear();
  c.cfg.api_alt = "https://puente/webapi";
  REQUIRE_MESSAGE(cloud::sync(c, ses, web::new_save("Ana", 1), "ctrprueba", 1800000002000LL, merged), c.error);
  CHECK(fs.commits == 1);
  CHECK(fs.bridged >= 2);
  CHECK(c.cfg.api_base == "https://puente/webapi");
  CHECK(cloud::profile_name(c, ses) == "Ana");
  save_raw() = Json();
}

// cloudsync.cpp — ver cloudsync.h.
#include "core/cloudsync.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/websave.h"

namespace ps {
namespace cloud {

namespace {
const char* const kDomain = "jugadores.pokesurvivor.net";   // db.js NAME_DOMAIN

std::string urlenc(const std::string& s) {
  std::string o;
  for (unsigned char ch : s) {
    if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') o += char(ch);
    else { char b[4]; std::snprintf(b, sizeof(b), "%%%02X", ch); o += b; }
  }
  return o;
}

std::string jstr(const std::string& s) { return Json::str(s).dump(); }

// Mensajes de error de Firebase → español (como authError de db.js).
std::string auth_error(const Json& j, int status) {
  const std::string m = j["error"]["message"].as_str();
  if (m.find("INVALID_LOGIN_CREDENTIALS") != std::string::npos || m.find("INVALID_PASSWORD") != std::string::npos ||
      m.find("EMAIL_NOT_FOUND") != std::string::npos || m.find("INVALID_EMAIL") != std::string::npos)
    return "Usuario o contraseña incorrectos.";
  if (m.find("TOO_MANY_ATTEMPTS") != std::string::npos) return "Demasiados intentos seguidos. Espera un poco.";
  if (m.find("USER_DISABLED") != std::string::npos) return "Esa cuenta está desactivada.";
  if (m.find("TOKEN_EXPIRED") != std::string::npos || m.find("INVALID_REFRESH_TOKEN") != std::string::npos)
    return "La sesión ha caducado: vuelve a entrar.";
  if (m.find("referer") != std::string::npos || m.find("Referer") != std::string::npos)
    return "La nube no acepta peticiones de la consola (clave limitada a la web).";
  if (status == 0) return "Sin conexión a internet.";
  return "Error del servidor (" + std::to_string(status) + (m.empty() ? "" : ": " + m) + ").";
}

// Errores de la API de la partida (ice.pokesurvivor.com) → español.
std::string api_error(const Json& j, int status) {
  const std::string e = j["error"].is_str() ? j["error"].as_str() : "";
  // La web pide aquí que la persona resuelva un «no soy un robot» (Turnstile),
  // que la consola no puede enseñar: se resuelve en la web con la misma cuenta.
  if (status == 403 && e == "human")
    return "pokesurvivor.com pide comprobar que no eres un robot: entra con tu cuenta en pokesurvivor.com (móvil u ordenador), pulsa Guardar y vuelve a sincronizar.";
  if (status == 403) return "La nube ha rechazado el acceso (permiso denegado).";
  if (status == 401) return "La sesión ha caducado: vuelve a entrar.";
  if (status == 503 || status == 502) return "La nube de pokesurvivor.com está en mantenimiento: prueba más tarde.";
  if (status == 0) return "Sin conexión a internet.";
  return "Error del servidor (" + std::to_string(status) + (e.empty() ? "" : ": " + e) + ").";
}

bool call(Client& c, HttpReq& rq, HttpResp& rs) {
  rs = HttpResp();
  if (!c.cfg.referer.empty()) rq.headers.push_back("Referer: " + c.cfg.referer);
  if (!c.http || !c.http(rq, rs, c.ctx) || rs.status == 0) { c.error = "Sin conexión a internet."; return false; }
  return true;
}

// Pasa una letra de UTF-8 a minúscula (ASCII y las que admite VALID de db.js).
std::string lower_utf8(const std::string& in) {
  std::string o;
  for (size_t i = 0; i < in.size(); ++i) {
    const unsigned char ch = static_cast<unsigned char>(in[i]);
    if (ch < 0x80) { o += char(std::tolower(ch)); continue; }
    if (ch == 0xC3 && i + 1 < in.size()) {
      unsigned char c2 = static_cast<unsigned char>(in[i + 1]);
      // À..Þ (menos ×) → à..þ: en UTF-8, 0xC3 0x80..0x9E → 0xC3 0xA0..0xBE
      if (c2 >= 0x80 && c2 <= 0x9E && c2 != 0x97) c2 = static_cast<unsigned char>(c2 + 0x20);
      o += char(ch); o += char(c2); ++i;
      continue;
    }
    o += char(ch);
  }
  return o;
}

std::string trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
  return s.substr(a, b - a);
}
}  // namespace

std::string name_to_email(const std::string& name) {
  const std::string n = lower_utf8(trim(name));
  std::string hex;
  for (unsigned char ch : n) { char b[3]; std::snprintf(b, sizeof(b), "%02x", ch); hex += b; }
  return hex + "@" + kDomain;
}

bool login(Client& c, const std::string& name, const std::string& pass, Session& s) {
  HttpReq rq;
  rq.method = "POST";
  rq.url = c.cfg.auth_base + "/v1/accounts:signInWithPassword?key=" + c.cfg.api_key;
  rq.headers.push_back("Content-Type: application/json");
  rq.body = "{\"email\":" + jstr(name_to_email(name)) + ",\"password\":" + jstr(pass) + ",\"returnSecureToken\":true}";
  HttpResp rs;
  if (!call(c, rq, rs)) return false;
  Json j;
  j.parse(rs.body);
  if (rs.status != 200 || !j["idToken"].is_str()) { c.error = auth_error(j, rs.status); return false; }
  s.name = trim(name);
  s.uid = j["localId"].as_str();
  s.id_token = j["idToken"].as_str();
  s.refresh_token = j["refreshToken"].as_str();
  if (rs.date_ms) c.clock_delta = 0;   // (se calcula al sincronizar)
  return true;
}

// ---------------- entrar con Google ----------------

namespace {
// El nombre que se enseña (displayNameOf de db.js: la primera palabra, sin símbolos).
std::string display_name_of(const Json& j) {
  std::string n = trim(j["displayName"].as_str());
  if (n.empty()) n = trim(j["fullName"].as_str());
  if (n.empty()) { const std::string e = j["email"].as_str(); n = e.substr(0, e.find('@')); }
  n = n.substr(0, n.find(' '));
  std::string o;
  for (unsigned char ch : n) if (std::isalnum(ch) || ch == '_' || ch >= 0x80) o += char(ch);
  return o.empty() ? "Entrenador" : o;
}
std::string google_error(const std::string& e) {
  if (e == "access_denied") return "Has cancelado el permiso en Google.";
  if (e == "expired_token") return "El código ha caducado: vuelve a intentarlo.";
  if (e == "invalid_client" || e == "unauthorized_client") return "El inicio con Google de la 3DS no está activado todavía.";
  return "Google no ha dejado entrar (" + e + ").";
}
}  // namespace

bool google_start(Client& c, GoogleDevice& d) {
  if (c.cfg.google_client_id.empty()) { c.error = "El inicio con Google de la 3DS no está activado todavía."; return false; }
  HttpReq rq;
  rq.method = "POST";
  rq.url = c.cfg.google_device_url;
  rq.headers.push_back("Content-Type: application/x-www-form-urlencoded");
  rq.body = "client_id=" + urlenc(c.cfg.google_client_id) + "&scope=" + urlenc("openid email profile");
  HttpResp rs;
  rs = HttpResp();
  if (!c.http || !c.http(rq, rs, c.ctx) || rs.status == 0) { c.error = "Sin conexión a internet."; return false; }
  Json j;
  j.parse(rs.body);
  if (rs.status != 200 || !j["device_code"].is_str()) {
    c.error = google_error(j["error"].is_str() ? j["error"].as_str() : std::to_string(rs.status));
    return false;
  }
  d.device_code = j["device_code"].as_str();
  d.user_code = j["user_code"].as_str();
  d.verification_url = j["verification_url"].is_str() ? j["verification_url"].as_str() : j["verification_uri"].as_str();
  if (d.verification_url.empty()) d.verification_url = "https://www.google.com/device";
  d.interval = int(j["interval"].as_int(5));
  if (d.interval < 1 || d.interval > 60) d.interval = 5;
  d.expires_in = int(j["expires_in"].as_int(1800));
  return true;
}

int google_poll(Client& c, GoogleDevice& d, Session& s) {
  HttpReq rq;
  rq.method = "POST";
  rq.url = c.cfg.google_token_url;
  rq.headers.push_back("Content-Type: application/x-www-form-urlencoded");
  rq.body = "client_id=" + urlenc(c.cfg.google_client_id) + "&client_secret=" + urlenc(c.cfg.google_client_secret) +
            "&device_code=" + urlenc(d.device_code) + "&grant_type=" + urlenc("urn:ietf:params:oauth:grant-type:device_code");
  HttpResp rs;
  if (!c.http || !c.http(rq, rs, c.ctx) || rs.status == 0) return 0;   // (sin red un momento: se vuelve a preguntar)
  Json j;
  j.parse(rs.body);
  if (rs.status == 200 && j["id_token"].is_str()) return google_firebase(c, j["id_token"].as_str(), s) ? 1 : -1;
  const std::string e = j["error"].as_str();
  if (e == "authorization_pending") return 0;
  if (e == "slow_down") { d.interval += 5; return 0; }
  c.error = google_error(e.empty() ? std::to_string(rs.status) : e);
  return -1;
}

bool session_from_refresh(Client& c, const std::string& refresh_token, const std::string& name_hint, Session& s) {
  s = Session();
  s.refresh_token = refresh_token;
  if (!refresh(c, s)) return false;
  // El nombre: el que manda la web (displayName) o el de la cuenta (accounts:lookup).
  Json j = Json::object();
  j.set("displayName", Json::str(name_hint));
  if (name_hint.empty()) {
    HttpReq rq;
    rq.method = "POST";
    rq.url = c.cfg.auth_base + "/v1/accounts:lookup?key=" + c.cfg.api_key;
    rq.headers.push_back("Content-Type: application/json");
    rq.body = "{\"idToken\":" + jstr(s.id_token) + "}";
    HttpResp rs;
    Json r;
    if (call(c, rq, rs) && rs.status == 200 && r.parse(rs.body) && r["users"][0].is_obj()) j = r["users"][0];
  }
  s.name = display_name_of(j);
  return true;
}

bool google_firebase(Client& c, const std::string& google_id_token, Session& s) {
  HttpReq rq;
  rq.method = "POST";
  rq.url = c.cfg.auth_base + "/v1/accounts:signInWithIdp?key=" + c.cfg.api_key;
  rq.headers.push_back("Content-Type: application/json");
  const std::string post = "id_token=" + urlenc(google_id_token) + "&providerId=google.com";
  rq.body = "{\"postBody\":" + jstr(post) + ",\"requestUri\":\"https://pokesurvivor.com/\",\"returnSecureToken\":true,\"returnIdpCredential\":true}";
  HttpResp rs;
  if (!call(c, rq, rs)) return false;
  Json j;
  j.parse(rs.body);
  if (rs.status != 200 || !j["idToken"].is_str()) {
    const std::string m = j["error"]["message"].as_str();
    if (m.find("INVALID_IDP_RESPONSE") != std::string::npos || m.find("audience") != std::string::npos)
      c.error = "pokesurvivor.com no acepta todavía el inicio con Google desde la 3DS.";
    else c.error = auth_error(j, rs.status);
    return false;
  }
  s.name = display_name_of(j);
  s.uid = j["localId"].as_str();
  s.id_token = j["idToken"].as_str();
  s.refresh_token = j["refreshToken"].as_str();
  return true;
}

bool refresh(Client& c, Session& s) {
  HttpReq rq;
  rq.method = "POST";
  rq.url = c.cfg.token_base + "/v1/token?key=" + c.cfg.api_key;
  rq.headers.push_back("Content-Type: application/x-www-form-urlencoded");
  rq.body = "grant_type=refresh_token&refresh_token=" + urlenc(s.refresh_token);
  HttpResp rs;
  if (!call(c, rq, rs)) return false;
  Json j;
  j.parse(rs.body);
  if (rs.status != 200 || !j["id_token"].is_str()) { c.error = auth_error(j, rs.status); return false; }
  s.id_token = j["id_token"].as_str();
  if (j["refresh_token"].is_str()) s.refresh_token = j["refresh_token"].as_str();
  if (j["user_id"].is_str()) s.uid = j["user_id"].as_str();
  return true;
}

int partner_code(const Json& s) {
  const Json& p = s["partner"];
  const int d = p.is_num() ? int(p.as_num()) : p.is_str() ? std::atoi(p.as_str().c_str()) : 0;
  if (d <= 0) return -1;
  return d * 2 + (s["partnerShiny"].as_bool() ? 1 : 0);
}

void set_partner_code(Json& s, int code) {
  if (code < 0) return;
  char k[16];
  std::snprintf(k, sizeof(k), "%d", code / 2);
  if (!s["owned"][k].is_obj() && !s["owned"][k].as_bool()) return;   // (sólo si lo tiene)
  s.set("partner", Json::num(code / 2));
  s.set("partnerShiny", Json::boolean((code & 1) && (s["shiny"][k].is_obj() || s["shiny"][k].as_bool())));
}

// Llamada a la API de pokesurvivor.com (ice.pokesurvivor.com/v1/d, como
// api.js de la web): con el token de la cuenta. Si el token ha caducado (401),
// se renueva una vez y se repite.
bool api(Client& c, Session& s, const std::string& method, const std::string& path, const std::string* body, HttpResp& rs, Json& j) {
  for (int k = 0; k < 2; ++k) {
    HttpReq rq;
    rq.method = method;
    rq.url = c.cfg.api_base + path;
    rq.headers = {"Authorization: Bearer " + s.id_token};
    if (body) { rq.headers.push_back("Content-Type: application/json"); rq.body = *body; }
    if (!call(c, rq, rs)) {
      // La red de la consola no llega a la API (08-11-2026: un jugador, siempre «Timeout»
      // ahí y no en nuestro servidor): por el puente del servidor del proyecto, una vez.
      if (c.cfg.api_alt.empty() || c.cfg.api_alt == c.cfg.api_base) return false;
      std::swap(c.cfg.api_base, c.cfg.api_alt);
      c.error.clear();
      rq.url = c.cfg.api_base + path;
      if (!call(c, rq, rs)) return false;
    }
    j = Json();
    j.parse(rs.body);
    if (rs.status != 401 || k > 0 || !refresh(c, s)) return true;
  }
  return true;
}

std::string profile_name(Client& c, const Session& s) {
  if (s.uid.empty() || s.id_token.empty()) return "";
  Session t = s;
  HttpResp rs;
  Json j;
  const std::string keep = c.error;
  if (!api(c, t, "GET", "/me", nullptr, rs, j) || rs.status != 200) { c.error = keep; return ""; }
  const std::string n = j["nick"].as_str();
  return n.size() <= 40 ? n : "";
}

namespace {
// Lo que el servidor deja en la partida al guardar (eco de PUT /save, como
// db.js de la web): manda él en monedas, tickets, libreta y lo que quite.
void apply_eco(Json& save, const Json& eco) {
  if (!eco.is_obj()) return;
  if (eco["coins"].is_num()) save.set("coins", Json::num(std::max(0.0, std::floor(eco["coins"].as_num()))));
  if (eco["tickets"].is_obj()) {
    Json t = Json::object();
    t.set("t1", Json::num(std::max(0.0, std::floor(eco["tickets"]["t1"].as_num(0)))));
    t.set("t10", Json::num(std::max(0.0, std::floor(eco["tickets"]["t10"].as_num(0)))));
    save.set("tickets", t);
  }
  if (eco["led"].is_obj()) save.set("led", eco["led"]);
  for (const char* k : {"dropOwned", "dropShiny"}) {
    const char* coll = k[4] == 'O' ? "owned" : "shiny";
    if (!eco[k].is_arr() || !save[coll].is_obj()) continue;
    Json keep = Json::object();
    for (const auto& kv : save[coll].items()) {
      bool drop = false;
      for (const Json& d : eco[k].elems()) if ((d.is_str() ? d.as_str() : std::to_string(int64_t(d.as_num()))) == kv.first) drop = true;
      if (!drop) keep.set(kv.first, kv.second);
    }
    save.set(coll, keep);
  }
  for (const char* k : {"pupg", "bag", "tms"}) if (eco[k].is_obj()) save.set(k, eco[k]);
  // (el compañero, si ya no se tiene: el inicial o el primero, como la web)
  const Json& own = save["owned"];
  const auto have = [&](const char* coll, int d) { return save[coll].is_obj() && save[coll].has(std::to_string(d).c_str()); };
  const int pc = partner_code(save);
  if (pc >= 0 && own.is_obj() && !have("owned", pc / 2)) {
    int d = save["starter"].is_num() && have("owned", int(save["starter"].as_num())) ? int(save["starter"].as_num()) : -1;
    if (d < 0 && !own.items().empty()) d = std::atoi(own.items()[0].first.c_str());
    if (d > 0) set_partner_code(save, d * 2);
  }
  if (save["partnerShiny"].as_bool() && !have("shiny", partner_code(save) / 2)) save.set("partnerShiny", Json::boolean(false));
}
}  // namespace

bool sync(Client& c, Session& s, const Json& local_in, const std::string& dev, int64_t local_now_ms, Json& merged) {
  if (!refresh(c, s)) return false;
  // Como db.js de la web (11-2026): se lee la partida y su versión (rev) de
  // la API y se sube con rev + 1. Si otro la ha guardado entre medias, la API
  // contesta 409 con la suya: se junta y se repite.
  HttpResp rs;
  Json j;
  if (!api(c, s, "GET", "/save", nullptr, rs, j)) return false;
  if (rs.status != 200 && rs.status != 404) { c.error = api_error(j, rs.status); return false; }
  if (rs.date_ms) c.clock_delta = rs.date_ms - local_now_ms;
  Json remote = rs.status == 200 ? j["save"] : Json();
  int64_t rev = rs.status == 200 ? int64_t(j["rev"].as_num(0)) : 0;
  c.last_remote = remote;
  int waits = 0;
  for (int attempt = 0; attempt < 4; ++attempt) {
    if (rev < 0) rev = 0;
    // Una web más nueva (otro formato de partida): no se toca nada.
    if (web::version_of(remote) > web::kVersion) {
      c.error = "La partida de la web es de una versión más nueva del juego: actualiza el juego de la 3DS para sincronizar (no se ha tocado nada).";
      return false;
    }
    // Fusión (la hora de la 3DS, pasada a la del servidor: su reloj va en hora local)
    Json local = local_in;
    if (local["updated"].is_num()) local.set("updated", Json::num(local["updated"].as_num() + double(c.clock_delta)));
    const int64_t now = local_now_ms + c.clock_delta;
    // (sin strict: lo que la 3DS no conoce de una web más nueva se queda)
    local = web::migrate(local, "", now, false);
    web::fold(local, dev);
    const Json rem = remote.is_obj() ? web::migrate(remote, "", now, false) : Json();
    merged = rem.is_obj() ? web::merge_safe(local, rem, now) : local;
    // El compañero: la fusión de la web se queda con el de la copia más nueva,
    // pero la consola guarda por cualquier cosa (una misión, un logro) y su
    // copia suele parecer más nueva aunque el compañero se cambiara en la web.
    // Se mira dónde se cambió desde la última sincronización de esta consola:
    //   primera vez con esta cuenta → el de la web; sólo en la web → el de la
    //   web; sólo aquí → el de aquí; en los dos → lo de la fusión.
    if (rem.is_obj()) {
      const int lp = partner_code(local), rp = partner_code(rem), base = c.base_partner;
      if (rp >= 0 && (base < 0 || (lp == base && rp != base))) set_partner_code(merged, rp);
      else if (lp >= 0 && lp != base && rp == base) set_partner_code(merged, lp);
    }
    // Última red de seguridad: si la fusión perdiera algo, no se sube nada.
    std::string lost;
    if (web::loses_progress(rem, merged, lost) || web::loses_progress(local, merged, lost)) {
      c.error = "Por seguridad no se ha sincronizado (se perdería " + lost + "). Avisa al creador del juego.";
      return false;
    }
    // Subir
    Json body = Json::object();
    body.set("save", merged);
    body.set("rev", Json::num(double(rev + 1)));
    const std::string b = body.dump();
    if (!api(c, s, "PUT", "/save", &b, rs, j)) return false;
    if (rs.status == 200) { apply_eco(merged, j["eco"]); return true; }
    // Otro aparato ha guardado entre medias: se junta con la suya y se repite.
    if (rs.status == 409 && j["error"].as_str() == "rev") {
      if (j["save"].is_obj()) { remote = j["save"]; c.last_remote = remote; }
      rev = int64_t(j["rev"].as_num(double(rev)));
      continue;
    }
    // Se acaba de guardar (desde la web u otro aparato): la API dice cuánto esperar.
    if (rs.status == 429 && j["error"].as_str() == "gap") {
      const int wait = int(std::min(60.0, std::max(1.0, j["wait"].as_num(20)))) * 1000 + 1500;
      if (!c.sleep_ms || waits >= 2) {
        c.error = "Se acaba de guardar la partida desde otro sitio: prueba otra vez en unos segundos.";
        return false;
      }
      c.sleep_ms(wait, c.ctx);
      local_now_ms += wait;
      ++waits;
      --attempt;   // (esperar no cuenta como intento)
      continue;
    }
    c.error = rs.status == 403 && j["error"].as_str() != "human" ? "La nube ha rechazado la partida (permiso denegado)." : api_error(j, rs.status);
    return false;
  }
  c.error = "La nube está ocupada; prueba otra vez.";
  return false;
}

}  // namespace cloud
}  // namespace ps

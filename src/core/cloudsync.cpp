// cloudsync.cpp — ver cloudsync.h.
#include "core/cloudsync.h"

#include <cctype>
#include <cstdio>
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

bool call(Client& c, HttpReq& rq, HttpResp& rs) {
  rs = HttpResp();
  if (!c.cfg.referer.empty()) rq.headers.push_back("Referer: " + c.cfg.referer);
  if (!c.http || !c.http(rq, rs, c.ctx) || rs.status == 0) { c.error = "Sin conexión a internet."; return false; }
  return true;
}

// Deshace una transacción a medias (si no, el documento queda bloqueado un rato).
void rollback(Client& c, const std::string& base, const std::string& auth, const std::string& tx) {
  HttpReq rq;
  rq.method = "POST";
  rq.url = base + ":rollback";
  rq.headers = {"Content-Type: application/json", auth};
  rq.body = "{\"transaction\":" + jstr(tx) + "}";
  if (!c.cfg.referer.empty()) rq.headers.push_back("Referer: " + c.cfg.referer);
  HttpResp rs;
  if (c.http) c.http(rq, rs, c.ctx);
}

std::string doc_path(const Client& c, const Session& s) {
  return "projects/" + c.cfg.project + "/databases/(default)/documents/saves/" + s.uid;
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

bool sync(Client& c, Session& s, const Json& local_in, const std::string& dev, int64_t local_now_ms, Json& merged) {
  if (!refresh(c, s)) return false;
  const std::string auth = "Authorization: Bearer " + s.id_token;
  const std::string base = c.cfg.fs_base + "/v1/projects/" + c.cfg.project + "/databases/(default)/documents";
  for (int attempt = 0; attempt < 3; ++attempt) {
    // 1. transacción
    HttpReq rq;
    rq.method = "POST";
    rq.url = base + ":beginTransaction";
    rq.headers = {"Content-Type: application/json", auth};
    rq.body = "{}";
    HttpResp rs;
    if (!call(c, rq, rs)) return false;
    Json j;
    j.parse(rs.body);
    if (rs.status != 200 || !j["transaction"].is_str()) { c.error = auth_error(j, rs.status); return false; }
    const std::string tx = j["transaction"].as_str();
    if (rs.date_ms) c.clock_delta = rs.date_ms - local_now_ms;
    // 2. lo que hay en la nube (batchGet dentro de la transacción, como el SDK)
    rq = HttpReq();
    rq.method = "POST";
    rq.url = base + ":batchGet";
    rq.headers = {"Content-Type: application/json", auth};
    rq.body = "{\"documents\":[" + jstr(doc_path(c, s)) + "],\"transaction\":" + jstr(tx) + "}";
    if (!call(c, rq, rs)) { rollback(c, base, auth, tx); return false; }
    Json remote, other = Json::object();   // (otros campos del documento: se conservan)
    bool ban = false;
    int doc_v = 0;
    {
      Json arr;
      arr.parse(rs.body);
      if (rs.status != 200 || !arr.is_arr()) {
        c.error = rs.status == 403 ? "La nube ha rechazado el acceso (permiso denegado)." : auth_error(arr.is_arr() ? Json() : arr, rs.status);
        rollback(c, base, auth, tx);
        return false;
      }
      for (const Json& e : arr.elems()) {
        if (!e["found"].is_obj()) continue;
        const Json& f = e["found"]["fields"];
        remote = web::from_firestore(f["save"]);
        ban = f["ban"]["booleanValue"].as_bool();
        doc_v = int(web::from_firestore(f["v"]).as_num(0));
        for (const auto& kv : f.items())
          if (kv.first != "save" && kv.first != "v" && kv.first != "updatedAt" && kv.first != "ban") other.set(kv.first, kv.second);
      }
    }
    c.last_remote = remote;
    // Una web más nueva (otro formato de partida): no se toca nada.
    if (doc_v > web::kVersion || web::version_of(remote) > web::kVersion) {
      c.error = "La partida de la web es de una versión más nueva del juego: actualiza el juego de la 3DS para sincronizar (no se ha tocado nada).";
      rollback(c, base, auth, tx);
      return false;
    }
    // 3. fusión (la hora de la 3DS, pasada a la del servidor: su reloj va en hora local)
    Json local = local_in;
    if (local["updated"].is_num()) local.set("updated", Json::num(local["updated"].as_num() + double(c.clock_delta)));
    const int64_t now = local_now_ms + c.clock_delta;
    // (sin strict: lo que la 3DS no conoce de una web más nueva se queda)
    local = web::migrate(local, "", now, false);
    web::fold(local, dev);
    const Json rem = remote.is_obj() ? web::migrate(remote, "", now, false) : Json();
    merged = rem.is_obj() ? web::merge_safe(local, rem, now) : local;
    // Última red de seguridad: si la fusión perdiera algo, no se sube nada.
    std::string lost;
    if (web::loses_progress(rem, merged, lost) || web::loses_progress(local, merged, lost)) {
      c.error = "Por seguridad no se ha sincronizado (se perdería " + lost + "). Avisa al creador del juego.";
      rollback(c, base, auth, tx);
      return false;
    }
    // 4. subir
    Json fields = other;
    fields.set("save", web::to_firestore(merged));
    fields.set("v", web::to_firestore(Json::num(web::kVersion)));
    if (ban) fields.set("ban", web::to_firestore(Json::boolean(true)));
    Json doc = Json::object();
    doc.set("name", Json::str(doc_path(c, s)));
    doc.set("fields", fields);
    Json tr = Json::object();
    tr.set("fieldPath", Json::str("updatedAt"));
    tr.set("setToServerValue", Json::str("REQUEST_TIME"));
    Json trs = Json::array();
    trs.push(tr);
    Json w = Json::object();
    w.set("update", doc);
    w.set("updateTransforms", trs);
    Json writes = Json::array();
    writes.push(w);
    Json body = Json::object();
    body.set("writes", writes);
    body.set("transaction", Json::str(tx));
    rq = HttpReq();
    rq.method = "POST";
    rq.url = base + ":commit";
    rq.headers = {"Content-Type: application/json", auth};
    rq.body = body.dump();
    if (!call(c, rq, rs)) { rollback(c, base, auth, tx); return false; }
    if (rs.status == 200) return true;
    rollback(c, base, auth, tx);
    if (rs.status == 409) continue;   // otra consola ha escrito a la vez: se repite
    j = Json();
    j.parse(rs.body);
    c.error = rs.status == 403 ? "La nube ha rechazado la partida (permiso denegado)." : auth_error(j, rs.status);
    return false;
  }
  c.error = "La nube está ocupada; prueba otra vez.";
  return false;
}

}  // namespace cloud
}  // namespace ps

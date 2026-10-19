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
  if (status == 0) return "Sin conexión a internet.";
  return "Error del servidor (" + std::to_string(status) + (m.empty() ? "" : ": " + m) + ").";
}

bool call(Client& c, HttpReq& rq, HttpResp& rs) {
  rs = HttpResp();
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
    Json remote;
    bool ban = false;
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
        remote = web::from_firestore(e["found"]["fields"]["save"]);
        ban = e["found"]["fields"]["ban"]["booleanValue"].as_bool();
      }
    }
    // 3. fusión (la hora de la 3DS, pasada a la del servidor: su reloj va en hora local)
    Json local = local_in;
    if (local["updated"].is_num()) local.set("updated", Json::num(local["updated"].as_num() + double(c.clock_delta)));
    const int64_t now = local_now_ms + c.clock_delta;
    local = web::migrate(local, "", now);
    web::fold(local, dev);
    merged = remote.is_obj() ? web::merge(local, web::migrate(remote, "", now), now) : local;
    // 4. subir
    Json fields = Json::object();
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

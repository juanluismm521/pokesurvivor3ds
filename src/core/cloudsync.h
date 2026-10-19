// cloudsync.h — sincronizar la partida con pokesurvivor.com (Firebase), como
// hace la web (db.js): entrar con usuario y contraseña, renovar el token y
// subir/bajar la partida con una transacción que la FUSIONA con la de la nube
// (web::merge), apuntando lo de la 3DS en su propia libreta.
//
// Sin nada de la 3DS: el HTTP lo pone quien llama (curl en la consola; en las
// pruebas de PC, un Firebase falso o el emulador de Firebase).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/json.h"

namespace ps {
namespace cloud {

struct HttpReq {
  std::string method = "GET";
  std::string url;
  std::vector<std::string> headers;   // "Clave: valor"
  std::string body;
};
struct HttpResp {
  int status = 0;                     // 0 = sin conexión
  std::string body;
  int64_t date_ms = 0;                // cabecera Date del servidor (0 si no hay)
};
using HttpFn = bool (*)(const HttpReq& req, HttpResp& resp, void* ctx);

struct Config {
  std::string api_key;
  std::string project = "pokesurvivor-free";
  std::string auth_base = "https://identitytoolkit.googleapis.com";
  std::string token_base = "https://securetoken.googleapis.com";
  std::string fs_base = "https://firestore.googleapis.com";
  // La clave de la web sólo acepta peticiones que vengan de la web (Referer);
  // con permiso del proyecto, la 3DS se presenta como la web.
  std::string referer = "https://pokesurvivor.com/";
};

struct Session {
  std::string name;                   // nombre de la cuenta (como se escribió)
  std::string uid, id_token, refresh_token;
};

struct Client {
  Config cfg;
  HttpFn http = nullptr;
  void* ctx = nullptr;
  std::string error;                  // último error, en español
  int64_t clock_delta = 0;            // hora del servidor - hora de esta consola (ms)
  Json last_remote;                   // la partida de la nube tal como estaba (copia de seguridad)
};

// db.js nameToEmail: el nombre en minúsculas, en hexadecimal (UTF-8), @dominio.
std::string name_to_email(const std::string& name);

bool login(Client& c, const std::string& name, const std::string& pass, Session& s);
bool refresh(Client& c, Session& s);
// Sube y baja: `local` es la partida completa de la 3DS; `dev`, el nombre de
// este aparato en la libreta; `local_now_ms`, la hora de la consola. Deja en
// `merged` la partida fusionada (la que queda en la nube).
bool sync(Client& c, Session& s, const Json& local, const std::string& dev, int64_t local_now_ms, Json& merged);

}  // namespace cloud
}  // namespace ps

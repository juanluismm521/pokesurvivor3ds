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
  // La partida: la API de pokesurvivor.com (desde noviembre de 2026 la web ya no la
  // guarda en Firestore; permiso del dueño de la web para la 3DS, 06-11-2026).
  std::string api_base = "https://ice.pokesurvivor.com/v1/d";
  // Si la consola no llega a la API, el puente del servidor del proyecto (lo mismo, por
  // pokesurvivor3ds.duckdns.org/webapi). Tras sincronizar, api_base es el que ha ido.
  std::string api_alt;
  // La clave de la web sólo acepta peticiones que vengan de la web (Referer);
  // con permiso del proyecto, la 3DS se presenta como la web.
  std::string referer = "https://pokesurvivor.com/";
  // Entrar con Google (OAuth para aparatos sin navegador: la consola enseña un
  // código y se acepta en google.com/device desde el móvil). Hace falta un
  // cliente OAuth de tipo «TV y dispositivos de entrada limitada» del MISMO
  // proyecto de Google que la web (si no, Firebase no acepta su token).
  std::string google_client_id, google_client_secret;
  std::string google_device_url = "https://oauth2.googleapis.com/device/code";
  std::string google_token_url = "https://oauth2.googleapis.com/token";
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
  // Compañero en la última sincronización de esta consola (partner_code), o -1
  // si es la primera con esta cuenta. Sirve para saber DÓNDE se cambió el
  // compañero (ver partner_code y sync).
  int base_partner = -1;
  // Esperar (lo pone la plataforma; null = no se espera y se avisa). Para el
  // tope de pokesurvivor.com entre dos guardados de la misma partida.
  void (*sleep_ms)(int ms, void* ctx) = nullptr;
};

// Compañero de una partida como un número: dex·2 + shiny (-1 si no tiene).
int partner_code(const Json& save);
void set_partner_code(Json& save, int code);


// db.js nameToEmail: el nombre en minúsculas, en hexadecimal (UTF-8), @dominio.
std::string name_to_email(const std::string& name);

bool login(Client& c, const std::string& name, const std::string& pass, Session& s);

// ---- Entrar con Google (flujo de dispositivo) ----
struct GoogleDevice {
  std::string device_code, user_code, verification_url;
  int interval = 5;                   // segundos entre preguntas
  int expires_in = 1800;              // validez del código (s)
};
// Pide el código que se enseña en la consola.
bool google_start(Client& c, GoogleDevice& d);
// ¿Lo ha aceptado ya? 1 = sí (sesión de Firebase en `s`, la MISMA cuenta que
// en la web), 0 = todavía no (esperar d.interval s), -1 = error (c.error).
int google_poll(Client& c, GoogleDevice& d, Session& s);
// Con la sesión que llega de la web (entrar con Google por QR, servidor
// «vincular»): el token de renovación de Firebase → sesión con su nombre.
bool session_from_refresh(Client& c, const std::string& refresh_token, const std::string& name_hint, Session& s);
// Con un token de Google ya conseguido: la cuenta de Firebase (signInWithIdp).
bool google_firebase(Client& c, const std::string& google_id_token, Session& s);
bool refresh(Client& c, Session& s);
// Sube y baja: `local` es la partida completa de la 3DS; `dev`, el nombre de
// este aparato en la libreta; `local_now_ms`, la hora de la consola. Deja en
// `merged` la partida fusionada (la que queda en la nube).
bool sync(Client& c, Session& s, const Json& local, const std::string& dev, int64_t local_now_ms, Json& merged);
// El apodo de jugador de la cuenta (GET /me: el que se ve en pokesurvivor.com).
// Vacío si no tiene (o no se puede leer). Tras sync (usa su id_token).
std::string profile_name(Client& c, const Session& s);

}  // namespace cloud
}  // namespace ps

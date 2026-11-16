// cloud3ds.h — sincronizar con pokesurvivor.com desde la consola: red (soc),
// HTTPS (curl + mbedTLS, certificados en romfs:/cacert.pem), la cuenta en la SD
// (cuenta.json: nombre, uid, token de renovación y el nombre de esta consola
// en la libreta; NUNCA la contraseña) y un hilo que hace el trabajo para que la
// pantalla no se congele. La lógica está en core/cloudsync.
#pragma once
#include <cstdint>
#include <string>

#include "core/cloudsync.h"
#include "core/json.h"

namespace ps {
namespace cloud3ds {

struct Account {
  bool on = false;               // hay sesión
  std::string name, uid, refresh, dev;
  int64_t delta = 0;             // hora del servidor - hora de la consola
  int64_t last_sync = 0;         // (hora de la consola)
  int synced_partner = -1;       // compañero en la última sincronización (cloud::partner_code)
  bool google = false;           // se entró con Google (para el logo)
  bool bridge = false;           // la API de la partida va por el puente de nuestro servidor (su red no llega)
};

enum JobState { JOB_IDLE, JOB_BUSY, JOB_DONE, JOB_FAILED };

void init(const char* dir);      // lee cuenta.json (no toca la red)
const Account& account();
void logout();

// Trabajo en segundo plano. Uno a la vez.
bool start_login(const std::string& name, const std::string& pass, const Json& local);   // entra y sincroniza
bool start_sync(const Json& local);
// Entrar con Google. De serie, por QR: la consola pide un código al servidor
// del proyecto (servicio «vincular») y enseña un QR a su página, que explica
// cómo mandar la sesión de Google desde pokesurvivor.com; la consola la recoge
// y entra sola. Si hay un cliente OAuth de Google configurado (google.json),
// en su lugar el código para google.com/device. Luego sincroniza como siempre.
bool start_google(const Json& local);
bool google_code(std::string& code, std::string& url);   // el código y su dirección, cuando los hay
bool google_qr();                // el código va en un QR (url = la página)
void cancel_google();
JobState state();
// Al acabar bien: la partida fusionada (para aplicarla) y se guarda la cuenta.
const Json& result();
const Json& last_remote();     // la partida de la nube antes de sincronizar (copia)
const std::string& error();
void finish();                   // vuelve a JOB_IDLE

// Lo ganado en las partidas (core/runclaim.h): desde el 15-11-2026 pokesurvivor.com sólo lo suma si
// le llega con su reclamo. Cada partida que acaba deja el suyo en runq.json (SD); se cobran al
// sincronizar, antes de leer la partida (con la cuenta con la que se jugaron, o con la primera
// con la que entres si se jugaron sin cuenta).
void queue_claim(const Json& claim);
// Al empezar una partida (con cuenta y sin otro trabajo en marcha): pide su id a pokesurvivor.com
// en segundo plano; queue_claim lo pone en el reclamo si ha llegado (si no, va sin id, como las
// partidas sin conexión).
void begin_run();
int queued_claims();
// El gacha con la cuenta de la web: se tira en el servidor (como la web). Al acabar bien, result()
// es la partida resultante y pull_results() lo que ha salido; si falla, pull_code(): "tickets",
// "closed" u otro (error()).
bool start_pull(const Json& local, int n, int gen, bool t10);
bool pull_job();
// ¿Hay conexión con pokesurvivor.com? (renueva la sesión de la cuenta; no toca la partida). Al
// abrir el gacha con cuenta: sin conexión no se entra (el servidor quitaría lo que saliera).
bool start_check();
bool check_job();
bool run_job();                  // el trabajo es el id de una partida (begin_run)                 // el trabajo de ahora (o el que acaba de terminar) es una tirada
const Json& pull_results();
// Monedas de las partidas cobradas en la última sincronización (pedidas y pagadas: el servidor
// tiene un tope). Se leen una vez (luego quedan a 0).
void claim_cut(int64_t& asked, int64_t& paid);
const std::string& pull_code();

void shutdown();
// Red de la consola (socInit), compartida con el juego online. Una sola vez.
bool soc_up();
// GET por HTTPS (para los paquetes de datos; se puede llamar desde otro hilo).
bool http_get(const std::string& url, std::string& body, int& status, std::string& err);
// La hora UTC del servidor (ms), si alguna respuesta la ha traído (cabecera Date).
bool server_now_ms(int64_t& out);
// Descarga grande a un archivo (para las versiones nuevas del juego): escribe en
// `path`.tmp y, si llega entera (tamaño `expect` y huella `hash`), la pasa a
// `path`. `pct` (0-100) para enseñar el avance; `cancel` la corta.
bool http_download(const std::string& url, const std::string& path, uint64_t expect, const uint8_t hash[32],
                   volatile int* pct, volatile bool* cancel, std::string& err);
// Prueba de HTTPS (autotest): una renovación con un token inventado, que la
// nube tiene que rechazar (400). Devuelve el código HTTP (0 = sin conexión) y
// el error de curl si lo hay. No toca ningún dato.
int probe_https(std::string& curl_error);

}  // namespace cloud3ds
}  // namespace ps

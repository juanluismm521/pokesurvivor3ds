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
};

enum JobState { JOB_IDLE, JOB_BUSY, JOB_DONE, JOB_FAILED };

void init(const char* dir);      // lee cuenta.json (no toca la red)
const Account& account();
void logout();

// Trabajo en segundo plano. Uno a la vez.
bool start_login(const std::string& name, const std::string& pass, const Json& local);   // entra y sincroniza
bool start_sync(const Json& local);
JobState state();
// Al acabar bien: la partida fusionada (para aplicarla) y se guarda la cuenta.
const Json& result();
const Json& last_remote();     // la partida de la nube antes de sincronizar (copia)
const std::string& error();
void finish();                   // vuelve a JOB_IDLE

void shutdown();
// Red de la consola (socInit), compartida con el juego online. Una sola vez.
bool soc_up();
// GET por HTTPS (para los paquetes de datos; se puede llamar desde otro hilo).
bool http_get(const std::string& url, std::string& body, int& status, std::string& err);
// Prueba de HTTPS (autotest): una renovación con un token inventado, que la
// nube tiene que rechazar (400). Devuelve el código HTTP (0 = sin conexión) y
// el error de curl si lo hay. No toca ningún dato.
int probe_https(std::string& curl_error);

}  // namespace cloud3ds
}  // namespace ps

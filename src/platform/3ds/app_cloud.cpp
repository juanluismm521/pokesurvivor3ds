// app_cloud.cpp — la cuenta de pokesurvivor.com en la 3DS: entrar (usuario y
// contraseña con el teclado de la consola), sincronizar con un botón (y sola
// al arrancar y al acabar cada partida) y salir. Lo que llega de la nube se
// aplica fusionándolo otra vez con lo que haya cambiado mientras (como la web).
#include <3ds.h>

#include <cstdio>
#include <cstring>

#include "core/save_io.h"
#include "core/websave.h"
#include "platform/3ds/app.h"
#include "platform/3ds/app_ui.h"
#include "platform/3ds/audio3ds.h"
#include "platform/3ds/cloud3ds.h"

namespace ps {

namespace {
// Teclado de la consola. Devuelve false si se cancela.
bool ask(const char* hint, char* out, int cap, bool password) {
  SwkbdState kb;
  swkbdInit(&kb, SWKBD_TYPE_WESTERN, 2, cap - 1);
  swkbdSetHintText(&kb, hint);
  swkbdSetValidation(&kb, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
  if (password) swkbdSetPasswordMode(&kb, SWKBD_PASSWORD_HIDE_DELAY);
  out[0] = 0;
  return swkbdInputText(&kb, out, size_t(cap)) == SWKBD_BUTTON_CONFIRM && out[0];
}

void ago(int64_t then, char* out, int cap) {
  if (!then) { std::snprintf(out, size_t(cap), "nunca"); return; }
  const int64_t s = (now_ms() - then) / 1000;
  if (s < 60) std::snprintf(out, size_t(cap), "hace un momento");
  else if (s < 3600) std::snprintf(out, size_t(cap), "hace %d min", int(s / 60));
  else if (s < 86400) std::snprintf(out, size_t(cap), "hace %d h", int(s / 3600));
  else std::snprintf(out, size_t(cap), "hace %d días", int(s / 86400));
}
}  // namespace

void App::cloud_autostart() {
  if (autotest || !cloud3ds::account().on || cloud3ds::state() != cloud3ds::JOB_IDLE) return;
  cloud_t0_updated_ = s_->updated;
  cloud3ds::start_sync(save_full_json(*s_));
}

bool App::cloud_test(const char* user, const char* pass) {
  cloud_t0_updated_ = s_->updated;
  return cloud3ds::start_login(user, pass, save_full_json(*s_));
}

void App::cloud_tick() {
  const cloud3ds::JobState st = cloud3ds::state();
  if (st == cloud3ds::JOB_FAILED) {
    std::snprintf(cloud_msg_, sizeof(cloud_msg_), "%s", cloud3ds::error().c_str());
    cloud3ds::finish();
    cloud_done = -1;
    return;
  }
  if (st != cloud3ds::JOB_DONE) return;
  if (scene == SC_RUN && g_->state != ST_OVER) return;   // (en plena partida, se espera)
  Json merged = cloud3ds::result();
  // Si has cambiado algo mientras (una compra...), se junta otra vez.
  if (s_->updated != cloud_t0_updated_) {
    Json mine = save_full_json(*s_);
    const int64_t now = now_ms() + cloud3ds::account().delta;
    mine = web::migrate(mine, "", now);
    web::fold(mine, cloud3ds::account().dev);
    merged = web::merge(mine, merged, now);
  }
  if (save_from_json(*s_, merged.dump())) {
    save_raw() = merged;
    save_write_file(dir_, *s_);
    g_->dirty = false;
    int owned = 0;
    for (int d = 1; d <= kMaxDex; ++d) if (s_->owned_at[d]) owned++;
    std::snprintf(cloud_msg_, sizeof(cloud_msg_), "Sincronizado: %d monedas · %d Pokémon", int(s_->coins), owned);
    if (!s_->partner || !s_->owns(s_->partner)) s_->partner = 0;
    cloud_done = 1;
  } else {
    std::snprintf(cloud_msg_, sizeof(cloud_msg_), "La partida de la nube no se ha podido leer");
    cloud_done = -1;
  }
  cloud3ds::finish();
}

void App::in_account(const UiIn& in) {
  const cloud3ds::Account& a = cloud3ds::account();
  const bool busy = cloud3ds::state() == cloud3ds::JOB_BUSY;
  const int n = a.on ? 2 : 1;
  if (in.rep & KEY_DUP) cur_ = (cur_ + n - 1) % n;
  if (in.rep & KEY_DDOWN) cur_ = (cur_ + 1) % n;
  int pick = -1;
  for (int i = 0; i < n; ++i) if (in_rect(&in, 20, 132 + i * 40, 280, 34)) pick = i;
  if (in.down & KEY_A) pick = cur_;
  if (in.down & KEY_B) { go(SC_SETTINGS); cur_ = 8; return; }
  if (pick < 0 || busy) return;
  cur_ = pick;
  if (!a.on) {
    char user[48], pass[64];
    if (!ask("Usuario de pokesurvivor.com", user, 17, false)) return;
    if (!ask("Contraseña", pass, 64, true)) return;
    cloud_t0_updated_ = s_->updated;
    cloud_msg_[0] = 0;
    cloud3ds::start_login(user, pass, save_full_json(*s_));
    std::memset(pass, 0, sizeof(pass));
  } else if (pick == 0) {
    cloud_msg_[0] = 0;
    cloud_t0_updated_ = s_->updated;
    cloud3ds::start_sync(save_full_json(*s_));
  } else {
    cloud3ds::logout();
    std::snprintf(cloud_msg_, sizeof(cloud_msg_), "Has cerrado la sesión (la partida se queda en la consola)");
    cur_ = 0;
  }
}

void App::bottom_account() {
  header("Cuenta de pokesurvivor.com");
  const cloud3ds::Account& a = cloud3ds::account();
  const bool busy = cloud3ds::state() == cloud3ds::JOB_BUSY;
  char b[128];
  if (!a.on) {
    r_->text_wrap("Entra con tu cuenta de pokesurvivor.com para tener la MISMA partida en la web y en la 3DS: Pokémon, shinies, monedas, tickets y mejoras. Nunca se pierde nada: se juntan las dos.", 160, 30, 0.36f, C_TEXT, 290, 1);
    r_->text_wrap("Sirven las cuentas de usuario y contraseña (las de Google no).", 160, 98, 0.34f, C_MUTED, 290, 1);
    button(20, 132, 280, 34, busy ? "Entrando…" : "Entrar", cur_ == 0);
  } else {
    std::snprintf(b, sizeof(b), "Conectado como %s", a.name.c_str());
    r_->text(b, 160, 36, 0.5f, C_GOLD, 1);
    char t[32];
    ago(a.last_sync, t, sizeof(t));
    std::snprintf(b, sizeof(b), "Última sincronización: %s", t);
    r_->text(b, 160, 66, 0.36f, C_MUTED, 1);
    r_->text_wrap("Se sincroniza sola al encender y al acabar cada partida. En la web, recarga la página para ver lo de la 3DS.", 160, 86, 0.34f, C_MUTED, 290, 1);
    button(20, 132, 280, 34, busy ? "Sincronizando…" : "Sincronizar ahora", cur_ == 0);
    button(20, 172, 280, 34, "Cerrar sesión", cur_ == 1);
  }
  if (busy) {
    const int dots = int(osGetTime() / 300) % 4;
    std::snprintf(b, sizeof(b), "Conectando con pokesurvivor.com%.*s", dots, "...");
    r_->text(b, 160, 212, 0.34f, C_GOLD, 1);
  } else if (cloud_msg_[0]) {
    r_->text(cloud_msg_, 160, 212, 0.34f, cloud_done < 0 ? C2D_Color32(255, 138, 150, 255) : C2D_Color32(126, 224, 122, 255), 1);
  }
  r_->text("A: elegir · B: volver", 160, 228, 0.32f, C_MUTED, 1);
}

}  // namespace ps

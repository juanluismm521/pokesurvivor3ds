// app_update.cpp — pantalla «Actualizaciones»: la versión que tienes, la
// publicada, buscar/descargar, instalar ya (cierra el juego) y volver a la
// versión anterior. La lógica está en prog3ds y core/update.
#include <3ds.h>

#include <cstdio>
#include <string>

#include "core/i18n.h"
#include "core/settings.h"
#include "core/version.h"
#include "gen/build.h"
#include "platform/3ds/app.h"
#include "platform/3ds/app_ui.h"
#include "platform/3ds/datos3ds.h"
#include "platform/3ds/prog3ds.h"

namespace ps {

namespace {
constexpr int kRows = 3;
constexpr float kRowY = 128, kRowStep = 24;   // (3 filas hasta y=200: debajo cabe el aviso)
}  // namespace

void App::in_update(const UiIn& in) {
  if (in.rep & KEY_DUP) cur_ = (cur_ + kRows - 1) % kRows;
  if (in.rep & KEY_DDOWN) cur_ = (cur_ + 1) % kRows;
  int pick = -1;
  for (int i = 0; i < kRows; ++i) if (in_rect(&in, 10, kRowY + i * kRowStep, 300, kRowStep - 3)) pick = i;
  if (in.down & KEY_A) pick = cur_;
  if (in.down & KEY_B) {   // (vuelve a donde estabas: el menú o Ajustes)
    if (update_back_ == SC_MENU) go(SC_MENU);
    else { go(SC_SETTINGS); cur_ = 9; }
    return;
  }
  if (pick < 0) return;
  cur_ = pick;
  const bool busy = prog3ds::state() == prog3ds::ST_CHECKING || prog3ds::state() == prog3ds::ST_DOWNLOADING;
  switch (pick) {
    case 0:
      if (!busy) prog3ds::start_check(true);   // buscar y, si hay, descargar
      break;
    case 1:
      if (prog3ds::pending()) quit = true;     // al salir se instala (main)
      break;
    case 2:
      if (!busy) prog3ds::start_rollback();
      break;
  }
}

void App::bottom_update() {
  header("Actualizaciones");
  char b[160];
  std::snprintf(b, sizeof(b), "Tienes la versión %s · build %d (%s)%s", kVersion, PS_BUILD, PS_COMMIT, prog3ds::is_cia() ? " · .cia" : " · .3dsx");
  r_->text(b, 160, 28, 0.34f, C_TEXT, 1);
  const UpdateEntry& e = prog3ds::latest();
  if (e.ok) {
    std::snprintf(b, sizeof(b), "Publicada: versión %s · build %d (%s)", e.version.c_str(), e.build, e.fecha.c_str());
    r_->text(b, 160, 44, 0.34f, e.build == PS_BUILD ? C_MUTED : C_GOLD, 1);
  }
  if (!prog3ds::supported()) {
    r_->text_wrap("Abre el juego desde el Homebrew Launcher (o instala el .cia) para que pueda actualizarse solo.", 160, 64, 0.32f, C_MUTED, 290, 1);
  } else {
    const std::string st = prog3ds::status();
    r_->text_wrap(st.c_str(), 160, 62, 0.34f, prog3ds::state() == prog3ds::ST_FAILED ? C2D_Color32(255, 138, 150, 255) : C_TEXT, 290, 1);
    if (prog3ds::state() == prog3ds::ST_DOWNLOADING) {
      const float k = prog3ds::pct() / 100.0f;
      C2D_DrawRectSolid(30, 96, 0, 260, 8, C2D_Color32(0, 0, 0, 160));
      C2D_DrawRectSolid(30, 96, 0, 260 * k, 8, C_GOLD);
      std::snprintf(b, sizeof(b), "%d %%", prog3ds::pct());
      r_->text(b, 160, 104, 0.3f, C_MUTED, 1);
    }
  }
  const bool busy = prog3ds::state() == prog3ds::ST_CHECKING || prog3ds::state() == prog3ds::ST_DOWNLOADING;
  const bool newer = e.ok && e.build != PS_BUILD && prog3ds::state() != prog3ds::ST_READY;
  const char* labels[kRows] = {busy ? "Buscando…" : newer ? "Descargar la versión nueva" : "Buscar ahora",
                               prog3ds::pending() ? "Instalar ahora (cierra el juego)" : "Instalar ahora (no hay nada listo)",
                               "Volver a la versión anterior"};
  for (int i = 0; i < kRows; ++i) {
    const float y = kRowY + i * kRowStep;
    const bool off = (i == 1 && !prog3ds::pending()) || (i == 2 && !prog3ds::can_rollback()) || !prog3ds::supported();
    C2D_DrawRectSolid(10, y, 0, 300, kRowStep - 3, i == cur_ ? C_SEL : C_PANEL);
    if (i == cur_) C2D_DrawRectSolid(10, y, 0, 3, kRowStep - 3, C_GOLD);
    r_->text(labels[i], 160, y + 1, 0.38f, off ? C_MUTED : C_TEXT, 1);
  }
  r_->text_wrap("La versión nueva del juego (con sus datos) se busca y se baja sola al encender; se instala al salir.", 160, 206, 0.3f, C_MUTED, 290, 1);
  r_->text("A: elegir · B: volver", 160, 226, 0.32f, C_MUTED, 1);
}

}  // namespace ps

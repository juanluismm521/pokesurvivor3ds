// progress.h — misiones, logros y estadísticas de la cuenta (progress.js).
// Misiones: 3 diarias + 2 semanales, sorteadas con una semilla de la fecha y de
// la partida guardada (mismo algoritmo que la web). La fecha es la del reloj de
// la consola. El premio se recoge a mano; los logros se dan solos.
#pragma once
#include <cstdint>

#include "core/gamedata.h"
#include "core/save.h"

namespace ps {

struct ProgEvent {
  int ev = EV_KILL;
  const int8_t* types = nullptr;   // kill
  bool boss = false;
  int legend = 0;                  // kill: dex del legendario vencido
  int id = -1;                     // item / evolve (movimiento) / rift (tipo)
  int level = 0, coins = 0;        // runEnd
  float time = 0;                  // runEnd / judgeEnd
  bool group = false;
  bool daily = false;              // missionDone
  int bosses = 0;                  // judgeEnd
};

// Avisos para la interfaz ("Misión completada", "¡Logro!").
struct Notice { char title[96]; char sub[64]; int16_t ach; };
struct Notices {
  Notice q[8];
  int n = 0;
  void push(const char* title, const char* sub, int ach);
  void pop();
};
extern Notices g_notices;

// Fecha de hoy (consola) → claves de día y semana como la web: d20261018, w2026-42.
void period_ids(int64_t now_ms, char* day, int nd, char* week, int nw);

void progress_ensure(Save& s, int64_t now_ms);
void progress_ensure_ids(Save& s, const char* day, const char* week);   // (con las claves ya hechas)
void progress_emit(Save& s, const ProgEvent& e, int64_t now_ms, bool check_ach = true);
bool progress_claim(Save& s, int i, bool weekly, int64_t now_ms);
bool progress_reroll(Save& s, int i, int64_t now_ms);
void progress_check_ach(Save& s, int64_t now_ms);
int progress_pending(Save& s, int64_t now_ms);
int ach_value(const Save& s, int ach);
// Texto de una misión ("Derrota a 300 Pokémon").
void mission_text(const Mission& m, char* out, int n);
inline bool mission_done(const Mission& m) { return m.prog >= m.goal; }
void grant_reward(Save& s, const Reward& r);

}  // namespace ps

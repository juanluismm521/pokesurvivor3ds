// save.h — la partida guardada, con el mismo esquema que el guardado local de
// la web (db.js: newSave). En la SD se guarda como JSON (save_json.cpp) para
// que algún día se pueda vincular con la cuenta web.
#pragma once
#include <cstdint>

#include "core/gamedata.h"

namespace ps {

constexpr int kSaveVersion = 3;
constexpr int kStartCoins = 500;   // START_COINS de db.js

enum From : int8_t { FROM_NONE = 0, FROM_STARTER, FROM_GACHA, FROM_WILD, FROM_RESCUE, FROM_OTHER };

struct Mission {
  int8_t def = -1;          // índice en kMissions
  int32_t goal = 0;
  int8_t param = T_NONE;    // tipo (misiones con tipo)
  int32_t prog = 0;
  bool claimed = false, told = false;
};

struct SaveStats {
  int32_t runs = 0, best_time = 0, best_level = 0, total_kills = 0, pulls = 0, coins_earned = 0, shinies = 0;
  int32_t bosses_total = 0, rift_wins = 0, evolutions = 0, missions_done = 0, group_runs = 0, best_group_time = 0;
  int32_t judged = 0, judge_time = 0, judge_bosses = 0;
};

struct Save {
  int v = kSaveVersion;
  char name[32] = "Jugador";
  int64_t created = 0, updated = 0;      // ms desde 1970 (como Date.now())
  int32_t coins = kStartCoins;
  // Colección: fecha (0 = no lo tiene) y de dónde vino.
  int64_t owned_at[kMaxDex + 1] = {};
  int8_t owned_from[kMaxDex + 1] = {};
  int64_t shiny_at[kMaxDex + 1] = {};
  int8_t shiny_from[kMaxDex + 1] = {};
  int16_t partner = 0;
  bool partner_shiny = false;
  int16_t starter = 0;
  char trait[16] = "";
  int16_t trait_scores[32] = {};         // por rasgo (kTraits)
  uint8_t pupg[kMaxDex + 1][UP_COUNT] = {};   // mejoras de cada Pokémon
  int8_t pbind[kMaxDex + 1][4];          // Atajos: movimiento por hueco (-1 nada)
  int8_t pban[kMaxDex + 1][3];           // Veto
  int8_t pstart[kMaxDex + 1];            // Movimiento inicial elegido (-1: el de serie)
  int8_t pmove2[kMaxDex + 1];            // segundo movimiento con «Repertorio» (del port: "pmove2"; -1: al azar)
  uint8_t pmega[kMaxDex + 1] = {};       // mejora «Megapiedra» por Pokémon (del port: "pmega")
  uint8_t pfour[kMaxDex + 1] = {};       // mejora «Más pasivas a niveles altos» por Pokémon (del port: "pfour"; 0 o 1)
  int64_t arceus_win[kMaxDex + 1] = {};  // medalla: cuándo derrotó a Arceus (modo rápido) cada Pokémon (del port: "arceusWin"; 0 nunca)
  int arceus_medals() const { int n = 0; for (int64_t t : arceus_win) n += t > 0; return n; }
  // Regalos firmados ya recibidos (core/gift.h; del port: "regalos" {id: fecha}): cada
  // uno se da una sola vez, también entre consolas (viaja con la partida).
  static constexpr int kMaxGifts = 16;
  char gift_id[kMaxGifts][32] = {};
  int64_t gift_at[kMaxGifts] = {};
  int ngifts = 0;
  int32_t pused[kMaxDex + 1] = {};       // partidas jugadas con cada Pokémon
  int32_t t1 = 0, t10 = 0;               // tickets
  int32_t pity[11] = {};                 // por banner: 0 «Todas» (del port), 1-9 regiones, 10 Noche Espectral (Halloween)
  // Halloween (web, 25-10-2026): caramelos (bag.candy), disfraces comprados
  // (skins {id: fecha}) y el que lleva puesto cada Pokémon (pskin {dex: [id, fecha]}).
  int32_t candy = 0;
  bool candy_known = false;              // (la partida tiene o ha tenido caramelos: entonces se escribe bag.candy, también el 0)
  int64_t skin_at[8] = {};               // por índice de kSkins (0 = no lo tiene)
  int8_t pskin[kMaxDex + 1];             // disfraz puesto (índice de kSkins; -1 ninguno)
  int64_t pskin_at[kMaxDex + 1] = {};    // cuándo se puso o quitó (gana el más reciente al juntar)
  int64_t legends[kMaxDex + 1] = {};
  int64_t arenas[T_COUNT] = {};
  int64_t items_found[IT_COUNT] = {};
  int64_t evo_moves[kMaxMoves] = {};
  int64_t ach[256] = {};
  // Misiones: día y semana (claves de fecha) y las sorteadas.
  char m_day[16] = "", m_week[16] = "";
  bool m_rerolled = false;
  Mission daily[3];
  Mission weekly[2];
  int16_t favs[3] = {};
  char title[48] = "";
  SaveStats stats;

  Save();
  void reset();   // vuelve a una partida vacía (sin crear otra en la pila: ocupa ~50 KB)
  bool owns(int dex) const { return dex >= 0 && dex <= kMaxDex && owned_at[dex] != 0; }
  bool owns_shiny(int dex) const { return dex >= 0 && dex <= kMaxDex && shiny_at[dex] != 0; }
  int owned_count() const;
  int shiny_count() const;
  int legends_count() const;
};

// Da un Pokémon. true si es nuevo.
bool save_grant_dex(Save& s, int dex, int from, int64_t now);
// Da la versión shiny (y la normal, si no la tenía). true si es nueva.
bool save_grant_shiny_dex(Save& s, int dex, int from, int64_t now);

}  // namespace ps

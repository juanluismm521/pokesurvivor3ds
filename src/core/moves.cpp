// moves.cpp
#include "core/moves.h"

namespace ps {

namespace {
void apply_mods(MoveInst& s, const Ladder& L) {
  for (int i = 0; i < L.n; ++i) {
    const int k = L.m[i].stat;
    const float val = L.m[i].val;
    switch (kMoveStatOp[k]) {
      case 1:  // MULT: sólo si existe
        if (s.has_(k)) s.v[k] *= val;
        break;
      case 2:  // ADD: (s[k] || 0) + m
        s.v[k] = (s.has_(k) ? s.v[k] : 0.0f) + val;
        s.has |= 1u << k;
        break;
      default:  // se asigna
        s.v[k] = val;
        s.has |= 1u << k;
        break;
    }
  }
}
}  // namespace

const char* MoveInst::name() const { return evolved ? kMoves[def].evo_name : kMoves[def].name; }

MoveInst move_instance(int def, int lvl) {
  MoveInst s;
  s.def = int16_t(def);
  s.lvl = int8_t(lvl);
  s.has = kMoves[def].has;
  for (int i = 0; i < MS_NUM; ++i) s.v[i] = kMoves[def].v[i];
  for (int i = 0; i < lvl - 1; ++i) apply_mods(s, kLadder[kMoves[def].kind][i]);
  return s;
}

bool move_level_up(MoveInst& s) {
  if (s.lvl >= move_max_lvl()) return false;
  apply_mods(s, kLadder[s.kind()][s.lvl - 1]);
  s.lvl++;
  return true;
}

const char* move_next_up_text(const MoveInst& s) {
  return s.lvl >= move_max_lvl() ? nullptr : kLadder[s.kind()][s.lvl - 1].text;
}

bool move_evo_info(const MoveInst& m, const uint8_t stats_taken[LS_COUNT], EvoInfo& out) {
  const MoveDef& d = m.d();
  if (d.evo_need < 0 || m.evolved || m.lvl < move_max_lvl() || !kEvoMods[d.kind].text) return false;
  out.name = d.evo_name;
  out.need = d.evo_need;
  out.need_name = kNeedName[d.evo_need];
  out.text = kEvoMods[d.kind].text;
  out.ready = stats_taken && stats_taken[d.evo_need] > 0;
  return true;
}

bool move_evolve(MoveInst& m) {
  const MoveDef& d = m.d();
  if (d.evo_need < 0 || m.evolved) return false;
  apply_mods(m, kEvoMods[d.kind]);
  m.evolved = true;
  return true;
}

int move_pool_for(const int8_t types[2], int16_t* out, int max) {
  int n = 0;
  auto add_type = [&](int t) {
    for (int i = 0; i < kMoveCount; ++i) {
      if (kMoves[i].type != t) continue;
      bool dup = false;
      for (int j = 0; j < n; ++j) if (out[j] == i) { dup = true; break; }
      if (!dup && n < max) out[n++] = int16_t(i);
    }
  };
  add_type(T_NORMAL);
  for (int k = 0; k < 2; ++k) if (types[k] >= 0) add_type(types[k]);
  return n;
}

int move_start_for(int dex) {
  for (int i = 0; i < kStarterCount; ++i) if (kStarters[i].dex == dex) return kStarters[i].start;
  const Mon* m = mon_by_dex(dex);
  if (!m) return kStartMove[T_NORMAL];
  for (int k = 0; k < 2; ++k) if (m->types[k] >= 0) return kStartMove[m->types[k]];
  return kStartMove[T_NORMAL];
}

}  // namespace ps

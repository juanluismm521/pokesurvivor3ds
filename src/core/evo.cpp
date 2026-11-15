// evo.cpp — ver evo.h. (_dc6cc1a de la web: begin, due, step, choose)
#include "core/evo.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/anim.h"
#include "core/game.h"
#include "core/mons.h"

namespace ps {

int evo_next(int dex, int16_t* out, int max) {
  int n = 0;
  for (int i = 0; i < kEvoCount && n < max; ++i)
    if (kEvos[i].from == dex && mon_by_dex(kEvos[i].to)) out[n++] = kEvos[i].to;
  return n;
}
int evo_prev(int dex) {
  for (int i = 0; i < kEvoCount; ++i) if (kEvos[i].to == dex && mon_by_dex(kEvos[i].from)) return kEvos[i].from;
  return 0;
}
int evo_level_real(int from, int to) {
  for (int i = 0; i < kEvoCount; ++i) if (kEvos[i].from == from && kEvos[i].to == to) return kEvos[i].lvl;
  return 0;
}
int evo_level(int from, int to) {
  const int l = evo_level_real(from, to);
  return l > 0 ? std::max(2, int(std::lround(l * kEvoPace))) : 0;
}

namespace {
bool has_next(int dex) { int16_t t[1]; return evo_next(dex, t, 1) > 0; }

// Media de los stats de varios Pokémon (×mul a vida, ataques y defensas), con tu naturaleza.
void stats_of(const Player& p, const int16_t* list, int n, float mul, Mon& out, float& spd) {
  Mon m = *mon_by_dex(p.dex);
  double hp = 0, pa = 0, sa = 0, df = 0, sd = 0, sp = 0;
  for (int i = 0; i < n; ++i) {
    const Mon& q = *mon_by_dex(list[i]);
    hp += q.hp; pa += q.pa; sa += q.sa; df += q.df; sd += q.sd; sp += q.spd;
  }
  m.hp = int16_t(std::lround(hp / n * mul));
  m.pa = float(pa / n * mul); m.sa = float(sa / n * mul);
  m.df = float(df / n * mul); m.sd = float(sd / n * mul);
  m.spd = int16_t(std::lround(sp / n));
  m.atk = m.pa > m.sa ? m.pa : m.sa;
  mon_with_nature(m, p.nature, out);
  spd = float(sp / n);
  if (p.nature >= 0) { const NatureDef& nd = kNatures[p.nature]; spd *= nd.up == NS_SPD ? 1.1f : nd.down == NS_SPD ? 0.9f : 1.0f; }
}

// Pone esos stats (la vida, en la misma proporción que tenías).
void apply(Player& p, const Mon& m, float spd) {
  const float frac = p.max_hp > 0 ? p.hp / p.max_hp : 1;
  p.max_hp = std::fmax(1.0f, p.max_hp + m.hp - p.mon_buf.hp);
  p.hp = std::fmax(1.0f, std::round(p.max_hp * frac));
  p.mon_buf = m;
  p.base_atk = m.atk;
  p.base_spd = spd;
}

void set_stats(Player& p, const int16_t* list, int n, float mul = 1) {
  Mon m; float spd;
  stats_of(p, list, n, mul, m, spd);
  apply(p, m, spd);
}

// A medias entre dos grupos (la primera de dos etapas de una línea corta).
void set_half(Player& p, const int16_t* a, int na, const int16_t* b, int nb) {
  Mon ma, mb; float sa, sb;
  stats_of(p, a, na, 1, ma, sa);
  stats_of(p, b, nb, 1, mb, sb);
  Mon m = ma;
  m.hp = int16_t(std::lround((ma.hp + mb.hp) / 2.0));
  m.pa = (ma.pa + mb.pa) / 2; m.sa = (ma.sa + mb.sa) / 2; m.df = (ma.df + mb.df) / 2; m.sd = (ma.sd + mb.sd) / 2;
  m.atk = m.pa > m.sa ? m.pa : m.sa;
  apply(p, m, (sa + sb) / 2);
}

// Los Pokémon de alrededor salen despedidos (y aturdidos un momento).
void push_away(Game& g, Player& p, bool big) {
  const float R = big ? 320 : 224;
  for (Enemy& e : g.enemies) {
    if (e.dead || e.arceus) continue;
    const float dx = e.x - p.x, dy = e.y - p.y, d = std::hypot(dx, dy);
    if (d > R || d < 1) continue;
    const float k = (0.35f + (1 - d / R) * 0.65f) * 750 * (big ? 1 : 0.6f) * (e.boss || e.legend ? 0.25f : 1);
    e.kx += dx / d * k; e.ky += dy / d * k;
    if (!e.boss && !e.legend) enemy_apply_stun(e, big ? 0.7f : 0.4f);
  }
}

void say_to(Game& g, const Player& p, const char* t, float s) { if (&p == &g.players[g.local]) g.say(t, s); }

void stronger(Game& g, Player& p) {
  push_away(g, p, false);
  g.sfx(SFX_LEVELUP);
  g.fx.ring(p.x, p.y, 8, 90, 0xffe14d, 0.5f, 4);
  char b[120];
  std::snprintf(b, sizeof(b), "¡%s se hace más fuerte!", mon_by_dex(p.dex)->name);
  say_to(g, p, b, 3);
}

// Cambia de aspecto (y de tipos) a `to`.
void become(Game& g, Player& p, int to) {
  const Mon& t = *mon_by_dex(to);
  p.dex = int16_t(to);
  p.mon_buf.dex = t.dex; p.mon_buf.name = t.name;
  p.mon_buf.types[0] = t.types[0]; p.mon_buf.types[1] = t.types[1];
  p.mon_buf.gen = t.gen; p.mon_buf.leg = t.leg; p.mon_buf.num = t.num; p.mon_buf.reg = t.reg;
  p.shiny = p.start_shiny && has_shiny(to);
  p.scale = player_scale(to);
  const int8_t dir = p.anim.dir;
  p.anim.init(to, p.scale, p.shiny);
  p.anim.dir = dir;
  p.r = player_radius(to);
  p.body_h = body_height(to, p.scale);
  p.skin = -1;   // (el disfraz era del de antes)
  push_away(g, p, true);
  g.flash(0xffffff, 0.3f);
  g.camera.kick(1.1f);
  g.sfx(SFX_LEGEND);
  g.fx.ring(p.x, p.y, 8, 140, 0xffffff, 0.7f, 6);
  g.fx.burst(p.x, p.y - p.body_h * 0.5f, 0xffe14d, 40, 240);
}

// Qué toca en la siguiente etapa. kind: 0 nada, 1 automática (stats de `list`), 2 elegir entre
// `list` (o mantener el aspecto), 3 «solo» (más fuerte, sin evolucionar), 4 automática a medias
// (half: entre lo de ahora y `list`).
struct Step { int kind = 0, lvl = 0, n = 0; int16_t list[8]; bool half = false; };

Step next_step(const Player& p) {
  Step s;
  const int k = p.evo_stage + 1;
  if (k > 2) return s;
  const int def = k == 1 ? kEvoLv1 : kEvoLv2;
  if (p.evo_solo) { s.kind = 3; s.lvl = def; return s; }
  // Tras una etapa «a medias», la siguiente completa la fuerza de lo elegido.
  if (p.evo_half) {
    s.kind = 1; s.n = p.evo_nstat;
    for (int i = 0; i < s.n; ++i) s.list[i] = p.evo_stat[i];
    s.lvl = std::max(kEvoLv2, p.evo_lv_prev + 6);
    return s;
  }
  if (k < p.evo_nline) {
    const int from = p.evo_line[k - 1], to = p.evo_line[k];
    const int lv = evo_level(from, to);
    s.lvl = lv > 0 ? lv : def;
    s.n = 1; s.list[0] = int16_t(to);
    // Línea de dos (Growlithe → Arcanine): a medias ahora y entera en la segunda etapa.
    s.kind = k == 1 && p.evo_nline == 2 && !has_next(to) ? 4 : 1;
    return s;
  }
  // Puede evolucionar: lo que viene después de los stats que tiene ahora.
  int lv = 0;
  for (int i = 0; i < p.evo_nstat; ++i) {
    int16_t nx[8];
    const int nn = evo_next(p.evo_stat[i], nx, 8);
    for (int j = 0; j < nn; ++j) {
      bool dup = false;
      for (int q = 0; q < s.n; ++q) dup = dup || s.list[q] == nx[j];
      if (!dup && s.n < 8) {
        s.list[s.n++] = nx[j];
        const int l = evo_level(p.evo_stat[i], nx[j]);
        if (l > 0 && (lv == 0 || l < lv)) lv = l;
      }
    }
  }
  if (!s.n) return s;
  s.kind = 2;
  s.lvl = lv > 0 ? lv : def;
  // La primera etapa, si después ya no hay más: a medias (y la segunda, entera).
  bool last = true;
  for (int i = 0; i < s.n; ++i) last = last && !has_next(s.list[i]);
  s.half = k == 1 && last;
  return s;
}
}  // namespace

void evo_begin(Game& g, Player& p) {
  (void)g;
  p.start_dex = p.dex;
  p.start_shiny = p.shiny;
  // Su línea hasta él (Charizard: Charmander, Charmeleon, Charizard)
  int16_t line[4]; int n = 0;
  line[n++] = p.dex;
  for (int d = evo_prev(p.dex); d && n < 4; d = evo_prev(d)) {
    for (int i = n; i > 0; --i) line[i] = line[i - 1];
    line[0] = int16_t(d); n++;
  }
  for (int i = 0; i < n; ++i) p.evo_line[i] = line[i];
  p.evo_nline = int8_t(n);
  p.evo_stage = 0;
  p.evo_half = false;
  p.evo_solo = n == 1 && !has_next(p.dex);
  p.evo_nstat = 1;
  if (n > 1) { p.evo_stat[0] = line[0]; set_stats(p, line, 1); }
  else { p.evo_stat[0] = p.dex; if (p.evo_solo) set_stats(p, p.evo_stat, 1, kEvoSolo[0]); }
}

int evo_due_level(const Player& p) {
  const Step s = next_step(p);
  return s.kind ? s.lvl : 0;
}

void evo_update(Game& g, Player& p) {
  if (p.dead || p.choice != CH_NONE || p.mega >= 0 || p.evo_stage >= 2) return;
  const Step s = next_step(p);
  if (!s.kind || p.level < s.lvl) return;
  const int k = p.evo_stage + 1;
  p.evo_lv_prev = int16_t(s.lvl);
  if (s.kind == 3) {
    p.evo_stage = int8_t(k);
    set_stats(p, p.evo_stat, 1, kEvoSolo[k]);
    stronger(g, p);
    return;
  }
  if (s.kind == 1 || s.kind == 4) {
    p.evo_stage = int8_t(k);
    if (s.kind == 4) { set_half(p, p.evo_stat, p.evo_nstat, s.list, s.n); p.evo_half = true; }
    else { set_stats(p, s.list, s.n); p.evo_half = false; }
    p.evo_nstat = int8_t(s.n);
    for (int i = 0; i < s.n; ++i) p.evo_stat[i] = s.list[i];
    stronger(g, p);
    return;
  }
  // Elegir: hasta 3 al azar entre los que puede ser (Eevee tiene 8) y «mantener su aspecto».
  int16_t pool[8]; int np = s.n;
  for (int i = 0; i < np; ++i) pool[i] = s.list[i];
  p.evo_nopts = 0;
  while (p.evo_nopts < 3 && np) {
    const int j = int(g.rng.next() * np);
    p.evo_opts[p.evo_nopts++] = pool[j];
    for (int q = j; q < np - 1; ++q) pool[q] = pool[q + 1];
    np--;
  }
  p.evo_nall = int8_t(s.n);
  for (int i = 0; i < s.n; ++i) p.evo_all[i] = s.list[i];
  p.evo_pending_half = s.half;
  p.choice = CH_EVO;
  if (g.state == ST_PLAYING) g.state = ST_CHOICE;
  g.sfx(SFX_LEVELUP);
}

void evo_choose(Game& g, Player& p, int pick) {
  if (p.choice != CH_EVO) return;
  p.evo_stage++;
  const bool half = p.evo_pending_half;
  int16_t to[8]; int n;
  if (pick >= 0 && pick < p.evo_nopts) { to[0] = p.evo_opts[pick]; n = 1; }
  else { n = p.evo_nall; for (int i = 0; i < n; ++i) to[i] = p.evo_all[i]; }
  char b[160];
  // Evolucionar: sus stats base pasan a ser los de su evolución (el dueño, 15-11-2026). Mantener
  // el aspecto: se queda como está (y en la siguiente etapa podrá evolucionar).
  const bool evolve = pick >= 0 && pick < p.evo_nopts;
  if (evolve) {
    if (half) set_half(p, p.evo_stat, p.evo_nstat, to, n);
    else set_stats(p, to, n);
    p.evo_half = half;
    p.evo_nstat = int8_t(n);
    for (int i = 0; i < n; ++i) p.evo_stat[i] = to[i];
  }
  if (evolve) {
    const char* from = mon_by_dex(p.dex)->name;
    std::snprintf(b, sizeof(b), "¡%s ha evolucionado en %s!", from, mon_by_dex(to[0])->name);
    become(g, p, to[0]);
    say_to(g, p, b, 3.4f);
  } else {
    push_away(g, p, false);
    g.sfx(SFX_LEVELUP);
    std::snprintf(b, sizeof(b), "%s mantiene su aspecto", mon_by_dex(p.dex)->name);
    say_to(g, p, b, 3.2f);
  }
  p.choice = CH_NONE;
  if (g.state == ST_CHOICE && !g.anyone_choosing()) g.state = ST_PLAYING;
}

}  // namespace ps

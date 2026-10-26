// mega.cpp — ver mega.h.
#include "core/mega.h"

#include <cmath>
#include <cstring>
#include <cstdio>

#include "core/anim.h"
#include "core/game.h"

namespace ps {

int mega_up_cost(int level) {
  static const int kCost[kMegaUpMax] = {400, 800, 1400, 2200, 3200};
  return level >= 0 && level < kMegaUpMax ? kCost[level] : 0;
}

int mega_index(int dex, int slot) {
  for (int i = 0; i < kMegaCount; ++i) if (kMegas[i].base == dex && kMegas[i].slot == slot) return i;
  return -1;
}
int mega_count(int dex) {
  int n = 0;
  for (int i = 0; i < kMegaCount; ++i) if (kMegas[i].base == dex) n++;
  return n;
}
float mega_chance(int up) { return kMegaChanceBase + kMegaChanceUp * float(up < 0 ? 0 : up > kMegaUpMax ? kMegaUpMax : up); }

const char* item_name(int id, int dex) {
  if (id >= 0 && id < IT_COUNT) return kItems[id].name;
  if (is_mega_stone(id)) {
    const int k = mega_index(dex, id - IT_MEGA_A);
    return k >= 0 ? kMegas[k].stone : "Megapiedra";
  }
  return "";
}

const char* item_desc(int id, int dex) {
  if (id >= 0 && id < IT_COUNT) return kItems[id].desc;
  if (is_mega_stone(id)) {
    static char b[160];
    const int k = mega_index(dex, id - IT_MEGA_A);
    const MegaBoost mb = mega_boost(k);
    std::snprintf(b, sizeof(b), "Y: %s %d s. +%d %% ataque, +%d %% velocidad, -%d %% recarga, regeneras y aguantas más",
                  k >= 0 ? kMegas[k].mon.name : "megaevolución", int(mb.time), int(std::lround((mb.atk - 1) * 100)),
                  int(std::lround((mb.spd - 1) * 100)), int(std::lround((1 - mb.cd) * 100)));
    return b;
  }
  return "";
}

const char* item_icon(int id) { return id >= 0 && id < IT_COUNT ? kItems[id].id : nullptr; }

int roll_item(Game& g) {
  // La mejor probabilidad de megapiedra de los que pueden tenerla. Si nadie
  // puede, ni se tira: así, sin megas, la partida es la misma que en la web.
  float best = 0;
  for (int i = 0; i < g.nplayers; ++i) {
    const Player& p = g.players[i];
    if (p.active && has_mega(p.dex)) best = std::fmax(best, mega_chance(p.mega_up));
  }
  if (best > 0 && g.rng.next() < best) return g.rng.next() < 0.5f ? IT_MEGA_A : IT_MEGA_B;
  return g.rng.range_int(0, IT_COUNT - 1);
}

int mega_stone_for(Game& g, const Player& p, int id) {
  if (!is_mega_stone(id)) return id;
  const int n = mega_count(p.dex);
  if (n == 0) return g.rng.range_int(0, IT_COUNT - 1);   // (no tiene mega: otro objeto)
  if (id == IT_MEGA_B && n < 2) return IT_MEGA_A;
  return id;
}

bool mega_ready(const Player& p) {
  return !p.dead && p.mega < 0 && p.mega_cd <= 0 && is_mega_stone(p.item) && mega_index(p.dex, p.item - IT_MEGA_A) >= 0;
}

static void set_form(Player& p, const Mon* m, int sprite, float scale) {
  p.mon = m;
  p.base_spd = m->spd;
  p.base_atk = m->atk;
  const int dir = p.anim.dir;
  p.anim.init(sprite, scale, p.shiny);   // (init sólo lo pone shiny si ese sprite lo tiene)
  p.anim.dir = int8_t(dir);
  p.body_h = body_height(sprite, scale);
}

bool mega_is_z(int k) {
  if (k < 0 || k >= kMegaCount) return false;
  const char* n = kMegas[k].mon.name;
  const size_t l = std::strlen(n);
  return l > 2 && n[l - 2] == ' ' && n[l - 1] == 'Z';
}

MegaBoost mega_boost(int k) {
  if (mega_is_z(k)) return MegaBoost{kMegaZTime, kMegaZAtk, kMegaZSpd, kMegaZCd, kMegaZRegen, kMegaZArmor};
  return MegaBoost{kMegaTime, kMegaAtk, kMegaSpd, kMegaCd, kMegaRegen, kMegaArmor};
}

void mega_start(Game& g, Player& p) {
  if (!mega_ready(p) || (g.state != ST_PLAYING && g.state != ST_CHOICE)) return;
  const int k = mega_index(p.dex, p.item - IT_MEGA_A);
  const MegaDef& d = kMegas[k];
  const MegaBoost mb = mega_boost(k);
  p.mega = int8_t(k);
  p.mega_t = mb.time;
  p.mega_tf = kMegaTransform;
  p.mega_prev = p.mon;
  // Su vida máxima (la diferencia; la vida sube lo mismo).
  p.mega_dhp = float(d.mon.hp - p.mon->hp);
  p.max_hp += p.mega_dhp;
  p.hp = std::fmin(p.max_hp, p.hp + std::fmax(0.0f, p.mega_dhp));
  // Su sprite de mega (shiny si eres shiny y lo hay). Sin sprite propio, o si
  // eres shiny y su mega aún no tiene shiny: el tuyo, algo más grande y con
  // su aura al dibujarlo (el shiny nunca se pierde).
  const bool own = d.own_sprite && (!p.shiny || has_shiny(d.sprite));
  set_form(p, &d.mon, own ? d.sprite : p.dex, own ? p.scale : p.scale * 1.15f);
  // El empujón de la mega (se deshace igual al acabar).
  p.atk_mul *= mb.atk;
  p.spd_mul *= mb.spd;
  p.cd_mul *= mb.cd;
  p.mega_regen = p.max_hp * mb.regen;
  p.regen_flat += p.mega_regen;
  p.dmg_reduce += mb.armor;
  p.invuln = std::fmax(p.invuln, kMegaTransform);
  p.flash = 0.3f;
  // La animación: anillos de colores que se cierran y una explosión de luz.
  static const uint32_t C[6] = {0xff5f6d, 0xffd23f, 0x5fe08a, 0x5fc8ff, 0xc47bff, 0xffffff};
  for (int i = 0; i < 6; ++i) g.fx.ring(p.x, p.y - p.body_h * 0.5f, 120 - i * 14, 8, C[i], 0.5f + i * 0.08f, 4);
  g.fx.burst(p.x, p.y - p.body_h * 0.5f, 0xffffff, 24, 200);
  g.camera.kick(0.5f);
  g.sfx(SFX_SURGE);
  if (&p == &g.players[g.local]) {
    char b[64];
    std::snprintf(b, sizeof(b), "¡%s!", d.mon.name);
    g.say(b, 2.4f);
  }
}

void mega_end(Game& g, Player& p) {
  if (p.mega < 0) return;
  const Mon* base = p.mega_prev ? p.mega_prev : mon_by_dex(p.dex);
  const MegaBoost mb = mega_boost(p.mega);
  p.max_hp -= p.mega_dhp;
  if (p.hp > p.max_hp) p.hp = p.max_hp;
  if (!p.dead && p.hp < 1) p.hp = 1;   // (acabar la mega nunca te deja KO)
  set_form(p, base, p.dex, player_scale(p.dex));
  p.atk_mul /= mb.atk;
  p.spd_mul /= mb.spd;
  p.cd_mul /= mb.cd;
  p.regen_flat -= p.mega_regen;
  p.mega_regen = 0;
  p.dmg_reduce -= mb.armor;
  p.mega = -1;
  p.mega_t = p.mega_tf = 0;
  p.mega_dhp = 0;
  p.mega_cd = kMegaCooldown;
  g.fx.burst(p.x, p.y - p.body_h * 0.5f, 0xc47bff, 12, 120);
}

void mega_update(Game& g, Player& p, float dt) {
  if (p.mega >= 0) {
    if (p.mega_tf > 0) {
      p.mega_tf -= dt;
      // (destellos durante la transformación)
      if (int((p.mega_tf + dt) * 10) != int(p.mega_tf * 10)) g.fx.burst(p.x, p.y - p.body_h * 0.5f, 0xffd23f, 3, 90);
    }
    p.mega_t -= dt;
    if (p.mega_t <= 0 || p.dead) mega_end(g, p);
  } else if (p.mega_cd > 0) {
    p.mega_cd -= dt;
  }
}

}  // namespace ps

// riftboss.cpp — los legendarios de la grieta pelean de verdad (RiftBoss de la web, Gran
// Actualización 13-11-2026). En vez de la IA de jefe de siempre:
//   - Llega del cielo y, en la arena, alterna «quieto» y ataques de su KIT: hasta 6, según sus
//     tipos (5 por tipo: Fuego, Nitrocarga, Ola-Lava, Llamarada, Giro Fuego, Lanzallamas…) y,
//     los legendarios más famosos, el suyo (Mewtwo, Onda Mental; Lugia, Aerochorro…).
//   - Tres fases: a 2/3 de vida «saca todo su poder» y a 1/3 «se enfurece» (se queda un momento
//     quieto e inmune, con onda): más ataques del kit, más rápido y más fuertes.
//   - En la fase 3, su ATAQUE DEFINITIVO: carga en el centro 4,6 s y arrasa la arena: sólo te
//     salvas escondido detrás de una columna, un árbol o una roca. Luego se queda agotado.
// Los ataques son peligros con aviso (core/hazards), disparos y amenazas propias que se mueven
// por la arena (anillos con huecos, muros con una puerta, torbellinos).
#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/game.h"

namespace ps {

namespace {
constexpr float TS = kTS;
constexpr float ULT_T = 4.6f;
enum Pat : int8_t { P_CHARGE, P_RING, P_SWEEP, P_LEAP, P_SPIRAL, P_WALL, P_METEOR, P_TORNADO, P_ORBS, P_SLASHES,
                    P_PINWHEEL, P_BLOOM, P_BLINK, P_GRAVITY, P_CLONES, P_COUNT };
using KitMove = Game::RiftBossState::KM;
// Por tipo (orden de los tipos), como la web
const KitMove kKit[T_COUNT][5] = {
    {{P_CHARGE, "Velocidad Extrema"}, {P_RING, "Vozarrón"}, {P_SWEEP, "Hiperrayo"}, {P_LEAP, "Golpe Cuerpo"}, {P_SPIRAL, "Giro Rápido"}},
    {{P_CHARGE, "Nitrocarga"}, {P_WALL, "Ola-Lava"}, {P_METEOR, "Llamarada"}, {P_TORNADO, "Giro Fuego"}, {P_SWEEP, "Lanzallamas"}},
    {{P_WALL, "Surf"}, {P_TORNADO, "Torbellino"}, {P_SWEEP, "Hidrobomba"}, {P_SPIRAL, "Rayo Burbuja"}, {P_ORBS, "Hidropulso"}},
    {{P_TORNADO, "Tormenta Floral"}, {P_SLASHES, "Hoja Afilada"}, {P_SWEEP, "Rayo Solar"}, {P_ORBS, "Bomba Germen"}, {P_SPIRAL, "Truco Floral"}},
    {{P_PINWHEEL, "Onda Trueno"}, {P_SWEEP, "Rayo"}, {P_CHARGE, "Placaje Eléctrico"}, {P_ORBS, "Bola Voltio"}, {P_METEOR, "Trueno"}},
    {{P_WALL, "Mundo Gélido"}, {P_METEOR, "Granizo"}, {P_BLOOM, "Liofilización"}, {P_SWEEP, "Rayo Hielo"}, {P_RING, "Ventisca"}},
    {{P_CHARGE, "Asalto Estelar"}, {P_LEAP, "Puño Dinámico"}, {P_SLASHES, "Patada Giro"}, {P_RING, "Onda Vacío"}, {P_ORBS, "Esfera Aural"}},
    {{P_ORBS, "Bomba Lodo"}, {P_SPIRAL, "Residuos"}, {P_TORNADO, "Giro Mortífero"}, {P_GRAVITY, "Tóxico"}, {P_WALL, "Púas Tóxicas"}},
    {{P_RING, "Terremoto"}, {P_LEAP, "Magnitud"}, {P_METEOR, "Terratemblor"}, {P_WALL, "Púas"}, {P_TORNADO, "Bucle Arena"}},
    {{P_TORNADO, "Tornado"}, {P_SLASHES, "Tajo Aéreo"}, {P_CHARGE, "Acróbata"}, {P_WALL, "Vendaval"}, {P_SWEEP, "Golpe Aéreo"}},
    {{P_BLOOM, "Psíquico"}, {P_BLINK, "Psicocarga"}, {P_SWEEP, "Psicorrayo"}, {P_ORBS, "Bola Neblina"}, {P_GRAVITY, "Paranormal"}},
    {{P_SPIRAL, "Zumbido"}, {P_SLASHES, "Tijera X"}, {P_CHARGE, "Megacuerno"}, {P_ORBS, "Rayo Señal"}, {P_TORNADO, "Viento Plata"}},
    {{P_METEOR, "Avalancha"}, {P_RING, "Roca Afilada"}, {P_WALL, "Tumba Rocas"}, {P_CHARGE, "Hachazo Pétreo"}, {P_SPIRAL, "Poder Pasado"}},
    {{P_CLONES, "Sombra Vil"}, {P_BLINK, "Golpe Umbrío"}, {P_ORBS, "Bola Sombra"}, {P_GRAVITY, "Infortunio"}, {P_BLOOM, "Viento Aciago"}},
    {{P_SWEEP, "Pulso Dragón"}, {P_CHARGE, "Enfado"}, {P_TORNADO, "Dragoaliento"}, {P_METEOR, "Furia Dragón"}, {P_SLASHES, "Garra Dragón"}},
    {{P_CLONES, "Golpe Bajo"}, {P_SLASHES, "Tajo Umbrío"}, {P_GRAVITY, "Tinieblas"}, {P_BLINK, "Triturar"}, {P_BLOOM, "Pulso Umbrío"}},
    {{P_PINWHEEL, "Giro Bola"}, {P_SWEEP, "Foco Resplandor"}, {P_CHARGE, "Meteoimpacto"}, {P_SLASHES, "Garra Metal"}, {P_METEOR, "Puño Meteoro"}},
    {{P_BLOOM, "Brillo Mágico"}, {P_SPIRAL, "Viento Feérico"}, {P_ORBS, "Fuerza Lunar"}, {P_SWEEP, "Carantoña"}, {P_BLINK, "Voz Cautivadora"}},
};
struct Sig { int16_t dex; KitMove m; };
const Sig kSig[] = {
    {150, {P_BLOOM, "Onda Mental"}}, {249, {P_SWEEP, "Aerochorro"}}, {250, {P_METEOR, "Fuego Sagrado"}}, {382, {P_WALL, "Pulso Primigenio"}},
    {383, {P_RING, "Filo del Abismo"}}, {384, {P_CHARGE, "Ascenso Draco"}}, {483, {P_PINWHEEL, "Distorsión"}}, {487, {P_CLONES, "Sombra Vil"}},
    {491, {P_GRAVITY, "Brecha Negra"}}, {492, {P_RING, "Fulgor Semilla"}}, {493, {P_METEOR, "Sentencia"}}, {643, {P_SWEEP, "Llama Azul"}},
    {647, {P_SLASHES, "Sable Místico"}}, {648, {P_RING, "Canto Arcaico"}}, {716, {P_SPIRAL, "Geocontrol"}}, {718, {P_WALL, "Mil Flechas"}},
    {719, {P_METEOR, "Tormenta de Diamantes"}}, {800, {P_SWEEP, "Géiser Fotónico"}}, {802, {P_CLONES, "Puntada Sombría"}}, {890, {P_SWEEP, "Cañón Dinamax"}},
};

Game::RiftBossState& B(Game& g) { return g.rboss; }
float base_dmg(Game& g) { return std::fmax(10.0f, B(g).dmg) * 0.5f; }

Player* target(Game& g, float x, float y) {
  Player* best = nullptr; float bd = 1e30f;
  for (int i = 0; i < g.nplayers; ++i) {
    Player& p = g.players[i];
    if (!p.active || p.dead) continue;
    const float d = dist2(p.x, p.y, x, y);
    if (d < bd) { bd = d; best = &p; }
  }
  return best;
}

void hit_player(Game& g, Player& p, float k) { player_hurt(g, p, base_dmg(g) * k, B(g).type, false, B(g).cat); }

Hazard* hz(Game& g, float k) {
  Hazard* h = hazard_add(g);
  if (!h) return nullptr;
  h->type = B(g).type; h->cat = B(g).cat; h->dmg = base_dmg(g) * k; h->color = kTypeColor[B(g).type];
  return h;
}

void shot(Game& g, const Enemy& e, float a, float sp, float k, float r = 7, float life = 4.5f) {
  Projectile* s = proj_spawn(g);
  if (!s) return;
  s->x = e.x; s->y = e.y - 6; s->vx = std::cos(a) * sp; s->vy = std::sin(a) * sp;
  s->dmg = base_dmg(g) * k; s->r = r; s->life = life; s->friendly = false; s->color = kTypeColor[B(g).type];
  s->mtype = B(g).type; s->mcat = B(g).cat;
  s->has_vis = true; s->vis_proj = kTypeShape[B(g).type]; s->vis_pal = B(g).type;
}

Game::RiftBossState::Threat* threat(Game& g, int kind) {
  auto& b = B(g);
  if (b.nth >= 8) { for (int i = 0; i < 7; ++i) b.th[i] = b.th[i + 1]; b.nth = 7; }
  auto& t = b.th[b.nth++];
  t = Game::RiftBossState::Threat();
  t.kind = int8_t(kind);
  return &t;
}

// ---------------- los ataques ----------------
void begin_cast(Game& g, Enemy& e, int idx) {
  auto& b = B(g);
  const KitMove& km = b.kit[idx];
  Player* tg = target(g, e.x, e.y);
  if (!tg) return;
  b.state = 2; b.st = 0;
  b.cast = km.pat; b.ct = 0; b.cn = 0; b.cdone = false;
  b.k = b.phase == 1 ? 1 : b.phase == 2 ? 1.12f : 1.25f;
  b.ang = std::atan2(tg->y - e.y, tg->x - e.x);
  b.tx = tg->x; b.ty = tg->y;
  b.last = int8_t(idx);
  std::snprintf(b.cast_name, sizeof(b.cast_name), "%s", km.name);
  const float k = b.k;
  switch (km.pat) {
    case P_CHARGE: {   // aviso en línea y embestida
      Hazard* h = hz(g, 1.6f * k);
      if (h) { h->beam = true; h->x = e.x; h->y = e.y; h->angle = b.ang; h->len = 520; h->w = 46; h->delay = 0.75f; }
      e.anim.play(A_CHARGE, 0.9f);
      b.wind = 1.35f;
      break;
    }
    case P_RING: b.wind = 0.4f + b.phase * 0.85f; e.anim.play(A_ATTACK, 1); break;
    case P_SWEEP: {
      const float dir = g.rng.next() < 0.5f ? 1.0f : -1.0f, arc = 1.7f + b.phase * 0.3f;
      const int n = 10 + b.phase * 2;
      for (int i = 0; i < n; ++i) {
        Hazard* h = hz(g, 1.2f * k);
        if (!h) break;
        h->beam = true; h->x = e.x; h->y = e.y; h->angle = b.ang - dir * arc / 2 + dir * i * arc / n;
        h->len = 600; h->w = 34; h->delay = 0.8f + i * 0.06f;
      }
      b.wind = 0.8f + n * 0.06f + 0.3f;
      e.anim.play(A_CHARGE, 0.8f);
      break;
    }
    case P_LEAP: {
      Hazard* h = hz(g, 1.5f * k);
      if (h) { h->x = tg->x; h->y = tg->y; h->r = 90; h->delay = 0.85f; }
      b.jx = e.x; b.jy = e.y;
      e.anim.play(A_ATTACK, 0.8f);
      b.wind = 1.1f;
      break;
    }
    case P_SPIRAL: b.wind = 2.4f; b.spin = b.ang; b.dir = g.rng.next() < 0.5f ? 1.0f : -1.0f; e.anim.play(A_SHOOT, 0.7f); break;
    case P_WALL: b.wind = 0.4f + (2 + b.phase) * 1.25f * 0.5f; b.wall_off = (g.rng.next() * 2 - 1) * 110; e.anim.play(A_SHOOT, 0.8f); break;
    case P_METEOR: {
      const int n = 4 + b.phase * 3;
      for (int i = 0; i < n; ++i) {
        const float a = g.rng.next() * kTau, d = i ? g.rng.range(30, 170) : 0;
        Hazard* h = hz(g, 1.3f * k);
        if (h) { h->x = tg->x + std::cos(a) * d; h->y = tg->y + std::sin(a) * d * 0.8f; h->r = 52; h->delay = 0.9f + i * 0.12f; }
      }
      b.wind = 0.9f + n * 0.12f;
      e.anim.play(A_CHARGE, 0.9f);
      break;
    }
    case P_TORNADO: {
      const int n = 2 + (b.phase >= 3 ? 1 : 0);
      for (int i = 0; i < n; ++i) {
        auto* t = threat(g, P_TORNADO);
        const float a = b.ang + (i - (n - 1) / 2.0f) * 0.9f;
        t->x = e.x + std::cos(a) * 30; t->y = e.y + std::sin(a) * 30; t->a = a; t->life = 4.5f; t->r = 34;
      }
      b.wind = 1.2f;
      e.anim.play(A_SHOOT, 0.8f);
      break;
    }
    case P_ORBS: {
      const int n = 2 + b.phase;
      for (int i = 0; i < n; ++i) {
        auto* t = threat(g, P_ORBS);
        const float a = b.ang + (i - (n - 1) / 2.0f) * 0.6f;
        t->x = e.x; t->y = e.y; t->a = a; t->life = 6; t->r = 13;
      }
      b.wind = 0.8f;
      e.anim.play(A_SHOOT, 0.8f);
      break;
    }
    case P_SLASHES: {
      const int n = 3 + b.phase;
      for (int i = 0; i < n; ++i) {
        const float a = g.rng.next() * kPi, off = g.rng.range(-60, 60);
        Hazard* h = hz(g, 1.2f * k);
        if (!h) break;
        h->beam = true; h->angle = a; h->len = 260; h->w = 26;
        h->x = tg->x + std::cos(a + kPi / 2) * off - std::cos(a) * 130; h->y = tg->y + std::sin(a + kPi / 2) * off - std::sin(a) * 130;
        h->delay = 0.7f + i * 0.18f;
      }
      b.wind = 0.7f + n * 0.18f;
      e.anim.play(A_ATTACK, 1);
      break;
    }
    case P_PINWHEEL: b.wind = 2.6f; b.spin = b.ang; b.dir = g.rng.next() < 0.5f ? 1.0f : -1.0f; e.anim.play(A_CHARGE, 0.7f); break;
    case P_BLOOM: b.wind = 1.4f; e.anim.play(A_CHARGE, 1); break;
    case P_BLINK: {
      const float a = std::atan2(tg->y - e.y, tg->x - e.x);
      b.jx = tg->x + std::cos(a) * 60; b.jy = tg->y + std::sin(a) * 60;   // (detrás de ti)
      b.wind = 1.1f;
      break;
    }
    case P_GRAVITY: {
      Hazard* h = hz(g, 1.6f * k);
      if (h) { h->x = e.x; h->y = e.y; h->r = 150; h->delay = 2.6f; }
      b.pull_t = 2.4f;
      b.wind = 2.7f;
      e.anim.play(A_CHARGE, 0.6f);
      break;
    }
    case P_CLONES: {   // sombras que embisten desde tres lados
      const int n = 3 + (b.phase >= 3 ? 1 : 0);
      for (int i = 0; i < n; ++i) {
        const float a = g.rng.next() * kTau;
        Hazard* h = hz(g, 1.3f * k);
        if (!h) break;
        h->beam = true; h->x = tg->x + std::cos(a) * 200; h->y = tg->y + std::sin(a) * 160; h->angle = a + kPi;
        h->len = 400; h->w = 38; h->delay = 0.9f + i * 0.2f;
      }
      b.wind = 0.9f + n * 0.2f;
      e.anim.play(A_CHARGE, 1);
      break;
    }
    default: b.wind = 1; break;
  }
}

// → true cuando el ataque ha acabado
bool tick_cast(Game& g, Enemy& e, float dt) {
  auto& b = B(g);
  b.ct += dt;
  const float k = b.k;
  Player* tg = target(g, e.x, e.y);
  switch (b.cast) {
    case P_CHARGE:
      if (b.ct >= 0.75f && b.ct < 1.15f) {   // embiste a lo largo del aviso
        const float sp = 900 * dt;
        const float nx = e.x + std::cos(b.ang) * sp, ny = e.y + std::sin(b.ang) * sp;
        if (g.world.is_free(nx, ny, e.r * 0.6f)) { e.x = nx; e.y = ny; }
        e.anim.loop(A_WALK, 2);
      }
      break;
    case P_RING: {
      static const float AT[3] = {0.1f, 0.95f, 1.8f};
      for (int i = 0; i < 3; ++i) if (b.cn == i && i < 1 + b.phase && b.ct >= AT[i]) {
        b.cn++;
        auto* t = threat(g, P_RING);
        t->x = e.x; t->y = e.y; t->R = 18; t->life = 6;
        const int gaps = b.phase >= 3 ? 2 : 3;
        const float a0 = g.rng.next() * kTau;
        t->ngaps = int8_t(gaps);
        for (int q = 0; q < gaps; ++q) t->gaps[q] = a0 + q * kTau / gaps;
        g.camera.kick(0.3f);
      }
      break;
    }
    case P_LEAP:
      if (b.ct < 0.85f) {   // por el aire hasta donde estabas
        const float f = b.ct / 0.85f;
        e.x = b.jx + (b.tx - b.jx) * f; e.y = b.jy + (b.ty - b.jy) * f;
        e.jump_lift = std::sin(f * kPi) * 120;
      } else if (e.jump_lift > 0) { e.jump_lift = 0; g.camera.kick(0.8f); g.fx.ring(e.x, e.y, 6, 90, kTypeColor[b.type], 0.4f, 4); }
      break;
    case P_SPIRAL:
      if (b.ct > 0.3f && b.ct < b.wind - 0.1f && (b.emit -= dt) <= 0) {
        b.emit = 0.12f;
        b.spin += b.dir * 0.32f;
        const int arms = 2 + b.phase;
        for (int i = 0; i < arms; ++i) shot(g, e, b.spin + i * kTau / arms, 150, 0.9f * k);
      }
      break;
    case P_WALL: {
      const int n = 2 + b.phase;
      if (b.cn < n && b.ct >= 0.4f + b.cn * 0.62f) {
        b.cn++;
        // Un muro de disparos que avanza hacia ti con una puerta de 130 px
        const float ca = std::cos(b.ang), sa = std::sin(b.ang);
        b.wall_off += (g.rng.next() < 0.5f ? -1 : 1) * g.rng.range(60, 105);
        if (std::fabs(b.wall_off) > 190) b.wall_off *= -0.5f;
        for (float o = -300; o <= 300; o += 22) {
          if (std::fabs(o - b.wall_off) < 65) continue;
          Projectile* s = proj_spawn(g);
          if (!s) break;
          s->x = e.x - sa * o; s->y = e.y + ca * o; s->vx = ca * 130; s->vy = sa * 130;
          s->dmg = base_dmg(g) * 1.1f * k; s->r = 8; s->life = 5; s->friendly = false; s->color = kTypeColor[b.type];
          s->mtype = b.type; s->mcat = b.cat; s->has_vis = true; s->vis_proj = kTypeShape[b.type]; s->vis_pal = b.type;
        }
      }
      break;
    }
    case P_PINWHEEL:
      if (b.ct > 0.3f && (b.emit -= dt) <= 0) {
        b.emit = 0.18f;
        b.spin += b.dir * 0.12f;
        const int arms = 2 + b.phase;
        for (int i = 0; i < arms; ++i) shot(g, e, b.spin + i * kTau / arms, 120, 0.9f * k, 8);
      }
      break;
    case P_BLOOM: {
      static const float AT[2] = {0.6f, 1.1f};
      for (int i = 0; i < 2; ++i) if (b.cn == i && b.ct >= AT[i]) {
        b.cn++;
        const int n = 16 + b.phase * 2;
        for (int q = 0; q < n; ++q) shot(g, e, (q + i * 0.5f) * kTau / n, 140, 0.9f * k);
        g.camera.kick(0.3f);
      }
      break;
    }
    case P_BLINK:
      if (b.cn == 0 && b.ct >= 0.4f) {   // desaparece y sale a tu espalda
        b.cn = 1;
        if (g.world.is_free(b.jx, b.jy, e.r * 0.6f)) { e.x = b.jx; e.y = b.jy; }
        g.fx.burst(e.x, e.y - 10, kTypeColor[b.type], 14, 140);
        Hazard* h = hz(g, 1.4f * k);
        if (h) { h->x = e.x; h->y = e.y; h->r = 80; h->delay = 0.45f; }
        e.anim.play(A_ATTACK, 1.2f);
      }
      break;
    default: break;
  }
  // Atrae (Gravedad): hacia él
  if (b.pull_t > 0) {
    b.pull_t -= dt;
    for (int i = 0; i < g.nplayers; ++i) {
      Player& p = g.players[i];
      if (!p.active || p.dead) continue;
      float nx, ny; norm(e.x - p.x, e.y - p.y, nx, ny);
      const float d = dist(p.x, p.y, e.x, e.y) - e.r - p.r;
      if (d > 0) { const float m = std::fmin(d, player_spd(g, p) * 0.55f * dt); p.x += nx * m; p.y += ny * m; }
    }
  }
  (void)tg;
  if (b.ct >= b.wind && !b.cdone) { b.cdone = true; e.anim.play(A_ATTACK, 1.1f); }
  return b.ct >= b.wind + 0.35f;
}

// Anillos, torbellinos y orbes que se mueven por la arena
void tick_threats(Game& g, const Enemy& e, float dt) {
  auto& b = B(g);
  const float R = (g.rift.arena ? 11 - 0.3f : 10) * TS;
  for (int i = b.nth - 1; i >= 0; --i) {
    auto& t = b.th[i];
    t.t += dt;
    bool dead = t.t >= t.life;
    if (t.kind == P_RING) {
      t.R += 150 * dt;
      if (t.R > R + 40) dead = true;
      for (int k = 0; k < g.nplayers && !t.hit; ++k) {
        Player& p = g.players[k];
        if (!p.active || p.dead) continue;
        const float d = dist(p.x, p.y, t.x, t.y);
        if (std::fabs(d - t.R) >= 10 + p.r * 0.5f) continue;
        const float a = std::atan2(p.y - t.y, p.x - t.x);
        bool gap = false;
        for (int q = 0; q < t.ngaps; ++q) gap = gap || std::fabs(std::remainder(a - t.gaps[q], kTau)) < 0.34f;
        if (!gap) { t.hit = true; hit_player(g, p, 1.15f * b.k); }
      }
    } else if (t.kind == P_TORNADO || t.kind == P_ORBS) {
      // Van hacia ti (los orbes, despacio y siguiéndote; los torbellinos, girando)
      Player* tg = target(g, t.x, t.y);
      if (tg) {
        const float want = std::atan2(tg->y - t.y, tg->x - t.x);
        const float turn = t.kind == P_ORBS ? 1.4f : 0.8f;
        t.a += clamp(float(std::remainder(want - t.a, kTau)), -turn * dt, turn * dt);
      }
      const float sp = t.kind == P_ORBS ? 72 : 110;
      t.x += std::cos(t.a) * sp * dt; t.y += std::sin(t.a) * sp * dt;
      if (dist(t.x, t.y, g.rift.ax, g.rift.ay) > R) dead = true;
      t.cd -= dt;
      for (int k = 0; k < g.nplayers && t.cd <= 0; ++k) {
        Player& p = g.players[k];
        if (!p.active || p.dead || dist2(p.x, p.y, t.x, t.y) > (t.r + p.r) * (t.r + p.r)) continue;
        t.cd = 0.6f;
        hit_player(g, p, (t.kind == P_ORBS ? 1.3f : 0.8f) * b.k);
        if (t.kind == P_ORBS) dead = true;
      }
      if (g.vrng.next() < dt * 20) g.fx.burst(t.x, t.y - 6, kTypeColor[b.type], 2, 60);
    }
    if (dead) { for (int q = i; q < b.nth - 1; ++q) b.th[q] = b.th[q + 1]; b.nth--; }
  }
  (void)e;
}

// ¿Hay una columna, un árbol o una roca entre el centro y el jugador?
bool covered(Game& g, float cx, float cy, const Player& p) {
  const float d = dist(cx, cy, p.x, p.y);
  if (d < TS * 2.2f) return false;
  for (float s = TS * 1.5f; s < d - p.r * 0.5f; s += 10) {
    const float x = cx + (p.x - cx) * s / d, y = cy + (p.y - cy) * s / d;
    if (g.world.is_wall(x, y)) return true;
    Prop* pr[4];
    const int n = g.world.props_in_circle(x, y, 6, pr, 4);
    for (int i = 0; i < n; ++i) if (!pr[i]->gone && pr[i]->solid) return true;
  }
  return false;
}

void set_state(Game& g, int st) { B(g).state = int8_t(st); B(g).st = 0; }
}  // namespace

void riftboss_arm(Game& g, Enemy& e, float dmg) {
  auto& b = B(g);
  b = Game::RiftBossState();
  b.on = true;
  b.boss_id = e.id;
  const Mon& m = *mon_by_dex(e.dex);
  b.type = int8_t(m.types[0] >= 0 ? int(m.types[0]) : int(T_NORMAL));
  b.cat = e.cat;
  b.dmg = dmg;
  b.phase = 1;
  // Su kit: el suyo (si lo tiene) y los de sus tipos, rotados según el Pokémon
  int n = 0;
  auto add = [&](const KitMove& km) {
    for (int i = 0; i < n; ++i) if (b.kit[i].pat == km.pat) return;
    if (n < 6) b.kit[n++] = km;
  };
  for (const Sig& s : kSig) if (s.dex == m.num) add(s.m);
  const int r1 = int(std::floor(hash(form_web_id(m.dex), 3, 11) * 5));
  const KitMove* k1 = kKit[b.type];
  if (m.types[1] >= 0) {
    const int r2 = int(std::floor(hash(form_web_id(m.dex), 7, 13) * 5));
    const KitMove* k2 = kKit[m.types[1]];
    const int order[8][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}, {0, 2}, {0, 3}, {1, 2}, {0, 4}};
    for (const auto& o : order) add(o[0] == 0 ? k1[(r1 + o[1]) % 5] : k2[(r2 + o[1]) % 5]);
  } else for (int i = 0; i < 5; ++i) add(k1[(r1 + i) % 5]);
  b.nkit = int8_t(n);
  b.state = 0; b.st = 0;   // llegando
  e.immune = true;
  e.jump_lift = 560;
}

bool riftboss_on(const Game& g) { return g.rboss.on; }

void riftboss_update(Game& g, Enemy& e, float dt) {
  auto& b = B(g);
  e.anim.update(dt);
  if (e.flash > 0) e.flash -= dt;
  if (e.dead) { e.fade -= dt; b.on = false; b.nth = 0; return; }
  if (e.stun_t > 0) e.stun_t -= dt;
  b.st += dt;
  e.kx = e.ky = 0;
  e.floor_k = b.phase < 2 ? 2.0f / 3 : b.phase == 2 ? 1.0f / 3 : 0;
  tick_threats(g, e, dt);
  Player* tg = target(g, e.x, e.y);
  // Tocarle duele (menos cuando está agotado)
  b.touch_cd -= dt;
  if (tg && b.state != 0 && b.state != 5 && b.touch_cd <= 0 && dist2(tg->x, tg->y, e.x, e.y) < (e.r + tg->r) * (e.r + tg->r)) {
    b.touch_cd = 0.8f;
    hit_player(g, *tg, 1);
  }
  switch (b.state) {
    case 0:   // llegando del cielo
      e.immune = true;
      e.jump_lift = b.st < 0.85f ? (1 - b.st / 0.85f) * (1 - b.st / 0.85f) * 560 : 0;
      if (b.st >= 0.85f && !b.landed) { b.landed = true; g.camera.kick(1.1f); g.fx.ring(e.x, e.y, 8, 140, kTypeColor[b.type], 0.5f, 5); }
      if (b.st >= 2.2f) { e.immune = false; set_state(g, 1); b.cd = 0.45f; }
      return;
    case 1: {   // quieto: se coloca a su distancia y elige ataque
      b.cd -= dt;
      if (tg) {
        const float d = dist(e.x, e.y, tg->x, tg->y), sp = e.spd * (b.phase >= 3 ? 1.3f : b.phase == 2 ? 1.15f : 1);
        float nx, ny; norm(tg->x - e.x, tg->y - e.y, nx, ny);
        if ((b.side_t -= dt) <= 0) { b.side_t = g.rng.range(3, 5); b.side = -b.side; }
        float vx, vy;
        if (d > 210) { vx = nx * sp * 1.25f; vy = ny * sp * 1.25f; }
        else if (d < 120) { vx = -nx * sp; vy = -ny * sp; }
        else { vx = -ny * sp * 0.8f * b.side; vy = nx * sp * 0.8f * b.side; }
        if (g.world.is_free(e.x + vx * dt, e.y + vy * dt, e.r * 0.6f)) { e.x += vx * dt; e.y += vy * dt; }
        e.anim.dir = int8_t(dir_from_angle(std::atan2(tg->y - e.y, tg->x - e.x)));
        e.anim.loop(A_WALK, 1);
      }
      if (b.cd <= 0 && tg) {
        // Saca ataques de una bolsa (sin repetir el último): 3 del kit en la fase 1, 5 en la 2, todos en la 3
        const int avail = std::min(int(b.nkit), b.phase == 1 ? 3 : b.phase == 2 ? 5 : int(b.nkit));
        if (!b.nbag) {
          for (int i = 0; i < avail; ++i) b.bag[b.nbag++] = int8_t(i);
          for (int i = b.nbag - 1; i > 0; --i) { const int j = int(g.rng.next() * (i + 1)); std::swap(b.bag[i], b.bag[j]); }
          if (b.nbag > 1 && b.bag[0] == b.last) std::swap(b.bag[0], b.bag[b.nbag - 1]);
        }
        const int idx = b.bag[0];
        for (int i = 0; i < b.nbag - 1; ++i) b.bag[i] = b.bag[i + 1];
        b.nbag--;
        begin_cast(g, e, idx);
      }
      break;
    }
    case 2:   // atacando
      if (tick_cast(g, e, dt)) { set_state(g, 1); b.cd = b.phase == 1 ? 1.25f : b.phase == 2 ? 0.95f : 0.65f; e.jump_lift = 0; b.cast_name[0] = 0; }
      break;
    case 3:   // cambio de fase: al centro, inmune, onda
      e.immune = true;
      if (b.st < 0.7f) { e.x += (g.rift.ax - e.x) * std::fmin(1.0f, 7 * dt); e.y += (g.rift.ay - e.y) * std::fmin(1.0f, 7 * dt); }
      if (!b.boom && b.st >= 0.95f) {
        b.boom = true;
        b.phase = b.next;
        b.nbag = 0;
        e.ai.fury = b.phase >= 3;
        e.anim.play(A_ATTACK, 1);
        g.flash(b.phase >= 3 ? 0xff5a4a : kTypeColor[b.type], 0.5f);
        g.camera.kick(1.2f);
        g.fx.ring(e.x, e.y, 8, 210, kTypeColor[b.type], 0.6f, 6);
        for (int i = 0; i < g.nplayers; ++i) {   // os aparta
          Player& p = g.players[i];
          if (!p.active || p.dead) continue;
          float nx, ny; norm(p.x - e.x, p.y - e.y, nx, ny);
          for (int s = 0; s < 20; ++s) if (g.world.is_free(p.x + nx * 8, p.y + ny * 8, p.r)) { p.x += nx * 8; p.y += ny * 8; }
        }
      }
      if (b.st >= 2.2f) {
        e.immune = false;
        if (b.phase >= 3) {   // el ataque definitivo
          set_state(g, 4);
          b.ult_t = 0; b.ult_done = false;
          e.immune = true;
          e.anim.play(A_CHARGE, 0.6f, true);
          g.say("¡Escóndete detrás de una columna, un árbol o una roca!", 3.4f);
        } else { set_state(g, 1); b.cd = 0.4f; }
      }
      break;
    case 4:   // ataque definitivo
      b.ult_t += dt;
      e.immune = true;
      if (b.ult_t < 0.7f) { e.x += (g.rift.ax - e.x) * std::fmin(1.0f, 8 * dt); e.y += (g.rift.ay - e.y) * std::fmin(1.0f, 8 * dt); }
      if (b.ult_t < ULT_T) { if (g.vrng.next() < dt * 3) g.camera.kick(0.12f + b.ult_t / ULT_T * 0.3f); break; }
      if (!b.ult_done) {
        b.ult_done = true;
        g.flash(0xffffff, 0.7f);
        g.camera.kick(1.6f);
        e.anim.play(A_ATTACK, 1);
        g.fx.ring(e.x, e.y, 8, 520, kTypeColor[b.type], 0.9f, 8);
        for (int i = 0; i < g.nplayers; ++i) {
          Player& p = g.players[i];
          if (!p.active || p.dead) continue;
          if (covered(g, g.rift.ax, g.rift.ay, p)) { g.fx.dmg_text(p.x, p.y - p.body_h - 8, "¡A salvo!", 0x7dffb0); continue; }
          p.invuln = 0;
          player_hurt(g, p, std::fmax(base_dmg(g) * 3, p.max_hp * std::fmin(0.62f, kDiffModes[g.cfg.diff].dmg * 0.42f)), b.type, true);
        }
      }
      if (b.ult_t >= 5.1f) { e.immune = false; e.anim.play(A_HURT, 1, true); set_state(g, 5); }
      break;
    case 5:   // agotado
      if (b.st >= 2.4f) { e.anim.loop(A_IDLE); set_state(g, 1); b.cd = 0.3f; }
      break;
  }
  // (pruebas y capturas: debug_win, «ganar» en el autotest)
  if (g.debug_win && (b.state == 1 || b.state == 2 || b.state == 5) && !e.immune) enemy_hurt(g, e, e.max_hp * 0.03f * dt, 0xffffff, 0, 0, 0, EFF_NONE);
  if (e.dead) return;
  // Cambio de fase a 2/3 y 1/3 de vida
  if (b.state == 1 || b.state == 2) {
    const int nx = b.phase < 2 && e.hp <= e.max_hp * (2.0f / 3) + 1 ? 2 : b.phase == 2 && e.hp <= e.max_hp * (1.0f / 3) + 1 ? 3 : 0;
    if (nx) {
      b.cast_name[0] = 0; b.nth = 0;
      g.hazards.clear();
      for (int i = g.projectiles.n - 1; i >= 0; --i) if (!g.projectiles[i].friendly) g.projectiles.remove_ordered(i);
      b.next = int8_t(nx); b.boom = false;
      e.jump_lift = 0;
      set_state(g, 3);
      e.immune = true;
      e.anim.play(A_CHARGE, 0.8f, true);
      char m[96];
      std::snprintf(m, sizeof(m), nx == 2 ? "¡%s saca todo su poder!" : "¡%s se ha enfurecido!", mon_by_dex(e.dex)->name);
      g.say(m, 2.4f);
    }
  }
}

void riftboss_reset(Game& g) { g.rboss = Game::RiftBossState(); }

}  // namespace ps

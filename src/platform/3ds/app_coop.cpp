// app_coop.cpp — el cooperativo local en la interfaz: pantalla «En grupo»
// (crear sala o buscar), la sala y la partida en grupo con el lockstep
// (core/lockstep) por el juego local de la 3DS (net3ds).
//
// Sala (mismo encabezado que los mensajes del lockstep):
//   invitado -> anfitrión   HELLO  su Pokémon, mejoras y nombre (cada 0,3 s)
//   anfitrión -> todos      ROSTER quién hay en la sala (cada 0,3 s)
//   anfitrión -> todos      START  la partida (RunConfig) y el nodo de cada
//                                  jugador; se repite hasta que cada uno
//                                  manda sus primeras entradas
//   cualquiera              BYE    me voy
#include <3ds.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/settings.h"
#include "platform/3ds/crash3ds.h"
#include "platform/3ds/app.h"
#include "platform/3ds/app_ui.h"
#include "platform/3ds/audio3ds.h"

namespace ps {

namespace {
constexpr double kStepMs = 1000.0 / 60;
constexpr float kLobbyEvery = 0.3f;
const uint32_t kMateColor[4] = {0xffcb3d, 0x5fc8ff, 0x7ee07a, 0xff8ad8};

int put_header(uint8_t* b, uint8_t type) {
  const uint32_t m = net::wire_magic();
  b[0] = uint8_t(m); b[1] = uint8_t(m >> 8); b[2] = uint8_t(m >> 16); b[3] = uint8_t(m >> 24);
  b[4] = type;
  return 5;
}

bool get_header(const uint8_t* b, int len, uint8_t& type) {
  if (len < 5) return false;
  const uint32_t m = uint32_t(b[0]) | uint32_t(b[1]) << 8 | uint32_t(b[2]) << 16 | uint32_t(b[3]) << 24;
  if (m != net::wire_magic()) return false;
  type = b[4];
  return true;
}

// Nombre de la consola (configuración del sistema), en UTF-8.
void console_name(char* out, int cap) {
  std::snprintf(out, size_t(cap), "Jugador");
  if (R_FAILED(cfguInit())) return;
  u16 blk[0x1C / 2] = {};
  if (R_SUCCEEDED(CFGU_GetConfigInfoBlk2(0x1C, 0x000A0000, blk))) {
    int n = 0;
    for (int i = 0; i < 10 && blk[i] && n < cap - 4; ++i) {
      const u16 c = blk[i];
      if (c < 0x80) out[n++] = char(c);
      else if (c < 0x800) { out[n++] = char(0xC0 | (c >> 6)); out[n++] = char(0x80 | (c & 63)); }
      else { out[n++] = char(0xE0 | (c >> 12)); out[n++] = char(0x80 | ((c >> 6) & 63)); out[n++] = char(0x80 | (c & 63)); }
    }
    if (n) out[n] = 0;
  }
  cfguExit();
}
}  // namespace

// ---------------- datos de esta consola ----------------

RunConfig::Member App::my_member() const {
  RunConfig::Member m;
  m.dex = autotest ? auto_dex_ : s_->partner;
  m.shiny = !autotest && s_->partner_shiny && s_->owns_shiny(m.dex);
  for (int k = 0; k < UP_COUNT; ++k) m.upgrades[k] = autotest ? 0 : s_->pupg[m.dex][k];
  for (int k = 0; k < 4; ++k) m.pbind[k] = autotest ? -1 : s_->pbind[m.dex][k];
  for (int k = 0; k < 3; ++k) m.pban[k] = autotest ? -1 : s_->pban[m.dex][k];
  m.pstart = autotest ? -1 : s_->pstart[m.dex];
  m.pmove2 = autotest ? -1 : s_->pmove2[m.dex];
  m.immortal = autotest && auto_immortal_;
  m.mega_up = autotest ? uint8_t(auto_mega_up_) : s_->pmega[m.dex];
  m.test_stone = autotest && auto_mega;
  if (std::strcmp(s_->name, "Jugador") != 0) { std::memcpy(m.name, s_->name, sizeof(m.name) - 1); m.name[sizeof(m.name) - 1] = 0; }
  else console_name(m.name, sizeof(m.name));
  return m;
}

void App::set_coop_autotest(bool host, int dex, bool immortal, bool online, bool suicidal) {
  autotest = true;
  coop_online_ = online;
  net3ds::set_online(online);
  auto_dex_ = dex;
  auto_immortal_ = immortal;
  auto_coop_ = true;
  auto_host_ = host;
  auto_t_ = 0;
  bot.init(host ? 12345 : 54321);
  bot.suicidal = suicidal;
  go(SC_COOP);
}

// ---------------- pantalla «En grupo» ----------------

namespace {
bool ask_text(const char* hint, const char* initial, char* out, int cap, bool numbers) {
  SwkbdState kb;
  swkbdInit(&kb, numbers ? SWKBD_TYPE_NUMPAD : SWKBD_TYPE_WESTERN, 2, cap - 1);
  swkbdSetHintText(&kb, hint);
  swkbdSetInitialText(&kb, initial);
  swkbdSetValidation(&kb, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
  out[0] = 0;
  return swkbdInputText(&kb, out, size_t(cap)) == SWKBD_BUTTON_CONFIRM && out[0];
}
constexpr int kRoomY = 116, kRoomStep = 21, kMaxRoomsShown = 5;   // 116 + 5·21 = 221: antes de la ayuda
// Buscar salas en otro hilo, cada pocos segundos (bloquea mientras espera
// respuesta): 0 nada, 1 buscando, 2 terminado.
Thread g_scan_thr = nullptr;
volatile int g_scan_state = 0;
net3ds::Room g_scan_rooms[kMaxRoomsShown];
volatile int g_scan_n = 0;
uint64_t g_scan_next = 0;
// Conectar con el servidor en otro hilo: 0 nada, 1 conectando, 2 terminado.
Thread g_conn_thr = nullptr;
volatile int g_conn_state = 0;
volatile bool g_conn_ok = false;
}  // namespace

void App::in_coop(const UiIn& in) {
  // (mientras el otro hilo conecta, no se cambia nada de la red: espera)
  const bool connecting = g_conn_state == 1 || g_scan_state == 1;   // (o buscando salas)
  if (!connecting) net3ds::set_server(st_->server, st_->server_port);
  // Por internet: Y (o tocar la fila) cambia el servidor (IP o dominio y puerto).
  if (!connecting && coop_online_ && !auto_coop_ && ((in.down & KEY_Y) || in_rect(&in, 20, 86, 280, 12))) {
    char host[64], port[8], cur[8];
    std::snprintf(cur, sizeof(cur), "%d", st_->server_port);
    if (ask_text("IP o dominio del servidor", st_->server, host, sizeof(host), false)) {
      if (!settings_valid_host(host)) { std::snprintf(coop_msg_, sizeof(coop_msg_), "Ese servidor no vale (sólo letras, cifras, puntos y guiones)"); return; }
      int p = st_->server_port;
      if (ask_text("Puerto (normalmente 24873)", cur, port, sizeof(port), true)) p = std::atoi(port);
      if (p <= 0 || p > 65535) { std::snprintf(coop_msg_, sizeof(coop_msg_), "Ese puerto no vale (1-65535)"); return; }
      std::snprintf(st_->server, sizeof(st_->server), "%s", host);
      st_->server_port = p;
      settings_write(dir_, *st_);
      net3ds::set_server(st_->server, st_->server_port);
      nrooms_ = 0; scanned_ = false; coop_retry_ = 0; coop_fails_ = 0; coop_err_init_ = false;
      std::snprintf(coop_msg_, sizeof(coop_msg_), "Servidor: %s:%d", st_->server, st_->server_port);
    }
    return;
  }
  // Pestañas: cerca (juego local) o por internet.
  bool sw = false;
  if (in.down & (KEY_L | KEY_R)) sw = true;
  if (in_rect(&in, 0, 24, 160, 20) && coop_online_) sw = true;
  if (in_rect(&in, 160, 24, 160, 20) && !coop_online_) sw = true;
  if (sw && !auto_coop_ && !connecting) {
    coop_online_ = !coop_online_;
    net3ds::set_online(coop_online_);
    nrooms_ = 0; scanned_ = false; coop_msg_[0] = 0; cur_ = 0; coop_retry_ = 0; coop_fails_ = 0; coop_err_init_ = false;
  }
  if (!net3ds::ready()) {
    // Se reintenta cada 2 s. Por internet, conectar (DNS, sockets) va en otro
    // hilo: hecho aquí, el juego se quedaba pillado mientras buscaba el servidor.
    const uint64_t now = osGetTime();
    bool ok = false;
    bool tried = false;
    if (coop_online_) {
      if (g_conn_state == 2) { tried = true; ok = g_conn_ok; g_conn_state = 0; threadJoin(g_conn_thr, U64_MAX); threadFree(g_conn_thr); g_conn_thr = nullptr; coop_retry_ = now + (ok ? 0 : 2000); }
      else if (g_conn_state == 0 && now >= coop_retry_) {
        g_conn_state = 1;
        s32 prio = 0x30;
        svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
        g_conn_thr = threadCreate([](void*) { crash3ds::guard("conectar"); g_conn_ok = net3ds::init(); g_conn_state = 2; }, nullptr, 64 * 1024, prio + 1, -2, false);
        if (!g_conn_thr) { g_conn_state = 0; tried = true; ok = net3ds::init(); coop_retry_ = now + 2000; }
      }
    } else if (now >= coop_retry_) {
      tried = true;
      ok = net3ds::init();
      coop_retry_ = now + 2000;
    }
    if (tried) {
      if (!ok) {
        // Por internet, el primer fallo suele ser el DNS de la consola recién
        // conectada al wifi: se sigue intentando sin asustar; el error sólo
        // sale si falla 4 veces seguidas (unos 8 s).
        coop_fails_++;
        if (!coop_online_ || coop_fails_ >= 4) {
          std::snprintf(coop_msg_, sizeof(coop_msg_), "%s", coop_online_ ? net3ds::last_error() : "No se puede usar el juego local (¿conexión inalámbrica apagada?)");
          coop_err_init_ = true;
        }
      }
    }
    if (!ok || !net3ds::ready()) {
      if (in.down & KEY_B) go(SC_MENU);
      return;
    }
  }
  // Ya conectado: fuera el aviso del intento fallido.
  coop_fails_ = 0;
  if (coop_err_init_) { coop_msg_[0] = 0; coop_err_init_ = false; }
  // Las salas se buscan solas (en otro hilo, cada 3 s).
  {
    const uint64_t now = osGetTime();
    if (g_scan_state == 2) {
      threadJoin(g_scan_thr, U64_MAX); threadFree(g_scan_thr); g_scan_thr = nullptr;
      nrooms_ = g_scan_n;
      for (int i = 0; i < nrooms_; ++i) rooms_[i] = g_scan_rooms[i];
      scanned_ = true;
      g_scan_state = 0;
      g_scan_next = now + 3000;
      if (cur_ > nrooms_) cur_ = nrooms_;
      if (!nrooms_ && coop_online_ && net3ds::last_error()[0]) std::snprintf(coop_msg_, sizeof(coop_msg_), "%s", net3ds::last_error());
      else if (coop_msg_[0] && !coop_err_init_) coop_msg_[0] = 0;
    } else if (g_scan_state == 0 && now >= g_scan_next) {
      g_scan_state = 1;
      s32 prio = 0x30;
      svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
      g_scan_thr = threadCreate([](void*) { crash3ds::guard("buscar salas"); net3ds::Room r[kMaxRoomsShown]; const int k = net3ds::scan(r, kMaxRoomsShown); for (int i = 0; i < k; ++i) g_scan_rooms[i] = r[i]; g_scan_n = k; g_scan_state = 2; },
                                nullptr, 64 * 1024, prio + 1, -2, false);
      if (!g_scan_thr) { g_scan_state = 0; g_scan_next = now + 3000; }
    }
  }
  const bool scanning = g_scan_state == 1;
  const int n = 1 + nrooms_;
  if (in.rep & KEY_DUP) cur_ = (cur_ + n - 1) % n;
  if (in.rep & KEY_DDOWN) cur_ = (cur_ + 1) % n;
  int pick = -1;
  if (in_rect(&in, 20, 50, 280, 34)) pick = 0;
  for (int i = 0; i < nrooms_; ++i) if (in_rect(&in, 20, kRoomY + i * kRoomStep, 280, kRoomStep - 2)) pick = 1 + i;
  if (in.down & KEY_A) pick = cur_;
  if (in.down & KEY_B) { if (!scanning) { coop_leave(); go(SC_MENU); } return; }
  // Autotest: el anfitrión crea la sala; el otro espera a que salga en la lista.
  if (auto_coop_) {
    if (auto_host_) pick = 0;
    else if (nrooms_ > 0 && !auto_look) pick = 1;
  }
  if (pick < 0 || scanning) return;   // (mientras busca, no se toca la red: un momento)
  cur_ = pick;
  coop_msg_[0] = 0;
  const RunConfig::Member me = my_member();
  net3ds::set_name(me.name);
  if (pick == 0) {
    if (!net3ds::create(me.name)) { std::snprintf(coop_msg_, sizeof(coop_msg_), "No se ha podido crear la sala. %s", coop_online_ ? net3ds::last_error() : ""); return; }
    nlobby_ = 1;
    lobby_[0].node = net3ds::kHostNode;
    lobby_[0].m = me;
    lobby_t_ = 0;
    go(SC_LOBBY);
  } else {
    const net3ds::Room& rm = rooms_[pick - 1];
    if (!net3ds::join(rm.index)) { std::snprintf(coop_msg_, sizeof(coop_msg_), "No se ha podido entrar en la sala de %s. %s", rm.host, coop_online_ ? net3ds::last_error() : ""); return; }
    nlobby_ = 0;
    lobby_t_ = 0;
    go(SC_LOBBY);
  }
}

void App::bottom_coop() {
  header("Jugar en grupo");
  // Pestañas
  for (int t = 0; t < 2; ++t) {
    const bool on = (t == 1) == coop_online_;
    C2D_DrawRectSolid(t * 160 + 4, 24, 0, 152, 20, on ? C_SEL : C_PANEL);
    if (on) C2D_DrawRectSolid(t * 160 + 4, 42, 0, 152, 2, C_GOLD);
    r_->text(t ? "Por internet" : "Cerca (juego local)", t * 160 + 80, 28, 0.36f, on ? C_GOLD : C_MUTED, 1);
  }
  button(20, 50, 280, 34, "Crear sala", cur_ == 0);
  char b[96];
  // El servidor (por internet), una línea de estado (el aviso en rojo si lo
  // hay) y debajo las salas, que se buscan solas: nada se pisa con 5.
  const char* status = nullptr;
  if (coop_online_) {
    // Servidor (se cambia con Y o tocando aquí).
    std::snprintf(b, sizeof(b), "Servidor: %s:%d  (Y: cambiar)", st_->server, st_->server_port);
    r_->text(b, 160, 88, 0.32f, C2D_Color32(154, 230, 255, 255), 1);
    static char conn[48];
    std::snprintf(conn, sizeof(conn), coop_fails_ ? "Conectando con el servidor… (intento %d)" : "Conectando con el servidor…", coop_fails_ + 1);
    if (!net3ds::ready() && !coop_msg_[0]) status = conn;
    else if (!scanned_) status = "Buscando salas…";
    else status = nrooms_ ? "Salas abiertas (se actualizan solas):" : "No hay salas abiertas. Crea una y avisa.";
  } else {
    if (!scanned_) status = "Buscando salas cerca…";
    else status = nrooms_ ? "Salas cerca (se actualizan solas):" : "No hay salas cerca. Pide a alguien que cree una.";
  }
  if (coop_msg_[0]) r_->text_wrap(coop_msg_, 160, 100, 0.32f, C2D_Color32(255, 138, 150, 255), 290, 1);
  else if (status) r_->text(status, nrooms_ ? 24 : 160, 100, 0.32f, C_MUTED, nrooms_ ? 0 : 1);
  for (int i = 0; i < nrooms_; ++i) {
    const float y = kRoomY + i * kRoomStep;
    C2D_DrawRectSolid(20, y, 0, 280, kRoomStep - 2, cur_ == 1 + i ? C_SEL : C_PANEL);
    if (cur_ == 1 + i) C2D_DrawRectSolid(20, y, 0, 3, kRoomStep - 2, C_GOLD);
    std::snprintf(b, sizeof(b), "Sala de %s", rooms_[i].host);
    r_->text(b, 30, y + 3, 0.34f, C_TEXT, 0);
    std::snprintf(b, sizeof(b), "%d / 4", rooms_[i].players);
    r_->text(b, 292, y + 3, 0.34f, C_MUTED, 2);
  }
  r_->text("A: crear o entrar · L/R: cerca o por internet · B: volver", 160, 228, 0.32f, C_MUTED, 1);
}

// ---------------- sala ----------------

void App::lobby_send_roster() {
  uint8_t b[512];
  int n = put_header(b, net::MSG_ROSTER);
  b[n++] = uint8_t(nlobby_);
  for (int i = 0; i < nlobby_; ++i) {
    b[n++] = uint8_t(lobby_[i].node); b[n++] = uint8_t(lobby_[i].node >> 8);
    const int w = net::write_member(lobby_[i].m, b + n, int(sizeof(b)) - n);
    if (w < 0) return;
    n += w;
  }
  net3ds::send(net3ds::kBroadcast, b, n);
}

void App::lobby_net(float dt) {
  uint8_t buf[1500];
  uint16_t from = 0;
  int len;
  const bool host = net3ds::is_host();
  while ((len = net3ds::poll(buf, sizeof(buf), &from)) > 0) {
    uint8_t type;
    if (!get_header(buf, len, type)) continue;
    const uint8_t* p = buf + 5;
    const int pl = len - 5;
    if (host && type == net::MSG_HELLO) {
      RunConfig::Member m;
      if (!net::read_member(p, pl, m)) continue;
      uint8_t tmpm[128];
      const int msz = net::write_member(m, tmpm, sizeof(tmpm));
      const int rtt = pl >= msz + 2 ? int(p[msz] | p[msz + 1] << 8) : 0;
      int slot = -1;
      for (int i = 0; i < nlobby_; ++i) if (lobby_[i].node == from) slot = i;
      if (slot < 0 && nlobby_ < kMaxPlayers) slot = nlobby_++;
      if (slot < 0) continue;
      // Nombres repetidos (dos consolas sin nombre): se numeran.
      for (int i = 0; i < nlobby_; ++i)
        if (i != slot && !std::strcmp(lobby_[i].m.name, m.name)) { const int L = int(std::min<size_t>(std::strlen(m.name), 12)); m.name[L] = ' '; m.name[L + 1] = char('1' + slot); m.name[L + 2] = 0; }
      lobby_[slot].node = from;
      lobby_[slot].m = m;
      lobby_[slot].rtt = rtt;
    } else if (host && type == net::MSG_BYE) {
      for (int i = 1; i < nlobby_; ++i)
        if (lobby_[i].node == from) { for (int k = i; k < nlobby_ - 1; ++k) lobby_[k] = lobby_[k + 1]; nlobby_--; break; }
    } else if (!host && type == net::MSG_ROSTER) {
      if (pl < 1) continue;
      const int n = std::min<int>(p[0], kMaxPlayers);
      uint8_t tmp[128];
      const int msz = net::write_member(RunConfig::Member(), tmp, sizeof(tmp));   // (tamaño fijo)
      int off = 1;
      int got = 0;
      for (int i = 0; i < n; ++i) {
        if (off + 2 + msz > pl) break;
        lobby_[i].node = uint16_t(p[off] | p[off + 1] << 8);
        off += 2;
        if (!net::read_member(p + off, msz, lobby_[i].m)) break;
        off += msz;
        got++;
      }
      nlobby_ = got;
    } else if (!host && type == net::MSG_START) {
      if (pl < 1) continue;
      const int n = p[0];
      if (n < 2 || n > kMaxPlayers || pl < 2 + 2 * n) continue;
      const int delay = p[1];
      uint16_t nodes[kMaxPlayers];
      for (int i = 0; i < n; ++i) nodes[i] = uint16_t(p[2 + 2 * i] | p[3 + 2 * i] << 8);
      RunConfig cfg;
      if (!net::read_config(p + 2 + 2 * n, pl - 2 - 2 * n, cfg) || cfg.nplayers != n) continue;
      const uint16_t me = net3ds::my_node();
      int idx = -1;
      for (int i = 0; i < n; ++i) if (nodes[i] == me) idx = i;
      if (idx <= 0) continue;
      for (int i = 0; i < n; ++i) coop_nodes_[i] = nodes[i];
      coop_begin(cfg, idx, delay);
      return;
    } else if (!host && type == net::MSG_BYE && from == net3ds::kHostNode) {
      coop_leave();
      std::snprintf(coop_msg_, sizeof(coop_msg_), "El anfitrión ha cerrado la sala");
      go(SC_COOP);
      return;
    }
  }
  // Anfitrión: fuera los que ya no están conectados.
  if (host) {
    const uint16_t mask = net3ds::node_mask();
    for (int i = nlobby_ - 1; i >= 1; --i)
      if (!(mask & (1u << (lobby_[i].node - 1)))) { for (int k = i; k < nlobby_ - 1; ++k) lobby_[k] = lobby_[k + 1]; nlobby_--; }
    net3ds::set_room_info(nlobby_);
  } else if (net3ds::in_room() && !(net3ds::node_mask() & 1u) && lobby_t_ > 2) {
    coop_leave();
    std::snprintf(coop_msg_, sizeof(coop_msg_), "Se ha perdido la sala");
    go(SC_COOP);
    return;
  }
  lobby_t_ += dt;
  static float every = 0;
  every += dt;
  if (every >= kLobbyEvery) {
    every = 0;
    if (host) lobby_send_roster();
    else {
      uint8_t b[160];
      int n = put_header(b, net::MSG_HELLO);
      const int w = net::write_member(my_member(), b + n, int(sizeof(b)) - n);
      if (w > 0) {
        const int rtt = net3ds::rtt_ms();   // (por internet, para elegir el retraso)
        b[n + w] = uint8_t(rtt); b[n + w + 1] = uint8_t(rtt >> 8);
        net3ds::send(net3ds::kHostNode, b, n + w + 2);
      }
    }
  }
}

void App::in_lobby(const UiIn& in, float dt) {
  lobby_net(dt);
  if (scene != SC_LOBBY) return;
  const bool host = net3ds::is_host();
  if (in.down & KEY_B) { coop_leave(); go(SC_COOP); return; }
  const bool go_now = (in.down & KEY_A) || in_rect(&in, 20, 186, 280, 30) || (auto_coop_ && host && lobby_t_ > 1.5f);
  if (host && nlobby_ >= 2 && go_now) coop_host_start();
}

void App::bottom_lobby() {
  const bool host = net3ds::is_host();
  char b[64];
  if (net3ds::online()) std::snprintf(b, sizeof(b), "%s %s · ping %d ms", host ? "Tu sala" : "Sala", net3ds::room_code(), net3ds::rtt_ms());
  else std::snprintf(b, sizeof(b), "%s", host ? "Tu sala" : "En la sala");
  header(b);
  for (int i = 0; i < kMaxPlayers; ++i) {
    const float y = 30 + i * 38;
    C2D_DrawRectSolid(20, y, 0, 280, 34, C_PANEL);
    if (i >= nlobby_) { r_->text("(libre)", 160, y + 11, 0.36f, C_MUTED, 1); continue; }
    const RunConfig::Member& m = lobby_[i].m;
    C2D_DrawRectSolid(20, y, 0, 3, 34, rgb(kMateColor[i]));
    draw_mon(m.dex, m.shiny, 46, y + 30, 1);
    r_->text(m.name, 72, y + 4, 0.45f, rgb(kMateColor[i]), 0);
    std::snprintf(b, sizeof(b), "%s%s", mon_by_dex(m.dex) ? mon_by_dex(m.dex)->name : "?", i == 0 ? " · anfitrión" : "");
    r_->text(b, 72, y + 20, 0.34f, C_MUTED, 0);
  }
  if (host) {
    if (nlobby_ >= 2) button(20, 186, 280, 30, "¡Empezar!", true);
    else r_->text("Esperando a que entre alguien…", 160, 196, 0.4f, C_MUTED, 1);
  } else {
    r_->text("Esperando a que el anfitrión empiece…", 160, 196, 0.4f, C_MUTED, 1);
  }
  r_->text(host ? "A: empezar · B: cerrar la sala" : "B: salir de la sala", 160, 226, 0.32f, C_MUTED, 1);
}

// ---------------- partida en grupo ----------------

void App::coop_host_start() {
  RunConfig c;
  c.seed = uint32_t(osGetTime()) ^ 0xa5a5a5a5u;
  c.judged = autotest ? 1 : s_->stats.judged;
  for (int d = 0; d <= kMaxDex; ++d) if (!autotest && s_->legends[d]) c.legends_beaten[d >> 3] |= uint8_t(1 << (d & 7));
  c.nplayers = nlobby_;
  for (int i = 0; i < nlobby_; ++i) { c.members[i] = lobby_[i].m; coop_nodes_[i] = lobby_[i].node; }
  c.dex = c.members[0].dex; c.shiny = c.members[0].shiny;
  if (autotest) { c.demo_time = uint16_t(auto_coop_time); c.demo_level = uint8_t(auto_level); }   // (para grabar)
  for (int i = 0; i < kMaxPlayers; ++i) start_acked_[i] = i == 0;
  // Retraso de los controles: en local, el fijo; por internet, lo que tarda en
  // ir y volver lo de cada uno por el servidor (con margen).
  int delay = net::kDelay;
  if (net3ds::online()) {
    int worst = 0;
    for (int i = 1; i < nlobby_; ++i) worst = std::max(worst, lobby_[i].rtt);
    delay = clamp(int(std::ceil((net3ds::rtt_ms() + worst) / 16.7)) + 3, net::kDelay, 30);
  }
  net3ds::close_room();
  coop_begin(c, 0, delay);
}

void App::coop_begin(const RunConfig& cfg, int idx, int delay) {
  coop_cfg_ = cfg;
  coop_ = true;
  g_->local = idx;
  g_->now_ms = now_ms();
  g_->start_run(cfg, autotest ? nullptr : s_);
  apply_settings();   // (la inmortalidad de las pruebas va por jugador, en cfg)
  r_->sprites.async = false;
  r_->preload_run(*g_);
  r_->sprites.async = true;
  r_->card_cursor = 0;
  bond_first_ = -1;
  paused_ = false;
  pending_input = Input();
  coop_over_t_ = 0;
  quit_t_ = -1;
  coop_msg_[0] = 0;
  ls_.begin(idx, cfg.nplayers, delay);
  scene = SC_RUN;
}

void App::coop_end(const char* msg) {
  if (msg) std::snprintf(coop_msg_, sizeof(coop_msg_), "%s", msg);
  if (g_->state != ST_OVER) g_->quit_run();
}

void App::coop_net() {
  uint8_t buf[1500];
  uint16_t from = 0;
  int len;
  while ((len = net3ds::poll(buf, sizeof(buf), &from)) > 0) {
    uint8_t type;
    if (!get_header(buf, len, type)) continue;
    int idx = -1;
    for (int i = 0; i < coop_cfg_.nplayers; ++i) if (coop_nodes_[i] == from) idx = i;
    if (idx < 0) continue;
    if (type == net::MSG_INPUT) start_acked_[idx] = true;
    if (type == net::MSG_INPUT || type == net::MSG_BUNDLE) ls_.on_packet(buf, len, idx);
  }
  if (ls_.host()) {
    // START hasta que cada uno mande sus primeras entradas.
    bool all = true;
    for (int i = 1; i < coop_cfg_.nplayers; ++i) if (!start_acked_[i] && !ls_.gone(i)) all = false;
    if (!all) {
      uint8_t b[1024];
      int n = put_header(b, net::MSG_START);
      b[n++] = uint8_t(coop_cfg_.nplayers);
      b[n++] = uint8_t(ls_.advised(1));   // (el retraso con el que empiezan los invitados; luego el anfitrión lo ajusta a cada uno)
      for (int i = 0; i < coop_cfg_.nplayers; ++i) { b[n++] = uint8_t(coop_nodes_[i]); b[n++] = uint8_t(coop_nodes_[i] >> 8); }
      const int w = net::write_config(coop_cfg_, b + n, int(sizeof(b)) - n);
      if (w > 0) net3ds::send(net3ds::kBroadcast, b, n + w);
    }
    // Quien se ha desconectado de la radio, fuera ya (sin esperar 20 s).
    const uint16_t mask = net3ds::node_mask();
    for (int i = 1; i < coop_cfg_.nplayers; ++i)
      if (!ls_.gone(i) && start_acked_[i] && !(mask & (1u << (coop_nodes_[i] - 1)))) ls_.drop(i);
  }
}

void App::coop_leave() {
  if (net3ds::in_room()) {
    uint8_t b[8];
    const int n = put_header(b, net::MSG_BYE);
    net3ds::send(net3ds::is_host() ? net3ds::kBroadcast : net3ds::kHostNode, b, n);
    net3ds::leave();
  }
  net3ds::exit();
  coop_ = false;
  ls_ = net::Lockstep();
  g_->local = 0;
  nlobby_ = 0;
  nrooms_ = 0;
  scanned_ = false;
}

void App::advance(double& acc, float dt) {
  if (!coop_) {
    int steps = 0;
    while (acc >= kStepMs && steps < 3) {
      g_->step(&pending_input);
      for (const SfxEvent& e : g_->sfx_queue) audio::play_game(e);
      // La orden, el hueco y el cambio se usan una sola vez.
      pending_input.select = -1; pending_input.cycle = 0; pending_input.cmd = CMD_NONE;
      acc -= kStepMs;
      steps++;
    }
    return;
  }
  coop_net();
  if (pending_input.cmd == CMD_QUIT && quit_t_ < 0) quit_t_ = 0;
  int steps = 0;
  while (acc >= kStepMs && steps < 4) {
    while (ls_.want_local()) {
      ls_.push_local(pending_input);
      pending_input.select = -1; pending_input.cycle = 0; pending_input.cmd = CMD_NONE;
    }
    if (!ls_.step(*g_)) break;
    for (const SfxEvent& e : g_->sfx_queue) audio::play_game(e);
    acc -= kStepMs;
    steps++;
  }
  if (acc > kStepMs * 4) acc = kStepMs * 4;   // (esperando a alguien: no se acumula)
  net::Packet pk[2];
  const int np = ls_.outbox(pk, 2);
  for (int k = 0; k < np; ++k) net3ds::send(pk[k].to < 0 ? net3ds::kBroadcast : net3ds::kHostNode, pk[k].data, pk[k].len);
  ls_.tick_frame();
  if (g_->state == ST_OVER) { coop_over_t_ += dt; return; }
  if (ls_.desync()) coop_end("Las consolas se han desincronizado (fallo del juego). Partida terminada.");
  else if (ls_.lost_host()) coop_end("Se ha perdido la conexión con el anfitrión.");
  else if (!g_->me().active) coop_end(nullptr);                              // te has ido (o te han quitado)
  else if (quit_t_ >= 0 && (quit_t_ += dt) > 4) coop_end(nullptr);          // pediste irte y no llega
}

// Arriba, en la partida en grupo: a quién se espera, pausa y avisos.
void App::coop_overlay() {
  if (!coop_ && !coop_msg_[0]) return;
  // Por internet: el ping siempre a la vista (verde, amarillo o rojo).
  if (coop_ && net3ds::online()) {
    char pb[24];
    const int p = net3ds::rtt_ms();
    std::snprintf(pb, sizeof(pb), "ping %d ms", p);
    const float w = r_->text_width(pb, 0.32f) + 8;
    C2D_DrawRectSolid(TW - w - 2, 2, 0, w, 11, C2D_Color32(0, 0, 0, 150));
    r_->text(pb, TW - 6, 3, 0.32f, p < 90 ? C2D_Color32(126, 224, 122, 255) : p < 160 ? C_GOLD : C2D_Color32(255, 95, 109, 255), 2, false);
  }
  if (coop_ && g_->state != ST_OVER && ls_.stall_frames() > 30) {
    char names[64] = "";
    const uint8_t mask = ls_.waiting_mask();
    for (int i = 0; i < coop_cfg_.nplayers; ++i)
      if (mask & (1 << i)) {
        if (names[0]) std::strncat(names, ", ", sizeof(names) - std::strlen(names) - 1);
        std::strncat(names, coop_cfg_.members[i].name, sizeof(names) - std::strlen(names) - 1);
      }
    char b[96];
    if (names[0]) std::snprintf(b, sizeof(b), "Esperando a %s…", names);
    else std::snprintf(b, sizeof(b), "%s", ls_.host() ? "Esperando…" : "Esperando al anfitrión…");
    C2D_DrawRectSolid(0, 100, 0, TW, 30, C2D_Color32(0, 0, 0, 170));
    r_->text(b, 200, 108, 0.45f, C_GOLD, 1);
  }
  // Colores de cada jugador del grupo (los mismos que sus nombres en el mapa).
static const uint32_t kTeamColor[4] = {0xffcb3d, 0x5fc8ff, 0x7ee07a, 0xff8ad8};
// Caído: el equipo arriba (sprite y nombre de cada uno), el que ves resaltado.
  if (coop_ && spect_ >= 0 && g_->me().dead && g_->state != ST_OVER) {
    const Game& g = *g_;
    const int n = g.nplayers;
    const float bw = 62, x0 = 200 - n * bw / 2;
    C2D_DrawRectSolid(x0 - 6, 16, 0, n * bw + 12, 62, C2D_Color32(8, 12, 22, 190));
    for (int i = 0; i < n; ++i) {
      const Player& p = g.players[i];
      if (!p.active) continue;
      const float x = x0 + i * bw;
      const bool sel = i == spect_;
      if (sel) {
        C2D_DrawRectSolid(x + 2, 18, 0, bw - 4, 2, C_GOLD); C2D_DrawRectSolid(x + 2, 74, 0, bw - 4, 2, C_GOLD);
        C2D_DrawRectSolid(x + 2, 18, 0, 2, 58, C_GOLD); C2D_DrawRectSolid(x + bw - 4, 18, 0, 2, 58, C_GOLD);
      }
      draw_mon(p.dex, p.shiny, x + bw / 2, 58, 1, p.dead ? 3 : 0);
      if (p.dead) r_->text("X", x + bw - 12, 20, 0.42f, C2D_Color32(255, 95, 109, 255), 1);   // (caído)
      const char* nm = i == g.local ? "Tú" : g.cfg.members[i].name;
      r_->text(nm, x + bw / 2, 61, 0.3f, sel ? C_GOLD : rgb(kTeamColor[i & 3]), 1);
    }
    char b[64];
    std::snprintf(b, sizeof(b), spect_ == g.local ? "Te ves a ti · L/R: ver a otro" : "Viendo a %s · L/R: cambiar", g.cfg.members[spect_].name);
    r_->text(b, 200, 2, 0.34f, C_TEXT, 1);   // (arriba: en medio está el nombre del que sigues)
  }
  if (coop_ && paused_ && g_->state == ST_PLAYING) {
    C2D_DrawRectSolid(0, 60, 0, TW, 44, C2D_Color32(0, 0, 0, 170));
    r_->text("PAUSA (la partida sigue para los demás)", 200, 64, 0.45f, C_GOLD, 1);
    r_->text("START: seguir · SELECT: dejar la partida", 200, 86, 0.36f, C_TEXT, 1);
  }
  if (coop_msg_[0] && g_->state == ST_OVER) {
    C2D_DrawRectSolid(0, 4, 0, TW, 20, C2D_Color32(0, 0, 0, 170));
    r_->text(coop_msg_, 200, 8, 0.36f, C2D_Color32(255, 138, 150, 255), 1);
  }
}

}  // namespace ps

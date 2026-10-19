// net3ds.cpp — ver net3ds.h. Basado en el ejemplo "uds" de devkitPro.
#include "platform/3ds/net3ds.h"

#include <3ds.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ps {
namespace net3ds {

namespace {
constexpr u32 kCommId = 0x50535333;        // id de la comunicación local de PokéSurvivor 3DS
constexpr u8 kChannel = 1;
const char kPass[] = "pokesurvivor-3ds-coop";
constexpr u32 kAppMagic = 0x50535331;      // datos de la sala: versión del juego y nombre

struct AppData { u32 magic; u8 version; u8 players; char host[16]; };

bool g_init = false, g_room = false, g_host = false;
udsBindContext g_bind;
udsNetworkScanInfo* g_found = nullptr;
size_t g_nfound = 0;
AppData g_app;
}  // namespace

bool init() {
  if (g_init) return true;
  if (R_FAILED(udsInit(0x3000, nullptr))) return false;
  g_init = true;
  return true;
}

void exit() {
  leave();
  if (g_found) { std::free(g_found); g_found = nullptr; g_nfound = 0; }
  if (g_init) udsExit();
  g_init = false;
}

bool ready() { return g_init; }

bool create(const char* host_name) {
  if (!g_init || g_room) return false;
  udsNetworkStruct net;
  udsGenerateDefaultNetworkStruct(&net, kCommId, 0, kMaxNodes);
  if (R_FAILED(udsCreateNetwork(&net, kPass, sizeof(kPass), &g_bind, kChannel, UDS_DEFAULT_RECVBUFSIZE))) return false;
  std::memset(&g_app, 0, sizeof(g_app));
  g_app.magic = kAppMagic; g_app.version = 1; g_app.players = 1;
  std::snprintf(g_app.host, sizeof(g_app.host), "%s", host_name);
  udsSetApplicationData(&g_app, sizeof(g_app));
  g_room = true; g_host = true;
  return true;
}

void set_room_info(int players) {
  if (!g_room || !g_host) return;
  g_app.players = u8(players);
  udsSetApplicationData(&g_app, sizeof(g_app));
}

int scan(Room* out, int max) {
  if (!g_init || g_room) return 0;
  if (g_found) { std::free(g_found); g_found = nullptr; g_nfound = 0; }
  const size_t sz = 0x4000;
  void* tmp = std::malloc(sz);
  if (!tmp) return 0;
  const Result r = udsScanBeacons(tmp, sz, &g_found, &g_nfound, kCommId, 0, nullptr, false);
  std::free(tmp);
  if (R_FAILED(r)) { g_found = nullptr; g_nfound = 0; return 0; }
  int n = 0;
  for (size_t i = 0; i < g_nfound && n < max; ++i) {
    AppData a;
    size_t got = 0;
    if (R_FAILED(udsGetNetworkStructApplicationData(&g_found[i].network, &a, sizeof(a), &got)) || got != sizeof(a) || a.magic != kAppMagic) continue;
    Room& rm = out[n++];
    std::memcpy(rm.host, a.host, sizeof(rm.host));
    rm.host[15] = 0;
    rm.players = g_found[i].network.total_nodes;
    rm.index = int(i);
  }
  return n;
}

bool join(int index) {
  if (!g_init || g_room || index < 0 || size_t(index) >= g_nfound) return false;
  if (R_FAILED(udsConnectNetwork(&g_found[index].network, kPass, sizeof(kPass), &g_bind, UDS_BROADCAST_NETWORKNODEID,
                                 UDSCONTYPE_Client, kChannel, UDS_DEFAULT_RECVBUFSIZE)))
    return false;
  g_room = true; g_host = false;
  return true;
}

void leave() {
  if (!g_room) return;
  if (g_host) udsDestroyNetwork(); else udsDisconnectNetwork();
  udsUnbind(&g_bind);
  g_room = false; g_host = false;
}

bool in_room() { return g_room; }
bool is_host() { return g_host; }

bool send(uint16_t to_node, const void* data, int len) {
  if (!g_room || len <= 0 || len > UDS_DATAFRAME_MAXSIZE) return false;
  const Result r = udsSendTo(to_node, kChannel, UDS_SENDFLAG_Default, data, size_t(len));
  return !(R_FAILED(r) && u32(r) != 0xC86113F0u);   // (ese error: nadie escuchando, no es grave)
}

int poll(void* buf, int cap, uint16_t* from_node) {
  if (!g_room) return 0;
  size_t got = 0;
  u16 src = 0;
  if (R_FAILED(udsPullPacket(&g_bind, buf, size_t(cap), &got, &src)) || !got) return 0;
  if (from_node) *from_node = src;
  return int(got);
}

uint16_t node_mask() {
  if (!g_room) return 0;
  udsConnectionStatus st;
  if (R_FAILED(udsGetConnectionStatus(&st))) return 0;
  return st.node_bitmask;
}

uint16_t my_node() {
  if (!g_room) return 0;
  udsConnectionStatus st;
  if (R_FAILED(udsGetConnectionStatus(&st))) return 0;
  return st.cur_NetworkNodeID;
}

}  // namespace net3ds
}  // namespace ps

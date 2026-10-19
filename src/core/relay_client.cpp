// relay_client.cpp — ver relay_client.h.
#include "core/relay_client.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>

namespace ps {
namespace relay {

namespace {
const uint8_t kMagic[4] = {'P', 'S', 'R', '1'};
enum : uint8_t { HOST = 1, JOIN, LIST, SEND, PING, INFO, BYE };
enum : uint8_t { ROOM = 101, JOINED, ROOMS, DATA, PEERS, PONG, ERR };
}  // namespace

uint64_t now_ms() {
  timeval tv;
  gettimeofday(&tv, nullptr);
  return uint64_t(tv.tv_sec) * 1000 + uint64_t(tv.tv_usec / 1000);
}

bool Client::open(const char* host, int port) {
  close();
  sockaddr_in sa;
  std::memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = htons(uint16_t(port));
  if (inet_pton(AF_INET, host, &sa.sin_addr) != 1) {
    hostent* he = gethostbyname(host);
    if (!he || !he->h_addr_list[0]) { error = "No se encuentra el servidor."; return false; }
    std::memcpy(&sa.sin_addr, he->h_addr_list[0], 4);
  }
  fd_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd_ < 0) { error = "No se puede abrir la conexión."; return false; }
  fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL, 0) | O_NONBLOCK);
  static_assert(sizeof(sa) <= sizeof(addr_), "sockaddr");
  std::memcpy(addr_, &sa, sizeof(sa));
  node_ = mask_ = 0;
  kicked_ = false;
  qn_ = qhead_ = 0;
  return true;
}

void Client::close() {
  if (fd_ >= 0) {
    if (node_) raw_send(BYE, nullptr, 0);
    ::close(fd_);
  }
  fd_ = -1;
  node_ = mask_ = 0;
}

bool Client::raw_send(uint8_t type, const void* body, int len) {
  if (fd_ < 0 || len < 0 || len > 1250) return false;
  uint8_t pk[1300];
  std::memcpy(pk, kMagic, 4);
  pk[4] = type;
  if (len) std::memcpy(pk + 5, body, size_t(len));
  return sendto(fd_, pk, size_t(5 + len), 0, reinterpret_cast<const sockaddr*>(addr_), sizeof(sockaddr_in)) == 5 + len;
}

int Client::raw_recv(uint8_t* buf, int cap, int timeout_ms) {
  if (fd_ < 0) return 0;
  if (timeout_ms > 0) {
    pollfd pf;
    pf.fd = fd_; pf.events = POLLIN; pf.revents = 0;
    if (::poll(&pf, 1, timeout_ms) <= 0) return 0;
  }
  sockaddr_in from;
  socklen_t fl = sizeof(from);
  const int n = int(recvfrom(fd_, buf, size_t(cap), 0, reinterpret_cast<sockaddr*>(&from), &fl));
  if (n < 5 || std::memcmp(buf, kMagic, 4) != 0) return 0;
  // Sólo del servidor.
  const sockaddr_in* srv = reinterpret_cast<const sockaddr_in*>(addr_);
  if (from.sin_addr.s_addr != srv->sin_addr.s_addr || from.sin_port != srv->sin_port) return 0;
  return n;
}

void Client::handle_ctrl(const uint8_t* p, int n) {
  const uint8_t t = p[4];
  if (t == PEERS && n >= 7) mask_ = uint16_t(p[5] | p[6] << 8);
  else if (t == PONG && n >= 15) {
    uint64_t sent = 0;
    std::memcpy(&sent, p + 5, 8);
    const uint64_t now = now_ms();
    if (now >= sent && now - sent < 10000) rtt_ = rtt_ ? (rtt_ * 3 + int(now - sent)) / 4 : int(now - sent);
    if (node_) mask_ = uint16_t(p[13] | p[14] << 8);
  } else if (t == ERR) {
    error.assign(reinterpret_cast<const char*>(p + 5), size_t(n - 5));
    if (node_ > 1) { kicked_ = true; node_ = 0; }
  } else if (t == DATA && n >= 7) {
    if (qn_ >= kQueue) return;   // (se pierde: el lockstep lo repite)
    const int slot = (qhead_ + qn_) % kQueue;
    qfrom_[slot] = uint16_t(p[5] | p[6] << 8);
    qlen_[slot] = n - 7;
    std::memcpy(q_[slot], p + 7, size_t(n - 7));
    qn_++;
  }
}

bool Client::wait_for(uint8_t type, uint8_t* buf, int cap, int timeout_ms, int& len) {
  const uint64_t end = now_ms() + uint64_t(timeout_ms);
  while (now_ms() < end) {
    const int n = raw_recv(buf, cap, int(end - now_ms()));
    if (!n) continue;
    if (buf[4] == type) { len = n; return true; }
    if (buf[4] == ERR) { handle_ctrl(buf, n); return false; }
    handle_ctrl(buf, n);
  }
  if (error.empty()) error = "El servidor no contesta (¿sin internet?).";
  return false;
}

bool Client::create(const char* name, int timeout_ms) {
  error.clear();
  uint8_t buf[1300];
  int len = 0;
  for (int tries = 0; tries < 3; ++tries) {
    raw_send(HOST, name, int(std::strlen(name) > 15 ? 15 : std::strlen(name)));
    if (wait_for(ROOM, buf, sizeof(buf), timeout_ms / 3, len) && len >= 10) {
      std::memcpy(code_, buf + 5, 4); code_[4] = 0;
      node_ = buf[9];
      mask_ = 1;
      return true;
    }
    if (!error.empty() && error.find("contesta") == std::string::npos) return false;
  }
  return false;
}

int Client::list(RoomInfo* out, int max, int timeout_ms) {
  error.clear();
  // Relleno: el servidor nunca contesta más grande de lo que recibe.
  uint8_t pad[600];
  std::memset(pad, 0, sizeof(pad));
  uint8_t buf[1300];
  int len = 0;
  for (int tries = 0; tries < 2; ++tries) {
    raw_send(LIST, pad, sizeof(pad));
    if (wait_for(ROOMS, buf, sizeof(buf), timeout_ms / 2, len) && len >= 6) {
      const int n = buf[5];
      int got = 0, off = 6;
      for (int i = 0; i < n && got < max && off + 21 <= len; ++i, off += 21) {
        RoomInfo& r = out[got++];
        std::memcpy(r.code, buf + off, 4); r.code[4] = 0;
        std::memcpy(r.host, buf + off + 4, 15); r.host[15] = 0;
        r.players = buf[off + 20];
      }
      return got;
    }
  }
  return 0;
}

bool Client::join(const char* code, const char* name, int timeout_ms) {
  error.clear();
  uint8_t body[24];
  std::memcpy(body, code, 4);
  const int nl = int(std::strlen(name) > 15 ? 15 : std::strlen(name));
  std::memcpy(body + 4, name, size_t(nl));
  uint8_t buf[1300];
  int len = 0;
  for (int tries = 0; tries < 3; ++tries) {
    raw_send(JOIN, body, 4 + nl);
    if (wait_for(JOINED, buf, sizeof(buf), timeout_ms / 3, len) && len >= 10) {
      std::memcpy(code_, buf + 5, 4); code_[4] = 0;
      node_ = buf[9];
      return true;
    }
    if (!error.empty() && error.find("contesta") == std::string::npos) return false;
  }
  return false;
}

void Client::leave() {
  if (node_) raw_send(BYE, nullptr, 0);
  node_ = mask_ = 0;
  qn_ = 0;
}

void Client::set_open(bool open) {
  const uint8_t b[2] = {uint8_t(open ? 1 : 0), 0};
  raw_send(INFO, b, 2);
}

bool Client::send(uint16_t to, const void* data, int len) {
  if (!node_ || len > 1200) return false;
  uint8_t body[1210];
  body[0] = uint8_t(to); body[1] = uint8_t(to >> 8);
  std::memcpy(body + 2, data, size_t(len));
  return raw_send(SEND, body, 2 + len);
}

int Client::poll(void* buf, int cap, uint16_t* from) {
  if (fd_ < 0) return 0;
  // PING cada segundo (mantiene la sala y mide el ping).
  const uint64_t now = now_ms();
  if (now - last_ping_ >= 1000) {
    last_ping_ = now;
    raw_send(PING, &now, 8);
  }
  uint8_t pk[1300];
  for (int k = 0; k < 64 && qn_ < kQueue; ++k) {
    const int n = raw_recv(pk, sizeof(pk), 0);
    if (!n) break;
    handle_ctrl(pk, n);
  }
  if (!qn_) return 0;
  const int slot = qhead_;
  qhead_ = (qhead_ + 1) % kQueue;
  qn_--;
  const int n = qlen_[slot] < cap ? qlen_[slot] : cap;
  std::memcpy(buf, q_[slot], size_t(n));
  if (from) *from = qfrom_[slot];
  return n;
}

}  // namespace relay
}  // namespace ps

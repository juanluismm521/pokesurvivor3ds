// lockstep.cpp — ver lockstep.h.
#include "core/lockstep.h"

#include <cmath>
#include <algorithm>
#include <cstring>

namespace ps {
namespace net {

// ---------------- bytes ----------------
namespace {
struct W {
  uint8_t* p; int cap, n = 0;
  bool ok = true;
  void u8(uint32_t v) { if (n + 1 > cap) { ok = false; return; } p[n++] = uint8_t(v); }
  void u16(uint32_t v) { u8(v & 255); u8((v >> 8) & 255); }
  void u32(uint32_t v) { u16(v & 0xFFFF); u16(v >> 16); }
  void raw(const void* d, int len) { if (n + len > cap) { ok = false; return; } std::memcpy(p + n, d, size_t(len)); n += len; }
};
struct R {
  const uint8_t* p; int len, n = 0;
  bool ok = true;
  uint32_t u8() { if (n + 1 > len) { ok = false; return 0; } return p[n++]; }
  uint32_t u16() { const uint32_t a = u8(); return a | (u8() << 8); }
  uint32_t u32() { const uint32_t a = u16(); return a | (u16() << 16); }
  void raw(void* d, int l) { if (n + l > len) { ok = false; std::memset(d, 0, size_t(l)); return; } std::memcpy(d, p + n, size_t(l)); n += l; }
};
int8_t q127(float v) { return int8_t(std::lround(std::fmax(-1.0f, std::fmin(1.0f, v)) * 127)); }
}  // namespace

void pack_input(const Input& in, uint8_t* o) {
  o[0] = uint8_t(q127(in.ax)); o[1] = uint8_t(q127(in.ay));
  o[2] = uint8_t(in.select); o[3] = uint8_t(in.cycle);
  o[4] = uint8_t(in.cmd); o[5] = uint8_t(in.arg);
  o[6] = uint8_t(in.moves[0]); o[7] = uint8_t(in.moves[1]);
}

Input unpack_input(const uint8_t* b) {
  Input in;
  in.ax = int8_t(b[0]) / 127.0f; in.ay = int8_t(b[1]) / 127.0f;
  in.select = int8_t(b[2]); in.cycle = int8_t(b[3]);
  in.cmd = int8_t(b[4]); in.arg = int8_t(b[5]);
  in.moves[0] = int8_t(b[6]); in.moves[1] = int8_t(b[7]); in.moves[2] = in.moves[3] = -1;
  return in;
}

static void put_member(W& w, const RunConfig::Member& m) {
  w.u16(uint32_t(m.dex)); w.u8(m.shiny ? 1 : 0);
  w.raw(m.upgrades, UP_COUNT); w.raw(m.pbind, 4); w.raw(m.pban, 3); w.raw(m.name, 16);
}
static void get_member(R& r, RunConfig::Member& m) {
  m.dex = int(r.u16()); m.shiny = r.u8() != 0;
  r.raw(m.upgrades, UP_COUNT); r.raw(m.pbind, 4); r.raw(m.pban, 3); r.raw(m.name, 16);
  m.name[15] = 0;
  if (m.dex < 1 || m.dex > kMaxDex) { m.dex = 25; r.ok = false; }
}

int write_member(const RunConfig::Member& m, uint8_t* out, int cap) { W w{out, cap}; put_member(w, m); return w.ok ? w.n : -1; }
bool read_member(const uint8_t* b, int len, RunConfig::Member& m) { R r{b, len}; get_member(r, m); return r.ok; }

int write_config(const RunConfig& c, uint8_t* out, int cap) {
  W w{out, cap};
  w.u32(c.seed); w.u32(uint32_t(c.judged)); w.u8(uint32_t(c.nplayers));
  w.raw(c.legends_beaten, int(sizeof(c.legends_beaten)));
  for (int i = 0; i < c.nplayers && i < kMaxPlayers; ++i) put_member(w, c.members[i]);
  return w.ok ? w.n : -1;
}

bool read_config(const uint8_t* b, int len, RunConfig& c) {
  R r{b, len};
  c = RunConfig();
  c.seed = r.u32(); c.judged = int(r.u32()); c.nplayers = int(r.u8());
  if (c.nplayers < 1 || c.nplayers > kMaxPlayers) return false;
  r.raw(c.legends_beaten, int(sizeof(c.legends_beaten)));
  for (int i = 0; i < c.nplayers; ++i) get_member(r, c.members[i]);
  c.dex = c.members[0].dex; c.shiny = c.members[0].shiny;
  return r.ok;
}

// ---------------- lockstep ----------------

void Lockstep::begin(int me, int n) {
  on_ = true;
  me_ = me; n_ = n;
  sim_ = local_ = bundled_ = 0;
  std::memset(have_, 0, sizeof(have_));
  for (int i = 0; i < kMaxPlayers; ++i) {
    gone_[i] = i >= n; acked_[i] = 0; heard_[i] = 0;
    for (int k = 0; k < kHashRing; ++k) { peer_tick_[i][k] = -1; peer_hash_[i][k] = 0; }
  }
  for (int k = 0; k < kHashRing; ++k) my_tick_[k] = -1;
  frames_ = stall_ = 0;
  last_hash_tick_ = -1;
  desync_ = lost_host_ = false;
  host_wait_ = 0;
}

bool Lockstep::ready() const { return on_ && sim_ < bundled_; }

// El invitado se adelanta según lo último que ha llegado del anfitrión (no
// según lo que ha simulado): si no, cada paso esperaría una ida y vuelta.
bool Lockstep::want_local() const { return on_ && local_ < (host() ? sim_ : std::max(sim_, bundled_)) + kDelay; }

void Lockstep::push_local(const Input& in) {
  const Input q = quantize(in);
  mine_[local_ % kRing] = q;
  if (host()) { in_[local_ % kRing][0] = q; have_[local_ % kRing][0] = true; host_collect(); }
  local_++;
}

bool Lockstep::step(Game& g) {
  if (!ready()) return false;
  const int slot = int(sim_ % kRing);
  Input ins[kMaxPlayers];
  for (int i = 0; i < n_; ++i) ins[i] = in_[slot][i];
  for (int i = 0; i < kMaxPlayers; ++i) have_[slot][i] = false;   // (el hueco se reusará para sim_ + kRing)
  g.step(ins);
  sim_++;
  if (sim_ % kHashEvery == 0) {
    const int k = int((sim_ / kHashEvery) % kHashRing);
    my_hash_[k] = g.state_hash();
    my_tick_[k] = sim_;
    last_hash_tick_ = sim_;
    for (int i = 0; i < n_; ++i) check_hash(i, sim_);
  }
  return true;
}

void Lockstep::host_collect() {
  for (;;) {
    const int slot = int(bundled_ % kRing);
    bool complete = true;
    for (int p = 0; p < n_; ++p) {
      if (have_[slot][p]) continue;
      if (gone_[p]) { in_[slot][p] = Input(); have_[slot][p] = true; continue; }
      // Lleva demasiado sin dar noticias: se va (todos lo verán en este paso).
      if (p != 0 && heard_[p] > kTimeoutFrames) {
        in_[slot][p] = Input(); in_[slot][p].cmd = CMD_QUIT; have_[slot][p] = true;
        gone_[p] = true;
        continue;
      }
      complete = false;
    }
    if (!complete || bundled_ >= local_ + kRing / 2) return;
    for (int p = 1; p < n_; ++p) if (in_[slot][p].cmd == CMD_QUIT) gone_[p] = true;   // (se ha ido él)
    bundled_++;
  }
}

// Si están la huella del otro y la tuya del mismo paso, se comparan.
void Lockstep::check_hash(int who, int64_t t) {
  if (who == me_ || t < 0) return;
  const int k = int((t / kHashEvery) % kHashRing);
  if (peer_tick_[who][k] != t || my_tick_[k] != t) return;
  if (peer_hash_[who][k] != my_hash_[k]) desync_ = true;
}

void Lockstep::got_hash(int who, int64_t t, uint32_t h) {
  if (t <= 0 || t % kHashEvery) return;
  const int k = int((t / kHashEvery) % kHashRing);
  peer_tick_[who][k] = t; peer_hash_[who][k] = h;
  check_hash(who, t);
}

void Lockstep::on_packet(const uint8_t* data, int len, int from) {
  if (!on_ || from < 0 || from >= kMaxPlayers) return;
  R r{data, len};
  if (r.u32() != kMagic) return;
  const uint32_t type = r.u8();
  if (type == MSG_INPUT && host()) {
    const int p = int(r.u8());
    if (p != from || p <= 0 || p >= n_ || gone_[p]) return;
    const int64_t first = int64_t(r.u32());
    const int count = int(r.u8());
    const int64_t ack = int64_t(r.u32());
    const int64_t ht = int64_t(int32_t(r.u32()));
    const uint32_t hh = r.u32();
    for (int k = 0; k < count && r.ok; ++k) {
      uint8_t b[kInputBytes];
      r.raw(b, kInputBytes);
      const int64_t t = first + k;
      if (!r.ok || t < bundled_ || t >= bundled_ + kRing / 2) continue;
      const int slot = int(t % kRing);
      if (!have_[slot][p]) { in_[slot][p] = unpack_input(b); have_[slot][p] = true; }
    }
    if (!r.ok) return;
    heard_[p] = 0;
    if (ack > acked_[p]) acked_[p] = ack;
    got_hash(p, ht, hh);
    host_collect();
  } else if (type == MSG_BUNDLE && !host()) {
    const int n = int(r.u8());
    if (n != n_) return;
    const int64_t first = int64_t(r.u32());
    const int count = int(r.u8());
    const int64_t ht = int64_t(int32_t(r.u32()));
    const uint32_t hh = r.u32();
    const uint32_t flags = r.u8();
    const uint8_t wait = uint8_t(r.u8());
    for (int k = 0; k < count && r.ok; ++k) {
      uint8_t b[kMaxPlayers][kInputBytes];
      for (int p = 0; p < n; ++p) r.raw(b[p], kInputBytes);
      const int64_t t = first + k;
      if (!r.ok || t < bundled_ || t >= sim_ + kRing / 2) continue;
      const int slot = int(t % kRing);
      if (have_[slot][0]) continue;
      for (int p = 0; p < n; ++p) in_[slot][p] = unpack_input(b[p]);
      have_[slot][0] = true;
    }
    if (!r.ok) return;
    while (have_[bundled_ % kRing][0] && bundled_ < sim_ + kRing / 2) bundled_++;
    heard_[0] = 0;
    host_wait_ = wait;
    if (flags & 1) desync_ = true;
    got_hash(0, ht, hh);
  }
}

int Lockstep::outbox(Packet* out, int max) {
  if (!on_ || max <= 0) return 0;
  Packet& pk = out[0];
  W w{pk.data, kMaxPacket};
  w.u32(kMagic);
  const int64_t ht = last_hash_tick_;
  const uint32_t hh = ht >= 0 ? my_hash_[(ht / kHashEvery) % kHashRing] : 0;
  if (host()) {
    // Desde lo más viejo que le falte a alguien (sin pasar de lo que cabe).
    int64_t from = bundled_ - kResend;
    for (int p = 1; p < n_; ++p) if (!gone_[p] && acked_[p] < from) from = acked_[p];
    if (from < 0) from = 0;
    if (from < bundled_ - kRing / 2) from = bundled_ - kRing / 2;
    const int per = n_ * kInputBytes;
    int count = int(bundled_ - from);
    const int fit = (kMaxPacket - 24) / per;
    if (count > fit) count = fit;
    if (count > 255) count = 255;
    w.u8(MSG_BUNDLE); w.u8(uint32_t(n_)); w.u32(uint32_t(from)); w.u8(uint32_t(count));
    w.u32(uint32_t(int32_t(ht))); w.u32(hh);
    w.u8(desync_ ? 1 : 0); w.u8(waiting_mask());
    for (int k = 0; k < count; ++k) {
      const int slot = int((from + k) % kRing);
      for (int p = 0; p < n_; ++p) { uint8_t b[kInputBytes]; pack_input(in_[slot][p], b); w.raw(b, kInputBytes); }
    }
    pk.to = -1;
  } else {
    int64_t from = local_ - kResend;
    if (from < bundled_) from = bundled_;        // (lo anterior ya lo tiene el anfitrión)
    if (from < 0) from = 0;
    const int count = int(local_ - from);
    w.u8(MSG_INPUT); w.u8(uint32_t(me_)); w.u32(uint32_t(from)); w.u8(uint32_t(count));
    w.u32(uint32_t(bundled_)); w.u32(uint32_t(int32_t(ht))); w.u32(hh);
    for (int k = 0; k < count; ++k) { uint8_t b[kInputBytes]; pack_input(mine_[(from + k) % kRing], b); w.raw(b, kInputBytes); }
    pk.to = 0;
  }
  pk.len = w.ok ? w.n : 0;
  return pk.len > 0 ? 1 : 0;
}

void Lockstep::tick_frame() {
  if (!on_) return;
  frames_++;
  for (int i = 0; i < kMaxPlayers; ++i) if (heard_[i] < 1 << 30) heard_[i]++;
  stall_ = ready() ? 0 : stall_ + 1;
  if (host()) host_collect();
  else if (heard_[0] > kTimeoutFrames) lost_host_ = true;
}

uint8_t Lockstep::waiting_mask() const {
  if (!host()) return stall_ > 30 ? host_wait_ : 0;
  uint8_t m = 0;
  const int slot = int(bundled_ % kRing);
  for (int p = 1; p < n_; ++p) if (!gone_[p] && !have_[slot][p]) m |= uint8_t(1 << p);
  return m;
}

}  // namespace net
}  // namespace ps

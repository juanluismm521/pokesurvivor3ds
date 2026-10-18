// gfx.cpp — ver gfx.h.
#include "platform/3ds/gfx.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/world.h"

namespace ps {

// ---------------- atlas ----------------

bool Atlas::load(const char* t3x, const char* idx) {
  FILE* f = std::fopen(t3x, "rb");
  if (!f) return false;
  t3x_ = Tex3DS_TextureImportStdio(f, &tex_, nullptr, false);
  std::fclose(f);
  if (!t3x_) return false;
  C3D_TexSetFilter(&tex_, GPU_NEAREST, GPU_NEAREST);
  FILE* fi = std::fopen(idx, "r");
  if (!fi) return false;
  int W = 0, H = 0;
  if (std::fscanf(fi, "%d %d", &W, &H) != 2) { std::fclose(fi); return false; }
  // Cuenta las líneas.
  long pos = std::ftell(fi);
  char name[32];
  int x, y, w, h, n = 0;
  while (std::fscanf(fi, "%31s %d %d %d %d", name, &x, &y, &w, &h) == 5) n++;
  std::fseek(fi, pos, SEEK_SET);
  subs_ = static_cast<Tex3DS_SubTexture*>(std::malloc(sizeof(Tex3DS_SubTexture) * n));
  names_ = static_cast<char(*)[32]>(std::malloc(32 * n));
  n_ = 0;
  while (n_ < n && std::fscanf(fi, "%31s %d %d %d %d", name, &x, &y, &w, &h) == 5) {
    Tex3DS_SubTexture& s = subs_[n_];
    s.width = u16(w); s.height = u16(h);
    s.left = float(x) / W; s.right = float(x + w) / W;
    s.top = 1.0f - float(y) / H; s.bottom = 1.0f - float(y + h) / H;
    std::snprintf(names_[n_], 32, "%s", name);
    n_++;
  }
  std::fclose(fi);
  return true;
}

void Atlas::free() {
  if (t3x_) { Tex3DS_TextureFree(t3x_); C3D_TexDelete(&tex_); t3x_ = nullptr; }
  std::free(subs_); subs_ = nullptr;
  std::free(names_); names_ = nullptr;
  n_ = 0;
}

int Atlas::find(const char* name) const {
  for (int i = 0; i < n_; ++i) if (!std::strcmp(names_[i], name)) return i;
  return -1;
}

// ---------------- sprites ----------------

namespace {
// Lector sobre un búfer en memoria (los archivos se leen enteros de una vez).
struct Mem {
  const uint8_t* p;
  const uint8_t* end;
  template <class T> bool rd(T& v) {
    if (p + sizeof(T) > end) return false;
    std::memcpy(&v, p, sizeof(T));
    p += sizeof(T);
    return true;
  }
};

bool read_all(const char* path, uint8_t*& buf, uint32_t& len) {
  FILE* f = std::fopen(path, "rb");
  if (!f) return false;
  std::fseek(f, 0, SEEK_END);
  const long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  buf = n > 0 ? static_cast<uint8_t*>(std::malloc(size_t(n))) : nullptr;
  const bool ok = buf && std::fread(buf, 1, size_t(n), f) == size_t(n);
  std::fclose(f);
  if (!ok) { std::free(buf); buf = nullptr; return false; }
  len = uint32_t(n);
  return true;
}
}  // namespace

// Lee de la SD el .fr y sus páginas (.t3x). Se puede llamar desde el hilo.
bool SpriteBank::fetch(int dex, bool shiny, Files& f) {
  char path[64];
  std::snprintf(path, sizeof(path), "romfs:/spr/%d%s.fr", dex, shiny ? "s" : "");
  f.n = 0;
  if (!read_all(path, f.buf[0], f.len[0])) return false;
  f.n = 1;
  if (f.len[0] < 8 || std::memcmp(f.buf[0], "PSF1", 4) || f.buf[0][4] > 2) { drop(f); return false; }
  const int npages = f.buf[0][4];
  for (int p = 0; p < npages; ++p) {
    std::snprintf(path, sizeof(path), "romfs:/spr/%d%s_%d.t3x", dex, shiny ? "s" : "", p);
    uint8_t* raw = nullptr;
    uint32_t rlen = 0;
    if (!read_all(path, raw, rlen)) { drop(f); return false; }
    // Cabecera de tex3ds: nº de subtexturas (u16), log2 de ancho y alto - 3
    // (3 bits cada uno) y tipo, formato, mipmaps; luego 12 bytes por
    // subtextura (no se usan: van en el .fr) y los datos comprimidos.
    bool ok = rlen >= 5;
    if (ok) {
      const uint32_t nsub = raw[0] | (raw[1] << 8), off = 5 + nsub * 12;
      const uint16_t w = uint16_t(8 << (raw[2] & 7)), h = uint16_t(8 << ((raw[2] >> 3) & 7));
      const uint32_t size = uint32_t(w) * h * 2;   // (RGBA5551 / RGBA4: 16 bits)
      ok = off < rlen && (raw[3] == GPU_RGBA5551 || raw[3] == GPU_RGBA4) && !(raw[2] & 0x40) && raw[4] == 0;
      if (ok) {
        f.buf[1 + p] = static_cast<uint8_t*>(std::malloc(size));
        ok = f.buf[1 + p] && decompress(f.buf[1 + p], size, nullptr, raw + off, rlen - off);
        f.len[1 + p] = size; f.w[1 + p] = w; f.h[1 + p] = h; f.fmt[1 + p] = raw[3];
      }
    }
    std::free(raw);
    f.n = 2 + p;
    if (!ok) { drop(f); return false; }
  }
  return true;
}

void SpriteBank::drop(Files& f) {
  for (auto& b : f.buf) { std::free(b); b = nullptr; }
  f.n = 0;
}

bool SpriteBank::build(SpriteSet& s, int dex, bool shiny, const Files& fl) {
  Mem f{fl.buf[0], fl.buf[0] + fl.len[0]};
  char magic[4];
  uint8_t npages = 0, pad = 0;
  uint16_t nimages = 0;
  if (!f.rd(magic) || std::memcmp(magic, "PSF1", 4) || !f.rd(npages) || !f.rd(pad) || !f.rd(nimages) || npages > 2 || fl.n < 1 + npages) return false;
  uint16_t pw[2] = {}, ph[2] = {};
  for (int p = 0; p < npages; ++p) { f.rd(pw[p]); f.rd(ph[p]); }
  s.nimages = nimages;
  s.subs = static_cast<Tex3DS_SubTexture*>(std::malloc(sizeof(Tex3DS_SubTexture) * nimages * 2));
  s.page_of = static_cast<uint8_t*>(std::malloc(nimages));
  for (int i = 0; i < nimages; ++i) {
    uint8_t page = 0, z;
    uint16_t x = 0, y = 0, w = 0, h = 0;
    f.rd(page); f.rd(z); f.rd(x); f.rd(y); f.rd(w); f.rd(h);
    if (page >= npages) page = 0;
    const float W = pw[page] ? pw[page] : 1, H = ph[page] ? ph[page] : 1;
    Tex3DS_SubTexture& a = s.subs[i * 2];
    a.width = w; a.height = h;
    a.left = x / W; a.right = (x + w) / W;
    a.top = 1.0f - y / H; a.bottom = 1.0f - (y + h) / H;
    Tex3DS_SubTexture& b = s.subs[i * 2 + 1];
    b = a;
    b.left = a.right; b.right = a.left;     // en espejo
    s.page_of[i] = page;
  }
  s.bytes = 0;
  for (int a = 0; a < A_COUNT; ++a) {
    s.an[a] = s.ar[a] = 0;
    f.rd(s.an[a]); f.rd(s.ar[a]);
    const int n = s.an[a] * s.ar[a];
    s.frames[a] = n ? static_cast<SpriteFrame*>(std::malloc(sizeof(SpriteFrame) * n)) : nullptr;
    for (int k = 0; k < n; ++k) {
      uint16_t img = 0xFFFF; uint8_t flip = 0, z; int16_t ox = 0, oy = 0;
      f.rd(img); f.rd(flip); f.rd(z); f.rd(ox); f.rd(oy);
      s.frames[a][k] = {img, flip, ox, oy};
    }
  }
  for (int p = 0; p < npages; ++p) {
    if (!C3D_TexInit(&s.tex[p], fl.w[1 + p], fl.h[1 + p], GPU_TEXCOLOR(fl.fmt[1 + p]))) { s.npages = p; unload(s); return false; }
    std::memcpy(s.tex[p].data, fl.buf[1 + p], fl.len[1 + p]);
    C3D_TexFlush(&s.tex[p]);
    C3D_TexSetFilter(&s.tex[p], GPU_NEAREST, GPU_NEAREST);
    s.bytes += s.tex[p].size;
  }
  s.npages = npages;
  s.dex = int16_t(dex);
  s.shiny = shiny;
  loads++;
  return true;
}

void SpriteBank::worker(void* arg) {
  SpriteBank* b = static_cast<SpriteBank*>(arg);
  for (;;) {
    LightEvent_Wait(&b->ev_);
    for (;;) {
      int dex = -1; bool shiny = false; Req* r = nullptr;
      LightLock_Lock(&b->lock_);
      for (auto& q : b->reqs_) if (q.state == RQ_QUEUED) { r = &q; dex = q.dex; shiny = q.shiny; break; }
      LightLock_Unlock(&b->lock_);
      if (!r) break;
      Files f;
      const u64 t0 = svcGetSystemTick();
      const bool ok = fetch(dex, shiny, f);
      const uint32_t us = uint32_t((svcGetSystemTick() - t0) * 1000000ull / SYSCLOCK_ARM11);
      LightLock_Lock(&b->lock_);
      r->f = f;
      r->state = ok ? RQ_READY : RQ_FAILED;
      if (us > b->read_max_us) b->read_max_us = us;
      LightLock_Unlock(&b->lock_);
    }
  }
}

void SpriteBank::init() {
  LightLock_Init(&lock_);
  LightEvent_Init(&ev_, RESET_ONESHOT);
  // En el núcleo principal y con más prioridad: casi todo el rato espera a la
  // SD, así que apenas quita tiempo a la partida.
  s32 prio = 0x30;
  svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
  thr_ = threadCreate(worker, this, 16 * 1024, prio - 1, -2, true);
}

void SpriteBank::unload(SpriteSet& s) {
  for (int p = 0; p < s.npages; ++p) C3D_TexDelete(&s.tex[p]);
  for (auto& fr : s.frames) { std::free(fr); fr = nullptr; }
  std::free(s.subs); s.subs = nullptr;
  std::free(s.page_of); s.page_of = nullptr;
  s.dex = -1; s.npages = 0; s.bytes = 0;
}

SpriteSet* SpriteBank::victim() {
  SpriteSet* v = nullptr;
  for (auto& s : slots_) {
    if (s.dex < 0) { if (!v || v->dex >= 0) v = &s; }
    else if (!v || (v->dex >= 0 && s.stamp < v->stamp)) v = &s;
  }
  if (v->dex >= 0) unload(*v);
  return v;
}

SpriteSet* SpriteBank::get(int dex, bool shiny) {
  for (auto& s : slots_) if (s.dex == dex && s.shiny == shiny) { s.stamp = ++stamp_; return &s; }
  if (async && thr_) {
    LightLock_Lock(&lock_);
    Req* free_rq = nullptr;
    for (auto& q : reqs_) {
      if (q.state != RQ_FREE && q.dex == dex && q.shiny == shiny) { LightLock_Unlock(&lock_); return nullptr; }
      if (q.state == RQ_FREE && !free_rq) free_rq = &q;
    }
    if (free_rq) { free_rq->dex = int16_t(dex); free_rq->shiny = shiny; free_rq->state = RQ_QUEUED; }
    LightLock_Unlock(&lock_);
    if (free_rq) LightEvent_Signal(&ev_);
    return nullptr;
  }
  Files f;
  if (!fetch(dex, shiny, f)) return nullptr;
  SpriteSet* v = victim();
  const bool ok = build(*v, dex, shiny, f);
  drop(f);
  if (!ok) return nullptr;
  v->stamp = ++stamp_;
  return v;
}

bool SpriteBank::loading(int dex, bool shiny) const {
  if (!thr_) return false;
  LightLock_Lock(&lock_);
  bool r = false;
  for (const auto& q : reqs_) if (q.state == RQ_QUEUED || q.state == RQ_READY) r = r || (q.dex == dex && q.shiny == shiny);
  LightLock_Unlock(&lock_);
  return r;
}

void SpriteBank::poll() {
  if (!thr_) return;
  for (auto& q : reqs_) {
    LightLock_Lock(&lock_);
    const int st = q.state;
    LightLock_Unlock(&lock_);
    if (st != RQ_READY) continue;   // (los fallidos se quedan: no se reintenta)
    bool have = false;
    for (const auto& s : slots_) have = have || (s.dex == q.dex && s.shiny == q.shiny);
    const u64 t0 = svcGetSystemTick();
    if (!have) {   // (la precarga pudo cargarlo mientras tanto)
      SpriteSet* v = victim();
      if (build(*v, q.dex, q.shiny, q.f)) v->stamp = ++stamp_;
    }
    const uint32_t us = uint32_t((svcGetSystemTick() - t0) * 1000000ull / SYSCLOCK_ARM11);
    late_loads++; late_total_us += us;
    if (us > late_max_us) late_max_us = us;
    drop(q.f);
    LightLock_Lock(&lock_);
    q.state = RQ_FREE; q.dex = -1;
    LightLock_Unlock(&lock_);
  }
}

bool SpriteBank::frame(SpriteSet* s, int anim, int row, int f, C2D_Image& img, int& ox, int& oy, int& w, int& h) {
  if (!s || anim < 0 || anim >= A_COUNT || !s->frames[anim]) return false;
  const int n = s->an[anim], r = s->ar[anim];
  if (row >= r) row = r - 1;
  if (f >= n) f = n - 1;
  const SpriteFrame& fr = s->frames[anim][row * n + f];
  if (fr.img == 0xFFFF || fr.img >= s->nimages) return false;
  const Tex3DS_SubTexture* sub = &s->subs[fr.img * 2 + (fr.flip ? 1 : 0)];
  img = C2D_Image{&s->tex[s->page_of[fr.img]], sub};
  ox = fr.ox; oy = fr.oy; w = sub->width; h = sub->height;
  return true;
}

uint32_t SpriteBank::bytes() const { uint32_t b = 0; for (const auto& s : slots_) if (s.dex >= 0) b += s.bytes; return b; }
int SpriteBank::loaded() const { int n = 0; for (const auto& s : slots_) if (s.dex >= 0) n++; return n; }
void SpriteBank::touch_all_free() { for (auto& s : slots_) if (s.dex >= 0) unload(s); }

// ---------------- suelo ----------------

static inline u32 morton(u32 x, u32 y) {
  // Orden Z dentro de un mosaico de 8×8 (x en los bits pares, y en los impares).
  static const u8 tbl[8] = {0, 1, 4, 5, 16, 17, 20, 21};
  return tbl[x & 7] | (tbl[y & 7] << 1);
}

void upload_rgb565(C3D_Tex& tex, const uint32_t* rgba, int w, int h) {
  u16* dst = static_cast<u16*>(tex.data);
  for (int y = 0; y < h; ++y) {
    const int ty = h - 1 - y;            // la GPU guarda las filas de abajo arriba
    for (int x = 0; x < w; ++x) {
      const uint32_t c = rgba[y * w + x];  // bytes R,G,B,A
      const u16 v = u16(((c & 0xF8) << 8) | ((c >> 5) & 0x7E0) | ((c >> 19) & 0x1F));
      dst[((ty >> 3) * (w >> 3) + (x >> 3)) * 64 + morton(u32(x), u32(ty))] = v;
    }
  }
  C3D_TexFlush(&tex);
}

void ChunkCache::worker(void* arg) {
  ChunkCache& c = *static_cast<ChunkCache*>(arg);
  while (!c.quit_) {
    LightEvent_Wait(&c.ev_);
    if (c.quit_) break;
    const int j = c.job_;
    if (j >= 0 && !c.job_done_) {
      c.painter_.paint_all(c.buf_);
      upload_rgb565(c.slots_[j].tex, c.buf_, kChunkPx, kChunkPx);
      c.job_done_ = true;
    }
  }
}

void ChunkCache::shutdown() {
  if (!thr_) return;
  quit_ = true;
  LightEvent_Signal(&ev_);
  threadJoin(thr_, U64_MAX);
  threadFree(thr_);
  thr_ = nullptr;
}

void ChunkCache::init() {
  buf_ = static_cast<uint32_t*>(std::malloc(kChunkPx * kChunkPx * 4));
  LightEvent_Init(&ev_, RESET_ONESHOT);
  bool n3ds = false;
  APT_CheckNew3DS(&n3ds);
  if (!n3ds) APT_SetAppCpuTimeLimit(30);   // la vieja: el núcleo 1 se comparte con el sistema
  s32 prio = 0x30;
  svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
  thr_ = threadCreate(worker, this, 32 * 1024, prio + 1, n3ds ? 2 : 1, false);
  for (auto& s : slots_) {
    s.used = s.ready = s.painting = false;
    C3D_TexInit(&s.tex, kChunkPx, kChunkPx, GPU_RGB565);
    C3D_TexSetFilter(&s.tex, GPU_NEAREST, GPU_NEAREST);
    s.sub = {u16(kChunkPx), u16(kChunkPx), 0.0f, 1.0f, 1.0f, 0.0f};
    s.img = C2D_Image{&s.tex, &s.sub};
  }
}

void ChunkCache::clear() {
  // (si el hilo está pintando uno, termina y se descarta: cambia la generación)
  for (int i = 0; i < kSlots; ++i) { Slot& s = slots_[i]; if (i == job_) { s.gen++; s.used = false; s.ready = false; continue; } s.used = s.ready = s.painting = false; s.gen++; }
  painting_ = -1;
}

C2D_Image* ChunkCache::get(World& w, int32_t cx, int32_t cy, int biome) {
  Slot* victim = nullptr;
  for (auto& s : slots_) {
    if (s.used && s.cx == cx && s.cy == cy && s.biome == biome) {
      s.stamp = ++stamp_;
      return s.ready ? &s.img : nullptr;
    }
    if (s.painting) continue;
    if (!s.used) { if (!victim || victim->used) victim = &s; }
    else if (!victim || (victim->used && s.stamp < victim->stamp)) victim = &s;
  }
  if (!victim) return nullptr;
  (void)w;
  victim->used = true; victim->ready = false; victim->painting = false;
  victim->cx = cx; victim->cy = cy; victim->biome = biome; victim->row = 0;
  victim->stamp = ++stamp_;
  return nullptr;
}

void ChunkCache::work(World& w, int rows) {
  if (thr_) {
    if (job_ >= 0 && job_done_) {
      Slot& s = slots_[job_];
      if (s.gen == job_gen_) s.ready = true;
      s.painting = false;
      job_ = -1;
      job_done_ = false;
    }
    if (job_ < 0) {
      int best = -1;
      for (int i = 0; i < kSlots; ++i)
        if (slots_[i].used && !slots_[i].ready && (best < 0 || slots_[i].stamp > slots_[best].stamp)) best = i;
      if (best < 0) return;
      Slot& s = slots_[best];
      s.painting = true;
      painter_.begin(w, s.cx, s.cy, s.biome);   // (lee el terreno aquí, en el hilo principal)
      job_gen_ = s.gen;
      job_done_ = false;
      job_ = best;
      LightEvent_Signal(&ev_);
    }
    return;
  }
  while (rows > 0) {
    if (painting_ < 0) {
      // El pedido más reciente primero (el que se está viendo).
      int best = -1;
      for (int i = 0; i < kSlots; ++i)
        if (slots_[i].used && !slots_[i].ready && (best < 0 || slots_[i].stamp > slots_[best].stamp)) best = i;
      if (best < 0) return;
      painting_ = best;
      Slot& s = slots_[best];
      s.painting = true; s.row = 0;
      painter_.begin(w, s.cx, s.cy, s.biome);
    }
    Slot& s = slots_[painting_];
    const int n = rows < 16 - s.row ? rows : 16 - s.row;
    painter_.paint_rows(s.row, s.row + n, buf_);
    s.row += n;
    rows -= n;
    if (s.row >= 16) {
      upload_rgb565(s.tex, buf_, kChunkPx, kChunkPx);
      s.ready = true; s.painting = false;
      painting_ = -1;
    }
  }
}

int ChunkCache::pending() const { int n = 0; for (const auto& s : slots_) if (s.used && !s.ready) n++; return n; }

}  // namespace ps

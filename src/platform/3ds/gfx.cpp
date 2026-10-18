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
template <class T> bool rd(FILE* f, T& v) { return std::fread(&v, sizeof(T), 1, f) == 1; }
}  // namespace

bool SpriteBank::load(SpriteSet& s, int dex, bool shiny) {
  char path[64];
  std::snprintf(path, sizeof(path), "romfs:/spr/%d%s.fr", dex, shiny ? "s" : "");
  FILE* f = std::fopen(path, "rb");
  if (!f) return false;
  char magic[4];
  uint8_t npages = 0, pad = 0;
  uint16_t nimages = 0;
  if (std::fread(magic, 1, 4, f) != 4 || std::memcmp(magic, "PSF1", 4) || !rd(f, npages) || !rd(f, pad) || !rd(f, nimages) || npages > 2) {
    std::fclose(f);
    return false;
  }
  uint16_t pw[2] = {}, ph[2] = {};
  for (int p = 0; p < npages; ++p) { rd(f, pw[p]); rd(f, ph[p]); }
  s.nimages = nimages;
  s.subs = static_cast<Tex3DS_SubTexture*>(std::malloc(sizeof(Tex3DS_SubTexture) * nimages * 2));
  s.page_of = static_cast<uint8_t*>(std::malloc(nimages));
  for (int i = 0; i < nimages; ++i) {
    uint8_t page, z;
    uint16_t x, y, w, h;
    rd(f, page); rd(f, z); rd(f, x); rd(f, y); rd(f, w); rd(f, h);
    const float W = pw[page], H = ph[page];
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
    rd(f, s.an[a]); rd(f, s.ar[a]);
    const int n = s.an[a] * s.ar[a];
    s.frames[a] = n ? static_cast<SpriteFrame*>(std::malloc(sizeof(SpriteFrame) * n)) : nullptr;
    for (int k = 0; k < n; ++k) {
      uint16_t img; uint8_t flip, z; int16_t ox, oy;
      rd(f, img); rd(f, flip); rd(f, z); rd(f, ox); rd(f, oy);
      s.frames[a][k] = {img, flip, ox, oy};
    }
  }
  std::fclose(f);
  for (int p = 0; p < npages; ++p) {
    std::snprintf(path, sizeof(path), "romfs:/spr/%d%s_%d.t3x", dex, shiny ? "s" : "", p);
    FILE* t = std::fopen(path, "rb");
    if (!t) { s.npages = p; unload(s); return false; }
    s.t3x[p] = Tex3DS_TextureImportStdio(t, &s.tex[p], nullptr, false);
    std::fclose(t);
    if (!s.t3x[p]) { s.npages = p; unload(s); return false; }
    C3D_TexSetFilter(&s.tex[p], GPU_NEAREST, GPU_NEAREST);
    s.bytes += s.tex[p].size;
  }
  s.npages = npages;
  s.dex = int16_t(dex);
  s.shiny = shiny;
  loads++;
  return true;
}

void SpriteBank::unload(SpriteSet& s) {
  for (int p = 0; p < s.npages; ++p) if (s.t3x[p]) { Tex3DS_TextureFree(s.t3x[p]); C3D_TexDelete(&s.tex[p]); s.t3x[p] = nullptr; }
  for (auto& fr : s.frames) { std::free(fr); fr = nullptr; }
  std::free(s.subs); s.subs = nullptr;
  std::free(s.page_of); s.page_of = nullptr;
  s.dex = -1; s.npages = 0; s.bytes = 0;
}

SpriteSet* SpriteBank::get(int dex, bool shiny) {
  SpriteSet* victim = nullptr;
  for (auto& s : slots_) {
    if (s.dex == dex && s.shiny == shiny) { s.stamp = ++stamp_; return &s; }
    if (s.dex < 0) { if (!victim || victim->dex >= 0) victim = &s; }
    else if (!victim || (victim->dex >= 0 && s.stamp < victim->stamp)) victim = &s;
  }
  if (victim->dex >= 0) unload(*victim);
  if (!load(*victim, dex, shiny)) return nullptr;
  victim->stamp = ++stamp_;
  return victim;
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

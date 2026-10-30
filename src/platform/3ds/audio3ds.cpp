// audio3ds.cpp — ver audio3ds.h.
#include "platform/3ds/crash3ds.h"
#include "platform/3ds/audio3ds.h"

#include <3ds.h>
#include <tremor/ivorbisfile.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ps {
namespace audio {

namespace {
constexpr int MUSIC_CH = 0;
constexpr int SFX_CH0 = 1, SFX_CHN = 16;
// 8 × 4096 muestras: casi un segundo de margen (con 3 se cortaba si la SD
// estaba ocupada escribiendo un momento).
constexpr int NBUF = 8, BUF_FRAMES = 4096;

bool g_ok = false;
float g_music_vol = 0.7f, g_sfx_vol = 0.8f;

// ---------------- efectos ----------------
struct Clip { char name[24]; uint32_t off, len; };
Clip* g_clips = nullptr;
int g_nclips = 0;
int16_t* g_pcm = nullptr;     // en memoria lineal (la lee el DSP)
int g_sfx_rate = 22050;
ndspWaveBuf g_sfx_wb[SFX_CHN];
uint64_t g_last_ms[256];
int g_next_ch = 0;
float g_xp_t = -1;
int g_xp_combo = 0;

int find(const char* name) {
  for (int i = 0; i < g_nclips; ++i) if (!std::strcmp(g_clips[i].name, name)) return i;
  return -1;
}

// ---------------- música ----------------
OggVorbis_File g_vf;
bool g_vf_open = false;
int16_t* g_mbuf = nullptr;
ndspWaveBuf g_mwb[NBUF];
int g_cur_track = MU_NONE, g_want_track = MU_NONE;
float g_fade = 1;             // ganancia de fundido
Thread g_thread = nullptr;
LightEvent g_event;
volatile bool g_quit = false;
LightLock g_lock;

const char* const kTrackName[MU_COUNT] = {"forest", "cave", "volcano", "boss", "rift", "judgment", "village"};

bool open_track(int t) {
  if (g_vf_open) { ov_clear(&g_vf); g_vf_open = false; }
  // (Halloween: kHwTrack + t, la versión tenebrosa; si no está, la de siempre)
  const bool hw = t >= kHwTrack;
  if (hw) t -= kHwTrack;
  if (t < 0 || t >= MU_COUNT) return false;
  char path[64];
  std::snprintf(path, sizeof(path), "romfs:/snd/music_%s%s.ogg", kTrackName[t], hw ? "_hw" : "");
  FILE* f = std::fopen(path, "rb");
  if (!f && hw) { std::snprintf(path, sizeof(path), "romfs:/snd/music_%s.ogg", kTrackName[t]); f = std::fopen(path, "rb"); }
  if (!f) return false;
  if (ov_open(f, &g_vf, nullptr, 0) < 0) { std::fclose(f); return false; }
  g_vf_open = true;
  return true;
}

// Rellena un búfer con música (vuelve al principio al acabar: bucle sin corte).
void fill(ndspWaveBuf& wb) {
  int16_t* dst = reinterpret_cast<int16_t*>(wb.data_pcm16);
  int got = 0;
  const int want = BUF_FRAMES * 2 * 2;   // bytes (estéreo, 16 bits)
  while (g_vf_open && got < want) {
    int bs = 0;
    const long n = ov_read(&g_vf, reinterpret_cast<char*>(dst) + got, want - got, &bs);
    if (n <= 0) { if (ov_pcm_seek(&g_vf, 0) != 0) break; continue; }
    got += int(n);
  }
  if (got < want) std::memset(reinterpret_cast<char*>(dst) + got, 0, want - got);
  wb.nsamples = BUF_FRAMES;
  DSP_FlushDataCache(dst, want);
  ndspChnWaveBufAdd(MUSIC_CH, &wb);
}

void start_track(int t);
// Cambio de tema pedido por el juego: lo hace el hilo de la música (abrir el
// .ogg y decodificar los primeros búferes paraba el juego un momento).
volatile int g_req_track = MU_NONE;
volatile bool g_has_req = false;

void music_thread(void*) {
  crash3ds::guard("música");
  while (!g_quit) {
    if (g_has_req) { g_has_req = false; start_track(g_req_track); }
    LightLock_Lock(&g_lock);
    if (g_vf_open) for (auto& wb : g_mwb) if (wb.status == NDSP_WBUF_DONE || wb.status == NDSP_WBUF_FREE) fill(wb);
    LightLock_Unlock(&g_lock);
    LightEvent_Wait(&g_event);
  }
}

void on_dsp(void*) { LightEvent_Signal(&g_event); }

void apply_music_mix() {
  float mix[12] = {};
  mix[0] = mix[1] = g_music_vol * g_fade;
  ndspChnSetMix(MUSIC_CH, mix);
}

void start_track(int t) {
  LightLock_Lock(&g_lock);
  ndspChnWaveBufClear(MUSIC_CH);
  g_cur_track = t;
  if (t >= 0 && open_track(t)) {
    const int rate = int(ov_info(&g_vf, -1)->rate);
    ndspChnReset(MUSIC_CH);
    ndspChnSetInterp(MUSIC_CH, NDSP_INTERP_LINEAR);
    ndspChnSetRate(MUSIC_CH, float(rate));
    ndspChnSetFormat(MUSIC_CH, NDSP_FORMAT_STEREO_PCM16);
    for (int i = 0; i < NBUF; ++i) {
      std::memset(&g_mwb[i], 0, sizeof(ndspWaveBuf));
      g_mwb[i].data_vaddr = g_mbuf + i * BUF_FRAMES * 2;
      g_mwb[i].status = NDSP_WBUF_FREE;
      fill(g_mwb[i]);
    }
  } else if (g_vf_open) { ov_clear(&g_vf); g_vf_open = false; }
  LightLock_Unlock(&g_lock);
  apply_music_mix();
}
}  // namespace

bool init() {
  if (R_FAILED(ndspInit())) return false;
  g_ok = true;
  ndspSetOutputMode(NDSP_OUTPUT_STEREO);
  // Efectos.
  FILE* f = std::fopen("romfs:/snd/sfx.bin", "rb");
  if (f) {
    char magic[4];
    uint16_t n = 0, rate = 0;
    if (std::fread(magic, 1, 4, f) == 4 && !std::memcmp(magic, "PSX1", 4) && std::fread(&n, 2, 1, f) == 1 && std::fread(&rate, 2, 1, f) == 1) {
      g_nclips = n;
      g_sfx_rate = rate;
      g_clips = static_cast<Clip*>(std::malloc(sizeof(Clip) * n));
      if (std::fread(g_clips, sizeof(Clip), n, f) == n) {
        uint32_t total = 0;
        for (int i = 0; i < n; ++i) total = std::max(total, g_clips[i].off + g_clips[i].len);
        g_pcm = static_cast<int16_t*>(linearAlloc(total * 2));
        if (g_pcm && std::fread(g_pcm, 2, total, f) == total) DSP_FlushDataCache(g_pcm, total * 2);
        else g_nclips = 0;
      } else g_nclips = 0;
    }
    std::fclose(f);
  }
  for (int c = 0; c < SFX_CHN; ++c) {
    ndspChnReset(SFX_CH0 + c);
    ndspChnSetInterp(SFX_CH0 + c, NDSP_INTERP_LINEAR);
    ndspChnSetFormat(SFX_CH0 + c, NDSP_FORMAT_MONO_PCM16);
    std::memset(&g_sfx_wb[c], 0, sizeof(ndspWaveBuf));
  }
  // Música: búferes y el hilo que los rellena (despierta con el DSP).
  g_mbuf = static_cast<int16_t*>(linearAlloc(NBUF * BUF_FRAMES * 2 * 2));
  LightEvent_Init(&g_event, RESET_ONESHOT);
  LightLock_Init(&g_lock);
  ndspSetCallback(on_dsp, nullptr);
  s32 prio = 0x30;
  svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
  // (128 KB: Vorbis pide mucha pila al decodificar y, en el .cia, leer del romfs
  // pasa por más capas del sistema; con 32 KB el .cia se caía con «stack overflow»)
  g_thread = threadCreate(music_thread, nullptr, 128 * 1024, prio - 1, -2, false);
  return true;
}

void exit() {
  if (!g_ok) return;
  g_quit = true;
  LightEvent_Signal(&g_event);
  if (g_thread) { threadJoin(g_thread, U64_MAX); threadFree(g_thread); }
  ndspChnWaveBufClear(MUSIC_CH);
  if (g_vf_open) ov_clear(&g_vf);
  ndspExit();
  if (g_pcm) linearFree(g_pcm);
  if (g_mbuf) linearFree(g_mbuf);
  std::free(g_clips);
  g_ok = false;
}

bool ok() { return g_ok; }

void set_volume(int music, int sfx) {
  g_music_vol = music / 100.0f * 0.9f;
  g_sfx_vol = sfx / 100.0f;
  if (g_ok) apply_music_mix();
}

void music(int track) { g_want_track = track; }

void update(float dt) {
  if (!g_ok) return;
  // Fundido de 0,7 s al cambiar de tema (como la web).
  if (g_want_track != g_cur_track) {
    g_fade -= dt / 0.7f;
    if (g_fade <= 0 || g_cur_track == MU_NONE) {
      g_fade = 0;
      if (g_thread) { g_cur_track = g_want_track; g_req_track = g_want_track; g_has_req = true; LightEvent_Signal(&g_event); }
      else start_track(g_want_track);
    }
  } else if (g_fade < 1) {
    g_fade = std::fmin(1.0f, g_fade + dt / 0.8f);
  }
  apply_music_mix();
}

void play(const char* name, float vol, float pitch) {
  if (!g_ok || !g_nclips || g_sfx_vol <= 0) return;
  const int id = find(name);
  if (id < 0) return;
  // Separación mínima entre repeticiones (MIN_GAP de audio.js, en ms).
  static const struct { const char* n; int ms; } GAP[] = {{"hit", 45}, {"faint", 60}, {"xp", 35}, {"coin", 50}, {"grass", 70}, {"hover", 40}, {"break", 60}, {"judge", 80}};
  int gap = !std::strncmp(name, "shot_", 5) ? 70 : 0;
  for (const auto& gp : GAP) if (!std::strcmp(gp.n, name)) gap = gp.ms;
  const uint64_t now = osGetTime();
  if (id < 256 && gap && now - g_last_ms[id] < uint64_t(gap)) return;
  if (id < 256) g_last_ms[id] = now;
  // Canal libre (o el siguiente en la rueda).
  int ch = -1;
  for (int k = 0; k < SFX_CHN; ++k) {
    const int c = (g_next_ch + k) % SFX_CHN;
    if (!ndspChnIsPlaying(SFX_CH0 + c)) { ch = c; break; }
  }
  if (ch < 0) ch = g_next_ch;
  g_next_ch = (ch + 1) % SFX_CHN;
  const int hw = SFX_CH0 + ch;
  ndspChnWaveBufClear(hw);
  ndspChnSetRate(hw, g_sfx_rate * pitch);
  float mix[12] = {};
  mix[0] = mix[1] = g_sfx_vol * vol;
  ndspChnSetMix(hw, mix);
  ndspWaveBuf& wb = g_sfx_wb[ch];
  std::memset(&wb, 0, sizeof(wb));
  wb.data_vaddr = g_pcm + g_clips[id].off;
  wb.nsamples = g_clips[id].len;
  ndspChnWaveBufAdd(hw, &wb);
}

void play_game(const SfxEvent& e) {
  static const char* const NAME[SFX_COUNT] = {
    nullptr, nullptr, "hit", "hurt", "faint", "bossDown", "xp", "coin", "heal", "break", "confirm", "shiny", "legend",
    "levelup", "select", "pause", "back", "gameover", "biome", "surge", "boss", "shinyGet", "grass", "chest", "rift",
    "judge", "quake", "toll", "descend", "land"};
  if (e.id == SFX_SHOT) {
    // (las familias nuevas sin sonido propio suenan como un proyectil, el caso por defecto de audio.js)
    static const char* const K[K_COUNT] = {"pr", "me", "be", "or", "no", "au", "pr", "be", "no", "no", "bu", "me", "au", "pr",
                                           "pr", "pr", "pr", "pr", "pr", "pr", "pr", "pr", "bu"};
    char b[24];
    std::snprintf(b, sizeof(b), "shot_%s_%s", K[e.kind >= 0 && e.kind < K_COUNT ? e.kind : 0], kTypeId[e.type >= 0 && e.type < T_COUNT ? e.type : 0]);
    play(b);
    return;
  }
  if (e.id == SFX_XP) {
    // Cada orbe seguido sube un semitono (hasta 2 octavas), como en la web.
    const float now = float(osGetTime()) / 1000.0f;
    g_xp_combo = now - g_xp_t < 0.45f ? std::min(g_xp_combo + 1, 24) : 0;
    g_xp_t = now;
    play("xp", 1, std::pow(2.0f, g_xp_combo / 12.0f));
    return;
  }
  if (e.id > 0 && e.id < SFX_COUNT && NAME[e.id]) play(NAME[e.id]);
}

}  // namespace audio
}  // namespace ps

// jsbench — banco de pruebas: el juego web ORIGINAL (JS) corriendo en la 3DS
// con QuickJS, sin pantalla (canvas falso) y con el bot de tools/web/bot.js.
// Mide cuánto tarda cada fotograma de la partida en la CPU de la consola.
// sd:/3ds/jsbench.txt = "segundos cada" (por defecto 120 20); el resultado va a
// la pantalla y a sd:/3ds/jsbench.log. Se compila con tools/web/jsbench/build.sh.
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "quickjs.h"

u32 __stacksize__ = 1024 * 1024;
static FILE* g_log;
static JSValue js_print(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  for (int i = 0; i < argc; i++) {
    const char* s = JS_ToCString(ctx, argv[i]);
    if (s) { printf("%s%s", i ? " " : "", s); if (g_log) fprintf(g_log, "%s%s", i ? " " : "", s); JS_FreeCString(ctx, s); }
  }
  printf("\n"); if (g_log) { fprintf(g_log, "\n"); fflush(g_log); }
  gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank();
  return JS_UNDEFINED;
}
static JSValue js_clock(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_NewFloat64(ctx, (double)svcGetSystemTick() * 1000.0 / SYSCLOCK_ARM11);
}
static char* slurp(const char* path, size_t* n) {
  FILE* f = fopen(path, "rb"); if (!f) return NULL;
  fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET);
  char* b = malloc(*n + 1); fread(b, 1, *n, f); b[*n] = 0; fclose(f); return b;
}
static void dump_exc(JSContext* ctx) {
  JSValue e = JS_GetException(ctx);
  const char* s = JS_ToCString(ctx, e);
  printf("EXCEPCION: %s\n", s ? s : "?"); if (g_log) fprintf(g_log, "EXCEPCION: %s\n", s ? s : "?");
  JSValue st = JS_GetPropertyStr(ctx, e, "stack");
  const char* ss = JS_ToCString(ctx, st);
  if (ss) { printf("%s\n", ss); if (g_log) fprintf(g_log, "%s\n", ss); JS_FreeCString(ctx, ss); }
  JS_FreeValue(ctx, st); if (s) JS_FreeCString(ctx, s); JS_FreeValue(ctx, e);
}
int main(void) {
  bool n3ds = false; APT_CheckNew3DS(&n3ds);
  osSetSpeedupEnable(true);
  gfxInitDefault(); consoleInit(GFX_TOP, NULL);
  romfsInit();
  mkdir("sdmc:/3ds", 0777);
  g_log = fopen("sdmc:/3ds/jsbench.log", "w");
  int secs = 120, rep = 20;
  FILE* cf = fopen("sdmc:/3ds/jsbench.txt", "r"); if (cf) { fscanf(cf, "%d %d", &secs, &rep); fclose(cf); }
  char hdr[128]; snprintf(hdr, sizeof hdr, "jsbench: %s, %d s de partida, memoria libre %lu KB", n3ds ? "New 3DS" : "3DS", secs, (unsigned long)(osGetMemRegionFree(MEMREGION_APPLICATION) / 1024));
  printf("%s\n", hdr); if (g_log) fprintf(g_log, "%s\n", hdr);
  JSRuntime* rt = JS_NewRuntime();
  JS_SetMaxStackSize(rt, 512 * 1024);
  JSContext* ctx = JS_NewContext(rt);
  JSValue glob = JS_GetGlobalObject(ctx);
  JS_SetPropertyStr(ctx, glob, "print", JS_NewCFunction(ctx, js_print, "print", 1));
  JS_SetPropertyStr(ctx, glob, "__clock", JS_NewCFunction(ctx, js_clock, "__clock", 0));
  JS_FreeValue(ctx, glob);
  size_t n = 0; char* src = slurp("romfs:/bundle.js", &n);
  if (!src) { printf("no hay romfs:/bundle.js\n"); }
  else {
    u64 t0 = svcGetSystemTick();
    JSValue r = JS_Eval(ctx, src, n, "bundle.js", JS_EVAL_TYPE_GLOBAL);
    double ms = (double)(svcGetSystemTick() - t0) * 1000.0 / SYSCLOCK_ARM11;
    if (JS_IsException(r)) dump_exc(ctx);
    JS_FreeValue(ctx, r); free(src);
    JSMemoryUsage mu; JS_ComputeMemoryUsage(rt, &mu);
    char b[160]; snprintf(b, sizeof b, "cargar el juego (%u KB de JS): %.0f ms; memoria JS %lld KB; libre %lu KB", (unsigned)(n / 1024), ms, (long long)(mu.memory_used_size / 1024), (unsigned long)(osGetMemRegionFree(MEMREGION_APPLICATION) / 1024));
    printf("%s\n", b); if (g_log) { fprintf(g_log, "%s\n", b); fflush(g_log); }
    char cmd[96]; snprintf(cmd, sizeof cmd, "__setup(25, 12345); __bench(%d, %d, __clock);", secs, rep);
    r = JS_Eval(ctx, cmd, strlen(cmd), "<bench>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(r)) dump_exc(ctx);
    JS_FreeValue(ctx, r);
    JS_ComputeMemoryUsage(rt, &mu);
    snprintf(b, sizeof b, "memoria JS al final %lld KB", (long long)(mu.memory_used_size / 1024));
    printf("%s\n", b); if (g_log) fprintf(g_log, "%s\n", b);
  }
  if (g_log) { fprintf(g_log, "FIN\n"); fclose(g_log); }
  printf("FIN. START para salir.\n");
  while (aptMainLoop()) { hidScanInput(); if (hidKeysDown() & KEY_START) break; gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank(); }
  JS_FreeContext(ctx); JS_FreeRuntime(rt);
  romfsExit(); gfxExit();
  return 0;
}

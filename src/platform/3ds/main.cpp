// main.cpp — arranque en la 3DS. De momento sólo muestra texto en las dos
// pantallas (esqueleto de la Fase 0).
#include <3ds.h>
#include <citro2d.h>

#include "core/version.h"

int main() {
  gfxInitDefault();
  C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
  C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
  C2D_Prepare();

  C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
  C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

  C2D_TextBuf buf = C2D_TextBufNew(256);
  C2D_Text title, info;
  C2D_TextParse(&title, buf, ps::kGameTitle);
  C2D_TextParse(&info, buf, "START: salir");
  C2D_TextOptimize(&title);
  C2D_TextOptimize(&info);

  while (aptMainLoop()) {
    hidScanInput();
    if (hidKeysDown() & KEY_START) break;

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TargetClear(top, C2D_Color32(0x1b, 0x2a, 0x3a, 0xff));
    C2D_SceneBegin(top);
    C2D_DrawText(&title, C2D_WithColor, 120.0f, 100.0f, 0.5f, 1.0f, 1.0f,
                 C2D_Color32(0xff, 0xd3, 0x3d, 0xff));
    C2D_TargetClear(bottom, C2D_Color32(0x10, 0x18, 0x22, 0xff));
    C2D_SceneBegin(bottom);
    C2D_DrawText(&info, C2D_WithColor, 110.0f, 110.0f, 0.5f, 0.7f, 0.7f,
                 C2D_Color32(0xff, 0xff, 0xff, 0xff));
    C3D_FrameEnd(0);
  }

  C2D_TextBufDelete(buf);
  C2D_Fini();
  C3D_Fini();
  gfxExit();
  return 0;
}

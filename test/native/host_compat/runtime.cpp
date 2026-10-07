#include "cstddef"
#include "citro2d.h"

static char s_heap[131072];
static size_t s_heap_pos = 0;

void operator delete(void* ptr) noexcept {
    (void)ptr;
}

void operator delete(void* ptr, unsigned long sz) noexcept {
    (void)ptr;
    (void)sz;
}

void* operator new(unsigned long sz) {
    if (s_heap_pos + sz > sizeof(s_heap)) return (void*)s_heap;
    void* p = (void*)&s_heap[s_heap_pos];
    s_heap_pos = (s_heap_pos + sz + 7) & ~7;
    return p;
}

extern "C" {

void* __dso_handle = nullptr;
int __cxa_atexit(void (*func)(void*), void* arg, void* dso_handle) {
    (void)func; (void)arg; (void)dso_handle;
    return 0;
}

bool C3D_Init(size_t cmdBufSize) { (void)cmdBufSize; return true; }
void C3D_Fini(void) {}
bool C3D_FrameBegin(u8 flags) { (void)flags; return true; }
void C3D_FrameEnd(u8 flags) { (void)flags; }
void C3D_TexSetFilter(C3D_Tex* tex, GPU_TEXTURE_FILTER_PARAM magFilter, GPU_TEXTURE_FILTER_PARAM minFilter) {
    (void)tex; (void)magFilter; (void)minFilter;
}

bool C2D_Init(size_t maxObjects) { (void)maxObjects; return true; }
void C2D_Fini(void) {}
void C2D_Prepare(void) {}

static int s_dummyTopTarget = 1;
static int s_dummyBottomTarget = 2;

C3D_RenderTarget* C2D_CreateScreenTarget(gfxScreen_t screen, gfx3dSide_t side) {
    (void)side;
    if (screen == GFX_TOP) return (C3D_RenderTarget*)&s_dummyTopTarget;
    return (C3D_RenderTarget*)&s_dummyBottomTarget;
}

void C2D_SceneBegin(C3D_RenderTarget* target) { (void)target; }
void C2D_TargetClear(C3D_RenderTarget* target, u32 clr) { (void)target; (void)clr; }
void C2D_DrawRectSolid(float x, float y, float z, float w, float h, u32 clr) {
    (void)x; (void)y; (void)z; (void)w; (void)h; (void)clr;
}
void C2D_DrawImageAt(C2D_Image img, float x, float y, float z, const C2D_ImageTint* tint, float scaleX, float scaleY) {
    (void)img; (void)x; (void)y; (void)z; (void)tint; (void)scaleX; (void)scaleY;
}
void C2D_DrawImageAtRotated(C2D_Image img, float x, float y, float z, float rotation, const C2D_ImageTint* tint, float scaleX, float scaleY) {
    (void)img; (void)x; (void)y; (void)z; (void)rotation; (void)tint; (void)scaleX; (void)scaleY;
}
void C2D_PlainImageTint(C2D_ImageTint* tint, u32 color, float blend) {
    if (tint) {
        tint->solid[0] = color;
        tint->solid[1] = color;
        tint->solid[2] = color;
        tint->solid[3] = color;
    }
    (void)blend;
}
static int s_dummyTextBuf = 1;
C2D_TextBuf C2D_TextBufNew(size_t maxGlyphs) { (void)maxGlyphs; return (C2D_TextBuf)&s_dummyTextBuf; }
void C2D_TextBufDelete(C2D_TextBuf buf) { (void)buf; }
void C2D_TextParse(C2D_Text* text, C2D_TextBuf buf, const char* str) { (void)text; (void)buf; (void)str; }
void C2D_TextOptimize(const C2D_Text* text) { (void)text; }
void C2D_DrawText(const C2D_Text* text, u32 flags, float x, float y, float z, float scaleX, float scaleY, ...) {
    (void)text; (void)flags; (void)x; (void)y; (void)z; (void)scaleX; (void)scaleY;
}

static int s_dummySheet = 42;
static C3D_Tex s_dummyTex = {};
static Tex3DS_SubTexture s_dummySub = { 64, 64, 0, 1, 1, 0 };

C2D_SpriteSheet C2D_SpriteSheetLoad(const char* filename) {
    if (!filename || filename[0] == '\0') return nullptr;
    return (C2D_SpriteSheet)&s_dummySheet;
}
C2D_Image C2D_SpriteSheetGetImage(C2D_SpriteSheet sheet, size_t index) {
    (void)sheet; (void)index;
    C2D_Image img = { &s_dummyTex, &s_dummySub };
    return img;
}
void C2D_SpriteSheetFree(C2D_SpriteSheet sheet) { (void)sheet; }

static int s_dummyFont = 43;
static C2D_GlyphInfo s_dummyGlyphInfo={16};
static C2D_FontInfo s_dummyFontInfo = { 16.0f,16,&s_dummyGlyphInfo };
C2D_Font C2D_FontLoad(const char* filename) { (void)filename; return (C2D_Font)&s_dummyFont; }
void C2D_FontFree(C2D_Font font) { (void)font; }
void C2D_TextFontParse(C2D_Text* text, C2D_Font font, C2D_TextBuf buf, const char* str) { (void)text; (void)font; (void)buf; (void)str; }
const C2D_FontInfo* C2D_FontGetInfo(C2D_Font font) { (void)font; return &s_dummyFontInfo; }
void C2D_TextGetDimensions(const C2D_Text* text, float scaleX, float scaleY, float* outWidth, float* outHeight) {
    (void)text; (void)scaleX; (void)scaleY;
    if (outWidth) *outWidth = 64.0f;
    if (outHeight) *outHeight = 16.0f;
}

}

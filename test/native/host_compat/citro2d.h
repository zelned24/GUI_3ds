#pragma once

#include "3ds.h"
#include <cstddef>

#define C3D_DEFAULT_CMDBUF_SIZE 0x40000
#define C3D_FRAME_SYNCDRAW 1

typedef struct {
    void* data;
} C3D_Tex;

typedef enum {
    GPU_NEAREST = 0x0,
    GPU_LINEAR = 0x1,
} GPU_TEXTURE_FILTER_PARAM;

void C3D_TexSetFilter(C3D_Tex* tex, GPU_TEXTURE_FILTER_PARAM magFilter, GPU_TEXTURE_FILTER_PARAM minFilter);

typedef struct {
    u16 width;
    u16 height;
    float left;
    float top;
    float right;
    float bottom;
} Tex3DS_SubTexture;

typedef struct {
    C3D_Tex* tex;
    const Tex3DS_SubTexture* subtex;
} C2D_Image;

typedef struct {
    u32 solid[4];
} C2D_ImageTint;

typedef void* C3D_RenderTarget;

typedef struct {
    void* buf;
    float width;
    float height;
} C2D_Text;

typedef void* C2D_TextBuf;

typedef void* C2D_SpriteSheet;
typedef void* C2D_Font;

typedef struct {
    float height;
} C2D_FontInfo;

#ifdef __cplusplus
extern "C" {
#endif

bool C3D_Init(size_t cmdBufSize);
void C3D_Fini(void);
bool C3D_FrameBegin(u8 flags);
void C3D_FrameEnd(u8 flags);

bool C2D_Init(size_t maxObjects);
void C2D_Fini(void);
void C2D_Prepare(void);
C3D_RenderTarget* C2D_CreateScreenTarget(gfxScreen_t screen, gfx3dSide_t side);
void C2D_SceneBegin(C3D_RenderTarget* target);
void C2D_TargetClear(C3D_RenderTarget* target, u32 clr);
void C2D_DrawRectSolid(float x, float y, float z, float w, float h, u32 clr);
void C2D_DrawImageAt(C2D_Image img, float x, float y, float z, const C2D_ImageTint* tint, float scaleX, float scaleY);
void C2D_DrawImageAtRotated(C2D_Image img, float x, float y, float z, float rotation, const C2D_ImageTint* tint, float scaleX, float scaleY);
void C2D_PlainImageTint(C2D_ImageTint* tint, u32 color, float blend);
#define C2D_WithColor (1 << 0)
C2D_TextBuf C2D_TextBufNew(size_t maxGlyphs);
void C2D_TextBufDelete(C2D_TextBuf buf);
void C2D_TextParse(C2D_Text* text, C2D_TextBuf buf, const char* str);
void C2D_TextOptimize(const C2D_Text* text);
void C2D_DrawText(const C2D_Text* text, u32 flags, float x, float y, float z, float scaleX, float scaleY, ...);

C2D_SpriteSheet C2D_SpriteSheetLoad(const char* filename);
C2D_Image C2D_SpriteSheetGetImage(C2D_SpriteSheet sheet, size_t index);
void C2D_SpriteSheetFree(C2D_SpriteSheet sheet);

C2D_Font C2D_FontLoad(const char* filename);
void C2D_FontFree(C2D_Font font);
void C2D_TextFontParse(C2D_Text* text, C2D_Font font, C2D_TextBuf buf, const char* str);
const C2D_FontInfo* C2D_FontGetInfo(C2D_Font font);
void C2D_TextGetDimensions(const C2D_Text* text, float scaleX, float scaleY, float* outWidth, float* outHeight);

#ifdef __cplusplus
}
#endif

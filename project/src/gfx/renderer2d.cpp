#include "gfx/renderer2d.hpp"
#include "gfx/ImageTintPolicy.hpp"
#include "screens/SceneAssets.hpp"
#include "content/WindowTexture.hpp"
#if !defined(__wasm__)
#include "runtime/RuntimeAssetManager.hpp"
#endif
#include <cmath>

#if defined(__arm__) || defined(__3DS__) || defined(_3DS)
// Citro2D C2D_DrawImageAtRotatedScaled bridge to real C2D_DrawImageAtRotated
static inline void C2D_DrawImageAtRotatedScaled(
    C2D_Image img,
    float x, float y,
    float depth,
    float angle,
    const C2D_ImageTint* tint,
    float scaleX, float scaleY
) {
    C2D_DrawImageAtRotated(img, x, y, depth, angle, tint, scaleX, scaleY);
}
#endif

Renderer2D::Renderer2D()
    : m_topTarget(nullptr)
    , m_bottomTarget(nullptr)
    , m_currentTarget(nullptr)
    , m_textBuf(nullptr)
    , m_initialized(false)
    , m_frameActive(false)
{
}

Renderer2D::~Renderer2D() {
    fini();
}

bool Renderer2D::init(size_t maxObjects) {
    if (m_initialized) return true;
    m_initError=nullptr;

    // 1. Initialize Citro3D and Citro2D
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
        m_initError="Citro3D initialization failed";
        return false;
    }
    if (!C2D_Init(maxObjects)) {
        m_initError="Citro2D initialization failed";
        C3D_Fini();
        return false;
    }
    m_initialized=true; // fini must release partial initialization after this point.
    C2D_Prepare();

    // 2. Create hardware render targets for Top (400x240) and Bottom (320x240)
    m_topTarget = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    m_bottomTarget = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    if (!m_topTarget || !m_bottomTarget) {
        m_initError="Dual-screen render target allocation failed";
        fini();
        return false;
    }

    // 3. Pre-allocate static text buffer to eliminate dynamic allocation per frame (Requirement 36)
    m_textBuf = C2D_TextBufNew(1024);
    if(!m_textBuf) { m_initError="Text buffer allocation failed"; fini(); return false; }
#if defined(__arm__) || defined(__3DS__) || defined(_3DS)
    // Physical font is produced from the pinned PokéRogue TTF by mkbcfnt.
    m_gameFont = C2D_FontLoad("romfs:/presentation/fonts/emerald.bcfnt");
    if(!m_gameFont) {
        m_initError="Missing or invalid romfs:/presentation/fonts/emerald.bcfnt";
        fini();return false;
    }
    C2D_FontSetFilter(m_gameFont,GPU_NEAREST,GPU_NEAREST);
    m_window = C2D_SpriteSheetLoad(Pokerogue3DS::kWindowTexturePath);
    if(!m_window) { m_initError="Missing or invalid default window texture"; fini();return false; }
    const auto windowImage=C2D_SpriteSheetGetImage(m_window,0);
    if(!windowImage.tex || !windowImage.subtex || windowImage.subtex->width!=24 || windowImage.subtex->height!=24) {
        m_initError="Invalid default window texture dimensions";fini();return false;
    }
    C3D_TexSetFilter(windowImage.tex,GPU_NEAREST,GPU_NEAREST);
    m_windowStyle=Pokerogue3DS::kWindowTextures[0].id;
#endif

#if !defined(__wasm__)
    // 4. Initialize global RuntimeAssetManager
    Citro2D::getRuntimeAssetManager().init();
#endif

    m_initialized = true;
    return true;
}

void Renderer2D::fini() {
    if (!m_initialized) return;

    if (m_frameActive) {
        endFrame();
    }

    if (m_textBuf) {
        C2D_TextBufDelete(m_textBuf);
        m_textBuf = nullptr;
    }

#if !defined(__wasm__)
    Citro2D::getRuntimeAssetManager().fini();
#endif

#if defined(__arm__) || defined(__3DS__) || defined(_3DS)
    if (m_gameFont) { C2D_FontFree(m_gameFont); m_gameFont = nullptr; }
    if (m_window) { C2D_SpriteSheetFree(m_window); m_window = nullptr; }
#endif
    C2D_Fini();
    C3D_Fini();

    m_topTarget = nullptr;
    m_bottomTarget = nullptr;
    m_currentTarget = nullptr;
    m_initialized = false;
}

void Renderer2D::beginFrame() {
    if (!m_initialized || m_frameActive) return;
    if (m_textBuf) C2D_TextBufClear(m_textBuf);
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    m_frameActive = true;
}

void Renderer2D::endFrame() {
    if (!m_frameActive) return;
    C3D_FrameEnd(0);
    m_frameActive = false;
    m_currentTarget = nullptr;
}

void Renderer2D::beginTop() {
    if (!m_frameActive) beginFrame();
    m_currentTarget = m_topTarget;
    C2D_SceneBegin(m_topTarget);
}

void Renderer2D::beginBottom() {
    if (!m_frameActive) beginFrame();
    m_currentTarget = m_bottomTarget;
    C2D_SceneBegin(m_bottomTarget);
}

void Renderer2D::clear(uint32_t color) {
    if (m_currentTarget) {
        C2D_TargetClear(m_currentTarget, color);
    }
}

void Renderer2D::drawRect(
    float x, float y,
    float width, float height,
    uint32_t color,
    float opacity
) {
    if (!m_currentTarget || opacity <= 0.001f) return;

    // Modulate alpha
    uint32_t a = (color >> 24) & 0xFF;
    a = static_cast<uint32_t>(a * opacity);
    uint32_t finalColor = (color & 0x00FFFFFF) | (a << 24);

    C2D_DrawRectSolid(x, y, 0.5f, width, height, finalColor);
}

void Renderer2D::drawImageDirect(
    C2D_Image img,
    float x, float y,
    float width, float height,
    float rotation,
    float opacity,
    bool flipX,
    bool flipY,
    uint32_t tintColor
) {
    if (!m_currentTarget || opacity <= 0.001f || !img.tex || !img.subtex
        || !img.subtex->width || !img.subtex->height) return;

    // Force nearest-neighbor sampling on PICA200 GPU to preserve crisp pixel art
    C3D_TexSetFilter(img.tex, GPU_NEAREST, GPU_NEAREST);

    float scaleX = width / img.subtex->width;
    float scaleY = height / img.subtex->height;
    if (flipX) scaleX = -scaleX;
    if (flipY) scaleY = -scaleY;

    // Configure tint & alpha modulation using official Citro2D API
    C2D_ImageTint tint;
    uint32_t a = (tintColor >> 24) & 0xFF;
    a = static_cast<uint32_t>(a * opacity);
    uint32_t modulatedTint = (tintColor & 0x00FFFFFF) | (a << 24);
    // Citro2D blend replaces RGB; it is independent of alpha.
    // White is our neutral tint, so preserve the source texture colors.
    const float blend = Pokerogue3DS::imageTintBlend(tintColor);
    C2D_PlainImageTint(&tint, modulatedTint, blend);

    // Call real Citro2D rotated & scaled image renderer
    C2D_DrawImageAtRotated(
        img,
        x + width * 0.5f, y + height * 0.5f,
        0.5f,
        rotation,
        &tint,
        scaleX,
        scaleY
    );
}

void Renderer2D::drawAtlasFrame(C2D_Image atlas, const AtlasFrame& frame,
    float x, float y, float width, float height, float opacity, uint32_t tintColor) {
    if (!m_currentTarget || !atlas.tex || !atlas.subtex || !frame.width || !frame.height
        || !frame.sourceWidth || !frame.sourceHeight || width <= 0 || height <= 0
        || frame.x + frame.width > atlas.subtex->width
        || frame.y + frame.height > atlas.subtex->height
        || frame.trimX + frame.width > frame.sourceWidth + 1
        || frame.trimY + frame.height > frame.sourceHeight + 1
        || atlas.subtex->top < atlas.subtex->bottom) return;

    const auto& base = *atlas.subtex;
    const float du = base.right - base.left;
    const float dv = base.bottom - base.top;
    Tex3DS_SubTexture sub = base;
    sub.width = frame.width;
    sub.height = frame.height;
    sub.left = base.left + du * (float(frame.x) / base.width);
    sub.right = base.left + du * (float(frame.x + frame.width) / base.width);
    sub.top = base.top + dv * (float(frame.y) / base.height);
    sub.bottom = base.top + dv * (float(frame.y + frame.height) / base.height);
    C2D_Image cropped = { atlas.tex, &sub };

    const float sx = width / frame.sourceWidth;
    const float sy = height / frame.sourceHeight;
    drawImageDirect(cropped, x + frame.trimX * sx, y + frame.trimY * sy,
        frame.width * sx, frame.height * sy, 0.0f, opacity, false, false, tintColor);
}

void Renderer2D::drawImage(
    const char* assetId,
    float x, float y,
    float width, float height,
    float rotation,
    float opacity,
    bool flipX,
    bool flipY,
    uint32_t tintColor
) {
    if (!assetId || !m_currentTarget || opacity <= 0.001f) return;

#if !defined(__wasm__)
    Citro2D::RuntimeAssetManager& assetMgr = Citro2D::getRuntimeAssetManager();
    assetMgr.recordDrawCall();

    const Citro2D::CachedAsset* cached = assetMgr.get(assetId);
    if (cached && cached->loaded) {
        drawImageDirect(cached->image, x, y, width, height, rotation, opacity, flipX, flipY, tintColor);
        return;
    }

    // Load once into cache rather than alloc/free per draw frame (Requirement 11)
    if (assetMgr.preload(assetId)) {
        const Citro2D::CachedAsset* newlyCached = assetMgr.get(assetId);
        if (newlyCached && newlyCached->loaded) {
            drawImageDirect(newlyCached->image, x, y, width, height, rotation, opacity, flipX, flipY, tintColor);
        }
    }
#else
    const Citro2D::AssetEntry* entry = Citro2D::findSceneAsset(assetId);
    if (!entry || !entry->romfsPath) return;

    C2D_SpriteSheet sheet = C2D_SpriteSheetLoad(entry->romfsPath);
    if (sheet) {
        C2D_Image img = C2D_SpriteSheetGetImage(sheet, 0);
        drawImageDirect(img, x, y, width, height, rotation, opacity, flipX, flipY, tintColor);
        C2D_SpriteSheetFree(sheet);
    }
#endif
}

void Renderer2D::drawText(
    const char* text,
    float x, float y,
    uint32_t color,
    float opacity
) {
    if (!text || !m_currentTarget || opacity <= 0.001f) return;

    uint32_t a = (color >> 24) & 0xFF;
    a = static_cast<uint32_t>(a * opacity);
    uint32_t finalColor = (color & 0x00FFFFFF) | (a << 24);

    if (m_textBuf) {
        C2D_Text c2dText;
        #if defined(__arm__) || defined(__3DS__) || defined(_3DS)
        if (m_gameFont) C2D_TextFontParse(&c2dText, m_gameFont, m_textBuf, text);
        else
#endif
        C2D_TextParse(&c2dText, m_textBuf, text);
        C2D_TextOptimize(&c2dText);
        C2D_DrawText(&c2dText, C2D_WithColor, std::round(x), std::round(y), 0.5f, 1.0f, 1.0f, finalColor);
    }
}

void Renderer2D::drawText(const char* text, float x, float y, float size, uint32_t color) {
    if (!m_initialized || !m_frameActive || !m_currentTarget || !m_textBuf || !text || size <= 0) return;
    C2D_Text value;
    // beginFrame clears the shared buffer once, preserving all strings until GPU submission.
    #if defined(__arm__) || defined(__3DS__) || defined(_3DS)
        if (m_gameFont) C2D_TextFontParse(&value, m_gameFont, m_textBuf, text);
        else
#endif
        C2D_TextParse(&value, m_textBuf, text);
    C2D_TextOptimize(&value);
    float scale=size;
#if defined(__arm__) || defined(__3DS__) || defined(_3DS)
    // Existing UI scale was authored against the 32px system font.
    // Custom-font compensation preserves existing authored UI sizes; line height uses font metrics.
    if (m_gameFont) scale*=2.0f;
#endif
    C2D_DrawText(&value, C2D_WithColor, std::round(x), std::round(y), 0.5f, scale, scale, color);
}

void Renderer2D::drawTextWrapped(const char* text,float x,float y,float size,float maxWidth,uint32_t color) {
    if(!m_initialized || !m_frameActive || !m_currentTarget || !m_textBuf || !text || size<=0 || maxWidth<=0) return;
#if defined(__arm__) || defined(__3DS__) || defined(_3DS)
    C2D_Text value;
    if(m_gameFont) C2D_TextFontParse(&value,m_gameFont,m_textBuf,text);
    else C2D_TextParse(&value,m_textBuf,text);
    C2D_TextOptimize(&value);
    const float scale=size*(m_gameFont ? 2.0f : 1.0f);
    C2D_DrawText(&value,C2D_WithColor | C2D_WordWrap,std::round(x),std::round(y),0.5f,scale,scale,color,maxWidth);
#else
    drawText(text,x,y,size,color);
#endif
}

float Renderer2D::drawTextFitted(const char* text,float x,float y,float size,float maxWidth,uint32_t color) {
    if(!m_initialized || !m_frameActive || !m_currentTarget || !m_textBuf || !text || size<=0 || maxWidth<=0) return size;
#if defined(__arm__) || defined(__3DS__) || defined(_3DS)
    C2D_Text value;
    if(m_gameFont) C2D_TextFontParse(&value,m_gameFont,m_textBuf,text);
    else C2D_TextParse(&value,m_textBuf,text);
    C2D_TextOptimize(&value);
    float scale=size*(m_gameFont ? 2.0f : 1.0f),width=0;
    C2D_TextGetDimensions(&value,scale,scale,&width,nullptr);
    const float fit=width>maxWidth ? maxWidth/width : 1.0f;
    scale*=fit;
    C2D_DrawText(&value,C2D_WithColor,std::round(x),std::round(y),0.5f,scale,scale,color);
    return size*fit;
#else
    drawText(text,x,y,size,color);
    return size;
#endif
}

float Renderer2D::textLineHeight(float size) const {
#if defined(__arm__) || defined(__3DS__) || defined(_3DS)
    const auto* info=m_gameFont ? C2D_FontGetInfo(m_gameFont) : nullptr;
    return info ? info->height*size*(m_gameFont ? 2.0f : 1.0f) : 0.0f;
#else
    return 30.0f*size;
#endif
}

bool Renderer2D::setWindowStyle(unsigned id) {
    const auto* definition=Pokerogue3DS::findWindowTexture(id);
    if(!definition || !m_initialized || m_frameActive) return false;
#if defined(__arm__) || defined(__3DS__) || defined(_3DS)
    // Keep the previous sheet alive if a new asset cannot be loaded/validated.
    auto next=C2D_SpriteSheetLoad(definition->path);
    if(!next) return false;
    const auto image=C2D_SpriteSheetGetImage(next,0);
    if(!image.tex || !image.subtex || image.subtex->width!=24 || image.subtex->height!=24) {
        C2D_SpriteSheetFree(next); return false;
    }
    C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST);
    if(m_window) C2D_SpriteSheetFree(m_window);
    m_window=next;
#endif
    m_windowStyle=id;
    return true;
}

bool Renderer2D::drawWindow(float x,float y,float width,float height) {
#if defined(__arm__) || defined(__3DS__) || defined(_3DS)
    const float border=Pokerogue3DS::kWindowBorder;
    if (!m_initialized || !m_frameActive || !m_currentTarget || !m_window ||
        width<2*border || height<2*border) return false;
    const auto image=C2D_SpriteSheetGetImage(m_window,0);
    if (!image.subtex || image.subtex->width!=24 || image.subtex->height!=24) return false;
    const float xs[]={x,x+border,x+width-border},ys[]={y,y+border,y+height-border};
    const float widths[]={border,width-2*border,border},heights[]={border,height-2*border,border};
    for (unsigned row=0;row<3;++row) for (unsigned column=0;column<3;++column) {
        AtlasFrame frame{uint16_t(column*8),uint16_t(row*8),8,8,8,8,0,0};
        drawAtlasFrame(image,frame,xs[column],ys[row],widths[column],heights[row]);
    }
    return true;
#else
    (void)x;(void)y;(void)width;(void)height;
    return false;
#endif
}

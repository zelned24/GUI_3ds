#pragma once

#include <cstdint>
#include <cstddef>
#include <3ds.h>
#include <citro2d.h>

/**
 * Renderer2D - Real Citro2D graphics subsystem for dual-screen Nintendo 3DS.
 * Directly integrates Citro2D / Citro3D hardware rendering pipeline.
 */
class Renderer2D {
public:
    struct AtlasFrame {
        uint16_t x, y, width, height;
        uint16_t sourceWidth, sourceHeight;
        uint16_t trimX, trimY;
    };
    Renderer2D();
    ~Renderer2D();

    // Hardware lifecycle
    bool init(size_t maxObjects = 4096);
    void fini();

    // Frame synchronization
    void beginFrame();
    void endFrame();

    // Screen selection (Top: 400x240, Bottom: 320x240)
    void beginTop();
    void beginBottom();

    // Render operations
    void clear(uint32_t color);

    void drawImageDirect(
        C2D_Image img,
        float x, float y,
        float width, float height,
        float rotation = 0.0f,
        float opacity = 1.0f,
        bool flipX = false,
        bool flipY = false,
        uint32_t tintColor = 0xFFFFFFFF
    );

    // Draw one unrotated frame from a source atlas, retaining its original
    // canvas and trim offset. Coordinates refer to the source PNG, not VRAM.
    void drawAtlasFrame(C2D_Image atlas, const AtlasFrame& frame,
        float x, float y, float width, float height,
        float opacity = 1.0f, uint32_t tintColor = 0xFFFFFFFF);

    void drawImage(
        const char* assetId,
        float x, float y,
        float width, float height,
        float rotation = 0.0f,
        float opacity = 1.0f,
        bool flipX = false,
        bool flipY = false,
        uint32_t tintColor = 0xFFFFFFFF
    );

    void drawRect(
        float x, float y,
        float width, float height,
        uint32_t color,
        float opacity = 1.0f
    );

    void drawText(
        const char* text,
        float x, float y,
        uint32_t color = 0xFFFFFFFF,
        float opacity = 1.0f
    );

    void drawTextWrapped(const char* text,float x,float y,float size,float maxWidth,uint32_t color);
    float textLineHeight(float size) const;
    float drawTextFitted(const char* text,float x,float y,float size,float maxWidth,uint32_t color,float* drawnWidth=nullptr);
    bool drawTypeLabel(const char* type,float x,float y,float width,float height);
    bool setWindowStyle(unsigned id);
    unsigned windowStyle() const { return m_windowStyle; }
    bool drawWindow(float x, float y, float width, float height);

    // Scaled text; reuses the frame buffer without clearing earlier text draws.
    void drawText(const char* text, float x, float y, float size, uint32_t color);

    // Screen target accessors
    C3D_RenderTarget* getTopTarget() const { return m_topTarget; }
    C3D_RenderTarget* getBottomTarget() const { return m_bottomTarget; }
    C3D_RenderTarget* getCurrentTarget() const { return m_currentTarget; }
    bool isInitialized() const { return m_initialized; }
    const char* initializationError() const { return m_initError; }

private:
    C3D_RenderTarget* m_topTarget;
    C3D_RenderTarget* m_bottomTarget;
    C3D_RenderTarget* m_currentTarget;
    C2D_TextBuf m_textBuf;
#if defined(__arm__) || defined(__3DS__) || defined(_3DS)
    C2D_Font m_gameFont = nullptr;
    C2D_SpriteSheet m_window = nullptr;
    C2D_SpriteSheet m_typeLabels = nullptr;
#endif
    const char* m_initError=nullptr;
    unsigned m_windowStyle=1;
    bool m_initialized;
    bool m_frameActive;
};

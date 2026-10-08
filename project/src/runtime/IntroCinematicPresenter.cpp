#include "runtime/IntroCinematicPresenter.hpp"
#include <cmath>
#include <cstdio>

namespace Pokerogue3DS {

void IntroCinematicPresenter::clear(Renderer2D* renderer) {
    if (m_sheet) {
        if(renderer) renderer->retireSpriteSheet(m_sheet);
        else C2D_SpriteSheetFree(m_sheet);
        m_sheet = nullptr;
    }
    m_active = false;
    m_finished = true;
    m_startMs = 0;
    m_frameCounter = 0;
    m_page = 0xffff;
}

void IntroCinematicPresenter::start() {
    m_active = true;
    m_finished = false;
    m_startMs = 0;
    m_frameCounter = 0;
}

void IntroCinematicPresenter::skip(Renderer2D* renderer) {
    clear(renderer);
}

bool IntroCinematicPresenter::draw(Renderer2D& renderer, uint64_t currentTimestampMs) {
    if (m_finished || !m_active) return false;

    // Compute elapsed playback time
    uint64_t elapsedMs = 0;
    if (currentTimestampMs > 0) {
        if (m_startMs == 0) m_startMs = currentTimestampMs;
        elapsedMs = currentTimestampMs >= m_startMs ? currentTimestampMs - m_startMs : 0;
    } else {
        elapsedMs = (m_frameCounter * 1000) / 60;
    }
    ++m_frameCounter;

    constexpr uint32_t kFadeOutDurationMs = 300;
    const uint32_t totalPlaybackMs = uint32_t(kIntroTotalDurationMs) + kFadeOutDurationMs;

    if (elapsedMs >= totalPlaybackMs) {
        m_finished = true;
        m_active = false;
        if(m_sheet) renderer.retireSpriteSheet(m_sheet);
        m_sheet = nullptr;
        return false;
    }

    // Integer 2x enlargement of the offline 200x100 nearest raster.
    constexpr float screenW = 400.0f;
    constexpr float screenH = 200.0f;
    constexpr float screenX = 0.0f;
    constexpr float screenY = 20.0f;
    renderer.clear(0xff000000);

    size_t current = 0;
    for (size_t i=1;i<kIntroKeyframeCount;++i) {
        if (elapsedMs<kIntroKeyframes[i].timeMs) break;
        current=i;
    }
    const auto& keyframe=kIntroKeyframes[current];
    if (keyframe.page>=kIntroPageCount) {
        if(m_sheet) renderer.retireSpriteSheet(m_sheet);
        m_sheet=nullptr;m_finished=true;m_active=false;return false;
    }
    if (!m_sheet || m_page!=keyframe.page) {
        // Retire after the GPU frame; a previously submitted draw may own it.
        if (m_sheet) renderer.retireSpriteSheet(m_sheet);
        m_sheet=C2D_SpriteSheetLoad(kIntroCinematicPaths[keyframe.page]);
        m_page=keyframe.page;
        if (!m_sheet) {m_finished=true;m_active=false;return false;}
    }
    C2D_Image image=C2D_SpriteSheetGetImage(m_sheet,0);
    if (!image.tex) {
        renderer.retireSpriteSheet(m_sheet);m_sheet=nullptr;
        m_finished=true;m_active=false;return false;
    }
    C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST);
    const Renderer2D::AtlasFrame frame{keyframe.x,keyframe.y,keyframe.width,keyframe.height,
        keyframe.width,keyframe.height,0,0};
    float opacity=1.0f;
    if (elapsedMs>=kIntroTotalDurationMs)
        opacity=1.0f-float(elapsedMs-kIntroTotalDurationMs)/float(kFadeOutDurationMs);
    // Hold the sampled source frame until its successor's source timestamp.
    renderer.drawAtlasFrame(image,frame,screenX,screenY,screenW,screenH,opacity);
    return true;
}

} // namespace Pokerogue3DS

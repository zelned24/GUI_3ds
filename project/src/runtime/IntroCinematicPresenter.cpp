#include "runtime/IntroCinematicPresenter.hpp"
#include <cmath>
#include <cstdio>

namespace Pokerogue3DS {

void IntroCinematicPresenter::clear() {
    if (m_sheet) {
        C2D_SpriteSheetFree(m_sheet);
        m_sheet = nullptr;
    }
    m_active = false;
    m_finished = true;
    m_startMs = 0;
    m_frameCounter = 0;
}

void IntroCinematicPresenter::start() {
    m_active = true;
    m_finished = false;
    m_startMs = 0;
    m_frameCounter = 0;
}

void IntroCinematicPresenter::skip() {
    m_active = false;
    m_finished = true;
    if (m_sheet) {
        C2D_SpriteSheetFree(m_sheet);
        m_sheet = nullptr;
    }
}

bool IntroCinematicPresenter::draw(Renderer2D& renderer, uint64_t currentTimestampMs) {
    if (m_finished || !m_active) return false;

    if (!m_sheet) {
        m_sheet = C2D_SpriteSheetLoad(kIntroCinematicPath);
        if (!m_sheet) {
            m_finished = true;
            m_active = false;
            return false;
        }
    }

    C2D_Image img = C2D_SpriteSheetGetImage(m_sheet, 0);
    if (!img.tex) {
        m_finished = true;
        m_active = false;
        return false;
    }

    C3D_TexSetFilter(img.tex, GPU_LINEAR, GPU_LINEAR);

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
        C2D_SpriteSheetFree(m_sheet);
        m_sheet = nullptr;
        return false;
    }

    // Video aspect ratio: 256:128 (2:1). Centered on top screen 400x240:
    // 400x200 positioned at x=0, y=20.
    constexpr float screenW = 400.0f;
    constexpr float screenH = 200.0f;
    constexpr float screenX = 0.0f;
    constexpr float screenY = 20.0f;

    // Fill background letterbox bars with black
    renderer.clear(0xff000000);

    if (elapsedMs >= kIntroTotalDurationMs) {
        // Fade out transition to title screen
        float fadeOutT = float(elapsedMs - kIntroTotalDurationMs) / float(kFadeOutDurationMs);
        if (fadeOutT > 1.0f) fadeOutT = 1.0f;
        float opacity = 1.0f - fadeOutT;

        const auto& lastKf = kIntroKeyframes[kIntroKeyframeCount - 1];
        Renderer2D::AtlasFrame frame{
            lastKf.x, lastKf.y, lastKf.width, lastKf.height,
            lastKf.width, lastKf.height, 0, 0
        };
        renderer.drawAtlasFrame(img, frame, screenX, screenY, screenW, screenH, opacity);
        return true;
    }

    // Locate current keyframe segment
    size_t curIdx = 0;
    for (size_t i = 0; i < kIntroKeyframeCount - 1; ++i) {
        if (elapsedMs >= kIntroKeyframes[i].timeMs && elapsedMs < kIntroKeyframes[i + 1].timeMs) {
            curIdx = i;
            break;
        }
        if (i == kIntroKeyframeCount - 2) {
            curIdx = i;
        }
    }

    const auto& kfA = kIntroKeyframes[curIdx];
    const auto& kfB = kIntroKeyframes[curIdx + 1];

    float segDuration = float(kfB.timeMs - kfA.timeMs);
    float t = segDuration > 0.0f ? float(elapsedMs - kfA.timeMs) / segDuration : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    Renderer2D::AtlasFrame frameA{
        kfA.x, kfA.y, kfA.width, kfA.height,
        kfA.width, kfA.height, 0, 0
    };
    Renderer2D::AtlasFrame frameB{
        kfB.x, kfB.y, kfB.width, kfB.height,
        kfB.width, kfB.height, 0, 0
    };

    // Alpha crossfade between keyframe A and keyframe B for smooth 60fps playback
    // Source-over: keep A opaque; fading both layers darkens the midpoint.
    renderer.drawAtlasFrame(img, frameA, screenX, screenY, screenW, screenH, 1.0f);
    renderer.drawAtlasFrame(img, frameB, screenX, screenY, screenW, screenH, t);

    return true;
}

} // namespace Pokerogue3DS

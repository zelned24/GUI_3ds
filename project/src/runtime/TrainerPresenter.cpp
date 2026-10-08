#include "runtime/TrainerPresenter.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace Pokerogue3DS {

void TrainerPresenter::clear(Renderer2D* renderer) {
    if (m_sheet) {
        if (renderer) renderer->retireSpriteSheet(m_sheet);
        else C2D_SpriteSheetFree(m_sheet);
        m_sheet = nullptr;
    }
    m_metadata.clear();
    m_currentDef = nullptr;
    m_currentKey[0] = '\0';
    m_failedKey[0] = '\0';
    m_animationStartMs = 0;
}

bool TrainerPresenter::load(const char* key, Renderer2D* renderer) {
    if (!key || !*key) { clear(renderer); return false; }
    if(std::strlen(key)>=sizeof(m_currentKey)) {clear(renderer);return false;}
    if (m_sheet && std::strcmp(m_currentKey, key) == 0) return true;
    if(std::strcmp(m_failedKey,key)==0) return false;
    clear(renderer);

    const auto* def = findTrainerSpriteByKey(key);
    if (!def || !def->texturePath) {std::strcpy(m_failedKey,key);return false;}
    m_sheet = C2D_SpriteSheetLoad(def->texturePath);
    if (!m_sheet) {std::strcpy(m_failedKey,key);return false;}

    std::strncpy(m_currentKey, key, sizeof(m_currentKey) - 1);
    m_currentKey[sizeof(m_currentKey) - 1] = '\0';
    m_currentDef = def;

    // Load .p3a metadata if present
    if (def->metadataPath && !m_metadata.load(def->metadataPath)) {
        clear(renderer);
        std::strcpy(m_failedKey,key);
        return false;
    }

    return true;
}

bool TrainerPresenter::loadTrainer(uint16_t trainerTypeId, bool female, Renderer2D* renderer) {
    const auto* def = findTrainerSprite(trainerTypeId, female);
    if (def) return load(def->key, renderer);
    clear(renderer); // Missing mapping must not retain the previous trainer.
    return false;
}

bool TrainerPresenter::loadPlayerBack(bool female, Renderer2D* renderer) {
    const auto* def = findPlayerBackSprite(female);
    if (def) return load(def->key, renderer);
    clear(renderer);
    return false;
}

void TrainerPresenter::draw(Renderer2D& renderer, float x, float y, float width, float height, uint64_t animationTimeMs) {
    if (!m_sheet) return;
    C2D_Image img = C2D_SpriteSheetGetImage(m_sheet, 0);
    if (!img.tex) return;
    C3D_TexSetFilter(img.tex, GPU_NEAREST, GPU_NEAREST);

    if (m_metadata.frameCount() > 0) {
        if (m_animationStartMs == 0) m_animationStartMs = animationTimeMs;
        const uint64_t elapsedMs = animationTimeMs >= m_animationStartMs ? animationTimeMs - m_animationStartMs : 0;
        const auto* frame = m_metadata.animationFrame(elapsedMs);
        if (frame) {
            Renderer2D::AtlasFrame rect{
                frame->x, frame->y, frame->width, frame->height,
                frame->sourceWidth, frame->sourceHeight, frame->trimX, frame->trimY
            };
            renderer.drawAtlasFrame(img, rect, x, y, width, height);
            return;
        }
    }

    renderer.drawImageDirect(img, x, y, width, height);
}

void TrainerPresenter::drawAnchored(Renderer2D& renderer, float anchorX, float anchorY, float scale, uint64_t animationTimeMs) {
    if (!m_sheet) return;
    C2D_Image img = C2D_SpriteSheetGetImage(m_sheet, 0);
    if (!img.tex) return;
    C3D_TexSetFilter(img.tex, GPU_NEAREST, GPU_NEAREST);

    if (m_metadata.frameCount() > 0) {
        if (m_animationStartMs == 0) m_animationStartMs = animationTimeMs;
        const uint64_t elapsedMs = animationTimeMs >= m_animationStartMs ? animationTimeMs - m_animationStartMs : 0;
        const auto* frame = m_metadata.animationFrame(elapsedMs);
        if (frame) {
            Renderer2D::AtlasFrame rect{
                frame->x, frame->y, frame->width, frame->height,
                frame->sourceWidth, frame->sourceHeight, frame->trimX, frame->trimY
            };
            const float width = std::round(frame->sourceWidth * scale);
            const float height = std::round(frame->sourceHeight * scale);
            const float x = std::round(anchorX - width * 0.5f);
            const float y = std::round(anchorY - height);
            renderer.drawAtlasFrame(img, rect, x, y, width, height);
            return;
        }
    }

    float srcW = m_currentDef ? float(m_currentDef->width) : (img.subtex ? float(img.subtex->width) : 64.0f);
    float srcH = m_currentDef ? float(m_currentDef->height) : (img.subtex ? float(img.subtex->height) : 64.0f);
    const float width = std::round(srcW * scale);
    const float height = std::round(srcH * scale);
    const float x = std::round(anchorX - width * 0.5f);
    const float y = std::round(anchorY - height);
    renderer.drawImageDirect(img, x, y, width, height);
}

} // namespace Pokerogue3DS

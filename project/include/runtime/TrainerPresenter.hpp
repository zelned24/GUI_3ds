#pragma once

#include "gfx/renderer2d.hpp"
#include "content/TrainerSprites.hpp"
#include "runtime/PokemonAtlasMetadata.hpp"
#include <citro2d.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>

namespace Pokerogue3DS {

class TrainerPresenter {
public:
    TrainerPresenter() = default;
    TrainerPresenter(const TrainerPresenter&) = delete;
    TrainerPresenter& operator=(const TrainerPresenter&) = delete;
    ~TrainerPresenter() { clear(); }

    void clear(Renderer2D* renderer = nullptr);
    bool load(const char* key, Renderer2D* renderer = nullptr);
    bool loadTrainer(uint16_t trainerTypeId, bool female = false, Renderer2D* renderer = nullptr);
    bool loadPlayerBack(bool female = false, Renderer2D* renderer = nullptr);

    bool isLoaded() const { return m_sheet != nullptr; }
    const char* currentKey() const { return m_currentKey; }
    const TrainerSpriteDefinition* currentDefinition() const { return m_currentDef; }

    void draw(Renderer2D& renderer, float x, float y, float width, float height, uint64_t animationTimeMs = 0);
    void drawAnchored(Renderer2D& renderer, float anchorX, float anchorY, float scale = 2.0f, uint64_t animationTimeMs = 0);

private:
    C2D_SpriteSheet m_sheet = nullptr;
    PokemonAtlasMetadata m_metadata;
    const TrainerSpriteDefinition* m_currentDef = nullptr;
    char m_currentKey[64]{};
    char m_failedKey[64]{}; // A failed physical load is retried after clear or another identity.
    uint64_t m_animationStartMs = 0;
    bool m_animationStarted = false;
};

} // namespace Pokerogue3DS

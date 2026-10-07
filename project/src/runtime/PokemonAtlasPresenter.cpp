#include "runtime/PokemonAtlasPresenter.hpp"
#include "game/FirstRunRuntime.hpp"
#include "gfx/renderer2d.hpp"
#include "runtime/DualScreenLayout.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace Pokerogue3DS {

void PokemonAtlasPresenter::Slot::clear() {
    for (size_t i = 0; i < kMaxPages; ++i) {
        if (sheets[i]) {
            C2D_SpriteSheetFree(sheets[i]);
            sheets[i] = nullptr;
        }
    }
    image = {};
    page = 0xff;
    activePageMask = 0;
    animationStartMs = 0;
    key.clear();
    metadata.clear();
}

PokemonAtlasPresenter::~PokemonAtlasPresenter() { invalidate(); }
void PokemonAtlasPresenter::invalidate() {
    m_front.clear();
    m_back.clear();
    m_trainerFront.clear();
    m_playerBack.clear();
    m_trainerFrontTypeId = 0;
    m_playerBackLoaded = false;
}

bool PokemonAtlasPresenter::atlasKey(const ResolvedPokemon& pokemon, std::string& out) {
    out.clear();
    if (!pokemon.dex) return false;
    const auto* form = pokemon.formId ? PokerogueContent::findFormById(pokemon.formId) : nullptr;
    if (form) {
        if (!form->atlasKey || !*form->atlasKey) return false;
        out = form->atlasKey;
    } else {
        char digits[8];
        std::snprintf(digits, sizeof(digits), "%u", static_cast<unsigned>(pokemon.dex));
        out = digits;
    }
    if (out.empty() || out.size() > 63) return false;
    for (char c : out) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || c == '-')) return false;
    }
    return true;
}

bool PokemonAtlasPresenter::selectMetadata(Slot& slot, const std::string& key, bool back, uint64_t nowMs) {
    if (slot.key == key) return slot.metadata.frameCount() != 0;
    slot.clear();
    slot.key = key; // Remember missing assets; do not retry filesystem I/O each frame.
    slot.animationStartMs = nowMs;
    char metadataPath[128];
    const char* facing = back ? "back/" : "front/";
    const int metadataLength = std::snprintf(metadataPath, sizeof(metadataPath),
        "romfs:/sprites/pokemon/atlas/%s%s.p3a", facing, key.c_str());
    if (metadataLength < 0 || metadataLength >= int(sizeof(metadataPath))
        || !slot.metadata.load(metadataPath)) return false;
    return true;
}

bool PokemonAtlasPresenter::selectPage(Slot& slot, const std::string& key, bool back, uint8_t page) {
    if (page >= Slot::kMaxPages) return false;
    if (slot.page == page && slot.sheets[page] != nullptr) return true;

    if (slot.sheets[page] != nullptr) {
        slot.page = page;
        slot.image = C2D_SpriteSheetGetImage(slot.sheets[page], 0);
        return slot.image.tex != nullptr;
    }

    char texturePath[128];
    const char* facing = back ? "back/" : "";
    const int length = slot.metadata.paged()
        ? std::snprintf(texturePath, sizeof(texturePath),
            "romfs:/sprites/pokemon/%s%s-p%u.t3x", facing, key.c_str(), unsigned(page))
        : std::snprintf(texturePath, sizeof(texturePath),
            "romfs:/sprites/pokemon/%s%s.t3x", facing, key.c_str());
    if (length < 0 || length >= int(sizeof(texturePath))) return false;
    C2D_SpriteSheet loaded = C2D_SpriteSheetLoad(texturePath);
    if (!loaded) return false;
    C2D_Image img = C2D_SpriteSheetGetImage(loaded, 0);
    if (!img.tex || !img.subtex
        || img.subtex->width != slot.metadata.width()
        || img.subtex->height != slot.metadata.height()) {
        C2D_SpriteSheetFree(loaded);
        return false;
    }
    C3D_TexSetFilter(img.tex, GPU_NEAREST, GPU_NEAREST);
    slot.sheets[page] = loaded;
    slot.activePageMask |= static_cast<uint8_t>(1u << page);
    slot.page = page;
    slot.image = img;
    return true;
}

void PokemonAtlasPresenter::draw(Renderer2D& renderer, const ResolvedPokemon& pokemon,
    bool back, float x, float y, float width, float height, uint64_t animationTimeMs) {
    std::string key;
    if (!atlasKey(pokemon, key)) return;
    Slot& slot = back ? m_back : m_front;
    if (!selectMetadata(slot, key, back, animationTimeMs)) return;
    const uint64_t elapsedMs = animationTimeMs >= slot.animationStartMs
        ? animationTimeMs - slot.animationStartMs : 0;
    const auto* frame = slot.metadata.animationFrame(elapsedMs);
    if (!frame || !selectPage(slot, key, back, frame->page())) return;
    const Renderer2D::AtlasFrame view{frame->x, frame->y, frame->width, frame->height,
        frame->sourceWidth, frame->sourceHeight, frame->trimX, frame->trimY};
    const float drawX = std::round(x);
    const float drawY = std::round(y);
    const float drawW = std::round(width);
    const float drawH = std::round(height);
    renderer.drawAtlasFrame(slot.image, view, drawX, drawY, drawW, drawH);
}

float PokemonAtlasPresenter::calculateProportionalScale(uint32_t sourceWidth, uint32_t sourceHeight,
                                                        bool isBossOrLegendary, bool back,
                                                        float anchorY) {
    (void)anchorY;
    float scale = (sourceWidth <= 48 && sourceHeight <= 48 && !isBossOrLegendary) ? 2.0f : 1.0f;
    const float maxHeight = back ? 100.0f : 72.0f;
    if (sourceHeight > 0 && (sourceHeight * scale) > maxHeight) {
        scale = maxHeight / static_cast<float>(sourceHeight);
    }
    return scale;
}

float PokemonAtlasPresenter::calculateProportionalScale(const ResolvedPokemon& pokemon,
                                                        uint32_t sourceWidth, uint32_t sourceHeight,
                                                        bool back, float anchorY) {
    bool isBossOrLegendary = false;
    if (pokemon.bossState.segmentCount > 0) {
        isBossOrLegendary = true;
    } else {
        const auto* sp = PokerogueContent::findSpeciesByDex(pokemon.dex);
        if (sp && (sp->legendary || sp->subLegendary || sp->mythical)) {
            isBossOrLegendary = true;
        }
    }
    return calculateProportionalScale(sourceWidth, sourceHeight, isBossOrLegendary, back, anchorY);
}

void PokemonAtlasPresenter::drawAnchored(Renderer2D& renderer, const ResolvedPokemon& pokemon,
    bool back, float anchorX, float anchorY, float scale, uint64_t animationTimeMs) {
    std::string key;
    if (!atlasKey(pokemon, key)) return;
    Slot& slot = back ? m_back : m_front;
    if (!selectMetadata(slot, key, back, animationTimeMs)) return;
    const uint64_t elapsedMs = animationTimeMs >= slot.animationStartMs
        ? animationTimeMs - slot.animationStartMs : 0;
    const auto* frame = slot.metadata.animationFrame(elapsedMs);
    if (!frame || !selectPage(slot, key, back, frame->page())) return;
    const Renderer2D::AtlasFrame view{frame->x, frame->y, frame->width, frame->height,
        frame->sourceWidth, frame->sourceHeight, frame->trimX, frame->trimY};
    const float propScale = calculateProportionalScale(pokemon, frame->sourceWidth, frame->sourceHeight, back, anchorY);
    const float finalScale = anchoredSpriteScale(scale, propScale);
    const float width = std::round(frame->sourceWidth * finalScale);
    const float height = std::round(frame->sourceHeight * finalScale);
    const float x = std::round(anchorX - width * 0.5f);
    const float y = std::round(anchorY - height);
    renderer.drawAtlasFrame(slot.image, view, x, y, width, height);
}

void PokemonAtlasPresenter::drawTrainerAnchored(Renderer2D& renderer, uint16_t trainerTypeId, bool female,
    float anchorX, float anchorY, float scale, uint64_t animationTimeMs) {
    if (!m_trainerFront.isLoaded() || m_trainerFrontTypeId != trainerTypeId || m_trainerFrontFemale != female) {
        m_trainerFrontTypeId = trainerTypeId;
        m_trainerFrontFemale = female;
        m_trainerFront.loadTrainer(trainerTypeId, female);
    }
    if (m_trainerFront.isLoaded()) {
        m_trainerFront.drawAnchored(renderer, anchorX, anchorY, scale, animationTimeMs);
    }
}

void PokemonAtlasPresenter::drawPlayerBackAnchored(Renderer2D& renderer, bool female,
    float anchorX, float anchorY, float scale, uint64_t animationTimeMs) {
    if (!m_playerBackLoaded || m_playerBackFemale != female) {
        m_playerBackFemale = female;
        m_playerBackLoaded = m_playerBack.loadPlayerBack(female);
    }
    if (m_playerBack.isLoaded()) {
        m_playerBack.drawAnchored(renderer, anchorX, anchorY, scale, animationTimeMs);
    }
}

} // namespace Pokerogue3DS

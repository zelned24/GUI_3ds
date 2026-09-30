#include "runtime/PokemonAtlasPresenter.hpp"
#include "game/FirstRunRuntime.hpp"
#include "gfx/renderer2d.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include <cstdio>
#include <cstring>

namespace Pokerogue3DS {

void PokemonAtlasPresenter::Slot::clear() {
    if (sheet) C2D_SpriteSheetFree(sheet);
    sheet = nullptr;
    image = {};
    page = 0xff;
    animationStartMs = 0;
    key.clear();
    metadata.clear();
}

PokemonAtlasPresenter::~PokemonAtlasPresenter() { invalidate(); }
void PokemonAtlasPresenter::invalidate() { m_front.clear(); m_back.clear(); }

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
    if (slot.page == page) return slot.sheet != nullptr;
    if (slot.sheet) C2D_SpriteSheetFree(slot.sheet);
    slot.sheet = nullptr;
    slot.image = {};
    slot.page = page;
    char texturePath[128];
    const char* facing = back ? "back/" : "";
    const int length = slot.metadata.paged()
        ? std::snprintf(texturePath, sizeof(texturePath),
            "romfs:/sprites/pokemon/%s%s-p%u.t3x", facing, key.c_str(), unsigned(page))
        : std::snprintf(texturePath, sizeof(texturePath),
            "romfs:/sprites/pokemon/%s%s.t3x", facing, key.c_str());
    if (length < 0 || length >= int(sizeof(texturePath))) return false;
    slot.sheet = C2D_SpriteSheetLoad(texturePath);
    if (!slot.sheet) return false;
    slot.image = C2D_SpriteSheetGetImage(slot.sheet, 0);
    if (!slot.image.tex || !slot.image.subtex
        || slot.image.subtex->width != slot.metadata.width()
        || slot.image.subtex->height != slot.metadata.height()) {
        C2D_SpriteSheetFree(slot.sheet); slot.sheet = nullptr;
        slot.image = {}; return false;
    }
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
    renderer.drawAtlasFrame(slot.image, view, x, y, width, height);
}

} // namespace Pokerogue3DS

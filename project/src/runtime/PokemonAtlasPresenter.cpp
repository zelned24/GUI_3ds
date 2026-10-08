#include "runtime/PokemonAtlasPresenter.hpp"
#include "game/FirstRunRuntime.hpp"
#include "gfx/renderer2d.hpp"
#include "runtime/DualScreenLayout.hpp"
#include "content/NativeSpritePolicy.hpp"
#include "content/PokemonAppearanceAssets.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace Pokerogue3DS {

void PokemonAtlasPresenter::Slot::clear(Renderer2D* renderer) {
    for (size_t i = 0; i < kMaxPages; ++i) {
        if (sheets[i]) {
            if (renderer) renderer->retireSpriteSheet(sheets[i]);
            else C2D_SpriteSheetFree(sheets[i]);
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
void PokemonAtlasPresenter::invalidate(Renderer2D* renderer) {
    m_lastUnsupportedAppearance.clear();
    m_front.clear(renderer);
    m_back.clear(renderer);
    m_trainerFront.clear(renderer);
    m_playerBack.clear(renderer);
    m_trainerFrontTypeId = 0;
    m_playerBackLoaded = false;
}

bool PokemonAtlasPresenter::atlasKey(const ResolvedPokemon& pokemon, bool back, std::string& out) {
    if(!resolveAtlasKey(pokemon.dex,pokemon.formId,out)) return false;
    const auto& appearance=pokemon.actor;
    if(!appearance.appearanceResolved) {
        if(!appearance.shiny && appearance.shinyVariant==0) return true; // Legacy identity remains unknown.
    } else if(appearance.shinyVariant<=2 && (appearance.shiny || appearance.shinyVariant==0)) {
        const auto* form=pokemon.formId && *pokemon.formId ? PokerogueContent::findFormById(pokemon.formId) : nullptr;
        int differences=PokerogueContent::speciesGenderDifferences(pokemon.dex);
        if(form) {
            const auto* visual=PokerogueContent::formGenderVisual(form->id);
            differences=visual ? int(visual->genderDiffs) : -1;
        }
        const auto hyphen=out.find('-');
        const char* spriteForm=hyphen==std::string::npos ? "" : out.c_str()+hyphen+1;
        if(PokerogueContent::genderSpriteFormExcluded(spriteForm)) differences=0;
        const bool female=appearance.gender==PokemonGender::Female && differences==1;
        if(differences>=0 && !(differences==1 && appearance.gender==PokemonGender::Unspecified)) {
            if(!appearance.shiny && !female) return true;
            if(appearance.shiny || female) {
                const auto* asset=findPokemonAppearanceAsset(out.c_str(),back,female,appearance.shinyVariant,appearance.shiny);
                if(asset) {out=asset->atlasKey;return true;}
            }
        }
    }
    // Never replace a known shiny/female appearance with a normal/male sprite.
    char diagnostic[160];
    std::snprintf(diagnostic,sizeof(diagnostic),"%u:%s:%s:%u:%u:%u:%u",unsigned(pokemon.dex),
        pokemon.formId ? pokemon.formId : "",back ? "back" : "front",
        unsigned(appearance.gender),unsigned(appearance.appearanceResolved),unsigned(appearance.shiny),unsigned(appearance.shinyVariant));
    if(m_lastUnsupportedAppearance!=diagnostic) {
        std::fprintf(stderr,"NOT_YET_SUPPORTED_POKEMON_APPEARANCE: %s\n",diagnostic);
        m_lastUnsupportedAppearance=diagnostic;
    }
    out.clear();return false;
}

bool PokemonAtlasPresenter::selectMetadata(Renderer2D& renderer, Slot& slot, const std::string& key, bool back, uint64_t nowMs) {
    if (slot.key == key) return slot.metadata.frameCount() != 0;
    slot.clear(&renderer); // Queued draws retain their texture until SYNCDRAW.
    slot.key = key; // Remember missing assets; do not retry filesystem I/O each frame.
    slot.animationStartMs = nowMs;
    char metadataPath[128];
    const char* facing = back ? "back/" : "front/";
    const int metadataLength = std::snprintf(metadataPath, sizeof(metadataPath),
        "romfs:/sprites/pokemon/atlas/%s%s.p3a", facing, key.c_str());
    if (metadataLength < 0 || metadataLength >= int(sizeof(metadataPath))
        || !slot.metadata.load(metadataPath)) return false;
    if(slot.metadata.canvasWidth()>(back ? kNativeBackCanvasWidth : kNativeFrontCanvasWidth)
        || slot.metadata.canvasHeight()>(back ? kNativeBackCanvasHeight : kNativeFrontCanvasHeight)) {
        std::fprintf(stderr,"NOT_YET_SUPPORTED_NATIVE_CANVAS: %s; prepare native sprite overrides\n",metadataPath);
        slot.metadata.clear();return false;
    }
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
    if (!atlasKey(pokemon, back, key)) return;
    Slot& slot = back ? m_back : m_front;
    if (!selectMetadata(renderer, slot, key, back, animationTimeMs)) return;
    const uint64_t elapsedMs = animationTimeMs >= slot.animationStartMs
        ? animationTimeMs - slot.animationStartMs : 0;
    const auto* frame = slot.metadata.animationFrame(elapsedMs);
    if (!frame || !selectPage(slot, key, back, frame->page())) return;
    const Renderer2D::AtlasFrame view{frame->x, frame->y, frame->width, frame->height,
        frame->sourceWidth, frame->sourceHeight, frame->trimX, frame->trimY};
    const float canvasW=slot.metadata.canvasWidth(),canvasH=slot.metadata.canvasHeight();
    if(!canvasW || !canvasH || !std::isfinite(width) || !std::isfinite(height) || width<canvasW || height<canvasH) return;
    const float scale=std::min(2.0f,std::floor(std::min(width/canvasW,height/canvasH)));
    const float drawW=frame->sourceWidth*scale,drawH=frame->sourceHeight*scale;
    renderer.drawAtlasFrame(slot.image,view,std::round(x+(width-drawW)/2),std::round(y+(height-drawH)/2),drawW,drawH);

}

float PokemonAtlasPresenter::calculateProportionalScale(uint32_t sourceWidth, uint32_t sourceHeight,
                                                        bool isBossOrLegendary, bool back,
                                                        float anchorY) {
    (void)anchorY;
    return nativeCombatSpriteScale(sourceWidth,sourceHeight,isBossOrLegendary,
        back ? kNativeBackCanvasHeight : kNativeFrontCanvasHeight);
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
    if (!atlasKey(pokemon, back, key)) return;
    Slot& slot = back ? m_back : m_front;
    if (!selectMetadata(renderer, slot, key, back, animationTimeMs)) return;
    const uint64_t elapsedMs = animationTimeMs >= slot.animationStartMs
        ? animationTimeMs - slot.animationStartMs : 0;
    const auto* frame = slot.metadata.animationFrame(elapsedMs);
    if (!frame || !selectPage(slot, key, back, frame->page())) return;
    const Renderer2D::AtlasFrame view{frame->x, frame->y, frame->width, frame->height,
        frame->sourceWidth, frame->sourceHeight, frame->trimX, frame->trimY};
    const float propScale = calculateProportionalScale(pokemon, slot.metadata.canvasWidth(), slot.metadata.canvasHeight(), back, anchorY);
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
        m_trainerFront.loadTrainer(trainerTypeId, female, &renderer);
    }
    if (m_trainerFront.isLoaded()) {
        m_trainerFront.drawAnchored(renderer, anchorX, anchorY, scale, animationTimeMs);
    }
}

void PokemonAtlasPresenter::drawPlayerBackAnchored(Renderer2D& renderer, bool female,
    float anchorX, float anchorY, float scale, uint64_t animationTimeMs) {
    if (!m_playerBackLoaded || m_playerBackFemale != female) {
        m_playerBackFemale = female;
        m_playerBackLoaded = m_playerBack.loadPlayerBack(female, &renderer);
    }
    if (m_playerBack.isLoaded()) {
        m_playerBack.drawAnchored(renderer, anchorX, anchorY, scale, animationTimeMs);
    }
}

} // namespace Pokerogue3DS

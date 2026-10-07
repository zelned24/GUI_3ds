#pragma once

#include "runtime/PokemonAtlasMetadata.hpp"
#include "runtime/TrainerPresenter.hpp"
#include <citro2d.h>
#include <cstdint>
#include <string>
#include <cstring>
#include <cstdio>
#include "content/PokerogueRuntimeContent.hpp"

class Renderer2D;
namespace Pokerogue3DS {
struct ResolvedPokemon;

// Presentation binding for the two active Pokémon. Source identity comes from
// canonical form/species data; assets are loaded only while they are visible.
class PokemonAtlasPresenter {
public:
    PokemonAtlasPresenter() = default;
    ~PokemonAtlasPresenter();
    PokemonAtlasPresenter(const PokemonAtlasPresenter&) = delete;
    PokemonAtlasPresenter& operator=(const PokemonAtlasPresenter&) = delete;

    void draw(Renderer2D& renderer, const ResolvedPokemon& pokemon,
              bool back, float x, float y, float width, float height,
              uint64_t animationTimeMs);
    void drawAnchored(Renderer2D& renderer, const ResolvedPokemon& pokemon,
                      bool back, float anchorX, float anchorY, float scale,
                      uint64_t animationTimeMs);
    void drawTrainerAnchored(Renderer2D& renderer, uint16_t trainerTypeId, bool female,
                             float anchorX, float anchorY, float scale, uint64_t animationTimeMs);
    void drawPlayerBackAnchored(Renderer2D& renderer, bool female,
                                float anchorX, float anchorY, float scale, uint64_t animationTimeMs);
    // Resolve canonical identity before constructing any RomFS path.
    static bool resolveAtlasKey(uint16_t dex,const char* formId,std::string& out) {
        out.clear();
        const auto* species=PokerogueContent::findSpeciesByDex(dex);
        if(!species) return false;
        const bool hasForm=formId && *formId;
        const auto* form=hasForm ? PokerogueContent::findFormById(formId) : nullptr;
        if(hasForm && (!form || std::strcmp(form->speciesId,species->id))) return false;
        if(form) {
            if(!form->atlasKey || !*form->atlasKey) return false;
            out=form->atlasKey;
        } else {
            char digits[8];std::snprintf(digits,sizeof(digits),"%u",unsigned(dex));out=digits;
        }
        if(out.empty() || out.size()>63) {out.clear();return false;}
        for(char c:out) if(!((c>='0' && c<='9') || (c>='a' && c<='z') || c=='-')) {
            out.clear();return false;
        }
        return true;
    }
    void invalidate(); // Retry after an installed content pack becomes active.

    // Proportional combat sprite scaling helper (2.0x for <=48px, 1.0x for >48px/boss, platform height clamping)
    static float calculateProportionalScale(uint32_t sourceWidth, uint32_t sourceHeight,
                                            bool isBossOrLegendary, bool back,
                                            float anchorY = 0.0f);
    static float calculateProportionalScale(const ResolvedPokemon& pokemon,
                                            uint32_t sourceWidth, uint32_t sourceHeight,
                                            bool back, float anchorY = 0.0f);
private:
    struct Slot {
        static constexpr size_t kMaxPages = 4;
        std::string key;
        C2D_SpriteSheet sheets[kMaxPages]{};
        C2D_Image image{};
        uint8_t page = 0xff;
        uint8_t activePageMask = 0;
        uint64_t animationStartMs = 0;
        PokemonAtlasMetadata metadata;
        void clear();
    };
    Slot m_front;
    Slot m_back;
    TrainerPresenter m_trainerFront;
    TrainerPresenter m_playerBack;
    uint16_t m_trainerFrontTypeId = 0;
    bool m_trainerFrontFemale = false;
    bool m_playerBackFemale = false;
    bool m_playerBackLoaded = false;
    static bool atlasKey(const ResolvedPokemon&, std::string& out);
    static bool selectMetadata(Slot&, const std::string& key, bool back, uint64_t nowMs);
    static bool selectPage(Slot&, const std::string& key, bool back, uint8_t page);
};

} // namespace Pokerogue3DS

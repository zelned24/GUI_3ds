#pragma once

#include "runtime/PokemonAtlasMetadata.hpp"
#include <citro2d.h>
#include <cstdint>
#include <string>

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
    void invalidate(); // Retry after an installed content pack becomes active.
private:
    struct Slot {
        std::string key;
        C2D_SpriteSheet sheet = nullptr;
        C2D_Image image{};
        uint8_t page = 0xff;
        uint64_t animationStartMs = 0;
        PokemonAtlasMetadata metadata;
        void clear();
    };
    Slot m_front;
    Slot m_back;
    static bool atlasKey(const ResolvedPokemon&, std::string& out);
    static bool selectMetadata(Slot&, const std::string& key, bool back, uint64_t nowMs);
    static bool selectPage(Slot&, const std::string& key, bool back, uint8_t page);
};

} // namespace Pokerogue3DS

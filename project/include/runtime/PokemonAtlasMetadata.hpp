#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Pokerogue3DS {

struct PokemonAtlasFrame {
    char filename[12]{};
    uint16_t x = 0, y = 0, width = 0, height = 0;
    uint16_t sourceWidth = 0, sourceHeight = 0;
    uint16_t trimX = 0, trimY = 0;
    uint16_t durationMs = 0, flags = 0;
    uint8_t page() const { return static_cast<uint8_t>(flags >> 1); }
};

// One atlas at a time. The file is separate from C++ catalog tables so the
// full Pokémon sprite inventory never occupies ARM11 RAM at once.
class PokemonAtlasMetadata {
public:
    bool load(const char* path, const uint8_t expectedImageSha256[32] = nullptr);
    void clear();
    const PokemonAtlasFrame* find(const char* filename) const;
    const PokemonAtlasFrame* frame(std::size_t index) const;
    const PokemonAtlasFrame* animationFrame(uint64_t timeMs) const;
    std::size_t frameCount() const { return m_frames.size(); }
    uint16_t width() const { return m_width; }
    uint16_t height() const { return m_height; }
    uint16_t canvasWidth() const { return m_canvasWidth; }
    uint16_t canvasHeight() const { return m_canvasHeight; }
    bool paged() const { return m_paged; }
    const uint8_t* imageSha256() const { return m_imageHash; }
    const uint8_t* manifestSha256() const { return m_manifestHash; }
private:
    uint16_t m_width = 0, m_height = 0;
    uint16_t m_canvasWidth=0,m_canvasHeight=0;
    bool m_paged = false;
    uint8_t m_imageHash[32]{}, m_manifestHash[32]{};
    std::vector<PokemonAtlasFrame> m_frames;
    std::vector<uint16_t> m_animationIndices;
};

} // namespace Pokerogue3DS

#include "runtime/PokemonAtlasMetadata.hpp"
#include <cstdio>
#include <cstring>

namespace Pokerogue3DS {
namespace {
uint16_t read16(const uint8_t* p) { return uint16_t(p[0] | uint16_t(p[1]) << 8); }
uint32_t read32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
bool validName(const char* name) {
    unsigned i = 0;
    for (; i < 12 && name[i]; ++i) {
        const char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return false;
    }
    return i > 0 && i < 12;
}
}

void PokemonAtlasMetadata::clear() {
    m_frames.clear();
    m_animationIndices.clear();
    m_width = m_height = m_canvasWidth = m_canvasHeight = 0;
    m_paged = false;
    std::memset(m_imageHash, 0, sizeof(m_imageHash));
    std::memset(m_manifestHash, 0, sizeof(m_manifestHash));
}

bool PokemonAtlasMetadata::load(const char* path, const uint8_t expectedImageSha256[32]) {
    clear();
    if (!path || !*path) return false;
    std::FILE* file = std::fopen(path, "rb");
    if (!file) return false;
    uint8_t header[84];
    bool valid = std::fread(header, 1, sizeof(header), file) == sizeof(header);
    const bool paged = valid && std::memcmp(header, "P3ATLAS2", 8) == 0
        && read32(header + 8) == 2;
    valid = valid && (paged || (std::memcmp(header, "P3ATLAS1", 8) == 0
        && read32(header + 8) == 1));
    if (!valid) { std::fclose(file); return false; }
    const uint16_t width = read16(header + 12), height = read16(header + 14);
    const uint32_t count = read32(header + 16);
    // 1024 frames is a per-atlas RAM safety bound, never a catalog limit.
    if (!width || !height || !count || count > 1024
        || (expectedImageSha256 && std::memcmp(header + 20, expectedImageSha256, 32))) {
        std::fclose(file); return false;
    }
    if (std::fseek(file, 0, SEEK_END) || std::ftell(file) != long(sizeof(header) + count * 32)
        || std::fseek(file, sizeof(header), SEEK_SET)) {
        std::fclose(file); return false;
    }
    m_frames.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        uint8_t raw[32];
        if (std::fread(raw, 1, sizeof(raw), file) != sizeof(raw)) { valid = false; break; }
        PokemonAtlasFrame frame;
        std::memcpy(frame.filename, raw, 12);
        frame.x = read16(raw + 12); frame.y = read16(raw + 14);
        frame.width = read16(raw + 16); frame.height = read16(raw + 18);
        frame.sourceWidth = read16(raw + 20); frame.sourceHeight = read16(raw + 22);
        frame.trimX = read16(raw + 24); frame.trimY = read16(raw + 26);
        frame.durationMs = read16(raw + 28); frame.flags = read16(raw + 30);
        if (!validName(frame.filename) || !frame.width || !frame.height
            || !frame.sourceWidth || !frame.sourceHeight
            || (paged ? (frame.flags >> 1) > 15 : (frame.flags & ~uint16_t(1)) != 0)
            || uint32_t(frame.x) + frame.width > width
            || uint32_t(frame.y) + frame.height > height
            || uint32_t(frame.trimX) + frame.width > uint32_t(frame.sourceWidth) + 1
            || uint32_t(frame.trimY) + frame.height > uint32_t(frame.sourceHeight) + 1) {
            valid = false; break;
        }
        if(frame.sourceWidth>m_canvasWidth) m_canvasWidth=frame.sourceWidth;
        if(frame.sourceHeight>m_canvasHeight) m_canvasHeight=frame.sourceHeight;
        m_frames.push_back(frame);
    }
    std::fclose(file);
    if (!valid || m_frames.size() != count) { clear(); return false; }
    m_width = width; m_height = height;
    m_paged = paged;
    std::memcpy(m_imageHash, header + 20, 32);
    std::memcpy(m_manifestHash, header + 52, 32);
    // Pinned Pokemon.loadAssets generates names 0001.png..0400.png and plays
    // the available frames at 10 FPS. Atlas packing order is not playback order.
    for (unsigned number = 1; number <= 400; ++number) {
        char name[12];
        std::snprintf(name, sizeof(name), "%04u.png", number);
        for (std::size_t i = 0; i < m_frames.size(); ++i) {
            if (std::strcmp(m_frames[i].filename, name) == 0) {
                m_animationIndices.push_back(static_cast<uint16_t>(i));
                break;
            }
        }
    }
    return true;
}

const PokemonAtlasFrame* PokemonAtlasMetadata::find(const char* filename) const {
    if (!filename) return nullptr;
    for (const auto& frame : m_frames) if (std::strcmp(frame.filename, filename) == 0) return &frame;
    return nullptr;
}

const PokemonAtlasFrame* PokemonAtlasMetadata::frame(std::size_t index) const {
    return index < m_frames.size() ? &m_frames[index] : nullptr;
}

const PokemonAtlasFrame* PokemonAtlasMetadata::animationFrame(uint64_t timeMs) const {
    if (m_animationIndices.empty()) return frame(0);
    return frame(m_animationIndices[(timeMs / 100) % m_animationIndices.size()]);
}

} // namespace Pokerogue3DS

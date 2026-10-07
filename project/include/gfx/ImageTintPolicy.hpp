#pragma once
#include <cstdint>
namespace Pokerogue3DS {
// Citro2D RGB replacement strength, independent of texture alpha.
constexpr float imageTintBlend(uint32_t color) {
    return (color & 0x00ffffffu) == 0x00ffffffu ? 0.0f : 1.0f;
}
}

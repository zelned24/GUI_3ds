#pragma once
#include <cstdint>
namespace Pokerogue3DS {
// Visual time only. Never used for battle RNG, seeds, save identity or content.
inline constexpr uint64_t presentationMilliseconds(uint64_t start,uint64_t now,uint64_t frequency) {
    if(!frequency || now<start) return 0;
    const uint64_t elapsed=now-start;
    return (elapsed/frequency)*1000+(elapsed%frequency)*1000/frequency;
}
}

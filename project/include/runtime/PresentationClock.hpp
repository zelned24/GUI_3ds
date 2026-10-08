#pragma once
#include <cstdint>
namespace Pokerogue3DS {
// Overflow-safe frame selection for a looping presentation animation.
inline constexpr uint64_t presentationAnimationFrame(uint64_t milliseconds,unsigned fps,unsigned count) {
    if(!fps || !count) return 0;
    return ((milliseconds/1000%count)*fps+(milliseconds%1000)*fps/1000)%count;
}
// Visual time only. Never used for battle RNG, seeds, save identity or content.
inline constexpr uint64_t presentationMilliseconds(uint64_t start,uint64_t now,uint64_t frequency) {
    if(!frequency || now<start) return 0;
    const uint64_t elapsed=now-start;
    return (elapsed/frequency)*1000+(elapsed%frequency)*1000/frequency;
}
}

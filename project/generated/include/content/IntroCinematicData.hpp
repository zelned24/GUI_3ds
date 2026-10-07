// Generated intro cinematic sequence definitions from images/intro_dark.mp4.
#pragma once
#include <cstdint>

namespace Pokerogue3DS {

struct IntroKeyframe {
    uint16_t timeMs;
    uint16_t x, y, width, height;
};

inline constexpr char kIntroCinematicPath[] = "romfs:/presentation/cinematics/intro_sequence.t3x";
inline constexpr uint16_t kIntroTotalDurationMs = 1667;
inline constexpr uint16_t kIntroKeyframeCount = 16;
inline constexpr IntroKeyframe kIntroKeyframes[16] = {
    {0, 0, 0, 256, 128},
    {100, 256, 0, 256, 128},
    {217, 512, 0, 256, 128},
    {333, 768, 0, 256, 128},
    {433, 0, 128, 256, 128},
    {550, 256, 128, 256, 128},
    {667, 512, 128, 256, 128},
    {767, 768, 128, 256, 128},
    {883, 0, 256, 256, 128},
    {1000, 256, 256, 256, 128},
    {1100, 512, 256, 256, 128},
    {1217, 768, 256, 256, 128},
    {1333, 0, 384, 256, 128},
    {1433, 256, 384, 256, 128},
    {1550, 512, 384, 256, 128},
    {1667, 768, 384, 256, 128},
};

} // namespace Pokerogue3DS

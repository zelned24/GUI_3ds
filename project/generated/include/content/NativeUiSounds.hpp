// Generated from pinned UI WAV sources, no resampling.
#pragma once
#include <cstdint>
namespace Pokerogue3DS {
struct NativeUiSoundDefinition {const char* key;const char* path;uint32_t frames,rate,bytes;};
inline constexpr NativeUiSoundDefinition kNativeUiSounds[]={
    {"select","romfs:/audio/ui/select.pcm",10092,44100,40368},
    {"error","romfs:/audio/ui/error.pcm",16904,44100,67616},
    {"menu_open","romfs:/audio/ui/menu_open.pcm",11830,44100,47320},
};
}

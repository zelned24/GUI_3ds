// Generated from pinned UiWindowStyle and physical 24x24 window assets.
#pragma once
#include <cstddef>
namespace Pokerogue3DS {
struct WindowTextureDefinition { unsigned id; const char* symbol; const char* path; };
inline constexpr WindowTextureDefinition kWindowTextures[] = {
    {1, "RED_ORANGE", "romfs:/presentation/ui/window_1.t3x"},
    {2, "TEAL", "romfs:/presentation/ui/window_2.t3x"},
    {3, "LIGHT_GRAY", "romfs:/presentation/ui/window_3.t3x"},
    {4, "GOLDENROD", "romfs:/presentation/ui/window_4.t3x"},
    {5, "MEDIUM_GRAY", "romfs:/presentation/ui/window_5.t3x"},
};
inline constexpr unsigned kWindowBorder=8;
inline constexpr std::size_t kWindowTextureCount=sizeof(kWindowTextures)/sizeof(kWindowTextures[0]);
inline constexpr const char* kWindowTexturePath=kWindowTextures[0].path;
inline const WindowTextureDefinition* findWindowTexture(unsigned id) { for(const auto& row:kWindowTextures) if(row.id==id) return &row; return nullptr; }
}

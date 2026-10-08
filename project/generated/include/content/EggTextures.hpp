// Generated pinned native egg frames.
#pragma once
#include <cstring>
#include <cstdint>
namespace Pokerogue3DS {
struct EggTexture {const char* atlas;const char* key;const char* path;uint16_t width,height;};
inline constexpr EggTexture kEggTextures[]={
    {"egg_icons","0","romfs:/presentation/eggs/egg_icons-0.t3x",40,30},
    {"egg_icons","1","romfs:/presentation/eggs/egg_icons-1.t3x",40,30},
    {"egg_icons","2","romfs:/presentation/eggs/egg_icons-2.t3x",40,30},
    {"egg_icons","3","romfs:/presentation/eggs/egg_icons-3.t3x",40,30},
    {"egg_icons","manaphy","romfs:/presentation/eggs/egg_icons-manaphy.t3x",40,30},
    {"egg","egg_0","romfs:/presentation/eggs/egg-egg_0.t3x",28,30},
    {"egg","egg_1","romfs:/presentation/eggs/egg-egg_1.t3x",28,30},
    {"egg","egg_2","romfs:/presentation/eggs/egg-egg_2.t3x",28,30},
    {"egg","egg_3","romfs:/presentation/eggs/egg-egg_3.t3x",28,30},
    {"egg","egg_manaphy","romfs:/presentation/eggs/egg-egg_manaphy.t3x",26,31},
};
inline const EggTexture* findEggTexture(const char* atlas,const char* key) {if(!atlas || !key) return nullptr;for(const auto& row:kEggTextures) if(!std::strcmp(atlas,row.atlas) && !std::strcmp(key,row.key)) return &row;return nullptr;}
}

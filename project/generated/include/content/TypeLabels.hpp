// Generated pinned Spanish type label atlas.
#pragma once
#include "gfx/renderer2d.hpp"
namespace Pokerogue3DS {
struct TypeLabelFrame {const char* key;Renderer2D::AtlasFrame frame;};
inline constexpr const char* kTypeLabelPath="romfs:/presentation/ui/types_es-ES.t3x";
inline constexpr unsigned kTypeLabelAtlasWidth=32,kTypeLabelAtlasHeight=280;
inline constexpr TypeLabelFrame kTypeLabelFrames[]={
    {"bug",{0,14,32,14,32,14,0,0}},
    {"dark",{0,28,32,14,32,14,0,0}},
    {"dragon",{0,42,32,14,32,14,0,0}},
    {"electric",{0,56,32,14,32,14,0,0}},
    {"fairy",{0,70,32,14,32,14,0,0}},
    {"fighting",{0,84,32,14,32,14,0,0}},
    {"fire",{0,98,32,14,32,14,0,0}},
    {"flying",{0,112,32,14,32,14,0,0}},
    {"ghost",{0,126,32,14,32,14,0,0}},
    {"grass",{0,140,32,14,32,14,0,0}},
    {"ground",{0,154,32,14,32,14,0,0}},
    {"ice",{0,168,32,14,32,14,0,0}},
    {"normal",{0,182,32,14,32,14,0,0}},
    {"poison",{0,196,32,14,32,14,0,0}},
    {"psychic",{0,210,32,14,32,14,0,0}},
    {"rock",{0,224,32,14,32,14,0,0}},
    {"steel",{0,238,32,14,32,14,0,0}},
    {"stellar",{0,266,32,14,32,14,0,0}},
    {"unknown",{0,0,32,14,32,14,0,0}},
    {"water",{0,252,32,14,32,14,0,0}},
};
inline const TypeLabelFrame* findTypeLabel(const char* type) {if(!type || !*type) return nullptr;for(const auto& row:kTypeLabelFrames) {const char* a=type;const char* b=row.key;while(*a && *b && ((*a>=65 && *a<=90) ? *a+32 : *a)==*b) {++a;++b;}if(!*a && !*b) return &row;}return nullptr;}
}

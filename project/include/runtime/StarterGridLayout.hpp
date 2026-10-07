#pragma once
#include "runtime/DualScreenLayout.hpp"
namespace Pokerogue3DS {
inline constexpr unsigned kStarterGridColumns=6,kStarterGridRows=4;
inline constexpr unsigned kStarterGridPageSize=kStarterGridColumns*kStarterGridRows;
inline int starterGridAt(unsigned x,unsigned y) {
    for(unsigned i=0;i<kStarterGridPageSize;++i)
        if(TouchRect{16+(i%6)*48,38+(i/6)*36,48,36}.contains(x,y)) return int(i);
    return -1;
}
}

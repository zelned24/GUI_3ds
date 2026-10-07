#pragma once
#include "runtime/DualScreenLayout.hpp"
#include "content/PokerogueRuntimeContent.hpp"
namespace Pokerogue3DS {
inline constexpr unsigned kStarterGridColumns=6,kStarterGridRows=3;
inline constexpr unsigned kStarterGridPageSize=kStarterGridColumns*kStarterGridRows;
inline constexpr TouchRect kStarterFilterRect{8,23,304,25};
inline constexpr TouchRect kStarterFooterRects[]={{8,205,83,18},{94,205,125,18},{222,205,90,18}};
inline int starterFooterAt(unsigned x,unsigned y) {
    for(unsigned i=0;i<3;++i) if(kStarterFooterRects[i].contains(x,y)) return int(i);
    return -1;
}
inline int starterGridAt(unsigned x,unsigned y) {
    for(unsigned i=0;i<kStarterGridPageSize;++i)
        if(TouchRect{16+(i%6)*48,54+(i/6)*36,48,36}.contains(x,y)) return int(i);
    return -1;
}
template<class Unlocked> inline unsigned starterCatalogCount(Unlocked unlocked,unsigned generation=0) {
    unsigned count=0;
    for(const auto& row:PokerogueContent::kSpecies)
        if(row.starterEligible && unlocked(row) && (!generation || row.generation==generation)) ++count;
    return count;
}
template<class Unlocked> inline unsigned nextStarterGeneration(unsigned current,int direction,Unlocked unlocked) {
    unsigned next=0;
    for(const auto& row:PokerogueContent::kSpecies) if(row.starterEligible && unlocked(row)) {
        const unsigned gen=row.generation;
        if(direction>0 && gen>current && (!next || gen<next)) next=gen;
        if(direction<0 && (!current || gen<current) && gen>next) next=gen;
    }
    return next;
}
template<class Unlocked> inline const PokerogueContent::Species* starterCatalogAt(unsigned ordinal,Unlocked unlocked,unsigned generation=0) {
    for(const auto& row:PokerogueContent::kSpecies)
        if(row.starterEligible && unlocked(row) && (!generation || row.generation==generation)) {
            if(!ordinal--) return &row;
        }
    return nullptr;
}
}

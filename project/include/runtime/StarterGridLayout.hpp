#pragma once
#include "runtime/DualScreenLayout.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include <cstring>
#include <cstdio>
#include <array>
namespace Pokerogue3DS {
// A bounded display page over canonical species; count remains data-driven.
template<std::size_t PageSize,class Matches>
inline std::array<const PokerogueContent::Species*,PageSize> catalogSpeciesPage(unsigned start,Matches matches) {
    static_assert(PageSize>0,"Display pages require at least one cell");
    std::array<const PokerogueContent::Species*,PageSize> page{};
    unsigned ordinal=0;std::size_t count=0;
    for(const auto& species:PokerogueContent::kSpecies) {
        if(!matches(species)) continue;
        if(ordinal++<start) continue;
        page[count++]=&species;
        if(count==page.size()) break;
    }
    return page;
}
// Pinned starter-select-ui-handler icon state, independent of catalogue filters.
enum class StarterDiscovery {Unknown,Seen,Caught};
inline constexpr StarterDiscovery starterDiscovery(bool caught,uint64_t observedForms) {
    return caught ? StarterDiscovery::Caught : observedForms ? StarterDiscovery::Seen : StarterDiscovery::Unknown;
}
inline constexpr uint32_t starterDiscoveryTint(StarterDiscovery state) {
    return state==StarterDiscovery::Caught ? 0xffffffffu : state==StarterDiscovery::Seen ? 0xff808080u : 0xff000000u;
}
inline bool formatStarterGridCost(uint16_t quarterUnits,char* output,std::size_t capacity) {
    if(!output || !capacity) return false;
    const unsigned remainder=quarterUnits%4;
    const int length=remainder ? std::snprintf(output,capacity,"%u.%02u",quarterUnits/4,remainder*25)
        : std::snprintf(output,capacity,"%u",quarterUnits/4);
    return length>=0 && std::size_t(length)<capacity;
}
enum class StarterCaptureFilter {All,Caught,Uncaught};
inline constexpr bool starterMatchesCaptureFilter(StarterCaptureFilter filter,bool caught) {
    return filter==StarterCaptureFilter::All || (filter==StarterCaptureFilter::Caught ? caught : !caught);
}
inline constexpr unsigned kStarterFormPageSize=6,kStarterFormRowHeight=23;
inline constexpr TouchRect kStarterFormRowsRect{24,43,272,kStarterFormPageSize*kStarterFormRowHeight};
inline constexpr TouchRect kStarterFormBackRect{24,183,272,21};
inline int starterFormRowAt(unsigned x,unsigned y) {
    return kStarterFormRowsRect.contains(x,y) ? int((y-kStarterFormRowsRect.y)/kStarterFormRowHeight) : -1;
}
inline constexpr TouchRect kStarterCaptureFilterRect{254,2,58,18};
inline constexpr unsigned kStarterGridColumns=6,kStarterGridRows=3;
inline constexpr unsigned kStarterGridPageSize=kStarterGridColumns*kStarterGridRows;
inline constexpr TouchRect kStarterFilterRect{8,23,304,25};
inline constexpr TouchRect kStarterGenerationRect{8,23,152,25},kStarterTypeRect{160,23,152,25};
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
inline bool starterMatchesType(const PokerogueContent::Species& row,const char* type) {
    return !type || (row.type1 && std::strcmp(row.type1,type)==0) || (row.type2 && std::strcmp(row.type2,type)==0);
}
template<class Unlocked> inline unsigned starterCatalogCount(Unlocked unlocked,unsigned generation=0,const char* type=nullptr) {
    unsigned count=0;
    for(const auto& row:PokerogueContent::kSpecies)
        if(row.starterEligible && unlocked(row) && (!generation || row.generation==generation) && starterMatchesType(row,type)) ++count;
    return count;
}
template<class Unlocked> inline const char* nextStarterType(const char* current,Unlocked unlocked,unsigned generation=0) {
    const char* next=nullptr;
    for(const auto& row:PokerogueContent::kSpecies)
        if(row.starterEligible && unlocked(row) && (!generation || row.generation==generation)) {
            const char* types[]={row.type1,row.type2};
            for(const char* type:types) if(type && *type && std::strcmp(type,"NONE")!=0 && std::strcmp(type,"none")!=0
                && (!current || std::strcmp(type,current)>0) && (!next || std::strcmp(type,next)<0)) next=type;
        }
    return next;
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
template<class Unlocked> inline const PokerogueContent::Species* starterCatalogAt(unsigned ordinal,Unlocked unlocked,unsigned generation=0,const char* type=nullptr) {
    for(const auto& row:PokerogueContent::kSpecies)
        if(row.starterEligible && unlocked(row) && (!generation || row.generation==generation) && starterMatchesType(row,type)) {
            if(!ordinal--) return &row;
        }
    return nullptr;
}
}

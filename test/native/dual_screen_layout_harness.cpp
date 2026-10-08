#include "runtime/DualScreenLayout.hpp"
#include "runtime/NativeTextRaster.hpp"
#include <cmath>
#include "runtime/TitleMenuLayout.hpp"
#include "runtime/StarterGridLayout.hpp"
#include "content/BallMenuContent.hpp"
#include "content/ItemIcons.hpp"
#include "runtime/PresentationClock.hpp"
#include "runtime/BattleHudGeometry.hpp"
#include "gfx/ImageTintPolicy.hpp"
#include <cassert>
#include <climits>
#include <initializer_list>
using namespace Pokerogue3DS;
int main() {
    static_assert(starterDiscovery(false,0)==StarterDiscovery::Unknown);
    static_assert(starterDiscovery(false,128)==StarterDiscovery::Seen);
    static_assert(starterDiscovery(true,0)==StarterDiscovery::Caught);
    static_assert(starterDiscoveryTint(StarterDiscovery::Unknown)==0xff000000u);
    static_assert(starterDiscoveryTint(StarterDiscovery::Seen)==0xff808080u);
    static_assert(starterDiscoveryTint(StarterDiscovery::Caught)==0xffffffffu);
    static_assert(starterMatchesCaptureFilter(StarterCaptureFilter::All,true));
    static_assert(starterMatchesCaptureFilter(StarterCaptureFilter::All,false));
    static_assert(starterMatchesCaptureFilter(StarterCaptureFilter::Caught,true));
    static_assert(!starterMatchesCaptureFilter(StarterCaptureFilter::Caught,false));
    static_assert(starterMatchesCaptureFilter(StarterCaptureFilter::Uncaught,false));
    static_assert(!starterMatchesCaptureFilter(StarterCaptureFilter::Uncaught,true));
    char gridCost[16];
    assert(formatStarterGridCost(12,gridCost,sizeof(gridCost)) && !std::strcmp(gridCost,"3"));
    assert(formatStarterGridCost(1,gridCost,sizeof(gridCost)) && !std::strcmp(gridCost,"0.25"));
    assert(formatStarterGridCost(6,gridCost,sizeof(gridCost)) && !std::strcmp(gridCost,"1.50"));
    assert(formatStarterGridCost(7,gridCost,sizeof(gridCost)) && !std::strcmp(gridCost,"1.75"));
    assert(!formatStarterGridCost(7,gridCost,2) && !formatStarterGridCost(7,nullptr,0));
    unsigned allEligible=0;
    for(const auto& species:PokerogueContent::kSpecies) allEligible+=species.starterEligible;
    const auto all=[](const auto&){return true;};
    assert(starterCatalogCount(all)==allEligible);
    for(unsigned i=0;i<allEligible;++i) assert(starterCatalogAt(i,all)->starterEligible);
    assert(!starterCatalogAt(allEligible,all));
    double duration=0,pause=0;
    assert(expSegmentTiming(50,50,1,0,true,duration,pause));
    assert(std::fabs(duration-1650)<1e-9 && pause==500);
    assert(expSegmentTiming(50,60,1,0,true,duration,pause));
    assert(std::fabs(duration-165)<1e-9 && pause==50);
    for(unsigned level=1;level<=250;++level) for(unsigned speed=0;speed<4;++speed) {
        assert(expSegmentTiming(level,level,0.5,speed,true,duration,pause));
        assert(duration>=0 && pause==500);
        if(speed==3) assert(duration==0);
    }
    assert(expSegmentTiming(100,100,1,1,true,duration,pause) && std::fabs(duration-825)<1e-9);
    assert(expSegmentTiming(100,100,1,2,true,duration,pause) && std::fabs(duration-412.5)<1e-9);
    assert(expSegmentTiming(100,100,1,0,false,duration,pause) && duration==0);
    assert(!expSegmentTiming(0,5,1,0,true,duration,pause));
    assert(!expSegmentTiming(6,5,1,0,true,duration,pause));
    assert(!expSegmentTiming(5,5,NAN,0,true,duration,pause));
    assert(!expSegmentTiming(5,5,1,4,true,duration,pause));
    assert(expSegmentFraction(0.2,1,0,100)==0.2 && expSegmentFraction(0.2,1,100,100)==1);
    assert(std::fabs(expSegmentFraction(0.2,1,50,100)-(0.2+0.8*(1-std::sqrt(0.5))))<1e-12);
    constexpr uint64_t frequency=268123480;
    static_assert(presentationMilliseconds(100,100+frequency,frequency)==1000,"Real ticks define visual duration");
    static_assert(presentationMilliseconds(100,99,frequency)==0,"Clock reset is bounded");
    static_assert(presentationMilliseconds(0,99,0)==0,"Invalid frequency is bounded");
    for(unsigned cadence:{15,30,60}) {
        for(unsigned second=0;second<=10;++second)
            assert(presentationMilliseconds(100,100+uint64_t(second)*frequency,frequency)==second*1000);
        uint64_t previous=0;
        for(unsigned frame=0;frame<=cadence*10;++frame) {
            const uint64_t actual=presentationMilliseconds(0,frequency*frame/cadence,frequency);
            const uint64_t ideal=uint64_t(frame)*1000/cadence;
            assert(actual<=ideal && ideal-actual<=1 && actual>=previous);
            previous=actual;
        }
    }
    for(const auto& frame:kItemIconFrames) {
        assert(frame.sourceWidth==32 && frame.sourceHeight==32);
        assert(frame.trimX+frame.width<=32 && frame.trimY+frame.height<=32);
    }
    constexpr unsigned balls=sizeof(kBallMenuDefinitions)/sizeof(kBallMenuDefinitions[0]);
    for(unsigned i=0;i<balls;++i) {
        const auto rect=ballMenuRectangle(i);
        assert(rect.y>=24 && rect.y+rect.height<=198 && rect.width==288 && rect.height==32);
        assert(rect.contains(36,rect.y) && rect.contains(67,rect.y+31));
        if(i) assert(ballMenuRectangle(i-1).y+32<rect.y);
    }
    for(unsigned y=0;y<240;++y) for(unsigned x=0;x<320;++x) {
        unsigned hits=0;for(unsigned i=0;i<balls;++i) hits+=ballMenuRectangle(i).contains(x,y);
        assert(hits<=1);
    }
    assert(kDialogueAdvanceRect.contains(12,208) && kDialogueAdvanceRect.contains(167,227));
    assert(!kDialogueAdvanceRect.contains(168,227) && !kDialogueAdvanceRect.contains(12,228));
    for(unsigned y=0;y<240;++y) for(unsigned x=0;x<320;++x)
        if(kDialogueAdvanceRect.contains(x,y)) assert(commandButtonAt(x,y)<0);
    for(unsigned y=0;y<240;++y) for(unsigned x=0;x<320;++x) {
        int expected=-1;
        for(unsigned i=0;i<4;++i) if(kCommandButtonRects[i].contains(x,y)) {
            assert(expected==-1);expected=int(i);
        }
        assert(commandButtonAt(x,y)==expected);
    }
    assert(commandButtonAt(10,10)==0 && commandButtonAt(164,10)==1);
    assert(commandButtonAt(10,112)==2 && commandButtonAt(164,112)==3);
    assert(commandButtonAt(159,50)==-1 && commandButtonAt(20,207)==-1);
    static_assert(anchoredSpriteScale(1.0f,2.0f)==1.0f,"Explicit native size must not be doubled");
    static_assert(anchoredSpriteScale(2.0f,0.75f)==2.0f,"Explicit double size must not be reduced");
    static_assert(anchoredSpriteScale(1.25f,2.0f)==1.25f,"Explicit adaptation scale is preserved");
    static_assert(anchoredSpriteScale(0.0f,0.75f)==0.75f,"Zero retains automatic layout");
    static_assert(nativeCombatSpriteScale(48,36,false,72)==2,"Small canvas fits 2x");
    static_assert(nativeCombatSpriteScale(48,37,false,72)==1,"No fractional clamp");
    static_assert(nativeCombatSpriteScale(48,48,false,100)==2,"Back canvas fits 2x");
    static_assert(nativeCombatSpriteScale(32,32,true,72)==1,"Boss stays native");
    for(unsigned h=1;h<=100;++h) for(unsigned w=1;w<=200;++w)
        assert(nativeCombatSpriteScale(w,h,false,72)==1 || nativeCombatSpriteScale(w,h,false,72)==2);
    for(unsigned n=1;n<=4096;++n) {
        const float requested=float(n)/4096;
        const auto raster=nativeTextRaster(requested);
        assert(raster.index<4 && raster.scale>=1 && raster.scale<=8);
        const float pixels=float(kNativeFontPoints[raster.index]*raster.scale);
        assert(raster.authoredSize==pixels/32);
        for(unsigned i=0;i<4;++i) for(unsigned scale=1;scale<=8;++scale)
            assert(std::fabs(pixels-requested*32)<=std::fabs(float(kNativeFontPoints[i]*scale)-requested*32));
        assert(nativeTextRaster(raster.authoredSize).index==raster.index);
    }
    assert(nativeTextRaster(0.3f).index==1 && nativeTextRaster(0.3f).scale==1);
    assert(nativeTextRaster(0.4f).index==2 && nativeTextRaster(0.4f).scale==1);
    assert(nativeTextRaster(0.5f).index==3 && nativeTextRaster(0.5f).scale==1);
    for(unsigned y=0;y<240;++y) for(unsigned x=0;x<320;++x) {
        const int expected=x>=16 && x<304 && y>=54 && y<162 ? int((y-54)/36*6+(x-16)/48) : -1;
        assert(starterGridAt(x,y)==expected);
        unsigned hits=unsigned(expected>=0)+unsigned(kStarterFilterRect.contains(x,y));
        assert(unsigned(kStarterGenerationRect.contains(x,y))+unsigned(kStarterTypeRect.contains(x,y))==unsigned(kStarterFilterRect.contains(x,y)));
        int footer=-1;
        for(unsigned i=0;i<3;++i) if(kStarterFooterRects[i].contains(x,y)) {++hits;footer=int(i);}
        assert(hits<=1 && starterFooterAt(x,y)==footer);
    }
    const auto none=[](const auto&){return false;};
    const auto fresh=[](const auto& row){return row.freshProfileStarter;};
    assert(starterCatalogCount(none)==0 && !starterCatalogAt(0,none));
    assert(nextStarterGeneration(0,1,none)==0 && nextStarterGeneration(0,-1,none)==0);
    unsigned total=0;
    for(const auto& row:PokerogueContent::kSpecies) if(row.starterEligible && row.freshProfileStarter) {
        assert(starterCatalogAt(total,fresh)->dex==row.dex);++total;
    }
    assert(starterCatalogCount(fresh)==total && !starterCatalogAt(total,fresh));
    unsigned last=0;
    for(unsigned gen=1;gen<=255;++gen) if(starterCatalogCount(fresh,gen)) {
        assert(nextStarterGeneration(last,1,fresh)==gen);
        assert(nextStarterGeneration(gen,-1,fresh)==last);last=gen;
    }
    assert(nextStarterGeneration(last,1,fresh)==0 && nextStarterGeneration(0,-1,fresh)==last);
    assert(!nextStarterType(nullptr,none));
    assert(starterCatalogCount(fresh,0,"UNKNOWN_TYPE")==0 && !starterCatalogAt(0,fresh,0,"UNKNOWN_TYPE"));
    for(unsigned gen=0;gen<=255;++gen) {
        const char* type=nullptr;
        while(const char* next=nextStarterType(type,fresh,gen)) {
            assert(!type || std::strcmp(next,type)>0);type=next;
            unsigned n=0;
            for(const auto& row:PokerogueContent::kSpecies)
                if(row.starterEligible && row.freshProfileStarter && (!gen || row.generation==gen) && starterMatchesType(row,type)) {
                    assert(starterCatalogAt(n,fresh,gen,type)->dex==row.dex);++n;
                }
            assert(n && starterCatalogCount(fresh,gen,type)==n && !starterCatalogAt(n,fresh,gen,type));
        }
    }

    for(unsigned gen=1;gen<=255;++gen) {
        unsigned count=0;
        for(const auto& row:PokerogueContent::kSpecies) if(row.starterEligible && row.freshProfileStarter && row.generation==gen) {
            assert(starterCatalogAt(count,fresh,gen)->dex==row.dex);++count;
        }
        assert(starterCatalogCount(fresh,gen)==count && !starterCatalogAt(count,fresh,gen));
    }
    assert(starterGridAt(UINT_MAX,UINT_MAX)==-1);
    static_assert(titleCursorY(48,25,15)==53, "Cursor is centered on the font height");
    static_assert(titleCursorY(48,15,15)==48, "Equal heights share the same top edge");
    for(bool hasSave:{false,true}) {
        TitleMenuSelection menu{hasSave,0};
        assert(menu.count()==(hasSave ? 5u : 4u));
        assert(menu.action()==(hasSave ? TitleMenuAction::Continue : TitleMenuAction::NewGame));
        menu.move(-1);assert(menu.action()==TitleMenuAction::Settings);
        menu.move(1);assert(menu.selected==0);
        for(unsigned y=0;y<240;++y) for(unsigned x=0;x<320;++x) {
            const int expected=x>=24 && x<296 && y>=48 && y<48+menu.count()*29 ? int((y-48)/29) : -1;
            assert(menu.hit(x,y)==expected);
        }
        assert(menu.hit(UINT_MAX,UINT_MAX)==-1);
    }
    static_assert(imageTintBlend(0xffffffffu)==0.0f, "Opaque textures keep original RGB");
    static_assert(imageTintBlend(0x80ffffffu)==0.0f, "Opacity must not whiten textures");
    static_assert(imageTintBlend(0x00ffffffu)==0.0f, "Transparent neutral tint stays neutral");
    static_assert(imageTintBlend(0xff0000ffu)==1.0f, "Explicit color replacement remains available");
    for (unsigned y=0;y<240;++y) for (unsigned x=0;x<320;++x) {
        const int column=x>=8 && x<96 ? 0 : x>=100 && x<188 ? 1 : -1;
        const int row=y>=36 && y<100 ? 0 : y>=106 && y<170 ? 1 : -1;
        assert(moveButtonAt(x,y)==(row<0 || column<0 ? -1 : row*2+column));
    }
    for (unsigned y=0;y<240;++y) for (unsigned x=0;x<320;++x) {
        const int column=x>=10 && x<156 ? 0 : x>=164 && x<310 ? 1 : -1;
        assert(targetButtonAt(x,y)==(y>=182 && y<208 ? column : -1));
        for (unsigned count=0;count<=6;++count) {
            int expected=-1;
            if (x>=8 && x<312 && y>=38) {
                const unsigned row=(y-38)/27;
                if (row<count && (y-38)%27<25) expected=int(row);
            }
            assert(partyButtonAt(x,y,count)==expected);
        }
    }
    assert(partyButtonAt(UINT_MAX,UINT_MAX,6)==-1);
    assert(partyButtonAt(8,38,7)==-1);
    assert(targetButtonAt(UINT_MAX,UINT_MAX)==-1);
    assert(moveButtonAt(UINT_MAX,UINT_MAX)==-1);
    assert(moveButtonAt(320,240)==-1);
    for(unsigned count=0;count<=4;++count) {
        RewardMoveSelection selection;
        assert(selection.selected==0);
        if(count) {
            assert(selection.move(-1,count) && selection.selected==count-1);
            assert(selection.move(1,count) && selection.selected==0);
        } else assert(!selection.move(1,count));
        assert(!selection.move(0,count));
        selection.reset();assert(selection.selected==0);
        for(unsigned y=0;y<240;++y) for(unsigned x=0;x<320;++x) {
            const int row=y>=44 && y<187 ? int((y-44)/37) : -1;
            const int expected=x>=16 && x<304 && row>=0 && unsigned(row)<count && (y-44)%37<32 ? row : -1;
            assert(RewardMoveSelection::hit(x,y,count)==expected);
        }
    }
    assert(RewardMoveSelection::hit(UINT_MAX,UINT_MAX,4)==-1);
    assert(RewardMoveSelection::hit(16,44,5)==-1);
    for(unsigned count=0;count<=3;++count) {
        for(unsigned y=0;y<240;++y) for(unsigned x=0;x<320;++x) {
            const int col=x>=16 && x<304 ? int((x-16)/100) : -1;
            const int expected=y>=54 && y<94 && col>=0 && unsigned(col)<count && (x-16)%100<88 ? col : -1;
            assert(rewardChoiceAt(x,y,count)==expected);
        }
    }
    assert(rewardChoiceAt(UINT_MAX,UINT_MAX,3)==-1);
    assert(rewardChoiceAt(16,54,4)==-1);
    for (const auto& rect:kMoveButtonRects) {
        assert(rect.x+rect.width<=320 && rect.y+rect.height<=240);
    }
}

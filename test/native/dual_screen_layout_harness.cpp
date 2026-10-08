#include "runtime/DualScreenLayout.hpp"
#include "runtime/NativeTextRaster.hpp"
#include <cmath>
#include "runtime/TitleMenuLayout.hpp"
#include "runtime/StarterGridLayout.hpp"
#include "content/BallMenuContent.hpp"
#include "content/ItemIcons.hpp"
#include "runtime/PresentationClock.hpp"
#include "runtime/BattleHudGeometry.hpp"
#include "runtime/ExperienceBarTimeline.hpp"
#include "gfx/ImageTintPolicy.hpp"
#include <cassert>
#include <climits>
#include <initializer_list>
using namespace Pokerogue3DS;
int main() {
    for(unsigned slot=0;slot<6;++slot) {
        const auto icon=starterTeamIconRectangle(slot);
        assert(icon.width==40 && icon.height==30 && icon.x==11+slot*50 && icon.y==170);
        assert(icon.x>=9+slot*50 && icon.x+icon.width<=9+slot*50+44);
        assert(icon.y>=169 && icon.y+icon.height<=201);
    }
    assert(starterTeamIconRectangle(6).width==0);
    for(const auto& nameBounds:{kStarterAbilityNameRect,kStarterPassiveNameRect}) {
        assert(nameBounds.x+nameBounds.width<=392 && nameBounds.y+nameBounds.height<=159);
        assert(textLinesWithinHeight(nameBounds.height,8,11,2)==2);
    }
    assert(kStarterAbilityNameRect.y+kStarterAbilityNameRect.height<=123);
    assert(kStarterPassiveNameRect.y+kStarterPassiveNameRect.height<=kStarterNatureNameRect.y);
    assert(kStarterNatureNameRect.x+kStarterNatureNameRect.width<=392);
    assert(kStarterNatureNameRect.y+kStarterNatureNameRect.height<=178);
    assert(textLinesWithinHeight(kStarterNatureNameRect.height,8,11,1)==1);

    for(unsigned y=0;y<=240;++y) for(unsigned x=0;x<=320;++x) {
        const bool ability=x>=24 && x<156 && y>=207 && y<225;
        assert(kStarterFormAbilityRect.contains(x,y)==ability);
        const bool nature=x>=164 && x<296 && y>=207 && y<225;
        assert(kStarterFormNatureRect.contains(x,y)==nature);
        assert(!(ability && nature));
        if(ability || nature) assert(starterFormRowAt(x,y)==-1 && !kStarterFormBackRect.contains(x,y) && !kStarterFormCandyRect.contains(x,y));
    }

    assert(movePpBarPixels(0,10,94)==0);
    assert(movePpBarPixels(1,3,94)==31);
    assert(movePpBarPixels(2,3,94)==62);
    assert(movePpBarPixels(3,3,94)==94);
    assert(movePpBarPixels(4,3,94)==94);
    assert(movePpBarPixels(1,0,94)==0);
    assert(movePpBarPixels(1,3,0)==0);
    assert(movePpBarPixels(UINT_MAX-1,UINT_MAX,UINT_MAX)==UINT_MAX-1);

    for(unsigned y=0;y<240;++y) for(unsigned x=0;x<320;++x) {
        const int expected=y>=116 && y<148 ? (x>=45 && x<150 ? 0 : x>=170 && x<275 ? 1 : -1) : -1;
        assert(starterConfirmAt(x,y)==expected);
    }
    assert(starterConfirmAt(0,130)==-1 && starterConfirmAt(320,130)==-1);
    assert(starterConfirmAt(UINT_MAX,UINT_MAX)==-1);

    for(unsigned y=0;y<240;++y) for(unsigned x=0;x<320;++x) {
        int expected=-1;
        for(unsigned cell=0;cell<24;++cell) {
            const unsigned cx=10+(cell%6)*50,cy=43+(cell/6)*36;
            if(x>=cx && x<cx+48 && y>=cy && y<cy+34) {assert(expected==-1);expected=int(cell);}
        }
        assert(pokedexCellAt(x,y)==expected);
    }
    assert(pokedexCellAt(320,240)==-1 && pokedexCellAt(UINT_MAX,UINT_MAX)==-1);
    assert(!pokedexCellRectangle(24).contains(0,0));
    for(const auto& button:kPokedexFilterRects) assert(button.x+button.width<=320 && button.y+button.height<=43);
    for(const auto& button:kPokedexPageRects) assert(button.x+button.width<=320 && button.y>=185 && button.y+button.height<=205);

    for(unsigned y=0;y<240;++y) for(unsigned x=0;x<320;++x) {
        const int row=x>=24 && x<296 && y>=42 && y<150 ? int((y-42)/36) : -1;
        assert(pauseButtonAt(x,y)==row);
    }
    assert(pauseButtonAt(UINT_MAX,UINT_MAX)==-1);
    assert(textLinesWithinHeight(46,10,26,3)==2);
    assert(textLinesWithinHeight(10,10,26,3)==1);
    assert(textLinesWithinHeight(9,10,26,3)==0);
    assert(textLinesWithinHeight(100,10,26,3)==3);
    assert(textLinesWithinHeight(NAN,10,26,3)==0);
    assert(textLinesWithinHeight(46,10,0,3)==0);
    assert(textLinesWithinHeight(46,10,26,0)==0);
    assert(presentationAnimationFrame(0,12,3)==0);
    assert(presentationAnimationFrame(83,12,3)==0);
    assert(presentationAnimationFrame(84,12,3)==1);
    assert(presentationAnimationFrame(250,12,3)==0);
    assert(presentationAnimationFrame(UINT64_MAX,12,3)==1);
    assert(presentationAnimationFrame(UINT64_MAX,0,3)==0);
    assert(presentationAnimationFrame(UINT64_MAX,12,0)==0);
    for(const auto& target:kTargetButtonRects) {
        assert(target.y>=210 && target.y+target.height<=240);
        assert(target.x+target.width<=320);
        for(const auto& move:kMoveButtonRects)
            assert(target.y>=move.y+move.height);
    }
    for(bool doubleBattle:{false,true}) {
        const auto back=moveBackRectangle(doubleBattle);
        for(unsigned y=0;y<240;++y) for(unsigned x=0;x<320;++x)
            assert(back.contains(x,y)==(x>=100 && x<188 && y>=(doubleBattle ? 174u : 192u) && y<(doubleBattle ? 204u : 222u)));
        assert(!back.contains(UINT_MAX,UINT_MAX));
        assert(!back.contains(208,202)); // PP panel never closes the menu.
        if(doubleBattle) for(const auto& target:kTargetButtonRects) assert(back.y+back.height<=target.y);
    }
    ExperienceBarTimeline defaultExp,fastExp,skipExp;
    assert(defaultExp.update("MEDIUM_FAST",5,150,0,false,0));
    assert(fastExp.update("MEDIUM_FAST",5,150,0,false,2));
    assert(skipExp.update("MEDIUM_FAST",5,150,0,false,3));
    assert(defaultExp.update("MEDIUM_FAST",5,200,10,false,0));
    assert(fastExp.update("MEDIUM_FAST",5,200,10,false,2));
    assert(skipExp.update("MEDIUM_FAST",5,200,10,false,3) && skipExp.total()==200);
    assert(defaultExp.update("MEDIUM_FAST",5,200,210,false,0));
    assert(fastExp.update("MEDIUM_FAST",5,200,210,false,2));
    assert(fastExp.total()>defaultExp.total());
    const auto unchanged=defaultExp.total();
    assert(!defaultExp.update("MEDIUM_FAST",5,200,300,false,4) && defaultExp.total()==unchanged);
    ExperienceBarTimeline expVisual;
    assert(expVisual.update("MEDIUM_FAST",5,150,1));
    assert(expVisual.level()==5 && expVisual.total()==150);
    assert(expVisual.update("MEDIUM_FAST",7,400,10));
    assert(expVisual.level()==5);
    assert(expVisual.update("MEDIUM_FAST",7,400,855));
    assert(expVisual.level()==6 && expVisual.fraction()==1 && expVisual.total()==216);
    assert(expVisual.update("MEDIUM_FAST",7,400,1112));
    assert(expVisual.level()==6 && expVisual.fraction()<0.01);
    assert(expVisual.update("MEDIUM_FAST",7,400,10000));
    assert(expVisual.level()==7 && expVisual.total()==400);
    assert(std::fabs(expVisual.fraction()-57.0/169)<1e-12);
    assert(!expVisual.update("UNKNOWN",7,400,10001));
    assert(!expVisual.update("MEDIUM_FAST",7,300,10001));
    assert(expVisual.update("MEDIUM_FAST",5,150,10002,true) && expVisual.level()==5);
    ExperienceBarTimeline correctedAfterGap;
    assert(correctedAfterGap.update("MEDIUM_FAST",5,150,1));
    assert(correctedAfterGap.update("MEDIUM_FAST",7,400,10));
    assert(correctedAfterGap.update("MEDIUM_FAST",6,250,10000));
    assert(correctedAfterGap.level()==6 && correctedAfterGap.total()==250);
    assert(std::fabs(correctedAfterGap.fraction()-34.0/127)<1e-12);
    assert(correctedAfterGap.update("MEDIUM_FAST",7,400,10001));
    assert(correctedAfterGap.update("MEDIUM_FAST",7,350,20000));
    assert(correctedAfterGap.level()==7 && correctedAfterGap.total()==350);
    assert(std::fabs(correctedAfterGap.fraction()-7.0/169)<1e-12);
    ExperienceBarTimeline updatedDuringPause;
    assert(updatedDuringPause.update("MEDIUM_FAST",5,150,1));
    assert(updatedDuringPause.update("MEDIUM_FAST",7,400,10));
    assert(updatedDuringPause.update("MEDIUM_FAST",7,400,855));
    assert(updatedDuringPause.update("MEDIUM_FAST",8,550,900));
    assert(updatedDuringPause.level()==6 && updatedDuringPause.fraction()==1);
    assert(updatedDuringPause.total()==216);
    assert(updatedDuringPause.update("MEDIUM_FAST",8,550,1000));
    assert(updatedDuringPause.level()==6 && updatedDuringPause.fraction()==1);
    assert(updatedDuringPause.update("MEDIUM_FAST",8,550,1112));
    assert(updatedDuringPause.level()==6 && updatedDuringPause.fraction()<0.01);
    assert(updatedDuringPause.update("MEDIUM_FAST",8,550,20000));
    assert(updatedDuringPause.level()==8 && updatedDuringPause.total()==550);
    assert(std::fabs(updatedDuringPause.fraction()-38.0/217)<1e-12);
    ExperienceBarTimeline updatedDuringFill;
    assert(updatedDuringFill.update("MEDIUM_FAST",5,150,1));
    assert(updatedDuringFill.update("MEDIUM_FAST",7,400,10));
    assert(updatedDuringFill.update("MEDIUM_FAST",8,550,100));
    assert(updatedDuringFill.update("MEDIUM_FAST",8,550,855));
    assert(updatedDuringFill.level()==6 && updatedDuringFill.fraction()==1);
    for(unsigned fps:{15u,30u,60u}) {
        ExperienceBarTimeline cadence;
        assert(cadence.update("MEDIUM_FAST",5,150,1));
        assert(cadence.update("MEDIUM_FAST",7,400,10));
        for(unsigned frame=0;frame<fps*10;++frame)
            assert(cadence.update("MEDIUM_FAST",7,400,10+uint64_t(frame)*1000/fps));
        assert(cadence.total()==400 && cadence.level()==7);
    }

    assert(hpTweenDuration(100,99,0)==250);
    assert(hpTweenDuration(100,0,0)==500);
    assert(hpTweenDuration(65535,0,0)==5000);
    assert(hpTweenDuration(100,0,1)==250 && hpTweenDuration(100,0,2)==125);
    assert(hpTweenDuration(100,0,3)==0);
    HpRatioTween hpVisual;
    assert(hpVisual.update(100,100,0)==1);
    assert(hpVisual.update(0,100,100)==1);
    assert(std::fabs(hpVisual.sample(350)-(1-std::sin(3.14159265358979323846/4)))<1e-12);
    assert(hpVisual.displayedHp(600)==0 && hpVisual.sample(600)==0);
    assert(hpVisual.update(100,100,600)==0);
    assert(hpVisual.sample(1100)==1);
    const auto interrupted=hpVisual.update(50,100,1100);
    assert(interrupted==1 && hpVisual.update(75,100,1225)>0.5);
    assert(hpVisual.update(40,100,1225,3,true)==0.4);
    for(unsigned fps:{15u,30u,60u}) {
        HpRatioTween cadence;
        cadence.update(100,100,1);cadence.update(0,100,101);
        for(unsigned frame=0;frame<=fps;++frame) cadence.sample(101+frame*1000/fps);
        assert(cadence.sample(601)==0);
    }

    for(const auto& species:PokerogueContent::kSpecies)
        assert(PokerogueContent::speciesPassiveAbilityId(species.dex,0)==species.abilityPassive);
    for(const auto& passive:PokerogueContent::kSpeciesPassiveFormAbilities) {
        assert(PokerogueContent::findSpeciesByDex(passive.dex));
        assert(PokerogueContent::speciesPassiveAbilityId(passive.dex,passive.formIndex)==passive.abilityId);
    }
    assert(PokerogueContent::speciesPassiveAbilityId(65535,0)==0);
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
        const int formExpected=x>=24 && x<296 && y>=43 && y<181 ? int((y-43)/23) : -1;
        assert(starterFormRowAt(x,y)==formExpected);
        assert(!(formExpected>=0 && kStarterFormBackRect.contains(x,y)));
        assert(!(kStarterFormCandyRect.contains(x,y) && kStarterFormBackRect.contains(x,y)));
        assert(!(formExpected>=0 && kStarterFormCandyRect.contains(x,y)));
        assert(!(kStarterCandyOptionRects[0].contains(x,y) && kStarterCandyOptionRects[1].contains(x,y)));
        for(const auto& option:kStarterCandyOptionRects)
            assert(!(option.contains(x,y) && kStarterCandyBackRect.contains(x,y)));
        unsigned hits=unsigned(expected>=0)+unsigned(kStarterFilterRect.contains(x,y));
        assert(unsigned(kStarterGenerationRect.contains(x,y))+unsigned(kStarterTypeRect.contains(x,y))==unsigned(kStarterFilterRect.contains(x,y)));
        int footer=-1;
        for(unsigned i=0;i<3;++i) if(kStarterFooterRects[i].contains(x,y)) {++hits;footer=int(i);}
        assert(hits<=1 && starterFooterAt(x,y)==footer);
    }
    assert(moveStarterFormCursor(0,0,1)==0);
    for(unsigned count=1;count<=65;++count) for(unsigned selected=0;selected<count;++selected) {
        for(int delta : {-6,-1,1,6}) {
            const unsigned next=moveStarterFormCursor(selected,count,delta);
            assert(next<count);
            assert(moveStarterFormCursor(next,count,-delta)==selected);
        }
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
        assert(targetButtonAt(x,y)==(y>=210 && y<234 ? column : -1));
        for (unsigned count=0;count<=6;++count) {
            int expected=-1;
            if (x>=8 && x<312 && y>=24) {
                const unsigned row=(y-24)/32;
                if (row<count && (y-24)%32<32) expected=int(row);
            }
            assert(partyButtonAt(x,y,count)==expected);
        }
    }
    for(const auto& row:kRewardMoveRects) {
        assert(textLinesWithinHeight(row.height-10,8,12,2)==2);
        assert(row.y+5+8+12<=row.y+row.height);
        assert(44+180<234 && 234+58<=row.x+row.width);
    }
    assert(kPartyHeaderRect.y+kPartyHeaderRect.height<kPartyButtonRects[0].y);
    assert(kPartyButtonRects[5].y+kPartyButtonRects[5].height<kPartyFooterRect.y);
    assert(kPartyFooterRect.y+kPartyFooterRect.height<=240);
    for(const auto& row:kPartyButtonRects) {
        assert(row.height==32 && row.y+row.height<=216);
        assert(row.y+1+30<=row.y+row.height);
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

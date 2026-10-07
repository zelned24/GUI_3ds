#include "runtime/DualScreenLayout.hpp"
#include "runtime/NativeTextRaster.hpp"
#include <cmath>
#include "runtime/TitleMenuLayout.hpp"
#include "runtime/StarterGridLayout.hpp"
#include "content/BallMenuContent.hpp"
#include "content/ItemIcons.hpp"
#include "gfx/ImageTintPolicy.hpp"
#include <cassert>
#include <climits>
#include <initializer_list>
using namespace Pokerogue3DS;
int main() {
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
        const int expected=x>=16 && x<304 && y>=38 && y<182 ? int((y-38)/36*6+(x-16)/48) : -1;
        assert(starterGridAt(x,y)==expected);
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

#include "runtime/DualScreenLayout.hpp"
#include "runtime/TitleMenuLayout.hpp"
#include "runtime/StarterGridLayout.hpp"
#include "gfx/ImageTintPolicy.hpp"
#include <cassert>
#include <climits>
#include <initializer_list>
using namespace Pokerogue3DS;
int main() {
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
    for (const auto& rect:kMoveButtonRects) {
        assert(rect.x+rect.width<=320 && rect.y+rect.height<=240);
    }
}

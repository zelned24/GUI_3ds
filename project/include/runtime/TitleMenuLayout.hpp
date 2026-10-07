#pragma once
#include "runtime/DualScreenLayout.hpp"
namespace Pokerogue3DS {
constexpr float titleCursorY(float textY,float textHeight,float cursorHeight) {
    return textY+(textHeight-cursorHeight)*0.5f;
}
enum class TitleMenuAction : unsigned { Continue, NewGame, LoadGame, RunHistory, Settings };
struct TitleMenuSelection {
    bool hasContinue=false;
    unsigned selected=0;
    unsigned count() const { return hasContinue ? 5 : 4; }
    TitleMenuAction action() const { return static_cast<TitleMenuAction>(selected+(hasContinue ? 0 : 1)); }
    void move(int direction) { if (direction>0) selected=(selected+1)%count(); else if (direction<0) selected=(selected+count()-1)%count(); }
    int hit(unsigned x,unsigned y) const {
        for(unsigned i=0;i<count();++i) if (TouchRect{24,48+i*29,272,29}.contains(x,y)) return int(i);
        return -1;
    }
};
}

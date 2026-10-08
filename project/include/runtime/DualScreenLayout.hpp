#pragma once
#include <cmath>
namespace Pokerogue3DS {
// Zero requests automatic placement; positive values are explicit presentation scale.
inline constexpr float anchoredSpriteScale(float requested,float automatic) {
    return requested>0.0f ? requested : automatic;
}

inline constexpr float nativeCombatSpriteScale(unsigned width,unsigned height,bool boss,unsigned maxHeight) {
    return width && height && !boss && width<=48 && height<=48 && height<=maxHeight/2 ? 2.0f : 1.0f;
}

// Count complete ink rows using actual native line feed, not authored font size.
inline unsigned textLinesWithinHeight(float height,float inkHeight,float lineHeight,unsigned limit) {
    if(!limit || !std::isfinite(height) || !std::isfinite(inkHeight) || !std::isfinite(lineHeight)
        || inkHeight<=0 || lineHeight<=0 || height<inkHeight) return 0;
    const float extra=std::floor((height-inkHeight)/lineHeight);
    return extra>=limit-1 ? limit : 1+static_cast<unsigned>(extra);
}

struct TouchRect {
    unsigned x,y,width,height;
    constexpr bool contains(unsigned px,unsigned py) const {
        return px>=x && px-x<width && py>=y && py-y<height;
    }
};
inline constexpr TouchRect kDialogueAdvanceRect{12,208,156,20};
inline constexpr TouchRect ballMenuRectangle(unsigned index) {return {16,24+index*34,288,32};}
inline constexpr TouchRect kMoveButtonRects[] = {
    {8, 36, 88, 64},
    {100, 36, 88, 64},
    {8, 106, 88, 64},
    {100, 106, 88, 64}
};
inline constexpr int moveButtonAt(unsigned x,unsigned y) {
    for (unsigned i=0;i<4;++i) if (kMoveButtonRects[i].contains(x,y)) return int(i);
    return -1;
}
inline constexpr TouchRect moveBackRectangle(bool doubleBattle) {
    return {100,doubleBattle ? 174u : 192u,88,30};
}
inline constexpr TouchRect kTargetButtonRects[]={{10,210,146,24},{164,210,146,24}};
inline constexpr int targetButtonAt(unsigned x,unsigned y) {
    for (unsigned i=0;i<2;++i) if (kTargetButtonRects[i].contains(x,y)) return int(i);
    return -1;
}
inline constexpr TouchRect kPartyButtonRects[]={
    {8,38,304,25},{8,65,304,25},{8,92,304,25},
    {8,119,304,25},{8,146,304,25},{8,173,304,25}
};
inline constexpr int partyButtonAt(unsigned x,unsigned y,unsigned count) {
    if (count>6) return -1;
    for (unsigned i=0;i<count;++i) if (kPartyButtonRects[i].contains(x,y)) return int(i);
    return -1;
}

inline constexpr TouchRect kPauseButtonRects[]={{24,42,272,36},{24,78,272,36},{24,114,272,36}};
inline constexpr int pauseButtonAt(unsigned x,unsigned y) {
    for(unsigned i=0;i<3;++i) if(kPauseButtonRects[i].contains(x,y)) return int(i);
    return -1;
}

// Full-width command grid; dialogue is displayed on the upper screen.
inline constexpr TouchRect kCommandButtonRects[] = {
    {10,10,146,94}, {164,10,146,94},
    {10,112,146,94}, {164,112,146,94}
};

inline constexpr int commandButtonAt(unsigned x, unsigned y) {
    for (unsigned i = 0; i < 4; ++i) if (kCommandButtonRects[i].contains(x, y)) return int(i);
    return -1;
}

// Reward choices and recipient moves use the same rectangles for draw and input.
inline constexpr TouchRect kRewardChoiceRects[]={{16,54,88,40},{116,54,88,40},{216,54,88,40}};
inline constexpr TouchRect kRewardMoveRects[]={{16,44,288,32},{16,81,288,32},{16,118,288,32},{16,155,288,32}};
inline constexpr int rewardChoiceAt(unsigned x,unsigned y,unsigned count) {
    if(count>3) return -1;
    for(unsigned i=0;i<count;++i) if(kRewardChoiceRects[i].contains(x,y)) return int(i);
    return -1;
}
struct RewardMoveSelection {
    unsigned selected=0;
    void reset() {selected=0;}
    bool move(int direction,unsigned count) {
        if(!count || count>4 || !direction) return false;
        selected=direction>0 ? (selected+1)%count : (selected+count-1)%count;
        return true;
    }
    static int hit(unsigned x,unsigned y,unsigned count) {
        if(count>4) return -1;
        for(unsigned i=0;i<count;++i) if(kRewardMoveRects[i].contains(x,y)) return int(i);
        return -1;
    }
};

// Reward screen action buttons (Bottom screen 320x240)
inline constexpr TouchRect kRewardClaimButtonRect{16, 170, 136, 54};
inline constexpr TouchRect kRewardSkipButtonRect{168, 170, 136, 54};

}

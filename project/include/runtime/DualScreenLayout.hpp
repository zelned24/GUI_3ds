#pragma once
namespace Pokerogue3DS {
// Zero requests automatic placement; positive values are explicit presentation scale.
inline constexpr float anchoredSpriteScale(float requested,float automatic) {
    return requested>0.0f ? requested : automatic;
}

inline constexpr float nativeCombatSpriteScale(unsigned width,unsigned height,bool boss,unsigned maxHeight) {
    return width && height && !boss && width<=48 && height<=48 && height<=maxHeight/2 ? 2.0f : 1.0f;
}

struct TouchRect {
    unsigned x,y,width,height;
    constexpr bool contains(unsigned px,unsigned py) const {
        return px>=x && px-x<width && py>=y && py-y<height;
    }
};
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
inline constexpr TouchRect kTargetButtonRects[]={{10,182,146,26},{164,182,146,26}};
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

// 2x2 grid in right command window (x: 180..314, y: 8..232)
inline constexpr TouchRect kCommandButtonRects[] = {
    {180, 8, 67, 112},   // 0: Luchar
    {247, 8, 67, 112},   // 1: Balls
    {180, 120, 67, 112},  // 2: Pokémon
    {247, 120, 67, 112}   // 3: Huir
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

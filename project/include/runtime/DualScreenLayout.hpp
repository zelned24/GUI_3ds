#pragma once
#include <cmath>
namespace Pokerogue3DS {
// PlayerPokemon.constructor (pinned upstream): (106,148), adapted 5/4
// and rounded to integer pixels. Sprite size is resolved independently.
inline constexpr float kPlayerBattleAnchorX=133.0f;
inline constexpr float kPlayerBattleAnchorY=185.0f;
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
inline constexpr TouchRect kFrontendConfirmRect{12,205,142,30};
inline constexpr TouchRect kFrontendBackRect{166,205,142,30};
inline constexpr TouchRect kFrontendReadOnlyBackRect{12,205,296,30};
inline constexpr TouchRect kEggListConfirmRect=kFrontendConfirmRect;
inline constexpr TouchRect kEggListBackRect=kFrontendBackRect;
inline constexpr TouchRect eggListRowRectangle(unsigned row) {
    return row<5 ? TouchRect{20,38+row*30,280,28} : TouchRect{};
}
// Nine upstream submenu entries fit the 320x240 touch screen.
inline constexpr unsigned kGlobalMenuRowCount=9;
inline constexpr TouchRect globalMenuRowRectangle(unsigned index) {
    return index<kGlobalMenuRowCount ? TouchRect{24,17+index*20,272,20} : TouchRect{};
}
inline constexpr int globalMenuRowAt(unsigned x,unsigned y) {
    for(unsigned i=0;i<kGlobalMenuRowCount;++i)
        if(globalMenuRowRectangle(i).contains(x,y)) return int(i);
    return -1;
}
// Pokédex uses native 40x30 icons in a 6x4 grid; draw and touch share bounds.
inline constexpr unsigned kPokedexPageSize=24;
inline constexpr TouchRect kPokedexFilterRects[]={{10,24,146,14},{164,24,146,14}};
inline constexpr TouchRect kPokedexPageRects[]={{10,188,96,16},{214,188,96,16}};
inline constexpr TouchRect pokedexCellRectangle(unsigned index) {
    return index<kPokedexPageSize ? TouchRect{10+(index%6)*50,43+(index/6)*36,48,34} : TouchRect{};
}
inline constexpr int pokedexCellAt(unsigned x,unsigned y) {
    for(unsigned i=0;i<kPokedexPageSize;++i) if(pokedexCellRectangle(i).contains(x,y)) return int(i);
    return -1;
}
inline constexpr TouchRect kDialogueAdvanceRect{12,208,156,20};
inline constexpr TouchRect ballMenuRectangle(unsigned index) {return {16,24+index*34,288,32};}
// Floor to complete columns; zero PP stays empty and over-cap values clamp.
inline constexpr unsigned movePpBarPixels(unsigned current,unsigned maximum,unsigned width) {
    return !maximum || !width ? 0 : current>=maximum ? width :
        static_cast<unsigned>((static_cast<unsigned long long>(current)*width)/maximum);
}
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
inline constexpr TouchRect moveConfirmRectangle(bool doubleBattle) {
    return {8,doubleBattle ? 174u : 192u,88,30};
}
inline constexpr TouchRect moveBackRectangle(bool doubleBattle) {
    return {100,doubleBattle ? 174u : 192u,88,30};
}
inline constexpr TouchRect kTargetButtonRects[]={{10,210,146,24},{164,210,146,24}};
inline constexpr int targetButtonAt(unsigned x,unsigned y) {
    for (unsigned i=0;i<2;++i) if (kTargetButtonRects[i].contains(x,y)) return int(i);
    return -1;
}
inline constexpr TouchRect kPartyHeaderRect{8,4,304,18};
inline constexpr TouchRect kPartyFooterRect{8,220,304,18};
inline constexpr TouchRect kPartyConfirmRect{8,220,148,18};
inline constexpr TouchRect kPartyBackRect{164,220,148,18};
inline constexpr TouchRect kPartyButtonRects[]={
    {8,24,304,32},{8,56,304,32},{8,88,304,32},
    {8,120,304,32},{8,152,304,32},{8,184,304,32}
};
inline constexpr int partyButtonAt(unsigned x,unsigned y,unsigned count) {
    if (count>6) return -1;
    for (unsigned i=0;i<count;++i) if (kPartyButtonRects[i].contains(x,y)) return int(i);
    return -1;
}

inline constexpr TouchRect kPauseConfirmRect=kFrontendConfirmRect;
inline constexpr TouchRect kPauseBackRect=kFrontendBackRect;
inline constexpr TouchRect kPauseFeedbackRect{36,158,248,34};
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

// Upper-screen reward cards have separate icon, name, rarity and action areas.
inline constexpr TouchRect rewardCardRectangle(unsigned index) {
    return index<3 ? TouchRect{14+index*127,46,118,175} : TouchRect{};
}
inline constexpr TouchRect rewardCardNameRectangle(unsigned index) {
    const auto card=rewardCardRectangle(index);
    return index<3 ? TouchRect{card.x+6,card.y+68,card.width-12,44} : TouchRect{};
}
inline constexpr unsigned kRewardRarityOffsetY=120;
// Reward choices and recipient moves use the same rectangles for draw and input.
inline constexpr TouchRect kRewardChoiceRects[]={{16,54,88,40},{116,54,88,40},{216,54,88,40}};
inline constexpr TouchRect kRewardMoveConfirmRect{16,200,136,24};
inline constexpr TouchRect kRewardMoveBackRect{168,200,136,24};
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

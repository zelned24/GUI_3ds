#include "game/EggIncubation.hpp"
#include <cassert>
#include <climits>
using namespace Pokerogue3DS;
int main() {
    EggIncubationRecord eggs[]={{7,0,EggTier::COMMON,EggSourceType::GACHA_MOVE,2},
        {9,0,EggTier::RARE,EggSourceType::EVENT,1},
        {3,0,EggTier::EPIC,EggSourceType::GACHA_SHINY,0}};
    uint32_t ready[3]={99,99,99};size_t count=88;
    assert(lapseEggIncubation(eggs,3,reinterpret_cast<uint32_t*>(eggs),3,count)==EggIncubationResult::InvalidInput);
    assert(lapseEggIncubation(eggs,3,ready,1,count)==EggIncubationResult::OutputTooSmall);
    assert(eggs[0].hatchWaves==2 && eggs[1].hatchWaves==1 && ready[0]==99 && count==88);
    assert(lapseEggIncubation(eggs,3,ready,3,count)==EggIncubationResult::Ok);
    assert(count==2 && ready[0]==9 && ready[1]==3 && eggs[0].hatchWaves==1);
    assert(lapseEggIncubation(eggs,3,ready,3,count)==EggIncubationResult::Ok);
    assert(count==3 && ready[0]==7 && ready[1]==9 && ready[2]==3);
    eggs[2].id=9;count=88;
    assert(lapseEggIncubation(eggs,3,ready,3,count)==EggIncubationResult::DuplicateId && count==88);
    eggs[2].id=3;eggs[0].tier=static_cast<EggTier>(255);
    assert(lapseEggIncubation(eggs,3,ready,3,count)==EggIncubationResult::InvalidTier);
    eggs[0].tier=EggTier::COMMON;eggs[0].sourceType=static_cast<EggSourceType>(255);
    assert(lapseEggIncubation(eggs,3,ready,3,count)==EggIncubationResult::InvalidSource);
    eggs[0].sourceType=EggSourceType::GACHA_MOVE;eggs[0].hatchWaves=INT_MIN;
    assert(lapseEggIncubation(eggs,3,ready,3,count)==EggIncubationResult::Ok && eggs[0].hatchWaves==INT_MIN);
    assert(lapseEggIncubation(nullptr,0,nullptr,0,count)==EggIncubationResult::Ok && count==0);
    assert(lapseEggIncubation(nullptr,1,ready,3,count)==EggIncubationResult::InvalidInput);
}

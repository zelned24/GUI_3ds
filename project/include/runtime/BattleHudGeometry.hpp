#pragma once
#include <cmath>
#include <cstdint>
namespace Pokerogue3DS {
// Pinned PlayerBattleInfo.getLevelDurationMultiplier / doUpdateExpAnimation.
// Return false for invalid native inputs instead of inventing an animation.
inline bool expSegmentTiming(unsigned lastLevel,unsigned finalLevel,double ratio,unsigned speed,
    bool visible,double& durationMs,double& levelPauseMs) {
    durationMs=levelPauseMs=0;
    if(!lastLevel || finalLevel<lastLevel
        || !std::isfinite(ratio) || ratio<0 || ratio>1 || speed>3) return false;
    const double delta=finalLevel-lastLevel;
    const double remaining=1-std::fmin(delta,10.0)/10;
    const double levelMultiplier=std::fmax(remaining*remaining*remaining,0.1);
    constexpr double pi=3.14159265358979323846;
    const double levelEase=1-std::cos((1-double(lastLevel>100 ? lastLevel-100 : 0)/150)*pi/2);
    durationMs=visible ? ratio*1650*levelEase*levelMultiplier : 0;
    if(speed) durationMs=speed>=3 ? 0 : durationMs/double(1u<<speed);
    levelPauseMs=500*levelMultiplier;
    return true;
}
inline double expSegmentFraction(double from,double to,double elapsedMs,double durationMs) {
    if(durationMs<=0 || elapsedMs>=durationMs) return to;
    if(elapsedMs<=0) return from;
    constexpr double pi=3.14159265358979323846;
    return from+(to-from)*(1-std::cos(elapsedMs/durationMs*pi/2));
}
inline const char* hudLevelDigitAtlas(bool player,uint16_t level,uint16_t resolvedCap) {
    return player && resolvedCap && level>=resolvedCap ? "numbers_red" : "numbers";
}
// EnemyBattleInfo.updateBossSegmentDividers, quantized to native pixel columns.
inline unsigned bossDividerPixel(uint32_t maxHp,unsigned segments,unsigned boundary,unsigned width) {
    if(!maxHp || segments<2 || !boundary || boundary>=segments || !width) return 0;
    const double ratio=std::round((double(maxHp)/segments)*boundary)/maxHp;
    const double pixel=std::round(ratio*width-0.5);
    return pixel<0 ? 0 : (pixel>=width ? width-1 : static_cast<unsigned>(pixel));
}
}

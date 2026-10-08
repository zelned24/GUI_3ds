#pragma once
#include <cmath>
#include <cstdint>
namespace Pokerogue3DS {
// Pinned BattleInfo.updatePokemonHp: visual state only; never writes battle HP.
inline double hpTweenDuration(unsigned lastHp,unsigned targetHp,unsigned speed,bool instant=false) {
    if(instant || speed>=3) return 0;
    const double delta=lastHp>targetHp ? lastHp-targetHp : targetHp-lastHp;
    return std::fmin(5000.0,std::fmax(250.0,delta*5))/double(1u<<speed);
}
struct HpRatioTween {
    double from=0,target=0,durationMs=0;
    uint64_t startMs=0;
    unsigned maxHp=0,targetHp=0;
    bool initialized=false;
    double sample(uint64_t now) const {
        const uint64_t elapsed=now>=startMs ? now-startMs : 0;
        if(durationMs<=0 || double(elapsed)>=durationMs) return target;
        constexpr double pi=3.14159265358979323846;
        return from+(target-from)*std::sin(double(elapsed)/durationMs*pi/2);
    }
    unsigned displayedHp(uint64_t now) const {
        return static_cast<unsigned>(std::ceil(std::fmin(1.0,std::fmax(0.0,sample(now)))*maxHp));
    }
    double update(unsigned hp,unsigned maximum,uint64_t now,unsigned speed=0,bool instant=false) {
        if(speed>=3) instant=true;
        hp=hp>maximum ? maximum : hp;
        const double ratio=maximum ? double(hp)/maximum : 0;
        if(!initialized) {
            initialized=true;from=target=ratio;targetHp=hp;maxHp=maximum;startMs=now;durationMs=0;
        } else if(targetHp!=hp || maxHp!=maximum || instant) {
            const unsigned lastHp=displayedHp(now);
            from=sample(now);target=ratio;targetHp=hp;maxHp=maximum;
            durationMs=hpTweenDuration(lastHp,hp,speed,instant);startMs=now;
        }
        return sample(now);
    }
};
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

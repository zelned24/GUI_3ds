#pragma once
#include "runtime/BattleHudGeometry.hpp"
#include "game/PokemonExperience.hpp"
#include <algorithm>
namespace Pokerogue3DS {
// Visual level segments from pinned PlayerBattleInfo.doUpdateExpAnimation.
// Uses canonical growth curves; never changes the Pokemon's EXP or level.
class ExperienceBarTimeline {
public:
    void clear() {*this={};}
    unsigned level() const {return m_level;}
    double fraction() const {return m_ratio;}
    uint32_t total() const {return m_displayed;}
    bool update(const char* growth,unsigned finalLevel,uint32_t total,uint64_t now,bool instant=false) {
        uint32_t low=0,high=0;
        if(!bounds(growth,finalLevel,low,high) || total<low || total>=high) return false;
        const auto snapshot=[&]() {
            m_initialized=true;m_phase=Idle;m_level=finalLevel;m_finalLevel=finalLevel;
            m_target=total;m_displayed=total;m_ratio=double(total-low)/(high-low);
            return true;
        };
        if(!m_initialized || instant || finalLevel<m_level || total<m_displayed) return snapshot();
        if(!advance(growth,double(now))) return false;
        // The old animation may advance past an incoming correction at a coarse
        // timestamp. Recheck before subtracting the current level's EXP floor.
        if(finalLevel<m_level || total<m_displayed) return snapshot();
        if(m_target!=total || m_finalLevel!=finalLevel) {
            m_target=total;m_finalLevel=finalLevel;
            // A full bar belongs to the preceding level during its pause.
            // Keep that pause (and an in-flight level-up segment) before
            // consuming the latest target in the following segment.
            if(m_phase!=Pause && !(m_phase==Tween && m_levelUp && m_level<finalLevel)) {
                if(!begin(growth,double(now))) return false;
            }
        }
        return advance(growth,double(now));
    }
private:
    enum Phase {Idle,Tween,Pause};
    static bool bounds(const char* growth,unsigned level,uint32_t& low,uint32_t& high) {
        return level && level<10000 &&
            pokemonTotalExperienceForLevel(growth,uint16_t(level),low)==PokemonExperienceResult::Ok &&
            pokemonTotalExperienceForLevel(growth,uint16_t(level+1),high)==PokemonExperienceResult::Ok && high>low;
    }
    bool begin(const char* growth,double now) {
        if(!bounds(growth,m_level,m_low,m_high)) return false;
        if(m_finalLevel<m_level || m_target<m_low) return false;
        m_levelUp=m_level<m_finalLevel;
        m_from=m_ratio;m_to=m_levelUp ? 1 : double(m_target-m_low)/(m_high-m_low);
        if(m_to<0 || m_to>1 || !expSegmentTiming(m_level,m_finalLevel,m_to,0,true,m_duration,m_pause)) return false;
        m_start=now;m_phase=Tween;return true;
    }
    bool advance(const char* growth,double now) {
        // Each iteration completes a tween or pause; bounded by the canonical level range.
        for(unsigned step=0;step<20000 && m_phase!=Idle;++step) {
            const double elapsed=std::max(0.0,now-m_start);
            if(m_phase==Pause) {
                if(elapsed<m_pause) return true;
                m_start+=m_pause;m_ratio=0;
                if(!begin(growth,m_start)) return false;
                continue;
            }
            m_ratio=expSegmentFraction(m_from,m_to,elapsed,m_duration);
            m_displayed=static_cast<uint32_t>(m_low+double(m_high-m_low)*m_ratio);
            if(elapsed<m_duration) return true;
            m_start+=m_duration;
            if(m_levelUp) {
                m_displayed=m_high;++m_level;m_phase=Pause;
            } else {m_displayed=m_target;m_phase=Idle;return true;}
        }
        return m_phase==Idle;
    }
    bool m_initialized=false,m_levelUp=false;
    Phase m_phase=Idle;
    unsigned m_level=0,m_finalLevel=0;
    uint32_t m_target=0,m_displayed=0,m_low=0,m_high=0;
    double m_ratio=0,m_from=0,m_to=0,m_start=0,m_duration=0,m_pause=0;
};
} // namespace Pokerogue3DS

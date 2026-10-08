#pragma once
#include "gfx/renderer2d.hpp"
#include "game/FirstRunRuntime.hpp"
#include "game/PokemonExperience.hpp"
#include "content/BattleHudTextures.hpp"
#include "runtime/TypePresentation.hpp"
#include "runtime/BattleHudGeometry.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

namespace Pokerogue3DS {

class BattleHudPresenter {
public:
    void clear() {
        for (auto& sheet : m_sheets) {
            if (sheet) C2D_SpriteSheetFree(sheet);
            sheet = nullptr;
        }
        resetExperienceDisplay();
        resetHpDisplay();
    }
    void resetHpDisplay() {m_hpDisplays={};}
    void resetExperienceDisplay() {m_displayedExp=0;m_lastPlayerId=0;m_expInitialized=false;}
    uint32_t displayedExperience() const {return m_displayedExp;}
    ~BattleHudPresenter() { clear(); }
    BattleHudPresenter() = default;
    BattleHudPresenter(const BattleHudPresenter&) = delete;
    BattleHudPresenter& operator=(const BattleHudPresenter&) = delete;

    void draw(Renderer2D& renderer, const ResolvedPokemon& actor, bool player, float x, float y, bool isCaught = false,uint16_t experienceLevelCap=0,uint64_t animationTimeMs=0) {
        if (!actor.actorIdentityResolved) return;
        const unsigned index = player ? 0 : (actor.bossState.segmentCount != 0 ? 2 : 1);
        const auto& texture = kBattleHudTextures[index];
        if (!m_sheets[index]) {
            m_sheets[index] = C2D_SpriteSheetLoad(texture.path);
            if (m_sheets[index]) {
                const auto img = C2D_SpriteSheetGetImage(m_sheets[index], 0);
                if (img.tex) C3D_TexSetFilter(img.tex, GPU_NEAREST, GPU_NEAREST);
            }
        }
        if (!m_sheets[index]) return;
        constexpr float scale = 1.0f;
        const float hudW = texture.width * scale;
        const float hudH = texture.height * scale;
        renderer.drawImageDirect(C2D_SpriteSheetGetImage(m_sheets[index], 0), x, y, hudW, hudH);

        // Authentic PokéRogue BattleInfo text styling: text #f8f8f8, shadow #6b5a73
        constexpr uint32_t kTextColor = C2D_Color32(0xf8, 0xf8, 0xf8, 255);
        constexpr uint32_t kShadowColor = C2D_Color32(0x6b, 0x5a, 0x73, 255);

        // Name origin also anchors the status and owned indicator row.
        float nameOffsetX = player ? 15.0f : 6.0f;
        // Draw Name with 1px drop shadow
        const char* name = actor.localizedName ? actor.localizedName : "";
        const float nameX = x + nameOffsetX;
        const float nameY = y + (player ? 6.0f : 4.0f);
        char level[6];
        std::snprintf(level,sizeof(level),"%u",unsigned(actor.level));
        const unsigned levelDigits=std::strlen(level);
        const float levelShift=levelDigits>3 ? (levelDigits-3)*8.0f : 0;
        const float levelX = x + (player ? 89.0f : (actor.bossState.segmentCount ? 126.0f : 80.0f))-levelShift;
        const float nameWidth=levelX-nameX-14.0f;
        float displayedNameWidth=0;
        char displayName[128];
        if(renderer.abbreviateText(name,0.30f,nameWidth,displayName,sizeof(displayName),displayedNameWidth,true)) {
            renderer.drawText(displayName,nameX+1,nameY+1,0.30f,kShadowColor);
            renderer.drawText(displayName,nameX,nameY,0.30f,kTextColor);
        }

        // Gender icon next to name
        const char* genderSymbol = nullptr;
        uint32_t genderColor = 0xFFFFFFFF;
        if (actor.battleState.gender == PokemonGender::Male) {
            genderSymbol = "♂";
            genderColor = C2D_Color32(110, 180, 255, 255);
        } else if (actor.battleState.gender == PokemonGender::Female) {
            genderSymbol = "♀";
            genderColor = C2D_Color32(255, 140, 220, 255);
        }
        if (genderSymbol) {
            const float genderX = nameX + displayedNameWidth + 3.0f;
            renderer.drawText(genderSymbol, genderX + 1.0f, nameY + 1.0f, 0.28f, kShadowColor);
            renderer.drawText(genderSymbol, genderX, nameY, 0.28f, genderColor);
        }

        // BattleInfo.setLevelDisplay uses 8x8 digit images, not scaled font glyphs.
        renderer.drawHudGraphic("overlay_lv","overlay_lv",levelX-3,y+(player ? 8.0f : 7.0f));
        const char* levelAtlas=hudLevelDigitAtlas(player,actor.level,experienceLevelCap);
        for(unsigned i=0;i<levelDigits;++i) {
            const char digit[2]={level[i],0};
            renderer.drawHudGraphic(levelAtlas,digit,levelX+6+i*8,y+(player ? 7.0f : 6.0f));
        }

        // BattleInfo.setTypes uses single/dual compact icons outside the panel.
        const auto* species = PokerogueContent::findSpeciesByDex(actor.dex);
        const char* type1=nullptr;const char* type2=nullptr;
        if (canonicalPresentationTypes(actor.dex,actor.formId,type1,type2)) {
            const bool dual=type2 && *type2;
            const float iconX=player ? x-9.0f : x+hudW-15.0f;
            const float iconY=y+(player ? 4.0f : 0.0f);
            renderer.drawHudTypeIcon(type1,player,0,dual,iconX,iconY);
            if(dual) renderer.drawHudTypeIcon(type2,player,1,true,iconX,iconY+(player ? 16.0f : 13.0f));
        }

        // Native coordinates derived from BattleInfo origins and boss offsets.
        const auto identity=actor.battleState.pokemonId;
        HpDisplay* display=nullptr;
        for(auto& state:m_hpDisplays) if(state.bound && state.identity==identity && state.player==player) {display=&state;break;}
        if(!display) {
            for(auto& state:m_hpDisplays) if(!state.bound) {display=&state;break;}
            if(!display) display=&*std::min_element(m_hpDisplays.begin(),m_hpDisplays.end(),
                [](const HpDisplay& a,const HpDisplay& b){return a.lastSeen<b.lastSeen;});
            *display={};display->bound=true;display->identity=identity;display->player=player;
        }
        display->lastSeen=animationTimeMs;
        // Timestamp-less callers and unresolved identities use an instant projection.
        // DEFAULT hpBarSpeed (0); settings persistence/phase waiting remain separate work.
        const float fraction=static_cast<float>(display->tween.update(actor.battleState.hp,
            actor.battleState.maxHp,animationTimeMs,0,!animationTimeMs || !identity));
        const bool boss=!player && actor.bossState.segmentCount;
        const float hpX=x+(player || boss ? 69.0f : 59.0f);
        const float hpY=y+(boss ? 18.0f : 20.0f);
        const float hpMaxW=boss ? 86.0f : 48.0f;
        renderer.drawHudGraphic(boss ? "overlay_hp_label_boss" : "overlay_hp_label",boss ? "overlay_hp_label_boss" : "overlay_hp_label",hpX-(boss ? 26 : 14),y+(boss ? 16 : 17));
        renderer.drawHudBar(false,boss,fraction,hpX,hpY);

        // EnemyBattleInfo.updateBossSegmentDividers: one-pixel lines inside the bar.
        if(boss && actor.battleState.maxHp && actor.bossState.segmentCount>1) {
            for(unsigned s=1;s<actor.bossState.segmentCount;++s) {
                const float dividerX=hpX+bossDividerPixel(actor.battleState.maxHp,actor.bossState.segmentCount,s,static_cast<unsigned>(hpMaxW));
                const uint32_t color=actor.bossState.segmentIndex>=s ? C2D_Color32(255,255,255,255) : C2D_Color32(64,64,64,255);
                renderer.drawRect(dividerX,hpY+1,1,3,color);
            }
        }

        // BattleInfo.updateStatusIcon selects the localized status atlas frame.
        const char* statusKey=nullptr;
        if(actor.battleState.status.present) switch(actor.battleState.status.effect) {
            case PokemonStatusEffect::Paralysis: statusKey="paralysis";break;
            case PokemonStatusEffect::Poison: statusKey="poison";break;
            case PokemonStatusEffect::Toxic: statusKey="toxic";break;
            case PokemonStatusEffect::Burn: statusKey="burn";break;
            case PokemonStatusEffect::Sleep: statusKey="sleep";break;
            case PokemonStatusEffect::Freeze: statusKey="freeze";break;
            default:break;
        }
        if(statusKey) renderer.drawHudIndicator(statusKey,false,x+nameOffsetX,y+(player ? 17.0f : 16.0f));
        // Owned marker shares the name's lower row, offset after a visible status.
        if(!player && isCaught) renderer.drawHudIndicator("owned",true,x+nameOffsetX+(statusKey ? 22.0f : 0.0f),y+(player ? 17.0f : 16.0f));

        // Player specific: HP numbers ("206 / 276") and smooth EXP bar animation
        if (player) {
            char hp[12];
            std::snprintf(hp,sizeof(hp),"%u/%u",display->tween.displayedHp(animationTimeMs),unsigned(actor.battleState.maxHp));
            const unsigned count=std::strlen(hp);
            for(unsigned i=0;i<count;++i) {
                const char digit[2]={hp[i],0};
                renderer.drawHudGraphic("numbers",digit,x+111-(count-1-i)*8,y+27);
            }

            // Smooth EXP bar lerp animation towards actor.totalExperience
            if (!m_expInitialized || m_lastPlayerId != actor.battleState.pokemonId) {
                m_displayedExp = actor.totalExperience;
                m_lastPlayerId = actor.battleState.pokemonId;
                m_expInitialized = true;
            } else if (m_displayedExp < actor.totalExperience) {
                const uint32_t diff = actor.totalExperience - m_displayedExp;
                const uint32_t step = std::max<uint32_t>(1, diff / 8 + (diff % 8 != 0));
                m_displayedExp += step;
            } else if (m_displayedExp > actor.totalExperience) {
                m_displayedExp = actor.totalExperience;
            }

            // PlayerBattleInfo overlay_exp: origin (130-98, 21+18), 85x2.
            const float expX=x+32.0f,expY=y+39.0f;
            renderer.drawHudGraphic("overlay_exp_label","overlay_exp_label",x+23,y+34);
            if (species) {
                uint16_t animLevel = actor.level;
                uint32_t lvlExp = 0;
                uint32_t nextExp = 0;
                pokemonTotalExperienceForLevel(species->growthRate, animLevel, lvlExp);
                while (animLevel > 1 && m_displayedExp < lvlExp) {
                    --animLevel;
                    pokemonTotalExperienceForLevel(species->growthRate, animLevel, lvlExp);
                }
                pokemonTotalExperienceForLevel(species->growthRate, animLevel + 1, nextExp);
                float expFraction = 0.0f;
                if (nextExp > lvlExp && m_displayedExp >= lvlExp) {
                    expFraction = float(m_displayedExp - lvlExp) / float(nextExp - lvlExp);
                    if (expFraction > 1.0f) expFraction = 1.0f;
                }
                if (expFraction > 0.0f) {
                    renderer.drawHudBar(true,false,expFraction,expX,expY);
                }
            }
        }
    }

private:
    struct HpDisplay {uint32_t identity=0;uint64_t lastSeen=0;bool bound=false,player=false;HpRatioTween tween;};
    // Four concurrent field actors; catalogue size does not affect this cache.
    std::array<HpDisplay,4> m_hpDisplays{};
    C2D_SpriteSheet m_sheets[3]{};
    uint32_t m_displayedExp = 0;
    uint32_t m_lastPlayerId = 0;
    bool m_expInitialized = false;
};

} // namespace Pokerogue3DS

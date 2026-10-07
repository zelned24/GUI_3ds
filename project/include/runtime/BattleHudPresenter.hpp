#pragma once
#include "gfx/renderer2d.hpp"
#include "game/FirstRunRuntime.hpp"
#include "game/PokemonExperience.hpp"
#include "content/BattleHudTextures.hpp"
#include "runtime/TypePresentation.hpp"
#include <algorithm>
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
        m_displayedExp = 0;
        m_lastPlayerDex = 0;
    }
    ~BattleHudPresenter() { clear(); }
    BattleHudPresenter() = default;
    BattleHudPresenter(const BattleHudPresenter&) = delete;
    BattleHudPresenter& operator=(const BattleHudPresenter&) = delete;

    void draw(Renderer2D& renderer, const ResolvedPokemon& actor, bool player, float x, float y, bool isCaught = false) {
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
        constexpr float scale = 1.25f;
        const float hudW = texture.width * scale;
        const float hudH = texture.height * scale;
        renderer.drawImageDirect(C2D_SpriteSheetGetImage(m_sheets[index], 0), x, y, hudW, hudH);

        // Authentic PokéRogue BattleInfo text styling: text #f8f8f8, shadow #6b5a73
        constexpr uint32_t kTextColor = C2D_Color32(0xf8, 0xf8, 0xf8, 255);
        constexpr uint32_t kShadowColor = C2D_Color32(0x6b, 0x5a, 0x73, 255);
        constexpr uint32_t kLevelColor = C2D_Color32(0xff, 0xe0, 0x60, 255);
        constexpr uint32_t kLevelShadow = C2D_Color32(0x70, 0x50, 0x10, 255);

        // Name origin also anchors the status and owned indicator row.
        float nameOffsetX = player ? 18.0f : 12.0f;
        // Draw Name with 1px drop shadow
        const char* name = actor.localizedName ? actor.localizedName : "";
        const float nameX = x + nameOffsetX;
        const float nameY = y + (player ? 7.0f : 5.0f);
        const float levelX = x + (player ? 112.0f : (actor.bossState.segmentCount ? 142.0f : 102.0f));
        const float nameWidth=levelX-nameX-16.0f;
        float displayedNameWidth=0;
        renderer.drawTextFitted(name,nameX+1,nameY+1,0.36f,nameWidth,kShadowColor);
        renderer.drawTextFitted(name,nameX,nameY,0.36f,nameWidth,kTextColor,&displayedNameWidth);

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
            renderer.drawText(genderSymbol, genderX + 1.0f, nameY + 1.0f, 0.34f, kShadowColor);
            renderer.drawText(genderSymbol, genderX, nameY, 0.34f, genderColor);
        }

        // Draw Level ("N. %u") with drop shadow
        char level[24];
        std::snprintf(level, sizeof(level), "N. %u", unsigned(actor.level));
        const float levelY = y + (player ? 7.0f : 5.0f);
        renderer.drawText(level, levelX + 1.0f, levelY + 1.0f, 0.34f, kLevelShadow);
        renderer.drawText(level, levelX, levelY, 0.34f, kLevelColor);

        // BattleInfo.setTypes uses single/dual compact icons outside the panel.
        const auto* species = PokerogueContent::findSpeciesByDex(actor.dex);
        const char* type1=nullptr;const char* type2=nullptr;
        if (canonicalPresentationTypes(actor.dex,actor.formId,type1,type2)) {
            const bool dual=type2 && *type2;
            const float iconX=player ? x-15.0f : x+hudW-5.0f;
            const float iconY=y+(player ? 0.0f : 4.0f);
            renderer.drawHudTypeIcon(type1,player,0,dual,iconX,iconY);
            if(dual) renderer.drawHudTypeIcon(type2,player,1,true,iconX,iconY+(player ? 16.0f : 14.0f));
        }

        // HP Bar in the exact texture groove (hpH = 2.5f, fits frame groove cleanly):
        const float fraction = actor.battleState.maxHp ? float(actor.battleState.hp) / actor.battleState.maxHp : 0.0f;
        const float clampedFraction = fraction < 0.0f ? 0.0f : (fraction > 1.0f ? 1.0f : fraction);
        const float hpX = x + (player ? 86.0f : (actor.bossState.segmentCount ? 86.0f : 74.0f));
        const float hpY = y + 25.0f;
        const float hpMaxW = actor.bossState.segmentCount ? 107.0f : 60.0f;
        const float hpH = 2.5f;

        // Authentic PokéRogue HP colors (High: #39ff7b, Medium: #f3b200, Low: #fb3041)
        const uint32_t hpFillColor = clampedFraction > 0.5f
            ? C2D_Color32(57, 255, 123, 255)
            : clampedFraction > 0.25f
            ? C2D_Color32(243, 178, 0, 255)
            : C2D_Color32(251, 48, 65, 255);
        if (clampedFraction > 0.0f) {
            renderer.drawRect(hpX, hpY, hpMaxW * clampedFraction, hpH, hpFillColor);
        }

        // Boss HP shield segment indicators
        if (!player && actor.bossState.segmentCount > 0) {
            for (uint8_t s = 0; s < actor.bossState.segmentCount && s < 6; ++s) {
                const bool remaining = s >= actor.bossState.segmentIndex;
                const float segX = hpX + s * (hpMaxW / actor.bossState.segmentCount);
                renderer.drawRect(segX, hpY - 5.0f, 8.0f, 3.0f, remaining ? C2D_Color32(0x40, 0xd8, 0xe8, 255) : C2D_Color32(0x40, 0x40, 0x40, 255));
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
        if(statusKey) renderer.drawHudIndicator(statusKey,false,x+nameOffsetX,y+23.0f);
        // Owned marker shares the name's lower row, offset after a visible status.
        if(!player && isCaught) renderer.drawHudIndicator("owned",true,x+nameOffsetX+(statusKey ? 22.0f : 0.0f),y+23.0f);

        // Player specific: HP numbers ("206 / 276") and smooth EXP bar animation
        if (player) {
            char hp[32];
            std::snprintf(hp, sizeof(hp), "%u / %u", unsigned(actor.battleState.hp), unsigned(actor.battleState.maxHp));
            const float hpTextX = x + 82.0f;
            const float hpTextY = y + 33.0f;
            renderer.drawTextFitted(hp,hpTextX+1,hpTextY+1,0.26f,hudW-86,kShadowColor);
            renderer.drawTextFitted(hp,hpTextX,hpTextY,0.26f,hudW-86,kTextColor);

            // Smooth EXP bar lerp animation towards actor.totalExperience
            if (m_lastPlayerDex != actor.dex || m_displayedExp == 0) {
                m_displayedExp = actor.totalExperience;
                m_lastPlayerDex = actor.dex;
            } else if (m_displayedExp < actor.totalExperience) {
                const uint32_t diff = actor.totalExperience - m_displayedExp;
                const uint32_t step = std::max<uint32_t>(1, diff / 8 + (diff % 8 != 0));
                m_displayedExp += step;
            } else if (m_displayedExp > actor.totalExperience) {
                m_displayedExp = actor.totalExperience;
            }

            // EXP bar (groove at x+35 scaled -> x+44, y+36 scaled -> y+45)
            const float expX = x + 44.0f;
            const float expY = y + 45.0f;
            const float expMaxW = 104.0f;
            const float expH = 2.0f;

            // "EXP" label before EXP bar
            renderer.drawText("EXP", expX - 16.0f, expY - 2.5f, 0.18f, C2D_Color32(0, 210, 255, 255));

            renderer.drawRect(expX, expY, expMaxW, expH, C2D_Color32(0x18, 0x18, 0x22, 255));
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
                    renderer.drawRect(expX, expY, expMaxW * expFraction, expH, C2D_Color32(0, 186, 243, 255));
                }
            }
        }
    }

private:
    C2D_SpriteSheet m_sheets[3]{};
    uint32_t m_displayedExp = 0;
    uint16_t m_lastPlayerDex = 0;
};

} // namespace Pokerogue3DS

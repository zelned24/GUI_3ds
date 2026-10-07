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

        // Enemy caught indicator (small Pokéball dot before name)
        float nameOffsetX = player ? 18.0f : 12.0f;
        if (!player && isCaught) {
            // Draw mini Pokéball mark
            renderer.drawRect(x + nameOffsetX - 6.0f, y + 6.0f, 4.0f, 2.0f, C2D_Color32(230, 40, 40, 255));
            renderer.drawRect(x + nameOffsetX - 6.0f, y + 8.0f, 4.0f, 2.0f, C2D_Color32(245, 245, 245, 255));
        }

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

        // Type badges inside frame slot under name (size 28x10)
        const auto* species = PokerogueContent::findSpeciesByDex(actor.dex);
        if (species) {
            if (player) {
                const float badgeX = x + 18.0f;
                const float badgeY = y + 25.0f;
                drawTypeBadge(renderer, species->type1, badgeX, badgeY, 28.0f, 10.0f, 0.18f);
                if (species->type2 && *species->type2 && !typeIEquals(species->type1, species->type2)) {
                    drawTypeBadge(renderer, species->type2, badgeX + 30.0f, badgeY, 28.0f, 10.0f, 0.18f);
                }
            } else {
                const float badgeX = x + 12.0f;
                const float badgeY = y + 23.0f;
                drawTypeBadge(renderer, species->type1, badgeX, badgeY, 28.0f, 10.0f, 0.18f);
                if (species->type2 && *species->type2 && !typeIEquals(species->type1, species->type2)) {
                    drawTypeBadge(renderer, species->type2, badgeX + 30.0f, badgeY, 28.0f, 10.0f, 0.18f);
                }
            }
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

        // Status badge (PAR, VEN, TOX, QUE, DOR, CON)
        if (actor.battleState.status.present && actor.battleState.status.effect != PokemonStatusEffect::None) {
            const char* tag = "";
            uint32_t tagColor = C2D_Color32(0x80, 0x80, 0x80, 255);
            switch (actor.battleState.status.effect) {
                case PokemonStatusEffect::Paralysis: tag = "PAR"; tagColor = C2D_Color32(240, 180, 0, 255); break;
                case PokemonStatusEffect::Poison: tag = "VEN"; tagColor = C2D_Color32(160, 64, 160, 255); break;
                case PokemonStatusEffect::Toxic: tag = "TOX"; tagColor = C2D_Color32(120, 30, 140, 255); break;
                case PokemonStatusEffect::Burn: tag = "QUE"; tagColor = C2D_Color32(240, 80, 30, 255); break;
                case PokemonStatusEffect::Sleep: tag = "DOR"; tagColor = C2D_Color32(140, 140, 150, 255); break;
                case PokemonStatusEffect::Freeze: tag = "CON"; tagColor = C2D_Color32(60, 180, 240, 255); break;
                default: break;
            }
            if (*tag) {
                const float tagX = x + (player ? 52.0f : 38.0f);
                const float tagY = y + 22.0f;
                renderer.drawRect(tagX, tagY, 20.0f, 8.0f, tagColor);
                renderer.drawText(tag, tagX + 1.0f, tagY, 0.22f, C2D_Color32(255, 255, 255, 255));
            }
        }

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

#pragma once
#include "gfx/renderer2d.hpp"
#include "runtime/DualScreenLayout.hpp"
#include "runtime/PokemonIconPresenter.hpp"
#include "runtime/TitleMenuPresenter.hpp"
#include "runtime/TypePresentation.hpp"
#include "game/FirstRunRuntime.hpp"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace Pokerogue3DS {

class PartyMenuPresenter {
public:
    void clear() { m_icons.clear(); m_cursor.clear(); }
    bool open = false;
    unsigned selected = 0;

    bool available(const FirstRunRuntime& game) const {
        return game.presentationStage() == NativeSaveStage::BattleActive &&
            !game.moveLearningPending() && !game.evolutionPending() &&
            !game.capturePartyChoicePending() && !game.rewardsPending();
    }

    void move(int direction, unsigned count) {
        if (!count) return;
        selected = unsigned((int(selected) + direction + int(count)) % int(count));
    }

    void draw(Renderer2D& renderer, const FirstRunRuntime& game) {
        renderer.clear(C2D_Color32(36, 28, 44, 255));

        // Header
        renderer.drawText("EQUIPO POKÉMON", 12, 10, 0.40f, C2D_Color32(245, 245, 250, 255));

        const auto& context = game.presentation();
        const unsigned count = context.playerPartyCount ? context.playerPartyCount : 1;

        for (unsigned i = 0; i < count && i < 6; ++i) {
            const auto& actor = (i == context.activePlayerPartyIndex) ? context.player : context.playerParty[i];
            const auto& bounds = kPartyButtonRects[i];
            const float y = bounds.y;
            const bool isSel = (i == selected);

            if (isSel) {
                renderer.drawRect(bounds.x, y, bounds.width, bounds.height, C2D_Color32(255, 235, 70, 255));
                renderer.drawWindow(bounds.x + 1, y + 1, bounds.width - 2, bounds.height - 2);
            } else {
                renderer.drawWindow(bounds.x, y, bounds.width, bounds.height);
            }

            uint16_t formIndex = 0;
            if (actor.formId) {
                for (const auto& form : PokerogueContent::kForms) {
                    if (std::strcmp(form.id, actor.formId) == 0) { formIndex = form.upstreamFormIndex; break; }
                }
            }
            m_icons.draw(renderer, actor.dex, formIndex, bounds.x + 14, y + 2, 1.0f, 1.0f);

            // Name + Gender
            const char* name = actor.localizedName ? actor.localizedName : "Pokémon";
            const char* gender=actor.battleState.gender==PokemonGender::Male ? "♂" :
                actor.battleState.gender==PokemonGender::Female ? "♀" : nullptr;
            float nameWidth=0;
            const float nameSize=renderer.drawTextFitted(name, bounds.x + 40, y + 3, 0.32f, gender ? 74 : 88,
                isSel ? C2D_Color32(255, 255, 255, 255) : C2D_Color32(220, 215, 230, 255),&nameWidth);
            if(gender) renderer.drawText(gender,bounds.x+40+nameWidth+3,y+3,nameSize,
                actor.battleState.gender==PokemonGender::Male ? C2D_Color32(110,180,255,255) : C2D_Color32(255,140,220,255));

            if(isSel) m_cursor.drawCursor(renderer,bounds.x+3,y+3,nameSize);

            // Level ("N. %u")
            char lvl[16];
            std::snprintf(lvl, sizeof(lvl), "N.%u", unsigned(actor.level));
            renderer.drawTextFitted(lvl, bounds.x + 132, y + 4, 0.28f, 42, C2D_Color32(255, 225, 90, 255));

            // Active or status indicator
            if (actor.battleState.hp == 0) {
                renderer.drawText("DEB", bounds.x + 178, y + 4, 0.26f, C2D_Color32(240, 50, 50, 255));
            } else if (actor.battleState.status.present && actor.battleState.status.effect != PokemonStatusEffect::None) {
                const char* st = "EST";
                uint32_t stCol = C2D_Color32(240, 180, 30, 255);
                switch (actor.battleState.status.effect) {
                    case PokemonStatusEffect::Paralysis: st = "PAR"; stCol = C2D_Color32(240, 180, 0, 255); break;
                    case PokemonStatusEffect::Poison: st = "VEN"; stCol = C2D_Color32(160, 64, 160, 255); break;
                    case PokemonStatusEffect::Toxic: st = "TOX"; stCol = C2D_Color32(120, 30, 140, 255); break;
                    case PokemonStatusEffect::Burn: st = "QUE"; stCol = C2D_Color32(240, 80, 30, 255); break;
                    case PokemonStatusEffect::Sleep: st = "DOR"; stCol = C2D_Color32(140, 140, 150, 255); break;
                    case PokemonStatusEffect::Freeze: st = "CON"; stCol = C2D_Color32(60, 180, 240, 255); break;
                    default: break;
                }
                renderer.drawText(st, bounds.x + 178, y + 4, 0.26f, stCol);
            } else if (i == context.activePlayerPartyIndex) {
                renderer.drawText("ACT", bounds.x + 178, y + 4, 0.26f, C2D_Color32(80, 220, 140, 255));
            }

            // "PS" label and HP track groove
            const float hpX = bounds.x + 210;
            renderer.drawText("PS", hpX, y + 4, 0.20f, C2D_Color32(245, 195, 60, 255));

            const float frac = actor.battleState.maxHp ? float(std::min(actor.battleState.hp,actor.battleState.maxHp)) / actor.battleState.maxHp : 0.0f;
            const uint32_t barCol = frac > 0.5f ? C2D_Color32(60, 220, 100, 255)
                : frac > 0.25f ? C2D_Color32(245, 180, 20, 255) : C2D_Color32(235, 60, 60, 255);
            renderer.drawRect(hpX, y + 15, 84, 3, C2D_Color32(30, 28, 38, 255));
            if (frac > 0.0f) {
                renderer.drawRect(hpX, y + 15, std::max(1.0f,std::floor(84*frac)), 3, barCol);
            }

            // Numeric HP ("%u/%u")
            char hpStr[24];
            std::snprintf(hpStr, sizeof(hpStr), "%u/%u", unsigned(actor.battleState.hp), unsigned(actor.battleState.maxHp));
            renderer.drawTextFitted(hpStr, hpX + 16, y + 3, 0.24f, 70, C2D_Color32(215, 210, 225, 255));
        }

        // Bottom footer window
        renderer.drawWindow(8, 204, 304, 30);
        renderer.drawTextFitted("Elige a un Pokémon.", 18, 210, 0.30f, 160, C2D_Color32(245, 245, 245, 255));
        renderer.drawTextFitted("A: Cambiar   B: Salir", 188, 210, 0.30f, 112, C2D_Color32(140, 210, 255, 255));
    }

private:
    PokemonIconPresenter m_icons{true};
    TitleMenuPresenter m_cursor;
};

}

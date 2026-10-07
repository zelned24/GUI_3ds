#pragma once
#include "gfx/renderer2d.hpp"
#include "runtime/DualScreenLayout.hpp"
#include "runtime/PokemonIconPresenter.hpp"
#include "runtime/TitleMenuPresenter.hpp"
#include "runtime/TypePresentation.hpp"
#include "game/FirstRunRuntime.hpp"
#include <cstdio>
#include <cstring>

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
                m_cursor.drawCursor(renderer, bounds.x + 3, y + 3, 0.32f);
            } else {
                renderer.drawWindow(bounds.x, y, bounds.width, bounds.height);
            }

            uint16_t formIndex = 0;
            if (actor.formId) {
                for (const auto& form : PokerogueContent::kForms) {
                    if (std::strcmp(form.id, actor.formId) == 0) { formIndex = form.upstreamFormIndex; break; }
                }
            }
            m_icons.draw(renderer, actor.dex, formIndex, bounds.x + 14, y + 2, 1.0f, 0.5f);

            // Name + Gender
            const char* name = actor.localizedName ? actor.localizedName : "Pokémon";
            renderer.drawTextFitted(name, bounds.x + 40, y + 3, 0.32f, 70,
                isSel ? C2D_Color32(255, 255, 255, 255) : C2D_Color32(220, 215, 230, 255));

            // Level ("N. %u")
            char lvl[16];
            std::snprintf(lvl, sizeof(lvl), "N.%u", unsigned(actor.level));
            renderer.drawText(lvl, bounds.x + 115, y + 4, 0.28f, C2D_Color32(255, 225, 90, 255));

            // Active or status indicator
            if (actor.battleState.hp == 0) {
                renderer.drawText("DEB", bounds.x + 155, y + 4, 0.26f, C2D_Color32(240, 50, 50, 255));
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
                renderer.drawText(st, bounds.x + 155, y + 4, 0.26f, stCol);
            } else if (i == context.activePlayerPartyIndex) {
                renderer.drawText("ACT", bounds.x + 155, y + 4, 0.26f, C2D_Color32(80, 220, 140, 255));
            }

            // "PS" label and HP track groove
            const float hpX = bounds.x + 185;
            renderer.drawText("PS", hpX - 12, y + 4, 0.20f, C2D_Color32(245, 195, 60, 255));

            const float frac = actor.battleState.maxHp ? float(actor.battleState.hp) / actor.battleState.maxHp : 0.0f;
            const uint32_t barCol = frac > 0.5f ? C2D_Color32(60, 220, 100, 255)
                : frac > 0.2f ? C2D_Color32(245, 180, 20, 255) : C2D_Color32(235, 60, 60, 255);
            renderer.drawRect(hpX, y + 15, 65, 3, C2D_Color32(30, 28, 38, 255));
            if (frac > 0.0f) {
                renderer.drawRect(hpX, y + 15, 65 * frac, 3, barCol);
            }

            // Numeric HP ("%u/%u")
            char hpStr[24];
            std::snprintf(hpStr, sizeof(hpStr), "%u/%u", unsigned(actor.battleState.hp), unsigned(actor.battleState.maxHp));
            renderer.drawText(hpStr, hpX + 68, y + 11, 0.24f, C2D_Color32(215, 210, 225, 255));
        }

        // Bottom footer window
        renderer.drawWindow(8, 204, 304, 30);
        renderer.drawText("Elige a un Pokémon.", 18, 210, 0.30f, C2D_Color32(245, 245, 245, 255));
        renderer.drawText("A: Cambiar   B: Salir", 188, 210, 0.30f, C2D_Color32(140, 210, 255, 255));
    }

private:
    PokemonIconPresenter m_icons;
    TitleMenuPresenter m_cursor;
};

}

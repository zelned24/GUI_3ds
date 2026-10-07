#pragma once
#include "gfx/renderer2d.hpp"
#include "runtime/DualScreenLayout.hpp"
#include "runtime/TypePresentation.hpp"
#include "game/FirstRunRuntime.hpp"
#include "game/PokemonRecoilEffect.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include "content/EntityUiNames.hpp"
#include "runtime/TitleMenuPresenter.hpp"
#include <cstdio>

namespace Pokerogue3DS {

class MoveMenuPresenter {
public:
    static TitleMenuPresenter& cursor() {
        static TitleMenuPresenter s_cursor;
        return s_cursor;
    }
    static void clear() {
        cursor().clear();
    }
    static bool visible(const FirstRunRuntime& game) {
        return game.presentationStage() == NativeSaveStage::BattleActive &&
            !game.battleFinished() && !game.moveLearningPending() && !game.evolutionPending();
    }

    static int hitTest(unsigned x, unsigned y) {
        return moveButtonAt(x, y);
    }

    static void draw(Renderer2D& renderer, const FirstRunRuntime& game) {
        const auto& pokemon = game.presentation().player;
        renderer.clear(C2D_Color32(36, 28, 44, 255));

        // Left Window: 2x2 Move List
        renderer.drawWindow(6, 8, 186, 224);

        // Header inside left window
        renderer.drawText("MOVIMIENTOS", 18, 18, 0.30f, C2D_Color32(190, 180, 205, 255));

        const bool struggleActive = pokemonMovePpExhausted(pokemon.battleState);
        const uint8_t selectedMove = game.selectedBattleMove();

        if (struggleActive) {
            const auto* struggle = PokerogueContent::findMoveById(PokerogueContent::kStruggleMoveId);
            if (struggle) {
                cursor().drawCursor(renderer, 18, 50, 0.40f);
                renderer.drawText(moveUiName(struggle->id), 32, 50, 0.40f, C2D_Color32(255, 255, 255, 255));
                renderer.drawText("PP --", 32, 74, 0.28f, C2D_Color32(220, 80, 80, 255));
            }
        } else {
            static const struct { float x; float y; } kMovePos[] = {
                {28.0f, 44.0f},   // Move 0: Top-left
                {112.0f, 44.0f},  // Move 1: Top-right
                {28.0f, 114.0f},  // Move 2: Bottom-left
                {112.0f, 114.0f}  // Move 3: Bottom-right
            };

            for (unsigned i = 0; i < pokemon.moveCount && i < 4; ++i) {
                const auto* move = PokerogueContent::findMoveById(pokemon.moveIds[i]);
                if (!move) continue;

                const float mx = kMovePos[i].x;
                const float my = kMovePos[i].y;
                const bool isSel = (i == selectedMove);
                const uint8_t curPp = pokemon.battleState.moves[i].pp;
                const uint8_t maxPp = pokemon.battleState.moves[i].maxPp;
                const bool outOfPp = (curPp == 0);

                const char* moveName = moveUiName(move->id);
                const uint32_t textColor = outOfPp ? C2D_Color32(130, 130, 140, 255)
                    : isSel ? C2D_Color32(255, 255, 255, 255) : C2D_Color32(215, 210, 225, 255);
                const uint32_t shadowColor = C2D_Color32(0x50, 0x40, 0x60, 255);

                renderer.drawTextFitted(moveName, mx + 1.0f, my + 1.0f, 0.36f, 72.0f, shadowColor);
                const float nameSize=renderer.drawTextFitted(moveName,mx,my,0.36f,72.0f,textColor);
                if(isSel) cursor().drawCursor(renderer,mx-12,my,nameSize);

                char ppText[16];
                std::snprintf(ppText, sizeof(ppText), "PP %u/%u", unsigned(curPp), unsigned(maxPp));
                const uint32_t ppColor = outOfPp ? C2D_Color32(230, 60, 60, 255) : C2D_Color32(165, 160, 180, 255);
                renderer.drawText(ppText, mx, my + 24.0f, 0.26f, ppColor);
            }
        }

        // Left window footer instructions
        renderer.drawText("A: Atacar   B: Volver", 20, 198, 0.30f, C2D_Color32(240, 240, 245, 255));

        // Right Window: Move Details Panel
        renderer.drawWindow(196, 8, 118, 224);

        const auto* curMove = struggleActive
            ? PokerogueContent::findMoveById(PokerogueContent::kStruggleMoveId)
            : (selectedMove < pokemon.moveCount ? PokerogueContent::findMoveById(pokemon.moveIds[selectedMove]) : nullptr);

        if (curMove) {
            // Type badge pill
            drawTypeBadge(renderer, curMove->type, 208, 24, 94, 18, 0.26f);

            // Category
            renderer.drawText("CATEGORÍA", 208, 54, 0.22f, C2D_Color32(175, 170, 185, 255));
            const char* catName = "Estado";
            uint32_t catCol = C2D_Color32(160, 160, 160, 255);
            if (curMove->category == PokerogueContent::MovePhysical) {
                catName = "Físico";
                catCol = C2D_Color32(235, 75, 45, 255);
            } else if (curMove->category == PokerogueContent::MoveSpecial) {
                catName = "Especial";
                catCol = C2D_Color32(65, 120, 240, 255);
            }
            renderer.drawText(catName, 208, 68, 0.34f, catCol);

            // Power
            renderer.drawText("POTENCIA", 208, 98, 0.22f, C2D_Color32(175, 170, 185, 255));
            char powStr[16];
            if (curMove->power > 0) std::snprintf(powStr, sizeof(powStr), "%d", curMove->power);
            else std::snprintf(powStr, sizeof(powStr), "--");
            renderer.drawText(powStr, 208, 112, 0.36f, C2D_Color32(255, 255, 255, 255));

            // Accuracy
            renderer.drawText("PRECISIÓN", 208, 142, 0.22f, C2D_Color32(175, 170, 185, 255));
            char accStr[16];
            if (curMove->accuracy > 0) std::snprintf(accStr, sizeof(accStr), "%d%%", curMove->accuracy);
            else std::snprintf(accStr, sizeof(accStr), "--");
            renderer.drawText(accStr, 208, 156, 0.36f, C2D_Color32(255, 255, 255, 255));

            // PP Details
            if (!struggleActive && selectedMove < pokemon.moveCount) {
                const uint8_t curPp = pokemon.battleState.moves[selectedMove].pp;
                const uint8_t maxPp = pokemon.battleState.moves[selectedMove].maxPp;
                char ppFull[24];
                std::snprintf(ppFull, sizeof(ppFull), "PP  %u / %u", unsigned(curPp), unsigned(maxPp));
                renderer.drawText(ppFull, 208, 186, 0.28f, C2D_Color32(230, 225, 240, 255));

                const float ppRatio = maxPp ? float(curPp) / maxPp : 0.0f;
                renderer.drawRect(208, 202, 94, 3, C2D_Color32(35, 30, 42, 255));
                if (ppRatio > 0.0f) {
                    const uint32_t barCol = ppRatio > 0.5f ? C2D_Color32(60, 220, 100, 255)
                        : ppRatio > 0.2f ? C2D_Color32(245, 180, 20, 255) : C2D_Color32(235, 60, 60, 255);
                    renderer.drawRect(208, 202, 94 * ppRatio, 3, barCol);
                }
            }
        }

        // Double battle target selector overlay if required
        if (game.doubleBattle()) {
            for (unsigned i = 0; i < 2; ++i) {
                const auto& bounds = kTargetButtonRects[i];
                const auto& target = i ? game.presentation().secondEnemy : game.presentation().enemy;
                const bool isSel = (i == game.selectedTarget());
                renderer.drawRect(bounds.x, bounds.y, bounds.width, bounds.height,
                    isSel ? C2D_Color32(255, 235, 70, 255) : C2D_Color32(70, 60, 80, 255));
                renderer.drawWindow(bounds.x + 2, bounds.y + 2, bounds.width - 4, bounds.height - 4);
                renderer.drawText(target.localizedName ? target.localizedName : "", bounds.x + 8, bounds.y + 5, 0.32f, C2D_Color32(255, 255, 255, 255));
            }
        }
    }
};

}

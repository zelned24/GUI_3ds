#pragma once
#include "gfx/renderer2d.hpp"
#include "runtime/DualScreenLayout.hpp"
#include "runtime/TypePresentation.hpp"
#include "game/FirstRunRuntime.hpp"
#include "game/PokemonRecoilEffect.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include "content/EntityUiNames.hpp"
#include "content/RuntimeUiText.hpp"
#include "runtime/TitleMenuPresenter.hpp"
#include <cstdio>

namespace Pokerogue3DS {

class MoveMenuPresenter {
public:
    static TitleMenuPresenter& cursor() {
        static TitleMenuPresenter s_cursor;
        return s_cursor;
    }
    static void clear(Renderer2D* renderer=nullptr) {
        cursor().clear(renderer);
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
        renderer.drawWindow(6, 8, 186, game.doubleBattle() ? 196 : 224);

        // Header inside left window
        renderer.drawText("MOVIMIENTOS", 18, 18, 0.30f, C2D_Color32(190, 180, 205, 255));

        const bool struggleActive = pokemonMovePpExhausted(pokemon.battleState);
        const uint8_t selectedMove = game.selectedBattleMove();

        if (struggleActive) {
            const auto* struggle = PokerogueContent::findMoveById(PokerogueContent::kStruggleMoveId);
            if (struggle) {
                cursor().drawCursor(renderer, 18, 50, 0.40f);
                renderer.drawTextFitted(moveUiName(struggle->id), 32, 50, 0.40f, 60, C2D_Color32(255, 255, 255, 255));
                renderer.drawText("PP --", 32, 74, 0.28f, C2D_Color32(220, 80, 80, 255));
            }
        } else {
            for (unsigned i = 0; i < pokemon.moveCount && i < 4; ++i) {
                const auto* move = PokerogueContent::findMoveById(pokemon.moveIds[i]);
                if (!move) continue;

                const auto& bounds=kMoveButtonRects[i];
                const float mx=bounds.x+20;
                const float my=bounds.y+8;
                const float nameWidth=bounds.width-28;
                const bool isSel = (i == selectedMove);
                const uint8_t curPp = pokemon.battleState.moves[i].pp;
                const uint8_t maxPp = pokemon.battleState.moves[i].maxPp;
                const bool outOfPp = (curPp == 0);

                const char* moveName = moveUiName(move->id);
                const uint32_t textColor = outOfPp ? C2D_Color32(130, 130, 140, 255)
                    : isSel ? C2D_Color32(255, 255, 255, 255) : C2D_Color32(215, 210, 225, 255);

                // Two native raster lines preserve long localized names without squeezing glyphs.
                float nameSize=0.3125f;
                const unsigned nameLines=textLinesWithinHeight(30,renderer.textInkHeight(nameSize),renderer.textLineHeight(nameSize),2);
                // One raster pass, like dialogue: an offset shadow crowds tiny glyphs.
                if(!nameLines || !renderer.drawTextBox(moveName,mx,my,nameSize,nameWidth,nameLines,textColor))
                    nameSize=renderer.drawTextFitted(moveName,mx,my,nameSize,nameWidth,textColor);
                if(isSel) cursor().drawCursor(renderer,mx-12,my,nameSize);

                char ppText[16];
                std::snprintf(ppText, sizeof(ppText), "PP %u/%u", unsigned(curPp), unsigned(maxPp));
                const uint32_t ppColor = outOfPp ? C2D_Color32(230, 60, 60, 255) : C2D_Color32(165, 160, 180, 255);
                renderer.drawTextFitted(ppText, mx, my + 36.0f, 0.26f, nameWidth, ppColor);
            }
        }

        // Left window footer instructions
        const auto confirm=moveConfirmRectangle(game.doubleBattle());
        const auto back=moveBackRectangle(game.doubleBattle());
        renderer.drawWindow(confirm.x,confirm.y,confirm.width,confirm.height);
        renderer.drawWindow(back.x,back.y,back.width,back.height);
        renderer.drawTextFitted("A: Atacar",confirm.x+8,confirm.y+8,0.375f,confirm.width-16,0xffffffff);
        renderer.drawTextFitted("B: Volver",back.x+8,back.y+8,0.375f,back.width-16,0xffffffff);

        // Right Window: Move Details Panel
        renderer.drawWindow(196, 8, 118, game.doubleBattle() ? 196 : 224);

        const auto* curMove = struggleActive
            ? PokerogueContent::findMoveById(PokerogueContent::kStruggleMoveId)
            : (selectedMove < pokemon.moveCount ? PokerogueContent::findMoveById(pokemon.moveIds[selectedMove]) : nullptr);

        if (curMove) {
            // Type badge pill
            drawTypeBadge(renderer, curMove->type, 208, 24, 94, 18, 0.26f);

            // Category
            renderer.drawTextFitted("CATEGORÍA", 208, 54, 0.3125f, 94, C2D_Color32(175, 170, 185, 255));
            const char* categoryKey = curMove->category == PokerogueContent::MovePhysical ? "physical"
                : (curMove->category == PokerogueContent::MoveSpecial ? "special" : "status");
            if (!renderer.drawHudGraphic("categories", categoryKey, 208, 70))
                renderer.drawText("?", 208, 70, 0.3125f, 0xffffffff);

            // Power
            renderer.drawTextFitted(runtimeUiText("fight-ui-handler:power"), 208, 98, 0.3125f, 94, C2D_Color32(175, 170, 185, 255));
            char powStr[16];
            if (curMove->power > 0) std::snprintf(powStr, sizeof(powStr), "%d", curMove->power);
            else std::snprintf(powStr, sizeof(powStr), "--");
            renderer.drawText(powStr, 208, 112, 0.36f, C2D_Color32(255, 255, 255, 255));

            // Accuracy
            renderer.drawTextFitted(runtimeUiText("fight-ui-handler:accuracy"), 208, 142, 0.3125f, 94, C2D_Color32(175, 170, 185, 255));
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
                renderer.drawTextFitted(ppFull, 208, 182, 0.3125f, 94, C2D_Color32(230, 225, 240, 255));

                const float ppRatio = maxPp ? float(curPp) / maxPp : 0.0f;
                renderer.drawRect(208, game.doubleBattle() ? 198 : 202, 94, 3, C2D_Color32(35, 30, 42, 255));
                if (ppRatio > 0.0f) {
                    const uint32_t barCol = ppRatio > 0.5f ? C2D_Color32(60, 220, 100, 255)
                        : ppRatio > 0.2f ? C2D_Color32(245, 180, 20, 255) : C2D_Color32(235, 60, 60, 255);
                    renderer.drawRect(208, game.doubleBattle() ? 198 : 202, movePpBarPixels(curPp,maxPp,94), 3, barCol);
                }
            }
        }

        // Double battle target selector overlay if required
        if (game.doubleBattle()) {
            // Target controls own the bottom row; keep the action footer unobstructed.
            for (unsigned i = 0; i < 2; ++i) {
                const auto& bounds = kTargetButtonRects[i];
                const auto& target = i ? game.presentation().secondEnemy : game.presentation().enemy;
                const bool isSel = (i == game.selectedTarget());
                renderer.drawRect(bounds.x, bounds.y, bounds.width, bounds.height,
                    isSel ? C2D_Color32(255, 235, 70, 255) : C2D_Color32(70, 60, 80, 255));
                renderer.drawWindow(bounds.x + 2, bounds.y + 2, bounds.width - 4, bounds.height - 4);
                renderer.drawTextFitted(target.localizedName ? target.localizedName : "", bounds.x + 8, bounds.y + 5, 0.32f, bounds.width-16, C2D_Color32(255, 255, 255, 255));
            }
        }
    }
};

}

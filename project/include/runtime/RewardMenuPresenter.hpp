#pragma once
#include "gfx/renderer2d.hpp"
#include "game/FirstRunRuntime.hpp"
#include "runtime/DualScreenLayout.hpp"
#include "runtime/ItemIconPresenter.hpp"
#include "runtime/PartyMenuPresenter.hpp"
#include "runtime/TitleMenuPresenter.hpp"
#include "content/ItemIconReferences.hpp"
#include "content/EntityUiNames.hpp"
#include <cstring>

namespace Pokerogue3DS {

class RewardMenuPresenter {
public:
    static TouchRect rectangle(unsigned index) { return index<3 ? kRewardChoiceRects[index] : TouchRect{}; }
    static int hitTest(unsigned x, unsigned y, unsigned count) {
        return rewardChoiceAt(x,y,count);
    }

    void clear(Renderer2D* renderer=nullptr) {
        m_icons.clear(renderer);
        m_partyPresenter.clear(renderer);
        m_cursor.clear(renderer);
        resetSelection();
    }

    void releasePartyIcons(Renderer2D& renderer) {m_partyPresenter.clear(&renderer);}

    bool partySelectionMode() const { return m_partySelectMode; }
    void resetSelection() { m_partySelectMode=false; m_moveSelectMode=false; m_moveSelection.reset(); }
    void setPartySelectionMode(bool mode) { m_partySelectMode=mode; if(mode) m_moveSelectMode=false; }
    bool moveSelectionMode() const { return m_moveSelectMode; }
    void setMoveSelectionMode(bool mode) { m_moveSelectMode=mode; if(mode) {m_partySelectMode=false;m_moveSelection.reset();} }
    RewardMoveSelection& moveSelection() {return m_moveSelection;}
    PartyMenuPresenter& partyPresenter() { return m_partyPresenter; }
    const PartyMenuPresenter& partyPresenter() const { return m_partyPresenter; }

    // Renders the 3 reward choices on Top Screen (400x240) in PokéRogue card style
    void drawTop(Renderer2D& renderer, const FirstRunRuntime& game) {
        renderer.clear(0xff241b2c); // Dark purple navy

        // Title header
        renderer.drawWindow(12.0f, 6.0f, 376.0f, 32.0f);
        renderer.drawText("Elige una Recompensa", 24.0f, 12.0f, 0.46f, 0xffffffff);

        const unsigned count = game.rewardChoiceCount();
        const float cardW = 118.0f;
        const float cardH = 175.0f;
        const float startX = 14.0f;
        const float spacing = 127.0f;
        const float cardY = 46.0f;

        for (unsigned i = 0; i < count && i < 3; ++i) {
            const auto* reward = game.rewardChoice(i);
            if (!reward || !reward->poolEntry || !reward->poolEntry->itemId) continue;
            const char* name = reward->poolEntry->itemId;
            for (const auto& item : PokerogueContent::kItems) {
                if (std::strcmp(item.id, name) == 0) { name = item.name; break; }
            }

            const float cx = startX + i * spacing;
            const bool isSelected = (i == game.selectedRewardChoice());

            // Presentation accents by the imported rarity tier:
            // Common: Green (0xff50d250)
            // Great/Super: Blue-Orange (0xff3ca0f0)
            // Ultra: Pink/Magenta (0xfff06496)
            // Master: Purple (0xffb43ce6)
            const char* tier = reward->poolEntry->tier ? reward->poolEntry->tier : "COMMON";
            uint32_t rarityColor = 0xff50d250; // Common green
            if (std::strcmp(tier, "GREAT") == 0 || std::strcmp(tier, "SUPER") == 0) {
                rarityColor = 0xff3ca0f0; // Great blue-orange
            } else if (std::strcmp(tier, "ULTRA") == 0) {
                rarityColor = 0xfff06496; // Ultra pink
            } else if (std::strcmp(tier, "MASTER") == 0) {
                rarityColor = 0xffb43ce6; // Master purple
            }

            // Outer highlight glow if selected
            if (isSelected) {
                renderer.drawRect(cx - 3.0f, cardY - 3.0f, cardW + 6.0f, cardH + 6.0f, 0xff70d8f0);
            }

            // Card background frame with canonical rarity border tint
            renderer.drawRect(cx, cardY, cardW, cardH, rarityColor);
            renderer.drawWindow(cx + 2.0f, cardY + 2.0f, cardW - 4.0f, cardH - 4.0f);

            // Item Icon centered in upper card
            // Original item canvases are 32x32; avoid the previous 1.125x enlargement.
            m_icons.drawItem(renderer,reward->poolEntry->itemId,cx+(cardW-32)*0.5f,cardY+16,32);

            // Item Name
            if(!renderer.drawTextBox(name,cx+6,cardY+68,0.3125f,cardW-12,3,0xffffffff))
                renderer.drawTextFitted(name,cx+6,cardY+68,0.3125f,cardW-12,0xffffffff);

            // Visual rarity accent band (strictly NO textual "COMMON" or "Común")
            renderer.drawRect(cx + 12.0f, cardY + 104.0f, cardW - 24.0f, 3.0f, rarityColor);

            // Selected indicator
            if (isSelected) {
                m_cursor.drawCursor(renderer, cx + 10.0f, cardY + 142.0f, 0.30f);
                renderer.drawText("ELEGIR", cx + 24.0f, cardY + 142.0f, 0.30f, 0xff70d8f0);
            }
        }
    }

    // Renders touch confirmation / interaction on Bottom Screen (320x240)
    void drawBottom(Renderer2D& renderer, const FirstRunRuntime& game, bool assigningParty = false) {
        renderer.clear(0xff241c2c);

        if(m_moveSelectMode) {
            renderer.drawWindow(16,6,288,30);
            renderer.drawText("Elige el movimiento",28,11,0.38f,0xffffffff);
            const auto& field=game.presentation();
            if(m_partyPresenter.selected>=field.playerPartyCount) return;
            const auto& actor=m_partyPresenter.selected==field.activePlayerPartyIndex ? field.player : field.playerParty[m_partyPresenter.selected];
            for(unsigned i=0;i<actor.battleState.moveCount && i<4;++i) {
                const auto& bounds=kRewardMoveRects[i];
                renderer.drawWindow(bounds.x,bounds.y,bounds.width,bounds.height);
                const auto& move=actor.battleState.moves[i];
                const auto* definition=PokerogueContent::findMoveById(move.moveId);
                const char* name=definition ? moveUiName(definition->id) : "Movimiento desconocido";
                float nameSize=0.3125f;
                const unsigned nameLines=textLinesWithinHeight(bounds.height-10,renderer.textInkHeight(nameSize),renderer.textLineHeight(nameSize),2);
                if(!nameLines || !renderer.drawTextBox(name,44,bounds.y+5,nameSize,180,nameLines,0xffffffff))
                    nameSize=renderer.drawTextFitted(name,44,bounds.y+5,nameSize,180,0xffffffff);
                char pp[32];std::snprintf(pp,sizeof(pp),"PP %u/%u",unsigned(move.pp),unsigned(move.maxPp));
                renderer.drawTextFitted(pp,234,bounds.y+6,0.25f,58,0xff80ffff);
                if(i==m_moveSelection.selected) m_cursor.drawCursor(renderer,27,bounds.y+5,nameSize);
            }
            renderer.drawTextFitted("A: aplicar   B: volver al equipo",16,204,0.30f,288,0xff80ffff);
            renderer.drawTextFitted(game.battleFeedback().c_str(),16,225,0.24f,288,0xffffffff);
            return;
        }
        if (assigningParty || m_partySelectMode) {
            // Party selection mode for applying held items, berries, or potions
            m_partyPresenter.draw(renderer, game);
            renderer.drawWindow(kPartyHeaderRect.x,kPartyHeaderRect.y,kPartyHeaderRect.width,kPartyHeaderRect.height);
            const auto& feedback=game.battleFeedback();
            renderer.drawTextFitted(feedback.empty() ? "Elige el Pokémon destinatario" : feedback.c_str(),18,8,0.3125f,284,0xff70d8f0);
            renderer.drawWindow(kPartyFooterRect.x,kPartyFooterRect.y,kPartyFooterRect.width,kPartyFooterRect.height);
            renderer.drawTextFitted("A: elegir   B: volver a recompensas",18,224,0.25f,284,0xff80ffff);
            return;
        }

        renderer.drawWindow(16,12,288,32);
        renderer.drawTextFitted("Recompensas de combate",28,18,0.40f,264,0xffffffff);
        for(unsigned i=0;i<game.rewardChoiceCount() && i<3;++i) {
            const auto& bounds=kRewardChoiceRects[i];
            renderer.drawWindow(bounds.x,bounds.y,bounds.width,bounds.height);
            char label[24];std::snprintf(label,sizeof(label),"%u",i+1);
            renderer.drawText(label,bounds.x+38,bounds.y+9,0.40f,0xffffffff);
            if(i==game.selectedRewardChoice()) m_cursor.drawCursor(renderer,bounds.x+20,bounds.y+9,0.40f);
        }
        renderer.drawTextFitted("D-Pad o táctil: elegir recompensa",20,108,0.34f,280,0xffd0c0d8);
        renderer.drawTextFitted("A: seleccionar   B: omitir",20,135,0.34f,280,0xff80ffff);

        // Clean action buttons on Bottom Screen
        // Left: A: Elegir (TouchRect{16, 170, 136, 54})
        renderer.drawWindow(16.0f, 170.0f, 136.0f, 54.0f);
        renderer.drawText("A: Elegir", 44.0f, 188.0f, 0.42f, 0xff70d8f0);

        // Right: B: Omitir (TouchRect{168, 170, 136, 54})
        renderer.drawWindow(168.0f, 170.0f, 136.0f, 54.0f);
        renderer.drawText("B: Omitir", 198.0f, 188.0f, 0.42f, 0xfff08080);
        renderer.drawTextFitted(game.battleFeedback().c_str(),16,226,0.24f,288,0xffffffff);
    }

    // Backwards compatibility draw helpers
    void draw(Renderer2D& renderer, const FirstRunRuntime& game) {
        drawBottom(renderer, game, false);
    }
    void draw(Renderer2D& renderer, const FirstRunRuntime& game, bool assigningParty) {
        drawBottom(renderer, game, assigningParty);
    }

private:
    ItemIconPresenter m_icons;
    PartyMenuPresenter m_partyPresenter;
    TitleMenuPresenter m_cursor;
    bool m_partySelectMode = false;
    bool m_moveSelectMode = false;
    RewardMoveSelection m_moveSelection{};
};

} // namespace Pokerogue3DS

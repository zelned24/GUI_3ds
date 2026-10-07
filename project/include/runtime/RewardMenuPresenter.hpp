#pragma once
#include "gfx/renderer2d.hpp"
#include "game/FirstRunRuntime.hpp"
#include "runtime/DualScreenLayout.hpp"
#include "runtime/ItemIconPresenter.hpp"
#include "runtime/PartyMenuPresenter.hpp"
#include "runtime/TitleMenuPresenter.hpp"
#include "content/ItemIconReferences.hpp"
#include <cstring>

namespace Pokerogue3DS {

class RewardMenuPresenter {
public:
    static TouchRect rectangle(unsigned index) { return {16, 32 + index * 46, 288, 40}; }
    static int hitTest(unsigned x, unsigned y, unsigned count) {
        if (count > 3) return -1;
        for (unsigned i = 0; i < count; ++i) if (rectangle(i).contains(x, y)) return int(i);
        return -1;
    }

    void clear() {
        m_icons.clear();
        m_partyPresenter.clear();
        m_cursor.clear();
        m_partySelectMode = false;
    }

    bool partySelectionMode() const { return m_partySelectMode; }
    void setPartySelectionMode(bool mode) { m_partySelectMode = mode; }
    void togglePartySelectionMode() { m_partySelectMode = !m_partySelectMode; }
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

            // Canonical rarity tints:
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
            m_icons.drawItem(renderer, reward->poolEntry->itemId, cx + (cardW - 36.0f) * 0.5f, cardY + 16.0f, 36.0f);

            // Item Name
            renderer.drawTextFitted(name, cx + 6.0f, cardY + 68.0f, 0.35f, cardW - 12.0f, 0xffffffff);

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

        if (assigningParty || m_partySelectMode) {
            // Party selection mode for applying held items, berries, or potions
            m_partyPresenter.draw(renderer, game);
            renderer.drawWindow(8.0f, 6.0f, 304.0f, 26.0f);
            renderer.drawText("Elige el Pokémon destinatario", 18.0f, 11.0f, 0.32f, 0xff70d8f0);
            return;
        }

        // Clean instruction window (no duplicate reward cards on Bottom Screen)
        renderer.drawWindow(16.0f, 20.0f, 288.0f, 130.0f);
        renderer.drawText("Recompensas de Combate", 32.0f, 34.0f, 0.42f, 0xffffffff);
        renderer.drawText("Navega entre recompensas con el D-Pad.", 32.0f, 66.0f, 0.32f, 0xffd0c0d8);
        renderer.drawText("Pulsa A para seleccionar la recompensa.", 32.0f, 92.0f, 0.32f, 0xff70d8f0);
        renderer.drawText("Pulsa B para omitir la recompensa.", 32.0f, 116.0f, 0.32f, 0xfff08080);

        // Clean action buttons on Bottom Screen
        // Left: A: Elegir (TouchRect{16, 170, 136, 54})
        renderer.drawWindow(16.0f, 170.0f, 136.0f, 54.0f);
        renderer.drawText("A: Elegir", 44.0f, 188.0f, 0.42f, 0xff70d8f0);

        // Right: B: Omitir (TouchRect{168, 170, 136, 54})
        renderer.drawWindow(168.0f, 170.0f, 136.0f, 54.0f);
        renderer.drawText("B: Omitir", 198.0f, 188.0f, 0.42f, 0xfff08080);
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
};

} // namespace Pokerogue3DS

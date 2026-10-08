#pragma once
#include "runtime/MoveMenuPresenter.hpp"
#include "runtime/TitleMenuPresenter.hpp"
#include "runtime/ItemIconPresenter.hpp"
#include "runtime/DualScreenLayout.hpp"
#include "runtime/DialoguePresenter.hpp"
#include "content/BallMenuContent.hpp"
#include "content/RuntimeUiText.hpp"
#include <3ds.h>
#include <cstdio>
#include <string>

namespace Pokerogue3DS {

enum class BattleMenuPage { Root, Moves, Balls };
enum class BattleMenuCommand {
    None,
    MovePrevious,
    MoveNext,
    MoveRowToggle,
    MoveColToggle,
    TargetPrevious,
    TargetNext,
    ExecuteMove,
    Party,
    ThrowBall,
    Flee
};

class BattleCommandMenuPresenter {
public:
    void clear() { m_icons.clear(); m_cursor.clear(); }
    void reset() { m_page = BattleMenuPage::Root; m_selected = 0; m_dialogue.reset();m_advanceDialogue=false; }
    bool movesOpen() const { return m_page == BattleMenuPage::Moves; }
    PokeballType ballType() const { return static_cast<PokeballType>(kBallMenuDefinitions[m_selected].id); }

    BattleMenuCommand input(uint32_t keys, bool doubleBattle, unsigned x = 0, unsigned y = 0) {
        if ((keys & KEY_B) || ((keys & KEY_TOUCH) && ((m_page==BattleMenuPage::Moves && moveBackRectangle(doubleBattle).contains(x,y)) || (m_page==BattleMenuPage::Balls && TouchRect{12,202,296,32}.contains(x,y))))) {
            reset();
            return BattleMenuCommand::None;
        }

        if (m_page == BattleMenuPage::Moves) {
            // D-Pad navigates every move; shoulders select the double-battle target.
            if(doubleBattle && (keys & KEY_L)) return BattleMenuCommand::TargetPrevious;
            if(doubleBattle && (keys & KEY_R)) return BattleMenuCommand::TargetNext;
            if (keys & (KEY_DUP | KEY_CPAD_UP | KEY_DDOWN | KEY_CPAD_DOWN)) {
                return BattleMenuCommand::MoveRowToggle;
            }
            if (keys & (KEY_DLEFT | KEY_CPAD_LEFT | KEY_DRIGHT | KEY_CPAD_RIGHT)) {
                return BattleMenuCommand::MoveColToggle;
            }
            if (keys & KEY_A) return BattleMenuCommand::ExecuteMove;
            return BattleMenuCommand::None;
        }

        if (m_page == BattleMenuPage::Root) {
            if(m_dialogue.hasNavigation() && (keys & KEY_SELECT)) {
                m_advanceDialogue=true;return BattleMenuCommand::None;
            }
            // 2D D-pad navigation: UP/DOWN toggles row, LEFT/RIGHT toggles column
            if (keys & (KEY_DUP | KEY_CPAD_UP | KEY_DDOWN | KEY_CPAD_DOWN)) {
                m_selected ^= 2;
            }
            if (keys & (KEY_DLEFT | KEY_CPAD_LEFT | KEY_DRIGHT | KEY_CPAD_RIGHT)) {
                m_selected ^= 1;
            }

            bool touchActivated = false;
            if (keys & KEY_TOUCH) {
                const int cmd = commandButtonAt(x, y);
                if (cmd >= 0) {
                    if (m_selected == unsigned(cmd)) {
                        // Safe touch: second tap on already selected command activates it
                        touchActivated = true;
                    } else {
                        // First tap on unselected command selects it safely
                        m_selected = unsigned(cmd);
                    }
                }
            }

            if (!((keys & KEY_A) || touchActivated)) return BattleMenuCommand::None;

            if (m_selected == 0) m_page = BattleMenuPage::Moves;
            else if (m_selected == 1) { m_page = BattleMenuPage::Balls; m_selected = 0; }
            else if (m_selected == 2) return BattleMenuCommand::Party;
            else if (m_selected == 3) return BattleMenuCommand::Flee;
            return BattleMenuCommand::None;
        }

        // Balls page
        const unsigned count = sizeof(kBallMenuDefinitions) / sizeof(kBallMenuDefinitions[0]);
        if (keys & (KEY_DUP | KEY_CPAD_UP)) m_selected = (m_selected + count - 1) % count;
        if (keys & (KEY_DDOWN | KEY_CPAD_DOWN)) m_selected = (m_selected + 1) % count;

        bool touchActivated = false;
        if (keys & KEY_TOUCH) {
            for (unsigned i = 0; i < count; ++i) {
                if (ballMenuRectangle(i).contains(x, y)) {
                    m_selected = i;
                    touchActivated = true;
                    break;
                }
            }
        }

        if (!((keys & KEY_A) || touchActivated)) return BattleMenuCommand::None;
        return BattleMenuCommand::ThrowBall;
    }

    void drawTop(Renderer2D& renderer,const FirstRunRuntime& game) {
        const auto& feedback=game.battleFeedback();
        if(!feedback.empty() && feedback.rfind("Ola ",0)!=0) {
            m_dialogue.sync(renderer,feedback);
        } else {
            const char* name=game.presentation().player.localizedName;
            m_dialogue.sync(renderer,std::string("¿Qué debería hacer ")+(name ? name : "Pokémon")+"?");
        }
        if(m_advanceDialogue) m_dialogue.advance(renderer);
        m_advanceDialogue=false;
        m_dialogue.draw(renderer);
    }

    void draw(Renderer2D& renderer, const FirstRunRuntime& game) {
        if (m_page == BattleMenuPage::Moves) {
            MoveMenuPresenter::draw(renderer, game);
            return;
        }

        // Dark navy/violet clear color
        renderer.clear(C2D_Color32(36, 28, 44, 255));

        if (m_page == BattleMenuPage::Root) {
            // The upper screen owns dialogue; commands fill the touch screen.
            static const struct {
                const char* label;
                float bx; float by; float bw; float bh;
                float tx; float ty; float tsize;
            } kCmds[] = {
                {"Luchar",  10.0f,  10.0f, 146.0f, 94.0f, 40.0f,  50.0f, 0.375f},
                {"Balls",   164.0f,  10.0f, 146.0f, 94.0f, 194.0f,  50.0f, 0.375f},
                {"Pokémon", 10.0f, 112.0f, 146.0f, 94.0f, 40.0f, 152.0f, 0.375f},
                {"Huir",    164.0f,112.0f, 146.0f, 94.0f, 194.0f,152.0f, 0.375f}
            };

            for (unsigned i = 0; i < 4; ++i) {
                const bool isSel = (i == m_selected);
                const auto& cmd = kCmds[i];

                if (isSel) {
                    renderer.drawRect(cmd.bx - 1.0f, cmd.by - 1.0f, cmd.bw + 2.0f, cmd.bh + 2.0f,
                        C2D_Color32(255, 235, 70, 255));
                }
                renderer.drawWindow(cmd.bx, cmd.by, cmd.bw, cmd.bh);

                if (isSel) {
                    const float cursorX = std::max(cmd.bx + 2.0f, cmd.tx - 10.0f);
                    m_cursor.drawCursor(renderer, cursorX, cmd.ty, cmd.tsize);
                }

                const uint32_t textColor = isSel ? C2D_Color32(255, 255, 255, 255) : C2D_Color32(210, 205, 220, 255);
                const uint32_t shadowColor = C2D_Color32(0x50, 0x40, 0x60, 255);

                renderer.drawText(cmd.label, cmd.tx + 1.0f, cmd.ty + 1.0f, cmd.tsize, shadowColor);
                renderer.drawText(cmd.label, cmd.tx, cmd.ty, cmd.tsize, textColor);
            }
        } else {
            // Poké Balls Inventory Window
            renderer.drawWindow(12, 12, 296, 186);
            static const char* rates[] = {"1.0x", "1.5x", "2.0x", "3.0x", "100%"};
            for (unsigned i = 0; i < sizeof(kBallMenuDefinitions) / sizeof(kBallMenuDefinitions[0]); ++i) {
                const auto& ball = kBallMenuDefinitions[i];
                const float y=ballMenuRectangle(i).y;
                const unsigned count = game.pokeballCount(static_cast<PokeballType>(ball.id));
                m_icons.draw(renderer,ball.iconKey,36,y,32,count ? 1.0f : 0.35f);
                renderer.drawTextFitted(ball.label,76,y+8,0.3125f,110,count ? 0xffffffff : 0xff909090);
                if (i < sizeof(rates) / sizeof(rates[0])) {
                    renderer.drawText(rates[i],192,y+8,0.30f,count ? 0xffa0d0f0 : 0xff708090);
                }
                char quantity[16];
                std::snprintf(quantity, sizeof(quantity), "x%u", count);
                renderer.drawTextFitted(quantity,257,y+8,0.3125f,42,count ? 0xffffffff : 0xff808080);
                if (i == m_selected) {
                    m_cursor.drawCursor(renderer,21,y+8,0.3125f);
                }
            }

            // Footer bar
            renderer.drawWindow(12, 202, 296, 32);
            renderer.drawText("A: Lanzar    B: Volver", 24, 210, 0.30f, C2D_Color32(255, 255, 255, 255));
        }
    }

private:
    BattleMenuPage m_page = BattleMenuPage::Root;
    unsigned m_selected = 0;
    ItemIconPresenter m_icons;
    TitleMenuPresenter m_cursor;
    DialoguePresenter m_dialogue{2,270};
    bool m_advanceDialogue=false;
};

}

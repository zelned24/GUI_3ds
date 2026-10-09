#pragma once
#include "gfx/renderer2d.hpp"
#include "runtime/DualScreenLayout.hpp"
#include "runtime/PokemonIconPresenter.hpp"
#include "runtime/TitleMenuPresenter.hpp"
#include "runtime/TypePresentation.hpp"
#include "content/StarterVariantIcons.hpp"
#include "content/RuntimeUiText.hpp"
#include "game/FirstRunRuntime.hpp"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace Pokerogue3DS {

class PartyMenuPresenter {
public:
    ~PartyMenuPresenter() {clear();}
    void clear(Renderer2D* renderer=nullptr) {
        m_icons.clear(renderer);m_cursor.clear(renderer);
        if(m_variantSheet) {
            if(renderer) renderer->retireSpriteSheet(m_variantSheet);
            else C2D_SpriteSheetFree(m_variantSheet);
        }
        m_variantSheet=nullptr;m_variantAttempted=false;
    }
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
        renderer.drawTextFitted("EQUIPO POKÉMON",12,6,0.3125f,296,C2D_Color32(245,245,250,255));

        const auto& context = game.presentation();
        const unsigned count = context.playerPartyCount ? context.playerPartyCount : 1;
        const AppearanceIconIdentity* appearances[6]{};
        uint16_t formIndices[6]{};
        bool normalIconAllowed[6]{};
        for(unsigned i=0;i<count && i<6;++i) {
            const auto& actor=(i==context.activePlayerPartyIndex) ? context.player : context.playerParty[i];
            const bool appearanceKnown=actor.actorIdentityResolved && actor.actor.appearanceResolved && actor.actor.gender!=PokemonGender::Unspecified;
            auto icon=resolvePokemonIcon(actor.dex,actor.formId,appearanceKnown,actor.actor.gender==PokemonGender::Female,actor.actor.shiny,actor.actor.shinyVariant);
            resolvePokemonDiscoveryIcon(icon);
            appearances[i]=icon.appearance;formIndices[i]=icon.formIndex;normalIconAllowed[i]=icon.normalIconAllowed;
        }
        m_icons.prepareAppearances(renderer,appearances,std::min(count,6u));


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

            if(appearances[i]) {
                if(!m_icons.drawAppearance(renderer,appearances[i],bounds.x+14,y+1))
                    renderer.drawTextFitted("?",bounds.x+14,y+1,0.3125f,20,0xffffffff);
            } else if(normalIconAllowed[i]) {
                if(!m_icons.draw(renderer,actor.dex,formIndices[i],bounds.x+14,y+1,1.0f,1.0f))
                    renderer.drawTextFitted("?",bounds.x+14,y+1,0.3125f,20,0xffffffff);
            } else {
                renderer.drawTextFitted("?",bounds.x+14,y+1,0.3125f,20,0xffffffff);
            }

            if(actor.actorIdentityResolved && actor.actor.appearanceResolved && actor.actor.shiny && actor.actor.shinyVariant<=2)
                drawVariant(renderer,actor.actor.shinyVariant,bounds.x+14,y+9);

            // Name + Gender
            const char* name = actor.localizedName ? actor.localizedName : "Pokémon";
            const char* gender=actor.battleState.gender==PokemonGender::Male ? "♂" :
                actor.battleState.gender==PokemonGender::Female ? "♀" : nullptr;
            const float nameSize=renderer.drawTextFitted(name, bounds.x + 60, y + 3, 0.3125f, 112,
                isSel ? C2D_Color32(255, 255, 255, 255) : C2D_Color32(220, 215, 230, 255));
            if(gender) renderer.drawTextFitted(gender,bounds.x+110,y+17,0.25f,16,
                actor.battleState.gender==PokemonGender::Male ? C2D_Color32(110,180,255,255) : C2D_Color32(255,140,220,255));

            if(isSel) m_cursor.drawCursor(renderer,bounds.x+3,y+3,nameSize);

            // Level ("N. %u")
            char lvl[16];
            std::snprintf(lvl, sizeof(lvl), "N.%u", unsigned(actor.level));
            renderer.drawTextFitted(lvl, bounds.x + 60, y + 17, 0.25f, 42, C2D_Color32(255, 225, 90, 255));

            // Active or status indicator
            if (actor.battleState.hp == 0) {
                if(!renderer.drawHudIndicator("faint",false,bounds.x+178,y+4))
                    renderer.drawTextFitted("?",bounds.x+178,y+4,0.25f,28,0xffffffff);
            } else if (actor.battleState.status.present && actor.battleState.status.effect != PokemonStatusEffect::None) {
                // Pinned PartySlot: localized statuses atlas, native pixels.
                const char* statusKey=nullptr;
                switch(actor.battleState.status.effect) {
                    case PokemonStatusEffect::Paralysis: statusKey="paralysis";break;
                    case PokemonStatusEffect::Poison: statusKey="poison";break;
                    case PokemonStatusEffect::Toxic: statusKey="toxic";break;
                    case PokemonStatusEffect::Burn: statusKey="burn";break;
                    case PokemonStatusEffect::Sleep: statusKey="sleep";break;
                    case PokemonStatusEffect::Freeze: statusKey="freeze";break;
                    default:break;
                }
                if(!statusKey || !renderer.drawHudIndicator(statusKey,false,bounds.x+178,y+4))
                    renderer.drawTextFitted("?",bounds.x+178,y+4,0.25f,28,0xffffffff);
            } else if (i == context.activePlayerPartyIndex) {
                renderer.drawTextFitted("ACT", bounds.x + 178, y + 4, 0.26f, 28, C2D_Color32(80, 220, 140, 255));
            }

            // "PS" label and HP track groove
            const float hpX = bounds.x + 210;
            renderer.drawTextFitted("PS", hpX, y + 4, 0.20f, 14, C2D_Color32(245, 195, 60, 255));

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
        renderer.drawWindow(kPartyFooterRect.x,kPartyFooterRect.y,kPartyFooterRect.width,kPartyFooterRect.height);
        char acceptLabel[64],backLabel[64];
        std::snprintf(acceptLabel,sizeof(acceptLabel),"A: %s",runtimeUiText("party-ui-handler:sendOut"));
        std::snprintf(backLabel,sizeof(backLabel),"B: %s",runtimeUiText("party-ui-handler:cancel"));
        renderer.drawTextFitted(acceptLabel,kPartyConfirmRect.x+10,kPartyConfirmRect.y+4,0.375f,kPartyConfirmRect.width-20,0xffffffff);
        renderer.drawTextFitted(backLabel,kPartyBackRect.x+10,kPartyBackRect.y+4,0.375f,kPartyBackRect.width-20,0xffffffff);
    }

private:
    void drawVariant(Renderer2D& renderer,unsigned variant,float x,float y) {
        if(!m_variantAttempted) {
            m_variantAttempted=true;m_variantSheet=C2D_SpriteSheetLoad(kStarterVariantIconPath);
            if(m_variantSheet) {
                const auto image=C2D_SpriteSheetGetImage(m_variantSheet,0);
                if(image.tex) C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST);
            }
        }
        if(!m_variantSheet) return;
        const auto& frame=kStarterVariantIconFrames[variant];
        renderer.drawAtlasFrame(C2D_SpriteSheetGetImage(m_variantSheet,0),frame,x,y,
            frame.width,frame.height,1,kStarterVariantIconTints[variant]);
    }
    C2D_SpriteSheet m_variantSheet=nullptr;
    bool m_variantAttempted=false;
    PokemonIconPresenter m_icons{true,6,true};
    TitleMenuPresenter m_cursor;
};

}

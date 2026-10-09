#pragma once
#include "gfx/renderer2d.hpp"
#include "game/FirstRunRuntime.hpp"
#include "runtime/TitleMenuPresenter.hpp"
#include "runtime/PokemonIconPresenter.hpp"
#include "content/EntityUiNames.hpp"
#include "content/RuntimeUiText.hpp"
#include <string>
#include <cstring>
#include <cstdio>
namespace Pokerogue3DS {
class DecisionMenuPresenter {
public:
    static std::string pauseEvolutionPrompt(const char* pokemonName) {
        std::string text=runtimeUiText("menu:pauseEvolutionsQuestion");
        constexpr const char* token="{{pokemonName}}";
        const auto position=text.find(token);
        if(position!=std::string::npos)
            text.replace(position,std::strlen(token),pokemonName && *pokemonName
                ? pokemonName : runtimeUiText("command-ui-handler:pokemon"));
        return text;
    }
    void clear(Renderer2D* renderer=nullptr) {m_icons.clear(renderer);m_cursor.clear(renderer);}
    void releaseIcons(Renderer2D& renderer) {m_icons.clear(&renderer);}
    void draw(Renderer2D& renderer,const FirstRunRuntime& game) {
        renderer.clear(0xff3a303d);
        if(game.capturePartyChoicePending()) {
            renderer.drawTextFitted(game.pendingCapturedPokemon().localizedName,12,6,0.5f,294,0xffffffff);
            const auto& context=game.presentation();
            ResolvedPokemonIcon icons[6]{};const AppearanceIconIdentity* appearances[6]{};
            for(unsigned i=0;i<context.playerPartyCount && i<6;++i) {
                const auto& actor=i==context.activePlayerPartyIndex ? context.player : context.playerParty[i];
                const bool known=actor.actorIdentityResolved && actor.actor.appearanceResolved && actor.actor.gender!=PokemonGender::Unspecified;
                icons[i]=resolvePokemonIcon(actor.dex,actor.formId,known,actor.actor.gender==PokemonGender::Female,actor.actor.shiny,actor.actor.shinyVariant);
                resolvePokemonDiscoveryIcon(icons[i]);
                appearances[i]=icons[i].appearance;
            }
            m_icons.prepareAppearances(renderer,appearances,std::min(unsigned(context.playerPartyCount),6u));
            for(unsigned i=0;i<context.playerPartyCount && i<6;++i) {
                const auto& actor=i==context.activePlayerPartyIndex ? context.player : context.playerParty[i];const auto& bounds=kPartyButtonRects[i];
                const float y=bounds.y;
                renderer.drawWindow(bounds.x,y,bounds.width,bounds.height);
                bool drawn=false;
                if(icons[i].appearance) drawn=m_icons.drawAppearance(renderer,icons[i].appearance,24,y+1);
                else if(icons[i].normalIconAllowed) drawn=m_icons.draw(renderer,actor.dex,icons[i].formIndex,34,y+5,1,1);
                if(!drawn) renderer.drawTextFitted("?",34,y+5,0.3125f,20,0xffffffff);
                float nameSize=0.3125f;
                const unsigned nameLines=textLinesWithinHeight(bounds.height-8,renderer.textInkHeight(nameSize),renderer.textLineHeight(nameSize),2);
                if(!nameLines || !renderer.drawTextBox(actor.localizedName,70,y+4,nameSize,159,nameLines,0xffffffff))
                    nameSize=renderer.drawTextFitted(actor.localizedName,70,y+4,nameSize,159,0xffffffff);
                char hp[32];std::snprintf(hp,sizeof(hp),"%u/%u",actor.battleState.hp,actor.battleState.maxHp);
                renderer.drawTextFitted(hp,237,y+4,0.3125f,67,0xffffffff);
                if(i==game.selectedCapturePartyChoice()) m_cursor.drawCursor(renderer,10,y+4,nameSize);
            }
            drawActions(renderer,kPartyConfirmRect,kPartyBackRect,"A: Sustituir","B: Descartar",4);
            return;
        }
        if(game.moveLearningPending()) {
            const auto* move=PokerogueContent::findMoveById(game.pendingLearnMoveId());
            renderer.drawTextFitted(move ? moveUiName(move->id) : "",12,6,0.5f,294,0xffffffff);
            const auto& actor=game.progressionPokemon().battleState;
            for(unsigned i=0;i<4;++i) {
                const auto& rect=kLearnMoveRects[i];renderer.drawWindow(rect.x,rect.y,rect.width,rect.height);
                const auto* current=i<actor.moveCount ? PokerogueContent::findMoveById(actor.moves[i].moveId) : nullptr;
                const char* name=current ? moveUiName(current->id) : "--";
                float nameSize=0.3125f;
                const unsigned lines=textLinesWithinHeight(rect.height-18,renderer.textInkHeight(nameSize),renderer.textLineHeight(nameSize),3);
                if(!lines || !renderer.drawTextBox(name,rect.x+21,rect.y+9,nameSize,rect.width-30,lines,0xffffffff))
                    nameSize=renderer.drawTextFitted(name,rect.x+21,rect.y+9,nameSize,rect.width-30,0xffffffff);
                if(i==game.selectedBattleMove()) m_cursor.drawCursor(renderer,rect.x+7,rect.y+9,nameSize);
            }
            drawActions(renderer,kLearnConfirmRect,kLearnBackRect,"A: Aprender","B: Omitir",9);
            return;
        }
        renderer.drawWindow(12,40,296,158);
        if(game.evolutionPending()) {
            const auto& actor=game.progressionPokemon();
            if(game.evolutionPauseConfirmationPending()) {
                const auto question=pauseEvolutionPrompt(actor.localizedName);
                const float size=0.375f;
                const unsigned lines=textLinesWithinHeight(77,renderer.textInkHeight(size),renderer.textLineHeight(size),4);
                if(!lines || !renderer.drawTextBox(question.c_str(),26,55,size,267,lines,0xffffffff))
                    renderer.drawTextFitted(question.c_str(),26,55,size,267,0xffffffff);
                const auto yes=std::string("A: ")+runtimeUiText("menu:yes");
                const auto no=std::string("B: ")+runtimeUiText("menu:no");
                drawActions(renderer,kEvolutionConfirmRect,kEvolutionBackRect,yes.c_str(),no.c_str(),9);
                return;
            }
            renderer.drawTextFitted(actor.localizedName,26,55,0.5f,267,0xffffffff);
            const PokerogueContent::Species* target=nullptr;
            if(game.pendingEvolutionSpeciesId()) for(const auto& species:PokerogueContent::kSpecies)
                if(std::strcmp(species.id,game.pendingEvolutionSpeciesId())==0) {target=&species;break;}
            const char* question=target ? target->name : "?";
            if(!renderer.drawTextBox(question,26,91,0.375f,267,2,0xffffffff))
                renderer.drawTextFitted(question,26,91,0.375f,267,0xffffffff);
            drawActions(renderer,kEvolutionConfirmRect,kEvolutionBackRect,"A: Confirmar","B: Cancelar",9);
        } else {
            renderer.drawText(game.playerWon() ? "Victoria" : "Fin de partida",26,55,0.55f,0xffffffff);
            char line[64];std::snprintf(line,sizeof(line),"Ola %u   Nivel %u",unsigned(game.run().wave),unsigned(game.presentation().player.level));
            renderer.drawText(line,26,91,0.4f,0xffffffff);
            if(game.playerWon()) drawAction(renderer,kResultConfirmRect,"A: Continuar",9);
            else drawActions(renderer,kResultConfirmRect,kResultBackRect,"A: Reiniciar","B: Título",9);
        }
        // Gameplay feedback is owned by the upper dialogue banner.
    }
private:
    static void drawActions(Renderer2D& renderer,const TouchRect& confirm,const TouchRect& back,
        const char* confirmText,const char* backText,unsigned insetY) {
        drawAction(renderer,confirm,confirmText,insetY);
        drawAction(renderer,back,backText,insetY);
    }
    static void drawAction(Renderer2D& renderer,const TouchRect& bounds,const char* label,unsigned insetY) {
        renderer.drawWindow(bounds.x,bounds.y,bounds.width,bounds.height);
        renderer.drawTextFitted(label,bounds.x+8,bounds.y+insetY,0.375f,bounds.width-16,0xffffffff);
    }
    TitleMenuPresenter m_cursor;PokemonIconPresenter m_icons{true,6,true};
};
}

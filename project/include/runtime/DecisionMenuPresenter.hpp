#pragma once
#include "gfx/renderer2d.hpp"
#include "game/FirstRunRuntime.hpp"
#include "runtime/TitleMenuPresenter.hpp"
#include "runtime/PokemonIconPresenter.hpp"
#include "content/EntityUiNames.hpp"
#include <cstring>
#include <cstdio>
namespace Pokerogue3DS {
class DecisionMenuPresenter {
public:
    void clear() {m_icons.clear();m_cursor.clear();}
    void draw(Renderer2D& renderer,const FirstRunRuntime& game) {
        renderer.clear(0xff3a303d);
        if(game.capturePartyChoicePending()) {
            renderer.drawTextFitted(game.pendingCapturedPokemon().localizedName,12,6,0.5f,294,0xffffffff);
            const auto& context=game.presentation();
            for(unsigned i=0;i<context.playerPartyCount && i<6;++i) {
                const auto& actor=context.playerParty[i];const auto& bounds=kPartyButtonRects[i];
                const float y=bounds.y;
                renderer.drawWindow(bounds.x,y,bounds.width,bounds.height);
                const auto* form=PokerogueContent::findFormById(actor.formId);
                const unsigned formIndex=form ? form->upstreamFormIndex : 0;
                m_icons.draw(renderer,actor.dex,formIndex,34,y+5,1,1);
                const float nameSize=renderer.drawTextFitted(actor.localizedName,61,y+3,0.36f,168,0xffffffff);
                char hp[32];std::snprintf(hp,sizeof(hp),"%u/%u",actor.battleState.hp,actor.battleState.maxHp);
                renderer.drawTextFitted(hp,237,y+3,0.32f,67,0xffffffff);
                if(i==game.selectedCapturePartyChoice()) m_cursor.drawCursor(renderer,17,y+3,nameSize);
            }
            renderer.drawTextFitted("A: sustituir   B: no incorporar",12,213,0.32f,296,0xffffffff);
            return;
        }
        if(game.moveLearningPending()) {
            const auto* move=PokerogueContent::findMoveById(game.pendingLearnMoveId());
            renderer.drawTextFitted(move ? moveUiName(move->id) : "",12,6,0.5f,294,0xffffffff);
            const auto& actor=game.progressionPokemon().battleState;
            for(unsigned i=0;i<4;++i) {
                const auto& rect=kMoveButtonRects[i];renderer.drawWindow(rect.x,rect.y,rect.width,rect.height);
                const auto* current=i<actor.moveCount ? PokerogueContent::findMoveById(actor.moves[i].moveId) : nullptr;
                const char* name=current ? moveUiName(current->id) : "--";
                float nameSize=0.3125f;
                if(!renderer.drawTextBox(name,rect.x+21,rect.y+9,nameSize,rect.width-30,3,0xffffffff))
                    nameSize=renderer.drawTextFitted(name,rect.x+21,rect.y+9,nameSize,rect.width-30,0xffffffff);
                if(i==game.selectedBattleMove()) m_cursor.drawCursor(renderer,rect.x+7,rect.y+9,nameSize);
            }
            renderer.drawTextFitted("A: aprender   B: no aprender",12,204,0.32f,296,0xffffffff);
            return;
        }
        renderer.drawWindow(12,40,296,158);
        if(game.evolutionPending()) {
            const auto& actor=game.progressionPokemon();
            renderer.drawTextFitted(actor.localizedName,26,55,0.5f,267,0xffffffff);
            const PokerogueContent::Species* target=nullptr;
            if(game.pendingEvolutionSpeciesId()) for(const auto& species:PokerogueContent::kSpecies)
                if(std::strcmp(species.id,game.pendingEvolutionSpeciesId())==0) {target=&species;break;}
            const char* question=game.evolutionPauseConfirmationPending() ? "Pausar evoluciones futuras?" :
                target ? target->name : "Evolucion";
            if(!renderer.drawTextBox(question,26,91,0.375f,267,2,0xffffffff))
                renderer.drawTextFitted(question,26,91,0.375f,267,0xffffffff);
            renderer.drawTextFitted("A: confirmar   B: cancelar",26,145,0.35f,267,0xffffffff);
        } else {
            renderer.drawText(game.playerWon() ? "Victoria" : "Fin de partida",26,55,0.55f,0xffffffff);
            char line[64];std::snprintf(line,sizeof(line),"Ola %u   Nivel %u",unsigned(game.run().wave),unsigned(game.presentation().player.level));
            renderer.drawText(line,26,91,0.4f,0xffffffff);
            renderer.drawText(game.playerWon() ? "A: continuar" : "A: nueva partida",26,145,0.4f,0xffffffff);
        }
        renderer.drawTextFitted(game.battleFeedback().c_str(),12,213,0.3f,296,0xffffffff);
    }
private:
    TitleMenuPresenter m_cursor;PokemonIconPresenter m_icons{true};
};
}

#pragma once
#include "gfx/renderer2d.hpp"
#include "game/FirstRunRuntime.hpp"
#include "game/PokemonStarterMoveset.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include "runtime/PokemonIconPresenter.hpp"
#include "runtime/StarterGridLayout.hpp"
#include "runtime/TitleMenuPresenter.hpp"
#include "runtime/IntroCinematicPresenter.hpp"
#include "game/PokemonFreshProfile.hpp"
#include "content/RuntimeUiText.hpp"
#include "content/EntityUiNames.hpp"
#include "storage/NativeStarterCandyProfile.hpp"
#include "storage/NativeProgressStore.hpp"
#include <cstdio>
namespace Pokerogue3DS {
class SetupPresenter {
public:
    SetupPresenter()=default;
    SetupPresenter(const SetupPresenter&)=delete;
    SetupPresenter& operator=(const SetupPresenter&)=delete;
    ~SetupPresenter() {clear();}
    void clear() {
        if(m_logo) C2D_SpriteSheetFree(m_logo);
        if(m_background) C2D_SpriteSheetFree(m_background);
        if(m_grid) C2D_SpriteSheetFree(m_grid);
        m_logo=nullptr;m_background=nullptr;m_grid=nullptr;m_icons.clear();m_prompt.clear();
        m_introCinematic.clear();
    }
    bool confirmStart=false,confirmYes=true,formsOpen=false,candyStoreOpen=false;
    unsigned selectedForm=0,candyStoreSelection=0;
    const char* formFeedback=nullptr;
    const char* feedback=nullptr;
    const char* candyFeedback=nullptr;

    void openCandyStore() { candyStoreOpen = true; candyStoreSelection = 0; candyFeedback = nullptr; }
    void closeCandyStore() { candyStoreOpen = false; candyFeedback = nullptr; }

    static const NativeStarterCandyRecord* candyRecord(const FirstRunRuntime& game, uint16_t dex) {
        for(size_t i = 0; i < game.starterProfileCount(); ++i) {
            const auto& row = game.starterProfileRecords()[i];
            if(row.speciesDex == dex) return &row;
        }
        return nullptr;
    }
    static uint16_t candyBalance(const FirstRunRuntime& game, uint16_t dex) {
        const auto* rec = candyRecord(game, dex);
        return rec ? rec->candyCount : 0;
    }
    static bool passiveUnlocked(const FirstRunRuntime& game, uint16_t dex) {
        const auto* rec = candyRecord(game, dex);
        return rec ? rec->passiveUnlocked : false;
    }
    static const PokerogueContent::StarterCandyPrice* candyPriceFor(uint16_t dex) {
        const auto* species = PokerogueContent::findSpeciesByDex(dex);
        if(!species || !species->starterEligible || species->starterCost < 1) return nullptr;
        for(const auto& row : PokerogueContent::kStarterCandyPrices) {
            if(row.cost == species->starterCost) return &row;
        }
        return nullptr;
    }
    static uint16_t costReductionPrice(const FirstRunRuntime& game, uint16_t dex) {
        const auto* rec = candyRecord(game, dex);
        const auto* price = candyPriceFor(dex);
        if(!price || !rec || rec->costReduction >= 2) return 0;
        return price->costReduction[rec->costReduction];
    }
    static uint16_t passiveUnlockPrice(uint16_t dex) {
        const auto* price = candyPriceFor(dex);
        return price ? price->passive : 0;
    }

    bool handleCandyStoreInput(uint32_t rawPressed, FirstRunRuntime& game, NativeProgressStore* store = nullptr) {
        if(!candyStoreOpen) return false;
        if(rawPressed & 0x0002 /* KEY_B */) {
            candyStoreOpen = false;
            candyFeedback = nullptr;
            return true;
        }
        if(rawPressed & (0x0040 /* KEY_UP */ | 0x00010000 /* KEY_CPAD_UP */)) {
            candyStoreSelection = (candyStoreSelection == 0) ? 1 : 0;
            candyFeedback = nullptr;
            return true;
        }
        if(rawPressed & (0x0080 /* KEY_DOWN */ | 0x00020000 /* KEY_CPAD_DOWN */)) {
            candyStoreSelection = (candyStoreSelection == 0) ? 1 : 0;
            candyFeedback = nullptr;
            return true;
        }
        if(rawPressed & 0x0001 /* KEY_A */) {
            const uint16_t dex = game.selectedSetupStarterDex();
            const auto* rec = candyRecord(game, dex);
            if(!rec) {
                candyFeedback = "Registro no disponible.";
                return true;
            }
            if(candyStoreSelection == 0) {
                if(rec->costReduction >= 2) {
                    candyFeedback = "Reduccion al maximo (2/2).";
                    return true;
                }
                const uint16_t req = costReductionPrice(game, dex);
                if(rec->candyCount < req) {
                    candyFeedback = "Caramelos insuficientes.";
                    return true;
                }
                if(store) {
                    StarterCostPurchaseResult pres{};
                    const auto saveRes = game.purchaseStarterCostReduction(dex, *store, &pres);
                    if(saveRes == NativeSaveResult::Ok && pres == StarterCostPurchaseResult::Applied) {
                        candyFeedback = "Coste reducido con exito!";
                        return true;
                    }
                    candyFeedback = "Error al guardar compra.";
                    return true;
                } else {
                    auto mutRecord = *rec;
                    const auto pres = applyNativeStarterCostReduction(mutRecord);
                    if(pres == StarterCostPurchaseResult::Applied) {
                        candyFeedback = "Coste reducido con exito!";
                        return true;
                    }
                    candyFeedback = "No se pudo aplicar la reduccion.";
                    return true;
                }
            } else {
                if(rec->passiveUnlocked) {
                    candyFeedback = "Habilidad pasiva ya desbloqueada.";
                    return true;
                }
                const uint16_t req = passiveUnlockPrice(dex);
                if(rec->candyCount < req) {
                    candyFeedback = "Caramelos insuficientes.";
                    return true;
                }
                if(store) {
                    StarterPassivePurchaseResult pres{};
                    if(game.purchaseStarterPassiveUnlock(rec->speciesDex, *store, &pres) && pres == StarterPassivePurchaseResult::Applied) {
                        candyFeedback = "Pasiva desbloqueada con exito!";
                        return true;
                    }
                    candyFeedback = "Error al guardar compra.";
                    return true;
                } else {
                    auto mutRecord = *rec;
                    const auto pres = applyNativeStarterPassiveUnlock(mutRecord, req);
                    if(pres == StarterPassivePurchaseResult::Applied) {
                        candyFeedback = "Pasiva desbloqueada con exito!";
                        return true;
                    }
                    candyFeedback = "No se pudo desbloquear la pasiva.";
                    return true;
                }
            }
        }
        return false;
    }
    static unsigned formCount(const FirstRunRuntime& game) {
        const auto* species=PokerogueContent::findSpeciesByDex(game.selectedSetupStarterDex());
        if(!species) return 0;
        unsigned count=1;
        for(const auto& form:PokerogueContent::kForms)
            if(form.upstreamFormIndex && std::strcmp(form.speciesId,species->id)==0) ++count;
        return count;
    }
    static uint16_t formIndexAt(const FirstRunRuntime& game,unsigned ordinal) {
        if(!ordinal) return 0;
        const auto* species=PokerogueContent::findSpeciesByDex(game.selectedSetupStarterDex());
        if(!species) return 65535;
        for(const auto& form:PokerogueContent::kForms)
            if(form.upstreamFormIndex && std::strcmp(form.speciesId,species->id)==0 && !--ordinal) return form.upstreamFormIndex;
        return 65535;
    }
    static bool formUnlocked(const FirstRunRuntime& game,uint16_t index) {
        for(size_t i=0;i<game.starterProfileCount();++i) {
            const auto& row=game.starterProfileRecords()[i];
            if(row.speciesDex==game.selectedSetupStarterDex())
                return pokemonValidateStarterForm(row.speciesDex,index,row.unlockedFormAttr)==PokemonStarterFormResult::Ok;
        }
        return false;
    }
    static unsigned count() {unsigned n=0;for(const auto& row:PokerogueContent::kSpecies) if(row.starterEligible) ++n;return n;}
    static const PokerogueContent::Species* at(unsigned ordinal) {
        for(const auto& row:PokerogueContent::kSpecies) if(row.starterEligible) {if(!ordinal--) return &row;}
        return nullptr;
    }
    static unsigned selectedOrdinal(const FirstRunRuntime& game) {
        unsigned n=0;for(const auto& row:PokerogueContent::kSpecies) if(row.starterEligible) {
            if(row.dex==game.selectedSetupStarterDex()) return n;
            ++n;
        }
        return 0;
    }
    static bool move(FirstRunRuntime& game,int delta) {
        const int total=int(count());if(!total) return false;
        const int ordinal=((int(selectedOrdinal(game))+delta)%total+total)%total;
        const auto* species=at(unsigned(ordinal));return species && game.selectSetupStarter(species->dex);
    }
    static bool touch(FirstRunRuntime& game,unsigned x,unsigned y) {
        const int cell=starterGridAt(x,y);if(cell<0) return false;
        const auto* species=at(selectedOrdinal(game)/kStarterGridPageSize*kStarterGridPageSize+unsigned(cell));
        return species && game.selectSetupStarter(species->dex);
    }
    void drawBackground(Renderer2D& renderer) {
        if(!m_background) {
            m_background=C2D_SpriteSheetLoad("romfs:/presentation/ui/starter_select_bg.t3x");
            if(m_background) {
                const auto img=C2D_SpriteSheetGetImage(m_background,0);
                if(img.tex) C3D_TexSetFilter(img.tex, GPU_NEAREST, GPU_NEAREST);
            }
        }
        renderer.clear(0xff303030);
        if(m_background) renderer.drawImageDirect(C2D_SpriteSheetGetImage(m_background,0),0,0,400,225);
    }
    void skipIntro() { m_introCinematic.skip(); }
    void drawTop(Renderer2D& renderer,const FirstRunRuntime& game,bool showMode=true,uint64_t animationTimeMs=0) {
        if(!showMode) {
            if(m_introCinematic.active()) {
                if(m_introCinematic.draw(renderer, animationTimeMs)) return;
            }
            if(!m_logo) {
                m_logo=C2D_SpriteSheetLoad("romfs:/presentation/images/logo.t3x");
                if(m_logo) {
                    const auto img=C2D_SpriteSheetGetImage(m_logo,0);
                    if(img.tex) C3D_TexSetFilter(img.tex, GPU_NEAREST, GPU_NEAREST);
                }
            }
            if(m_logo) {
                const auto image=C2D_SpriteSheetGetImage(m_logo,0);
                if(image.subtex && image.subtex->width) renderer.drawImageDirect(image,65,12,270,image.subtex->height*(270.0f/image.subtex->width));
            }
            return;
        }
        const auto* species=PokerogueContent::findSpeciesByDex(game.selectedSetupStarterDex());
        if(!species) return;
        const auto* form=PokerogueContent::findFormByUpstreamIndex(species->dex,game.setupStarterFormIndex(species->dex));
        renderer.drawWindow(151,12,241,214);
        renderer.drawTextFitted(species->name,161,22,0.6f,220,0xffffffff);
        renderer.drawTypeLabel((form ? form->type1 : species->type1),161,49,32,14);
        if((form ? form->type2 : species->type2)) renderer.drawTypeLabel((form ? form->type2 : species->type2),207,49,32,14);
        char label[80];uint16_t quarters=0;
        const uint8_t reduction=game.starterCostReduction(species->dex);
        if(pokemonStarterCostQuarterUnits(species->dex,reduction,quarters)) {
            if(reduction)
                std::snprintf(label,sizeof(label),"Coste: %u.%02u (-%u)",unsigned(quarters/4),unsigned(quarters%4)*25,unsigned(reduction));
            else
                std::snprintf(label,sizeof(label),"Coste: %u.%02u pts",unsigned(quarters/4),unsigned(quarters%4)*25);
        } else std::snprintf(label,sizeof(label),"Coste no disponible");
        renderer.drawText(label,161,73,0.4f,0xffffffff);
        const char* ability=abilityUiName((form ? form->ability1 : species->ability1));
        if(!renderer.drawTextBox(ability ? ability : "",161,94,0.3125f,220,2,0xffffffff))
            renderer.drawTextFitted(ability ? ability : "",161,99,0.3125f,220,0xffffffff);
        renderer.drawText(game.starterUnlocked(species->dex) ? "Disponible" : runtimeUiText("starter-select-ui-handler:locked"),161,122,0.32f,0xffffffff);
        std::snprintf(label,sizeof(label),"PS %u   ATQ %u   DEF %u",(form ? form->hp : species->hp),(form ? form->atk : species->atk),(form ? form->def : species->def));
        renderer.drawText(label,161,146,0.33f,0xffffffff);
        std::snprintf(label,sizeof(label),"AE %u   DE %u   VEL %u",(form ? form->spatk : species->spatk),(form ? form->spdef : species->spdef),(form ? form->speed : species->speed));
        renderer.drawText(label,161,168,0.33f,0xffffffff);
        const unsigned bst=(form ? form->hp : species->hp)+(form ? form->atk : species->atk)+(form ? form->def : species->def)+(form ? form->spatk : species->spatk)+(form ? form->spdef : species->spdef)+(form ? form->speed : species->speed);
        std::snprintf(label,sizeof(label),"Total base (BST): %u",bst);
        renderer.drawText(label,161,193,0.3f,0xffffffff);
        renderer.drawText(game.presentation().modeName ? game.presentation().modeName : "",16,217,0.4f,0xffffffff);
    }
    void drawBottom(Renderer2D& renderer,const FirstRunRuntime& game) {
        renderer.clear(0xff606800);
        char label[80];
        const unsigned ordinal=selectedOrdinal(game),start=ordinal/kStarterGridPageSize*kStarterGridPageSize;
        uint16_t totalCost=0;bool costResolved=true;
        for(unsigned i=0;i<game.presentation().playerPartyCount && i<6;++i) {
            uint16_t quarterUnits=0;const auto dex=game.presentation().playerParty[i].dex;
            if(!pokemonStarterCostQuarterUnits(dex,game.starterCostReduction(dex),quarterUnits)) costResolved=false;
            else totalCost+=quarterUnits;
        }
        if(costResolved) std::snprintf(label,sizeof(label),"%u / %u   Coste: %u.%02u / %u pts",ordinal+1,count(),
            unsigned(totalCost/4),unsigned(totalCost%4)*25,unsigned(kClassicStarterValueLimit));
        else std::snprintf(label,sizeof(label),"%u / %u   Coste sin resolver",ordinal+1,count());
        renderer.drawText(label,13,5,0.4f,0xffffffff);
        if(!m_grid) {
            m_grid=C2D_SpriteSheetLoad("romfs:/presentation/ui/starter_container_bg.t3x");
            if(m_grid) {
                const auto img=C2D_SpriteSheetGetImage(m_grid,0);
                if(img.tex) C3D_TexSetFilter(img.tex, GPU_NEAREST, GPU_NEAREST);
            }
        }
        if(m_grid) renderer.drawImageDirect(C2D_SpriteSheetGetImage(m_grid,0),8,29,304,154);
        else renderer.drawWindow(8,29,304,154);
        for(unsigned i=0;i<kStarterGridPageSize;++i) {
            const auto* species=at(start+i);if(!species) break;
            const float x=16+(i%6)*48,y=38+(i/6)*36;
            if(start+i==ordinal) renderer.drawRect(x,y,46,34,0xff827660);
            if(!m_icons.draw(renderer,species->dex,game.setupStarterFormIndex(species->dex),x+3,y+2,
                game.starterUnlocked(species->dex) ? 1.0f : 0.35f))
                renderer.drawText("?",x+16,y+8,0.4f,0xffffffff);
        }
        const auto& context=game.presentation();
        for(unsigned slot=0;slot<6;++slot) {
            renderer.drawRect(8+slot*50,186,46,26,slot<context.playerPartyCount ? 0xff463747 : 0xff2e2630);
            renderer.drawWindow(9+slot*50,187,44,24);
        }
        for(unsigned i=0;i<context.playerPartyCount && i<6;++i)
            m_icons.draw(renderer,context.playerParty[i].dex,game.setupStarterFormIndex(context.playerParty[i].dex),10+i*50,188);
        if(feedback) renderer.drawText(feedback,10,216,0.28f,0xff80ffff);
        else renderer.drawText("SELECT: Formas    START: Comenzar    B: Volver",10,216,0.28f,0xffffffff);
        renderer.drawText("Tocar: elegir/anadir    L/R: pagina",10,228,0.24f,0xffe0e0e0);
        if(confirmStart) {
            renderer.drawWindow(12,66,296,105);
            renderer.drawText(runtimeUiText("starter-select-ui-handler:confirmStartTeam"),24,80,0.34f,0xffffffff);
            renderer.drawRect(45,116,105,32,confirmYes ? 0xff70d8f0 : 0xff463747);
            renderer.drawWindow(47,118,101,28);
            renderer.drawText(runtimeUiText("menu:yes"),72,123,0.45f,0xffffffff);
            renderer.drawRect(170,116,105,32,!confirmYes ? 0xff70d8f0 : 0xff463747);
            renderer.drawWindow(172,118,101,28);
            renderer.drawText(runtimeUiText("menu:no"),212,123,0.45f,0xffffffff);
            m_prompt.drawCursor(renderer,confirmYes ? 55 : 195,123,0.45f);
        } else if(formsOpen) {
            renderer.drawWindow(12,30,296,174);
            const auto* species=PokerogueContent::findSpeciesByDex(game.selectedSetupStarterDex());
            const unsigned start=selectedForm/6*6;
            for(unsigned i=0;i<6 && start+i<formCount(game);++i) {
                const uint16_t index=formIndexAt(game,start+i);
                const auto* form=PokerogueContent::findFormByUpstreamIndex(game.selectedSetupStarterDex(),index);
                renderer.drawText(form ? form->name : species ? species->name : "",43,43+i*23,0.36f,
                    formUnlocked(game,index) ? 0xffffffff : 0xff909090);
                if(start+i==selectedForm) m_prompt.drawCursor(renderer,25,43+i*23,0.36f);
            }
            renderer.drawText(formFeedback ? formFeedback : "A: elegir forma   B: volver",24,182,0.28f,0xffffffff);
        } else if(candyStoreOpen) {
            renderer.drawWindow(12,24,296,186);
            const uint16_t dex=game.selectedSetupStarterDex();
            const auto* species=PokerogueContent::findSpeciesByDex(dex);
            const auto* rec=candyRecord(game,dex);
            const uint16_t candies=rec ? rec->candyCount : 0;
            const uint8_t red=rec ? rec->costReduction : 0;
            const bool passive=rec ? rec->passiveUnlocked : false;
            const auto* price=candyPriceFor(dex);

            renderer.drawText("Tienda de Caramelos",24,30,0.42f,0xffffffff);
            if(species) renderer.drawText(species->name,24,52,0.36f,0xff70d8f0);
            char cbuf[80];
            std::snprintf(cbuf,sizeof(cbuf),"Caramelos: %u",unsigned(candies));
            renderer.drawText(cbuf,175,52,0.36f,0xffffd700);

            // Option 0: Cost reduction
            renderer.drawRect(20,74,280,44,candyStoreSelection==0 ? 0xff70d8f0 : 0xff463747);
            renderer.drawWindow(22,76,276,40);
            if(candyStoreSelection==0) m_prompt.drawCursor(renderer,28,80,0.35f);
            renderer.drawText("Reduccion de coste",46,80,0.35f,0xffffffff);
            if(red>=2) {
                std::snprintf(cbuf,sizeof(cbuf),"Nivel: 2/2 (MAX)");
            } else if(price) {
                std::snprintf(cbuf,sizeof(cbuf),"Nivel: %u/2  -  Coste: %u caramelos",unsigned(red),unsigned(price->costReduction[red]));
            } else {
                std::snprintf(cbuf,sizeof(cbuf),"No disponible");
            }
            renderer.drawText(cbuf,46,98,0.28f,0xffe0e0e0);

            // Option 1: Passive ability
            renderer.drawRect(20,124,280,44,candyStoreSelection==1 ? 0xff70d8f0 : 0xff463747);
            renderer.drawWindow(22,126,276,40);
            if(candyStoreSelection==1) m_prompt.drawCursor(renderer,28,130,0.35f);
            renderer.drawText("Habilidad Pasiva",46,130,0.35f,0xffffffff);
            if(passive) {
                std::snprintf(cbuf,sizeof(cbuf),"Estado: Desbloqueada");
            } else if(price) {
                std::snprintf(cbuf,sizeof(cbuf),"Coste: %u caramelos",unsigned(price->passive));
            } else {
                std::snprintf(cbuf,sizeof(cbuf),"No disponible");
            }
            renderer.drawText(cbuf,46,148,0.28f,0xffe0e0e0);

            if(candyFeedback) renderer.drawText(candyFeedback,24,176,0.28f,0xff80ffff);
            else renderer.drawText("A: Comprar   B: Volver   Arriba/Abajo: Elegir",24,176,0.28f,0xffffffff);
        }
    }
private:
    C2D_SpriteSheet m_logo=nullptr,m_background=nullptr,m_grid=nullptr;
    PokemonIconPresenter m_icons;
    TitleMenuPresenter m_prompt;
    IntroCinematicPresenter m_introCinematic;
};
}

#pragma once
#include "content/AudioVolumePolicy.hpp"
#include "runtime/TitleMenuPresenter.hpp"
#include "content/FrontendModes.hpp"
#include "content/RuntimeUiText.hpp"
#include "content/EggUiText.hpp"
#include "runtime/EggTexturePresenter.hpp"
#include "storage/NativeRunSave.hpp"
#include "game/FirstRunRuntime.hpp"
#include "runtime/PokemonIconPresenter.hpp"
#include "runtime/StarterGridLayout.hpp"
#include "content/StarterVariantIcons.hpp"
#include <3ds.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
namespace Pokerogue3DS {
enum class FrontendPage {Title,Modes,Load,History,Settings,SettingsGroup,GlobalMenu,Pokedex,ServiceInfo,ManageData};
enum class FrontendCommand {None,Continue,NewClassic,Load,DeleteSave,NextWindowStyle,PreviousWindowStyle,ToggleTouchControls,ExportProgress,ImportProgress,NextHpBarSpeed,PreviousHpBarSpeed,NextExpGainsSpeed,PreviousExpGainsSpeed,NextMasterVolume,PreviousMasterVolume,NextUiVolume,PreviousUiVolume};
// Owns navigation and presentation only. Returned commands are handled by main.
class FrontendMenuPresenter {
public:
    explicit FrontendMenuPresenter(bool hasSave):m_titleSelection{hasSave,0} {}
    ~FrontendMenuPresenter() { clear(); }
    void clear(Renderer2D* renderer=nullptr) {m_eggTextures.clear(renderer);m_title.clear(renderer);m_dexIcons.clear(renderer);
        if(m_dexVariants) {if(renderer) renderer->retireSpriteSheet(m_dexVariants);else C2D_SpriteSheetFree(m_dexVariants);}
        m_dexVariants=nullptr;m_dexVariantsAttempted=false; m_confirmingDelete=false;m_confirmingImport=false;m_navigationSound=nullptr;}
    void setExpGainsSpeed(unsigned speed) {if(speed<=3) m_expGainsSpeed=speed;}
    void setHpBarSpeed(unsigned speed) {if(speed<=3) m_hpBarSpeed=speed;}
    void setAudioVolumes(unsigned master,unsigned ui) {if(master<=kAudioVolumeMax && ui<=kAudioVolumeMax) {m_masterVolume=master;m_uiVolume=ui;}}
    void setTouchControls(bool enabled) { m_touchControls=enabled; }
    bool confirmingTouchDisable() const { return m_confirmingTouchDisable; }
    static uint32_t filterTouchInput(uint32_t keys,bool enabled) {return enabled ? keys : keys & ~KEY_TOUCH;}
    FrontendPage page() const {return m_page;}
    bool overlaysTitle() const {
        return m_page==FrontendPage::GlobalMenu || m_page==FrontendPage::ServiceInfo ||
            m_page==FrontendPage::ManageData ||
            (m_settingsFromGlobal && (m_page==FrontendPage::Settings || m_page==FrontendPage::SettingsGroup));
    }
    void feedback(const char* value) {m_feedback=value;}
    void setAudioInitializationError(const char* error) {m_audioInitializationError=error;}
    bool isConfirmingDelete() const {return m_confirmingDelete;}
    bool isConfirmingImport() const {return m_confirmingImport;}
    void setHasSave(bool hasSave) {
        m_titleSelection.hasContinue=hasSave;
        m_titleSelection.selected=0;
        if(!hasSave) m_confirmingDelete=false;
    }
    // Feedback belongs to the information screen. It must not replace active
    // lower-screen controls while their touch rectangles remain enabled.
    void drawFeedbackTop(Renderer2D& renderer) const {
        const char* message=m_feedback;
        if((!message || !*message) && m_page==FrontendPage::SettingsGroup && m_group==2)
            message=m_audioInitializationError;
        if(!message || !*message) return;
        const auto& bounds=kFrontendFeedbackRect;
        renderer.drawWindow(bounds.x,bounds.y,bounds.width,bounds.height);
        constexpr float size=0.375f;
        const unsigned lines=textLinesWithinHeight(bounds.height-12,
            renderer.textInkHeight(size),renderer.textLineHeight(size),2);
        if(!lines || !renderer.drawTextBox(message,bounds.x+8,bounds.y+6,
            size,bounds.width-16,lines,0xffffffff))
            renderer.drawTextFitted(message,bounds.x+8,bounds.y+6,
                size,bounds.width-16,0xffffffff);
    }
    void drawCursor(Renderer2D& renderer, float x, float y, float size) {
        m_title.drawCursor(renderer, x, y, size);
    }
    const char* takeNavigationSound() {
        const char* sound=m_navigationSound;m_navigationSound=nullptr;return sound;
    }
    FrontendCommand input(uint32_t keys,unsigned touchX=0,unsigned touchY=0,const FirstRunRuntime* game=nullptr) {
        m_navigationSound=nullptr;m_navigationRejected=false;
        const auto before=navigationState();
        const auto previousPage=m_page;
        const char* previousFeedback=m_feedback;
        const auto command=inputNavigation(keys,touchX,touchY,game);
        if(m_navigationRejected || (m_feedback && m_feedback!=previousFeedback)) m_navigationSound="error";
        else if(previousPage!=FrontendPage::GlobalMenu && m_page==FrontendPage::GlobalMenu)
            m_navigationSound="menu_open";
        else if(command!=FrontendCommand::None || before!=navigationState()) m_navigationSound="select";
        return command;
    }
private:
    // UI event identity is independent of whether an engine command was emitted.
    std::array<size_t,14> navigationState() const {
        return {size_t(m_page),m_titleSelection.selected,m_selected,m_group,m_service,
            m_globalSelection,m_dexSelected,m_dexGeneration,m_dexCapture,
            m_eggSelected,size_t(m_eggDetails),size_t(m_confirmingDelete),
            size_t(m_confirmingTouchDisable),size_t(m_confirmingImport)*2+size_t(m_importYes)};
    }
    FrontendCommand inputNavigation(uint32_t keys,unsigned touchX,unsigned touchY,const FirstRunRuntime* game) {
        // Footer touches emit the same action as physical buttons. The previous
        // full-width back region swallowed the displayed A action.
        if((keys & KEY_TOUCH) && m_page!=FrontendPage::Title) {
            const bool readOnly=m_page==FrontendPage::Pokedex ||
                (m_page==FrontendPage::ServiceInfo && m_service!=3);
            if(readOnly && kFrontendReadOnlyBackRect.contains(touchX,touchY)) keys|=KEY_B;
            else if(kFrontendConfirmRect.contains(touchX,touchY)) keys|=KEY_A;
            else if(kFrontendBackRect.contains(touchX,touchY)) keys|=KEY_B;
        }
        if(m_confirmingImport) {
            if(keys & KEY_B) {m_confirmingImport=false;return FrontendCommand::None;}
            if(keys & (KEY_DLEFT | KEY_CPAD_LEFT)) m_importYes=true;
            if(keys & (KEY_DRIGHT | KEY_CPAD_RIGHT)) m_importYes=false;
            if(keys & KEY_A) {
                m_confirmingImport=false;
                return m_importYes ? FrontendCommand::ImportProgress : FrontendCommand::None;
            }
            if((keys & KEY_TOUCH) && kConfirmationYesRect.contains(touchX,touchY)) {
                m_confirmingImport=false;return FrontendCommand::ImportProgress;
            }
            if((keys & KEY_TOUCH) && kConfirmationNoRect.contains(touchX,touchY)) m_confirmingImport=false;
            return FrontendCommand::None;
        }
        if(m_confirmingTouchDisable) {
            if(keys & KEY_B) {m_confirmingTouchDisable=false;return FrontendCommand::None;}
            if(keys & KEY_A) {m_confirmingTouchDisable=false;return FrontendCommand::ToggleTouchControls;}
            if(keys & KEY_TOUCH) {
                if(kConfirmationYesRect.contains(touchX,touchY)) {
                    m_confirmingTouchDisable=false;return FrontendCommand::ToggleTouchControls;
                }
                if(kConfirmationNoRect.contains(touchX,touchY)) m_confirmingTouchDisable=false;
            }
            return FrontendCommand::None;
        }
        if(m_page==FrontendPage::ServiceInfo && m_service==3 && m_eggDetails
            && ((keys & KEY_B) || ((keys & KEY_TOUCH) && kEggListBackRect.contains(touchX,touchY)))) {
            m_eggDetails=false;return FrontendCommand::None;
        }
        if(keys & KEY_B) {
            if(m_confirmingDelete) {
                m_confirmingDelete = false;
                return FrontendCommand::None;
            }
            if(m_page==FrontendPage::SettingsGroup) m_page=FrontendPage::Settings;
            else if((m_page==FrontendPage::Settings && m_settingsFromGlobal) || m_page==FrontendPage::Pokedex || m_page==FrontendPage::ServiceInfo || m_page==FrontendPage::ManageData) m_page=FrontendPage::GlobalMenu;
            else m_page=FrontendPage::Title;
            m_selected=m_page==FrontendPage::GlobalMenu ? m_globalSelection : 0;
            m_feedback=nullptr;return FrontendCommand::None;
        }
        if(m_page==FrontendPage::ServiceInfo && m_service==3) {
            const size_t count=game ? game->eggInventoryCount() : 0;
            if(!count) return FrontendCommand::None;
            if(m_eggSelected>=count) m_eggSelected=0;
            if(keys & (KEY_UP | KEY_CPAD_UP)) m_eggSelected=(m_eggSelected+count-1)%count;
            else if(keys & (KEY_DOWN | KEY_CPAD_DOWN)) m_eggSelected=(m_eggSelected+1)%count;
            if(!m_eggDetails && (keys & KEY_TOUCH)) {
                const size_t first=(m_eggSelected/5)*5;
                for(size_t row=0;row<5 && first+row<count;++row)
                    if(eggListRowRectangle(unsigned(row)).contains(touchX,touchY)) {
                        m_eggSelected=first+row;m_eggDetails=true;break;
                    }
            }
            if((keys & KEY_A) || ((keys & KEY_TOUCH) && kEggListConfirmRect.contains(touchX,touchY))) m_eggDetails=true;
            return FrontendCommand::None;
        }
        if(m_page==FrontendPage::Pokedex) {
            if((keys & KEY_X) || ((keys & KEY_TOUCH) && kPokedexFilterRects[0].contains(touchX,touchY))) {
                unsigned next=0;
                for(const auto& species:PokerogueContent::kSpecies)
                    if(species.generation>m_dexGeneration && (!next || species.generation<next)) next=species.generation;
                m_dexGeneration=next;m_dexSelected=0;
            }
            if((keys & KEY_Y) || ((keys & KEY_TOUCH) && kPokedexFilterRects[1].contains(touchX,touchY))) {
                m_dexCapture=(m_dexCapture+1)%4;m_dexSelected=0;
            }
            const unsigned total=dexCount(game);
            if(m_dexSelected>=total) m_dexSelected=0;
            if(!total) return FrontendCommand::None;
            if(keys & (KEY_DLEFT | KEY_CPAD_LEFT)) m_dexSelected=(m_dexSelected+total-1)%total;
            if(keys & (KEY_DRIGHT | KEY_CPAD_RIGHT)) m_dexSelected=(m_dexSelected+1)%total;
            if(keys & (KEY_DUP | KEY_CPAD_UP)) m_dexSelected=m_dexSelected>=6 ? m_dexSelected-6 : 0;
            if(keys & (KEY_DDOWN | KEY_CPAD_DOWN)) m_dexSelected=std::min(m_dexSelected+6,total-1);
            const bool previous=(keys & KEY_L) || ((keys & KEY_TOUCH) && kPokedexPageRects[0].contains(touchX,touchY));
            const bool next=(keys & KEY_R) || ((keys & KEY_TOUCH) && kPokedexPageRects[1].contains(touchX,touchY));
            if(previous) m_dexSelected=m_dexSelected>=24 ? m_dexSelected-24 : 0;
            if(next) m_dexSelected=std::min(m_dexSelected+24,total-1);
            if(keys & KEY_TOUCH) {
                const int cell=pokedexCellAt(touchX,touchY);
                if(cell>=0) {
                    const unsigned ordinal=(m_dexSelected/kPokedexPageSize)*kPokedexPageSize+unsigned(cell);
                    if(ordinal<total) m_dexSelected=ordinal;
                }
            }
            return FrontendCommand::None;
        }
        if(m_page==FrontendPage::Title) {
            m_confirmingDelete = false;
            if((keys & KEY_X) || ((keys & KEY_TOUCH) && TouchRect{16,205,288,28}.contains(touchX,touchY))) {
                m_page=FrontendPage::GlobalMenu;m_selected=0;m_feedback=nullptr;
                return FrontendCommand::None;
            }
            if(keys & (KEY_DUP | KEY_CPAD_UP)) m_titleSelection.move(-1);
            if(keys & (KEY_DDOWN | KEY_CPAD_DOWN)) m_titleSelection.move(1);
            bool activatedByTouch = false;
            if(keys & KEY_TOUCH) {
                const int row = m_titleSelection.hit(touchX,touchY);
                if(row>=0) {
                    if(m_titleSelection.selected==unsigned(row)) activatedByTouch = true;
                    else m_titleSelection.selected=unsigned(row);
                }
            }
            if(!((keys & (KEY_A | KEY_START)) || activatedByTouch)) return FrontendCommand::None;
            switch(m_titleSelection.action()) {
            case TitleMenuAction::Continue:return FrontendCommand::Continue;
            case TitleMenuAction::NewGame:m_page=FrontendPage::Modes;break;
            case TitleMenuAction::LoadGame:m_page=FrontendPage::Load;break;
            case TitleMenuAction::RunHistory:m_page=FrontendPage::History;break;
            case TitleMenuAction::Settings:m_settingsFromGlobal=false;m_page=FrontendPage::Settings;break;
            }
            m_selected=0;m_feedback=nullptr;return FrontendCommand::None;
        }
        if(m_page==FrontendPage::Load) {
            if(m_confirmingDelete) {
                if(keys & KEY_A) {
                    m_confirmingDelete = false;
                    return FrontendCommand::DeleteSave;
                }
                if(keys & (KEY_B | KEY_X)) {
                    m_confirmingDelete = false;
                    return FrontendCommand::None;
                }
                if(keys & KEY_TOUCH) {
                    if(kConfirmationYesRect.contains(touchX,touchY)) {
                        m_confirmingDelete=false;return FrontendCommand::DeleteSave;
                    }
                    if(kConfirmationNoRect.contains(touchX,touchY)) m_confirmingDelete=false;
                }
                return FrontendCommand::None;
            }
            if(m_titleSelection.hasContinue) {
                if(keys & KEY_X) {
                    m_confirmingDelete = true;
                    return FrontendCommand::None;
                }
                if((keys & KEY_TOUCH) && kLoadActionRects[1].contains(touchX,touchY)) {
                    m_confirmingDelete = true;
                    return FrontendCommand::None;
                }
            }
        }
        if(m_page==FrontendPage::Load && (keys & KEY_TOUCH) && kLoadActionRects[0].contains(touchX,touchY))
            return FrontendCommand::Load;
        if(m_page==FrontendPage::SettingsGroup && m_group==1 && m_selected==1) {
            if(keys & (KEY_DLEFT | KEY_CPAD_LEFT)) return FrontendCommand::PreviousWindowStyle;
            if(keys & (KEY_DRIGHT | KEY_CPAD_RIGHT)) return FrontendCommand::NextWindowStyle;
        }
        if(m_page==FrontendPage::SettingsGroup && m_group==1 && m_selected==2) {
            if(keys & (KEY_DLEFT | KEY_CPAD_LEFT)) return FrontendCommand::PreviousHpBarSpeed;
            if(keys & (KEY_DRIGHT | KEY_CPAD_RIGHT)) return FrontendCommand::NextHpBarSpeed;
        }
        if(m_page==FrontendPage::SettingsGroup && m_group==1 && m_selected==3) {
            if(keys & (KEY_DLEFT | KEY_CPAD_LEFT)) return FrontendCommand::PreviousExpGainsSpeed;
            if(keys & (KEY_DRIGHT | KEY_CPAD_RIGHT)) return FrontendCommand::NextExpGainsSpeed;
        }
        if(m_page==FrontendPage::SettingsGroup && m_group==2 && (m_selected==0 || m_selected==4)) {
            if(keys & (KEY_DLEFT | KEY_CPAD_LEFT)) return m_selected==0 ? FrontendCommand::PreviousMasterVolume : FrontendCommand::PreviousUiVolume;
            if(keys & (KEY_DRIGHT | KEY_CPAD_RIGHT)) return m_selected==0 ? FrontendCommand::NextMasterVolume : FrontendCommand::NextUiVolume;
        }
        const unsigned total=rowCount();
        bool activatedByTouch = false;
        if(total) {
            if(keys & (KEY_DUP | KEY_CPAD_UP)) m_selected=(m_selected+total-1)%total;
            if(keys & (KEY_DDOWN | KEY_CPAD_DOWN)) m_selected=(m_selected+1)%total;
            if(keys & KEY_TOUCH) for(unsigned i=0;i<total;++i)
                if(rowRectangle(i).contains(touchX,touchY)) {
                    if(m_selected==i) activatedByTouch = true;
                    else m_selected=i;
                    break;
                }
        }
        if(!((keys & (KEY_A | KEY_START)) || activatedByTouch)) return FrontendCommand::None;
        switch(m_page) {
        case FrontendPage::GlobalMenu:
            m_globalSelection=m_selected;
            if(m_selected==0) {
                m_settingsFromGlobal=true;m_page=FrontendPage::Settings;m_selected=0;m_feedback=nullptr;
            } else if(m_selected==5) {m_page=FrontendPage::Pokedex;m_feedback=nullptr;}
            else if(m_selected==6) {m_page=FrontendPage::ManageData;m_selected=0;m_feedback=nullptr;}
            else {m_service=m_selected;m_page=FrontendPage::ServiceInfo;m_feedback=nullptr;m_eggSelected=0;m_eggDetails=false;}
            break;
        case FrontendPage::ManageData:
            if(m_selected==0) return FrontendCommand::ExportProgress;
            m_confirmingImport=true;m_importYes=false;return FrontendCommand::None;
        case FrontendPage::Modes:
            if(std::strcmp(kFrontendModes[m_selected].id,"classic")==0) return FrontendCommand::NewClassic;
            m_feedback="Runtime de este modo pendiente.";m_navigationRejected=true;break;
        case FrontendPage::Load:
            return FrontendCommand::Load; // Explicit retry can discover/recover an SD save.
        case FrontendPage::Settings:m_group=m_selected;m_page=FrontendPage::SettingsGroup;m_selected=0;break;
        case FrontendPage::SettingsGroup:
            if(m_group==1 && m_selected==1) return FrontendCommand::NextWindowStyle;
            if(m_group==1 && m_selected==2) return FrontendCommand::NextHpBarSpeed;
            if(m_group==1 && m_selected==3) return FrontendCommand::NextExpGainsSpeed;
            if(m_group==2 && (m_selected==0 || m_selected==4)) return m_selected==0 ? FrontendCommand::NextMasterVolume : FrontendCommand::NextUiVolume;
            if(m_group==3 && m_selected==0) {
                if(m_touchControls) m_confirmingTouchDisable=true;
                else return FrontendCommand::ToggleTouchControls;
                break;
            }
            m_feedback="Conexion con el runtime pendiente.";m_navigationRejected=true;break;
        default:break;
        }
        return FrontendCommand::None;
    }
public:
    void drawPokedexTop(Renderer2D& renderer,const FirstRunRuntime& game) const {
        if(m_page!=FrontendPage::Pokedex) return;
        renderer.clear(0xff3a303d);
        renderer.drawWindow(10,10,380,220);
        const auto* species=dexAt(m_dexSelected,&game);
        if(!species || !game.starterProfileReady()) {
            renderer.drawTextWrapped("Sin resultados para este filtro o perfil no disponible.",24,28,0.4f,352,0xffffffff);return;
        }
        const auto* progress=game.starterProgress(species->dex);
        const bool known=progress && (progress->caught || progress->observedFormAttr);
        char text[144];std::snprintf(text,sizeof(text),"#%u  %s",unsigned(species->dex),known ? species->name : "???");
        renderer.drawTextFitted(text,24,26,0.5f,352,0xffffffff);
        renderer.drawText(progress && progress->caught ? "Capturado" : known ? "Visto" : "Desconocido",24,54,0.4f,0xffffffff);
        if(!known) return;
        renderer.drawTypeLabel(species->type1,24,82,64,14);
        if(species->type2 && std::strcmp(species->type2,"NONE")) renderer.drawTypeLabel(species->type2,112,82,64,14);
        std::snprintf(text,sizeof(text),"PS %u   ATQ %u   DEF %u",unsigned(species->hp),unsigned(species->atk),unsigned(species->def));
        renderer.drawTextFitted(text,24,122,0.4f,352,0xffffffff);
        std::snprintf(text,sizeof(text),"AE %u   DE %u   VEL %u",unsigned(species->spatk),unsigned(species->spdef),unsigned(species->speed));
        renderer.drawTextFitted(text,24,150,0.4f,352,0xffffffff);
        std::snprintf(text,sizeof(text),"Generación %u   Total base %u",unsigned(species->generation),unsigned(species->baseTotal));
        renderer.drawTextFitted(text,24,186,0.4f,352,0xffffffff);
    }
    void draw(Renderer2D& renderer,const NativeRunSave* saved,const FirstRunRuntime* game=nullptr) {
        if(m_page!=FrontendPage::ServiceInfo || m_service!=3) m_eggTextures.clear(&renderer);
        if(m_page!=FrontendPage::Pokedex) {
            m_dexIcons.clear(&renderer); // Caller began a synchronized frame.
            if(m_dexVariants) C2D_SpriteSheetFree(m_dexVariants);
            m_dexVariants=nullptr;m_dexVariantsAttempted=false;
        }
        if(m_page==FrontendPage::Pokedex) {drawPokedex(renderer,game);return;}
        if(m_page==FrontendPage::ServiceInfo) {drawServiceInfo(renderer,game);return;}
        if(m_page==FrontendPage::Title) {m_title.draw(renderer,m_titleSelection,nullptr);return;}
        renderer.clear(0xff3a303d);
        const char* heading=m_page==FrontendPage::Modes ? runtimeUiText("menu:selectGameMode") :
            m_page==FrontendPage::Load ? runtimeUiText("menu:loadGame") :
            m_page==FrontendPage::History ? runtimeUiText("menu:runHistory") :
            m_page==FrontendPage::ManageData ? runtimeUiText("menu-ui-handler:manageData") : runtimeUiText("menu:settings");
        if(m_page!=FrontendPage::GlobalMenu) renderer.drawTextFitted(heading,12,8,0.45f,296,0xffffffff);
        renderer.drawWindow(16,m_page==FrontendPage::GlobalMenu ? 8 : 33,288,m_page==FrontendPage::GlobalMenu ? 196 : 168);
        if(m_confirmingImport) {
            drawBoundedDescription(renderer,"Importar reemplazará la partida y el perfil locales. ¿Continuar?",28,62,264,55);
            const TouchRect buttons[]={kConfirmationYesRect,kConfirmationNoRect};
            const char* const keys[]={"menu:yes","menu:no"};
            for(unsigned i=0;i<2;++i) {
                const auto& rect=buttons[i];
                const bool selected=m_importYes ? i==0 : i==1;
                if(selected) renderer.drawRect(rect.x-1,rect.y-1,rect.width+2,rect.height+2,0xff70d8f0);
                renderer.drawWindow(rect.x,rect.y,rect.width,rect.height);
                const float labelSize=renderer.drawTextFitted(runtimeUiText(keys[i]),rect.x+24,rect.y+9,
                    0.375f,rect.width-32,0xffffffff);
                if(selected) m_title.drawCursor(renderer,rect.x+7,rect.y+9,labelSize);
            }
        } else if(m_confirmingTouchDisable) {
            drawBoundedDescription(renderer,runtimeUiText("settings:confirmDisableTouch"),28,62,264,55);
            renderer.drawWindow(kConfirmationYesRect.x,kConfirmationYesRect.y,kConfirmationYesRect.width,kConfirmationYesRect.height);
            renderer.drawWindow(kConfirmationNoRect.x,kConfirmationNoRect.y,kConfirmationNoRect.width,kConfirmationNoRect.height);
            char yesLabel[96],noLabel[96];
            std::snprintf(yesLabel,sizeof(yesLabel),"A: %s",runtimeUiText("menu:yes"));
            std::snprintf(noLabel,sizeof(noLabel),"B: %s",runtimeUiText("menu:no"));
            renderer.drawTextFitted(yesLabel,kConfirmationYesRect.x+8,kConfirmationYesRect.y+6,
                0.4f,kConfirmationYesRect.width-16,0xffffffff);
            renderer.drawTextFitted(noLabel,kConfirmationNoRect.x+8,kConfirmationNoRect.y+6,
                0.4f,kConfirmationNoRect.width-16,0xffffffff);
        } else if(m_page==FrontendPage::History) {
            drawBoundedDescription(renderer,"Historial pendiente: todavía no se guardan los resúmenes de partidas finalizadas.",28,58,264,100);
        } else if(m_page==FrontendPage::Load) {
            if(!saved || !m_titleSelection.hasContinue) {
                renderer.drawTextFitted(runtimeUiText("menu:noSaves"),28,58,0.36f,264,0xffffffff);
                renderer.drawWindow(kLoadActionRects[0].x,kLoadActionRects[0].y,kLoadActionRects[0].width,kLoadActionRects[0].height);
                renderer.drawTextFitted("A: reintentar",kLoadActionRects[0].x+8,kLoadActionRects[0].y+9,0.34f,kLoadActionRects[0].width-16,0xffffffff);
            } else if(m_confirmingDelete) {
                renderer.drawWindow(28,55,264,115);
                renderer.drawTextFitted("¿Eliminar partida guardada?",44,70,0.40f,232,0xffff6060);
                renderer.drawTextFitted("Esta acción no se puede deshacer.",34,98,0.32f,252,0xffffffff);
                renderer.drawWindow(kConfirmationYesRect.x,kConfirmationYesRect.y,kConfirmationYesRect.width,kConfirmationYesRect.height);
                renderer.drawWindow(kConfirmationNoRect.x,kConfirmationNoRect.y,kConfirmationNoRect.width,kConfirmationNoRect.height);
                renderer.drawTextFitted("A: Sí, eliminar",kConfirmationYesRect.x+8,kConfirmationYesRect.y+6,0.34f,kConfirmationYesRect.width-16,0xffff6060);
                renderer.drawTextFitted("B: No, cancelar",kConfirmationNoRect.x+8,kConfirmationNoRect.y+6,0.34f,kConfirmationNoRect.width-16,0xffffffff);
            } else {
                char line[80];std::snprintf(line,sizeof(line),"Partida SD  -  Ola %u / 200",unsigned(saved->wave));
                renderer.drawTextFitted(line,43,43,0.43f,249,0xffffffff);
                if(saved->biomeId[0]) {
                    char biomeLine[80];std::snprintf(biomeLine,sizeof(biomeLine),"Bioma: %s",saved->biomeId);
                    renderer.drawTextFitted(biomeLine,43,68,0.35f,249,0xffffffff);
                }
                const unsigned partyCount = saved->playerPartyCount ? saved->playerPartyCount : 1;
                std::snprintf(line,sizeof(line),"Lider: Nivel %u   PS %u   (Equipo: %u/6)",
                    unsigned(saved->playerLevel),unsigned(saved->playerHp),partyCount);
                renderer.drawTextFitted(line,43,92,0.34f,249,0xffffffff);
                for(const auto& button:kLoadActionRects) renderer.drawWindow(button.x,button.y,button.width,button.height);
                renderer.drawTextFitted("A: cargar",kLoadActionRects[0].x+8,kLoadActionRects[0].y+9,0.34f,kLoadActionRects[0].width-16,0xffffffff);
                renderer.drawTextFitted("X: eliminar",kLoadActionRects[1].x+8,kLoadActionRects[1].y+9,0.34f,kLoadActionRects[1].width-16,0xffff6060);
                m_title.drawCursor(renderer,25,43,0.43f);
            }
        } else {
            for(unsigned i=0;i<rowCount();++i) {
                const float y=rowY(i);
                const char* label=m_page==FrontendPage::GlobalMenu ? runtimeUiText(globalMenuKeys()[i]) :
                    m_page==FrontendPage::ManageData ? (i==0 ? "Exportar progreso a SD" : "Importar progreso de SD") :
                    m_page==FrontendPage::Modes ? kFrontendModes[i].label :
                    m_page==FrontendPage::Settings ? runtimeUiText(groupKeys()[i]) : runtimeUiText(settingKey(m_group,i));
                const float labelSize=renderer.drawTextFitted(label,43,y,0.4f,m_page==FrontendPage::SettingsGroup ? 180 : 249,0xffffffff);
                if(i==m_selected) m_title.drawCursor(renderer,25,y,labelSize);
                if(m_page==FrontendPage::SettingsGroup) {
                    char value[16];
                    if(m_group==1 && i==1) std::snprintf(value,sizeof(value),"%u",renderer.windowStyle());
                    else if(m_group==1 && (i==2 || i==3)) {
                        static const char* keys[]={"settings:default","settings:fast","settings:faster","settings:skip"};
                        std::snprintf(value,sizeof(value),"%s",runtimeUiText(keys[i==2 ? m_hpBarSpeed : m_expGainsSpeed]));
                    }
                    else if(m_group==2 && (i==0 || i==4)) {
                        const unsigned volume=i==0 ? m_masterVolume : m_uiVolume;
                        if(volume) std::snprintf(value,sizeof(value),"%u",volume*10);
                        else std::snprintf(value,sizeof(value),"%s",runtimeUiText("settings:mute"));
                    }
                    else if(m_group==3 && i==0) std::snprintf(value,sizeof(value),"%s",runtimeUiText(m_touchControls ? "settings:on" : "settings:off"));
                    else std::snprintf(value,sizeof(value),"--");
                    renderer.drawTextFitted(value,238,y,0.3125f,54,0xffffffff);
                }
            }
        }
        {
            renderer.drawWindow(kFrontendConfirmRect.x,kFrontendConfirmRect.y,kFrontendConfirmRect.width,kFrontendConfirmRect.height);
            renderer.drawWindow(kFrontendBackRect.x,kFrontendBackRect.y,kFrontendBackRect.width,kFrontendBackRect.height);
            renderer.drawTextFitted("A: elegir",kFrontendConfirmRect.x+8,kFrontendConfirmRect.y+9,0.375f,kFrontendConfirmRect.width-16,0xffffffff);
            renderer.drawTextFitted("B: volver",kFrontendBackRect.x+8,kFrontendBackRect.y+9,0.375f,kFrontendBackRect.width-16,0xffffffff);
        }
    }
private:
    static bool drawBoundedDescription(Renderer2D& renderer,const char* text,float x,float y,float width,float height) {
        // Select complete native rasters; never downscale glyphs fractionally or
        // let a wrapped description overlap confirmation buttons/footer.
        for(const float size:{0.375f,0.3125f,0.25f}) {
            const float lineHeight=renderer.textLineHeight(size);
            if(!std::isfinite(lineHeight) || lineHeight<=0) continue;
            const unsigned lines=std::min(12u,unsigned(height/lineHeight));
            if(lines && renderer.drawTextBox(text,x,y,size,width,lines,0xffffffff)) return true;
        }
        return false;
    }
    // Drawing and input share the same resistive-touch button bounds.
    inline static constexpr TouchRect kLoadActionRects[]={{24,120,132,34},{164,120,132,34}};
    inline static constexpr TouchRect kConfirmationYesRect{32,125,120,30};
    inline static constexpr TouchRect kConfirmationNoRect{160,125,128,30};
    // These destinations expose missing integrations without inventing profile data
    // or pretending that network/account actions succeeded.
    void drawServiceInfo(Renderer2D& renderer,const FirstRunRuntime* game) {
        renderer.clear(0xff3a303d);
        renderer.drawTextFitted(runtimeUiText(globalMenuKeys()[m_service]),12,8,0.45f,296,0xffffffff);
        renderer.drawWindow(16,33,288,171);
        if(m_service==3) {
            if(!game || !game->eggInventoryReady()) {
                drawBoundedDescription(renderer,"Inventario no disponible para este perfil.",28,54,264,140);
            } else if(!game->eggInventoryCount()) {
                drawBoundedDescription(renderer,"No tienes huevos.",28,54,264,140);
            } else {
                const size_t count=game->eggInventoryCount();
                const size_t selected=m_eggSelected<count ? m_eggSelected : 0;
                if(m_eggDetails) {
                    const auto* egg=game->eggAt(selected);
                    if(egg) {
                        char label[64];std::snprintf(label,sizeof(label),"%s %u / %u",eggUiText("egg"),unsigned(selected+1),unsigned(count));
                        char textureKey[24];eggTextureKey(*egg,textureKey,sizeof(textureKey),true);
                        if(!m_eggTextures.draw(renderer,"egg",textureKey,28,45))
                            renderer.drawTextFitted("?",28,46,0.375f,28,0xffffffff);
                        renderer.drawTextFitted(label,68,46,0.375f,224,0xffffffff);
                        renderer.drawTextFitted(eggTierText(*egg),68,70,0.375f,224,0xff80ffff);
                        drawBoundedDescription(renderer,eggUiText(eggHatchMessageKey(egg->hatchWaves)),28,100,264,90);
                    }
                } else {
                    const size_t first=(selected/5)*5;
                    for(size_t row=0;row<5 && first+row<count;++row) {
                        const auto* egg=game->eggAt(first+row);
                        if(!egg) continue;
                        char label[96];std::snprintf(label,sizeof(label),"%s %u: %s",eggUiText("egg"),unsigned(first+row+1),eggTierText(*egg));
                        const float y=42+row*30;
                        renderer.drawTextFitted(first+row==selected ? ">" : "",24,y,0.375f,12,0xffffffff);
                        char textureKey[24];eggTextureKey(*egg,textureKey,sizeof(textureKey),false);
                        if(!m_eggTextures.draw(renderer,"egg_icons",textureKey,32,y-10))
                            renderer.drawTextFitted("?",46,y,0.375f,20,0xffffffff);
                        renderer.drawTextFitted(label,76,y,0.375f,212,0xffffffff);
                    }
                }
            }
            if(!m_eggDetails) {
                renderer.drawWindow(kEggListConfirmRect.x,kEggListConfirmRect.y,kEggListConfirmRect.width,kEggListConfirmRect.height);
                renderer.drawTextFitted("A: Detalles",kEggListConfirmRect.x+8,kEggListConfirmRect.y+9,0.375f,kEggListConfirmRect.width-16,0xffffffff);
            }
            renderer.drawWindow(kEggListBackRect.x,kEggListBackRect.y,kEggListBackRect.width,kEggListBackRect.height);
            renderer.drawTextFitted(m_eggDetails ? "B: Lista" : "B: Volver",kEggListBackRect.x+8,kEggListBackRect.y+9,0.375f,kEggListBackRect.width-16,0xffffffff);
            return;
        }
        if(m_service==2) {
            FirstRunRuntime::ProfileCatalogStats stats;
            if(!game || !game->profileCatalogStats(stats)) {
                drawBoundedDescription(renderer,"Perfil no disponible.",28,54,264,140);
            } else {
                const char* keys[]={"game-stats-ui-handler:starters","game-stats-ui-handler:shinyStarters",
                    "game-stats-ui-handler:speciesSeen","game-stats-ui-handler:speciesCaught"};
                const unsigned values[]={stats.startersCaught,stats.shinyStartersCaught,stats.speciesSeen,stats.speciesCaught};
                const unsigned totals[]={stats.startersTotal,stats.startersTotal,stats.speciesTotal,stats.speciesTotal};
                for(unsigned row=0;row<4;++row) {
                    const float y=46+row*31;
                    renderer.drawTextFitted(runtimeUiText(keys[row]),28,y,0.3125f,264,0xffffffff);
                    char value[48];std::snprintf(value,sizeof(value),"%u / %u",values[row],totals[row]);
                    renderer.drawTextFitted(value,28,y+13,0.3125f,264,0xff80ffff);
                }
                renderer.drawTextFitted("Historial de partidas pendiente.",28,178,0.25f,264,0xffffffff);
            }
            renderer.drawTextFitted("B: volver al menú",12,214,0.3125f,296,0xffffffff);
            return;
        }
        const char* description=nullptr;
        switch(m_service) {
        case 1:description="Logros pendientes: falta conectar las condiciones y recompensas al perfil persistente.";break;
        case 3:description="Inventario de huevos persistente conectado. Lista visual, incubación y eclosión pendientes.";break;
        case 4:description="Gacha pendiente: falta conectar vales, máquinas, probabilidades y resultados al perfil.";break;
        case 7:description="Comunidad requiere enlaces externos. Este runtime todavía no dispone de un servicio conectado para abrirlos.";break;
        case 8:description="No hay una sesión web conectada que cerrar. El progreso local de la consola se conserva.";break;
        default:description="Integración pendiente.";break;
        }
        drawBoundedDescription(renderer,description,28,54,264,140);
        renderer.drawTextFitted("B: volver al menú",12,214,0.3125f,296,0xffffffff);
    }
    static bool specialEggTexture(const EggIncubationRecord& egg) {
        for(const auto dex:kSpecialEggIncubationSpecies)
            if(egg.speciesDex==dex) return true;
        return !egg.speciesDex && egg.tier==EggTier::COMMON && egg.id%kEggSpecialIdDivisor==0;
    }
    static void eggTextureKey(const EggIncubationRecord& egg,char* output,size_t capacity,bool detail) {
        if(specialEggTexture(egg)) std::snprintf(output,capacity,"%smanaphy",detail ? "egg_" : "");
        else std::snprintf(output,capacity,"%s%u",detail ? "egg_" : "",unsigned(egg.tier));
    }
    static const char* eggTierText(const EggIncubationRecord& egg) {
        if(specialEggTexture(egg)) return eggUiText("manaphyTier");
        static const char* keys[]={"defaultTier","greatTier","ultraTier","masterTier"};
        const unsigned tier=static_cast<unsigned>(egg.tier);
        return tier<4 ? eggUiText(keys[tier]) : "?";
    }
    bool dexMatches(const PokerogueContent::Species& species,const FirstRunRuntime* game) const {
        if(m_dexGeneration && species.generation!=m_dexGeneration) return false;
        if(!m_dexCapture) return true;
        if(!game || !game->starterProfileReady()) return false;
        const auto* progress=game->starterProgress(species.dex);
        const bool caught=progress && progress->caught;
        const bool seen=progress && (progress->observedFormAttr || caught);
        return m_dexCapture==1 ? caught : m_dexCapture==2 ? seen && !caught : !seen;
    }
    unsigned dexCount(const FirstRunRuntime* game) const {
        unsigned count=0;for(const auto& species:PokerogueContent::kSpecies) if(dexMatches(species,game)) ++count;return count;
    }
    const PokerogueContent::Species* dexAt(unsigned ordinal,const FirstRunRuntime* game) const {
        for(const auto& species:PokerogueContent::kSpecies) {
            if(!dexMatches(species,game)) continue;
            if(!ordinal) return &species;
            --ordinal;
        }
        return nullptr;
    }
    void drawDexFilters(Renderer2D& renderer) const {
        char text[48];
        if(m_dexGeneration) std::snprintf(text,sizeof(text),"X: Gen. %u",m_dexGeneration);
        else std::snprintf(text,sizeof(text),"X: Todas las gen.");
        renderer.drawTextFitted(text,kPokedexFilterRects[0].x+2,kPokedexFilterRects[0].y,0.3125f,kPokedexFilterRects[0].width-4,0xffffffff);
        const char* labels[]={"Todos","Capturados","Vistos","Desconocidos"};
        std::snprintf(text,sizeof(text),"Y: %s",labels[m_dexCapture]);
        renderer.drawTextFitted(text,kPokedexFilterRects[1].x+2,kPokedexFilterRects[1].y,0.3125f,kPokedexFilterRects[1].width-4,0xffffffff);
    }
    void drawPokedex(Renderer2D& renderer,const FirstRunRuntime* game) {
        renderer.clear(0xff3a303d);
        renderer.drawWindow(8,38,304,149);
        if(!game || !game->starterProfileReady()) {
            renderer.drawTextWrapped("Perfil no disponible. No se puede resolver el progreso.",16,60,0.4f,288,0xffffffff);
        } else {
            const unsigned total=dexCount(game);
            drawDexFilters(renderer);
            if(!total) {renderer.drawTextFitted("Sin resultados",16,80,0.4f,288,0xffffffff);renderer.drawTextFitted("X: generación   Y: captura   B: volver",12,214,0.3125f,296,0xffffffff);return;}
            if(m_dexSelected>=total) m_dexSelected=total-1;
            const unsigned start=(m_dexSelected/24)*24;
            const auto page=catalogSpeciesPage<24>(start,[&](const auto& species) {return dexMatches(species,game);});
            const auto* selectedRow=page[m_dexSelected-start];
            if(!selectedRow) return; // The same immutable catalogue/filter must supply this row.
            const auto& selected=*selectedRow;
            const auto* progress=game->starterProgress(selected.dex);
            const bool known=progress && (progress->caught || progress->observedFormAttr);
            char header[128];std::snprintf(header,sizeof(header),"#%u  %s",unsigned(selected.dex),known ? selected.name : "???");
            renderer.drawTextFitted(header,12,8,0.4f,296,0xffffffff);

            const AppearanceIconIdentity* icons[24]{};
            unsigned visible=0;
            for(;visible<page.size() && page[visible];++visible) {
                const auto& species=*page[visible];
                const auto* record=game->starterProgress(species.dex);
                bool shiny=false;uint8_t variant=0;PokemonGender gender=PokemonGender::Male;
                const bool caughtAppearance=record && record->caught && nativeStarterDefaultAppearance(*record,shiny,variant);
                if(record && record->caught && record->genderAttr) nativeStarterDefaultGender(*record,gender);
                icons[visible]=findAppearanceIconIdentity(species.dex,0,caughtAppearance && gender==PokemonGender::Female,
                    caughtAppearance && shiny,caughtAppearance ? variant : 0);
            }
            m_dexIcons.prepareAppearances(renderer,icons,visible);
            for(unsigned cell=0;cell<page.size() && page[cell];++cell) {
                const auto& species=*page[cell];
                const auto* record=game->starterProgress(species.dex);
                const auto bounds=pokedexCellRectangle(cell);
                const float x=bounds.x,y=bounds.y;
                if(start+cell==m_dexSelected) renderer.drawWindow(x,y,bounds.width,bounds.height);
                const uint32_t tint=starterDiscoveryTint(starterDiscovery(record && record->caught,record ? record->observedFormAttr : 0));
                if(!m_dexIcons.drawAppearance(renderer,icons[cell],x+4,y+2,1.0f,1,tint)) renderer.drawText("?",x+18,y+8,0.4f,0xffffffff);
                if(record) drawDexVariants(renderer,nativeCaughtShinyVariants(*record),x+1,y+20);
            }
            char position[32];std::snprintf(position,sizeof(position),"%u / %u",m_dexSelected+1,total);
            renderer.drawTextFitted(position,110,190,0.3125f,100,0xffffffff);
        }
        renderer.drawTextFitted("L: anterior",kPokedexPageRects[0].x+2,kPokedexPageRects[0].y+2,0.3125f,kPokedexPageRects[0].width-4,0xffffffff);
        renderer.drawTextFitted("R: siguiente",kPokedexPageRects[1].x+2,kPokedexPageRects[1].y+2,0.3125f,kPokedexPageRects[1].width-4,0xffffffff);
        renderer.drawTextFitted("X: generación  Y: captura  B: volver",12,214,0.3125f,296,0xffffffff);
    }
    void drawDexVariants(Renderer2D& renderer,uint8_t mask,float x,float y) {
        if(!mask) return;
        if(!m_dexVariantsAttempted) {
            m_dexVariantsAttempted=true;
            m_dexVariants=C2D_SpriteSheetLoad(kStarterVariantIconPath);
            if(m_dexVariants) {
                const auto image=C2D_SpriteSheetGetImage(m_dexVariants,0);
                if(image.tex) C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST);
            }
        }
        if(!m_dexVariants) return;
        unsigned slot=0;
        for(unsigned variant=0;variant<3;++variant) if(mask & (1u<<variant)) {
            const auto& frame=kStarterVariantIconFrames[variant];
            renderer.drawAtlasFrame(C2D_SpriteSheetGetImage(m_dexVariants,0),frame,x+slot*15,y,
                frame.sourceWidth,frame.sourceHeight,1.0f,kStarterVariantIconTints[variant]);
            ++slot;
        }
    }
    TouchRect rowRectangle(unsigned index) const {
        return m_page==FrontendPage::GlobalMenu ? globalMenuRowRectangle(index) : TouchRect{24,43+index*29,272,29};
    }
    unsigned rowY(unsigned index) const {return rowRectangle(index).y;}
    unsigned rowHeight() const {return m_page==FrontendPage::GlobalMenu ? 20 : 29;}
    static const char* const* globalMenuKeys() {
        static const char* keys[]={"menu-ui-handler:gameSettings","menu-ui-handler:achievements",
            "menu-ui-handler:stats","menu-ui-handler:eggList","menu-ui-handler:eggGacha",
            "menu-ui-handler:pokedex","menu-ui-handler:manageData","menu-ui-handler:community",
            "menu-ui-handler:logOut"};
        return keys;
    }
    unsigned rowCount() const {
        switch(m_page) {
        case FrontendPage::ManageData:return 2;
        case FrontendPage::GlobalMenu:return kGlobalMenuRowCount;
        case FrontendPage::Modes:return sizeof(kFrontendModes)/sizeof(kFrontendModes[0]);
        case FrontendPage::Load:return m_titleSelection.hasContinue ? 1 : 0;
        case FrontendPage::Settings:return 4;
        case FrontendPage::SettingsGroup:return m_group==2 ? 5 : m_group==3 ? 1 : 4;
        default:return 0;
        }
    }
    static const char* const* groupKeys() {
        static const char* keys[]={"settings:general","settings:display","settings:audio","settings:gamepad"};return keys;
    }
    static const char* settingKey(unsigned group,unsigned row) {
        static const char* general[]={"settings:gameSpeed","settings:battleStyle","settings:enableRetries","settings:tutorials"};
        static const char* display[]={"settings:language","settings:windowType","settings:hpBarSpeed","settings:expGainsSpeed"};
        static const char* audio[]={"settings:masterVolume","settings:bgmVolume","settings:fieldVolume","settings:seVolume","settings:uiVolume"};
        return group==0 ? general[row] : group==1 ? display[row] : group==2 ? audio[row] : "settings:touchControls";
    }
    PokemonIconPresenter m_dexIcons{true,24,true};
    C2D_SpriteSheet m_dexVariants=nullptr;
    bool m_dexVariantsAttempted=false;
    unsigned m_hpBarSpeed=0,m_expGainsSpeed=0;
    unsigned m_masterVolume=kDefaultMasterVolume,m_uiVolume=kDefaultUiVolume;
    unsigned m_dexSelected=0,m_dexGeneration=0,m_dexCapture=0;
    bool m_confirmingImport=false,m_importYes=false;
    TitleMenuPresenter m_title;
    TitleMenuSelection m_titleSelection;
    FrontendPage m_page=FrontendPage::Title;
    EggTexturePresenter m_eggTextures;
    size_t m_eggSelected=0;
    bool m_eggDetails=false;
    unsigned m_selected=0,m_group=0,m_service=0,m_globalSelection=0;
    bool m_confirmingDelete=false,m_touchControls=true,m_confirmingTouchDisable=false,m_settingsFromGlobal=false;
    const char* m_feedback=nullptr;
    const char* m_audioInitializationError=nullptr;
    const char* m_navigationSound=nullptr;
    bool m_navigationRejected=false;
};
}

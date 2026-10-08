#pragma once
#include "runtime/TitleMenuPresenter.hpp"
#include "content/FrontendModes.hpp"
#include "content/RuntimeUiText.hpp"
#include "storage/NativeRunSave.hpp"
#include "game/FirstRunRuntime.hpp"
#include "runtime/PokemonIconPresenter.hpp"
#include "runtime/StarterGridLayout.hpp"
#include <3ds.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
namespace Pokerogue3DS {
enum class FrontendPage {Title,Modes,Load,History,Settings,SettingsGroup,GlobalMenu,Pokedex,ServiceInfo,ManageData};
enum class FrontendCommand {None,Continue,NewClassic,Load,DeleteSave,NextWindowStyle,PreviousWindowStyle,ToggleTouchControls,ExportProgress,ImportProgress,NextHpBarSpeed,PreviousHpBarSpeed};
// Owns navigation and presentation only. Returned commands are handled by main.
class FrontendMenuPresenter {
public:
    explicit FrontendMenuPresenter(bool hasSave):m_titleSelection{hasSave,0} {}
    void clear(Renderer2D* renderer=nullptr) {m_title.clear(renderer);m_dexIcons.clear(renderer); m_confirmingDelete=false;m_confirmingImport=false;}
    void setHpBarSpeed(unsigned speed) {if(speed<=3) m_hpBarSpeed=speed;}
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
    bool isConfirmingDelete() const {return m_confirmingDelete;}
    bool isConfirmingImport() const {return m_confirmingImport;}
    void setHasSave(bool hasSave) {
        m_titleSelection.hasContinue=hasSave;
        m_titleSelection.selected=0;
        if(!hasSave) m_confirmingDelete=false;
    }
    void drawCursor(Renderer2D& renderer, float x, float y, float size) {
        m_title.drawCursor(renderer, x, y, size);
    }
    FrontendCommand input(uint32_t keys,unsigned touchX=0,unsigned touchY=0,const FirstRunRuntime* game=nullptr) {
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
        if((keys & KEY_B) || ((keys & KEY_TOUCH) && TouchRect{12,205,296,35}.contains(touchX,touchY) && m_page != FrontendPage::Title)) {
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
        if(m_page==FrontendPage::Pokedex) {
            if((keys & KEY_X) || ((keys & KEY_TOUCH) && TouchRect{10,24,146,14}.contains(touchX,touchY))) {
                unsigned next=0;
                for(const auto& species:PokerogueContent::kSpecies)
                    if(species.generation>m_dexGeneration && (!next || species.generation<next)) next=species.generation;
                m_dexGeneration=next;m_dexSelected=0;
            }
            if((keys & KEY_Y) || ((keys & KEY_TOUCH) && TouchRect{164,24,146,14}.contains(touchX,touchY))) {
                m_dexCapture=(m_dexCapture+1)%4;m_dexSelected=0;
            }
            const unsigned total=dexCount(game);
            if(m_dexSelected>=total) m_dexSelected=0;
            if(!total) return FrontendCommand::None;
            if(keys & (KEY_DLEFT | KEY_CPAD_LEFT)) m_dexSelected=(m_dexSelected+total-1)%total;
            if(keys & (KEY_DRIGHT | KEY_CPAD_RIGHT)) m_dexSelected=(m_dexSelected+1)%total;
            if(keys & (KEY_DUP | KEY_CPAD_UP)) m_dexSelected=m_dexSelected>=6 ? m_dexSelected-6 : 0;
            if(keys & (KEY_DDOWN | KEY_CPAD_DOWN)) m_dexSelected=std::min(m_dexSelected+6,total-1);
            const bool previous=(keys & KEY_L) || ((keys & KEY_TOUCH) && TouchRect{10,188,96,16}.contains(touchX,touchY));
            const bool next=(keys & KEY_R) || ((keys & KEY_TOUCH) && TouchRect{214,188,96,16}.contains(touchX,touchY));
            if(previous) m_dexSelected=m_dexSelected>=24 ? m_dexSelected-24 : 0;
            if(next) m_dexSelected=std::min(m_dexSelected+24,total-1);
            if(keys & KEY_TOUCH) for(unsigned cell=0;cell<24;++cell) {
                const unsigned ordinal=(m_dexSelected/24)*24+cell;
                if(ordinal<total && TouchRect{10+(cell%6)*50,43+(cell/6)*36,48,34}.contains(touchX,touchY)) {m_dexSelected=ordinal;break;}
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
                    if(touchY >= 125 && touchY <= 155) {
                        if(touchX >= 24 && touchX <= 150) {
                            m_confirmingDelete = false;
                            return FrontendCommand::DeleteSave;
                        } else if(touchX >= 160 && touchX <= 290) {
                            m_confirmingDelete = false;
                            return FrontendCommand::None;
                        }
                    }
                }
                return FrontendCommand::None;
            }
            if(m_titleSelection.hasContinue) {
                if(keys & KEY_X) {
                    m_confirmingDelete = true;
                    return FrontendCommand::None;
                }
                if((keys & KEY_TOUCH) && touchY >= 115 && touchY <= 145 && touchX >= 110 && touchX <= 200) {
                    m_confirmingDelete = true;
                    return FrontendCommand::None;
                }
            }
        }
        if(m_page==FrontendPage::SettingsGroup && m_group==1 && m_selected==1) {
            if(keys & (KEY_DLEFT | KEY_CPAD_LEFT)) return FrontendCommand::PreviousWindowStyle;
            if(keys & (KEY_DRIGHT | KEY_CPAD_RIGHT)) return FrontendCommand::NextWindowStyle;
        }
        if(m_page==FrontendPage::SettingsGroup && m_group==1 && m_selected==2) {
            if(keys & (KEY_DLEFT | KEY_CPAD_LEFT)) return FrontendCommand::PreviousHpBarSpeed;
            if(keys & (KEY_DRIGHT | KEY_CPAD_RIGHT)) return FrontendCommand::NextHpBarSpeed;
        }
        const unsigned total=rowCount();
        bool activatedByTouch = false;
        if(total) {
            if(keys & (KEY_DUP | KEY_CPAD_UP)) m_selected=(m_selected+total-1)%total;
            if(keys & (KEY_DDOWN | KEY_CPAD_DOWN)) m_selected=(m_selected+1)%total;
            if(keys & KEY_TOUCH) for(unsigned i=0;i<total;++i)
                if(TouchRect{24,rowY(i),272,rowHeight()}.contains(touchX,touchY)) {
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
            else {m_service=m_selected;m_page=FrontendPage::ServiceInfo;m_feedback=nullptr;}
            break;
        case FrontendPage::ManageData:
            if(m_selected==0) return FrontendCommand::ExportProgress;
            m_confirmingImport=true;m_importYes=false;return FrontendCommand::None;
        case FrontendPage::Modes:
            if(std::strcmp(kFrontendModes[m_selected].id,"classic")==0) return FrontendCommand::NewClassic;
            m_feedback="Runtime de este modo pendiente.";break;
        case FrontendPage::Load:
            return FrontendCommand::Load; // Explicit retry can discover/recover an SD save.
        case FrontendPage::Settings:m_group=m_selected;m_page=FrontendPage::SettingsGroup;m_selected=0;break;
        case FrontendPage::SettingsGroup:
            if(m_group==1 && m_selected==1) return FrontendCommand::NextWindowStyle;
            if(m_group==1 && m_selected==2) return FrontendCommand::NextHpBarSpeed;
            if(m_group==3 && m_selected==0) {
                if(m_touchControls) m_confirmingTouchDisable=true;
                else return FrontendCommand::ToggleTouchControls;
                break;
            }
            m_feedback="Conexion con el runtime pendiente.";break;
        default:break;
        }
        return FrontendCommand::None;
    }
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
        if(m_page!=FrontendPage::Pokedex) m_dexIcons.clear(); // Caller began a synchronized frame.
        if(m_page==FrontendPage::Pokedex) {drawPokedex(renderer,game);return;}
        if(m_page==FrontendPage::ServiceInfo) {drawServiceInfo(renderer,game);return;}
        if(m_page==FrontendPage::Title) {m_title.draw(renderer,m_titleSelection,m_feedback);return;}
        renderer.clear(0xff3a303d);
        const char* heading=m_page==FrontendPage::Modes ? runtimeUiText("menu:selectGameMode") :
            m_page==FrontendPage::Load ? runtimeUiText("menu:loadGame") :
            m_page==FrontendPage::History ? runtimeUiText("menu:runHistory") :
            m_page==FrontendPage::ManageData ? runtimeUiText("menu-ui-handler:manageData") : runtimeUiText("menu:settings");
        if(m_page!=FrontendPage::GlobalMenu) renderer.drawTextFitted(heading,12,8,0.45f,296,0xffffffff);
        renderer.drawWindow(16,m_page==FrontendPage::GlobalMenu ? 8 : 33,288,m_page==FrontendPage::GlobalMenu ? 196 : 168);
        if(m_confirmingImport) {
            drawBoundedDescription(renderer,"Importar reemplazará la partida y el perfil locales. ¿Continuar?",28,62,264,55);
            renderer.drawWindow(kConfirmationYesRect.x,kConfirmationYesRect.y,kConfirmationYesRect.width,kConfirmationYesRect.height);
            renderer.drawWindow(kConfirmationNoRect.x,kConfirmationNoRect.y,kConfirmationNoRect.width,kConfirmationNoRect.height);
            renderer.drawTextFitted("Sí",56,134,0.375f,88,0xffffffff);
            renderer.drawTextFitted("No",184,134,0.375f,96,0xffffffff);
            m_title.drawCursor(renderer,m_importYes ? 39 : 167,134,0.375f);
        } else if(m_confirmingTouchDisable) {
            drawBoundedDescription(renderer,runtimeUiText("settings:confirmDisableTouch"),28,62,264,55);
            renderer.drawTextFitted("A: Sí",32,130,0.4f,120,0xffffffff);
            renderer.drawTextFitted("B: No",160,130,0.4f,128,0xffffffff);
        } else if(m_page==FrontendPage::History) {
            drawBoundedDescription(renderer,"Historial pendiente: todavía no se guardan los resúmenes de partidas finalizadas.",28,58,264,100);
        } else if(m_page==FrontendPage::Load) {
            if(!saved || !m_titleSelection.hasContinue) {
                renderer.drawTextFitted(runtimeUiText("menu:noSaves"),28,58,0.36f,264,0xffffffff);
                renderer.drawTextFitted("A: reintentar lectura   B: volver",28,104,0.32f,264,0xff80ffff);
            } else if(m_confirmingDelete) {
                renderer.drawWindow(28,55,264,115);
                renderer.drawTextFitted("¿Eliminar partida guardada?",44,70,0.40f,232,0xffff6060);
                renderer.drawTextFitted("Esta acción no se puede deshacer.",34,98,0.32f,252,0xffffffff);
                renderer.drawTextFitted("A: Sí (eliminar)   B: No (cancelar)",32,134,0.34f,256,0xff80ffff);
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
                renderer.drawTextFitted("A: cargar partida   X: eliminar   B: volver",26,126,0.34f,272,0xff80ffff);
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
                    else if(m_group==1 && i==2) {
                        static const char* keys[]={"settings:default","settings:fast","settings:faster","settings:skip"};
                        std::snprintf(value,sizeof(value),"%s",runtimeUiText(keys[m_hpBarSpeed]));
                    }
                    else if(m_group==3 && i==0) std::snprintf(value,sizeof(value),"%s",runtimeUiText(m_touchControls ? "settings:on" : "settings:off"));
                    else std::snprintf(value,sizeof(value),"--");
                    renderer.drawTextFitted(value,238,y,0.3125f,54,0xffffffff);
                }
            }
        }
        renderer.drawTextFitted(m_feedback ? m_feedback : "A: elegir   B: volver",12,214,0.3f,296,0xffffffff);
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
    inline static constexpr TouchRect kConfirmationYesRect{32,125,120,30};
    inline static constexpr TouchRect kConfirmationNoRect{160,125,128,30};
    // These destinations expose missing integrations without inventing profile data
    // or pretending that network/account actions succeeded.
    void drawServiceInfo(Renderer2D& renderer,const FirstRunRuntime* game) const {
        renderer.clear(0xff3a303d);
        renderer.drawTextFitted(runtimeUiText(globalMenuKeys()[m_service]),12,8,0.45f,296,0xffffffff);
        renderer.drawWindow(16,33,288,171);
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
        case 3:description="Lista de huevos pendiente: falta el inventario persistente, la incubación y la eclosión.";break;
        case 4:description="Gacha pendiente: falta conectar vales, máquinas, probabilidades y resultados al perfil.";break;
        case 7:description="Comunidad requiere enlaces externos. Este runtime todavía no dispone de un servicio conectado para abrirlos.";break;
        case 8:description="No hay una sesión web conectada que cerrar. El progreso local de la consola se conserva.";break;
        default:description="Integración pendiente.";break;
        }
        drawBoundedDescription(renderer,description,28,54,264,140);
        renderer.drawTextFitted("B: volver al menú",12,214,0.3125f,296,0xffffffff);
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
        for(const auto& species:PokerogueContent::kSpecies) if(dexMatches(species,game)) {if(!ordinal) return &species;--ordinal;}return nullptr;
    }
    void drawDexFilters(Renderer2D& renderer) const {
        char text[48];
        if(m_dexGeneration) std::snprintf(text,sizeof(text),"X: Gen. %u",m_dexGeneration);
        else std::snprintf(text,sizeof(text),"X: Todas las gen.");
        renderer.drawTextFitted(text,12,24,0.3125f,142,0xffffffff);
        const char* labels[]={"Todos","Capturados","Vistos","Desconocidos"};
        std::snprintf(text,sizeof(text),"Y: %s",labels[m_dexCapture]);
        renderer.drawTextFitted(text,166,24,0.3125f,142,0xffffffff);
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

            for(unsigned cell=0;cell<page.size() && page[cell];++cell) {
                const auto& species=*page[cell];
                const auto* record=game->starterProgress(species.dex);
                const float x=10+(cell%6)*50,y=43+(cell/6)*36;
                if(start+cell==m_dexSelected) renderer.drawWindow(x,y,48,34);
                const uint32_t tint=starterDiscoveryTint(starterDiscovery(record && record->caught,record ? record->observedFormAttr : 0));
                if(!m_dexIcons.draw(renderer,species.dex,0,x+4,y+2,1.0f,1.0f,tint)) renderer.drawText("?",x+18,y+8,0.4f,0xffffffff);
            }
            char position[32];std::snprintf(position,sizeof(position),"%u / %u",m_dexSelected+1,total);
            renderer.drawTextFitted(position,110,190,0.3125f,100,0xffffffff);
        }
        renderer.drawText("L: anterior",12,190,0.3125f,0xffffffff);
        renderer.drawText("R: siguiente",216,190,0.3125f,0xffffffff);
        renderer.drawTextFitted("X: generación  Y: captura  B: volver",12,214,0.3125f,296,0xffffffff);
    }
    unsigned rowY(unsigned index) const {return (m_page==FrontendPage::GlobalMenu ? 17 : 43)+index*rowHeight();}
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
        case FrontendPage::GlobalMenu:return 9;
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
        static const char* display[]={"settings:language","settings:windowType","settings:hpBarSpeed","settings:expPartyDisplay"};
        static const char* audio[]={"settings:masterVolume","settings:bgmVolume","settings:fieldVolume","settings:seVolume","settings:uiVolume"};
        return group==0 ? general[row] : group==1 ? display[row] : group==2 ? audio[row] : "settings:touchControls";
    }
    PokemonIconPresenter m_dexIcons;
    unsigned m_hpBarSpeed=0;
    unsigned m_dexSelected=0,m_dexGeneration=0,m_dexCapture=0;
    bool m_confirmingImport=false,m_importYes=false;
    TitleMenuPresenter m_title;
    TitleMenuSelection m_titleSelection;
    FrontendPage m_page=FrontendPage::Title;
    unsigned m_selected=0,m_group=0,m_service=0,m_globalSelection=0;
    bool m_confirmingDelete=false,m_touchControls=true,m_confirmingTouchDisable=false,m_settingsFromGlobal=false;
    const char* m_feedback=nullptr;
};
}

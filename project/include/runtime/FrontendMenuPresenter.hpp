#pragma once
#include "runtime/TitleMenuPresenter.hpp"
#include "content/FrontendModes.hpp"
#include "content/RuntimeUiText.hpp"
#include "storage/NativeRunSave.hpp"
#include <3ds.h>
#include <cstdio>
#include <cstring>
namespace Pokerogue3DS {
enum class FrontendPage {Title,Modes,Load,History,Settings,SettingsGroup};
enum class FrontendCommand {None,Continue,NewClassic,Load,DeleteSave};
// Owns navigation and presentation only. Returned commands are handled by main.
class FrontendMenuPresenter {
public:
    explicit FrontendMenuPresenter(bool hasSave):m_titleSelection{hasSave,0} {}
    void clear() {m_title.clear(); m_confirmingDelete=false;}
    FrontendPage page() const {return m_page;}
    void feedback(const char* value) {m_feedback=value;}
    bool isConfirmingDelete() const {return m_confirmingDelete;}
    void setHasSave(bool hasSave) {
        m_titleSelection.hasContinue=hasSave;
        m_titleSelection.selected=0;
        if(!hasSave) m_confirmingDelete=false;
    }
    void drawCursor(Renderer2D& renderer, float x, float y, float size) {
        m_title.drawCursor(renderer, x, y, size);
    }
    FrontendCommand input(uint32_t keys,unsigned touchX=0,unsigned touchY=0) {
        if((keys & KEY_B) || ((keys & KEY_TOUCH) && touchY >= 205 && m_page != FrontendPage::Title)) {
            if(m_confirmingDelete) {
                m_confirmingDelete = false;
                return FrontendCommand::None;
            }
            if(m_page==FrontendPage::SettingsGroup) m_page=FrontendPage::Settings;
            else m_page=FrontendPage::Title;
            m_selected=0;m_feedback=nullptr;return FrontendCommand::None;
        }
        if(m_page==FrontendPage::Title) {
            m_confirmingDelete = false;
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
            case TitleMenuAction::Settings:m_page=FrontendPage::Settings;break;
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
        const unsigned total=rowCount();
        bool activatedByTouch = false;
        if(total) {
            if(keys & (KEY_DUP | KEY_CPAD_UP)) m_selected=(m_selected+total-1)%total;
            if(keys & (KEY_DDOWN | KEY_CPAD_DOWN)) m_selected=(m_selected+1)%total;
            if(keys & KEY_TOUCH) for(unsigned i=0;i<total;++i)
                if(TouchRect{24,43+i*29,272,29}.contains(touchX,touchY)) {
                    if(m_selected==i) activatedByTouch = true;
                    else m_selected=i;
                    break;
                }
        }
        if(!((keys & (KEY_A | KEY_START)) || activatedByTouch)) return FrontendCommand::None;
        switch(m_page) {
        case FrontendPage::Modes:
            if(std::strcmp(kFrontendModes[m_selected].id,"classic")==0) return FrontendCommand::NewClassic;
            m_feedback="Runtime de este modo pendiente.";break;
        case FrontendPage::Load:
            if(m_titleSelection.hasContinue) return FrontendCommand::Load;
            break;
        case FrontendPage::Settings:m_group=m_selected;m_page=FrontendPage::SettingsGroup;m_selected=0;break;
        case FrontendPage::SettingsGroup:m_feedback="Conexion con el runtime pendiente.";break;
        default:break;
        }
        return FrontendCommand::None;
    }
    void draw(Renderer2D& renderer,const NativeRunSave* saved) {
        if(m_page==FrontendPage::Title) {m_title.draw(renderer,m_titleSelection,m_feedback);return;}
        renderer.clear(0xff3a303d);
        const char* heading=m_page==FrontendPage::Modes ? runtimeUiText("menu:selectGameMode") :
            m_page==FrontendPage::Load ? runtimeUiText("menu:loadGame") :
            m_page==FrontendPage::History ? runtimeUiText("menu:runHistory") : runtimeUiText("menu:settings");
        renderer.drawTextFitted(heading,12,8,0.45f,296,0xffffffff);
        renderer.drawWindow(16,33,288,168);
        if(m_page==FrontendPage::History) {
            renderer.drawTextFitted("No hay partidas finalizadas registradas.",28,58,0.32f,264,0xffffffff);
        } else if(m_page==FrontendPage::Load) {
            if(!saved || !m_titleSelection.hasContinue) {
                renderer.drawTextFitted(runtimeUiText("menu:noSaves"),28,58,0.36f,264,0xffffffff);
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
                const float y=43+i*29;
                const char* label=m_page==FrontendPage::Modes ? kFrontendModes[i].label :
                    m_page==FrontendPage::Settings ? runtimeUiText(groupKeys()[i]) : runtimeUiText(settingKey(m_group,i));
                const float labelSize=renderer.drawTextFitted(label,43,y,0.4f,m_page==FrontendPage::SettingsGroup ? 220 : 249,0xffffffff);
                if(i==m_selected) m_title.drawCursor(renderer,25,y,labelSize);
                if(m_page==FrontendPage::SettingsGroup) renderer.drawText("--",275,y,0.4f,0xffffffff);
            }
        }
        renderer.drawTextFitted(m_feedback ? m_feedback : "A: elegir   B: volver",12,214,0.3f,296,0xffffffff);
    }
private:
    unsigned rowCount() const {
        switch(m_page) {
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
    TitleMenuPresenter m_title;
    TitleMenuSelection m_titleSelection;
    FrontendPage m_page=FrontendPage::Title;
    unsigned m_selected=0,m_group=0;
    bool m_confirmingDelete=false;
    const char* m_feedback=nullptr;
};
}

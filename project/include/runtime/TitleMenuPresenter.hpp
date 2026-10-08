#pragma once
#include "gfx/renderer2d.hpp"
#include "runtime/TitleMenuLayout.hpp"
#include "content/TitleMenuText.hpp"
#include <cmath>
namespace Pokerogue3DS {
class TitleMenuPresenter {
public:
    TitleMenuPresenter()=default;
    TitleMenuPresenter(const TitleMenuPresenter&)=delete;
    TitleMenuPresenter& operator=(const TitleMenuPresenter&)=delete;
    ~TitleMenuPresenter() {clear();}
    void clear(Renderer2D* renderer=nullptr) {
        if(m_cursor) {
            if(renderer) renderer->retireSpriteSheet(m_cursor);
            else C2D_SpriteSheetFree(m_cursor);
        }
        m_cursor=nullptr;m_cursorAttempted=false;
    }
    void draw(Renderer2D& renderer,const TitleMenuSelection& menu,const char* feedback) {
        ensureCursor();
        renderer.clear(0xff3a303d);
        renderer.drawWindow(16,36,288,menu.count()*29+28);
        for(unsigned i=0;i<menu.count();++i) {
            const float y=48+i*29;
            const float labelSize=renderer.drawTextFitted(kTitleMenuLabels[i+(menu.hasContinue ? 0 : 1)],43,y,0.48f,249,0xffffffff);
            if(i==menu.selected) drawCursor(renderer,25,y,labelSize);
        }
        renderer.drawTextFitted(feedback ? feedback : "X: menú   A: elegir   D-Pad: mover",16,212,0.3f,288,0xffffffff);
    }
    void drawCursor(Renderer2D& renderer,float x,float y,float textSize) {
        ensureCursor();
        if(m_cursor) {
            const auto image=C2D_SpriteSheetGetImage(m_cursor,0);
            if(image.subtex) {
                renderer.drawImageDirect(image,std::round(x),std::round(titleCursorY(y,renderer.textInkHeight(textSize),15)),9,15);
                return;
            }
        }
        renderer.drawText(">", x, y, textSize, 0xffffffff);
    }
private:
    void ensureCursor() {
        if(!m_cursorAttempted) {
            m_cursorAttempted=true;
            m_cursor=C2D_SpriteSheetLoad("romfs:/presentation/ui/cursor.t3x");
            if(m_cursor) {
                const auto image=C2D_SpriteSheetGetImage(m_cursor,0);
                if(image.tex) C3D_TexSetFilter(image.tex, GPU_NEAREST, GPU_NEAREST);
            }
        }
    }
    C2D_SpriteSheet m_cursor=nullptr;
    bool m_cursorAttempted=false;
};
}

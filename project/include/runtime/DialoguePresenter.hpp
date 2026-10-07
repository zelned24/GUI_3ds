#pragma once
#include "gfx/renderer2d.hpp"
#include "runtime/TextPageLayout.hpp"
#include <string>
#include <limits>
#include <cstdio>
namespace Pokerogue3DS {
class DialoguePresenter {
public:
    explicit DialoguePresenter(unsigned lines=2,float width=270):m_lines(lines),m_width(width) {}
    void reset() {m_message.clear();m_offset=0;m_page={};}
    bool hasNavigation() const {return m_page.valid && !m_message.empty() && (!m_page.complete || m_offset);}
    void sync(Renderer2D& renderer,const std::string& message) {
        if(m_message==message) return;
        m_message=message;m_offset=0;prepare(renderer);
    }
    bool advance(Renderer2D& renderer) {
        if(!m_page.valid || m_message.empty() || (m_page.complete && !m_offset)) return false;
        m_offset=m_page.complete ? 0 : m_offset+m_page.consumed;
        prepare(renderer);return true;
    }
    void draw(Renderer2D& renderer) const {
        if(m_message.empty() || !m_page.valid) return;
        renderer.drawWindow(16,194,368,40);
        drawAt(renderer,28,200,310,200);
    }
    void drawAt(Renderer2D& renderer,float x,float y,float noteX,float noteY) const {
        if(m_message.empty() || !m_page.valid) return;
        const float spacing=renderer.textLineHeight(kSize);
        for(unsigned i=0;i<m_page.lineCount;++i) renderer.drawText(m_page.lines[i],x,y+i*spacing,kSize,0xffffffff);
        if(hasNavigation()) renderer.drawText("SELECT >",noteX,noteY,0.25f,0xff80ffff);
    }
private:
    void prepare(Renderer2D& renderer) {
        m_page=layoutTextPage(m_message.c_str()+m_offset,m_width,[&](const char* text) {
            char scratch[256];float width=0;
            return renderer.abbreviateText(text,kSize,65536,scratch,sizeof(scratch),width)
                ? width : std::numeric_limits<float>::quiet_NaN();
        },m_lines);
        if(!m_page.valid) std::fprintf(stderr,"INVALID_DIALOGUE_LAYOUT: UTF-8 or native glyph measurement failed\n");
    }
    static constexpr float kSize=0.375f;
    std::string m_message;
    std::size_t m_offset=0;
    TextPageLayout m_page;
    unsigned m_lines;
    float m_width;
};
}

#pragma once
#include "gfx/renderer2d.hpp"
#include "content/PokemonIcons.hpp"
namespace Pokerogue3DS {
// Two physical atlas pages are resident; the catalog itself is not capped.
class PokemonIconPresenter {
public:
    PokemonIconPresenter()=default;
    PokemonIconPresenter(const PokemonIconPresenter&)=delete;
    PokemonIconPresenter& operator=(const PokemonIconPresenter&)=delete;
    ~PokemonIconPresenter() {clear();}
    void clear() {for(auto& slot:m_slots) {if(slot.sheet) C2D_SpriteSheetFree(slot.sheet);slot.sheet=nullptr;slot.page=0xffff;}}
    bool draw(Renderer2D& renderer,uint16_t dex,uint16_t formIndex,float x,float y,float opacity=1.0f,float scale=1.0f) {
        const PokemonIconDefinition* icon=nullptr;
        for(const auto& row:kPokemonIcons) if(row.dex==dex && row.formIndex==formIndex) {icon=&row;break;}
        if(!icon) return false;
        Slot* slot=nullptr;
        for(auto& candidate:m_slots) if(candidate.page==icon->page && candidate.sheet) {slot=&candidate;break;}
        if(!slot) {
            slot=&m_slots[m_next];m_next=(m_next+1)%2;
            if(slot->sheet) C2D_SpriteSheetFree(slot->sheet);
            slot->sheet=C2D_SpriteSheetLoad(kPokemonIconPages[icon->page]);slot->page=icon->page;
            if(slot->sheet) {
                const auto img=C2D_SpriteSheetGetImage(slot->sheet,0);
                if(img.tex) C3D_TexSetFilter(img.tex, GPU_NEAREST, GPU_NEAREST);
            }
        }
        if(!slot->sheet) return false;
        Renderer2D::AtlasFrame frame{icon->x,icon->y,icon->width,icon->height,icon->width,icon->height,0,0};
        renderer.drawAtlasFrame(C2D_SpriteSheetGetImage(slot->sheet,0),frame,x,y,icon->width*scale,icon->height*scale,opacity);
        return true;
    }
private:
    struct Slot {C2D_SpriteSheet sheet=nullptr;uint16_t page=0xffff;};
    Slot m_slots[2]{};
    unsigned m_next=0;
};
}

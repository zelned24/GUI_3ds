#pragma once
#include "gfx/renderer2d.hpp"
#include "content/PokemonIcons.hpp"
#include "content/CompactPokemonIcons.hpp"
#include <cmath>
#include <algorithm>
namespace Pokerogue3DS {
inline constexpr std::size_t kPokemonIconCount=sizeof(kPokemonIcons)/sizeof(kPokemonIcons[0]);
inline constexpr bool pokemonIconIndexOrdered() {
    for(std::size_t i=1;i<kPokemonIconCount;++i) {
        const auto& previous=kPokemonIcons[i-1];const auto& current=kPokemonIcons[i];
        if(previous.dex>current.dex || (previous.dex==current.dex && previous.formIndex>=current.formIndex)) return false;
    }
    return true;
}
static_assert(sizeof(kCompactPokemonIconPages)==sizeof(kPokemonIconPages),"Compact icon page count must match source index");
static_assert(pokemonIconIndexOrdered(),"Generated icon index must have unique sorted species/form identities");
inline const PokemonIconDefinition* findPokemonIcon(uint16_t dex,uint16_t formIndex) {
    std::size_t first=0,last=kPokemonIconCount;
    while(first<last) {
        const auto middle=first+(last-first)/2;const auto& row=kPokemonIcons[middle];
        if(row.dex<dex || (row.dex==dex && row.formIndex<formIndex)) first=middle+1;
        else last=middle;
    }
    if(first==kPokemonIconCount) return nullptr;
    const auto& row=kPokemonIcons[first];
    return row.dex==dex && row.formIndex==formIndex ? &row : nullptr;
}
// Lazy residency for indexed pages avoids synchronous reloads while browsing.
// Current snapshot: eight 512x512 RGBA8 pages, up to 8 MiB of texture RAM.
class PokemonIconPresenter {
public:
    explicit PokemonIconPresenter(bool compact=false):m_compact(compact) {}
    PokemonIconPresenter(const PokemonIconPresenter&)=delete;
    PokemonIconPresenter& operator=(const PokemonIconPresenter&)=delete;
    ~PokemonIconPresenter() {clear();}
    void clear(Renderer2D* renderer=nullptr) {for(auto& slot:m_slots) {if(slot.sheet) {if(renderer) renderer->retireSpriteSheet(slot.sheet);else C2D_SpriteSheetFree(slot.sheet);}slot.sheet=nullptr;slot.page=0xffff;}}
    bool draw(Renderer2D& renderer,uint16_t dex,uint16_t formIndex,float x,float y,float opacity=1.0f,float scale=1.0f,uint32_t tint=0xffffffff) {
        if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(opacity) || opacity<=0 ||
            !std::isfinite(scale) || scale<=0) return false;
        opacity=std::min(opacity,1.0f);
        const auto* icon=findPokemonIcon(dex,formIndex);
        if(!icon || icon->page>=sizeof(kPokemonIconPages)/sizeof(kPokemonIconPages[0])) return false;
        if(m_compact && ((icon->x|icon->y|icon->width|icon->height)&1)) return false;
        Slot* slot=&m_slots[icon->page];
        // page records the attempted identity even after an I/O failure.
        // clear() resets it, allowing recovery after an asset update.
        if(slot->page!=icon->page) {
            slot->sheet=C2D_SpriteSheetLoad(m_compact ? kCompactPokemonIconPages[icon->page] : kPokemonIconPages[icon->page]);slot->page=icon->page;
            if(slot->sheet) {
                const auto img=C2D_SpriteSheetGetImage(slot->sheet,0);
                if(img.tex) C3D_TexSetFilter(img.tex, GPU_NEAREST, GPU_NEAREST);
            }
        }
        if(!slot->sheet) return false;
        const unsigned divisor=m_compact ? 2 : 1;
        Renderer2D::AtlasFrame frame{uint16_t(icon->x/divisor),uint16_t(icon->y/divisor),uint16_t(icon->width/divisor),uint16_t(icon->height/divisor),uint16_t(icon->width/divisor),uint16_t(icon->height/divisor),0,0};
        renderer.drawAtlasFrame(C2D_SpriteSheetGetImage(slot->sheet,0),frame,x,y,frame.width*scale,frame.height*scale,opacity,tint);
        return true;
    }
private:
    bool m_compact=false;
    struct Slot {C2D_SpriteSheet sheet=nullptr;uint16_t page=0xffff;};
    Slot m_slots[sizeof(kPokemonIconPages)/sizeof(kPokemonIconPages[0])]{};
};
}

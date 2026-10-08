#pragma once
#include "gfx/renderer2d.hpp"
#include "content/PokemonIcons.hpp"
#include "content/CompactPokemonIcons.hpp"
#include "content/AppearanceIconIdentities.hpp"
#include "content/PokerogueRuntimeContent.hpp"
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
struct ResolvedPokemonIcon {
    const AppearanceIconIdentity* appearance=nullptr;
    uint16_t formIndex=0;
    bool normalIconAllowed=false;
};
// Resolves imported form ownership before looking up a baseline appearance.
inline ResolvedPokemonIcon resolvePokemonIcon(uint16_t dex,const char* formId,bool appearanceKnown,bool female,bool shiny,uint8_t variant) {
    ResolvedPokemonIcon result;
    const auto* owner=PokerogueContent::findSpeciesByDex(dex);if(!owner) return result;
    if(formId && *formId) {
        const auto* form=PokerogueContent::findFormById(formId);
        if(!form || std::strcmp(form->speciesId,owner->id)) return result;
        result.formIndex=form->upstreamFormIndex;
    }
    result.normalIconAllowed=!appearanceKnown;
    if(appearanceKnown) result.appearance=findAppearanceIconIdentity(dex,result.formIndex,female,shiny,variant);
    return result;
}
// Lazy residency for indexed pages avoids synchronous reloads while browsing.
// Current snapshot: eight 512x512 RGBA8 pages, up to 8 MiB of texture RAM.
class PokemonIconPresenter {
public:
    explicit PokemonIconPresenter(bool compact=false,unsigned appearanceCapacity=6):m_compact(compact),m_appearanceCapacity(compact && appearanceCapacity==18 ? 18 : 6) {}
    PokemonIconPresenter(const PokemonIconPresenter&)=delete;
    PokemonIconPresenter& operator=(const PokemonIconPresenter&)=delete;
    ~PokemonIconPresenter() {clear();}
    void clear(Renderer2D* renderer=nullptr) {
        for(auto& slot:m_slots) clearSlot(slot,renderer);
        for(auto& slot:m_appearanceSlots) clearSlot(slot,renderer);
        m_appearancesReady=false;
    }
    // Call once per visible team, after beginFrame and before submitting its draws.
    // Keeps pages needed by this batch; no eviction occurs while its icons are drawn.
    bool prepareAppearances(Renderer2D& renderer,const AppearanceIconIdentity* const* identities,unsigned count) {
        m_appearancesReady=false;
        if(count>m_appearanceCapacity || (count && !identities)) return false;
        uint16_t pages[18]{};unsigned pageCount=0;
        for(unsigned i=0;i<count;++i) {
            if(!identities[i]) continue; // Legacy appearance remains explicitly unknown.
            const auto* frame=appearanceIconPhysicalFrame(identities[i]);
            if(!frame || frame->page>=sizeof(kAppearanceIconPages)/sizeof(kAppearanceIconPages[0])) return false;
            bool present=false;for(unsigned j=0;j<pageCount;++j) if(pages[j]==frame->page) present=true;
            if(!present) {if(pageCount==m_appearanceCapacity) return false;pages[pageCount++]=frame->page;}
        }
        for(auto& slot:m_appearanceSlots) {
            bool wanted=false;for(unsigned i=0;i<pageCount;++i) if(slot.page==pages[i]) wanted=true;
            if(!wanted) clearSlot(slot,&renderer);
        }
        bool loaded=true;
        for(unsigned i=0;i<pageCount;++i) {
            Slot* selected=nullptr;
            for(auto& slot:m_appearanceSlots) if(slot.page==pages[i]) selected=&slot;
            if(!selected) {
                for(unsigned index=0;index<m_appearanceCapacity;++index) if(m_appearanceSlots[index].page==0xffff) {selected=&m_appearanceSlots[index];break;}
                if(!selected) return false;
                selected->page=pages[i];
                selected->sheet=C2D_SpriteSheetLoad(m_compact ? kCompactAppearanceIconPages[pages[i]] : kAppearanceIconPages[pages[i]]);
                if(selected->sheet) {
                    const auto image=C2D_SpriteSheetGetImage(selected->sheet,0);
                    const unsigned expected=m_compact ? 256 : 512;
                    if(!image.tex || !image.subtex || image.subtex->width!=expected || image.subtex->height!=expected ||
                        image.subtex->top<image.subtex->bottom) {
                        renderer.retireSpriteSheet(selected->sheet);selected->sheet=nullptr;
                    } else C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST);
                }
            }
            if(!selected->sheet) loaded=false;
        }
        m_appearancesReady=true;
        return loaded;
    }
    bool drawAppearance(Renderer2D& renderer,const AppearanceIconIdentity* identity,float x,float y,float opacity=1.0f,unsigned scale=1) {
        if(scale<1 || scale>2) return false;
        if(!m_appearancesReady || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(opacity) || opacity<=0) return false;
        const auto* icon=appearanceIconPhysicalFrame(identity);if(!icon) return false;
        Slot* selected=nullptr;for(auto& slot:m_appearanceSlots) if(slot.page==icon->page) selected=&slot;
        if(!selected || !selected->sheet) return false;
        const unsigned divisor=m_compact ? 2 : 1;
        if(m_compact && ((icon->x|icon->y|icon->width|icon->height)&1)) return false;
        Renderer2D::AtlasFrame frame{uint16_t(icon->x/divisor),uint16_t(icon->y/divisor),uint16_t(icon->width/divisor),uint16_t(icon->height/divisor),uint16_t(icon->width/divisor),uint16_t(icon->height/divisor),0,0};
        renderer.drawAtlasFrame(C2D_SpriteSheetGetImage(selected->sheet,0),frame,std::round(x),std::round(y),frame.width*scale,frame.height*scale,std::min(opacity,1.0f));
        return true;
    }
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
                const unsigned expected=m_compact ? 256 : 512;
                if(!img.tex || !img.subtex || img.subtex->width!=expected || img.subtex->height!=expected
                    || !std::isfinite(img.subtex->left) || !std::isfinite(img.subtex->right)
                    || !std::isfinite(img.subtex->top) || !std::isfinite(img.subtex->bottom)
                    || img.subtex->top<img.subtex->bottom) {
                    renderer.retireSpriteSheet(slot->sheet);slot->sheet=nullptr;
                } else C3D_TexSetFilter(img.tex,GPU_NEAREST,GPU_NEAREST);
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
    static void clearSlot(Slot& slot,Renderer2D* renderer) {
        if(slot.sheet) {if(renderer) renderer->retireSpriteSheet(slot.sheet);else C2D_SpriteSheetFree(slot.sheet);}
        slot.sheet=nullptr;slot.page=0xffff;
    }
    bool m_appearancesReady=false;
    unsigned m_appearanceCapacity=6;
    Slot m_appearanceSlots[18]{};
    Slot m_slots[sizeof(kPokemonIconPages)/sizeof(kPokemonIconPages[0])]{};
};
}

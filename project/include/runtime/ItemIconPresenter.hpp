#pragma once
#include "gfx/renderer2d.hpp"
#include "content/ItemIconTextures.hpp"
#include "content/ItemIconReferences.hpp"
#include <cmath>
#include <cstdint>

namespace Pokerogue3DS {
class ItemIconPresenter {
public:
    ItemIconPresenter() = default;
    ItemIconPresenter(const ItemIconPresenter&) = delete;
    ItemIconPresenter& operator=(const ItemIconPresenter&) = delete;
    ~ItemIconPresenter() { clear(); }
    void clear(Renderer2D* renderer=nullptr) {
        for(auto& slot:m_slots) {
            if(slot.sheet) {
                if(renderer) renderer->retireSpriteSheet(slot.sheet);
                else C2D_SpriteSheetFree(slot.sheet);
            }
            slot={};
        }
        m_serial=0;
    }
    static bool validImage(C2D_Image image,uint16_t width,uint16_t height) {
        if(!width || !height || !image.tex || !image.subtex) return false;
        const auto& sub=*image.subtex;
        return sub.width==width && sub.height==height
            && std::isfinite(sub.left) && std::isfinite(sub.right)
            && std::isfinite(sub.top) && std::isfinite(sub.bottom)
            && sub.left<sub.right && sub.top>sub.bottom;
    }
    bool draw(Renderer2D& renderer,const char* key,float x,float y,float size = 32,float opacity=1.0f) {
        if(!key || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(size)
            || size<=0 || !std::isfinite(opacity) || opacity<=0) return false;
        const auto* definition=findItemIconTexture(key);
        if(!definition || !definition->width || !definition->height) return false;
        Slot* chosen=nullptr;
        for(auto& slot:m_slots) if(slot.definition==definition) {chosen=&slot;break;}
        if(!chosen) {
            for(auto& slot:m_slots) if(!slot.definition) {chosen=&slot;break;}
            if(!chosen) {
                chosen=&m_slots[0];
                for(auto& slot:m_slots) if(slot.serial<chosen->serial) chosen=&slot;
            }
            if(chosen->sheet) renderer.retireSpriteSheet(chosen->sheet);
            *chosen={};chosen->definition=definition;
            chosen->sheet=C2D_SpriteSheetLoad(definition->path);
            // Retain failed entries too; retry only after explicit cache invalidation.
        }
        if(++m_serial==0) {for(auto& slot:m_slots) slot.serial=0;m_serial=1;}
        chosen->serial=m_serial;
        if(!chosen->sheet) return false;
        const auto image=C2D_SpriteSheetGetImage(chosen->sheet,0);
        if(!validImage(image,definition->width,definition->height)) {
            renderer.retireSpriteSheet(chosen->sheet);
            chosen->sheet=nullptr; // Keep the failed identity; recover only after clear().
            return false;
        }
        C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST);
        renderer.drawImageDirect(image,std::round(x),std::round(y),size,size,0,
            opacity>1 ? 1 : opacity);
        return true;
    }
    bool drawItem(Renderer2D& renderer,const char* itemId,float x,float y,float size = 32,float opacity=1.0f) {
        const char* key=findItemIconKey(itemId);
        return key && draw(renderer,key,x,y,size,opacity);
    }
private:
    // Five balls or three reward choices fit without per-frame texture churn.
    // Physical 32x32 sources replace each presenter's full 512x512 atlas allocation.
    struct Slot {const ItemIconTexture* definition=nullptr;C2D_SpriteSheet sheet=nullptr;uint64_t serial=0;};
    Slot m_slots[8]{};
    uint64_t m_serial=0;
};
}

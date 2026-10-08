#pragma once
#include "gfx/renderer2d.hpp"
#include "content/EggTextures.hpp"
#include <cmath>
namespace Pokerogue3DS {
class EggTexturePresenter {
public:
    EggTexturePresenter()=default;
    EggTexturePresenter(const EggTexturePresenter&)=delete;
    EggTexturePresenter& operator=(const EggTexturePresenter&)=delete;
    ~EggTexturePresenter() {clear();}
    void clear(Renderer2D* renderer=nullptr) {
        for(auto& slot:m_slots) {
            if(slot.sheet) {
                if(renderer) renderer->retireSpriteSheet(slot.sheet);
                else C2D_SpriteSheetFree(slot.sheet);
            }
            slot={};
        }
    }
    bool draw(Renderer2D& renderer,const char* atlas,const char* key,float x,float y) {
        if(!std::isfinite(x) || !std::isfinite(y)) return false;
        const auto* definition=findEggTexture(atlas,key);
        if(!definition) return false;
        const size_t index=static_cast<size_t>(definition-kEggTextures);
        if(index>=kCount) return false;
        auto& slot=m_slots[index];
        if(!slot.attempted) {
            slot.attempted=true;slot.sheet=C2D_SpriteSheetLoad(definition->path);
        }
        if(!slot.sheet) return false;
        const auto image=C2D_SpriteSheetGetImage(slot.sheet,0);
        if(!image.tex || !image.subtex || image.subtex->width!=definition->width
            || image.subtex->height!=definition->height) return false;
        C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST);
        renderer.drawImageDirect(image,std::round(x),std::round(y),definition->width,definition->height);
        return true;
    }
private:
    static constexpr size_t kCount=sizeof(kEggTextures)/sizeof(kEggTextures[0]);
    struct Slot {C2D_SpriteSheet sheet=nullptr;bool attempted=false;};
    Slot m_slots[kCount]{};
};
}

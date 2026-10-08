#include "runtime/TrainerPresenter.hpp"
#include "runtime/ArenaPresenter.hpp"
#include <cassert>
#include <string>
using namespace Pokerogue3DS;
// Host doubles exercise load ownership/cache, not the PICA200 GPU.
static unsigned loads=0,frees=0,retired=0;
static bool failLoad=true,missingTexture=false,missingRegion=false;
static C3D_Tex texture{};
static Tex3DS_SubTexture subtexture{60,155,0,0,1,1};
C2D_SpriteSheet C2D_SpriteSheetLoad(const char*) {++loads;return failLoad ? nullptr : &texture;}
C2D_Image C2D_SpriteSheetGetImage(C2D_SpriteSheet,size_t) {return {missingTexture ? nullptr : &texture,missingRegion ? nullptr : &subtexture};}
void C2D_SpriteSheetFree(C2D_SpriteSheet sheet) {assert(sheet==&texture);++frees;}
void C3D_TexSetFilter(C3D_Tex*,GPU_TEXTURE_FILTER_PARAM mag,GPU_TEXTURE_FILTER_PARAM min) {
    assert(mag==GPU_NEAREST && min==GPU_NEAREST);
}
Renderer2D::Renderer2D() {}
Renderer2D::~Renderer2D() {}
void Renderer2D::retireSpriteSheet(C2D_SpriteSheet sheet) {assert(sheet==&texture);++retired;}
void Renderer2D::drawAtlasFrame(C2D_Image,const AtlasFrame&,float,float,float,float,float,uint32_t) {}
void Renderer2D::drawImageDirect(C2D_Image,float,float,float,float,float,float,bool,bool,uint32_t) {}
void Renderer2D::drawText(const char*,float,float,float,uint32_t) {}
float Renderer2D::drawTextFitted(const char*,float x,float y,float size,float width,uint32_t,float* drawnWidth) {
    assert(x==20.0f && y==13.0f && width==164.0f);
    if(drawnWidth) *drawnWidth=0;
    return size;
}
bool Renderer2D::drawWindow(float,float,float,float) {return true;}
int main() {
    Renderer2D renderer;
    TrainerPresenter trainer;
    assert(!trainer.load("marley",&renderer));
    assert(!trainer.load("marley",&renderer) && loads==1 && !trainer.isLoaded());
    failLoad=false;
    assert(!trainer.load("marley",&renderer) && loads==1); // Clear explicitly enables recovery.
    trainer.clear(&renderer);
    assert(trainer.load("marley",&renderer) && loads==2 && trainer.isLoaded());
    assert(trainer.load("marley",&renderer) && loads==2);
    assert(!trainer.load("missing-test-identity",&renderer) && retired==1 && !trainer.isLoaded());
    assert(!trainer.load("missing-test-identity",&renderer) && loads==2);
    assert(trainer.load("marley",&renderer) && loads==3); // A new identity also permits retry.
    const std::string oversized(64,'a');
    assert(!trainer.load(oversized.c_str(),&renderer) && !trainer.isLoaded() && retired==2);
    assert(trainer.load("marley",&renderer) && loads==4);
    // No host RomFS file exists for Aaron: metadata failure releases the new sheet.
    assert(!trainer.load("aaron",&renderer) && loads==5 && retired==4 && frees==0);
    assert(!trainer.load("aaron",&renderer) && loads==5);
    trainer.clear(&renderer);
    assert(trainer.load("marley",&renderer) && loads==6);
    trainer.clear();assert(frees==1);
    // A successful sheet handle is insufficient: malformed images must fail and retire.
    for(unsigned failure=0;failure<3;++failure) {
        const unsigned beforeLoads=loads,beforeRetired=retired;
        missingTexture=failure==0;missingRegion=failure==1;
        subtexture.width=failure==2 ? 0 : 60;
        assert(!trainer.load("marley",&renderer) && !trainer.isLoaded());
        assert(loads==beforeLoads+1 && retired==beforeRetired+1);
        assert(!trainer.load("marley",&renderer) && loads==beforeLoads+1);
        trainer.clear(&renderer);
    }
    missingTexture=false;missingRegion=false;subtexture.width=60;
    const unsigned arenaBaseline=loads;
    {
        ArenaPresenter arena;
        assert(arena.drawTrainerBattleIntro(renderer,65535,false,"marley") && loads==arenaBaseline+1);
        assert(arena.drawTrainerBattleIntro(renderer,65535,false,"marley") && loads==arenaBaseline+1);
        assert(arena.drawTrainerBattleIntro(renderer,65535,true,"marley") && loads==arenaBaseline+2);
        assert(arena.drawTrainerBattleIntro(renderer,65535,true,"mira") && loads==arenaBaseline+3);
        failLoad=true;
        assert(!arena.drawTrainerBattleIntro(renderer,65535,true,"riley") && loads==arenaBaseline+4);
        assert(!arena.drawTrainerBattleIntro(renderer,65535,true,"riley") && loads==arenaBaseline+4);
        failLoad=false;
        assert(!arena.drawTrainerBattleIntro(renderer,65535,true,"riley") && loads==arenaBaseline+4);
        arena.clear(&renderer);
        assert(arena.drawTrainerBattleIntro(renderer,65535,true,"riley") && loads==arenaBaseline+5);
    }

}

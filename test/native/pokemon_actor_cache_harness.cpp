#include "runtime/PokemonAtlasPresenter.hpp"
#include "gfx/renderer2d.hpp"
#include <cassert>
using namespace Pokerogue3DS;
static unsigned trainerAttempts=0,playerAttempts=0;
void TrainerPresenter::clear(Renderer2D*) {}
bool TrainerPresenter::loadTrainer(uint16_t,bool,Renderer2D*) {++trainerAttempts;return false;}
bool TrainerPresenter::loadPlayerBack(bool,Renderer2D*) {++playerAttempts;return false;}
void TrainerPresenter::drawAnchored(Renderer2D&,float,float,float,uint64_t) {assert(false);}
void C2D_SpriteSheetFree(C2D_SpriteSheet) {assert(false);}
Renderer2D::Renderer2D() {}
Renderer2D::~Renderer2D() {}
void Renderer2D::retireSpriteSheet(C2D_SpriteSheet) {assert(false);}
int main() {
    C3D_Tex texture{};Tex3DS_SubTexture region{};
    region.width=64;region.height=32;region.left=0;region.right=1;region.top=1;region.bottom=0;
    C2D_Image image{&texture,&region};
    assert(PokemonAtlasPresenter::validAtlasImage(image,64,32));
    assert(!PokemonAtlasPresenter::validAtlasImage(image,32,32));
    assert(!PokemonAtlasPresenter::validAtlasImage(image,0,32));
    region.right=region.left;assert(!PokemonAtlasPresenter::validAtlasImage(image,64,32));
    region.right=-1;assert(!PokemonAtlasPresenter::validAtlasImage(image,64,32));
    region.right=1;region.bottom=region.top;assert(!PokemonAtlasPresenter::validAtlasImage(image,64,32));
    region.bottom=2;assert(!PokemonAtlasPresenter::validAtlasImage(image,64,32));
    region.bottom=NAN;assert(!PokemonAtlasPresenter::validAtlasImage(image,64,32));
    region.bottom=0;region.left=INFINITY;assert(!PokemonAtlasPresenter::validAtlasImage(image,64,32));
    region.left=0;
    image.subtex=nullptr;assert(!PokemonAtlasPresenter::validAtlasImage(image,64,32));
    image.subtex=&region;image.tex=nullptr;assert(!PokemonAtlasPresenter::validAtlasImage(image,64,32));
    Renderer2D renderer;PokemonAtlasPresenter actors;
    for(unsigned i=0;i<100;++i) {
        actors.drawTrainerAnchored(renderer,1,false,0,0,1,i);
        actors.drawPlayerBackAnchored(renderer,false,0,0,1,i);
    }
    assert(trainerAttempts==1 && playerAttempts==1);
    actors.drawTrainerAnchored(renderer,2,false,0,0,1,100);
    actors.drawTrainerAnchored(renderer,2,true,0,0,1,101);
    actors.drawPlayerBackAnchored(renderer,true,0,0,1,100);
    assert(trainerAttempts==3 && playerAttempts==2);
    actors.invalidate(&renderer);
    actors.drawTrainerAnchored(renderer,2,true,0,0,1,102);
    actors.drawPlayerBackAnchored(renderer,true,0,0,1,102);
    assert(trainerAttempts==4 && playerAttempts==3);
}

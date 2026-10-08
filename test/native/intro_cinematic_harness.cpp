#include "runtime/IntroCinematicPresenter.hpp"
#include <cassert>
#include <cstring>
using namespace Pokerogue3DS;
static C3D_Tex texture{};
static Tex3DS_SubTexture subtexture{1024,512,0,0,1,1};
static unsigned loads=0,retired=0,frees=0,draws=0;
static bool failLoad=false;
static const char* loadedPath=nullptr;
static Renderer2D::AtlasFrame lastFrame{};
C2D_SpriteSheet C2D_SpriteSheetLoad(const char* path) {++loads;loadedPath=path;return failLoad?nullptr:&texture;}
C2D_Image C2D_SpriteSheetGetImage(C2D_SpriteSheet,size_t) {return {&texture,&subtexture};}
void C2D_SpriteSheetFree(C2D_SpriteSheet) {++frees;}
void C3D_TexSetFilter(C3D_Tex*,GPU_TEXTURE_FILTER_PARAM mag,GPU_TEXTURE_FILTER_PARAM min) {assert(mag==GPU_NEAREST && min==GPU_NEAREST);}
Renderer2D::Renderer2D() {}
Renderer2D::~Renderer2D() {}
void Renderer2D::clear(uint32_t) {}
void Renderer2D::retireSpriteSheet(C2D_SpriteSheet sheet) {assert(sheet);++retired;}
void Renderer2D::drawAtlasFrame(C2D_Image,const AtlasFrame& frame,float x,float y,float w,float h,float opacity,uint32_t) {
    assert(x==0 && y==20 && w==400 && h==200);
    assert(frame.width==200 && frame.height==100 && opacity>=0 && opacity<=1);
    lastFrame=frame;++draws;
}
int main() {
    Renderer2D renderer;
    IntroCinematicPresenter intro;intro.start();
    for(unsigned i=0;i<kIntroKeyframeCount;++i) {
        const auto& frame=kIntroKeyframes[i];
        assert(intro.draw(renderer,1+frame.timeMs));
        assert(lastFrame.x==frame.x && lastFrame.y==frame.y);
        assert(std::strcmp(loadedPath,kIntroCinematicPaths[frame.page])==0);
    }
    assert(loads==kIntroPageCount && retired==kIntroPageCount-1 && draws==kIntroKeyframeCount);
    assert(intro.draw(renderer,1+kIntroTotalDurationMs));
    assert(!intro.draw(renderer,1+kIntroTotalDurationMs+300));
    assert(intro.isFinished() && retired==kIntroPageCount && frees==0);
    intro.start();failLoad=true;
    assert(!intro.draw(renderer,1) && intro.isFinished());
    failLoad=false;intro.start();assert(intro.draw(renderer,1));
    intro.skip();assert(intro.isFinished() && frees==1);
}

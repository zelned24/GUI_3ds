#include "gfx/renderer2d.hpp"
#include "content/TypeLabels.hpp"
#include "content/HudTypeIcons.hpp"
#include <cassert>
#include "screens/SceneAssets.hpp"
#include <cstring>
#include <cstdarg>
#include <cmath>
namespace {
int fail=0,c3Free=0,c2Free=0,fontFree=0,sheetFree=0,bufferFree=0,targets=0;
int draws=0;u32 lastFlags=0;float lastX=0,lastY=0,lastScale=0;
int hudTokens[6]={3,4,5,6,7,8};Tex3DS_SubTexture hudSubs[6]={{20,460,0,1,1,0},{20,240,0,1,1,0},{20,240,0,1,1,0},{20,460,0,1,1,0},{20,240,0,1,1,0},{20,240,0,1,1,0}};
int hudLoads=0;bool hudMissing=false;
int typeToken=2,typeLoads=0,imageDraws=0;bool typeMissing=false;float imageScaleX=0,imageScaleY=0;Tex3DS_SubTexture typeSub{32,280,0,1,1,0};
int token=1;C3D_Tex tex{};Tex3DS_SubTexture sub{24,24,0,1,1,0};C2D_FontInfo fontInfo{26};
}
void C3D_TexSetFilter(C3D_Tex*,GPU_TEXTURE_FILTER_PARAM a,GPU_TEXTURE_FILTER_PARAM b) {assert(a==GPU_NEAREST && b==GPU_NEAREST);}
extern "C" {
bool C3D_Init(size_t) {return fail!=1;}
void C3D_Fini() {++c3Free;}
bool C2D_Init(size_t) {return fail!=2;}
void C2D_Fini() {++c2Free;}
void C2D_Prepare() {}
C3D_RenderTarget* C2D_CreateScreenTarget(gfxScreen_t,gfx3dSide_t) {++targets;return fail==3 ? nullptr : reinterpret_cast<C3D_RenderTarget*>(&token);}
C2D_TextBuf C2D_TextBufNew(size_t) {return fail==4 ? nullptr : &token;}
void C2D_TextBufDelete(C2D_TextBuf) {++bufferFree;}
C2D_Font C2D_FontLoad(const char*) {return fail==5 ? nullptr : &token;}
void C2D_FontFree(C2D_Font) {++fontFree;}
void C2D_FontSetFilter(C2D_Font,GPU_TEXTURE_FILTER_PARAM a,GPU_TEXTURE_FILTER_PARAM b) {assert(a==GPU_NEAREST && b==GPU_NEAREST);}
const C2D_FontInfo* C2D_FontGetInfo(C2D_Font font) {assert(font);return &fontInfo;}
C2D_SpriteSheet C2D_SpriteSheetLoad(const char* path) {for(unsigned i=0;i<6;++i) if(!std::strcmp(path,Pokerogue3DS::kHudTypeAtlases[i].path)) {++hudLoads;return hudMissing ? nullptr : &hudTokens[i];}if(std::strcmp(path,Pokerogue3DS::kTypeLabelPath)==0) {++typeLoads;return typeMissing ? nullptr : &typeToken;}return fail==6 ? nullptr : &token;}
void C2D_SpriteSheetFree(C2D_SpriteSheet) {++sheetFree;}
C2D_Image C2D_SpriteSheetGetImage(C2D_SpriteSheet sheet,size_t) {for(unsigned i=0;i<6;++i) if(sheet==&hudTokens[i]) return {&tex,&hudSubs[i]};return {&tex,sheet==&typeToken ? &typeSub : (fail==7 ? nullptr : &sub)};}
bool C3D_FrameBegin(u8) {return true;}
void C2D_TextBufClear(C2D_TextBuf) {}
void C2D_SceneBegin(C3D_RenderTarget*) {}
void C2D_TargetClear(C3D_RenderTarget*,u32) {}
void C2D_DrawRectSolid(float,float,float,float,float,u32) {}
void C2D_DrawImageAtRotated(C2D_Image,float,float,float,float,const C2D_ImageTint*,float sx,float sy) {++imageDraws;imageScaleX=sx;imageScaleY=sy;}
void C2D_PlainImageTint(C2D_ImageTint*,u32,float) {}
void C2D_TextParse(C2D_Text*,C2D_TextBuf,const char*) {assert(false && "System font fallback in production path");}
void C2D_TextFontParse(C2D_Text* text,C2D_Font,C2D_TextBuf,const char*) {text->width=100;}
void C2D_TextOptimize(const C2D_Text*) {}
void C2D_TextGetDimensions(const C2D_Text* text,float x,float,float* width,float*) {if(width) *width=text->width*x;}
void C2D_DrawText(const C2D_Text*,u32 flags,float x,float y,float,float scale,float,...) {
    ++draws;lastFlags=flags;lastX=x;lastY=y;lastScale=scale;
}
void C3D_FrameEnd(u8) {}
}
namespace Citro2D {const AssetEntry* findSceneAsset(const char*) {return nullptr;}}
int main() {
    for(int scenario=1;scenario<=7;++scenario) {
        fail=scenario;c3Free=c2Free=fontFree=sheetFree=bufferFree=targets=0;
        Renderer2D renderer;assert(!renderer.init());assert(!renderer.isInitialized());
        assert(renderer.initializationError() && std::strlen(renderer.initializationError()));
        assert(c3Free==(scenario==1 ? 0 : 1));assert(c2Free==(scenario<=2 ? 0 : 1));
        assert(bufferFree==(scenario<=4 ? 0 : 1));assert(fontFree==(scenario<=5 ? 0 : 1));
        assert(sheetFree==(scenario==7 ? 1 : 0));
        renderer.fini();assert(c3Free==(scenario==1 ? 0 : 1));
        fail=0;assert(renderer.init());assert(renderer.isInitialized() && !renderer.initializationError());
        assert(renderer.textLineHeight(0.5f)==26.0f);assert(renderer.windowStyle()==1);
        renderer.beginTop();
        renderer.drawText("ABC",1.4f,2.7f,0.4f,0xffffffff);
        assert(lastX==1 && lastY==3 && std::fabs(lastScale-0.8f)<0.0001f);
        renderer.drawTextWrapped("ABC DEF",1,2,0.4f,40,0xffffffff);
        assert((lastFlags & C2D_WordWrap)!=0 && std::fabs(lastScale-0.8f)<0.0001f);
        float width=0;
        assert(std::fabs(renderer.drawTextFitted("ABC",1,2,0.4f,40,0xffffffff,&width)-0.2f)<0.0001f);
        assert(width==40);
        renderer.drawTextFitted("ABC",1,2,0.4f,200,0xffffffff,&width);assert(width==80);
        const int before=typeLoads;
        assert(!renderer.drawTypeLabel(nullptr,0,0,32,14));
        assert(!renderer.drawTypeLabel("NONE",0,0,32,14));
        assert(!renderer.drawTypeLabel("INVALID",0,0,32,14));
        assert(!renderer.drawTypeLabel("FIRE",0,0,0,14));assert(typeLoads==before);
        typeMissing=true;assert(!renderer.drawTypeLabel("FIRE",0,0,32,14));typeMissing=false;
        assert(renderer.drawTypeLabel("FIRE",0,0,32,14));assert(imageScaleX==1 && imageScaleY==1);
        const int loaded=typeLoads;
        for(const auto& row:Pokerogue3DS::kTypeLabelFrames) {
            assert(renderer.drawTypeLabel(row.key,0,0,94,18));assert(imageScaleX==1 && imageScaleY==1);
        }
        assert(typeLoads==loaded);
        assert(renderer.drawTypeLabel("Water",0,0,16,7));assert(imageScaleX==0.5f && imageScaleY==0.5f);
        typeSub.width=31;assert(!renderer.drawTypeLabel("FIRE",0,0,32,14));typeSub.width=32;
        assert(renderer.drawTypeLabel("FIRE",0,0,32,14));
        assert(!renderer.drawHudTypeIcon("NONE",true,0,false,0,0));
        assert(!renderer.drawHudTypeIcon("FIRE",true,2,true,0,0));
        assert(!renderer.drawHudTypeIcon("FIRE",true,1,false,0,0));
        hudMissing=true;assert(!renderer.drawHudTypeIcon("FIRE",true,0,false,0,0));hudMissing=false;
        for(unsigned i=0;i<6;++i) {
            const bool player=i<3;const unsigned slot=i%3==2 ? 1 : 0;const bool dual=i%3!=0;
            const auto& atlas=Pokerogue3DS::kHudTypeAtlases[i];
            for(unsigned f=0;f<atlas.count;++f) {
                const int before=imageDraws;
                assert(renderer.drawHudTypeIcon(atlas.frames[f].key,player,slot,dual,1.4f,2.7f));
                assert(imageDraws==before+1 && imageScaleX==1 && imageScaleY==1);
            }
        }
        hudSubs[0].width=19;assert(!renderer.drawHudTypeIcon("FIRE",true,0,false,0,0));hudSubs[0].width=20;
        assert(renderer.drawHudTypeIcon("Fire",true,0,false,0,0));
        assert(!renderer.setWindowStyle(2)); // Never free a texture before GPU submission.
        renderer.endFrame();assert(renderer.setWindowStyle(2) && renderer.windowStyle()==2);
        const int sheetsBeforeClose=sheetFree;
        renderer.fini();assert(sheetFree==sheetsBeforeClose+8); // Window, localized labels and six HUD variants.
        assert(!renderer.isInitialized());assert(renderer.textLineHeight(0.5f)==0);
        assert(renderer.init() && renderer.windowStyle()==1);renderer.fini();
    }
}

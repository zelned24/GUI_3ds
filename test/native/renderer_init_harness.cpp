#include "gfx/renderer2d.hpp"
#include "runtime/Utf8Abbreviation.hpp"
#include "runtime/NativeTextRaster.hpp"
#include "runtime/DialoguePresenter.hpp"
#include "runtime/BattleHudPresenter.hpp"
#include "runtime/PokemonIconPresenter.hpp"
#include "content/TypeLabels.hpp"
#include "content/NativeFontMetrics.hpp"
#include "content/EntityUiNames.hpp"
#include "content/HudTypeIcons.hpp"
#include <cassert>
#include "screens/SceneAssets.hpp"
#include <cstring>
#include <cstdarg>
#include <cmath>
#include <limits>
namespace {
constexpr unsigned iconPageCount=sizeof(Pokerogue3DS::kPokemonIconPages)/sizeof(Pokerogue3DS::kPokemonIconPages[0]);
int iconTokens[iconPageCount]{},iconLoads=0;Tex3DS_SubTexture iconSub{512,512,0,1,1,0};
int compactIconTokens[iconPageCount]{},compactIconLoads=0;Tex3DS_SubTexture compactIconSub{256,256,0,1,1,0};
int fail=0,c3Free=0,c2Free=0,fontFree=0,sheetFree=0,bufferFree=0,targets=0;
unsigned imageWidth=0,imageHeight=0;float imageTop=0,imageCenterX=0,imageCenterY=0,imageRotation=0;
int draws=0;u32 lastFlags=0;float lastX=0,lastY=0,lastScale=0;
int hudTokens[17]={3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19};Tex3DS_SubTexture hudSubs[17]={{20,460,0,1,1,0},{20,240,0,1,1,0},{20,240,0,1,1,0},{20,460,0,1,1,0},{20,240,0,1,1,0},{20,240,0,1,1,0},{22,64,0,1,1,0},{7,7,0,1,1,0},{48,6,0,1,1,0},{86,12,0,1,1,0},{85,2,0,1,1,0},{88,8,0,1,1,0},{88,8,0,1,1,0},{8,7,0,1,1,0},{13,7,0,1,1,0},{25,8,0,1,1,0},{16,7,0,1,1,0}};
int hudLoads=0;bool hudMissing=false;
int typeToken=2,typeLoads=0,imageDraws=0;bool typeMissing=false;float imageScaleX=0,imageScaleY=0;Tex3DS_SubTexture typeSub{32,280,0,1,1,0};
int measureToken=20,measureClears=0,mainClears=0,textAllocations=0;char lastParsed[256]{};
int fontTokens[3]={80,100,120};C2D_GlyphInfo glyphInfo[4]={{17},{19},{23},{31}};C2D_FontInfo smallInfo[3]={{26,9,&glyphInfo[0]},{26,12,&glyphInfo[1]},{26,14,&glyphInfo[2]}};
int token=1;C3D_Tex tex{};Tex3DS_SubTexture sub{24,24,0,1,1,0};C2D_FontInfo fontInfo{26,19,&glyphInfo[3]};
}
void C3D_TexSetFilter(C3D_Tex*,GPU_TEXTURE_FILTER_PARAM a,GPU_TEXTURE_FILTER_PARAM b) {assert(a==GPU_NEAREST && b==GPU_NEAREST);}
extern "C" {
bool C3D_Init(size_t) {return fail!=1;}
void C3D_Fini() {++c3Free;}
bool C2D_Init(size_t) {return fail!=2;}
void C2D_Fini() {++c2Free;}
void C2D_Prepare() {}
C3D_RenderTarget* C2D_CreateScreenTarget(gfxScreen_t,gfx3dSide_t) {++targets;return fail==3 ? nullptr : reinterpret_cast<C3D_RenderTarget*>(&token);}
C2D_TextBuf C2D_TextBufNew(size_t size) {++textAllocations;if(size==256) return fail==8 ? nullptr : &measureToken;return fail==4 ? nullptr : &token;}
void C2D_TextBufDelete(C2D_TextBuf) {++bufferFree;}
C2D_Font C2D_FontLoad(const char* path) {
    const char* names[]={"emerald-8.bcfnt","emerald-10.bcfnt","emerald-12.bcfnt"};
    for(unsigned i=0;i<3;++i) if(std::strstr(path,names[i])) return fail==int(9+i) ? nullptr : &fontTokens[i];
    assert(std::strstr(path,"emerald.bcfnt"));return fail==5 ? nullptr : &token;
}
void C2D_FontFree(C2D_Font) {++fontFree;}
void C2D_FontSetFilter(C2D_Font,GPU_TEXTURE_FILTER_PARAM a,GPU_TEXTURE_FILTER_PARAM b) {assert(a==GPU_NEAREST && b==GPU_NEAREST);}
const C2D_FontInfo* C2D_FontGetInfo(C2D_Font font) {assert(font);for(unsigned i=0;i<3;++i) if(font==&fontTokens[i]) return &smallInfo[i];return &fontInfo;}
C2D_SpriteSheet C2D_SpriteSheetLoad(const char* path) {for(unsigned p=0;p<iconPageCount;++p) if(!std::strcmp(path,Pokerogue3DS::kCompactPokemonIconPages[p])) {++compactIconLoads;return &compactIconTokens[p];}for(unsigned p=0;p<iconPageCount;++p) if(!std::strcmp(path,Pokerogue3DS::kPokemonIconPages[p])) {++iconLoads;return &iconTokens[p];}for(unsigned i=0;i<17;++i) if(!std::strcmp(path,Pokerogue3DS::kHudIconAtlases[i].path)) {++hudLoads;return hudMissing ? nullptr : &hudTokens[i];}if(std::strcmp(path,Pokerogue3DS::kTypeLabelPath)==0) {++typeLoads;return typeMissing ? nullptr : &typeToken;}return fail==6 ? nullptr : &token;}
void C2D_SpriteSheetFree(C2D_SpriteSheet) {++sheetFree;}
C2D_Image C2D_SpriteSheetGetImage(C2D_SpriteSheet sheet,size_t) {for(unsigned p=0;p<iconPageCount;++p) if(sheet==&compactIconTokens[p]) return {&tex,&compactIconSub};for(unsigned p=0;p<iconPageCount;++p) if(sheet==&iconTokens[p]) return {&tex,&iconSub};for(unsigned i=0;i<17;++i) if(sheet==&hudTokens[i]) return {&tex,&hudSubs[i]};return {&tex,sheet==&typeToken ? &typeSub : (fail==7 ? nullptr : &sub)};}
bool C3D_FrameBegin(u8) {return true;}
void C2D_TextBufClear(C2D_TextBuf buf) {if(buf==&measureToken) ++measureClears;else ++mainClears;}
void C2D_SceneBegin(C3D_RenderTarget*) {}
void C2D_TargetClear(C3D_RenderTarget*,u32) {}
void C2D_DrawRectSolid(float,float,float,float,float,u32) {}
void C2D_DrawImageAtRotated(C2D_Image image,float x,float y,float,float rotation,const C2D_ImageTint*,float sx,float sy) {imageCenterX=x;imageCenterY=y;imageRotation=rotation;++imageDraws;imageWidth=image.subtex->width;imageHeight=image.subtex->height;imageTop=image.subtex->top;imageScaleX=sx;imageScaleY=sy;}
void C2D_PlainImageTint(C2D_ImageTint*,u32,float) {}
void C2D_TextParse(C2D_Text*,C2D_TextBuf,const char*) {assert(false && "System font fallback in production path");}
void C2D_TextFontParse(C2D_Text* text,C2D_Font font,C2D_TextBuf buf,const char* value) {
    unsigned points=16;for(unsigned i=0;i<3;++i) if(font==&fontTokens[i]) points=Pokerogue3DS::kNativeFontPoints[i];
    text->font=font;
    text->width=0;for(const char* p=value;*p;) {uint32_t cp;unsigned n=Pokerogue3DS::utf8CodePoint(p,cp);assert(n);text->width+=std::round((cp=='W' ? 20 : 10)*float(points)/16);p+=n;}
    text->width*=30.0f/C2D_FontGetInfo(font)->tglp->cellHeight;
    if(buf!=&measureToken) {std::strncpy(lastParsed,value,sizeof(lastParsed)-1);lastParsed[sizeof(lastParsed)-1]=0;}
}
void C2D_TextOptimize(const C2D_Text*) {}
void C2D_TextGetDimensions(const C2D_Text* text,float x,float,float* width,float*) {if(width) *width=text->width*x;}
void C2D_DrawText(const C2D_Text* text,u32 flags,float x,float y,float,float scale,float,...) {
    ++draws;lastFlags=flags;lastX=x;lastY=y;lastScale=scale*30.0f/C2D_FontGetInfo(text->font)->tglp->cellHeight;
}
void C3D_FrameEnd(u8) {}
}
namespace Citro2D {const AssetEntry* findSceneAsset(const char*) {return nullptr;}}
int main() {
    fail=0;
    {Renderer2D invalid;glyphInfo[3].cellHeight=0;
     assert(!invalid.init() && !invalid.isInitialized());
     assert(!std::strcmp(invalid.initializationError(),"Invalid native font metrics"));
     glyphInfo[3].cellHeight=31;assert(invalid.init());invalid.fini();}
    {Renderer2D invalid;smallInfo[1].lineFeed=0;
     assert(!invalid.init() && !invalid.isInitialized());
     assert(!std::strcmp(invalid.initializationError(),"Invalid small native font metrics"));
     smallInfo[1].lineFeed=12;assert(invalid.init());invalid.fini();}
    for(int scenario=1;scenario<=11;++scenario) {
        fail=scenario;c3Free=c2Free=fontFree=sheetFree=bufferFree=targets=0;
        Renderer2D renderer;assert(!renderer.init());assert(!renderer.isInitialized());
        assert(renderer.initializationError() && std::strlen(renderer.initializationError()));
        assert(c3Free==(scenario==1 ? 0 : 1));assert(c2Free==(scenario<=2 ? 0 : 1));
        assert(bufferFree==(scenario<=4 ? 0 : (scenario<=8 ? 1 : 2)));assert(fontFree==(scenario<=5 ? 0 : (scenario<=9 ? 1 : scenario-8)));
        assert(sheetFree==(scenario>=7 ? 1 : 0));
        renderer.fini();assert(c3Free==(scenario==1 ? 0 : 1));
        fail=0;assert(renderer.init());assert(renderer.isInitialized() && !renderer.initializationError());
        assert(renderer.textLineHeight(0.5f)==19.0f);assert(renderer.textInkHeight(0.5f)==12.0f);assert(renderer.windowStyle()==1);
        renderer.beginTop();
        {
            const C2D_Image image{&tex,&sub};
            const int before=imageDraws;
            const float nan=std::numeric_limits<float>::quiet_NaN();
            const float inf=std::numeric_limits<float>::infinity();
            renderer.drawImageDirect(image,nan,0,24,24);
            renderer.drawImageDirect(image,0,inf,24,24);
            renderer.drawImageDirect(image,0,0,nan,24);
            renderer.drawImageDirect(image,0,0,24,inf);
            renderer.drawImageDirect(image,0,0,24,24,inf);
            renderer.drawImageDirect(image,0,0,24,24,0,nan);
            renderer.drawImageDirect(image,0,0,0,24);
            renderer.drawImageDirect(image,0,0,24,-1);
            renderer.drawImageDirect(image,std::numeric_limits<float>::max(),0,std::numeric_limits<float>::max(),24);
            assert(imageDraws==before);
            renderer.drawImageDirect(image,0,0,24,24,0,2);
            assert(imageDraws==before+1 && imageScaleX==1 && imageScaleY==1);
            renderer.drawImageDirect(image,1.4f,-2.7f,23,15);
            assert(imageDraws==before+2 && imageCenterX==12.5f && imageCenterY==4.5f && imageRotation==0);
            renderer.drawImageDirect(image,1.6f,-2.4f,23,15,0,1,true,true);
            assert(imageDraws==before+3 && imageCenterX==13.5f && imageCenterY==5.5f && imageScaleX<0 && imageScaleY<0);
            renderer.drawImageDirect(image,1.4f,-2.7f,23,15,0.25f);
            assert(imageDraws==before+4 && std::fabs(imageCenterX-12.9f)<0.00001f
                && std::fabs(imageCenterY-4.8f)<0.00001f && imageRotation==0.25f);
        }

        const float requested[]={0.25f,0.30f,0.375f,0.5f,1.0f};
        for(float size:requested) {
            renderer.drawText("WÉ",1.4f,2.7f,size,0xffffffff);
            const auto expected=Pokerogue3DS::nativeTextRaster(size);
            assert(std::fabs(lastScale-expected.scale)<0.000001f);
        }
        renderer.drawText("ABC",1,2,0xffffffff,1.0f);assert(std::fabs(lastScale-1)<0.000001f);
        renderer.drawText("ABC",1.4f,2.7f,0.4f,0xffffffff);
        assert(lastX==1 && lastY==3.0f-Pokerogue3DS::kNativeFontInkTop[2] && std::fabs(lastScale-1)<0.000001f);
        renderer.drawTextWrapped("ABC DEF",1,2,0.4f,40,0xffffffff);
        assert((lastFlags & C2D_WordWrap)!=0 && std::fabs(lastScale-1)<0.000001f);
        const int beforeBox=draws;
        assert(renderer.drawTextBox("AB CD",1.4f,2.7f,0.375f,18,2,0xffffffff));
        assert(draws==beforeBox+2 && lastX==1 && lastY==17.0f-Pokerogue3DS::kNativeFontInkTop[2] && lastScale==1);
        assert(!renderer.drawTextBox("AB CD EF",1,2,0.375f,18,2,0xffffffff));
        assert(draws==beforeBox+2);
        assert(!renderer.drawTextBox("AB",1,2,0.375f,18,13,0xffffffff));
        assert(!renderer.drawTextBox("AB",NAN,2,0.375f,18,2,0xffffffff));
        assert(!renderer.drawTextBox("AB",1,2,NAN,18,2,0xffffffff));
        assert(!renderer.drawTextBox("AB",1,2,0.375f,NAN,2,0xffffffff));
        assert(!renderer.drawTextBox(nullptr,1,2,0.375f,18,2,0xffffffff));
        assert(!renderer.drawTextBox("\xc3",1,2,0.375f,18,2,0xffffffff));
        assert(draws==beforeBox+2);
        const int beforeTrailingSpaces=draws;
        assert(renderer.drawTextBox("AB CD   \t",1,2,0.375f,18,2,0xffffffff));
        assert(draws==beforeTrailingSpaces+2 && lastScale==1);
        for(const auto& ability:Pokerogue3DS::kAbilityUiNames)
            assert(renderer.drawTextBox(ability.name,161,94,0.3125f,220,2,0xffffffff));
        Pokerogue3DS::DialoguePresenter dialogue;
        dialogue.sync(renderer,std::string(700,'A'));
        const int cleared=measureClears;
        dialogue.sync(renderer,std::string(700,'A'));assert(measureClears==cleared);
        const int beforeDialogue=draws;
        dialogue.draw(renderer);assert(draws==beforeDialogue+3 && lastX==310 && lastY==200.0f-Pokerogue3DS::kNativeFontInkTop[0] && lastScale==1);
        assert(dialogue.advance(renderer));
        dialogue.reset();dialogue.draw(renderer);assert(draws==beforeDialogue+3);
        dialogue.sync(renderer,std::string(700,'A'));assert(dialogue.advance(renderer));
        dialogue.sync(renderer,"ABC");assert(!dialogue.advance(renderer));
        const int beforeShort=draws;dialogue.draw(renderer);assert(draws==beforeShort+1);
        dialogue.sync(renderer,"");assert(!dialogue.advance(renderer));
        const int beforeEmpty=draws;dialogue.draw(renderer);assert(draws==beforeEmpty);
        float width=0;
        assert(renderer.drawTextFitted("ABC",1,2,0.4f,10,0xffffffff,&width)==0.25f);
        assert(std::fabs(width-10)<0.000001f && std::fabs(lastScale-1)<0.000001f && !std::strcmp(lastParsed,"A."));
        assert(lastY==2.0f-Pokerogue3DS::kNativeFontInkTop[0]); // Fitted text uses the chosen raster, not the requested size.
        renderer.drawTextFitted("ABC",1,2,0.4f,200,0xffffffff,&width);assert(width==24.0f && std::fabs(lastScale-1)<0.000001f);
        const int frameClears=mainClears,allocatedBefore=textAllocations;
        char abbreviated[128];float nameWidth=0;
        assert(renderer.abbreviateText("ABCDE",0.3f,20,abbreviated,sizeof(abbreviated),nameWidth));
        assert(!std::strcmp(abbreviated,"AB.") && nameWidth<=20 && measureClears>1);
        renderer.drawText(abbreviated,1,2,0.3f,0xffffffff);assert(std::fabs(lastScale-1)<0.000001f && !std::strcmp(lastParsed,"AB."));
        assert(renderer.abbreviateText("Nidoran♀",0.3f,100,abbreviated,sizeof(abbreviated),nameWidth,true));
        assert(!std::strcmp(abbreviated,"Nidoran"));
        assert(renderer.abbreviateText("Évoli",0.3f,20,abbreviated,sizeof(abbreviated),nameWidth));assert(!std::strcmp(abbreviated,"Év."));
        assert(!renderer.abbreviateText("ABC",0.3f,1,abbreviated,sizeof(abbreviated),nameWidth) && !*abbreviated && nameWidth==0);
        assert(!renderer.abbreviateText("ABC",-1,20,abbreviated,sizeof(abbreviated),nameWidth));
        assert(!renderer.abbreviateText("ABC",0.3f,20,abbreviated,300,nameWidth));
        assert(mainClears==frameClears && textAllocations==allocatedBefore);
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
        const int typeDrawsBefore=imageDraws,typeLoadsBefore=typeLoads;
        assert(!renderer.drawTypeLabel("Water",0,0,16,7));
        assert(!renderer.drawTypeLabel("Water",0,0,31,14));
        assert(!renderer.drawTypeLabel("Water",0,0,32,13));
        assert(!renderer.drawTypeLabel("Water",NAN,0,32,14));
        assert(!renderer.drawTypeLabel("Water",0,INFINITY,32,14));
        assert(!renderer.drawTypeLabel("Water",0,0,INFINITY,14));
        assert(imageDraws==typeDrawsBefore && typeLoads==typeLoadsBefore);
        typeSub.width=31;assert(!renderer.drawTypeLabel("FIRE",0,0,32,14));typeSub.width=32;
        assert(renderer.drawTypeLabel("FIRE",0,0,32,14));
        assert(!renderer.drawHudTypeIcon("NONE",true,0,false,0,0));
        assert(!renderer.drawHudTypeIcon("FIRE",true,2,true,0,0));
        assert(!renderer.drawHudTypeIcon("FIRE",true,1,false,0,0));
        hudMissing=true;assert(!renderer.drawHudTypeIcon("FIRE",true,0,false,0,0));hudMissing=false;
        for(unsigned i=0;i<6;++i) {
            const bool player=i<3;const unsigned slot=i%3==2 ? 1 : 0;const bool dual=i%3!=0;
            const auto& atlas=Pokerogue3DS::kHudIconAtlases[i];
            for(unsigned f=0;f<atlas.count;++f) {
                const int before=imageDraws;
                assert(renderer.drawHudTypeIcon(atlas.frames[f].key,player,slot,dual,1.4f,2.7f));
                assert(imageDraws==before+1 && imageScaleX==1 && imageScaleY==1);
            }
        }
        hudSubs[0].width=19;assert(!renderer.drawHudTypeIcon("FIRE",true,0,false,0,0));hudSubs[0].width=20;
        assert(renderer.drawHudTypeIcon("Fire",true,0,false,0,0));
        assert(!renderer.drawHudIndicator(nullptr,false,0,0));
        assert(!renderer.drawHudIndicator("NONE",false,0,0));
        assert(!renderer.drawHudIndicator("owned",false,0,0));
        assert(!renderer.drawHudIndicator("burn",true,0,0));
        hudMissing=true;assert(!renderer.drawHudIndicator("burn",false,0,0));hudMissing=false;
        for(unsigned i=6;i<8;++i) {
            const auto& atlas=Pokerogue3DS::kHudIconAtlases[i];
            for(unsigned f=0;f<atlas.count;++f) {
                const int before=imageDraws;
                assert(renderer.drawHudIndicator(atlas.frames[f].key,i==7,1.4f,2.7f));
                assert(imageDraws==before+1 && imageScaleX==1 && imageScaleY==1);
            }
        }
        hudSubs[6].width=21;assert(!renderer.drawHudIndicator("burn",false,0,0));hudSubs[6].width=22;
        assert(renderer.drawHudIndicator("burn",false,0,0));
        const int beforeBar=imageDraws;
        assert(renderer.drawHudBar(false,false,0,0,0));assert(imageDraws==beforeBar);
        assert(renderer.drawHudBar(false,false,-1,0,0));assert(imageDraws==beforeBar);
        assert(!renderer.drawHudBar(false,false,std::numeric_limits<float>::quiet_NaN(),0,0));
        assert(!renderer.drawHudBar(false,false,std::numeric_limits<float>::infinity(),0,0));
        hudMissing=true;assert(!renderer.drawHudBar(false,false,1,0,0));hudMissing=false;
        assert(renderer.drawHudBar(false,false,1,0,0));assert(imageWidth==48 && imageHeight==2 && imageTop==1);
        assert(renderer.drawHudBar(false,false,0.5f,0,0));assert(imageWidth==24 && std::fabs(imageTop-2.0f/3)<0.0001f);
        assert(renderer.drawHudBar(false,false,0.25f,0,0));assert(imageWidth==12 && std::fabs(imageTop-1.0f/3)<0.0001f);
        assert(renderer.drawHudBar(false,false,0.0001f,0,0));assert(imageWidth==1);
        assert(renderer.drawHudBar(false,true,2,0,0));assert(imageWidth==86 && imageHeight==4);
        assert(renderer.drawHudBar(true,false,0.5f,0,0));assert(imageWidth==42 && imageHeight==2 && imageScaleX==1 && imageScaleY==1);
        hudSubs[8].width=47;assert(!renderer.drawHudBar(false,false,1,0,0));hudSubs[8].width=48;
        assert(renderer.drawHudBar(false,false,1,0,0));
        assert(!renderer.drawHudGraphic(nullptr,"0",0,0));
        assert(!renderer.drawHudGraphic("numbers",nullptr,0,0));
        assert(!renderer.drawHudGraphic("unknown","0",0,0));
        assert(!renderer.drawHudGraphic("numbers","bad",0,0));
        hudMissing=true;assert(!renderer.drawHudGraphic("numbers","0",0,0));hudMissing=false;
        for(unsigned i=11;i<17;++i) {
            const auto& atlas=Pokerogue3DS::kHudIconAtlases[i];
            for(unsigned f=0;f<atlas.count;++f) {
                const int before=imageDraws;
                assert(renderer.drawHudGraphic(atlas.key,atlas.frames[f].key,1.4f,2.7f));
                assert(imageDraws==before+1 && imageScaleX==1 && imageScaleY==1);
            }
        }
        hudSubs[11].width=87;assert(!renderer.drawHudGraphic("numbers","0",0,0));hudSubs[11].width=88;
        assert(renderer.drawHudGraphic("numbers","0",0,0));
        assert(!renderer.setWindowStyle(2)); // Never free a texture before GPU submission.
        {
            Pokerogue3DS::BattleHudPresenter hud;
            Pokerogue3DS::ResolvedPokemon actor{};
            actor.actorIdentityResolved=true;actor.dex=1;actor.level=10;
            actor.localizedName="Bulbasaur";actor.battleState.pokemonId=11;
            actor.battleState.maxHp=20;actor.battleState.hp=20;actor.totalExperience=125;
            hud.draw(renderer,actor,true,258,146);
            assert(hud.displayedExperience()==125);
            actor.totalExperience=225;hud.draw(renderer,actor,true,258,146);
            assert(hud.displayedExperience()==225); // Timestamp-less calls project instantly.
            actor.battleState.pokemonId=12;actor.totalExperience=1000;
            hud.draw(renderer,actor,true,258,146);assert(hud.displayedExperience()==1000);
            actor.totalExperience=0;hud.draw(renderer,actor,true,258,146);assert(hud.displayedExperience()==0);
            actor.totalExperience=10;hud.draw(renderer,actor,true,258,146);assert(hud.displayedExperience()==10);
            hud.resetExperienceDisplay();hud.draw(renderer,actor,true,258,146);assert(hud.displayedExperience()==10);
            // Real canonical growth thresholds and elapsed time reach the same final EXP.
            const auto* species=PokerogueContent::findSpeciesByDex(actor.dex);assert(species);
            uint32_t base5=0,next5=0,base7=0,next7=0;
            assert(Pokerogue3DS::pokemonTotalExperienceForLevel(species->growthRate,5,base5)==Pokerogue3DS::PokemonExperienceResult::Ok);
            assert(Pokerogue3DS::pokemonTotalExperienceForLevel(species->growthRate,6,next5)==Pokerogue3DS::PokemonExperienceResult::Ok);
            assert(Pokerogue3DS::pokemonTotalExperienceForLevel(species->growthRate,7,base7)==Pokerogue3DS::PokemonExperienceResult::Ok);
            assert(Pokerogue3DS::pokemonTotalExperienceForLevel(species->growthRate,8,next7)==Pokerogue3DS::PokemonExperienceResult::Ok);
            actor.level=5;actor.totalExperience=base5+(next5-base5)/4;hud.resetExperienceDisplay();
            hud.draw(renderer,actor,true,258,146,false,0,1);assert(hud.displayedExperience()==actor.totalExperience);
            actor.level=7;actor.totalExperience=base7+(next7-base7)/3;
            hud.draw(renderer,actor,true,258,146,false,0,10);assert(hud.displayedExperience()<actor.totalExperience);
            hud.draw(renderer,actor,true,258,146,false,0,10000);assert(hud.displayedExperience()==actor.totalExperience);
            const int hudFreeBefore=sheetFree;
            hud.clear(&renderer);assert(sheetFree==hudFreeBefore && hud.displayedExperience()==0);
            renderer.endFrame(); // The next SYNCDRAW drains the retired HUD sheet.
        }
        assert(renderer.setWindowStyle(2) && renderer.windowStyle()==2);

        renderer.beginFrame();
        const int beforeRetire=sheetFree;
        renderer.retireSpriteSheet(reinterpret_cast<C2D_SpriteSheet>(&token));
        renderer.retireSpriteSheet(nullptr);
        assert(sheetFree==beforeRetire); // Queued draws still own this texture.
        renderer.endFrame();
        assert(sheetFree==beforeRetire); // Submission is not GPU completion.
        renderer.beginFrame();
        assert(sheetFree==beforeRetire+1); // SYNCDRAW completed prior users.
        renderer.endFrame();

        {
            Pokerogue3DS::PokemonIconPresenter icons;
            const int before=iconLoads,freeBefore=sheetFree;
            for(unsigned frame=0;frame<2;++frame) {
                renderer.beginFrame();renderer.beginTop();
                for(unsigned page=0;page<iconPageCount;++page) {
                    const Pokerogue3DS::PokemonIconDefinition* row=nullptr;
                    for(const auto& candidate:Pokerogue3DS::kPokemonIcons)
                        if(candidate.page==page) {row=&candidate;break;}
                    assert(row && icons.draw(renderer,row->dex,row->formIndex,0,0));
                }
                assert(iconLoads==before+int(iconPageCount)); // No repeated frame I/O.
                assert(sheetFree==freeBefore); // No queued GPU texture freed.
                renderer.endFrame();
            }
            renderer.beginFrame(); // Wait before releasing the setup working set.
            icons.clear();assert(sheetFree==freeBefore+int(iconPageCount));
            renderer.endFrame();
        }

        {
            Pokerogue3DS::PokemonIconPresenter compact(true);
            const int loadsBefore=compactIconLoads,normalBefore=iconLoads,freeBefore=sheetFree;
            renderer.beginFrame();renderer.beginBottom();
            const auto& first=Pokerogue3DS::kPokemonIcons[0];
            assert(!compact.draw(renderer,first.dex,first.formIndex,NAN,0));
            assert(!compact.draw(renderer,first.dex,first.formIndex,0,INFINITY));
            assert(!compact.draw(renderer,first.dex,first.formIndex,0,0,NAN));
            assert(!compact.draw(renderer,first.dex,first.formIndex,0,0,0));
            assert(!compact.draw(renderer,first.dex,first.formIndex,0,0,1,NAN));
            assert(!compact.draw(renderer,first.dex,first.formIndex,0,0,1,0));
            assert(!compact.draw(renderer,first.dex,first.formIndex,0,0,1,-1));
            assert(compactIconLoads==loadsBefore);
            renderer.endFrame();
            for(unsigned frame=0;frame<2;++frame) {
                renderer.beginFrame();renderer.beginBottom();
                for(unsigned page=0;page<iconPageCount;++page) {
                    const Pokerogue3DS::PokemonIconDefinition* row=nullptr;
                    for(const auto& candidate:Pokerogue3DS::kPokemonIcons)
                        if(candidate.page==page) {row=&candidate;break;}
                    assert(row && compact.draw(renderer,row->dex,row->formIndex,22,40));
                    assert(imageWidth==row->width/2 && imageHeight==row->height/2);
                    assert(imageScaleX==1 && imageScaleY==1);
                }
                assert(compactIconLoads==loadsBefore+int(iconPageCount));
                assert(iconLoads==normalBefore);
                renderer.endFrame();
            }
            renderer.beginFrame();renderer.beginBottom();
            assert(!compact.draw(renderer,65535,65535,0,0));
            assert(compactIconLoads==loadsBefore+int(iconPageCount));
            compact.clear(&renderer);
            assert(sheetFree==freeBefore); // Retirement waits for a GPU fence.
            renderer.endFrame();renderer.beginFrame();
            assert(sheetFree==freeBefore+int(iconPageCount));
            renderer.endFrame();
        }

        const int fontsBeforeClose=fontFree;
        const int buffersBeforeClose=bufferFree;
        const int sheetsBeforeClose=sheetFree;
        renderer.fini();assert(fontFree==fontsBeforeClose+4);assert(bufferFree==buffersBeforeClose+2);assert(sheetFree==sheetsBeforeClose+19); // Window, localized labels, six type variants and two indicator sheets.
        assert(!renderer.isInitialized());assert(renderer.textLineHeight(0.5f)==0);
        assert(renderer.init() && renderer.windowStyle()==1);renderer.fini();
    }
}

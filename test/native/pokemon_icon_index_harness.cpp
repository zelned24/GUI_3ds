#include "content/PokemonAppearanceAssets.hpp"
#include "content/PokemonIcons.hpp"
#include "runtime/PokemonAtlasPresenter.hpp"
#include "runtime/PokemonIconPresenter.hpp"
#include "runtime/ItemIconPresenter.hpp"
#include <limits>
#include "storage/NativeStarterCandyProfile.hpp"
#include "runtime/TypePresentation.hpp"
#include "runtime/BattleHudGeometry.hpp"
#include "runtime/Utf8Abbreviation.hpp"
#include "content/TypeLabels.hpp"
#include "content/HudTypeIcons.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include <cassert>
#include <initializer_list>
using namespace Pokerogue3DS;
// Host doubles verify cache ownership; they do not prove GPU behavior.
static unsigned iconLoads=0,iconDraws=0,iconFilters=0,iconFrees=0,iconRetired=0;
static bool failIconLoad=true;
static C3D_Tex iconTexture{};
static Tex3DS_SubTexture iconSubtexture{512,512,0,0,1,1};
C2D_SpriteSheet C2D_SpriteSheetLoad(const char*) {++iconLoads;return failIconLoad ? nullptr : &iconTexture;}
C2D_Image C2D_SpriteSheetGetImage(C2D_SpriteSheet sheet,size_t index) {
    assert(sheet==&iconTexture && index==0);return {&iconTexture,&iconSubtexture};
}
void C2D_SpriteSheetFree(C2D_SpriteSheet sheet) {assert(sheet==&iconTexture);++iconFrees;}
void C3D_TexSetFilter(C3D_Tex* texture,GPU_TEXTURE_FILTER_PARAM magnify,GPU_TEXTURE_FILTER_PARAM minify) {
    assert(texture==&iconTexture && magnify==GPU_NEAREST && minify==GPU_NEAREST);++iconFilters;
}
Renderer2D::Renderer2D() {}
Renderer2D::~Renderer2D() {}
void Renderer2D::retireSpriteSheet(C2D_SpriteSheet sheet) {assert(sheet==&iconTexture);++iconRetired;}
static Renderer2D::AtlasFrame lastIconFrame{};
static float lastIconX=0,lastIconY=0,lastIconWidth=0,lastIconHeight=0,lastIconOpacity=0;
void Renderer2D::drawAtlasFrame(C2D_Image,const AtlasFrame& frame,float x,float y,float width,float height,float opacity,uint32_t) {
    ++iconDraws;lastIconFrame=frame;lastIconX=x;lastIconY=y;
    lastIconWidth=width;lastIconHeight=height;lastIconOpacity=opacity;
}
int main() {
    NativeStarterCandyRecord shinyRecord;
    assert(nativeCaughtShinyVariants(shinyRecord)==0);
    shinyRecord.caught=true;
    for(unsigned mask=1;mask<8;++mask) {
        shinyRecord.caughtAppearanceAttr=uint8_t(2u | (mask<<4));
        assert(nativeCaughtShinyVariants(shinyRecord)==mask);
        shinyRecord.caught=false;assert(nativeCaughtShinyVariants(shinyRecord)==0);shinyRecord.caught=true;
    }
    for(unsigned attr:{0u,1u,2u,17u,33u,65u,128u,255u}) {
        shinyRecord.caughtAppearanceAttr=uint8_t(attr);
        assert(nativeCaughtShinyVariants(shinyRecord)==0);
    }

    assert(pokemonIconIndexOrdered());
    for(const auto& row:kPokemonIcons) assert(findPokemonIcon(row.dex,row.formIndex)==&row);
    assert(findPokemonIcon(0,0)==nullptr);
    assert(findPokemonIcon(1,0xffff)==nullptr);
    Renderer2D renderer;
    {
        PokemonIconPresenter presenter;
        assert(!presenter.draw(renderer,1,0,0,0));
        assert(!presenter.draw(renderer,1,0,0,0));
        assert(iconLoads==1 && iconDraws==0);
        presenter.clear();failIconLoad=false;
        assert(presenter.draw(renderer,1,0,0,0));
        assert(presenter.draw(renderer,1,0,0,0));
        assert(iconLoads==2 && iconDraws==2 && iconFilters==1);
        assert(!presenter.draw(renderer,0,0,0,0) && iconLoads==2);
        presenter.clear(&renderer);
        assert(iconRetired==1 && iconFrees==0);
        assert(presenter.draw(renderer,1,0,0,0) && iconLoads==3);
    }
    assert(iconFrees==1);
    // Item canvases keep pinned trim geometry and draw at integer destinations.
    {
        ItemIconPresenter items;
        const auto& item=kItemIconFrames[0];
        const unsigned loads=iconLoads,draws=iconDraws,frees=iconFrees,retired=iconRetired;
        const float nan=std::numeric_limits<float>::quiet_NaN();
        const float inf=std::numeric_limits<float>::infinity();
        assert(!items.draw(renderer,item.key,nan,0,32));
        assert(!items.draw(renderer,item.key,0,inf,32));
        for(float size:{0.0f,-1.0f,nan,inf}) assert(!items.draw(renderer,item.key,0,0,size));
        for(float opacity:{0.0f,-1.0f,nan,inf}) assert(!items.draw(renderer,item.key,0,0,32,opacity));
        assert(!items.draw(renderer,nullptr,0,0,32));
        assert(!items.draw(renderer,"missing-test-item",0,0,32));
        assert(iconLoads==loads && iconDraws==draws);
        failIconLoad=true;
        assert(!items.draw(renderer,item.key,10.25f,11.75f,32));
        failIconLoad=false;
        assert(items.draw(renderer,item.key,10.25f,11.75f,32,2));
        assert(iconLoads==loads+2 && iconDraws==draws+1);
        assert(lastIconX==10 && lastIconY==12 && lastIconWidth==32 && lastIconHeight==32);
        assert(lastIconOpacity==1);
        assert(lastIconFrame.x==item.x && lastIconFrame.y==item.y);
        assert(lastIconFrame.width==item.width && lastIconFrame.height==item.height);
        assert(lastIconFrame.sourceWidth==item.sourceWidth && lastIconFrame.sourceHeight==item.sourceHeight);
        assert(lastIconFrame.trimX==item.trimX && lastIconFrame.trimY==item.trimY);
        assert(items.draw(renderer,item.key,10,12,32,0.35f));
        assert(iconLoads==loads+2 && lastIconOpacity==0.35f);
        items.clear(&renderer);
        assert(iconRetired==retired+1 && iconFrees==frees);
        items.clear(&renderer);
        assert(iconRetired==retired+1);
        assert(items.draw(renderer,item.key,10,12,32) && iconLoads==loads+3);
        items.clear();
        assert(iconFrees==frees+1);
    }

    // Generated physical appearances resolve by exact identity; never fallback to another variant/facing.
    for(const auto& asset:kPokemonAppearanceAssets) {
        assert(findPokemonAppearanceAsset(asset.baseKey,asset.back,asset.female,asset.variant,asset.shiny)==&asset);
        assert(findPokemonAppearanceAsset(asset.baseKey,asset.back,asset.female,3)==nullptr);
        assert(asset.atlasKey && *asset.atlasKey);
    }
    assert(findPokemonAppearanceAsset(nullptr,false,false,0)==nullptr);
    assert(findPokemonAppearanceAsset("unknown-test-identity",false,false,0)==nullptr);

    assert(!std::strcmp(hudLevelDigitAtlas(true,9,10),"numbers"));
    assert(!std::strcmp(hudLevelDigitAtlas(true,10,10),"numbers_red"));
    assert(!std::strcmp(hudLevelDigitAtlas(true,11,10),"numbers_red"));
    assert(!std::strcmp(hudLevelDigitAtlas(false,200,10),"numbers"));
    assert(!std::strcmp(hudLevelDigitAtlas(true,200,0),"numbers"));

    char out[128];float width;
    char tiny[1]={'x'};assert(!abbreviateUtf8("AB",tiny,1,3,false,[](const char*){return 1.0f;},width) && !tiny[0]);
    const auto measure=[](const char* value) {float w=0;while(*value) {uint32_t cp;unsigned n=utf8CodePoint(value,cp);assert(n);w+=cp=='W' ? 2 : 1;value+=n;}return w;};
    assert(abbreviateUtf8("",out,sizeof(out),1,false,measure,width) && !*out);
    assert(abbreviateUtf8("ABCDE",out,sizeof(out),3,false,measure,width) && !std::strcmp(out,"AB."));
    assert(abbreviateUtf8("Évolí",out,sizeof(out),3,false,measure,width) && !std::strcmp(out,"Év."));
    assert(abbreviateUtf8("😀ABCD",out,sizeof(out),3,false,measure,width) && !std::strcmp(out,"😀A."));
    assert(abbreviateUtf8("Nidoranâ™‚",out,sizeof(out),20,true,measure,width) && !std::strcmp(out,"Nidoran"));
    assert(abbreviateUtf8("AB .CDEF",out,sizeof(out),4,false,measure,width) && !std::strcmp(out,"AB."));
    assert(abbreviateUtf8("ABã€€CDEF",out,sizeof(out),3,false,measure,width) && !std::strcmp(out,"AB."));
    char small[4];assert(abbreviateUtf8("ÉABCDE",small,sizeof(small),3,false,measure,width) && !std::strcmp(small,"É."));
    for(const char* invalid:{"\xc0\xaf","\xed\xa0\x80","\xf4\x90\x80\x80","\xe2\x99"})
        assert(!abbreviateUtf8(invalid,out,sizeof(out),3,false,measure,width) && !*out);

    assert(bossDividerPixel(100,3,1,86)==28 && bossDividerPixel(100,3,2,86)==57);
    assert(bossDividerPixel(100,2,1,86)==43);
    assert(bossDividerPixel(0,2,1,86)==0 && bossDividerPixel(100,1,1,86)==0);
    assert(bossDividerPixel(100,2,0,86)==0 && bossDividerPixel(100,2,2,86)==0);
    for(unsigned hp=1;hp<=100;++hp) for(unsigned segments=2;segments<=16;++segments) {
        unsigned previous=0;
        for(unsigned s=1;s<segments;++s) {
            const unsigned pixel=bossDividerPixel(hp,segments,s,86);
            assert(pixel<86 && pixel>=previous);previous=pixel;
        }
    }

    std::string key;const char* first=nullptr;const char* second=nullptr;
    for(const auto& species:PokerogueContent::kSpecies) {
        assert(PokemonAtlasPresenter::resolveAtlasKey(species.dex,nullptr,key));
        assert(key==std::to_string(species.dex));
        assert(canonicalPresentationTypes(species.dex,nullptr,first,second));
        assert(first==species.type1);
        assert(PokemonAtlasPresenter::resolveAtlasKey(species.dex,"",key));
        assert(key==std::to_string(species.dex));
    }
    for(const auto& form:PokerogueContent::kForms) {
        uint16_t owner=0;
        for(const auto& species:PokerogueContent::kSpecies)
            if(!std::strcmp(species.id,form.speciesId)) owner=species.dex;
        assert(owner);
        assert(PokemonAtlasPresenter::resolveAtlasKey(owner,form.id,key));
        assert(key==form.atlasKey);
        assert(canonicalPresentationTypes(owner,form.id,first,second));
        assert(first==form.type1);
        if(form.type2 && *form.type2 && !typeIEquals(form.type2,"NONE") && !typeIEquals(form.type1,form.type2)) assert(second==form.type2);
        else assert(!second);
        uint16_t different=owner==1 ? 4 : 1;
        assert(!PokemonAtlasPresenter::resolveAtlasKey(different,form.id,key) && key.empty());
        assert(!canonicalPresentationTypes(different,form.id,first,second) && !first && !second);
    }
    assert(!canonicalPresentationTypes(0,nullptr,first,second) && !first && !second);
    assert(!canonicalPresentationTypes(1,"missing_form",first,second) && !first && !second);
    key="old";assert(!PokemonAtlasPresenter::resolveAtlasKey(0,nullptr,key) && key.empty());
    key="old";assert(!PokemonAtlasPresenter::resolveAtlasKey(65535,nullptr,key) && key.empty());
    key="old";assert(!PokemonAtlasPresenter::resolveAtlasKey(1,"missing_form",key) && key.empty());

    for(const auto& species:PokerogueContent::kSpecies) {
        assert(canonicalPresentationTypes(species.dex,nullptr,first,second));
        assert(findTypeLabel(first));if(second) assert(findTypeLabel(second));
    }
    for(const auto& form:PokerogueContent::kForms) {
        assert(findTypeLabel(form.type1));
        if(form.type2 && *form.type2 && !typeIEquals(form.type2,"NONE")) assert(findTypeLabel(form.type2));
    }
    assert(!findTypeLabel("NONE") && !findTypeLabel("missing") && !findTypeLabel(nullptr));
    for(unsigned i=0;i<6;++i) {
        for(const auto& species:PokerogueContent::kSpecies) {
            assert(canonicalPresentationTypes(species.dex,nullptr,first,second));
            assert(findHudTypeFrame(i,first));if(second) assert(findHudTypeFrame(i,second));
        }
        for(const auto& form:PokerogueContent::kForms) {
            assert(findHudTypeFrame(i,form.type1));
            if(form.type2 && *form.type2 && !typeIEquals(form.type2,"NONE")) assert(findHudTypeFrame(i,form.type2));
        }
    }
    assert(!findHudTypeFrame(6,"FIRE"));
    const unsigned pages=sizeof(kPokemonIconPages)/sizeof(kPokemonIconPages[0]);
    const unsigned count=sizeof(kPokemonIcons)/sizeof(kPokemonIcons[0]);
    assert(pages>0 && count>0);
    for(unsigned i=0;i<count;++i) {
        const auto& icon=kPokemonIcons[i];
        assert(PokerogueContent::findSpeciesByDex(icon.dex));
        assert(icon.page<pages && icon.width>0 && icon.height>0);
        assert(icon.x+icon.width<=512 && icon.y+icon.height<=512);
        if(i) assert(kPokemonIcons[i-1].dex<icon.dex ||
            (kPokemonIcons[i-1].dex==icon.dex && kPokemonIcons[i-1].formIndex<icon.formIndex));
    }
    for(const auto& species:PokerogueContent::kSpecies) if(species.starterEligible) {
        bool found=false;
        for(const auto& icon:kPokemonIcons) if(icon.dex==species.dex && !icon.formIndex) {found=true;break;}
        assert(found);
    }
    for(unsigned dex:{1u,6u,642u,8901u}) {
        bool found=false;for(const auto& icon:kPokemonIcons) if(icon.dex==dex && !icon.formIndex) found=true;
        assert(found);
    }
    bool therian=false;for(const auto& icon:kPokemonIcons) if(icon.dex==642 && icon.formIndex==1) therian=true;
    assert(therian);
}

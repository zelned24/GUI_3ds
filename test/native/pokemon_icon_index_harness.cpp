#include "content/PokemonAppearanceAssets.hpp"
#include "content/PokemonIcons.hpp"
#include "content/AppearanceIcons.hpp"
#include "content/AppearanceIconIdentities.hpp"
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
static bool malformedIconImage=false;
static C3D_Tex iconTexture{};
static Tex3DS_SubTexture iconSubtexture{512,512,0,0,1,1};
C2D_SpriteSheet C2D_SpriteSheetLoad(const char* path) {
    ++iconLoads;
    const bool compact=std::strstr(path,"appearance-icons-compact-")!=nullptr;
    iconSubtexture.width=iconSubtexture.height=compact ? 256 : 512;
    if(std::strstr(path,"/icon-tiles/")) {iconSubtexture.width=64;iconSubtexture.height=32;}
    return failIconLoad ? nullptr : &iconTexture;
}
C2D_Image C2D_SpriteSheetGetImage(C2D_SpriteSheet sheet,size_t index) {
    assert(sheet==&iconTexture && index==0);return {malformedIconImage ? nullptr : &iconTexture,&iconSubtexture};
}
void C2D_SpriteSheetFree(C2D_SpriteSheet sheet) {assert(sheet==&iconTexture);++iconFrees;}
void C3D_TexSetFilter(C3D_Tex* texture,GPU_TEXTURE_FILTER_PARAM magnify,GPU_TEXTURE_FILTER_PARAM minify) {
    assert(texture==&iconTexture && magnify==GPU_NEAREST && minify==GPU_NEAREST);++iconFilters;
}
Renderer2D::Renderer2D() {}
Renderer2D::~Renderer2D() {}
void Renderer2D::retireSpriteSheet(C2D_SpriteSheet sheet) {assert(sheet==&iconTexture);++iconRetired;}
static Renderer2D::AtlasFrame lastIconFrame{};
static uint32_t lastIconTint=0;
static float lastIconX=0,lastIconY=0,lastIconWidth=0,lastIconHeight=0,lastIconOpacity=0;
void Renderer2D::drawAtlasFrame(C2D_Image,const AtlasFrame& frame,float x,float y,float width,float height,float opacity,uint32_t tint) {
    ++iconDraws;lastIconTint=tint;lastIconFrame=frame;lastIconX=x;lastIconY=y;
    lastIconWidth=width;lastIconHeight=height;lastIconOpacity=opacity;
}
static unsigned badgeRects=0,badgeTexts=0;
static float badgeX=0,badgeY=0;
bool Renderer2D::drawTypeLabel(const char*,float,float,float,float) {return false;}
void Renderer2D::drawRect(float x,float y,float,float,uint32_t,float) {++badgeRects;badgeX=x;badgeY=y;}
float Renderer2D::textInkHeight(float) const {return 8;}
float Renderer2D::drawTextFitted(const char*,float x,float y,float size,float,uint32_t,float*) {++badgeTexts;badgeX=x;badgeY=y;return size;}
int main() {
    {
        Renderer2D badgeRenderer;
        const float nan=std::numeric_limits<float>::quiet_NaN();
        drawTypeBadge(badgeRenderer,"WATER",nan,0,32,14);
        drawTypeBadge(badgeRenderer,"WATER",0,0,6,14);
        drawTypeBadge(badgeRenderer,"WATER",0,0,32,14,nan);
        assert(badgeRects==0 && badgeTexts==0);
        drawTypeBadge(badgeRenderer,"WATER",10.25f,11.75f,32,14);
        assert(badgeRects==1 && badgeTexts==1 && badgeX==13 && badgeY==15);
        drawTypeBadge(badgeRenderer,"WATER",0,0,32,7);
        assert(badgeRects==2 && badgeTexts==1); // Never overflow a too-short badge.
    }
    uint64_t previousIdentity=0;
    for(const auto& identity:kAppearanceIconIdentities) {
        const auto key=appearanceIconIdentityKey(identity.dex,identity.formIndex,identity.appearance);
        assert(key>previousIdentity);previousIdentity=key;
        assert(findAppearanceIconIdentity(identity.dex,identity.formIndex,(identity.appearance&8)!=0,(identity.appearance&4)!=0,identity.appearance&3)==&identity);
        assert(appearanceIconPhysicalFrame(&identity));
    }
    assert(!findAppearanceIconIdentity(0,0,false,false,0));
    assert(!resolvePokemonIcon(0,nullptr,false,false,false,0).normalIconAllowed);
    assert(!resolvePokemonIcon(1,"missing-form",false,false,false,0).normalIconAllowed);
    const auto invalidVariantIcon=resolvePokemonIcon(1,nullptr,true,false,false,1);
    assert(!invalidVariantIcon.appearance && !invalidVariantIcon.normalIconAllowed);

    assert(!findAppearanceIconIdentity(1,65535,false,false,0));
    assert(!findAppearanceIconIdentity(1,0,false,false,1));
    assert(!findAppearanceIconIdentity(1,0,false,true,3));
    assert(!appearanceIconPhysicalFrame(nullptr));
    const auto* rareIcon=findAppearanceIconIdentity(1,0,false,true,1);
    assert(rareIcon && rareIcon->upstreamFallback);
    assert(!std::strcmp(appearanceIconPhysicalFrame(rareIcon)->sourceKey,"1/1.png"));
    const auto* shinyIcon=findAppearanceIconIdentity(1,0,false,true,0);
    assert(shinyIcon && !shinyIcon->upstreamFallback);
    assert(!std::strcmp(appearanceIconPhysicalFrame(shinyIcon)->sourceKey,"1/1s.png"));

    assert(!findAppearanceIcon(nullptr) && !findAppearanceIcon("") && !findAppearanceIcon("missing/icon.png"));
    for(std::size_t i=0;i<kAppearanceIconCount;++i) {
        const auto& frame=kAppearanceIconFrames[i];
        assert(findAppearanceIcon(frame.sourceKey)==&frame);
        assert(frame.page<sizeof(kAppearanceIconPages)/sizeof(kAppearanceIconPages[0]));
        assert(frame.width==40 && frame.height==30 && frame.x+frame.width<=512 && frame.y+frame.height<=512);
        if(i) assert(std::strcmp(kAppearanceIconFrames[i-1].sourceKey,frame.sourceKey)<0);
    }
    assert(findAppearanceIcon("1/1.png") && findAppearanceIcon("1/1s.png"));

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
        const auto normal=resolvePokemonIcon(1,nullptr,false,false,false,0);
        assert(presenter.retainNormalPages(renderer,&normal,1));
        assert(presenter.draw(renderer,1,0,0,0) && iconLoads==2);
        assert(!presenter.retainNormalPages(renderer,nullptr,1));
        assert(!presenter.retainNormalPages(renderer,&normal,19));
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
        assert(!items.draw(renderer,item.key,10.25f,11.75f,32) && iconLoads==loads+1);
        items.clear(&renderer); // Explicit recovery, never a repeated per-frame load.
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

    {
        ItemIconPresenter malformed;
        const auto& item=kItemIconFrames[0];
        const unsigned loads=iconLoads,draws=iconDraws,retired=iconRetired;
        malformedIconImage=true;
        assert(!malformed.draw(renderer,item.key,0,0,32));
        assert(iconLoads==loads+1 && iconDraws==draws && iconRetired==retired+1);
        malformedIconImage=false;
        assert(!malformed.draw(renderer,item.key,0,0,32) && iconLoads==loads+1);
        malformed.clear(&renderer);
        assert(malformed.draw(renderer,item.key,0,0,32) && iconLoads==loads+2);
        malformed.clear(&renderer);
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
        const auto knownIcon=resolvePokemonIcon(owner,form.id,true,false,true,0);
        assert(knownIcon.formIndex==form.upstreamFormIndex && !knownIcon.normalIconAllowed);
        assert(knownIcon.appearance==findAppearanceIconIdentity(owner,form.upstreamFormIndex,false,true,0));
        const auto legacyIcon=resolvePokemonIcon(owner,form.id,false,false,false,0);
        assert(legacyIcon.formIndex==form.upstreamFormIndex && legacyIcon.normalIconAllowed && !legacyIcon.appearance);
        const auto wrongOwnerIcon=resolvePokemonIcon(owner==1 ? 4 : 1,form.id,true,false,false,0);
        assert(!wrongOwnerIcon.appearance && !wrongOwnerIcon.normalIconAllowed);

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
    // Visible-team batch owns at most six distinct physical pages; draws do no I/O.
    {
        PokemonIconPresenter icons(true);
        const AppearanceIconIdentity* batch[7]{};unsigned found=0;
        for(const auto& identity:kAppearanceIconIdentities) {
            const auto* frame=appearanceIconPhysicalFrame(&identity);if(!frame) continue;bool unique=true;
            for(unsigned i=0;i<found;++i) if(appearanceIconPhysicalFrame(batch[i])->page==frame->page) unique=false;
            if(unique) batch[found++]=&identity;
            if(found==7) break;
        }
        assert(found==7);
        const unsigned loads=iconLoads,retired=iconRetired;
        assert(!icons.drawAppearance(renderer,batch[0],0,0));
        assert(!icons.prepareAppearances(renderer,batch,7) && iconLoads==loads);
        assert(icons.prepareAppearances(renderer,batch,6) && iconLoads==loads+6);
        for(unsigned i=0;i<6;++i) {
            assert(icons.drawAppearance(renderer,batch[i],10.25f,11.75f));
            assert(lastIconWidth==20 && lastIconHeight==15 && lastIconX==10 && lastIconY==12);
        }
        assert(iconLoads==loads+6);
        assert(icons.prepareAppearances(renderer,batch,6) && iconLoads==loads+6);
        batch[0]=batch[6];
        assert(icons.prepareAppearances(renderer,batch,6));
        assert(iconLoads==loads+7 && iconRetired==retired+1);
        icons.clear(&renderer);assert(iconRetired==retired+7);
        failIconLoad=true;
        assert(!icons.prepareAppearances(renderer,batch,1));
        assert(!icons.drawAppearance(renderer,batch[0],0,0));
        const unsigned failedLoads=iconLoads;
        failIconLoad=false;
        assert(!icons.prepareAppearances(renderer,batch,1) && iconLoads==failedLoads);
        icons.clear(&renderer);
        assert(icons.prepareAppearances(renderer,batch,1) && iconLoads==failedLoads+1);
        assert(!icons.drawAppearance(renderer,batch[0],std::numeric_limits<float>::quiet_NaN(),0));
        assert(icons.prepareAppearances(renderer,nullptr,0));
        assert(!icons.drawAppearance(renderer,batch[0],0,0));
        const unsigned retiredBeforeMalformed=iconRetired;
        malformedIconImage=true;
        assert(!icons.prepareAppearances(renderer,batch,1));
        assert(iconRetired==retiredBeforeMalformed+1);
        assert(!icons.drawAppearance(renderer,batch[0],0,0));
        const unsigned loadsAfterMalformed=iconLoads;
        malformedIconImage=false;
        assert(!icons.prepareAppearances(renderer,batch,1) && iconLoads==loadsAfterMalformed);
        icons.clear(&renderer);
        assert(icons.prepareAppearances(renderer,batch,1));
        icons.clear(&renderer);
    }

    {
        PokemonIconPresenter grid(true,18);
        const AppearanceIconIdentity* batch[19]{};unsigned count=0;
        for(const auto& identity:kAppearanceIconIdentities) {
            const auto* frame=appearanceIconPhysicalFrame(&identity);if(!frame) continue;
            bool unique=true;
            for(unsigned i=0;i<count;++i) if(appearanceIconPhysicalFrame(batch[i])->page==frame->page) unique=false;
            if(unique) batch[count++]=&identity;
            if(count==19) break;
        }
        assert(count==19);
        const unsigned before=iconLoads;
        assert(!grid.prepareAppearances(renderer,batch,19) && iconLoads==before);
        assert(grid.prepareAppearances(renderer,batch,18) && iconLoads==before+18);
        for(unsigned i=0;i<18;++i) {
            assert(grid.drawAppearance(renderer,batch[i],1.4f,2.7f,1,2));
            assert(lastIconWidth==40 && lastIconHeight==30 && lastIconX==1 && lastIconY==3);
            assert(!grid.drawAppearance(renderer,batch[i],0,0,1,0));
            assert(!grid.drawAppearance(renderer,batch[i],0,0,1,3));
        }
        assert(grid.prepareAppearances(renderer,batch,18) && iconLoads==before+18);
        grid.clear(&renderer);
    }

    {
        PokemonIconPresenter nativeGrid(true,18,true);
        const AppearanceIconIdentity* batch[]={findAppearanceIconIdentity(1,0,false,true,0)};
        assert(batch[0] && nativeGrid.prepareAppearances(renderer,batch,1));
        const unsigned loaded=iconLoads;
        assert(nativeGrid.drawAppearance(renderer,batch[0],1.4f,2.7f));
        assert(lastIconFrame.x==0 && lastIconFrame.y==0 && lastIconWidth==40 && lastIconHeight==30);
        assert(nativeGrid.drawAppearance(renderer,batch[0],0,0,1,1,0xff000000));
        assert(lastIconTint==0xff000000 && lastIconWidth==40 && lastIconHeight==30);
        assert(nativeGrid.prepareAppearances(renderer,batch,1) && iconLoads==loaded);
        nativeGrid.clear(&renderer);
    }

    {
        auto unknown=resolvePokemonIcon(1,nullptr,false,false,false,0);
        resolvePokemonDiscoveryIcon(unknown);
        assert(unknown.normalIconAllowed && unknown.appearance==findAppearanceIconIdentity(1,0,false,false,0));
        auto known=resolvePokemonIcon(1,nullptr,true,false,true,0);
        const auto* original=known.appearance;
        resolvePokemonDiscoveryIcon(known);
        assert(!known.normalIconAllowed && known.appearance==original);
        auto invalid=resolvePokemonIcon(0,nullptr,false,false,false,0);
        resolvePokemonDiscoveryIcon(invalid);assert(!invalid.appearance);
    }

    {
        PokemonIconPresenter dex(true,24,true);
        const AppearanceIconIdentity* entries[25]{};
        for(unsigned i=0;i<25;++i) entries[i]=findAppearanceIconIdentity(1,0,false,false,0);
        const unsigned before=iconLoads;
        assert(!dex.prepareAppearances(renderer,entries,25) && iconLoads==before);
        assert(dex.prepareAppearances(renderer,entries,24) && iconLoads==before+1);
        for(unsigned i=0;i<24;++i) assert(dex.drawAppearance(renderer,entries[i],0,0));
        assert(dex.prepareAppearances(renderer,nullptr,0));
        assert(!dex.drawAppearance(renderer,entries[0],0,0));
        dex.clear(&renderer);
    }

    // Reject degenerate/inverted UV rectangles before reporting a drawn icon.
    failIconLoad=false;malformedIconImage=false;
    for(unsigned invalid=0;invalid<4;++invalid) {
        Renderer2D renderer;PokemonIconPresenter icons;
        iconSubtexture.left=0;iconSubtexture.right=1;
        iconSubtexture.top=1;iconSubtexture.bottom=0;
        if(invalid==0) iconSubtexture.left=iconSubtexture.right;
        if(invalid==1) iconSubtexture.left=2;
        if(invalid==2) iconSubtexture.top=iconSubtexture.bottom;
        if(invalid==3) iconSubtexture.top=-1;
        const unsigned loads=iconLoads,draws=iconDraws,retired=iconRetired;
        assert(!icons.draw(renderer,1,0,0,0));
        assert(iconLoads==loads+1 && iconDraws==draws && iconRetired==retired+1);
        assert(!icons.draw(renderer,1,0,0,0) && iconLoads==loads+1);
        icons.clear(&renderer);
        iconSubtexture.left=0;iconSubtexture.right=1;
        iconSubtexture.top=1;iconSubtexture.bottom=0;
        assert(icons.draw(renderer,1,0,0,0) && iconLoads==loads+2);
        icons.clear(&renderer);
    }
}

#include "content/PokemonIcons.hpp"
#include "runtime/PokemonAtlasPresenter.hpp"
#include "runtime/TypePresentation.hpp"
#include "runtime/BattleHudGeometry.hpp"
#include "runtime/Utf8Abbreviation.hpp"
#include "content/TypeLabels.hpp"
#include "content/HudTypeIcons.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include <cassert>
#include <initializer_list>
using namespace Pokerogue3DS;
int main() {
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
    assert(abbreviateUtf8("Nidoran♂",out,sizeof(out),20,true,measure,width) && !std::strcmp(out,"Nidoran"));
    assert(abbreviateUtf8("AB .CDEF",out,sizeof(out),4,false,measure,width) && !std::strcmp(out,"AB."));
    assert(abbreviateUtf8("AB　CDEF",out,sizeof(out),3,false,measure,width) && !std::strcmp(out,"AB."));
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

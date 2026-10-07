#include "content/PokemonIcons.hpp"
#include "runtime/PokemonAtlasPresenter.hpp"
#include "runtime/TypePresentation.hpp"
#include "content/TypeLabels.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include <cassert>
#include <initializer_list>
using namespace Pokerogue3DS;
int main() {
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

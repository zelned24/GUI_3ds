#include "content/PokemonIcons.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include <cassert>
#include <initializer_list>
using namespace Pokerogue3DS;
int main() {
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

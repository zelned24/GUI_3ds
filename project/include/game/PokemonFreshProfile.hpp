#pragma once

#include "content/PokerogueRuntimeContent.hpp"
#include "game/PokemonBattleState.hpp"
#include "game/PokerogueRngAdapter.hpp"

namespace Pokerogue3DS {

enum class PokemonFreshProfileResult : uint8_t {
    Ok = 0, MissingSpecies, NotDefaultStarter, InvalidStarterOrder
};

// Reproduces GameData.initDexData()'s isolated executeWithSeedOffset(0,
// "default") nature stream, preserving the pinned defaultStarterSpecies order.
inline PokemonFreshProfileResult pokemonFreshProfileNature(
    uint16_t speciesDex, PokemonNature& output) {
    const auto* species = PokerogueContent::findSpeciesByDex(speciesDex);
    if (!species) return PokemonFreshProfileResult::MissingSpecies;
    if (!species->freshProfileStarter) return PokemonFreshProfileResult::NotDefaultStarter;
    const uint8_t ordinal = species->freshProfileStarterOrdinal;
    if (ordinal >= PokerogueContent::kSpeciesCount)
        return PokemonFreshProfileResult::InvalidStarterOrder;

    static constexpr uint16_t kDefaultSeed[] = {
        'd', 'e', 'f', 'a', 'u', 'l', 't'
    };
    static constexpr PokemonNature kNeutralNatures[] = {
        PokemonNature::Hardy, PokemonNature::Docile, PokemonNature::Serious,
        PokemonNature::Bashful, PokemonNature::Quirky
    };
    PokerogueRngAdapter rng;
    rng.sow(kDefaultSeed, sizeof(kDefaultSeed) / sizeof(kDefaultSeed[0]));
    for (uint8_t index = 0; index <= ordinal; ++index) {
        const int32_t natureIndex = rng.pickIndex(
            static_cast<uint32_t>(sizeof(kNeutralNatures) / sizeof(kNeutralNatures[0])));
        if (natureIndex < 0 || natureIndex >= static_cast<int32_t>(sizeof(kNeutralNatures) / sizeof(kNeutralNatures[0])))
            return PokemonFreshProfileResult::InvalidStarterOrder;
        if (index == ordinal) output = kNeutralNatures[natureIndex];
    }
    return PokemonFreshProfileResult::Ok;
}

} // namespace Pokerogue3DS

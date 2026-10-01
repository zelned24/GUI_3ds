#pragma once

#include "content/PokerogueRuntimeContent.hpp"
#include "game/PokemonBattleState.hpp"
#include "game/PokerogueRngAdapter.hpp"

namespace Pokerogue3DS {

enum class PokemonObservedFormResult : uint8_t {
    Ok, MissingSpecies, MissingForm, SpeciesMismatch, UnresolvedForm, AttributeCapacityUnsupported
};
// Pokemon.getDexAttr -> GameData.getFormAttr: DEFAULT_FORM (128) shifted
// by the actual source form index. This preserves observation, not unlock rules.
inline PokemonObservedFormResult pokemonObservedDexFormAttr(uint16_t dex,
    const PokemonActorIdentity& actor, uint64_t& output) {
    const auto* species = PokerogueContent::findSpeciesByDex(dex);
    if (!species) return PokemonObservedFormResult::MissingSpecies;
    uint16_t index = 0;
    if (actor.formId && *actor.formId) {
        const auto* form = PokerogueContent::findFormById(actor.formId);
        if (!form) return PokemonObservedFormResult::MissingForm;
        const char* left = form->speciesId;
        const char* right = species->id;
        if (!left || !right) return PokemonObservedFormResult::SpeciesMismatch;
        while (*left && *right && *left == *right) { ++left; ++right; }
        if (*left != *right) return PokemonObservedFormResult::SpeciesMismatch;
        index = form->upstreamFormIndex;
    } else if (species->firstFormId && *species->firstFormId) {
        return PokemonObservedFormResult::UnresolvedForm;
    }
    if (index > 56) return PokemonObservedFormResult::AttributeCapacityUnsupported;
    output = uint64_t(128) << index;
    return PokemonObservedFormResult::Ok;
}

enum class PokemonFreshProfileResult : uint8_t {
    Ok = 0, MissingSpecies, NotDefaultStarter, InvalidStarterOrder
};

// addFriendship credits the root species, including a fusion's own root separately.
// This identifies the canonical ledger key; it does not create or persist a profile.
inline const PokerogueContent::Species* pokemonFriendshipStarterSpecies(uint16_t dex) {
    return pokemonRootSpecies(dex);
}

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

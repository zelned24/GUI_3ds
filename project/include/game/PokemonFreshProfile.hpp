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

enum class PokemonFormUnlockMaskResult : uint8_t {
    Ok, MissingSpecies, MissingPermission, UnsupportedPermission, AttributeCapacityUnsupported
};
// Form component of PokemonSpecies.getFullUnlocksData. This is an allowed
// mask, not the player's earned unlocks. Failure leaves output unchanged.
inline PokemonFormUnlockMaskResult pokemonObtainableFormMask(uint16_t dex, uint64_t& output) {
    const auto* species = PokerogueContent::findSpeciesByDex(dex);
    if (!species) return PokemonFormUnlockMaskResult::MissingSpecies;
    size_t formCount = 0;
    for (const auto& form : PokerogueContent::kForms) {
        const char* left = form.speciesId;
        const char* right = species->id;
        if (!left || !right) continue;
        while (*left && *right && *left == *right) { ++left; ++right; }
        if (!*left && !*right) ++formCount;
    }
    if (formCount <= 1) { output = uint64_t(128); return PokemonFormUnlockMaskResult::Ok; }
    uint64_t mask = 0;
    for (size_t index = 0; index < formCount; ++index) {
        const auto* form = PokerogueContent::findFormByUpstreamIndex(dex, static_cast<uint16_t>(index));
        if (!form) return PokemonFormUnlockMaskResult::MissingPermission;
        const PokerogueContent::FormPermission* permission = nullptr;
        for (const auto& entry : PokerogueContent::kFormPermissions) {
            const char* left = entry.formId;
            const char* right = form->id;
            if (!left || !right) continue;
            while (*left && *right && *left == *right) { ++left; ++right; }
            if (!*left && !*right) { permission = &entry; break; }
        }
        if (!permission) return PokemonFormUnlockMaskResult::MissingPermission;
        if (permission->isUnobtainable < 0) return PokemonFormUnlockMaskResult::UnsupportedPermission;
        if (permission->isUnobtainable) continue;
        if (index > 56) return PokemonFormUnlockMaskResult::AttributeCapacityUnsupported;
        mask |= uint64_t(128) << index;
    }
    output = mask;
    return PokemonFormUnlockMaskResult::Ok;
}

enum class PokemonCaptureFormUnlockResult : uint8_t {
    Ok, InvalidActor, InvalidSpecies, UnsupportedMask, UnsupportedReference
};
inline bool pokemonFormTextEquals(const char* left, const char* right) {
    if (!left || !right) return false;
    while (*left && *right && *left == *right) { ++left; ++right; }
    return !*left && !*right;
}
// GameData.setPokemonSpeciesCaught form component, for each recursive recipient.
inline PokemonCaptureFormUnlockResult pokemonCaptureFormUnlocks(uint16_t capturedDex,
    const PokemonActorIdentity& actor, uint16_t recipientDex, uint64_t& output) {
    const auto* original = PokerogueContent::findSpeciesByDex(capturedDex);
    const auto* recipient = PokerogueContent::findSpeciesByDex(recipientDex);
    if (!original || !recipient) return PokemonCaptureFormUnlockResult::InvalidSpecies;
    uint64_t observed = 0, allowed = 0;
    if (pokemonObservedDexFormAttr(capturedDex, actor, observed) != PokemonObservedFormResult::Ok)
        return PokemonCaptureFormUnlockResult::InvalidActor;
    if (pokemonObtainableFormMask(recipientDex, allowed) != PokemonFormUnlockMaskResult::Ok)
        return PokemonCaptureFormUnlockResult::UnsupportedMask;
    uint64_t unlocked = observed & allowed;
    const auto* capturedForm = actor.formId && *actor.formId
        ? PokerogueContent::findFormById(actor.formId) : nullptr;
    const uint16_t index = capturedForm ? capturedForm->upstreamFormIndex : 0;
    if (index) {
        if (pokemonFormTextEquals(original->id, "pikachu") && pokemonFormTextEquals(recipient->id, "pichu"))
            unlocked |= uint64_t(128);
        if (pokemonFormTextEquals(original->id, "urshifu")) {
            if (index == 2) unlocked |= uint64_t(128);
            else if (index == 3) unlocked |= uint64_t(256);
        } else if (pokemonFormTextEquals(original->id, "zygarde")) {
            if (index == 4) unlocked |= uint64_t(512);
            else if (index == 5) unlocked |= uint64_t(1024);
        } else {
            for (const auto& change : PokerogueContent::kFormChangeReferences)
                if (change.speciesDex == recipientDex && capturedForm &&
                    pokemonFormTextEquals(change.formKey, capturedForm->formKey)) {
                    unlocked |= uint64_t(128);
                    break;
                }
        }
    }
    // The current durable schema requires concrete form references. Report the
    // upstream recursive exceptions explicitly until that schema supports them.
    for (uint8_t i = 0; i <= 56; ++i) {
        if (!(unlocked & (uint64_t(128) << i))) continue;
        if (PokerogueContent::findFormByUpstreamIndex(recipientDex, i)) continue;
        if (!i && (!recipient->firstFormId || !*recipient->firstFormId)) continue;
        return PokemonCaptureFormUnlockResult::UnsupportedReference;
    }
    output = unlocked;
    return PokemonCaptureFormUnlockResult::Ok;
}

// StarterSelectUiHandler validates preferences using the actual caughtAttr,
// not our separate observedFormAttr. Caller must supply resolved unlock data.
enum class PokemonStarterFormResult : uint8_t {
    Ok, MissingSpecies, MissingForm, MissingPermission, UnsupportedPermission,
    NotSelectable, NotUnlocked, AttributeCapacityUnsupported
};
inline PokemonStarterFormResult pokemonValidateStarterForm(uint16_t dex,
    uint16_t formIndex, uint64_t unlockedCaughtAttr) {
    const auto* species = PokerogueContent::findSpeciesByDex(dex);
    if (!species) return PokemonStarterFormResult::MissingSpecies;
    const auto* form = PokerogueContent::findFormByUpstreamIndex(dex, formIndex);
    if (!form) return PokemonStarterFormResult::MissingForm;
    const PokerogueContent::FormPermission* permission = nullptr;
    for (const auto& entry : PokerogueContent::kFormPermissions) {
        const char* left = entry.formId;
        const char* right = form->id;
        if (!left || !right) continue;
        while (*left && *right && *left == *right) { ++left; ++right; }
        if (!*left && !*right) { permission = &entry; break; }
    }
    if (!permission) return PokemonStarterFormResult::MissingPermission;
    if (permission->isStarterSelectable < 0)
        return PokemonStarterFormResult::UnsupportedPermission;
    if (!permission->isStarterSelectable) return PokemonStarterFormResult::NotSelectable;
    if (formIndex > 56) return PokemonStarterFormResult::AttributeCapacityUnsupported;
    if (!(unlockedCaughtAttr & (uint64_t(128) << formIndex)))
        return PokemonStarterFormResult::NotUnlocked;
    return PokemonStarterFormResult::Ok;
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

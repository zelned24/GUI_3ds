#pragma once
#include "storage/NativeRunSave.hpp"
#include "storage/IntegritySha256.hpp"
#include "game/PokemonBattleState.hpp"
#include "game/PokemonExperience.hpp"
#include "game/PokemonFreshProfile.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace Pokerogue3DS {

struct NativeStarterCandyRecord {
    uint16_t speciesDex = 0; // Canonical SpeciesId, never a catalog offset.
    uint16_t candyCount = 0;
    uint32_t friendship = 0; // Starter progress, separate from Pokemon friendship.
    bool caught = false; // Exact canonical species; separate from starter/root progress.
    uint8_t costReduction = 0; // Pinned starterData.valueReduction, maximum two.
    bool passiveUnlocked = false;
    uint32_t natureAttr = 0; // Upstream bit n+1; zero means metadata unavailable.
    uint8_t dexIvs[6]{}; // Per-stat maxima, never actor IVs overwritten in place.
    uint8_t abilityAttr = 0; // Upstream AbilityAttr 1/2/4; zero is unavailable.
    uint8_t genderAttr = 0; // Upstream DexAttr.MALE/FEMALE 4/8; genderless adds neither.
    uint64_t observedFormAttr = 0; // Observed source forms, NOT starter unlocks.
    uint64_t unlockedFormAttr = 0; // Form component of upstream caughtAttr; legacy unavailable is zero.
    uint16_t preferredFormIndex = 65535; // No explicit preference; upstream default index zero.
    uint8_t observedAppearanceAttr = 0; // Upstream NON_SHINY/SHINY and variant bits; zero unknown.
    uint8_t caughtAppearanceAttr = 0; // Legacy profiles retain unknown, never inferred from caught.
    uint8_t preferredAbilityIndex = 255; // No explicit selection; preserve source default.
    uint8_t preferredNatureIndex = 255; // Legacy/default choice, separate from nature unlocks.
};

// Pokemon.getDexAttr appearance component. Unknown actors leave output zero.
inline bool nativePokemonAppearanceAttr(const PokemonActorIdentity& actor,uint8_t& output) {
    output=0;
    if(!actor.appearanceResolved) return !actor.shiny && !actor.shinyVariant;
    if(actor.shinyVariant>2 || (!actor.shiny && actor.shinyVariant)) return false;
    output=static_cast<uint8_t>((actor.shiny ? 2u : 1u) | (16u << actor.shinyVariant));
    return true;
}

// Pinned game-data.ts/GameData.getSpeciesDefaultDexAttrProps. Shiny is the
// default when caught, with the highest caught variant. Unknown legacy metadata
// cannot supply an appearance. Output is published only for valid caught bits.
inline bool nativeStarterDefaultAppearance(const NativeStarterCandyRecord& record,
    bool& shiny, uint8_t& variant, bool defaultIsShiny = true) {
    const uint8_t attr = record.caughtAppearanceAttr;
    if (!record.caught || !attr || (attr & ~0x73u) || !(attr & 3u) || !(attr & 0x70u) ||
        ((attr & 0x60u) && !(attr & 2u))) return false;
    bool selectedShiny = false;
    uint8_t selectedVariant = 0;
    if (defaultIsShiny || !(attr & 1u)) {
        selectedShiny = (attr & 2u) != 0;
        selectedVariant = (attr & 64u) ? 2 : (attr & 32u) ? 1 : 0;
    }
    shiny = selectedShiny;
    variant = selectedVariant;
    return true;
}

// Pinned pokedex-ui-handler.ts: caught SHINY gates the recorded variant stars.
// Legacy/invalid metadata never implies an unlocked appearance.
inline uint8_t nativeCaughtShinyVariants(const NativeStarterCandyRecord& record) {
    bool shiny=false;uint8_t variant=0;
    if(!nativeStarterDefaultAppearance(record,shiny,variant) || !shiny) return 0;
    return static_cast<uint8_t>((record.caughtAppearanceAttr >> 4) & 7u);
}

// GameData.initDexData/initStarterData pinned baseline. Only default starters
// receive this known metadata; never infer attributes for other caught species.
inline bool seedNativeFreshStarterDexMetadata(NativeStarterCandyRecord& record) {
    PokemonNature nature = PokemonNature::Unspecified;
    if (pokemonFreshProfileNature(record.speciesDex, nature) != PokemonFreshProfileResult::Ok) return false;
    record.natureAttr |= 1u << (static_cast<uint8_t>(nature) + 1);
    record.abilityAttr |= 1u; // ABILITY_1
    record.unlockedFormAttr |= uint64_t(128); // initDexData DEFAULT_FORM, fresh profiles only.
    record.genderAttr |= 12u; // initDexData unlocks MALE and FEMALE bits.
    for (uint8_t& iv : record.dexIvs) if (iv < 15) iv = 15;
    return true;
}

// Pinned starter-select-ui-utils.ts/getStarterDefaultNature scans in enum
// order. Missing metadata is explicit here; callers may use a known fresh baseline.
inline bool nativeStarterDefaultNature(const NativeStarterCandyRecord& record, PokemonNature& output) {
    if (!record.natureAttr || (record.natureAttr & ~0x03fffffeu)) return false;
    for (uint8_t n = 0; n < 25; ++n) {
        if (record.natureAttr & (1u << (n + 1))) {
            output = static_cast<PokemonNature>(n);
            return true;
        }
    }
    return false;
}

// GameData.getNaturesForAttr / StarterSelectUiHandler.CYCLE_NATURE,
// pinned 8555c08c823b856cbec4eb99ca84ea52a955836d: enum order, bit n+1.
inline bool nativeStarterSelectedNature(const NativeStarterCandyRecord& record,uint8_t index,PokemonNature& output) {
    if(index>=25 || !record.natureAttr || (record.natureAttr & ~0x03fffffeu) ||
        !(record.natureAttr & (1u<<(index+1)))) return false;
    output=static_cast<PokemonNature>(index);return true;
}
inline bool nativeStarterPreparedNature(const NativeStarterCandyRecord& record,PokemonNature& output) {
    return record.preferredNatureIndex==255 ? nativeStarterDefaultNature(record,output) :
        nativeStarterSelectedNature(record,record.preferredNatureIndex,output);
}
inline bool nativeStarterNextNature(const NativeStarterCandyRecord& record,PokemonNature current,int direction,PokemonNature& output) {
    const uint8_t start=static_cast<uint8_t>(current);PokemonNature validated=PokemonNature::Unspecified;
    if(!direction || !nativeStarterSelectedNature(record,start,validated)) return false;
    for(unsigned step=1;step<25;++step) {
        const uint8_t next=static_cast<uint8_t>((start+(direction>0 ? step : 25-step))%25);
        if(nativeStarterSelectedNature(record,next,validated)) {output=validated;return true;}
    }
    return false; // One unlocked nature has no alternate; no mutation or save.
}

// Pinned StarterSelectUiHandler preference validation and canCycle.ability:
// duplicate normal slots collapse unless only legacy slot 1 was unlocked.
inline uint8_t nativeStarterAbilityChoiceMask(const NativeStarterCandyRecord& record) {
    const auto* species=PokerogueContent::findSpeciesByDex(record.speciesDex);
    if(!species || !record.abilityAttr || (record.abilityAttr & ~7u)) return 0;
    uint8_t mask=record.abilityAttr;
    const uint16_t second=species->ability2 ? species->ability2 : species->ability1;
    if(species->ability1==second && (mask & 1u)) mask &= ~2u;
    if(!species->ability1) mask &= ~3u;
    if(!species->abilityHidden) mask &= ~4u;
    return mask;
}
inline bool nativeStarterSelectedAbility(const NativeStarterCandyRecord& record,uint8_t index,uint16_t& output) {
    if(index>2 || !(nativeStarterAbilityChoiceMask(record) & (1u<<index))) return false;
    const auto* species=PokerogueContent::findSpeciesByDex(record.speciesDex);
    output=index==0 ? species->ability1 : index==1 ?
        (species->ability2 ? species->ability2 : species->ability1) : species->abilityHidden;
    return true;
}

// Pinned PokemonSpeciesForm constructor aliases raw ability2=NONE to ability1.
// getStarterDefaultAbilityIndex therefore always reserves slot two for hidden.
inline bool nativeStarterDefaultAbility(const NativeStarterCandyRecord& record,
    uint8_t& outputIndex, uint16_t& outputAbility) {
    const uint8_t mask=nativeStarterAbilityChoiceMask(record);
    if(!mask) return false;
    const uint8_t index=(mask & 1u) ? 0 : (mask & 2u) ? 1 : 2;
    uint16_t ability=0;
    if(!nativeStarterSelectedAbility(record,index,ability)) return false;
    outputIndex = index;
    outputAbility = ability;
    return true;
}

inline bool nativeStarterPreparedAbility(const NativeStarterCandyRecord& record,uint8_t& index,uint16_t& ability) {
    if(record.preferredAbilityIndex==255) return nativeStarterDefaultAbility(record,index,ability);
    uint16_t selected=0;
    if(!nativeStarterSelectedAbility(record,record.preferredAbilityIndex,selected)) return false;
    index=record.preferredAbilityIndex;ability=selected;return true;
}

// Preserve canonical raw NONE; apply constructor normalization at runtime.
inline uint16_t nativeStarterFormAbility(const PokerogueContent::Species& species,
    const PokerogueContent::Form* form,uint8_t index,uint16_t fallback) {
    if(index>2 || (form && std::strcmp(form->speciesId,species.id))) return 0;
    if(!form) return fallback;
    const uint16_t primary=form->ability1 ? form->ability1 : species.ability1;
    const uint16_t selected=index==0 ? primary : index==1
        ? (form->ability2 ? form->ability2 : primary) : form->abilityHidden;
    return selected ? selected : fallback;
}

// StarterSelectUiUtils.getStarterDexAttrPropsFromPreferences delegates to
// GameData.getSpeciesDefaultDexAttrProps: female iff malePercent is zero.
// Caught gender bits validate explicit preferences, not the default gender.
inline bool nativeStarterDefaultGender(const NativeStarterCandyRecord& record, PokemonGender& output) {
    const auto* species = PokerogueContent::findSpeciesByDex(record.speciesDex);
    if (!species || (record.genderAttr & ~12u)) return false;
    if (species->malePercentTenths == 65534) { output = PokemonGender::Genderless; return true; }
    if ((!record.genderAttr && (!record.abilityAttr || !record.natureAttr)) ||
        species->malePercentTenths > 1000) return false;
    output = species->malePercentTenths == 0 ? PokemonGender::Female : PokemonGender::Male;
    return true;
}

enum class StarterCandyApplyResult : uint8_t {
    Applied = 0, InvalidRootSpecies, MissingStarterCost, InvalidCandyCount, Overflow
};
struct StarterCandyAwardEvent {
    uint16_t speciesDex = 0;
    uint32_t requestedAward = 0; // Upstream candyBar receives the requested amount.
    uint16_t appliedAward = 0; // Inventory may clamp at MAX_STARTER_CANDY_COUNT.
};

// GameData.addStarterCandy: clamp inventory, while presentation keeps the
// requested amount. Zero applied at the cap is valid, not a failed capture.
inline StarterCandyApplyResult applyNativeStarterCandyAward(NativeStarterCandyRecord& record,
    uint32_t requested, StarterCandyAwardEvent& event) {
    const auto* root = pokemonRootSpecies(record.speciesDex);
    if (!root || root->dex != record.speciesDex) return StarterCandyApplyResult::InvalidRootSpecies;
    if (!root->starterEligible) return StarterCandyApplyResult::MissingStarterCost;
    if (record.candyCount > PokerogueContent::kMaxStarterCandyCount) return StarterCandyApplyResult::InvalidCandyCount;
    const uint32_t remaining = PokerogueContent::kMaxStarterCandyCount - record.candyCount;
    StarterCandyAwardEvent next{};
    next.speciesDex = record.speciesDex;
    next.requestedAward = requested;
    next.appliedAward = static_cast<uint16_t>(requested > remaining ? remaining : requested);
    record.candyCount += next.appliedAward;
    event = next;
    return StarterCandyApplyResult::Applied;
}

// Numeric starterData side of Pokemon.addFriendship and GameData.addStarterCandy.
// Caller commits all fusion roots and Pokemon friendship together with its run.
inline StarterCandyApplyResult applyNativeStarterCandyFriendship(NativeStarterCandyRecord& record,
    uint32_t gain, StarterCandyAwardEvent& event) {
    const auto* root = pokemonRootSpecies(record.speciesDex);
    if (!root || root->dex != record.speciesDex) return StarterCandyApplyResult::InvalidRootSpecies;
    if (!root->starterEligible || root->starterCost < 1) return StarterCandyApplyResult::MissingStarterCost;
    if (record.candyCount > PokerogueContent::kMaxStarterCandyCount)
        return StarterCandyApplyResult::InvalidCandyCount;
    uint32_t cap = PokerogueContent::kStarterCandyFriendshipFallback;
    for (const auto& entry : PokerogueContent::kStarterCandyFriendshipCaps)
        if (entry.cost == root->starterCost) { cap = entry.value; break; }
    StarterCandyProgressPlan progress{};
    if (planStarterCandyProgress(record.friendship, gain, cap, true,
            record.candyCount < PokerogueContent::kMaxStarterCandyCount, progress) != PokemonExperienceResult::Ok)
        return StarterCandyApplyResult::Overflow;
    auto next = record;
    next.friendship = progress.friendship;
    const uint64_t total = static_cast<uint64_t>(record.candyCount) + progress.candyAward;
    next.candyCount = static_cast<uint16_t>(total > PokerogueContent::kMaxStarterCandyCount
        ? PokerogueContent::kMaxStarterCandyCount : total);
    StarterCandyAwardEvent nextEvent{};
    nextEvent.speciesDex = record.speciesDex;
    nextEvent.requestedAward = progress.candyAward;
    nextEvent.appliedAward = static_cast<uint16_t>(next.candyCount - record.candyCount);
    record = next;
    event = nextEvent;
    return StarterCandyApplyResult::Applied;
}

// Pinned starter-select-ui-handler.ts reduce-cost purchase, default Classic.
// Caller must commit the prepared profile before publishing it to gameplay.
enum class StarterCostPurchaseResult : uint8_t {
    Applied, InvalidRecord, MissingPrice, MaximumReduction, InsufficientCandy
};
inline StarterCostPurchaseResult applyNativeStarterCostReduction(NativeStarterCandyRecord& record) {
    const auto* species = PokerogueContent::findSpeciesByDex(record.speciesDex);
    if (!species || !species->starterEligible || species->starterCost < 1 ||
        record.candyCount > PokerogueContent::kMaxStarterCandyCount || record.costReduction > 2)
        return StarterCostPurchaseResult::InvalidRecord;
    if (record.costReduction == 2) return StarterCostPurchaseResult::MaximumReduction;
    const PokerogueContent::StarterCandyPrice* price = nullptr;
    for (const auto& row : PokerogueContent::kStarterCandyPrices)
        if (row.cost == species->starterCost) { price = &row; break; }
    if (!price) return StarterCostPurchaseResult::MissingPrice;
    const uint16_t required = price->costReduction[record.costReduction];
    if (record.candyCount < required) return StarterCostPurchaseResult::InsufficientCandy;
    record.candyCount -= required;
    ++record.costReduction;
    return StarterCostPurchaseResult::Applied;
}

enum class StarterPassivePurchaseResult : uint8_t {
    Applied, InvalidRecord, MissingPrice, AlreadyUnlocked, InsufficientCandy
};
inline StarterPassivePurchaseResult applyNativeStarterPassiveUnlock(
    NativeStarterCandyRecord& record, uint16_t price = 0) {
    const auto* species = PokerogueContent::findSpeciesByDex(record.speciesDex);
    if (!species || !species->starterEligible || species->starterCost < 1 ||
        record.candyCount > PokerogueContent::kMaxStarterCandyCount)
        return StarterPassivePurchaseResult::InvalidRecord;
    if (record.passiveUnlocked) return StarterPassivePurchaseResult::AlreadyUnlocked;
    uint16_t required = price;
    if (!required) {
        const PokerogueContent::StarterCandyPrice* candyPrice = nullptr;
        for (const auto& row : PokerogueContent::kStarterCandyPrices)
            if (row.cost == species->starterCost) { candyPrice = &row; break; }
        if (!candyPrice) return StarterPassivePurchaseResult::MissingPrice;
        required = candyPrice->passive;
    }
    if (record.candyCount < required) return StarterPassivePurchaseResult::InsufficientCandy;
    record.candyCount -= required;
    record.passiveUnlocked = true;
    return StarterPassivePurchaseResult::Applied;
}

enum class NativeFriendshipApplyResult : uint8_t {
    Applied = 0, UnresolvedPolicy, RootMismatch, InvalidProgress
};

// Prepare actor and root ledger together. Policy must resolve boosters, timed
// events/fusion and override behavior; max-friendship callbacks must be ready
// before publishing. The caller still commits the pair through NativeProgressStore.
inline NativeFriendshipApplyResult applyNativePokemonFriendship(
    PokemonBattleState& actor, NativeStarterCandyRecord& record, int32_t gain,
    const PokemonFriendshipPolicy& policy, bool maxFriendshipCallbacksResolved,
    StarterCandyAwardEvent& event) {
    PokemonFriendshipChangePlan friendship{};
    const auto planned = planPokemonFriendshipChange(actor.friendship, gain, policy, friendship);
    if (planned == PokemonExperienceResult::UnresolvedPolicy)
        return NativeFriendshipApplyResult::UnresolvedPolicy;
    if (planned != PokemonExperienceResult::Ok) return NativeFriendshipApplyResult::InvalidProgress;
    if (gain > 0 && friendship.requiresMaxFriendshipCallbacks && !maxFriendshipCallbacksResolved)
        return NativeFriendshipApplyResult::UnresolvedPolicy;
    StarterCandyAwardEvent nextEvent{};
    auto nextRecord = record;
    if (gain > 0) {
        const auto* root = pokemonRootSpecies(actor.speciesDex);
        if (!root || root->dex != record.speciesDex) return NativeFriendshipApplyResult::RootMismatch;
        if (applyNativeStarterCandyFriendship(nextRecord, friendship.candyFriendshipGain, nextEvent) !=
                StarterCandyApplyResult::Applied) return NativeFriendshipApplyResult::InvalidProgress;
    }
    actor.friendship = friendship.friendship;
    record = nextRecord;
    event = nextEvent;
    return NativeFriendshipApplyResult::Applied;
}

inline constexpr size_t kStarterCandyProfileOverhead = 144;
inline constexpr size_t kStarterCandyProfileRecordBytes = 43;
inline constexpr size_t kStarterCandyProfileMaxBytes = kStarterCandyProfileOverhead +
    PokerogueContent::kSpeciesCount * kStarterCandyProfileRecordBytes;

namespace StarterCandyProfileCodec {
inline bool overlaps(const void* a, size_t aSize, const void* b, size_t bSize) {
    const uintptr_t left = reinterpret_cast<uintptr_t>(a);
    const uintptr_t right = reinterpret_cast<uintptr_t>(b);
    return left <= right ? right - left < aSize : left - right < bSize;
}
inline bool validHash(const char* hash) {
    if (!hash) return false;
    for (size_t i = 0; i < 64; ++i)
        if (!((hash[i] >= '0' && hash[i] <= '9') || (hash[i] >= 'a' && hash[i] <= 'f'))) return false;
    return hash[64] == 0;
}
inline void put(uint32_t value, char* output, size_t bytes) {
    for (size_t i = 0; i < bytes; ++i) output[i] = static_cast<char>(value >> (i * 8));
}
inline uint32_t get(const char* input, size_t bytes) {
    uint32_t value = 0;
    for (size_t i = 0; i < bytes; ++i)
        value |= static_cast<uint32_t>(static_cast<uint8_t>(input[i])) << (i * 8);
    return value;
}
inline uint8_t version(const char* input) {
    if(std::memcmp(input,"P3CANDY",7)) return 0;
    if(input[7]=='A') return 10; // Alphabetic versions retain the eight-byte magic width.
    if(input[7]=='B') return 11;
    if(input[7]<'1' || input[7]>'9') return 0;
    return static_cast<uint8_t>(input[7]-'0');
}
inline size_t recordBytes(uint8_t v) {
    return v == 1 ? 8 : v == 2 || v == 3 ? 9 : v == 4 ? 19 : v == 5 ? 21 : v == 6 ? 29 : v == 7 ? 37 : v == 8 ? 39 : v == 9 ? 41 : v == 10 ? 42 : v == 11 ? 43 : 0;
}
inline void put64(uint64_t value, char* output) {
    for (uint8_t i = 0; i < 8; ++i) output[i] = static_cast<char>(value >> (i * 8));
}
inline uint64_t get64(const char* input) {
    uint64_t value = 0;
    for (uint8_t i = 0; i < 8; ++i) value |= uint64_t(static_cast<uint8_t>(input[i])) << (i * 8);
    return value;
}
inline NativeStarterCandyRecord record(const char* input, uint8_t v) {
    NativeStarterCandyRecord value{static_cast<uint16_t>(get(input, 2)), static_cast<uint16_t>(get(input + 2, 2)), get(input + 4, 4),
        v >= 2 && (static_cast<uint8_t>(input[8]) & 1) != 0,
        static_cast<uint8_t>(v >= 3 ? (static_cast<uint8_t>(input[8]) >> 1) & 3 : 0),
        (v >= 8) && ((static_cast<uint8_t>(input[8]) & 8u) != 0)};
    if (v >= 4) {
        value.natureAttr = get(input + 9, 4);
        for (uint8_t i = 0; i < 6; ++i) value.dexIvs[i] = static_cast<uint8_t>(input[13 + i]);
    }
    if (v >= 5) { value.abilityAttr = static_cast<uint8_t>(input[19]); value.genderAttr = static_cast<uint8_t>(input[20]); }
    if (v >= 6) value.observedFormAttr = get64(input + 21);
    if (v >= 7) value.unlockedFormAttr = get64(input + 29);
    if(v>=9) {value.observedAppearanceAttr=static_cast<uint8_t>(input[39]);value.caughtAppearanceAttr=static_cast<uint8_t>(input[40]);}
    if (v >= 8) value.preferredFormIndex = static_cast<uint16_t>(get(input + 37, 2));
    if(v>=10) value.preferredAbilityIndex=static_cast<uint8_t>(input[41]);
    if(v>=11) value.preferredNatureIndex=static_cast<uint8_t>(input[42]);
    return value;
}
inline bool valid(const NativeStarterCandyRecord& value, uint16_t previous, uint16_t candyLimit) {
    const auto validAppearance=[](uint8_t bits) {
        return !(bits & ~0x73u) && (!bits || ((bits & 3u) && (bits & 0x70u)))
            && (!(bits & 0x60u) || (bits & 2u));
    };
    if(!validAppearance(value.observedAppearanceAttr) || !validAppearance(value.caughtAppearanceAttr)
        || (value.caughtAppearanceAttr && !value.caught)) return false;

    if ((value.natureAttr & ~0x03fffffeu) || (value.abilityAttr & ~7u) || (value.genderAttr & ~12u)) return false;
    for (uint8_t iv : value.dexIvs) if (iv > 31) return false;
    const auto* species = PokerogueContent::findSpeciesByDex(value.speciesDex);
    if ((value.observedFormAttr | value.unlockedFormAttr) & uint64_t(127)) return false;
    if (species) for (uint8_t index = 0; index <= 56; ++index) {
        if (!((value.observedFormAttr | value.unlockedFormAttr) & (uint64_t(128) << index))) continue;
        if (PokerogueContent::findFormByUpstreamIndex(value.speciesDex, index)) continue;
        if (!index && (!species->firstFormId || !*species->firstFormId)) continue;
        const uint64_t bit = uint64_t(128) << index;
        if (!(value.observedFormAttr & bit) && (value.unlockedFormAttr & bit) &&
            pokemonRecursiveFormUnlockMetadata(*species, index)) continue;
        return false;
    }
    if (value.preferredFormIndex != 65535 && (!value.caught || !species || !species->starterEligible ||
        pokemonValidateStarterForm(value.speciesDex, value.preferredFormIndex, value.unlockedFormAttr) !=
            PokemonStarterFormResult::Ok)) return false;
    PokemonNature preferredNature=PokemonNature::Unspecified;
    if(value.preferredNatureIndex!=255 && (!value.caught || !species || !species->starterEligible ||
        !nativeStarterSelectedNature(value,value.preferredNatureIndex,preferredNature))) return false;
    uint16_t preferredAbility=0;
    if(value.preferredAbilityIndex!=255 && (!value.caught || !species || !species->starterEligible ||
        !nativeStarterSelectedAbility(value,value.preferredAbilityIndex,preferredAbility))) return false;
    const auto* root = pokemonRootSpecies(value.speciesDex);
    return species && root && value.speciesDex > previous && value.candyCount <= candyLimit && value.costReduction <= 2 &&
        (!value.costReduction || (species->starterEligible && species->starterCost >= 1)) &&
        (!value.passiveUnlocked || (value.caught && species->starterEligible && species->starterCost >= 1)) &&
        (root->dex == value.speciesDex || (!value.candyCount && !value.friendship));
}
}

// Caller supplies capacity and a resolved pinned candy limit. No heap or fixed
// species capacity in the codec; SD journal and gameplay transactions are separate.
inline NativeSaveResult encodeNativeStarterCandyProfile(const NativeStarterCandyRecord* records,
    size_t count, uint32_t generation, const char* contentHash, uint16_t candyLimit,
    char* output, size_t capacity, size_t& written) {
    written = 0;
    if (!output || !generation || !candyLimit || !StarterCandyProfileCodec::validHash(contentHash) ||
        (count && !records) || count > PokerogueContent::kSpeciesCount) return NativeSaveResult::InvalidRecord;
    const size_t size = kStarterCandyProfileOverhead + count * kStarterCandyProfileRecordBytes;
    if (capacity < size) return NativeSaveResult::TooLarge;
    if ((count && StarterCandyProfileCodec::overlaps(records, count * sizeof(*records), output, size)) ||
        StarterCandyProfileCodec::overlaps(contentHash, 65, output, size)) return NativeSaveResult::InvalidRecord;
    uint16_t previous = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!StarterCandyProfileCodec::valid(records[i], previous, candyLimit)) return NativeSaveResult::InvalidRecord;
        previous = records[i].speciesDex;
    }
    std::memcpy(output, "P3CANDYB", 8);
    std::memcpy(output + 8, contentHash, 64);
    StarterCandyProfileCodec::put(generation, output + 72, 4);
    StarterCandyProfileCodec::put(static_cast<uint32_t>(count), output + 76, 4);
    for (size_t i = 0; i < count; ++i) {
        char* target = output + 80 + i * kStarterCandyProfileRecordBytes;
        StarterCandyProfileCodec::put(records[i].speciesDex, target, 2);
        StarterCandyProfileCodec::put(records[i].candyCount, target + 2, 2);
        StarterCandyProfileCodec::put(records[i].friendship, target + 4, 4);
        target[8] = static_cast<char>((records[i].caught ? 1 : 0) | (records[i].costReduction << 1) | (records[i].passiveUnlocked ? 8 : 0));
        StarterCandyProfileCodec::put(records[i].natureAttr, target + 9, 4);
        for (uint8_t iv = 0; iv < 6; ++iv) target[13 + iv] = static_cast<char>(records[i].dexIvs[iv]);
        target[19] = static_cast<char>(records[i].abilityAttr);
        target[20] = static_cast<char>(records[i].genderAttr);
        StarterCandyProfileCodec::put64(records[i].observedFormAttr, target + 21);
        StarterCandyProfileCodec::put64(records[i].unlockedFormAttr, target + 29);
        StarterCandyProfileCodec::put(records[i].preferredFormIndex, target + 37, 2);
        target[39]=static_cast<char>(records[i].observedAppearanceAttr);
        target[40]=static_cast<char>(records[i].caughtAppearanceAttr);
        target[41]=static_cast<char>(records[i].preferredAbilityIndex);
        target[42]=static_cast<char>(records[i].preferredNatureIndex);
    }
    char digest[65]{};
    IntegritySha256::hashHex(output, size - 64, digest);
    std::memcpy(output + size - 64, digest, 64);
    written = size;
    return NativeSaveResult::Ok;
}

inline NativeSaveResult inspectNativeStarterCandyProfile(const char* input, size_t length,
    const char* contentHash, uint16_t candyLimit, size_t& count, uint32_t& generation) {
    if (!input || length < kStarterCandyProfileOverhead || length > kStarterCandyProfileMaxBytes ||
        !candyLimit || !StarterCandyProfileCodec::validHash(contentHash)) return NativeSaveResult::InvalidFormat;
    const uint8_t v = StarterCandyProfileCodec::version(input);
    if (!v) return NativeSaveResult::UnsupportedVersion;
    const bool legacy = v == 1;
    const bool reduced = v >= 3;
    const size_t recordBytes = StarterCandyProfileCodec::recordBytes(v);
    const uint32_t entries = StarterCandyProfileCodec::get(input + 76, 4);
    if (entries > PokerogueContent::kSpeciesCount ||
        length != kStarterCandyProfileOverhead + entries * recordBytes)
        return NativeSaveResult::InvalidFormat;
    char digest[65]{};
    IntegritySha256::hashHex(input, length - 64, digest);
    if (std::memcmp(digest, input + length - 64, 64)) return NativeSaveResult::ChecksumMismatch;
    if (std::memcmp(contentHash, input + 8, 64)) return NativeSaveResult::ContentMismatch;
    const uint32_t sequence = StarterCandyProfileCodec::get(input + 72, 4);
    if (!sequence) return NativeSaveResult::InvalidRecord;
    uint16_t previous = 0;
    for (size_t i = 0; i < entries; ++i) {
        const char* source = input + 80 + i * recordBytes;
        if (!legacy) {
            const uint8_t mask = (v >= 8) ? ~15u : (reduced ? ~7u : 0);
            if (reduced ? (static_cast<uint8_t>(source[8]) & mask) != 0 : static_cast<uint8_t>(source[8]) > 1)
                return NativeSaveResult::InvalidRecord;
        }
        auto value = StarterCandyProfileCodec::record(source, v);
        value.passiveUnlocked = (v >= 8) && ((static_cast<uint8_t>(source[8]) & 8u) != 0);
        if (legacy) {
            const auto* root = pokemonRootSpecies(value.speciesDex);
            if (!root || root->dex != value.speciesDex) return NativeSaveResult::InvalidRecord;
        }
        if (!StarterCandyProfileCodec::valid(value, previous, candyLimit)) return NativeSaveResult::InvalidRecord;
        previous = value.speciesDex;
    }
    count = entries;
    generation = sequence;
    return NativeSaveResult::Ok;
}

inline NativeSaveResult decodeNativeStarterCandyProfile(const char* input, size_t length,
    const char* contentHash, uint16_t candyLimit, NativeStarterCandyRecord* output, size_t capacity,
    size_t& count, uint32_t& generation) {
    size_t entries = 0;
    uint32_t sequence = 0;
    const auto status = inspectNativeStarterCandyProfile(input, length, contentHash, candyLimit, entries, sequence);
    if (status != NativeSaveResult::Ok) return status;
    if (entries > capacity || (entries && !output)) return NativeSaveResult::TooLarge;
    if (entries && StarterCandyProfileCodec::overlaps(input, length, output, entries * sizeof(*output)))
        return NativeSaveResult::InvalidRecord;
    const uint8_t v = StarterCandyProfileCodec::version(input);
    const size_t recordBytes = StarterCandyProfileCodec::recordBytes(v);
    for (size_t i = 0; i < entries; ++i)
        output[i] = StarterCandyProfileCodec::record(input + 80 + i * recordBytes, v);
    count = entries;
    generation = sequence;
    return NativeSaveResult::Ok;
}

} // namespace Pokerogue3DS

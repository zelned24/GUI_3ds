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
    uint32_t natureAttr = 0; // Upstream bit n+1; zero means metadata unavailable.
    uint8_t dexIvs[6]{}; // Per-stat maxima, never actor IVs overwritten in place.
    uint8_t abilityAttr = 0; // Upstream AbilityAttr 1/2/4; zero is unavailable.
    uint8_t genderAttr = 0; // Upstream DexAttr.MALE/FEMALE 4/8; genderless adds neither.
};

// GameData.initDexData/initStarterData pinned baseline. Only default starters
// receive this known metadata; never infer attributes for other caught species.
inline bool seedNativeFreshStarterDexMetadata(NativeStarterCandyRecord& record) {
    PokemonNature nature = PokemonNature::Unspecified;
    if (pokemonFreshProfileNature(record.speciesDex, nature) != PokemonFreshProfileResult::Ok) return false;
    record.natureAttr |= 1u << (static_cast<uint8_t>(nature) + 1);
    record.abilityAttr |= 1u; // ABILITY_1
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

// Pinned getStarterDefaultAbilityIndex: hidden uses slot one when there is
// no second regular ability, otherwise slot two. Keep the upstream slot ID.
inline bool nativeStarterDefaultAbility(const NativeStarterCandyRecord& record,
    uint8_t& outputIndex, uint16_t& outputAbility) {
    const auto* species = PokerogueContent::findSpeciesByDex(record.speciesDex);
    if (!species || !record.abilityAttr || (record.abilityAttr & ~7u)) return false;
    const uint8_t index = (record.abilityAttr & 1u) ? 0 :
        (!species->ability2 || (record.abilityAttr & 2u)) ? 1 : 2;
    const uint16_t ability = index == 0 ? species->ability1 :
        index == 1 && species->ability2 ? species->ability2 : species->abilityHidden;
    if (!ability) return false;
    outputIndex = index;
    outputAbility = ability;
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
inline constexpr size_t kStarterCandyProfileRecordBytes = 21;
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
inline NativeStarterCandyRecord record(const char* input, bool legacy = false, bool reduced = false, bool dexMetadata = false, bool captureAttributes = false) {
    NativeStarterCandyRecord value{static_cast<uint16_t>(get(input, 2)), static_cast<uint16_t>(get(input + 2, 2)), get(input + 4, 4),
        !legacy && (static_cast<uint8_t>(input[8]) & 1) != 0,
        static_cast<uint8_t>(reduced ? (static_cast<uint8_t>(input[8]) >> 1) & 3 : 0)};
    if (dexMetadata) {
        value.natureAttr = get(input + 9, 4);
        for (uint8_t i = 0; i < 6; ++i) value.dexIvs[i] = static_cast<uint8_t>(input[13 + i]);
    }
    if (captureAttributes) {
        value.abilityAttr = static_cast<uint8_t>(input[19]);
        value.genderAttr = static_cast<uint8_t>(input[20]);
    }
    return value;
}
inline bool valid(const NativeStarterCandyRecord& value, uint16_t previous, uint16_t candyLimit) {
    if ((value.natureAttr & ~0x03fffffeu) || (value.abilityAttr & ~7u) || (value.genderAttr & ~12u)) return false;
    for (uint8_t iv : value.dexIvs) if (iv > 31) return false;
    const auto* species = PokerogueContent::findSpeciesByDex(value.speciesDex);
    const auto* root = pokemonRootSpecies(value.speciesDex);
    return species && root && value.speciesDex > previous && value.candyCount <= candyLimit && value.costReduction <= 2 &&
        (!value.costReduction || (species->starterEligible && species->starterCost >= 1)) &&
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
    std::memcpy(output, "P3CANDY5", 8);
    std::memcpy(output + 8, contentHash, 64);
    StarterCandyProfileCodec::put(generation, output + 72, 4);
    StarterCandyProfileCodec::put(static_cast<uint32_t>(count), output + 76, 4);
    for (size_t i = 0; i < count; ++i) {
        char* target = output + 80 + i * kStarterCandyProfileRecordBytes;
        StarterCandyProfileCodec::put(records[i].speciesDex, target, 2);
        StarterCandyProfileCodec::put(records[i].candyCount, target + 2, 2);
        StarterCandyProfileCodec::put(records[i].friendship, target + 4, 4);
        target[8] = static_cast<char>((records[i].caught ? 1 : 0) | (records[i].costReduction << 1));
        StarterCandyProfileCodec::put(records[i].natureAttr, target + 9, 4);
        for (uint8_t iv = 0; iv < 6; ++iv) target[13 + iv] = static_cast<char>(records[i].dexIvs[iv]);
        target[19] = static_cast<char>(records[i].abilityAttr);
        target[20] = static_cast<char>(records[i].genderAttr);
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
    const bool legacy = !std::memcmp(input, "P3CANDY1", 8);
    const bool captureAttributes = !std::memcmp(input, "P3CANDY5", 8);
    const bool dexMetadata = captureAttributes || !std::memcmp(input, "P3CANDY4", 8);
    const bool reduced = dexMetadata || !std::memcmp(input, "P3CANDY3", 8);
    if (!legacy && !reduced && std::memcmp(input, "P3CANDY2", 8)) return NativeSaveResult::UnsupportedVersion;
    const size_t recordBytes = legacy ? 8 : !std::memcmp(input, "P3CANDY5", 8) ? kStarterCandyProfileRecordBytes :
        !std::memcmp(input, "P3CANDY4", 8) ? 19 : 9;
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
        if (!legacy && (reduced ? (static_cast<uint8_t>(source[8]) & ~7u) != 0 : static_cast<uint8_t>(source[8]) > 1)) return NativeSaveResult::InvalidRecord;
        const auto value = StarterCandyProfileCodec::record(source, legacy, reduced, dexMetadata, captureAttributes);
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
    const bool legacy = !std::memcmp(input, "P3CANDY1", 8);
    const size_t recordBytes = legacy ? 8 : !std::memcmp(input, "P3CANDY5", 8) ? kStarterCandyProfileRecordBytes :
        !std::memcmp(input, "P3CANDY4", 8) ? 19 : 9;
    for (size_t i = 0; i < entries; ++i)
        output[i] = StarterCandyProfileCodec::record(input + 80 + i * recordBytes, legacy,
            !std::memcmp(input, "P3CANDY3", 8) || !std::memcmp(input, "P3CANDY4", 8) || !std::memcmp(input, "P3CANDY5", 8),
            !std::memcmp(input, "P3CANDY4", 8) || !std::memcmp(input, "P3CANDY5", 8), !std::memcmp(input, "P3CANDY5", 8));
    count = entries;
    generation = sequence;
    return NativeSaveResult::Ok;
}

} // namespace Pokerogue3DS

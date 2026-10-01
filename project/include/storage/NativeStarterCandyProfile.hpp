#pragma once
#include "storage/NativeRunSave.hpp"
#include "storage/IntegritySha256.hpp"
#include "game/PokemonBattleState.hpp"
#include "game/PokemonExperience.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace Pokerogue3DS {

struct NativeStarterCandyRecord {
    uint16_t speciesDex = 0; // Canonical root SpeciesId, never a catalog offset.
    uint16_t candyCount = 0;
    uint32_t friendship = 0; // Starter progress, separate from Pokemon friendship.
};

enum class StarterCandyApplyResult : uint8_t {
    Applied = 0, InvalidRootSpecies, MissingStarterCost, InvalidCandyCount, Overflow
};
struct StarterCandyAwardEvent {
    uint16_t speciesDex = 0;
    uint32_t requestedAward = 0; // Upstream candyBar receives the requested amount.
    uint16_t appliedAward = 0; // Inventory may clamp at MAX_STARTER_CANDY_COUNT.
};

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
inline constexpr size_t kStarterCandyProfileRecordBytes = 8;
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
inline NativeStarterCandyRecord record(const char* input) {
    return {static_cast<uint16_t>(get(input, 2)), static_cast<uint16_t>(get(input + 2, 2)), get(input + 4, 4)};
}
inline bool valid(const NativeStarterCandyRecord& value, uint16_t previous, uint16_t candyLimit) {
    const auto* species = pokemonRootSpecies(value.speciesDex);
    return species && species->dex == value.speciesDex && value.speciesDex > previous &&
        value.candyCount <= candyLimit;
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
    std::memcpy(output, "P3CANDY1", 8);
    std::memcpy(output + 8, contentHash, 64);
    StarterCandyProfileCodec::put(generation, output + 72, 4);
    StarterCandyProfileCodec::put(static_cast<uint32_t>(count), output + 76, 4);
    for (size_t i = 0; i < count; ++i) {
        char* target = output + 80 + i * kStarterCandyProfileRecordBytes;
        StarterCandyProfileCodec::put(records[i].speciesDex, target, 2);
        StarterCandyProfileCodec::put(records[i].candyCount, target + 2, 2);
        StarterCandyProfileCodec::put(records[i].friendship, target + 4, 4);
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
    if (std::memcmp(input, "P3CANDY1", 8)) return NativeSaveResult::UnsupportedVersion;
    const uint32_t entries = StarterCandyProfileCodec::get(input + 76, 4);
    if (entries > PokerogueContent::kSpeciesCount ||
        length != kStarterCandyProfileOverhead + entries * kStarterCandyProfileRecordBytes)
        return NativeSaveResult::InvalidFormat;
    char digest[65]{};
    IntegritySha256::hashHex(input, length - 64, digest);
    if (std::memcmp(digest, input + length - 64, 64)) return NativeSaveResult::ChecksumMismatch;
    if (std::memcmp(contentHash, input + 8, 64)) return NativeSaveResult::ContentMismatch;
    const uint32_t sequence = StarterCandyProfileCodec::get(input + 72, 4);
    if (!sequence) return NativeSaveResult::InvalidRecord;
    uint16_t previous = 0;
    for (size_t i = 0; i < entries; ++i) {
        const auto value = StarterCandyProfileCodec::record(input + 80 + i * kStarterCandyProfileRecordBytes);
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
    for (size_t i = 0; i < entries; ++i)
        output[i] = StarterCandyProfileCodec::record(input + 80 + i * kStarterCandyProfileRecordBytes);
    count = entries;
    generation = sequence;
    return NativeSaveResult::Ok;
}

} // namespace Pokerogue3DS

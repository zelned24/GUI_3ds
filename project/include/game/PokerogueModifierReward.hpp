#pragma once

#include "content/PokerogueRuntimeContent.hpp"
#include "game/PokemonBattleState.hpp"
#include "game/PokerogueRngAdapter.hpp"
#include <cstdint>
#include <cstring>

namespace Pokerogue3DS {

// Actor instances reference canonical ItemDefinition rows; they do not carry
// invented effects or assume that matching an item ID equals matchType.
struct NativeHeldModifierInstance {
    size_t canonicalItemIndex = 0;
    uint32_t ownerPokemonId = 0;
    uint16_t stackCount = 0;
    bool transferable = true;
    char rawArguments[128]{}; // Opaque until the modifier-specific adapter resolves them.
};
enum class HeldModifierStorageResult : uint8_t { Ok, MissingItem, InvalidState, CapacityExceeded };

inline const PokerogueContent::Entity* heldModifierDefinition(const NativeHeldModifierInstance& instance) {
    return instance.canonicalItemIndex < PokerogueContent::kItemCount
        ? &PokerogueContent::kItems[instance.canonicalItemIndex] : nullptr;
}

inline bool validateHeldModifierInstance(const NativeHeldModifierInstance& instance) {
    if (!heldModifierDefinition(instance) || !instance.stackCount) return false;
    for (char ch : instance.rawArguments) if (!ch) return true;
    return false;
}

inline HeldModifierStorageResult initializeHeldModifierInstance(const char* canonicalItemId,
    uint32_t ownerPokemonId, uint16_t stackCount, bool transferable,
    const char* rawArguments, NativeHeldModifierInstance& output) {
    if (!canonicalItemId || !stackCount) return HeldModifierStorageResult::InvalidState;
    size_t itemIndex = PokerogueContent::kItemCount;
    for (size_t i = 0; i < PokerogueContent::kItemCount; ++i)
        if (!std::strcmp(PokerogueContent::kItems[i].id, canonicalItemId)) { itemIndex = i; break; }
    if (itemIndex == PokerogueContent::kItemCount) return HeldModifierStorageResult::MissingItem;
    NativeHeldModifierInstance next{};
    next.canonicalItemIndex = itemIndex;
    next.ownerPokemonId = ownerPokemonId;
    next.stackCount = stackCount;
    next.transferable = transferable;
    if (rawArguments) {
        size_t i = 0;
        for (; rawArguments[i]; ++i) {
            if (i + 1 >= sizeof(next.rawArguments)) return HeldModifierStorageResult::CapacityExceeded;
            next.rawArguments[i] = rawArguments[i];
        }
    }
    output = next;
    return HeldModifierStorageResult::Ok;
}

// Caller-owned storage: explicit capacity is a hardware budget, never a catalog limit.
// Append preserves upstream modifier-list order. A resolved matchType policy must
// decide whether to merge before this primitive is used.
inline HeldModifierStorageResult appendHeldModifierInstance(NativeHeldModifierInstance* records,
    size_t capacity, size_t& count, const NativeHeldModifierInstance& instance) {
    if ((!records && capacity) || count > capacity || !validateHeldModifierInstance(instance))
        return HeldModifierStorageResult::InvalidState;
    if (count == capacity) return HeldModifierStorageResult::CapacityExceeded;
    records[count++] = instance;
    return HeldModifierStorageResult::Ok;
}

inline bool removeHeldModifierInstance(NativeHeldModifierInstance* records, size_t capacity, size_t& count, size_t index) {
    if (!records || count > capacity || index >= count) return false;
    for (size_t i = index + 1; i < count; ++i) records[i - 1] = records[i];
    records[--count] = {};
    return true;
}

enum class HeldItemStackTransferResult : uint8_t { Transferred, NoCapacity, InvalidState };
struct HeldItemStackTransferEvent {
    uint16_t transferred = 0;
    uint16_t sourceRemaining = 0;
    uint16_t targetStack = 0;
    bool removeSource = false;
};

// BattleScene.tryTransferHeldItemModifier stack portion. Type matching and
// BlockItemTheft/PostItemLost ability dispatch must resolve before applying it.
inline HeldItemStackTransferResult calculateHeldItemStackTransfer(uint16_t sourceStack,
    bool targetHasMatchingModifier, uint16_t targetStack, uint16_t targetMaxStack,
    uint16_t quantity, HeldItemStackTransferEvent& output) {
    if (!sourceStack || !quantity || !targetMaxStack ||
        (!targetHasMatchingModifier && targetStack) || targetStack > targetMaxStack)
        return HeldItemStackTransferResult::InvalidState;
    if (targetHasMatchingModifier && targetStack == targetMaxStack)
        return HeldItemStackTransferResult::NoCapacity;
    uint16_t taken = quantity < sourceStack ? quantity : sourceStack;
    if (targetHasMatchingModifier && taken > targetMaxStack - targetStack)
        taken = targetMaxStack - targetStack;
    // A new modifier is added through the inventory's normal add policy;
    // upstream only checks matching-stack capacity in this method.
    if (!targetHasMatchingModifier && taken > targetMaxStack)
        return HeldItemStackTransferResult::InvalidState;
    HeldItemStackTransferEvent event{};
    event.transferred = taken;
    event.sourceRemaining = sourceStack - taken;
    event.targetStack = targetStack + taken;
    event.removeSource = event.sourceRemaining == 0;
    output = event;
    return HeldItemStackTransferResult::Transferred;
}

struct HeldItemTheftPolicy {
    bool resolved = false; // Includes matchType, stack cap and ability/post-loss dispatch capability.
    bool blockedByAbility = false;
    size_t matchingTargetIndex = static_cast<size_t>(-1);
    uint16_t targetMaxStack = 0;
};
enum class HeldItemTheftAbilityPolicyResult : uint8_t {
    Resolved, InvalidState, UnknownAbility, UnresolvedApplicability, UnresolvedCallbacks
};

// IDs are the source actor's currently applicable abilities/passives, after
// suppression/ignorable/fainted/fusion rules. This resolver does not invent that set.
inline HeldItemTheftAbilityPolicyResult resolveHeldItemTheftAbilityPolicy(
    const uint16_t* activeAbilityIds, size_t count, bool applicabilityResolved, bool& theftBlocked,
    bool nativePostLostCallbacksReady = false) {
    if (count && !activeAbilityIds) return HeldItemTheftAbilityPolicyResult::InvalidState;
    if (!applicabilityResolved) return HeldItemTheftAbilityPolicyResult::UnresolvedApplicability;
    bool blocked = false, pending = false;
    for (size_t i = 0; i < count; ++i) {
        const PokerogueContent::HeldItemTheftAbilityProfile* selected = nullptr;
        for (const auto& profile : PokerogueContent::kHeldItemTheftAbilityProfiles)
            if (profile.abilityId == activeAbilityIds[i]) { selected = &profile; break; }
        if (!selected) return HeldItemTheftAbilityPolicyResult::UnknownAbility;
        if (selected->conditionalCallbacks) return HeldItemTheftAbilityPolicyResult::UnresolvedCallbacks;
        blocked |= selected->blocksTheft;
        pending |= selected->requiresPostLostDispatcher &&
            !(nativePostLostCallbacksReady && selected->appliesUnburden);
    }
    // CancelInteractionAbAttr cancels before inventory mutation/PostItemLost.
    if (!blocked && pending) return HeldItemTheftAbilityPolicyResult::UnresolvedCallbacks;
    theftBlocked = blocked;
    return HeldItemTheftAbilityPolicyResult::Resolved;
}

// Transient summon tag; the caller owns reset/serialization with other battle tags.
enum class HeldItemLostCallbackResult : uint8_t {
    Applied, NoChange, InvalidState, UnknownAbility, UnresolvedApplicability, UnsupportedCallback
};
inline HeldItemLostCallbackResult applyHeldItemLostCallbacks(
    const uint16_t* activeAbilityIds, size_t count, bool applicabilityResolved,
    bool simulated, HeldItemLostTagState& tags) {
    if (count && !activeAbilityIds) return HeldItemLostCallbackResult::InvalidState;
    if (!applicabilityResolved) return HeldItemLostCallbackResult::UnresolvedApplicability;
    bool applyUnburden = false;
    for (size_t i = 0; i < count; ++i) {
        const PokerogueContent::HeldItemTheftAbilityProfile* selected = nullptr;
        for (const auto& profile : PokerogueContent::kHeldItemTheftAbilityProfiles)
            if (profile.abilityId == activeAbilityIds[i]) { selected = &profile; break; }
        if (!selected) return HeldItemLostCallbackResult::UnknownAbility;
        if (selected->conditionalCallbacks ||
            (selected->requiresPostLostDispatcher && !selected->appliesUnburden))
            return HeldItemLostCallbackResult::UnsupportedCallback;
        applyUnburden |= selected->appliesUnburden;
    }
    // PostItemLostApplyBattlerTagAbAttr.canApply: absent tag and !simulated.
    if (!applyUnburden || simulated || tags.unburden) return HeldItemLostCallbackResult::NoChange;
    tags.unburden = true;
    return HeldItemLostCallbackResult::Applied;
}

enum class HeldItemMatchPolicyResult : uint8_t {
    Resolved, UnresolvedAbilities, UnsupportedModifierClass, UnresolvedArguments, InvalidState
};

inline bool isTurnHeldItemTransferInstance(const NativeHeldModifierInstance& instance) {
    const auto* definition = heldModifierDefinition(instance);
    if (!definition) return false;
    for (const auto& profile : PokerogueContent::kHeldModifierClassProfiles)
        if (!std::strcmp(profile.itemId, definition->id)) return profile.isTurnHeldItemTransfer;
    return false;
}

// TurnHeldItemTransferModifier.matchType/getMaxHeldItemCount pinned: class
// match, cap one. Ability dispatch must be independently resolved by the caller.
inline HeldItemMatchPolicyResult resolveTurnHeldItemTransferMatchPolicy(
    const NativeHeldModifierInstance& source, const NativeHeldModifierInstance* records, size_t count,
    uint32_t targetPokemonId, bool abilityPolicyResolved, bool theftBlocked, HeldItemTheftPolicy& output) {
    if (!validateHeldModifierInstance(source) || (count && !records) || source.ownerPokemonId == targetPokemonId)
        return HeldItemMatchPolicyResult::InvalidState;
    if (!isTurnHeldItemTransferInstance(source)) return HeldItemMatchPolicyResult::UnsupportedModifierClass;
    if (source.rawArguments[0] && std::strcmp(source.rawArguments, "[]"))
        return HeldItemMatchPolicyResult::UnresolvedArguments;
    if (source.stackCount != 1) return HeldItemMatchPolicyResult::InvalidState;
    if (!abilityPolicyResolved) return HeldItemMatchPolicyResult::UnresolvedAbilities;
    HeldItemTheftPolicy policy{};
    policy.resolved = true;
    policy.blockedByAbility = theftBlocked;
    policy.targetMaxStack = 1;
    // An ability cancellation precedes matchType/capacity lookup upstream.
    if (!theftBlocked) for (size_t i = 0; i < count; ++i) {
        if (!validateHeldModifierInstance(records[i])) return HeldItemMatchPolicyResult::InvalidState;
        if (records[i].ownerPokemonId != targetPokemonId) continue;
        if (!isTurnHeldItemTransferInstance(records[i]))
            return HeldItemMatchPolicyResult::UnsupportedModifierClass;
        if (records[i].rawArguments[0] && std::strcmp(records[i].rawArguments, "[]"))
            return HeldItemMatchPolicyResult::UnresolvedArguments;
        if (records[i].stackCount != 1 || policy.matchingTargetIndex != static_cast<size_t>(-1))
            return HeldItemMatchPolicyResult::InvalidState;
        policy.matchingTargetIndex = i;
    }
    output = policy;
    return HeldItemMatchPolicyResult::Resolved;
}

struct HeldItemInventoryTransferEvent {
    HeldItemStackTransferEvent stacks{};
    uint32_t sourcePokemonId = 0;
    uint32_t targetPokemonId = 0;
    size_t canonicalItemIndex = 0;
    size_t resultingInventoryIndex = 0;
};
enum class HeldItemInventoryTransferResult : uint8_t {
    Transferred, ProtectedItem, BlockedByAbility, NoCapacity, StorageCapacity,
    UnresolvedPolicy, InvalidState
};

// The caller resolves matchType and ability policies before invoking this
// inventory mutation. PostItemLost dispatch consumes the returned event.
inline HeldItemInventoryTransferResult applySelectedHeldItemTheft(
    NativeHeldModifierInstance* records, size_t capacity, size_t& count, size_t sourceIndex,
    uint32_t targetPokemonId, const HeldItemTheftPolicy& policy, HeldItemInventoryTransferEvent& output) {
    if (!records || count > capacity || sourceIndex >= count) return HeldItemInventoryTransferResult::InvalidState;
    for (size_t i = 0; i < count; ++i)
        if (!validateHeldModifierInstance(records[i])) return HeldItemInventoryTransferResult::InvalidState;
    const auto source = records[sourceIndex];
    if (source.ownerPokemonId == targetPokemonId) return HeldItemInventoryTransferResult::InvalidState;
    if (!source.transferable) return HeldItemInventoryTransferResult::ProtectedItem;
    if (!policy.resolved) return HeldItemInventoryTransferResult::UnresolvedPolicy;
    if (policy.blockedByAbility) return HeldItemInventoryTransferResult::BlockedByAbility;
    const bool matching = policy.matchingTargetIndex != static_cast<size_t>(-1);
    if (matching && (policy.matchingTargetIndex >= count || policy.matchingTargetIndex == sourceIndex ||
        records[policy.matchingTargetIndex].ownerPokemonId != targetPokemonId))
        return HeldItemInventoryTransferResult::InvalidState;
    HeldItemStackTransferEvent stacks{};
    const auto stackResult = calculateHeldItemStackTransfer(source.stackCount, matching,
        matching ? records[policy.matchingTargetIndex].stackCount : 0, policy.targetMaxStack, 1, stacks);
    if (stackResult == HeldItemStackTransferResult::NoCapacity) return HeldItemInventoryTransferResult::NoCapacity;
    if (stackResult != HeldItemStackTransferResult::Transferred) return HeldItemInventoryTransferResult::InvalidState;
    const size_t finalCount = count - (stacks.removeSource ? 1 : 0) - (matching ? 1 : 0) + 1;
    if (finalCount > capacity) return HeldItemInventoryTransferResult::StorageCapacity;
    auto received = source; // Clone preserves canonical definition, args and transferability.
    received.ownerPokemonId = targetPokemonId;
    received.stackCount = stacks.targetStack;
    // All fallible checks finish before the first inventory write.
    records[sourceIndex].stackCount = stacks.sourceRemaining;
    const size_t previousCount = count;
    size_t writeIndex = 0;
    for (size_t readIndex = 0; readIndex < previousCount; ++readIndex) {
        if ((stacks.removeSource && readIndex == sourceIndex) ||
            (matching && readIndex == policy.matchingTargetIndex)) continue;
        records[writeIndex++] = records[readIndex];
    }
    records[writeIndex++] = received;
    for (size_t i = writeIndex; i < previousCount; ++i) records[i] = {};
    count = writeIndex;
    HeldItemInventoryTransferEvent event{};
    event.stacks = stacks;
    event.sourcePokemonId = source.ownerPokemonId;
    event.targetPokemonId = targetPokemonId;
    event.canonicalItemIndex = source.canonicalItemIndex;
    event.resultingInventoryIndex = count - 1;
    output = event;
    return HeldItemInventoryTransferResult::Transferred;
}

// Preflight copied tags; commit them only after inventory transfer succeeds.
// The caller resolves applicability and modifier matching before this operation.
inline HeldItemInventoryTransferResult applyHeldItemTheftWithCallbacks(
    NativeHeldModifierInstance* records, size_t capacity, size_t& count, size_t sourceIndex,
    PokemonBattleState& sourceActor, uint32_t targetPokemonId, const HeldItemTheftPolicy& matchPolicy,
    const uint16_t* activeAbilityIds, size_t abilityCount, bool applicabilityResolved,
    bool itemLost, HeldItemInventoryTransferEvent& output) {
    if (!records || count > capacity || sourceIndex >= count ||
        records[sourceIndex].ownerPokemonId != sourceActor.pokemonId)
        return HeldItemInventoryTransferResult::InvalidState;
    if (!matchPolicy.resolved) return HeldItemInventoryTransferResult::UnresolvedPolicy;
    bool blocked = false;
    if (resolveHeldItemTheftAbilityPolicy(activeAbilityIds, abilityCount, applicabilityResolved,
            blocked, true) != HeldItemTheftAbilityPolicyResult::Resolved)
        return HeldItemInventoryTransferResult::UnresolvedPolicy;
    auto nextTags = sourceActor.heldItemLostTags;
    if (!blocked && itemLost) {
        const auto callback = applyHeldItemLostCallbacks(activeAbilityIds, abilityCount,
            applicabilityResolved, false, nextTags);
        if (callback != HeldItemLostCallbackResult::Applied && callback != HeldItemLostCallbackResult::NoChange)
            return HeldItemInventoryTransferResult::UnresolvedPolicy;
    }
    auto policy = matchPolicy;
    policy.blockedByAbility = blocked;
    HeldItemInventoryTransferEvent event{};
    const auto result = applySelectedHeldItemTheft(records, capacity, count, sourceIndex,
        targetPokemonId, policy, event);
    if (result != HeldItemInventoryTransferResult::Transferred) return result;
    sourceActor.heldItemLostTags = nextTags;
    output = event;
    return result;
}

struct HeldItemTransferCandidate {
    uint32_t ownerPokemonId = 0;
    size_t inventoryIndex = 0;
    bool transferable = false;
};
struct HeldItemTransferSelection {
    size_t opponentIndex = 0;
    size_t inventoryIndex = 0;
    bool itemFound = false;
};
enum class HeldItemTransferSelectionResult : uint8_t {
    Selected, NoOpponent, NoTransferCount, NoItem, InvalidState
};

// One-item activation (Mini Black Hole max stack one). Uses the holder's
// battle RNG and preserves upstream findModifiers order, without a heap pool.
template <typename CandidateAt>
inline HeldItemTransferSelectionResult selectHeldItemTransferAttemptCore(
    const uint32_t* opponents, size_t opponentCount, size_t inventoryCount,
    uint16_t transferCount, PokerogueRngAdapter& battleRng, HeldItemTransferSelection& output,
    const CandidateAt& candidateAt) {
    if ((opponentCount && !opponents) ||
        opponentCount > 0x7FFFFFFFu || inventoryCount > 0x7FFFFFFFu || transferCount > 1)
        return HeldItemTransferSelectionResult::InvalidState;
    HeldItemTransferSelection selection{};
    if (!opponentCount) { output = selection; return HeldItemTransferSelectionResult::NoOpponent; }
    auto nextRng = battleRng;
    selection.opponentIndex = static_cast<size_t>(nextRng.randSeedInt(static_cast<int32_t>(opponentCount)));
    if (!transferCount) {
        battleRng = nextRng;
        output = selection;
        return HeldItemTransferSelectionResult::NoTransferCount;
    }
    size_t eligibleCount = 0;
    for (size_t i = 0; i < inventoryCount; ++i)
        if (candidateAt(i).ownerPokemonId == opponents[selection.opponentIndex] && candidateAt(i).transferable)
            ++eligibleCount;
    if (!eligibleCount) {
        battleRng = nextRng;
        output = selection;
        return HeldItemTransferSelectionResult::NoItem;
    }
    size_t ordinal = static_cast<size_t>(nextRng.randSeedInt(static_cast<int32_t>(eligibleCount)));
    for (size_t i = 0; i < inventoryCount; ++i) {
        if (candidateAt(i).ownerPokemonId != opponents[selection.opponentIndex] || !candidateAt(i).transferable) continue;
        if (ordinal) { --ordinal; continue; }
        selection.inventoryIndex = candidateAt(i).inventoryIndex;
        selection.itemFound = true;
        break;
    }
    battleRng = nextRng;
    output = selection;
    return HeldItemTransferSelectionResult::Selected;
}

inline HeldItemTransferSelectionResult selectHeldItemTransferAttempt(
    const uint32_t* opponents, size_t opponentCount,
    const HeldItemTransferCandidate* inventory, size_t inventoryCount,
    uint16_t transferCount, PokerogueRngAdapter& battleRng, HeldItemTransferSelection& output) {
    if (inventoryCount && !inventory) return HeldItemTransferSelectionResult::InvalidState;
    return selectHeldItemTransferAttemptCore(opponents, opponentCount, inventoryCount,
        transferCount, battleRng, output, [inventory](size_t i) { return inventory[i]; });
}

inline HeldItemTransferSelectionResult selectNativeHeldItemTransferAttempt(
    const uint32_t* opponents, size_t opponentCount,
    const NativeHeldModifierInstance* records, size_t capacity, size_t count,
    uint16_t transferCount, PokerogueRngAdapter& battleRng, HeldItemTransferSelection& output) {
    if ((capacity && !records) || count > capacity || count > 0x7FFFFFFFu)
        return HeldItemTransferSelectionResult::InvalidState;
    for (size_t i = 0; i < count; ++i)
        if (!validateHeldModifierInstance(records[i])) return HeldItemTransferSelectionResult::InvalidState;
    return selectHeldItemTransferAttemptCore(opponents, opponentCount, count,
        transferCount, battleRng, output, [records](size_t i) {
            return HeldItemTransferCandidate{records[i].ownerPokemonId, i, records[i].transferable};
        });
}

enum class ModifierRewardRollResult : uint8_t {
    Ok, InvalidLuck, MissingWeight, InvalidWeight, EmptyPool, MissingItem
};

struct ModifierRewardRoll {
    const PokerogueContent::ModifierPoolEntry* poolEntry = nullptr;
    uint8_t tier = 0;
    uint16_t upgrades = 0;
};

// The caller supplies evaluated, current-party weights. The pinned pool has
// 55 function-valued weights, and even literal weights can be suppressed by
// stack caps in regenerateModifierPoolThresholds. This boundary never treats
// the upstream maxWeight field as a gameplay fallback.
class ModifierRewardWeightProvider {
public:
    virtual ~ModifierRewardWeightProvider() = default;
    virtual bool weightFor(const PokerogueContent::ModifierPoolEntry& entry,
                           uint32_t& weight) const = 0;
};

// Pinned BattleScene starts a fresh Classic run with five ordinary Pokeballs.
// This provider is valid only for the first wild victory, before a modifier
// has been granted, caught Pokemon added, or a lure activated. Later rewards
// require the actual inventory/party model and another weight provider.
class InitialClassicRewardWeights final : public ModifierRewardWeightProvider {
public:
    InitialClassicRewardWeights(const PokemonBattleState& starter,
                                bool hasLearnableLevelMoves)
        : m_starter(starter), m_hasLearnableLevelMoves(hasLearnableLevelMoves) {}

    bool weightFor(const PokerogueContent::ModifierPoolEntry& entry,
                   uint32_t& weight) const override {
        weight = 0;
        if (std::strcmp(entry.pool, "modifierPool") != 0) return false;
        if (!m_starter.maxHp || m_starter.hp > m_starter.maxHp ||
            m_starter.moveCount > 4) return false;
        if (std::strcmp(entry.tier, "GREAT") == 0)
            return greatWeight(entry, weight);
        if (std::strcmp(entry.tier, "COMMON") != 0) return false;
        if (std::strcmp(entry.itemId, "POKEBALL") == 0) { weight = 6; return true; }
        const bool living = m_starter.hp != 0;
        const uint32_t missingHp = m_starter.maxHp - m_starter.hp;
        if (std::strcmp(entry.itemId, "POTION") == 0) {
            weight = living && missingHp >= 10 &&
                8u * m_starter.hp <= 7u * m_starter.maxHp ? 3u : 0u;
            return true;
        }
        if (std::strcmp(entry.itemId, "SUPER_POTION") == 0) {
            weight = living && missingHp >= 25 &&
                4u * m_starter.hp <= 3u * m_starter.maxHp ? 1u : 0u;
            return true;
        }
        if (std::strcmp(entry.itemId, "ETHER") == 0 ||
            std::strcmp(entry.itemId, "MAX_ETHER") == 0) {
            bool lowPp = false;
            if (!hasLowPp(lowPp)) return false;
            weight = living && lowPp
                ? (std::strcmp(entry.itemId, "ETHER") == 0 ? 3u : 1u) : 0u;
            return true;
        }
        if (std::strcmp(entry.itemId, "LURE") == 0) { weight = 2; return true; }
        // Remaining Common entries use literal weights. A fresh profile has
        // no existing modifiers whose stack cap could suppress them.
        if (entry.staticWeight < 0 || entry.staticWeight > 0x7fffffff ||
            entry.staticWeight != static_cast<uint32_t>(entry.staticWeight)) return false;
        weight = static_cast<uint32_t>(entry.staticWeight);
        return true;
    }

private:
    bool hasLowPp(bool& lowPp) const {
        lowPp = false;
        for (uint8_t i = 0; i < m_starter.moveCount; ++i) {
            const auto& move = m_starter.moves[i];
            if (move.pp > move.maxPp) return false;
            const uint32_t used = move.maxPp - move.pp;
            if (used && move.pp <= 5 && used > move.maxPp / 2) lowPp = true;
        }
        return true;
    }

    bool greatWeight(const PokerogueContent::ModifierPoolEntry& entry,
                     uint32_t& weight) const {
        const char* id = entry.itemId;
        const bool living = m_starter.hp != 0;
        const uint32_t missingHp = m_starter.maxHp - m_starter.hp;
        if (std::strcmp(id, "GREAT_BALL") == 0) { weight = 6; return true; }
        // The supported first battle has one living starter and no status,
        // held items, fusion, lures, rerolls, or existing modifiers.
        if (std::strcmp(id, "FULL_HEAL") == 0 ||
            std::strcmp(id, "REVIVE") == 0 ||
            std::strcmp(id, "MAX_REVIVE") == 0 ||
            std::strcmp(id, "SACRED_ASH") == 0 ||
            std::strcmp(id, "FULL_RESTORE") == 0 ||
            std::strcmp(id, "DNA_SPLICERS") == 0) { weight = 0; return true; }
        if (std::strcmp(id, "HYPER_POTION") == 0) {
            weight = living && missingHp >= 100 &&
                8u * m_starter.hp <= 5u * m_starter.maxHp ? 3u : 0u;
            return true;
        }
        if (std::strcmp(id, "MAX_POTION") == 0) {
            weight = living && missingHp >= 100 &&
                2u * m_starter.hp <= m_starter.maxHp ? 1u : 0u;
            return true;
        }
        if (std::strcmp(id, "ELIXIR") == 0 ||
            std::strcmp(id, "MAX_ELIXIR") == 0) {
            bool lowPp = false;
            if (!hasLowPp(lowPp)) return false;
            weight = living && lowPp
                ? (std::strcmp(id, "ELIXIR") == 0 ? 3u : 1u) : 0u;
            return true;
        }
        if (std::strcmp(id, "SUPER_LURE") == 0) { weight = 4; return true; }
        if (std::strcmp(id, "NUGGET") == 0) { weight = 5; return true; }
        if (std::strcmp(id, "EVOLUTION_ITEM") == 0) { weight = 1; return true; }
        if (std::strcmp(id, "MAP") == 0) { weight = 2; return true; }
        if (std::strcmp(id, "MEMORY_MUSHROOM") == 0) {
            weight = m_hasLearnableLevelMoves ? 1u : 0u;
            return true;
        }
        if (std::strcmp(id, "TERA_SHARD") == 0) {
            const auto* species = PokerogueContent::findSpeciesByDex(m_starter.speciesDex);
            if (!species) return false;
            weight = std::strcmp(species->id, "terapagos") &&
                std::strcmp(species->id, "ogerpon") &&
                std::strcmp(species->id, "shedinja") ? 1u : 0u;
            return true;
        }
        if (std::strcmp(id, "VOUCHER") == 0) { weight = 1; return true; }
        if (entry.staticWeight < 0 || entry.staticWeight > 0x7fffffff ||
            entry.staticWeight != static_cast<uint32_t>(entry.staticWeight)) return false;
        weight = static_cast<uint32_t>(entry.staticWeight);
        return true;
    }

    const PokemonBattleState& m_starter;
    bool m_hasLearnableLevelMoves;
};

inline bool hasPlayerModifierTier(uint8_t tier) {
    static constexpr const char* names[] = {"COMMON", "GREAT", "ULTRA", "ROGUE", "MASTER"};
    if (tier >= 5) return false;
    for (const auto& entry : PokerogueContent::kModifierPoolEntries)
        if (std::strcmp(entry.pool, "modifierPool") == 0 &&
            std::strcmp(entry.tier, names[tier]) == 0) return true;
    return false;
}

// One getNewModifierTypeOption tier-and-weight draw. The caller retains the
// resulting RNG stream for duplicate retry and generator resolution, which
// are later phases of SelectModifierPhase in the pinned source.
inline ModifierRewardRollResult rollPlayerModifierReward(
    PokerogueRngAdapter& rng, uint8_t partyLuck,
    const ModifierRewardWeightProvider& weights, ModifierRewardRoll& output) {
    output = {};
    if (partyLuck > 14) return ModifierRewardRollResult::InvalidLuck;
    PokerogueRngAdapter draw = rng;
    const int32_t tierValue = draw.randSeedInt(1024);
    uint16_t upgrades = 0;
    if (tierValue) {
        // Pinned getNewModifierTypeOption: floor(128 / ((luck + 4) / 4)).
        const int32_t upgradeOdds = 512 / (partyLuck + 4);
        bool upgraded;
        do {
            upgraded = draw.randSeedInt(upgradeOdds) < 4;
            if (upgraded) {
                if (upgrades == 0xffff) return ModifierRewardRollResult::InvalidWeight;
                ++upgrades;
            }
        } while (upgraded);
    }
    uint32_t tier = tierValue > 255 ? 0 : tierValue > 60 ? 1 :
        tierValue > 12 ? 2 : tierValue ? 3 : 4;
    tier += upgrades;
    while (tier && !hasPlayerModifierTier(tier)) {
        --tier;
        if (upgrades) --upgrades;
    }
    if (!hasPlayerModifierTier(tier)) return ModifierRewardRollResult::EmptyPool;
    static constexpr const char* names[] = {"COMMON", "GREAT", "ULTRA", "ROGUE", "MASTER"};
    const char* tierName = names[tier];
    uint32_t totalWeight = 0;
    uint32_t evaluated[PokerogueContent::kModifierPoolEntryCount]{};
    for (std::size_t index = 0; index < PokerogueContent::kModifierPoolEntryCount; ++index) {
        const auto& entry = PokerogueContent::kModifierPoolEntries[index];
        if (std::strcmp(entry.pool, "modifierPool") || std::strcmp(entry.tier, tierName)) continue;
        uint32_t& weight = evaluated[index];
        if (!weights.weightFor(entry, weight)) return ModifierRewardRollResult::MissingWeight;
        if (weight > static_cast<uint32_t>(0x7fffffff) - totalWeight)
            return ModifierRewardRollResult::InvalidWeight;
        totalWeight += weight;
    }
    if (!totalWeight) return ModifierRewardRollResult::EmptyPool;
    const uint32_t choice = static_cast<uint32_t>(draw.randSeedInt(static_cast<int32_t>(totalWeight)));
    uint32_t threshold = 0;
    for (std::size_t index = 0; index < PokerogueContent::kModifierPoolEntryCount; ++index) {
        const auto& entry = PokerogueContent::kModifierPoolEntries[index];
        if (std::strcmp(entry.pool, "modifierPool") || std::strcmp(entry.tier, tierName)) continue;
        threshold += evaluated[index];
        if (choice < threshold) {
            for (const auto& item : PokerogueContent::kItems) {
                if (std::strcmp(item.id, entry.itemId) == 0) {
                    output = {&entry, static_cast<uint8_t>(tier), upgrades};
                    rng = draw;
                    return ModifierRewardRollResult::Ok;
                }
            }
            return ModifierRewardRollResult::MissingItem;
        }
    }
    return ModifierRewardRollResult::EmptyPool;
}

} // namespace Pokerogue3DS

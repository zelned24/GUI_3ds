#pragma once

#include "content/PokerogueRuntimeContent.hpp"
#include "game/PokemonBattleState.hpp"
#include "game/PokerogueRngAdapter.hpp"
#include <cstdint>
#include <cstring>

namespace Pokerogue3DS {

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
inline HeldItemTransferSelectionResult selectHeldItemTransferAttempt(
    const uint32_t* opponents, size_t opponentCount,
    const HeldItemTransferCandidate* inventory, size_t inventoryCount,
    uint16_t transferCount, PokerogueRngAdapter& battleRng, HeldItemTransferSelection& output) {
    if ((opponentCount && !opponents) || (inventoryCount && !inventory) ||
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
        if (inventory[i].ownerPokemonId == opponents[selection.opponentIndex] && inventory[i].transferable)
            ++eligibleCount;
    if (!eligibleCount) {
        battleRng = nextRng;
        output = selection;
        return HeldItemTransferSelectionResult::NoItem;
    }
    size_t ordinal = static_cast<size_t>(nextRng.randSeedInt(static_cast<int32_t>(eligibleCount)));
    for (size_t i = 0; i < inventoryCount; ++i) {
        if (inventory[i].ownerPokemonId != opponents[selection.opponentIndex] || !inventory[i].transferable) continue;
        if (ordinal) { --ordinal; continue; }
        selection.inventoryIndex = inventory[i].inventoryIndex;
        selection.itemFound = true;
        break;
    }
    battleRng = nextRng;
    output = selection;
    return HeldItemTransferSelectionResult::Selected;
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

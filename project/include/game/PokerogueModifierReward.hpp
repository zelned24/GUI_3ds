#pragma once

#include "content/PokerogueRuntimeContent.hpp"
#include "game/PokemonBattleState.hpp"
#include "game/PokerogueRngAdapter.hpp"
#include <cstdint>
#include <cstring>

namespace Pokerogue3DS {

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
class InitialClassicCommonWeights final : public ModifierRewardWeightProvider {
public:
    explicit InitialClassicCommonWeights(const PokemonBattleState& starter)
        : m_starter(starter) {}

    bool weightFor(const PokerogueContent::ModifierPoolEntry& entry,
                   uint32_t& weight) const override {
        weight = 0;
        if (std::strcmp(entry.pool, "modifierPool") != 0 ||
            std::strcmp(entry.tier, "COMMON") != 0) return false;
        if (!m_starter.maxHp || m_starter.hp > m_starter.maxHp ||
            m_starter.moveCount > 4) return false;
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
            for (uint8_t i = 0; i < m_starter.moveCount; ++i) {
                const auto& move = m_starter.moves[i];
                if (move.pp > move.maxPp) return false;
                const uint32_t used = move.maxPp - move.pp;
                if (used && move.pp <= 5 && used > move.maxPp / 2) lowPp = true;
            }
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
    const PokemonBattleState& m_starter;
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

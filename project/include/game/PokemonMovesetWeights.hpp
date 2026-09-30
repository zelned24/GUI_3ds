#pragma once
#include "game/PokerogueRngAdapter.hpp"
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace Pokerogue3DS {

// Inputs are resolved by the move/actor adapters. Effective power is not the
// move's printed power: variable-power and category attributes must be resolved
// before calling this stage. No missing metadata is replaced with a guess.
struct PokemonMovesetWeightInput {
    uint16_t moveId;
    uint8_t category; // canonical MovePhysical / MoveSpecial / MoveStatus
    double baseWeight;
    double effectivePower;
    bool usesDefense;
    bool selectsOffensiveCategory;
};

struct PokemonWeightedMove {
    uint16_t moveId;
    uint32_t weight;
};

struct PokemonWildMoveCandidate {
    PokemonWeightedMove weighted{};
    const char* type = nullptr;
    bool damaging = false;
    bool forbiddenStab = false;
    bool sacrificial = false;
};

enum class PokemonMovesetWeightResult : uint8_t {
    Ok, InvalidInput, InsufficientCapacity, WeightOverflow, EmptyPool
};

// Pinned src/ai/ai-moveset-gen.ts: adjustDamageMoveWeights, followed by
// generateMoveset's regular-wild BASE_WEIGHT_MULTIPLIER exponentiation.
// Caller retains insertion order from the upstream Map.
inline PokemonMovesetWeightResult weightWildPokemonMoves(
    const PokemonMovesetWeightInput* input, std::size_t count,
    uint32_t attack, uint32_t specialAttack, uint32_t defense,
    PokemonWeightedMove* output, std::size_t capacity, std::size_t& written) {
    written = 0;
    if ((!input && count) || !attack || !specialAttack || !defense)
        return PokemonMovesetWeightResult::InvalidInput;
    if ((!output && count) || capacity < count) return PokemonMovesetWeightResult::InsufficientCapacity;
    double maxPower = 40;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& move = input[i];
        if (!move.moveId || move.category > 2 || !std::isfinite(move.baseWeight) ||
            (move.category != 2 && (!std::isfinite(move.effectivePower) || move.effectivePower < 0)))
            return PokemonMovesetWeightResult::InvalidInput;
        for (std::size_t j = 0; j < i; ++j)
            if (input[j].moveId == move.moveId) return PokemonMovesetWeightResult::InvalidInput;
        if (move.category != 2 && move.effectivePower > maxPower) maxPower = move.effectivePower;
    }
    if (maxPower > 120) maxPower = 120;
    const double high = attack > specialAttack ? attack : specialAttack;
    const double low = attack < specialAttack ? attack : specialAttack;
    const double ratio = low / high;
    const double rawAdjustment = std::pow(ratio, 3) * 2;
    const double adjustment = rawAdjustment < 1 ? rawAdjustment : 1;
    const uint8_t worseCategory = attack > specialAttack ? 1 : 0;
    const double defRatio = defense / high;
    const double rawDefAdjustment = std::pow(defRatio, 3) * 1.3;
    const double defAdjustment = rawDefAdjustment < 1.1 ? rawDefAdjustment : 1.1;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& move = input[i];
        double weight = move.baseWeight;
        if (move.category != 2) {
            double scale = move.effectivePower / maxPower;
            if (scale < 0.25) scale = 0.25;
            if (scale > 1) scale = 1;
            weight *= scale;
            if (move.usesDefense) weight *= defAdjustment;
            else if (move.category == worseCategory && !move.selectsOffensiveCategory)
                weight *= adjustment;
            // Regular wild actors never instantly tera. Tera category attrs
            // therefore do not bypass the above category reduction.
        }
        if (weight <= 0) continue;
        const double scaled = std::ceil(std::pow(weight, 1.6) * 100);
        if (!std::isfinite(scaled) || scaled > 2147483647.0) {
            written = 0;
            return PokemonMovesetWeightResult::WeightOverflow;
        }
        output[written++] = {move.moveId, static_cast<uint32_t>(scaled)};
    }
    return PokemonMovesetWeightResult::Ok;
}

// Pinned forceStabMove/fillInRemainingMovesetSlots deliberately use `>` at
// interval boundaries. Changing this to >= changes deterministic encounters.
// Eligibility is supplied by the STAB/signature/sacrificial-filter stages.
inline PokemonMovesetWeightResult drawPokemonWeightedMove(
    const PokemonWeightedMove* pool, const bool* eligible, std::size_t count,
    PokerogueRngAdapter& rng, std::size_t& selected) {
    selected = count;
    if (!pool && count) return PokemonMovesetWeightResult::InvalidInput;
    uint32_t total = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (eligible && !eligible[i]) continue;
        if (!pool[i].moveId || !pool[i].weight) return PokemonMovesetWeightResult::InvalidInput;
        if (pool[i].weight > 2147483647u - total) return PokemonMovesetWeightResult::WeightOverflow;
        total += pool[i].weight;
    }
    if (!total) return PokemonMovesetWeightResult::EmptyPool;
    uint32_t roll = static_cast<uint32_t>(rng.randSeedInt(static_cast<int32_t>(total)));
    for (std::size_t i = 0; i < count; ++i) {
        if (eligible && !eligible[i]) continue;
        if (roll <= pool[i].weight) { selected = i; return PokemonMovesetWeightResult::Ok; }
        roll -= pool[i].weight;
    }
    return PokemonMovesetWeightResult::InvalidInput;
}

inline PokemonMovesetWeightResult generatePokemonWildMoveset(
    const PokemonWildMoveCandidate* pool, std::size_t count,
    const char* type1, const char* type2, PokerogueRngAdapter& rng,
    uint16_t outputMoves[4], uint8_t& outputCount) {
    outputCount = 0;
    if ((!pool && count) || !outputMoves || !type1 || !*type1)
        return PokemonMovesetWeightResult::InvalidInput;
    bool poolUsed[64]{};
    if (count > 64) return PokemonMovesetWeightResult::InsufficientCapacity;
    const auto sameType = [](const char* a, const char* b) {
        if (!a || !b) return false;
        while (*a && *b) {
            char left = *a++, right = *b++;
            if (left >= 'a' && left <= 'z') left = static_cast<char>(left - 'a' + 'A');
            if (right >= 'a' && right <= 'z') right = static_cast<char>(right - 'a' + 'A');
            if (left != right) return false;
        }
        return *a == *b;
    };
    bool stabEligible[64]{};
    bool anyStab = false;
    for (std::size_t i = 0; i < count; ++i) {
        if (!pool[i].weighted.moveId || !pool[i].weighted.weight || !pool[i].type) return PokemonMovesetWeightResult::InvalidInput;
        for (std::size_t j = 0; j < i; ++j)
            if (pool[i].weighted.moveId == pool[j].weighted.moveId) return PokemonMovesetWeightResult::InvalidInput;
        stabEligible[i] = pool[i].damaging && !pool[i].forbiddenStab &&
            (sameType(pool[i].type, type1) || (type2 && *type2 && sameType(pool[i].type, type2)));
        anyStab |= stabEligible[i];
    }
    while (outputCount < 4) {
        PokemonWeightedMove remaining[64]{};
        bool remainingEligible[64]{};
        std::size_t sourceIndex[64]{};
        std::size_t remainingCount = 0;
        bool usedSacrificial = false;
        for (uint8_t slot = 0; slot < outputCount; ++slot)
            for (std::size_t j = 0; j < count; ++j)
                if (pool[j].weighted.moveId == outputMoves[slot]) usedSacrificial |= pool[j].sacrificial;
        for (std::size_t i = 0; i < count; ++i) {
            if (poolUsed[i] || (usedSacrificial && pool[i].sacrificial)) continue;
            remaining[remainingCount] = pool[i].weighted;
            sourceIndex[remainingCount] = i;
            remainingEligible[remainingCount] = outputCount != 0 || !anyStab || stabEligible[i];
            ++remainingCount;
        }
        if (!remainingCount) break;
        std::size_t selected = remainingCount;
        const auto result = drawPokemonWeightedMove(remaining, remainingEligible, remainingCount, rng, selected);
        if (result == PokemonMovesetWeightResult::EmptyPool && outputCount == 0 && anyStab) {
            // Upstream's forceStabMove does not fall back to arbitrary damage
            // here. It preserves the pool; the subsequent weighted fill draws.
            for (std::size_t i = 0; i < remainingCount; ++i) remainingEligible[i] = true;
            const auto fill = drawPokemonWeightedMove(remaining, remainingEligible, remainingCount, rng, selected);
            if (fill != PokemonMovesetWeightResult::Ok) return fill;
        } else if (result == PokemonMovesetWeightResult::EmptyPool) break;
        else if (result != PokemonMovesetWeightResult::Ok) return result;
        const std::size_t chosen = sourceIndex[selected];
        outputMoves[outputCount++] = pool[chosen].weighted.moveId;
        poolUsed[chosen] = true;
    }
    return PokemonMovesetWeightResult::Ok;
}
} // namespace Pokerogue3DS

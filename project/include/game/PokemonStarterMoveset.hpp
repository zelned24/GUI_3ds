#pragma once

#include "content/PokerogueRuntimeContent.hpp"
#include <cstddef>
#include <cstdint>

namespace Pokerogue3DS {

// Pinned starter-select-ui-utils.ts/getRunValueLimit, default Classic policy.
inline constexpr uint16_t kClassicStarterValueLimit = 10;
enum class PokemonStarterSelectionResult : uint8_t {
    Ok, InvalidCount, MissingSpecies, IneligibleSpecies, DuplicateSpecies, OverBudget
};
inline PokemonStarterSelectionResult classicStarterSelectionValue(const uint16_t* dexes,
    size_t count, uint16_t& output) {
    if (!dexes || !count || count > 6) return PokemonStarterSelectionResult::InvalidCount;
    uint16_t total = 0;
    for (size_t i = 0; i < count; ++i) {
        const auto* species = PokerogueContent::findSpeciesByDex(dexes[i]);
        if (!species) return PokemonStarterSelectionResult::MissingSpecies;
        if (!species->starterEligible || species->starterCost < 1) return PokemonStarterSelectionResult::IneligibleSpecies;
        for (size_t prior = 0; prior < i; ++prior)
            if (dexes[prior] == dexes[i]) return PokemonStarterSelectionResult::DuplicateSpecies;
        total += static_cast<uint16_t>(species->starterCost);
        if (total > kClassicStarterValueLimit) return PokemonStarterSelectionResult::OverBudget;
    }
    output = total;
    return PokemonStarterSelectionResult::Ok;
}

enum class PokemonStarterMovesetResult : uint8_t {
    Ok = 0, MissingSpecies, InvalidForm, InvalidPreferredMove, MissingMove,
    InvalidEggMoveMask, InsufficientCapacity
};

// Mirrors the pinned fresh/default starter move selection path: level 1-5
// learnset moves, then unlocked egg moves, valid saved preferences first,
// duplicate removal, and a four-move cap. The caller owns unlock/profile data.
inline PokemonStarterMovesetResult selectPokemonStarterMoveset(
    uint16_t speciesDex, const char* formId,
    uint8_t unlockedEggMoveMask,
    const uint16_t* preferredMoveIds, std::size_t preferredMoveCount,
    uint16_t outputMoveIds[4], uint8_t& outputMoveCount) {
    outputMoveCount = 0;
    if (!outputMoveIds || (preferredMoveCount && !preferredMoveIds) || preferredMoveCount > 4)
        return PokemonStarterMovesetResult::InvalidPreferredMove;
    const auto* species = PokerogueContent::findSpeciesByDex(speciesDex);
    if (!species) return PokemonStarterMovesetResult::MissingSpecies;
    if (species->eggMoveCount < 8 && (unlockedEggMoveMask >> species->eggMoveCount) != 0)
        return PokemonStarterMovesetResult::InvalidEggMoveMask;
    const auto* form = formId && *formId ? PokerogueContent::findFormById(formId) : nullptr;
    if (formId && *formId && (!form || !form->speciesId || !species->id))
        return PokemonStarterMovesetResult::InvalidForm;
    const auto sameText = [](const char* left, const char* right) {
        if (!left || !right) return left == right;
        while (*left && *right && *left == *right) { ++left; ++right; }
        return *left == *right;
    };
    if (form && !sameText(form->speciesId, species->id)) return PokemonStarterMovesetResult::InvalidForm;

    struct Candidate { int8_t level; uint16_t moveId; uint16_t order; };
    Candidate candidates[128]{};
    std::size_t candidateCount = 0;
    uint16_t order = 0;
    const auto appendLearnset = [&](const PokerogueContent::SpeciesLevelMove* learnset,
                                    uint16_t count) -> PokemonStarterMovesetResult {
        for (uint16_t i = 0; learnset && i < count; ++i, ++order) {
            const auto& learned = learnset[i];
            if (learned.level <= 0 || learned.level > 5) continue;
            if (!PokerogueContent::findMoveById(learned.moveId)) return PokemonStarterMovesetResult::MissingMove;
            if (candidateCount >= sizeof(candidates) / sizeof(candidates[0]))
                return PokemonStarterMovesetResult::InsufficientCapacity;
            candidates[candidateCount++] = {learned.level, learned.moveId, order};
        }
        return PokemonStarterMovesetResult::Ok;
    };
    auto status = appendLearnset(PokerogueContent::levelMovesFor(*species), species->learnsetCount);
    if (status != PokemonStarterMovesetResult::Ok) return status;
    if (form) {
        status = appendLearnset(PokerogueContent::levelMovesFor(*form), form->learnsetCount);
        if (status != PokemonStarterMovesetResult::Ok) return status;
    }
    for (std::size_t i = 1; i < candidateCount; ++i) {
        const Candidate value = candidates[i];
        std::size_t j = i;
        while (j > 0 && (candidates[j - 1].level > value.level ||
               (candidates[j - 1].level == value.level && candidates[j - 1].order > value.order))) {
            candidates[j] = candidates[j - 1];
            --j;
        }
        candidates[j] = value;
    }

    uint16_t available[132]{};
    std::size_t availableCount = 0;
    for (std::size_t i = 0; i < candidateCount; ++i) {
        bool duplicate = false;
        for (std::size_t j = 0; j < availableCount; ++j) duplicate |= available[j] == candidates[i].moveId;
        if (!duplicate) available[availableCount++] = candidates[i].moveId;
    }
    const auto* eggMoves = PokerogueContent::eggMovesFor(*species);
    for (uint16_t i = 0; eggMoves && i < species->eggMoveCount; ++i) {
        if (!(unlockedEggMoveMask & (1u << i))) continue;
        if (!PokerogueContent::findMoveById(eggMoves[i].moveId)) return PokemonStarterMovesetResult::MissingMove;
        bool duplicate = false;
        for (std::size_t j = 0; j < availableCount; ++j) duplicate |= available[j] == eggMoves[i].moveId;
        if (!duplicate) available[availableCount++] = eggMoves[i].moveId;
    }

    const auto isAvailable = [&](uint16_t moveId) {
        for (std::size_t i = 0; i < availableCount; ++i) if (available[i] == moveId) return true;
        return false;
    };
    for (std::size_t i = 0; i < preferredMoveCount; ++i) {
        const uint16_t moveId = preferredMoveIds[i];
        if (!PokerogueContent::findMoveById(moveId)) return PokemonStarterMovesetResult::MissingMove;
        if (!isAvailable(moveId)) continue;
        bool duplicate = false;
        for (uint8_t j = 0; j < outputMoveCount; ++j) duplicate |= outputMoveIds[j] == moveId;
        if (!duplicate && outputMoveCount < 4) outputMoveIds[outputMoveCount++] = moveId;
    }
    for (std::size_t i = 0; i < availableCount && outputMoveCount < 4; ++i) {
        bool duplicate = false;
        for (uint8_t j = 0; j < outputMoveCount; ++j) duplicate |= outputMoveIds[j] == available[i];
        if (!duplicate) outputMoveIds[outputMoveCount++] = available[i];
    }
    return PokemonStarterMovesetResult::Ok;
}

} // namespace Pokerogue3DS

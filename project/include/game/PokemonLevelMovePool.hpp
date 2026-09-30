#pragma once

#include "content/PokerogueRuntimeContent.hpp"
#include <cstddef>
#include <cstdint>

namespace Pokerogue3DS {

enum PokemonLearnsetSource : uint8_t {
    LearnsetSourceLevel = 0,
    LearnsetSourceRelearn = 2,
    LearnsetSourceEvolution = 4,
    LearnsetSourcePrevolution = 6,
};

struct PokemonLevelMoveCandidate {
    uint16_t moveId = 0;
    uint16_t weight = 0;
    int8_t sourceLevel = 0;
    uint8_t source = LearnsetSourceLevel;
};

enum class PokemonLevelMovePoolResult : uint8_t {
    Ok = 0, MissingSpecies, InvalidLevel, InvalidForm, MissingMove,
    InvalidEvolutionChain, InsufficientCapacity
};

inline bool pokemonLevelMoveTextEqual(const char* left, const char* right) {
    if (!left || !right) return left == right;
    while (*left && *right && *left == *right) { ++left; ++right; }
    return *left == *right;
}

// Builds the pinned level pool before AI power/STAB weighting. Trainers include
// RELEARN_MOVE entries, weighted zero below level 40; wild actors omit them.
// Weighted selection and post-selection move filters remain separate.
inline PokemonLevelMovePoolResult buildPokemonLevelMovePool(
    uint16_t speciesDex, const char* formId, uint16_t level,
    PokemonLevelMoveCandidate* output, std::size_t capacity,
    std::size_t& outputCount, bool includeRelearnerMoves = false) {
    outputCount = 0;
    if (level == 0) return PokemonLevelMovePoolResult::InvalidLevel;
    const auto* species = PokerogueContent::findSpeciesByDex(speciesDex);
    if (!species) return PokemonLevelMovePoolResult::MissingSpecies;
    const auto* form = formId ? PokerogueContent::findFormById(formId) : nullptr;
    if (formId && (!form || !form->speciesId || !species->id ||
                   !pokemonLevelMoveTextEqual(form->speciesId, species->id)))
        return PokemonLevelMovePoolResult::InvalidForm;

    // The selected ordinal is the upstream formIndex. For predecessor species,
    // the registry selects that ordinal and falls back to the base form.
    std::size_t selectedFormIndex = 0;
    if (form) {
        std::size_t ordinal = 0;
        bool found = false;
        for (std::size_t i = 0; i < PokerogueContent::kFormCount; ++i) {
            const auto& candidate = PokerogueContent::kForms[i];
            if (!pokemonLevelMoveTextEqual(candidate.speciesId, species->id)) continue;
            if (&candidate == form) { selectedFormIndex = ordinal; found = true; break; }
            ++ordinal;
        }
        if (!found) return PokemonLevelMovePoolResult::InvalidForm;
    }

    const auto formAtIndexOrBase = [&](const PokerogueContent::Species* owner,
                                       std::size_t index) -> const PokerogueContent::Form* {
        const PokerogueContent::Form* first = nullptr;
        std::size_t ordinal = 0;
        for (std::size_t i = 0; i < PokerogueContent::kFormCount; ++i) {
            const auto& candidate = PokerogueContent::kForms[i];
            if (!pokemonLevelMoveTextEqual(candidate.speciesId, owner->id)) continue;
            if (!first) first = &candidate;
            if (ordinal == index) return &candidate;
            ++ordinal;
        }
        return first;
    };

    if (!form) form = formAtIndexOrBase(species, 0);
    const auto containsOwnLevelMove = [&](uint16_t moveId, bool future) {
        const auto* moves = PokerogueContent::levelMovesFor(*species);
        for (uint16_t i = 0; moves && i < species->learnsetCount; ++i)
            if (moves[i].moveId == moveId && moves[i].level > 0 &&
                (moves[i].level != 1 || !species->prevolutionDex ||
                 includeRelearnerMoves) &&
                ((moves[i].level > level) == future)) return true;
        moves = form ? PokerogueContent::levelMovesFor(*form) : nullptr;
        for (uint16_t i = 0; moves && i < form->learnsetCount; ++i)
            if (moves[i].moveId == moveId && moves[i].level > 0 &&
                (moves[i].level != 1 || !species->prevolutionDex ||
                 includeRelearnerMoves) &&
                ((moves[i].level > level) == future)) return true;
        return false;
    };

    const auto appendRange = [&](const PokerogueContent::SpeciesLevelMove* moves,
                                 uint16_t count, bool isPrevolution,
                                 std::size_t prevolutionIndex) -> PokemonLevelMovePoolResult {
        for (uint16_t i = 0; moves && i < count; ++i) {
            const auto& entry = moves[i];
            if (entry.level < -1 || (entry.level == -1 && !includeRelearnerMoves) ||
                entry.level > level) continue;
            if (entry.level == 1 && prevolutionIndex != 0 &&
                !includeRelearnerMoves) continue;
            if ((isPrevolution || entry.level == 0) && containsOwnLevelMove(entry.moveId, false)) continue;
            if (isPrevolution && containsOwnLevelMove(entry.moveId, true)) continue;
            const auto* move = PokerogueContent::findMoveById(entry.moveId);
            if (!move) return PokemonLevelMovePoolResult::MissingMove;
            if ((move->upstreamFlags & PokerogueContent::MoveIsUnimplemented) ||
                (move->upstreamFlags & PokerogueContent::MoveHasSacrificialAttrOnHit)) continue;

            const uint8_t source = isPrevolution ? LearnsetSourcePrevolution
                : entry.level == -1 ? LearnsetSourceRelearn
                : entry.level == 0 ? LearnsetSourceEvolution : LearnsetSourceLevel;
            // Keep the earliest (level, source) entry for a move, matching the
            // upstream stable sort followed by getUniqueMoves().
            std::size_t duplicate = outputCount;
            for (std::size_t j = 0; j < outputCount; ++j)
                if (output[j].moveId == entry.moveId) { duplicate = j; break; }
            if (duplicate < outputCount) {
                const auto& previous = output[duplicate];
                if (entry.level > previous.sourceLevel ||
                    (entry.level == previous.sourceLevel && source >= previous.source)) continue;
            } else if (!output || outputCount >= capacity) {
                return PokemonLevelMovePoolResult::InsufficientCapacity;
            }

            uint16_t weight = entry.level == -1 ? (level >= 40 ? 50 : 0)
                : entry.level == 0 ? 60 : static_cast<uint16_t>(entry.level + 20);
            if (entry.level == 1 && move->power >= 70) weight = 50;
            const PokemonLevelMoveCandidate candidate{entry.moveId, weight, entry.level, source};
            if (duplicate < outputCount) {
                // Replacement belongs at the later insertion position before
                // stable sorting, just like upstream sort then deduplication.
                for (std::size_t j = duplicate + 1; j < outputCount; ++j) output[j - 1] = output[j];
                output[outputCount - 1] = candidate;
            }
            else output[outputCount++] = candidate;
        }
        return PokemonLevelMovePoolResult::Ok;
    };

    const auto appendSpeciesAndForm = [&](const PokerogueContent::Species* owner,
                                          const PokerogueContent::Form* ownerForm,
                                          bool isPrevolution,
                                          std::size_t prevolutionIndex) -> PokemonLevelMovePoolResult {
        auto result = appendRange(PokerogueContent::levelMovesFor(*owner), owner->learnsetCount,
                                  isPrevolution, prevolutionIndex);
        if (result != PokemonLevelMovePoolResult::Ok) return result;
        return ownerForm && ownerForm->learnsetCount
            ? appendRange(PokerogueContent::levelMovesFor(*ownerForm), ownerForm->learnsetCount,
                          isPrevolution, prevolutionIndex)
            : PokemonLevelMovePoolResult::Ok;
    };
    const auto finalize = [&]() {
        // getLevelMoves sorts by signed level, then LearnableMoveSource. Keep
        // stable order for equal source/level entries from the same catalog.
        for (std::size_t i = 1; i < outputCount; ++i) {
            const auto value = output[i];
            std::size_t j = i;
            while (j > 0 &&
                   (output[j - 1].sourceLevel > value.sourceLevel ||
                    (output[j - 1].sourceLevel == value.sourceLevel &&
                     output[j - 1].source > value.source))) {
                output[j] = output[j - 1];
                --j;
            }
            output[j] = value;
        }
        return PokemonLevelMovePoolResult::Ok;
    };

    // Registry initialization is imported canonically; incoming evolution
    // edges alone do not encode starter resets or mega-form exclusions.
    const PokerogueContent::Species* current = species;
    for (std::size_t depth = 0; depth < PokerogueContent::kSpeciesCount; ++depth) {
        if (!current->prevolutionDex) {
            const auto result = appendSpeciesAndForm(species, form, false, depth);
            if (result != PokemonLevelMovePoolResult::Ok) { outputCount = 0; return result; }
            return finalize();
        }
        const auto* predecessor = PokerogueContent::findSpeciesByDex(current->prevolutionDex);
        if (!predecessor || predecessor == current) {
            outputCount = 0;
            return PokemonLevelMovePoolResult::InvalidEvolutionChain;
        }
        const auto* predecessorForm = formAtIndexOrBase(predecessor, selectedFormIndex);
        const auto result = appendSpeciesAndForm(predecessor, predecessorForm, true, depth);
        if (result != PokemonLevelMovePoolResult::Ok) { outputCount = 0; return result; }
        current = predecessor;
    }
    outputCount = 0;
    return PokemonLevelMovePoolResult::InvalidEvolutionChain;
}

} // namespace Pokerogue3DS

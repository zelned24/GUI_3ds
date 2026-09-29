#pragma once

#include "content/PokerogueRuntimeContent.hpp"
#include <cstddef>
#include <cstdint>

namespace Pokerogue3DS {

struct PokemonLevelMoveCandidate {
    uint16_t moveId = 0;
    uint16_t weight = 0;
    int8_t sourceLevel = 0;
};

enum class PokemonLevelMovePoolResult : uint8_t {
    Ok = 0, MissingSpecies, InvalidLevel, InvalidForm, MissingMove,
    InsufficientCapacity
};

// Builds the supported portion of the pinned regular wild level-move pool.
// Evolution/prevolution merges and move-generation power/STAB weighting are
// intentionally handled by the higher-level moveset generator, not here.
inline PokemonLevelMovePoolResult buildPokemonLevelMovePool(
    uint16_t speciesDex, const char* formId, uint16_t level,
    PokemonLevelMoveCandidate* output, std::size_t capacity,
    std::size_t& outputCount) {
    outputCount = 0;
    if (level == 0 || level > 100) return PokemonLevelMovePoolResult::InvalidLevel;
    const auto* species = PokerogueContent::findSpeciesByDex(speciesDex);
    if (!species) return PokemonLevelMovePoolResult::MissingSpecies;
    const auto* form = formId ? PokerogueContent::findFormById(formId) : nullptr;
    if (formId && (!form || !form->speciesId || !species->id))
        return PokemonLevelMovePoolResult::InvalidForm;
    if (formId) {
        const char* a = form->speciesId;
        const char* b = species->id;
        while (*a && *b && *a == *b) { ++a; ++b; }
        if (*a != *b) return PokemonLevelMovePoolResult::InvalidForm;
    }

    const auto appendRange = [&](const PokerogueContent::SpeciesLevelMove* moves,
                                 uint16_t count) -> PokemonLevelMovePoolResult {
        for (uint16_t i = 0; i < count; ++i) {
            const auto& entry = moves[i];
            // EVOLVE_MOVE (0) is part of the pool; relearner-only (-1) is not
            // available to a regular wild Pokémon without a trainer.
            if (entry.level < 0 || entry.level > static_cast<int8_t>(level)) continue;
            const auto* move = PokerogueContent::findMoveById(entry.moveId);
            if (!move) return PokemonLevelMovePoolResult::MissingMove;
            if ((move->upstreamFlags & PokerogueContent::MoveIsUnimplemented) ||
                (move->upstreamFlags & PokerogueContent::MoveHasSacrificialAttrOnHit)) continue;
            bool duplicate = false;
            for (std::size_t j = 0; j < outputCount; ++j)
                if (output[j].moveId == entry.moveId) { duplicate = true; break; }
            if (duplicate) continue;
            if (!output || outputCount >= capacity)
                return PokemonLevelMovePoolResult::InsufficientCapacity;
            output[outputCount++] = {entry.moveId,
                static_cast<uint16_t>(entry.level == 0 ? 60 : entry.level + 20), entry.level};
        }
        return PokemonLevelMovePoolResult::Ok;
    };

    auto result = appendRange(PokerogueContent::levelMovesFor(*species), species->learnsetCount);
    if (result != PokemonLevelMovePoolResult::Ok) { outputCount = 0; return result; }
    if (form && form->learnsetCount) {
        result = appendRange(PokerogueContent::levelMovesFor(*form), form->learnsetCount);
        if (result != PokemonLevelMovePoolResult::Ok) { outputCount = 0; return result; }
    }
    return PokemonLevelMovePoolResult::Ok;
}

} // namespace Pokerogue3DS

#pragma once

#include "game/PokemonLevelMovePool.hpp"

namespace Pokerogue3DS {

enum class PokemonTrainerMoveFilterResult : uint8_t {
    Ok = 0, InvalidInput, MissingMove, InsufficientCapacity
};

// Pinned src/ai/ai-moveset-gen.ts::filterMovePool hard prohibitions for a
// non-boss trainer. Preserve Map insertion order and exclude zero-weight
// relearn moves. Soft blocklists, ability-sensitive rules, superceded moves,
// weight changes and final selection are separate stages.
inline PokemonTrainerMoveFilterResult filterTrainerHardForbiddenLevelMoves(
    const PokemonLevelMoveCandidate* input, std::size_t count,
    PokemonLevelMoveCandidate* output, std::size_t capacity,
    std::size_t& written) {
    written = 0;
    if ((!input && count) || (!output && count))
        return PokemonTrainerMoveFilterResult::InvalidInput;
    for (std::size_t i = 0; i < count; ++i) {
        const auto* move = PokerogueContent::findMoveById(input[i].moveId);
        if (!move) { written = 0; return PokemonTrainerMoveFilterResult::MissingMove; }
        if (!input[i].weight ||
            (move->upstreamFlags & (PokerogueContent::MoveIsUnimplemented |
                                    PokerogueContent::MoveHasSacrificialAttrOnHit)) ||
            PokerogueContent::moveHasAttribute(*move, "OneHitKOAttr")) continue;
        if (written == capacity) {
            written = 0;
            return PokemonTrainerMoveFilterResult::InsufficientCapacity;
        }
        output[written++] = input[i];
    }
    return PokemonTrainerMoveFilterResult::Ok;
}

} // namespace Pokerogue3DS

#pragma once

#include "game/PokemonLevelMovePool.hpp"

namespace Pokerogue3DS {

enum class PokemonTrainerMoveFilterResult : uint8_t {
    Ok = 0, InvalidInput, MissingMove, InsufficientCapacity
};

struct PokemonTrainerBaseWeightedMove {
    uint16_t moveId = 0;
    double weight = 0;
};

// Pinned filterSupercededMoves snapshots the original Map keys before deleting
// any entry. A replacement still supersedes a move even if that replacement
// is itself removed later in this pass or has zero weight.
inline PokemonTrainerMoveFilterResult filterTrainerSupercededLevelMoves(
    const PokemonLevelMoveCandidate* input, std::size_t count,
    PokemonLevelMoveCandidate* output, std::size_t capacity,
    std::size_t& written) {
    written = 0;
    if ((!input && count) || (!output && count))
        return PokemonTrainerMoveFilterResult::InvalidInput;
    for (std::size_t i = 0; i < count; ++i) {
        if (!PokerogueContent::findMoveById(input[i].moveId)) {
            written = 0;
            return PokemonTrainerMoveFilterResult::MissingMove;
        }
        bool superseded = false;
        for (const auto& edge : PokerogueContent::kMoveSupercedence) {
            if (edge.moveId != input[i].moveId) continue;
            for (std::size_t j = 0; j < count; ++j)
                if (input[j].moveId == edge.replacementMoveId) {
                    superseded = true;
                    break;
                }
            if (superseded) break;
        }
        if (superseded) continue;
        if (written == capacity) {
            written = 0;
            return PokemonTrainerMoveFilterResult::InsufficientCapacity;
        }
        output[written++] = input[i];
    }
    return PokemonTrainerMoveFilterResult::Ok;
}

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

// Pinned adjustWeightsForTrainer, before damage/stat weighting and the 1.6
// exponent. The strong self-boost flag is derived from each upstream
// StatStageChangeAttr's stages and selfTarget constructor arguments.
inline PokemonTrainerMoveFilterResult adjustTrainerLevelMoveBaseWeights(
    const PokemonLevelMoveCandidate* input, std::size_t count,
    PokemonTrainerBaseWeightedMove* output, std::size_t capacity,
    std::size_t& written) {
    written = 0;
    if ((!input && count) || (!output && count))
        return PokemonTrainerMoveFilterResult::InvalidInput;
    if (capacity < count) return PokemonTrainerMoveFilterResult::InsufficientCapacity;
    for (std::size_t i = 0; i < count; ++i) {
        const auto* move = PokerogueContent::findMoveById(input[i].moveId);
        if (!move) { written = 0; return PokemonTrainerMoveFilterResult::MissingMove; }
        if (!input[i].weight) { written = 0; return PokemonTrainerMoveFilterResult::InvalidInput; }
        double weight = input[i].weight;
        if (move->upstreamFlags & PokerogueContent::MoveHasSacrificialAttr) weight *= 0.5;
        if (move->upstreamFlags & PokerogueContent::MoveHasStrongSelfStatBoost) weight *= 1.25;
        if (move->upstreamFlags & (PokerogueContent::MoveIsCharging |
                                   PokerogueContent::MoveHasRecharge)) weight *= 0.7;
        output[written++] = {input[i].moveId, weight};
    }
    return PokemonTrainerMoveFilterResult::Ok;
}

} // namespace Pokerogue3DS

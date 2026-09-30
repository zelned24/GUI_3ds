#pragma once

#include "game/PokemonTrainerMoveFilter.hpp"
#include "game/PokemonWildMovesetGenerator.hpp"

namespace Pokerogue3DS {

enum class PokemonTrainerMovesetResult : uint8_t {
    Ok = 0, InvalidInput, MissingMetadata, WeightFailure,
    SelectionFailure, UnsupportedMove
};

// The pinned trainer path applies adjustWeightsForTrainer before the same
// adjustDamageMoveWeights and 1.6 exponent used by regular wild actors.
// This adapter accepts the already-filtered, adjusted level pool. TM/egg
// inputs and instant-Tera weighting require their own upstream metadata.
inline PokemonTrainerMovesetResult weightTrainerLevelMoves(
    const PokemonTrainerBaseWeightedMove* input, std::size_t count,
    uint16_t abilityId, uint32_t attack, uint32_t specialAttack,
    uint32_t defense, PokemonWeightedMove* output, std::size_t capacity,
    std::size_t& written) {
    written = 0;
    if ((!input && count) || (!output && count) || capacity < count)
        return PokemonTrainerMovesetResult::InvalidInput;
    if (count > 128) return PokemonTrainerMovesetResult::InvalidInput;
    PokemonMovesetWeightInput weights[128]{};
    for (std::size_t i = 0; i < count; ++i) {
        const auto* move = PokerogueContent::findMoveById(input[i].moveId);
        if (!move || input[i].weight <= 0 || !std::isfinite(input[i].weight))
            return PokemonTrainerMovesetResult::InvalidInput;
        PokemonWildMoveRuntimeMetadata metadata{};
        if (!buildPokemonWildMoveRuntimeMetadata(input[i].moveId, abilityId, metadata))
            return PokemonTrainerMovesetResult::MissingMetadata;
        double expectedHits = metadata.expectedHits;
        if (metadata.power.multiHit &&
            expectedPokemonMoveHitCount(metadata.power, metadata.ability.accuracyMultiplier,
                metadata.ability.maxMultiHitValue, 1, expectedHits) != PokemonMovePowerResult::Ok)
            return PokemonTrainerMovesetResult::MissingMetadata;
        double effectivePower = 0;
        if (calculatePokemonMoveEffectivePower(metadata.power, metadata.ability,
                expectedHits, effectivePower) != PokemonMovePowerResult::Ok)
            return PokemonTrainerMovesetResult::MissingMetadata;
        weights[i] = {input[i].moveId, move->category, input[i].weight,
            effectivePower, metadata.usesDefense, metadata.selectsOffensiveCategory};
    }
    return weightWildPokemonMoves(weights, count, attack, specialAttack, defense,
        output, capacity, written) == PokemonMovesetWeightResult::Ok
        ? PokemonTrainerMovesetResult::Ok : PokemonTrainerMovesetResult::WeightFailure;
}

inline bool sameTrainerMoveType(const char* left, const char* right) {
    if (!left || !right) return false;
    while (*left && *right) {
        char a = *left++, b = *right++;
        if (a >= 'a' && a <= 'z') a = static_cast<char>(a - 'a' + 'A');
        if (b >= 'a' && b <= 'z') b = static_cast<char>(b - 'a' + 'A');
        if (a != b) return false;
    }
    return *left == *right;
}

// Pinned forceSignatureMove -> forceStabMove ->
// fillInRemainingMovesetSlots/filterRemainingTrainerMovePool for a normal
// single trainer actor without instant Tera. Conditional filterUselessMoves
// cases are rejected until their source behavior is ported.
inline PokemonTrainerMovesetResult selectTrainerMovesetFromWeightedPool(
    uint16_t speciesDex, bool rivalSignatures, bool instantTera,
    const PokemonWeightedMove* pool, std::size_t count,
    const char* type1, const char* type2, PokerogueRngAdapter& rng,
    uint16_t output[4], uint8_t& outputCount) {
    outputCount = 0;
    if (!output || !type1 || !*type1 || (!pool && count) || count > 128)
        return PokemonTrainerMovesetResult::InvalidInput;
    if (instantTera) return PokemonTrainerMovesetResult::UnsupportedMove;
    const PokerogueContent::Move* moves[128]{};
    bool used[128]{};
    for (std::size_t i = 0; i < count; ++i) {
        moves[i] = PokerogueContent::findMoveById(pool[i].moveId);
        if (!moves[i] || !pool[i].weight || !moves[i]->type || !*moves[i]->type)
            return PokemonTrainerMovesetResult::MissingMetadata;
        if (moves[i]->upstreamFlags & PokerogueContent::MoveHasVariableMovegenType)
            return PokemonTrainerMovesetResult::UnsupportedMove;
        if (PokerogueContent::moveHasAttribute(*moves[i], "TeraMoveCategoryAttr"))
            return PokemonTrainerMovesetResult::UnsupportedMove;
        for (std::size_t j = 0; j < i; ++j)
            if (pool[i].moveId == pool[j].moveId)
                return PokemonTrainerMovesetResult::InvalidInput;
    }
    const auto isStab = [&](std::size_t i) {
        return sameTrainerMoveType(moves[i]->type, type1) ||
            (type2 && *type2 && sameTrainerMoveType(moves[i]->type, type2));
    };
    uint16_t signature = 0;
    if (selectPokemonForcedSignatureMove(speciesDex, rivalSignatures,
            pool, count, rng, signature) != PokemonSignatureMoveResult::Ok)
        return PokemonTrainerMovesetResult::SelectionFailure;
    bool needsStab = true;
    if (signature) {
        for (std::size_t i = 0; i < count; ++i) {
            if (pool[i].moveId != signature) continue;
            output[outputCount++] = signature;
            used[i] = true;
            needsStab = moves[i]->category == PokerogueContent::MoveStatus || !isStab(i);
            break;
        }
        if (!outputCount) return PokemonTrainerMovesetResult::SelectionFailure;
    }
    if (needsStab) {
        bool eligible[128]{};
        bool any = false;
        for (std::size_t i = 0; i < count; ++i) {
            eligible[i] = !used[i] && moves[i]->category != PokerogueContent::MoveStatus &&
                !(moves[i]->upstreamFlags & PokerogueContent::MoveIsStabBlacklisted) && isStab(i);
            any |= eligible[i];
        }
        if (any) {
            std::size_t chosen = count;
            if (drawPokemonWeightedMove(pool, eligible, count, rng, chosen) !=
                    PokemonMovesetWeightResult::Ok || chosen >= count)
                return PokemonTrainerMovesetResult::SelectionFailure;
            output[outputCount++] = pool[chosen].moveId;
            used[chosen] = true;
        }
    }
    while (outputCount < 4) {
        uint8_t damagingChosen = 0;
        bool hasSacrificial = false;
        for (std::size_t i = 0; i < count; ++i) {
            if (!used[i]) continue;
            if (moves[i]->power > 1) ++damagingChosen;
            hasSacrificial |= (moves[i]->upstreamFlags & PokerogueContent::MoveHasSacrificialAttr) != 0;
        }
        const double denominatorCandidate = std::pow(4.0, damagingChosen) / 8.0;
        const double denominator = denominatorCandidate > 0.5 ? denominatorCandidate : 0.5;
        PokemonWeightedMove remaining[128]{};
        std::size_t sourceIndex[128]{};
        std::size_t remainingCount = 0;
        for (std::size_t i = 0; i < count; ++i) {
            if (used[i] || (hasSacrificial &&
                    (moves[i]->upstreamFlags & PokerogueContent::MoveHasSacrificialAttr))) continue;
            double weight = pool[i].weight;
            if (moves[i]->category != PokerogueContent::MoveStatus) {
                bool overlappingType = false;
                if (!sameTrainerMoveType(moves[i]->type, "UNKNOWN")) {
                    for (std::size_t j = 0; j < count; ++j) {
                        if (!used[j] || moves[j]->category == PokerogueContent::MoveStatus ||
                            PokerogueContent::moveHasAttribute(*moves[j], "FixedDamageAttr")) continue;
                        if (sameTrainerMoveType(moves[j]->type, moves[i]->type)) {
                            overlappingType = true;
                            break;
                        }
                    }
                }
                if (overlappingType) weight = std::sqrt(weight);
                else {
                    weight /= denominator;
                    if (isStab(i) && !(moves[i]->upstreamFlags & PokerogueContent::MoveIsStabBlacklisted))
                        weight *= 20;
                }
                weight = std::ceil(weight);
            }
            if (!std::isfinite(weight) || weight <= 0 || weight > 2147483647.0)
                return PokemonTrainerMovesetResult::WeightFailure;
            remaining[remainingCount] = {pool[i].moveId, static_cast<uint32_t>(weight)};
            sourceIndex[remainingCount++] = i;
        }
        if (!remainingCount) break;
        std::size_t selected = remainingCount;
        if (drawPokemonWeightedMove(remaining, nullptr, remainingCount, rng, selected) !=
                PokemonMovesetWeightResult::Ok || selected >= remainingCount)
            return PokemonTrainerMovesetResult::SelectionFailure;
        const std::size_t chosen = sourceIndex[selected];
        output[outputCount++] = pool[chosen].moveId;
        used[chosen] = true;
    }
    for (std::size_t i = 0; i < count; ++i)
        if (used[i] && (moves[i]->upstreamFlags & PokerogueContent::MoveRequiresPostSelectionFilter)) {
            outputCount = 0;
            return PokemonTrainerMovesetResult::UnsupportedMove;
        }
    return PokemonTrainerMovesetResult::Ok;
}

} // namespace Pokerogue3DS

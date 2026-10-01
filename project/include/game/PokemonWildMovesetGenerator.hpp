#pragma once
#include "game/PokemonLevelMovePool.hpp"
#include "game/PokemonBattleState.hpp"
#include "game/PokemonMoveEffectivePower.hpp"
#include "game/PokemonMovesetWeights.hpp"

namespace Pokerogue3DS {

inline const PokerogueContent::FixedEnemyMoveset* findPokemonFixedEnemyMoveset(
    uint16_t speciesDex, uint8_t formIndex) {
    for (const auto& profile : PokerogueContent::kFixedEnemyMovesets)
        if (profile.speciesDex == speciesDex && profile.formIndex == formIndex) return &profile;
    return nullptr;
}

inline bool applyPokemonFixedEnemyMovePp(const PokerogueContent::FixedEnemyMoveset& profile,
    PokemonBattleState& state) {
    if (state.speciesDex != profile.speciesDex || state.moveCount != 4) return false;
    auto next = state;
    for (uint8_t slot = 0; slot < 4; ++slot) {
        const auto* move = PokerogueContent::findMoveById(profile.moveIds[slot]);
        if (!move || next.moves[slot].moveId != move->id || move->pp <= 0) return false;
        // PokemonMove.getMovePp uses toDmgValue (floor, minimum one) for each PP Up.
        const int32_t maxPp = move->pp + profile.ppUp[slot] * (move->pp / 5 > 0 ? move->pp / 5 : 1);
        if (maxPp < 1 || maxPp > 255 || profile.ppUsed[slot] > maxPp) return false;
        next.moves[slot].maxPp = static_cast<uint8_t>(maxPp);
        next.moves[slot].pp = static_cast<uint8_t>(maxPp - profile.ppUsed[slot]);
    }
    state = next;
    return true;
}

struct PokemonWildMoveRuntimeMetadata {
    PokemonMovePowerMetadata power{};
    PokemonMovePowerAbilityState ability{};
    double expectedHits = -1;
    const char* type = nullptr;
    bool usesDefense = false;
    bool selectsOffensiveCategory = false;
    bool forbiddenStab = false;
    bool sacrificial = false;
};

// Derives the supported declarative move/ability metadata directly from the
// pinned runtime catalog. Unknown AI callbacks fail closed before RNG use.
inline bool buildPokemonWildMoveRuntimeMetadata(
    uint16_t moveId, uint16_t abilityId, PokemonWildMoveRuntimeMetadata& output) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    const auto* ability = PokerogueContent::findAbilityMovegenProfile(abilityId);
    if (!move || !ability || (ability->flags & PokerogueContent::AbilityMovegenUnsupported) ||
        (move->upstreamFlags & PokerogueContent::MoveIsUnimplemented)) return false;
    PokemonMovePowerMetadata power{};
    if (!getPokemonMovePowerMetadata(moveId, power)) return false;
    PokemonWildMoveRuntimeMetadata next{};
    next.power = power;
    next.ability = {1.0, ability->accuracyMultiplier, true,
        (ability->flags & PokerogueContent::AbilityMovegenMaxMultiHit) != 0,
        (ability->flags & PokerogueContent::AbilityMovegenInstantCharge) != 0};
    if ((ability->flags & PokerogueContent::AbilityMovegenHustle) &&
        move->category == PokerogueContent::MovePhysical) {
        next.ability.accuracyMultiplier *= 0.8;
        next.ability.powerMultiplier *= 1.5;
    }
    if ((ability->flags & PokerogueContent::AbilityMovegenAnalytic) &&
        move->priority < 0) next.ability.powerMultiplier *= 1.3;
    next.type = move->type;
    next.usesDefense = (move->upstreamFlags & PokerogueContent::MoveUsesDefense) != 0;
    next.selectsOffensiveCategory = (move->upstreamFlags & PokerogueContent::MoveSelectsOffensiveCategory) != 0;
    next.forbiddenStab = (move->upstreamFlags & PokerogueContent::MoveIsStabBlacklisted) != 0;
    next.sacrificial = (move->upstreamFlags & (PokerogueContent::MoveHasSacrificialAttr |
        PokerogueContent::MoveHasSacrificialAttrOnHit)) != 0;
    output = next;
    return true;
}

enum class PokemonWildMovesetResult : uint8_t {
    Ok, PoolFailure, MissingMetadata, InvalidMetadata, WeightFailure, SelectionFailure
};

// Full native handoff between imported learnsets and the pinned regular-wild
// weighted picker. Every candidate must have attribute-resolved move data;
// partial/missing metadata fails before consuming any RNG.
inline PokemonWildMovesetResult generateWildMovesetFromLearnset(
    uint16_t speciesDex, const char* formId, uint16_t level,
    uint32_t attack, uint32_t specialAttack, uint32_t defense,
    const char* type1, const char* type2,
    const PokemonWildMoveRuntimeMetadata* metadata, std::size_t metadataCount,
    PokerogueRngAdapter& rng, uint16_t output[4], uint8_t& outputCount) {
    outputCount = 0;
    if ((!metadata && metadataCount) || !output) return PokemonWildMovesetResult::MissingMetadata;
    PokemonLevelMoveCandidate levelPool[128]{};
    std::size_t levelCount = 0;
    if (buildPokemonLevelMovePool(speciesDex, formId, level, levelPool, 128, levelCount) !=
        PokemonLevelMovePoolResult::Ok) return PokemonWildMovesetResult::PoolFailure;
    if (levelCount != metadataCount) return PokemonWildMovesetResult::MissingMetadata;
    PokemonMovesetWeightInput inputs[128]{};
    PokemonWildMoveCandidate weighted[128]{};
    for (std::size_t i = 0; i < levelCount; ++i) {
        const auto& raw = metadata[i];
        const auto& candidate = levelPool[i];
        if (!raw.type || !*raw.type || raw.power.category > 2 || raw.power.power < -1 ||
            !candidate.moveId) return PokemonWildMovesetResult::MissingMetadata;
        if (raw.power.category == 2) {
            if (!std::isfinite(raw.ability.powerMultiplier) || !std::isfinite(raw.ability.accuracyMultiplier))
                return PokemonWildMovesetResult::MissingMetadata;
        }
        PokemonMovePowerMetadata powerMetadata{};
        if (!getPokemonMovePowerMetadata(candidate.moveId, powerMetadata)) return PokemonWildMovesetResult::MissingMetadata;
        const auto* canonicalMove = PokerogueContent::findMoveById(candidate.moveId);
        const auto flags = canonicalMove ? canonicalMove->upstreamFlags : 0;
        const auto sameType = [](const char* left, const char* right) {
            if (!left || !right) return false;
            while (*left && *right) {
                char a = *left++, b = *right++;
                if (a >= 'a' && a <= 'z') a = static_cast<char>(a - 'a' + 'A');
                if (b >= 'a' && b <= 'z') b = static_cast<char>(b - 'a' + 'A');
                if (a != b) return false;
            }
            return *left == *right;
        };
        if (powerMetadata.category != raw.power.category || powerMetadata.power != raw.power.power ||
            powerMetadata.accuracy != raw.power.accuracy || powerMetadata.multiHit != raw.power.multiHit ||
            powerMetadata.multiHitType != raw.power.multiHitType || powerMetadata.checksAccuracyPerHit != raw.power.checksAccuracyPerHit ||
            powerMetadata.delayedAttack != raw.power.delayedAttack || powerMetadata.recharge != raw.power.recharge ||
            powerMetadata.charging != raw.power.charging || powerMetadata.multiHitPowerIncrement != raw.power.multiHitPowerIncrement ||
            raw.usesDefense != ((flags & PokerogueContent::MoveUsesDefense) != 0) ||
            raw.selectsOffensiveCategory != ((flags & PokerogueContent::MoveSelectsOffensiveCategory) != 0) ||
            raw.forbiddenStab != ((flags & PokerogueContent::MoveIsStabBlacklisted) != 0) ||
            !canonicalMove || !canonicalMove->type || !sameType(raw.type, canonicalMove->type) ||
            !raw.ability.maxMultiHitHolderPresent)
            return PokemonWildMovesetResult::MissingMetadata;
        double expectedHits = raw.expectedHits;
        if (powerMetadata.multiHit) {
            if (expectedPokemonMoveHitCount(powerMetadata, raw.ability.accuracyMultiplier,
                    raw.ability.maxMultiHitValue, 1, expectedHits) !=
                PokemonMovePowerResult::Ok) return PokemonWildMovesetResult::MissingMetadata;
        }
        double effectivePower = 0;
        if (calculatePokemonMoveEffectivePower(powerMetadata, raw.ability, expectedHits, effectivePower) !=
            PokemonMovePowerResult::Ok) return PokemonWildMovesetResult::MissingMetadata;
        inputs[i] = {candidate.moveId, raw.power.category, static_cast<double>(candidate.weight), effectivePower,
                     raw.usesDefense, raw.selectsOffensiveCategory};
        weighted[i].type = raw.type;
        weighted[i].damaging = raw.power.category != 2;
        weighted[i].forbiddenStab = raw.forbiddenStab;
        weighted[i].sacrificial = (flags & (PokerogueContent::MoveHasSacrificialAttr | PokerogueContent::MoveHasSacrificialAttrOnHit)) != 0;
    }
    PokemonWeightedMove results[128]{};
    std::size_t resultCount = 0;
    if (weightWildPokemonMoves(inputs, levelCount, attack, specialAttack, defense,
            results, 128, resultCount) != PokemonMovesetWeightResult::Ok)
        return PokemonWildMovesetResult::WeightFailure;
    for (std::size_t i = 0; i < resultCount; ++i) weighted[i].weighted = results[i];
    if (generatePokemonWildMoveset(weighted, resultCount, type1, type2, rng, output, outputCount) !=
        PokemonMovesetWeightResult::Ok) return PokemonWildMovesetResult::SelectionFailure;
    return PokemonWildMovesetResult::Ok;
}

} // namespace Pokerogue3DS

#pragma once
#include "content/PokerogueRuntimeContent.hpp"
#include <cmath>
#include <cstdint>

namespace Pokerogue3DS {

struct PokemonMovePowerMetadata {
    uint8_t category;
    int16_t power;
    int16_t accuracy;
    bool multiHitPowerIncrement;
    bool multiHit;
    bool delayedAttack;
    bool recharge;
    bool charging;
    uint8_t multiHitType; // pinned MultiHitType enum: TWO_TO_FIVE, TWO, THREE, TEN, BEAT_UP
    bool checksAccuracyPerHit;
};

inline bool getPokemonMovePowerMetadata(uint16_t moveId, PokemonMovePowerMetadata& output) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move) return false;
    const auto flags = move->upstreamFlags;
    output.category = move->category;
    output.power = move->power;
    output.accuracy = move->accuracy;
    output.multiHitPowerIncrement = (flags & PokerogueContent::MoveHasMultiHitPowerIncrement) != 0;
    output.multiHit = (flags & PokerogueContent::MoveHasMultiHit) != 0;
    output.delayedAttack = (flags & PokerogueContent::MoveHasDelayedAttack) != 0;
    output.recharge = (flags & PokerogueContent::MoveHasRecharge) != 0;
    output.charging = (flags & PokerogueContent::MoveIsCharging) != 0;
    output.multiHitType = move->multiHitType;
    output.checksAccuracyPerHit = (flags & PokerogueContent::MoveChecksAccuracyPerHit) != 0;
    return true;
}

// Adapter output after AiMovegenMoveStatsAbAttr/VariableMovePowerAbAttr.
// The pinned TS checks the holder object's presence in three conditions, not
// its .value. Keep that distinction: changing it changes upstream selection.
struct PokemonMovePowerAbilityState {
    double powerMultiplier;
    double accuracyMultiplier;
    bool maxMultiHitHolderPresent;
    bool maxMultiHitValue;
    bool instantChargeHolderPresent;
};

enum class PokemonMovePowerResult : uint8_t { Ok, InvalidMetadata, ExpectedHitsRequired };

inline PokemonMovePowerResult expectedPokemonMoveHitCount(
    const PokemonMovePowerMetadata& move, double accuracyMultiplier,
    bool maxMultiHit, double partySize, double& output) {
    output = 0;
    if (!move.multiHit) { output = 1; return PokemonMovePowerResult::Ok; }
    if (move.multiHitType < 1 || move.multiHitType > 5 || partySize < 1 ||
        !std::isfinite(partySize) || !std::isfinite(accuracyMultiplier) || accuracyMultiplier < 0)
        return PokemonMovePowerResult::InvalidMetadata;
    if (move.multiHitType == 1) output = maxMultiHit ? 5 : 3.1;
    else if (move.multiHitType == 2) output = 2;
    else if (move.multiHitType == 3) output = 3;
    else if (move.multiHitType == 4) output = 10;
    else output = partySize / 2 > 1 ? partySize / 2 : 1;
    if (move.accuracy == -1) return PokemonMovePowerResult::Ok;
    const double accuracy = (move.accuracy / 100.0) * accuracyMultiplier < 1.0
        ? (move.accuracy / 100.0) * accuracyMultiplier : 1.0;
    if (move.checksAccuracyPerHit && !maxMultiHit) {
        if (accuracy == 1) output *= 1;
        else output = (accuracy * (1.0 - std::pow(accuracy, output))) / (1.0 - accuracy);
    } else output *= accuracy;
    return std::isfinite(output) ? PokemonMovePowerResult::Ok : PokemonMovePowerResult::InvalidMetadata;
}

// src/data/moves/move.ts::Move.calculateEffectivePower at pinned revision
// 8555c08c823b856cbec4eb99ca84ea52a955836d. Expected hit count must come from
// the imported MultiHitAttr variant; a negative value explicitly means absent.
inline PokemonMovePowerResult calculatePokemonMoveEffectivePower(
    const PokemonMovePowerMetadata& move,
    const PokemonMovePowerAbilityState& ability,
    double expectedHits, double& output) {
    output = 0;
    if (move.category > 2) return PokemonMovePowerResult::InvalidMetadata;
    if (move.category == 2 || move.power <= 0) return PokemonMovePowerResult::Ok;
    if (!std::isfinite(ability.powerMultiplier) || ability.powerMultiplier < 0 ||
        !std::isfinite(ability.accuracyMultiplier) || ability.accuracyMultiplier < 0 ||
        move.accuracy < -1) return PokemonMovePowerResult::InvalidMetadata;
    double power;
    if (move.multiHitPowerIncrement) {
        if (ability.maxMultiHitHolderPresent || ability.accuracyMultiplier * move.accuracy > 100)
            power = move.power * ability.powerMultiplier * 6 * ability.accuracyMultiplier;
        else
            power = 47.07 * ability.powerMultiplier * (move.power / 10.0);
    } else if (move.multiHit && !ability.maxMultiHitHolderPresent) {
        if (!std::isfinite(expectedHits) || expectedHits < 0) return PokemonMovePowerResult::ExpectedHitsRequired;
        power = expectedHits * move.power;
    } else {
        const double adjustedAccuracy = move.accuracy * ability.accuracyMultiplier;
        const double accuracy = move.accuracy == -1 ? 1
            : (adjustedAccuracy < 100 ? adjustedAccuracy : 100) / 100;
        power = move.power * ability.powerMultiplier * accuracy;
    }
    unsigned turns = 1;
    if (move.delayedAttack) turns += 2;
    if (move.recharge) ++turns;
    if (move.charging && !ability.instantChargeHolderPresent) ++turns;
    output = power / turns;
    if (!std::isfinite(output)) { output = 0; return PokemonMovePowerResult::InvalidMetadata; }
    return PokemonMovePowerResult::Ok;
}

} // namespace Pokerogue3DS

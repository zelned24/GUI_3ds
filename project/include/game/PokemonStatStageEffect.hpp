#pragma once

#include "game/PokemonBattleState.hpp"
#include "game/PokerogueRngAdapter.hpp"

namespace Pokerogue3DS {

enum class PokemonStatStageEffectResult : uint8_t {
    Ok, InvalidDefinition, InvalidState, UnresolvedPolicy
};

// Resolved by field/ability dispatch before this phase. Missing policy must
// never silently become neutral behavior for a real Pokemon's ability.
struct PokemonStatStageEffectPolicy {
    bool resolved = false;
    int8_t stageMultiplier = 1;
    uint8_t cancelledStatMask = 0;
    int16_t chance = -1;
};

struct PokemonStatStageEffectEvent {
    bool triggered = false;
    uint8_t changedStatMask = 0;
    int8_t changes[7]{};
};

// Pinned StatStageChangeAttr.apply + the resolved mutation part of
// StatStageChangePhase. Animation, ability reactions and messages consume the
// event separately; this resolver has no presentation dependencies.
inline PokemonStatStageEffectResult applyPokemonStatStageEffect(
    PokemonBattleState& recipient, const PokerogueContent::MoveStatStageEffect& effect,
    const PokemonStatStageEffectPolicy& policy, PokerogueRngAdapter& battleRng,
    PokemonStatStageEffectEvent& output) {
    if (!policy.resolved) return PokemonStatStageEffectResult::UnresolvedPolicy;
    if (!PokerogueContent::findMoveById(effect.moveId) || !effect.statMask ||
        effect.statMask > 127 || effect.stages < -6 || effect.stages > 6 ||
        policy.stageMultiplier < -6 || policy.stageMultiplier > 6 ||
        policy.cancelledStatMask > 127 || policy.chance < -1 || policy.chance > 100)
        return PokemonStatStageEffectResult::InvalidDefinition;
    if (!recipient.hp) return PokemonStatStageEffectResult::InvalidState;
    for (int8_t stage : recipient.statStages)
        if (stage < -6 || stage > 6) return PokemonStatStageEffectResult::InvalidState;
    PokemonStatStageEffectEvent next{};
    // Upstream guaranteed effects bypass the chance draw, including -1.
    if (policy.chance >= 0 && policy.chance < 100 &&
        battleRng.randSeedInt(100) >= policy.chance) {
        output = next;
        return PokemonStatStageEffectResult::Ok;
    }
    next.triggered = true;
    for (uint8_t stat = 0; stat < 7; ++stat) {
        const uint8_t bit = static_cast<uint8_t>(1u << stat);
        if (!(effect.statMask & bit) || (policy.cancelledStatMask & bit)) continue;
        const int before = recipient.statStages[stat];
        const int requested = before + effect.stages * policy.stageMultiplier;
        const int after = requested < -6 ? -6 : requested > 6 ? 6 : requested;
        recipient.statStages[stat] = static_cast<int8_t>(after);
        next.changes[stat] = static_cast<int8_t>(after - before);
        if (after != before) next.changedStatMask |= bit;
    }
    output = next;
    return PokemonStatStageEffectResult::Ok;
}

} // namespace Pokerogue3DS

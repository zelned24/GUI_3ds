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

struct ResolvedStatStageAbilityComponent {
    const PokerogueContent::AbilityStatStageProfile* profile = nullptr;
    bool applies = false; // Ability/passive suppression and bypass resolved upstream.
};

// Compose constant ability components without treating them as a complete
// ability dispatcher. The caller's resolved flag and chance are preserved;
// field protection, reflection and post-change triggers remain separate.
inline bool composePokemonStatStageAbilityPolicy(
    const PokerogueContent::MoveStatStageEffect& effect,
    const ResolvedStatStageAbilityComponent* components, uint8_t count,
    bool ignoreMultiplierAbilities, PokemonStatStageEffectPolicy& policy) {
    if (count > 2 || (count && !components) || !effect.statMask || effect.statMask > 127)
        return false;
    int multiplier = policy.stageMultiplier;
    for (uint8_t i = 0; i < count; ++i) {
        if (!components[i].applies) continue;
        const auto* profile = components[i].profile;
        if (!profile || profile->multiplier < -6 || profile->multiplier > 6 ||
            profile->protectedMask > 127) return false;
        if (!ignoreMultiplierAbilities) multiplier *= profile->multiplier;
    }
    if (multiplier < -6 || multiplier > 6) return false;
    uint8_t cancelled = policy.cancelledStatMask;
    // Stage multipliers precede drop protection in StatStageChangePhase.
    // A Contrary-converted raise is therefore not blocked as a drop.
    if (!effect.selfTarget && effect.stages * multiplier < 0)
        for (uint8_t i = 0; i < count; ++i)
            if (components[i].applies)
                cancelled |= components[i].profile->protectedMask & effect.statMask;
    policy.stageMultiplier = static_cast<int8_t>(multiplier);
    policy.cancelledStatMask = cancelled;
    return true;
}

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

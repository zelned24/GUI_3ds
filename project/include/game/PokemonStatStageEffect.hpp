#pragma once

#include "game/PokemonBattleState.hpp"
#include "game/PokerogueRngAdapter.hpp"
#include <cmath>
#include <cstring>

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
    uint8_t reflectedStatMask = 0;
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
    bool ignoreMultiplierAbilities, PokemonStatStageEffectPolicy& policy,
    bool allowReflection = false) {
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
    uint8_t reflected = 0;
    if (allowReflection && !effect.selfTarget && effect.stages * multiplier < 0)
        for (uint8_t i = 0; i < count; ++i)
            if (components[i].applies && components[i].profile->reflectDrops)
                reflected |= effect.statMask & static_cast<uint8_t>(~cancelled);
    cancelled |= reflected;
    policy.reflectedStatMask = reflected;
    policy.stageMultiplier = static_cast<int8_t>(multiplier);
    policy.cancelledStatMask = cancelled;
    return true;
}

struct PokemonStatStageEffectEvent {
    bool triggered = false;
    uint8_t changedStatMask = 0;
    uint8_t reflectedStatMask = 0;
    int8_t reflectedStages = 0;
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
        policy.cancelledStatMask > 127 || policy.reflectedStatMask > 127 ||
        (policy.reflectedStatMask & ~policy.cancelledStatMask) || policy.chance < -1 || policy.chance > 100)
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
    next.reflectedStatMask = policy.reflectedStatMask & effect.statMask;
    next.reflectedStages = next.reflectedStatMask
        ? static_cast<int8_t>(effect.stages * policy.stageMultiplier) : 0;
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

// A reflected phase has no move-chance roll: the original attribute already
// triggered. Its sourceEffectType is MIRROR_ARMOR, forbidding another reflection.
inline PokemonStatStageEffectResult applyReflectedPokemonStatStages(
    PokemonBattleState& source, const PokemonStatStageEffectEvent& reflection,
    const PokemonStatStageEffectPolicy& sourcePolicy, PokemonStatStageEffectEvent& output) {
    if (!sourcePolicy.resolved || sourcePolicy.reflectedStatMask)
        return PokemonStatStageEffectResult::UnresolvedPolicy;
    if (reflection.reflectedStatMask > 127 || reflection.reflectedStages < -36 ||
        reflection.reflectedStages > 36 || sourcePolicy.stageMultiplier < -6 ||
        sourcePolicy.stageMultiplier > 6 || sourcePolicy.cancelledStatMask > 127)
        return PokemonStatStageEffectResult::InvalidDefinition;
    PokemonStatStageEffectEvent event{};
    if (!reflection.triggered || !reflection.reflectedStatMask || !source.hp) {
        output = event;
        return PokemonStatStageEffectResult::Ok;
    }
    for (int8_t stage : source.statStages)
        if (stage < -6 || stage > 6) return PokemonStatStageEffectResult::InvalidState;
    event.triggered = true;
    const int requestedChange = reflection.reflectedStages * sourcePolicy.stageMultiplier;
    for (uint8_t stat = 0; stat < 7; ++stat) {
        const uint8_t bit = static_cast<uint8_t>(1u << stat);
        if (!(reflection.reflectedStatMask & bit) || (sourcePolicy.cancelledStatMask & bit)) continue;
        const int before = source.statStages[stat];
        const int requested = before + requestedChange;
        const int after = requested < -6 ? -6 : requested > 6 ? 6 : requested;
        source.statStages[stat] = static_cast<int8_t>(after);
        event.changes[stat] = static_cast<int8_t>(after - before);
        if (after != before) event.changedStatMask |= bit;
    }
    output = event;
    return PokemonStatStageEffectResult::Ok;
}

struct PokemonStatStageMovePolicy {
    bool hitPolicyResolved = false;
    bool blockedBeforeAccuracy = false;
    bool bypassAccuracy = false;
    double accuracyMultiplier = 1.0;
    PokemonStatStageEffectPolicy stagePolicy{};
};

struct PokemonStatStageMoveEvent {
    bool hit = false;
    bool accuracyRolled = false;
    uint8_t accuracyRoll = 0;
    PokemonStatStageEffectEvent stages{};
};

// One-target status action with a single constant StatStageChangeAttr.
// Policy includes resolved immunity/protection/reflection and ability/field
// interactions. Unsupported definitions do not consume PP or RNG.
inline PokemonStatStageEffectResult usePokemonStatStageStatusMove(
    PokemonBattleState& user, PokemonBattleState& target, uint8_t slot,
    const PokemonStatStageMovePolicy& policy, PokerogueRngAdapter& battleRng,
    PokemonStatStageMoveEvent& output) {
    if (!policy.hitPolicyResolved || !policy.stagePolicy.resolved)
        return PokemonStatStageEffectResult::UnresolvedPolicy;
    if (slot >= user.moveCount || slot >= 4 || !user.moves[slot].pp || !user.hp)
        return PokemonStatStageEffectResult::InvalidState;
    const auto* move = PokerogueContent::findMoveById(user.moves[slot].moveId);
    if (!move || move->category != PokerogueContent::MoveStatus || move->attributeCount != 1 ||
        !PokerogueContent::moveHasAttribute(*move, "StatStageChangeAttr") ||
        !move->target || move->accuracy < -1 || move->accuracy > 100 ||
        !std::isfinite(policy.accuracyMultiplier) || policy.accuracyMultiplier < 0)
        return PokemonStatStageEffectResult::InvalidDefinition;
    const bool self = std::strcmp(move->target, "USER") == 0;
    if (!self && std::strcmp(move->target, "NEAR_ENEMY") != 0 &&
        std::strcmp(move->target, "ALL_NEAR_ENEMIES") != 0)
        return PokemonStatStageEffectResult::InvalidDefinition;
    if (!self && (!target.hp || &user == &target))
        return PokemonStatStageEffectResult::InvalidState;
    const PokerogueContent::MoveStatStageEffect* effect = nullptr;
    for (const auto& entry : PokerogueContent::kMoveStatStageEffects)
        if (entry.moveId == move->id) {
            if (effect) return PokemonStatStageEffectResult::InvalidDefinition;
            effect = &entry;
        }
    if (!effect) return PokemonStatStageEffectResult::InvalidDefinition;
    PokemonBattleState nextUser = user, nextTarget = target;
    PokerogueRngAdapter nextRng = battleRng;
    PokemonStatStageMoveEvent event{};
    // MoveTarget.USER bypasses all hit checks in MoveEffectPhase.hitCheck.
    event.hit = self || !policy.blockedBeforeAccuracy;
    if (event.hit && !self && move->accuracy >= 0 && !policy.bypassAccuracy) {
        event.accuracyRolled = true;
        event.accuracyRoll = static_cast<uint8_t>(nextRng.randSeedInt(100));
        event.hit = event.accuracyRoll < move->accuracy * policy.accuracyMultiplier;
    }
    if (event.hit) {
        auto& recipient = effect->selfTarget ? nextUser : nextTarget;
        const auto result = applyPokemonStatStageEffect(recipient, *effect,
            policy.stagePolicy, nextRng, event.stages);
        if (result != PokemonStatStageEffectResult::Ok) return result;
    }
    --nextUser.moves[slot].pp;
    user = nextUser;
    if (&user != &target) target = nextTarget;
    battleRng = nextRng;
    output = event;
    return PokemonStatStageEffectResult::Ok;
}

} // namespace Pokerogue3DS

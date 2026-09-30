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
    uint8_t processedStatMask = 0;
    int16_t requestedStages = 0;
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
    next.processedStatMask = effect.statMask & static_cast<uint8_t>(~policy.cancelledStatMask);
    next.requestedStages = static_cast<int8_t>(effect.stages * policy.stageMultiplier);
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

// Mutation part of a queued source/ability stat phase. No move-chance RNG
// belongs here. Policies must resolve phase-specific protection and reactions.
inline PokemonStatStageEffectResult applyResolvedPokemonStatStagePhase(
    PokemonBattleState& recipient, uint8_t statMask, int16_t requestedStages,
    const PokemonStatStageEffectPolicy& policy, PokemonStatStageEffectEvent& output) {
    if (!policy.resolved || policy.reflectedStatMask)
        return PokemonStatStageEffectResult::UnresolvedPolicy;
    if (statMask > 127 || requestedStages < -42 || requestedStages > 42 ||
        policy.stageMultiplier < -6 || policy.stageMultiplier > 6 ||
        policy.cancelledStatMask > 127) return PokemonStatStageEffectResult::InvalidDefinition;
    PokemonStatStageEffectEvent event{};
    if (!statMask || !requestedStages || !recipient.hp) {
        output = event;
        return PokemonStatStageEffectResult::Ok;
    }
    for (int8_t stage : recipient.statStages)
        if (stage < -6 || stage > 6) return PokemonStatStageEffectResult::InvalidState;
    event.processedStatMask = statMask & static_cast<uint8_t>(~policy.cancelledStatMask);
    const int change = requestedStages * policy.stageMultiplier;
    event.requestedStages = static_cast<int16_t>(change);
    event.triggered = event.processedStatMask && change;
    for (uint8_t stat = 0; stat < 7; ++stat) {
        const uint8_t bit = static_cast<uint8_t>(1u << stat);
        if (!(event.processedStatMask & bit)) continue;
        const int before = recipient.statStages[stat];
        const int requested = before + change;
        const int after = requested < -6 ? -6 : requested > 6 ? 6 : requested;
        recipient.statStages[stat] = static_cast<int8_t>(after);
        event.changes[stat] = static_cast<int8_t>(after - before);
        if (after != before) event.changedStatMask |= bit;
    }
    output = event;
    return PokemonStatStageEffectResult::Ok;
}

// SourceEffectType.MIRROR_ARMOR forbids another reflection. The caller must
// resolve that phase's policy accordingly before this function can commit.
inline PokemonStatStageEffectResult applyReflectedPokemonStatStages(
    PokemonBattleState& source, const PokemonStatStageEffectEvent& reflection,
    const PokemonStatStageEffectPolicy& sourcePolicy, PokemonStatStageEffectEvent& output) {
    if (reflection.reflectedStages < -36 || reflection.reflectedStages > 36)
        return PokemonStatStageEffectResult::InvalidDefinition;
    return applyResolvedPokemonStatStagePhase(source,
        reflection.triggered ? reflection.reflectedStatMask : 0,
        reflection.reflectedStages, sourcePolicy, output);
}

inline PokemonStatStageEffectResult applyPokemonCopiedStatStageRaise(
    PokemonBattleState& observer, const PokemonStatStageEffectEvent& original,
    bool originalIsOpportunistPhase, const PokemonStatStageEffectPolicy& copyPolicy,
    PokemonStatStageEffectEvent& output) {
    if (originalIsOpportunistPhase || !original.triggered || original.requestedStages <= 0) {
        output = {};
        return PokemonStatStageEffectResult::Ok;
    }
    // Upstream copies requested phase changes, including a boost at the cap.
    // The copied phase is self-target and OPPORTUNIST; no recursive copying.
    return applyResolvedPokemonStatStagePhase(observer, original.processedStatMask,
        original.requestedStages, copyPolicy, output);
}

struct PokemonStatStageReactionRequest {
    uint8_t stat = 0;
    uint8_t stages = 0;
};

inline bool planPokemonStatStageDropReaction(
    const PokerogueContent::AbilityStatStageReaction& reaction,
    const PokemonStatStageEffectEvent& event, bool selfTarget,
    PokemonStatStageReactionRequest& output) {
    if (!reaction.stat || reaction.stat > 7 || !reaction.stagesPerRequestedStat ||
        reaction.stagesPerRequestedStat > 6 || event.processedStatMask > 127) return false;
    PokemonStatStageReactionRequest next{};
    if (!selfTarget && event.triggered && event.requestedStages < 0) {
        uint8_t count = 0;
        for (uint8_t stat = 0; stat < 7; ++stat)
            if (event.processedStatMask & (1u << stat)) ++count;
        if (count) {
            next.stat = reaction.stat;
            next.stages = static_cast<uint8_t>(count * reaction.stagesPerRequestedStat);
        }
    }
    output = next;
    return true;
}

inline PokemonStatStageEffectResult applyPokemonStatStageDropReaction(
    PokemonBattleState& recipient, const PokerogueContent::AbilityStatStageReaction& reaction,
    const PokemonStatStageEffectEvent& changes, bool selfTarget,
    const PokemonStatStageEffectPolicy& reactionPolicy, PokemonStatStageEffectEvent& output) {
    PokemonStatStageReactionRequest request{};
    if (!planPokemonStatStageDropReaction(reaction, changes, selfTarget, request))
        return PokemonStatStageEffectResult::InvalidDefinition;
    const uint8_t mask = request.stat ? static_cast<uint8_t>(1u << (request.stat - 1)) : 0;
    return applyResolvedPokemonStatStagePhase(recipient, mask, request.stages,
        reactionPolicy, output);
}

struct PokemonStatStageMovePolicy {
    uint8_t ppCost = 1;
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
    if (slot >= user.moveCount || slot >= 4 || (!user.moves[slot].pp && policy.ppCost) || !user.hp)
        return PokemonStatStageEffectResult::InvalidState;
    const auto* move = PokerogueContent::findMoveById(user.moves[slot].moveId);
    if (!move || move->category != PokerogueContent::MoveStatus || move->attributeCount != 1 ||
        !PokerogueContent::moveHasAttribute(*move, "StatStageChangeAttr") ||
        !move->target || move->accuracy < -1 || move->accuracy > 100 ||
        !std::isfinite(policy.accuracyMultiplier) || policy.accuracyMultiplier < 0)
        return PokemonStatStageEffectResult::InvalidDefinition;
    const bool self = std::strcmp(move->target, "USER") == 0;
    if (!self && std::strcmp(move->target, "NEAR_OTHER") != 0 &&
        std::strcmp(move->target, "NEAR_ENEMY") != 0 &&
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
    const uint8_t consumed = policy.ppCost < nextUser.moves[slot].pp ? policy.ppCost : nextUser.moves[slot].pp;
    nextUser.moves[slot].pp = static_cast<uint8_t>(nextUser.moves[slot].pp - consumed);
    user = nextUser;
    if (&user != &target) target = nextTarget;
    battleRng = nextRng;
    output = event;
    return PokemonStatStageEffectResult::Ok;
}

struct PokemonStatStageCommandPolicy {
    PokemonStatStageMovePolicy move{};
    bool postChangePoliciesResolved = false;
    PokemonStatStageEffectPolicy reflection{};
    PokemonStatStageEffectPolicy recipientReaction{};
    PokemonStatStageEffectPolicy sourceReaction{};
    PokemonStatStageEffectPolicy opponentCopy{};
    const PokerogueContent::AbilityStatStageProfile* opponentCopyProfile = nullptr;
    const PokerogueContent::AbilityStatStageReaction* recipientReactions[2]{};
    const PokerogueContent::AbilityStatStageReaction* sourceReactions[2]{};
};

struct PokemonStatStageCommandEvent {
    PokemonStatStageMoveEvent move{};
    PokemonStatStageEffectEvent recipientReactions[2]{};
    PokemonStatStageEffectEvent reflection{};
    PokemonStatStageEffectEvent opponentCopy{};
    PokemonStatStageEffectEvent sourceReactions[2]{};
};

// Bounded one-target command transaction. Reaction phases queued by the
// recipient precede a reflected phase (source unshift ordering). No state or
// RNG commits if a later required phase policy is unresolved.
inline PokemonStatStageEffectResult usePokemonStatStageStatusCommand(
    PokemonBattleState& user, PokemonBattleState& target, uint8_t slot,
    const PokemonStatStageCommandPolicy& policy, PokerogueRngAdapter& battleRng,
    PokemonStatStageCommandEvent& output) {
    if (!policy.postChangePoliciesResolved)
        return PokemonStatStageEffectResult::UnresolvedPolicy;
    PokemonBattleState nextUser = user, nextTarget = target;
    PokerogueRngAdapter nextRng = battleRng;
    PokemonStatStageCommandEvent event{};
    auto result = usePokemonStatStageStatusMove(nextUser, nextTarget, slot,
        policy.move, nextRng, event.move);
    if (result != PokemonStatStageEffectResult::Ok) return result;
    const auto* move = PokerogueContent::findMoveById(nextUser.moves[slot].moveId);
    const bool self = std::strcmp(move->target, "USER") == 0;
    auto& recipient = self ? nextUser : nextTarget;
    if (event.move.hit && event.move.stages.triggered) {
        if (policy.opponentCopyProfile && policy.opponentCopyProfile->copiesRaises) {
            auto& observer = self ? nextTarget : nextUser;
            result = applyPokemonCopiedStatStageRaise(observer, event.move.stages,
                false, policy.opponentCopy, event.opponentCopy);
            if (result != PokemonStatStageEffectResult::Ok) return result;
        }
        for (uint8_t i = 0; i < 2; ++i) {
            if (!policy.recipientReactions[i]) continue;
            result = applyPokemonStatStageDropReaction(recipient,
                *policy.recipientReactions[i], event.move.stages, self,
                policy.recipientReaction, event.recipientReactions[i]);
            if (result != PokemonStatStageEffectResult::Ok) return result;
        }
        if (event.move.stages.reflectedStatMask) {
            if (self) return PokemonStatStageEffectResult::InvalidDefinition;
            result = applyReflectedPokemonStatStages(nextUser, event.move.stages,
                policy.reflection, event.reflection);
            if (result != PokemonStatStageEffectResult::Ok) return result;
            for (uint8_t i = 0; i < 2; ++i) {
                if (!policy.sourceReactions[i]) continue;
                result = applyPokemonStatStageDropReaction(nextUser,
                    *policy.sourceReactions[i], event.reflection, false,
                    policy.sourceReaction, event.sourceReactions[i]);
                if (result != PokemonStatStageEffectResult::Ok) return result;
            }
        }
    }
    user = nextUser;
    if (&user != &target) target = nextTarget;
    battleRng = nextRng;
    output = event;
    return PokemonStatStageEffectResult::Ok;
}

struct PokemonNegativeStageResetItemEvent {
    bool consumed = false;
    uint8_t restoredStatMask = 0;
    int8_t changes[7]{};
};

// Existing modifier inventory owns the stack; this resolver emits consumption
// for PostItemLost dispatch. Defer until the holder has no queued stat phases.
inline PokemonStatStageEffectResult applyPokemonNegativeStageResetItem(
    PokemonBattleState& holder, const char* canonicalItemId, uint32_t ownerPokemonId,
    uint8_t& stackCount, bool holderHasPendingStatPhases,
    PokemonNegativeStageResetItemEvent& output) {
    if (!canonicalItemId || !stackCount || stackCount > 2 ||
        (ownerPokemonId != 0xFFFFFFFFu && ownerPokemonId != holder.pokemonId))
        return PokemonStatStageEffectResult::InvalidState;
    bool supported = false;
    for (const auto& profile : PokerogueContent::kNegativeStageResetItemProfiles)
        if (std::strcmp(profile.itemId, canonicalItemId) == 0) supported = true;
    if (!supported) return PokemonStatStageEffectResult::InvalidDefinition;
    for (int8_t stage : holder.statStages)
        if (stage < -6 || stage > 6) return PokemonStatStageEffectResult::InvalidState;
    PokemonNegativeStageResetItemEvent event{};
    if (!holderHasPendingStatPhases) {
        for (uint8_t stat = 0; stat < 7; ++stat) {
            if (holder.statStages[stat] >= 0) continue;
            event.restoredStatMask |= static_cast<uint8_t>(1u << stat);
            event.changes[stat] = static_cast<int8_t>(-holder.statStages[stat]);
            holder.statStages[stat] = 0;
        }
        if (event.restoredStatMask) { --stackCount; event.consumed = true; }
    }
    output = event;
    return PokemonStatStageEffectResult::Ok;
}

} // namespace Pokerogue3DS

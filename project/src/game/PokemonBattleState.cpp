#include "game/PokemonBattleState.hpp"
#include "game/PokerogueRngAdapter.hpp"
#include <cmath>
#include <cstring>

namespace Pokerogue3DS {
bool canPokemonAddConfusionTag(const PokemonConfusionTagState& tag,
    const PokemonConfusionTagPolicy& policy, bool& output) {
    if (!validPokemonConfusionTag(tag)) return false;
    if (tag.present) { output = false; return true; }
    if (!policy.resolved) return false;
    output = !policy.ownAbilityBlocks && !policy.allyAbilityBlocks;
    return true;
}

PokemonConfusionTagResult addPokemonConfusionTag(PokemonConfusionTagState& tag,
    uint32_t turns, const PokemonConfusionTagPolicy& policy,
    uint16_t sourceMoveId, bool sourceMoveResolved,
    uint32_t sourcePokemonId, bool sourcePokemonResolved) {
    if (!turns || !validPokemonConfusionTag(tag) ||
        (!sourceMoveResolved && sourceMoveId) ||
        (!sourcePokemonResolved && sourcePokemonId) ||
        (sourceMoveId && !PokerogueContent::findMoveById(sourceMoveId)))
        return PokemonConfusionTagResult::Invalid;
    // Existing tag's onOverlap runs before immunity callbacks; never refresh duration.
    if (tag.present) return PokemonConfusionTagResult::Overlap;
    if (!policy.resolved) return PokemonConfusionTagResult::Unsupported;
    if (policy.ownAbilityBlocks) return PokemonConfusionTagResult::OwnAbility;
    if (policy.allyAbilityBlocks) return PokemonConfusionTagResult::AllyAbility;
    if (policy.grounded && policy.mistyTerrain) return PokemonConfusionTagResult::MistyTerrain;
    tag = {turns, true, sourceMoveId, sourceMoveResolved, sourcePokemonId, sourcePokemonResolved};
    return PokemonConfusionTagResult::Added;
}

bool applyPokemonMoveConfusion(PokemonBattleState& target, uint16_t moveId,
    int16_t effectiveChance, bool safeguardBlocks, const PokemonConfusionTagPolicy& policy,
    PokerogueRngAdapter& rng, PokemonMoveConfusionEvent& output,
    uint32_t sourcePokemonId, bool sourcePokemonResolved) {
    const PokerogueContent::MoveConfusionEffect* effect = nullptr;
    for (const auto& row : PokerogueContent::kMoveConfusionEffects)
        if (row.moveId == moveId) { if (effect) return false; effect = &row; }
    if (!effect || !effect->resolved || effect->selfTarget || !policy.resolved ||
        target.hp > target.maxHp || !validPokemonConfusionTag(target.confusion)) return false;
    auto nextTag = target.confusion;
    auto nextRng = rng;
    PokemonMoveConfusionEvent event{};
    event.moveId = moveId;
    event.safeguardBlocked = safeguardBlocks;
    event.targetFainted = !target.hp;
    if (!safeguardBlocks && target.hp) {
        bool passed = effectiveChance < 0 || effectiveChance == 100;
        if (!passed) {
            event.chanceRolled = true;
            event.chanceRoll = static_cast<uint8_t>(nextRng.randSeedInt(100));
            passed = event.chanceRoll < effectiveChance;
        }
        if (passed) {
            event.duration = nextRng.randSeedIntRange(effect->minimumTurns, effect->maximumTurns);
            event.tagAttempted = true;
            event.tagResult = addPokemonConfusionTag(nextTag, event.duration, policy, moveId, true,
                sourcePokemonId, sourcePokemonResolved);
            if (event.tagResult == PokemonConfusionTagResult::Invalid ||
                event.tagResult == PokemonConfusionTagResult::Unsupported) return false;
        }
    }
    target.confusion = nextTag;
    rng = nextRng;
    output = event;
    return true;
}

bool removePokemonConfusionTag(PokemonConfusionTagState& tag) {
    if (!validPokemonConfusionTag(tag)) return false;
    tag = {};
    return true;
}

PokemonStatusImmunityResult applyPokemonPostSummonConfusionRemoval(PokemonBattleState& actor,
    bool abilityActive, bool callbacksResolved, PokemonConfusionRemovalEvent& output) {
    if (!validPokemonConfusionTag(actor.confusion) || !callbacksResolved)
        return PokemonStatusImmunityResult::UnsupportedCondition;
    for (const auto& profile : PokerogueContent::kConfusionImmunityAbilityProfiles) {
        if (profile.abilityId != actor.abilityId) continue;
        if (abilityActive && !profile.removalResolved) return PokemonStatusImmunityResult::UnsupportedCondition;
        PokemonConfusionRemovalEvent event{};
        if (abilityActive && profile.removesConfusion && actor.confusion.present) {
            // Pinned apply has no simulated branch: caller controls invocation.
            event.abilityActivates = event.removed = true;
            actor.confusion = {};
        }
        output = event;
        return PokemonStatusImmunityResult::Resolved;
    }
    return PokemonStatusImmunityResult::UnknownAbility;
}

bool checkPokemonConfusionBeforeMove(PokemonBattleState& actor, PokemonConfusionTagState& tag,
    const PokemonConfusionMovePolicy& policy, PokerogueRngAdapter& actorRng,
    PokemonConfusionMoveEvent& output) {
    if (!validPokemonConfusionTag(tag) || actor.hp > actor.maxHp)
        return false;
    PokemonConfusionMoveEvent event{};
    if (!tag.present) { output = event; return true; }
    if (!policy.resolved || !actor.hp || !actor.level ||
        !std::isfinite(policy.effectiveAttack) || !std::isfinite(policy.effectiveDefense) ||
        policy.effectiveAttack <= 0 || policy.effectiveDefense <= 0) return false;
    auto nextTag = tag;
    auto nextRng = actorRng;
    if (--nextTag.turns == 0) {
        nextTag = {};
        event.removed = true;
    } else {
        event.activationRolled = true;
        if (nextRng.randSeedInt(3) == 0) {
            const double damage = std::floor(((((2.0 * actor.level / 5 + 2) * 40 *
                policy.effectiveAttack) / policy.effectiveDefense / 50 + 2) *
                (nextRng.randSeedIntRange(85, 100) / 100.0)));
            if (!std::isfinite(damage) || damage > UINT32_MAX) return false;
            event.requestedDamage = damage < 1 ? 1 : static_cast<uint32_t>(damage);
            event.hpLost = event.requestedDamage < actor.hp ? event.requestedDamage : actor.hp;
            event.hurtItself = event.moveCancelled = true;
        }
    }
    // No move PP, Rage Fist hitCount, or turnDamageDealt increments for confusion.
    actor.hp -= event.hpLost;
    tag = nextTag;
    actorRng = nextRng;
    output = event;
    return true;
}

PokemonStatusImmunityResult resolvePokemonStatusConfusionReaction(const PokemonBattleState& source,
    const PokemonBattleState& recipient, PokemonStatusEffect applied,
    const PokemonStatusConfusionReactionPolicy& policy, PokerogueRngAdapter& sourceRng,
    PokemonStatusConfusionReactionEvent& output) {
    if (static_cast<uint8_t>(applied) > 7) return PokemonStatusImmunityResult::InvalidEffect;
    if (!policy.resolved) return PokemonStatusImmunityResult::UnsupportedCondition;
    for (const auto& profile : PokerogueContent::kStatusConfusionAbilityProfiles) {
        if (profile.abilityId != source.abilityId) continue;
        if (policy.abilityActive && !profile.resolved) return PokemonStatusImmunityResult::UnsupportedCondition;
        auto nextRng = sourceRng;
        PokemonStatusConfusionReactionEvent event{};
        if (policy.abilityActive && !policy.simulated && recipient.hp && policy.targetCanAddConfusion &&
            (profile.statusMask & (1u << static_cast<uint8_t>(applied)))) {
            event.requestConfusionTag = true;
            event.turns = static_cast<uint8_t>(nextRng.randSeedIntRange(2, 5));
            event.targetPokemonId = recipient.pokemonId;
            // Pinned ConfusionOnStatusEffectAbAttr.apply passes opponent.id,
            // even though the source pokemon supplies the duration RNG.
            event.sourcePokemonId = recipient.pokemonId;
        }
        sourceRng = nextRng;
        output = event;
        return PokemonStatusImmunityResult::Resolved;
    }
    return PokemonStatusImmunityResult::UnknownAbility;
}

bool executePokemonSynchronizeReaction(const PokemonBattleState& statusRecipient,
    PokemonBattleState& originalSource, const PokemonQueuedStatusRequest& applied,
    bool abilityActive, bool callbacksResolved, const PokemonStatusApplicationPolicy& reflectedPolicy,
    bool reflectedReactionsResolved, PokerogueRngAdapter& reflectedRecipientRng,
    PokemonSynchronizeCommandEvent& output, bool simulated) {
    if (applied.recipientPokemonId != statusRecipient.pokemonId ||
        (applied.hasSource && applied.sourcePokemonId != originalSource.pokemonId)) return false;
    auto nextActor = originalSource;
    auto nextRng = reflectedRecipientRng;
    PokemonSynchronizeCommandEvent event{};
    if (resolvePokemonSynchronizeReaction(statusRecipient.abilityId, abilityActive, callbacksResolved,
            applied, event.reaction) != PokemonStatusImmunityResult::Resolved) return false;
    if (event.reaction.requestStatus && !simulated) {
        event.eligibility = enqueuePokemonStatusRequest(nextActor, event.reaction.request, reflectedPolicy);
        if (event.eligibility == PokemonStatusEligibility::UnsupportedPolicy ||
            event.eligibility == PokemonStatusEligibility::InvalidState) return false;
        if (event.eligibility == PokemonStatusEligibility::Allowed) {
            if (applyPokemonQueuedStatus(nextActor, event.reaction.request, reflectedReactionsResolved, nextRng) !=
                    PokemonStatusObtainResult::Applied) return false;
            event.statusApplied = true;
        }
    }
    originalSource = nextActor;
    reflectedRecipientRng = nextRng;
    output = event;
    return true;
}

PokemonStatusImmunityResult executePokemonStatusConfusionReaction(const PokemonBattleState& source,
    const PokemonBattleState& recipient, PokemonStatusEffect applied, bool abilityActive, bool simulated,
    const PokemonConfusionTagPolicy& probePolicy, const PokemonConfusionTagPolicy& applyPolicy,
    PokemonConfusionTagState& tag, PokerogueRngAdapter& sourceRng,
    PokemonStatusConfusionCommandEvent& output) {
    auto nextTag = tag;
    auto nextRng = sourceRng;
    bool canAdd = false;
    if (static_cast<uint8_t>(applied) > 7) return PokemonStatusImmunityResult::InvalidEffect;
    const PokerogueContent::StatusConfusionAbilityProfile* profile = nullptr;
    for (const auto& candidate : PokerogueContent::kStatusConfusionAbilityProfiles)
        if (candidate.abilityId == source.abilityId) { profile = &candidate; break; }
    if (!profile) return PokemonStatusImmunityResult::UnknownAbility;
    if (abilityActive && !profile->resolved) return PokemonStatusImmunityResult::UnsupportedCondition;
    // Do not require tag/terrain policies when canApply has already failed.
    // Simulated apply does not add the tag or draw its duration.
    if (abilityActive && !simulated && recipient.hp &&
        (profile->statusMask & (1u << static_cast<uint8_t>(applied))) &&
        !canPokemonAddConfusionTag(nextTag, probePolicy, canAdd))
        return PokemonStatusImmunityResult::UnsupportedCondition;
    PokemonStatusConfusionReactionPolicy reactionPolicy{};
    reactionPolicy.resolved = true;
    reactionPolicy.abilityActive = abilityActive;
    reactionPolicy.simulated = simulated;
    reactionPolicy.targetCanAddConfusion = canAdd;
    PokemonStatusConfusionCommandEvent event{};
    const auto result = resolvePokemonStatusConfusionReaction(source, recipient, applied,
        reactionPolicy, nextRng, event.reaction);
    if (result != PokemonStatusImmunityResult::Resolved) return result;
    if (event.reaction.requestConfusionTag) {
        event.tagAttempted = true;
        event.tagResult = addPokemonConfusionTag(nextTag, event.reaction.turns, applyPolicy, 0, true,
            recipient.pokemonId, true);
        if (event.tagResult == PokemonConfusionTagResult::Unsupported ||
            event.tagResult == PokemonConfusionTagResult::Invalid)
            return PokemonStatusImmunityResult::UnsupportedCondition;
    }
    tag = nextTag;
    sourceRng = nextRng;
    output = event;
    return PokemonStatusImmunityResult::Resolved;
}

bool executePokemonPostSetStatusReactions(PokemonBattleState& recipient, PokemonBattleState& source,
    const PokemonQueuedStatusRequest& applied, const PokemonPostSetStatusPolicy& policy,
    PokerogueRngAdapter& recipientRng, PokerogueRngAdapter& sourceRng,
    PokemonPostSetStatusEvent& output) {
    if (&recipient == &source || !applied.hasSource ||
        !pokemonStatusStateValid(recipient.status) || !recipient.status.present ||
        recipient.status.effect != applied.effect || recipient.pendingStatus != PokemonStatusEffect::None ||
        applied.recipientPokemonId != recipient.pokemonId || applied.sourcePokemonId != source.pokemonId)
        return false;
    // ObtainStatusEffectPhase skips all follow-up callbacks for FAINT.
    if (applied.effect == PokemonStatusEffect::Faint) { output = {}; return true; }
    if (!policy.formsResolved) return false;
    auto nextRecipient = recipient;
    auto nextSource = source;
    auto nextRecipientRng = recipientRng;
    auto nextSourceRng = sourceRng;
    auto& reflectedRng = &recipientRng == &sourceRng ? nextRecipientRng : nextSourceRng;
    PokemonPostSetStatusEvent event{};
    // ObtainStatusEffectPhase: form trigger, recipient PostSetStatus, then
    // source ConfusionOnStatusEffect. Synchronize duration uses its recipient.
    if (!executePokemonSynchronizeReaction(nextRecipient, nextSource, applied,
            policy.recipientAbilityActive, policy.recipientCallbacksResolved,
            policy.reflectedApplication, policy.reflectedReactionsResolved, reflectedRng,
            event.synchronize, policy.simulated)) return false;
    if (executePokemonStatusConfusionReaction(nextSource, nextRecipient, applied.effect,
            policy.sourceAbilityActive, policy.simulated, policy.confusionProbe,
            policy.confusionApplication, nextRecipient.confusion, reflectedRng, event.confusion) !=
            PokemonStatusImmunityResult::Resolved) return false;
    recipient = nextRecipient;
    source = nextSource;
    if (&recipientRng == &sourceRng) sourceRng = nextRecipientRng;
    else { recipientRng = nextRecipientRng; sourceRng = nextSourceRng; }
    output = event;
    return true;
}

bool executePokemonStatusAction(PokemonBattleState& user, PokemonBattleState& target, uint8_t slot,
    const PokemonStatusEffectCommandPolicy& commandPolicy, const PokemonPostSetStatusPolicy& reactionsPolicy,
    PokerogueRngAdapter& userRng, PokerogueRngAdapter& targetRng, PokemonStatusActionEvent& output) {
    if (&user == &target) return false; // Self-target post-set dispatcher still pending.
    auto nextUser = user;
    auto nextTarget = target;
    auto nextUserRng = userRng;
    auto nextTargetRng = targetRng;
    auto& recipientRng = &userRng == &targetRng ? nextUserRng : nextTargetRng;
    PokemonStatusActionEvent event{};
    if (!executePokemonStatusEffectCommand(nextUser, nextTarget, slot, commandPolicy,
            nextUserRng, recipientRng, event.move)) return false;
    if (event.move.application.requestObtainStatusPhase) {
        if (event.move.application.selfTarget) return false;
        PokemonQueuedStatusRequest applied{};
        applied.recipientPokemonId = nextTarget.pokemonId;
        applied.sourcePokemonId = nextUser.pokemonId;
        applied.hasSource = true;
        applied.effect = event.move.application.effect;
        if (!executePokemonPostSetStatusReactions(nextTarget, nextUser, applied, reactionsPolicy,
                recipientRng, nextUserRng, event.reactions)) return false;
        event.reactionsExecuted = true;
    }
    user = nextUser;
    target = nextTarget;
    if (&userRng == &targetRng) userRng = nextUserRng;
    else { userRng = nextUserRng; targetRng = nextTargetRng; }
    output = event;
    return true;
}

bool resolvePokemonStatusApplicationEnvironment(const PokemonBattleState& recipient,
    const PokemonBattleState* source, const PokemonStatusFieldContext& field,
    PokemonStatusApplicationPolicy& output) {
    if (!field.resolved || !pokemonStatusStateValid(recipient.status) ||
        static_cast<uint8_t>(recipient.pendingStatus) > 7 || recipient.hp > recipient.maxHp ||
        !field.effectiveTypes || !field.effectiveTypeCount || !field.originalIfStellarTypes ||
        !field.originalIfStellarTypeCount) return false;
    PokemonStatusApplicationPolicy policy{};
    policy.resolved = true;
    policy.pendingStatus = recipient.pendingStatus != PokemonStatusEffect::None;
    policy.hasSource = source != nullptr;
    policy.sourceIsTarget = source == &recipient;
    policy.overrideStatus = field.overrideStatus;
    policy.ignoreField = field.ignoreField;
    policy.grounded = field.grounded;
    policy.mistyTerrain = field.mistyTerrain;
    policy.electricTerrain = field.electricTerrain;
    policy.sunnyOrHarshSun = field.sunnyOrHarshSun;
    policy.safeguardBlocks = field.safeguardBlocks;
    for (size_t i = 0; i < field.effectiveTypeCount; ++i) {
        const char* type = resolvePokemonTypeSymbol(field.effectiveTypes[i]);
        if (!type) return false;
        policy.electricType |= std::strcmp(type, "ELECTRIC") == 0;
        policy.iceType |= std::strcmp(type, "ICE") == 0;
        policy.fireType |= std::strcmp(type, "FIRE") == 0;
    }
    for (size_t i = 0; i < field.originalIfStellarTypeCount; ++i) {
        const char* type = resolvePokemonTypeSymbol(field.originalIfStellarTypes[i]);
        if (!type) return false;
        policy.poisonType |= std::strcmp(type, "POISON") == 0;
        policy.steelType |= std::strcmp(type, "STEEL") == 0;
    }
    output = policy;
    return true;
}

PokemonStatusMoveCheckResult checkPokemonStatusBeforeMove(PokemonStatusState& status,
    const PokemonStatusMoveCheckPolicy& policy, PokerogueRngAdapter& rng, PokemonStatusMoveCheckEvent& output) {
    if (!pokemonStatusStateValid(status)) return PokemonStatusMoveCheckResult::InvalidStatus;
    if (!policy.resolved) return PokemonStatusMoveCheckResult::UnsupportedPolicy;
    auto next = status;
    auto nextRng = rng;
    PokemonStatusMoveCheckEvent event{};
    event.effect = status.effect;
    if (status.present && (status.effect == PokemonStatusEffect::Sleep || status.effect == PokemonStatusEffect::Freeze)) {
        if ((status.effect == PokemonStatusEffect::Sleep && policy.indirectSleepWake) ||
            (status.effect == PokemonStatusEffect::Freeze && policy.indirectFreezeWake)) event.cured = true;
        else if (status.effect == PokemonStatusEffect::Freeze && policy.deferredFreezeThawMove) {
            // MovePhase.checkFreeze sets thaw before incrementTurn; pre-use
            // doThawCheck owns the later cure after other checks succeed.
            event.thawAfterFailureChecks = true;
        } else {
            if (incrementPokemonStatusTurn(next) == PokemonStatusTickResult::CounterOverflow)
                return PokemonStatusMoveCheckResult::CounterOverflow;
            if (status.effect == PokemonStatusEffect::Sleep) {
                const uint32_t remaining = next.hasSleepTurnsRemaining ? next.sleepTurnsRemaining : 0;
                next.hasSleepTurnsRemaining = true;
                next.sleepTurnsRemaining = remaining > policy.sleepDurationReduction ?
                    remaining - policy.sleepDurationReduction : 0;
                event.cured = !next.sleepTurnsRemaining;
                event.cancelled = !event.cured && !policy.bypassSleep;
            } else {
                // JS evaluates randBattleSeedInt(4) before the expired-counter test.
                event.cured = policy.freezeCureAfterIncrement || nextRng.randSeedInt(4) == 0 ||
                    !next.hasFreezeTurnsRemaining || !next.freezeTurnsRemaining;
                event.cancelled = !event.cured;
            }
        }
    } else if (status.present && status.effect == PokemonStatusEffect::Paralysis) {
        event.cancelled = nextRng.randSeedInt(8) == 0;
    }
    if (event.cured) next = {};
    status = next;
    rng = nextRng;
    output = event;
    return PokemonStatusMoveCheckResult::Ok;
}

PokemonMoveStatusApplicationResult resolvePokemonMoveStatusApplication(
    const PokemonBattleState& recipient, uint16_t moveId, int16_t effectiveChance,
    bool chanceCallbacksResolved, const PokemonStatusApplicationPolicy& policy,
    PokerogueRngAdapter& userRng, PokemonMoveStatusApplicationEvent& output) {
    if (!pokemonStatusStateValid(recipient.status) || recipient.hp > recipient.maxHp)
        return PokemonMoveStatusApplicationResult::InvalidState;
    if (!chanceCallbacksResolved || !policy.resolved)
        return PokemonMoveStatusApplicationResult::UnresolvedPolicy;
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move) return PokemonMoveStatusApplicationResult::UnsupportedMove;
    const PokerogueContent::MoveStatusEffect* declaration = nullptr;
    for (const auto& row : PokerogueContent::kMoveStatusEffects) {
        if (row.moveId != moveId) continue;
        if (declaration || !row.parametersResolved || row.effectId > 7)
            return PokemonMoveStatusApplicationResult::UnsupportedMove;
        declaration = &row;
    }
    if (!declaration) return PokemonMoveStatusApplicationResult::UnsupportedMove;
    auto nextRng = userRng;
    PokemonMoveStatusApplicationEvent event{};
    event.effect = static_cast<PokemonStatusEffect>(declaration->effectId);
    event.selfTarget = declaration->selfTarget;
    event.quiet = move->category != PokerogueContent::MoveStatus;
    bool chancePassed = effectiveChance < 0 || effectiveChance == 100;
    if (!chancePassed) {
        event.chanceRolled = true;
        event.chanceRoll = static_cast<uint8_t>(nextRng.randSeedInt(100));
        chancePassed = event.chanceRoll < effectiveChance;
    }
    auto result = PokemonMoveStatusApplicationResult::ChanceFailed;
    if (chancePassed) {
        event.eligibility = event.effect == PokemonStatusEffect::None ? PokemonStatusEligibility::NoEffect :
            canPokemonSetStatus(recipient.status, event.effect, policy);
        if (event.eligibility != PokemonStatusEligibility::Allowed)
            result = PokemonMoveStatusApplicationResult::Ineligible;
        else if (!recipient.hp && event.effect != PokemonStatusEffect::Faint)
            result = PokemonMoveStatusApplicationResult::Fainted;
        else {
            event.requestObtainStatusPhase = true;
            result = PokemonMoveStatusApplicationResult::Requested;
        }
    }
    userRng = nextRng;
    output = event;
    return result;
}

bool executePokemonMoveStatusPhase(PokemonBattleState& user, PokemonBattleState& target,
    uint16_t moveId, int16_t effectiveChance, const PokemonStatusApplicationPolicy& application,
    const PokemonPostSetStatusPolicy& reactions, PokerogueRngAdapter& rng,
    PokemonMoveStatusPhaseEvent& output) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    // POST_APPLY target effects follow self-target recoil, which may KO the user.
    // This phase does not authorize starting a Status command from a fainted user.
    if (&user == &target || !move || (!user.hp && move->category == PokerogueContent::MoveStatus) ||
        user.hp > user.maxHp || !pokemonStatusStateValid(user.status) ||
        !reactions.formsResolved || application.overrideStatus) return false;
    auto nextUser = user;
    auto nextTarget = target;
    auto nextRng = rng;
    PokemonMoveStatusPhaseEvent event{};
    event.result = resolvePokemonMoveStatusApplication(nextTarget, moveId, effectiveChance,
        true, application, nextRng, event.application);
    if (event.result == PokemonMoveStatusApplicationResult::UnsupportedMove ||
        event.result == PokemonMoveStatusApplicationResult::UnresolvedPolicy ||
        event.result == PokemonMoveStatusApplicationResult::InvalidState || event.application.selfTarget) return false;
    if (event.application.requestObtainStatusPhase) {
        PokemonQueuedStatusRequest request{};
        request.recipientPokemonId = nextTarget.pokemonId;
        request.sourcePokemonId = nextUser.pokemonId;
        request.hasSource = true;
        request.effect = event.application.effect;
        if (enqueuePokemonStatusRequest(nextTarget, request, application) != PokemonStatusEligibility::Allowed ||
            applyPokemonQueuedStatus(nextTarget, request, true, nextRng) != PokemonStatusObtainResult::Applied ||
            !executePokemonPostSetStatusReactions(nextTarget, nextUser, request, reactions,
                nextRng, nextRng, event.reactions)) return false;
        event.applied = true;
    }
    user = nextUser;
    target = nextTarget;
    rng = nextRng;
    output = event;
    return true;
}

bool calculatePokemonStatusEffectAiBenefit(const PokemonBattleState& recipient, uint16_t moveId,
    int16_t effectiveChance, bool chanceCallbacksResolved,
    const PokemonStatusApplicationPolicy& application, double& output) {
    if (!chanceCallbacksResolved || !application.resolved) return false;
    const PokerogueContent::MoveStatusEffect* effect = nullptr;
    for (const auto& row : PokerogueContent::kMoveStatusEffects) {
        if (row.moveId != moveId) continue;
        if (effect || !row.parametersResolved || row.effectId > 7) return false;
        effect = &row;
    }
    if (!effect) return false;
    auto policy = application;
    policy.ignoreField = true; // Pinned getTargetBenefitScore canSetStatus argument.
    const auto eligibility = canPokemonSetStatus(recipient.status,
        static_cast<PokemonStatusEffect>(effect->effectId), policy);
    if (eligibility == PokemonStatusEligibility::InvalidState ||
        eligibility == PokemonStatusEligibility::UnsupportedPolicy) return false;
    output = eligibility == PokemonStatusEligibility::Allowed ?
        (effectiveChance < 0 ? -10.0 : std::floor(effectiveChance * -0.1)) : 0.0;
    return true;
}

bool resolvePokemonStatusMoveTypeImmunity(uint16_t moveId,
    const PokemonStatusMoveTypeImmunityPolicy& policy, bool& output) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move || move->category != PokerogueContent::MoveStatus || !move->target) return false;
    // Move.isTypeImmune exits for USER before querying types/abilities.
    if (std::strcmp(move->target, "USER") == 0) { output = false; return true; }
    if (!policy.resolved || !policy.originalIfStellarTypes || !policy.typeCount) return false;
    const PokerogueContent::StatusMoveFlagProfile* flags = nullptr;
    for (const auto& profile : PokerogueContent::kStatusMoveFlagProfiles)
        if (profile.moveId == moveId) { flags = &profile; break; }
    if (!flags || !flags->resolved) return false;
    bool immune = false;
    for (size_t i = 0; i < policy.typeCount; ++i) {
        const char* type = policy.originalIfStellarTypes[i];
        if (!type || !resolvePokemonTypeSymbol(type)) return false;
        if (std::strcmp(type, "GRASS") == 0 && flags->powder) immune = true;
        if (std::strcmp(type, "DARK") == 0 && policy.userHasPrankster && policy.opponents) immune = true;
    }
    output = immune;
    return true;
}

bool resolvePokemonStatusMoveHit(const PokerogueContent::Move& move, bool self,
    const PokemonStatusMoveHitPolicy& policy, PokerogueRngAdapter& rng, PokemonStatusMoveHitEvent& output) {
    if (!policy.resolved || move.accuracy < -1 || move.accuracy > 100 ||
        !std::isfinite(policy.accuracyMultiplier) || policy.accuracyMultiplier < 0) return false;
    auto nextRng = rng;
    PokemonStatusMoveHitEvent event{};
    event.typeImmune = !self && policy.typeImmune;
    event.hit = self || (!policy.blockedBeforeAccuracy && !event.typeImmune);
    if (event.hit && !self && move.accuracy >= 0 && !policy.bypassAccuracy) {
        event.accuracyRolled = true;
        event.accuracyRoll = static_cast<uint8_t>(nextRng.randSeedInt(100));
        event.hit = event.accuracyRoll < move.accuracy * policy.accuracyMultiplier;
    }
    rng = nextRng;
    output = event;
    return true;
}

bool usePokemonStatusEffectMove(PokemonBattleState& user, const PokemonBattleState& target,
    uint8_t slot, const PokemonStatusEffectMovePolicy& policy, PokerogueRngAdapter& rng,
    PokemonStatusEffectMoveEvent& output) {
    if (!policy.hit.resolved || !policy.application.resolved || !policy.chanceCallbacksResolved ||
        !user.hp || slot >= user.moveCount || slot >= 4 || (!user.moves[slot].pp && policy.ppCost) ||
        user.moves[slot].pp > user.moves[slot].maxPp) return false;
    const auto* move = PokerogueContent::findMoveById(user.moves[slot].moveId);
    if (!move || move->category != PokerogueContent::MoveStatus || move->attributeCount != 1 ||
        !PokerogueContent::moveHasAttribute(*move, "StatusEffectAttr") || !move->target) return false;
    const bool self = std::strcmp(move->target, "USER") == 0;
    if (!self && (std::strcmp(move->target, "NEAR_OTHER") && std::strcmp(move->target, "NEAR_ENEMY"))) return false;
    if (!self && (!target.hp || &user == &target)) return false;
    auto nextRng = rng;
    PokemonStatusEffectMoveEvent event{};
    if (!resolvePokemonStatusMoveHit(*move, self, policy.hit, nextRng, event.hit)) return false;
    // Validate the declaration even on a miss; unsupported content is an error.
    bool found = false;
    for (const auto& row : PokerogueContent::kMoveStatusEffects) {
        if (row.moveId != move->id) continue;
        if (found || !row.parametersResolved || row.effectId > 7 || row.selfTarget != self) return false;
        found = true;
    }
    if (!found) return false;
    if (event.hit.hit) {
        event.applicationResult = resolvePokemonMoveStatusApplication(self ? user : target, move->id,
            policy.effectiveChance, true, policy.application, nextRng, event.application);
        if (event.applicationResult == PokemonMoveStatusApplicationResult::UnresolvedPolicy ||
            event.applicationResult == PokemonMoveStatusApplicationResult::UnsupportedMove ||
            event.applicationResult == PokemonMoveStatusApplicationResult::InvalidState) return false;
    }
    event.ppConsumed = policy.ppCost < user.moves[slot].pp ? policy.ppCost : user.moves[slot].pp;
    user.moves[slot].pp -= event.ppConsumed;
    rng = nextRng;
    output = event;
    return true;
}

bool executePokemonStatusEffectCommand(PokemonBattleState& user, PokemonBattleState& target,
    uint8_t slot, const PokemonStatusEffectCommandPolicy& policy, PokerogueRngAdapter& sourceRng,
    PokerogueRngAdapter& recipientRng, PokemonStatusEffectMoveEvent& output) {
    if (!policy.reactionsResolved || policy.move.application.overrideStatus) return false;
    auto nextUser = user;
    auto nextTarget = target;
    auto nextSourceRng = sourceRng;
    auto nextRecipientRng = recipientRng;
    auto& moveTarget = &user == &target ? nextUser : nextTarget;
    PokemonStatusEffectMoveEvent event{};
    if (!usePokemonStatusEffectMove(nextUser, moveTarget, slot, policy.move, nextSourceRng, event)) return false;
    if (event.application.requestObtainStatusPhase) {
        auto& recipient = event.application.selfTarget ? nextUser : nextTarget;
        PokemonQueuedStatusRequest request{};
        request.recipientPokemonId = recipient.pokemonId;
        request.sourcePokemonId = nextUser.pokemonId;
        request.hasSource = true;
        request.effect = event.application.effect;
        if (enqueuePokemonStatusRequest(recipient, request, policy.move.application) != PokemonStatusEligibility::Allowed)
            return false;
        auto& durationRng = &sourceRng == &recipientRng ? nextSourceRng : nextRecipientRng;
        if (applyPokemonQueuedStatus(recipient, request, true, durationRng) != PokemonStatusObtainResult::Applied)
            return false;
    }
    user = nextUser;
    if (&user != &target) target = nextTarget;
    sourceRng = nextSourceRng;
    if (&sourceRng != &recipientRng) recipientRng = nextRecipientRng;
    output = event;
    return true;
}

PokemonStatusObtainResult obtainPokemonStatus(PokemonBattleState& actor, PokemonStatusEffect effect,
    const PokemonStatusApplicationPolicy& policy, bool reactionsResolved, PokerogueRngAdapter& rng,
    bool explicitSleepDuration, uint32_t sleepDuration) {
    if (effect == PokemonStatusEffect::None) return PokemonStatusObtainResult::Ineligible;
    auto eligibilityPolicy = policy;
    eligibilityPolicy.pendingStatus = policy.pendingStatus || actor.pendingStatus != PokemonStatusEffect::None;
    if (canPokemonSetStatus(actor.status, effect, eligibilityPolicy) != PokemonStatusEligibility::Allowed)
        return PokemonStatusObtainResult::Ineligible;
    if (!actor.hp && effect != PokemonStatusEffect::Faint) return PokemonStatusObtainResult::Fainted;
    if (!reactionsResolved) return PokemonStatusObtainResult::UnsupportedReactions;
    PokemonQueuedStatusRequest request{};
    request.recipientPokemonId = actor.pokemonId;
    request.effect = effect;
    request.explicitSleepDuration = explicitSleepDuration;
    request.sleepDuration = sleepDuration;
    auto nextActor = actor;
    auto nextRng = rng;
    nextActor.pendingStatus = effect;
    const auto result = applyPokemonQueuedStatus(nextActor, request, reactionsResolved, nextRng);
    if (result != PokemonStatusObtainResult::Applied) return result;
    actor = nextActor;
    rng = nextRng;
    return result;
}

PokemonStatusEligibility enqueuePokemonStatusRequest(PokemonBattleState& recipient,
    const PokemonQueuedStatusRequest& request, const PokemonStatusApplicationPolicy& policy) {
    if (request.recipientPokemonId != recipient.pokemonId || static_cast<uint8_t>(request.effect) > 7 ||
        static_cast<uint8_t>(recipient.pendingStatus) > 7 || recipient.hp > recipient.maxHp ||
        (!request.hasSource && request.sourcePokemonId)) return PokemonStatusEligibility::InvalidState;
    if (request.effect == PokemonStatusEffect::None) return PokemonStatusEligibility::NoEffect;
    if (policy.overrideStatus) return PokemonStatusEligibility::UnsupportedPolicy;
    auto pendingPolicy = policy;
    pendingPolicy.pendingStatus = policy.pendingStatus || recipient.pendingStatus != PokemonStatusEffect::None;
    const auto eligibility = canPokemonSetStatus(recipient.status, request.effect, pendingPolicy);
    if (eligibility != PokemonStatusEligibility::Allowed) return eligibility;
    if (!recipient.hp && request.effect != PokemonStatusEffect::Faint) return PokemonStatusEligibility::Fainted;
    recipient.pendingStatus = request.effect;
    return PokemonStatusEligibility::Allowed;
}

PokemonStatusObtainResult applyPokemonQueuedStatus(PokemonBattleState& recipient,
    const PokemonQueuedStatusRequest& request, bool reactionsResolved, PokerogueRngAdapter& recipientRng) {
    if (request.recipientPokemonId != recipient.pokemonId || recipient.pendingStatus != request.effect ||
        request.effect == PokemonStatusEffect::None || static_cast<uint8_t>(request.effect) > 7 ||
        (!request.hasSource && request.sourcePokemonId)) return PokemonStatusObtainResult::Ineligible;
    if (!reactionsResolved) return PokemonStatusObtainResult::UnsupportedReactions;
    auto nextRng = recipientRng;
    PokemonStatusState status{};
    status.present = true;
    status.effect = request.effect;
    // JS default parameter draws only for sleep without an explicit duration.
    status.hasSleepTurnsRemaining = true;
    status.sleepTurnsRemaining = request.explicitSleepDuration ? request.sleepDuration :
        request.effect == PokemonStatusEffect::Sleep ? (nextRng.randSeedInt(3) == 0 ? 2 : 3) : 0;
    status.hasFreezeTurnsRemaining = request.effect == PokemonStatusEffect::Freeze;
    status.freezeTurnsRemaining = status.hasFreezeTurnsRemaining ? 3 : 0;
    recipient.status = status;
    recipient.pendingStatus = PokemonStatusEffect::None;
    recipientRng = nextRng;
    return PokemonStatusObtainResult::Applied;
}

namespace {
bool abilityBelongsToSpecies(const PokerogueContent::Species& species, uint16_t id) {
    return id != 0 && (species.ability1 == id || species.ability2 == id ||
        species.abilityHidden == id || species.abilityPassive == id);
}
bool abilityBelongsToForm(const PokerogueContent::Form& form, uint16_t id) {
    return id != 0 && (form.ability1 == id || form.ability2 == id || form.abilityHidden == id);
}

uint32_t calculatedStat(uint8_t base, uint8_t iv, uint16_t level,
                        uint8_t statIndex, int8_t raised, int8_t lowered) {
    const uint32_t scaled = ((2u * base + iv) * level) / 100u;
    if (statIndex == 0) return scaled + level + 10u;

    uint32_t value = scaled + 5u;
    if (raised != lowered && raised == static_cast<int8_t>(statIndex)) value = (value * 11u + 9u) / 10u;
    else if (raised != lowered && lowered == static_cast<int8_t>(statIndex)) value = (value * 9u) / 10u;
    return value ? value : 1u;
}

bool typeInList(const char* type, const char* list) {
    if (!type || !list || !*type) return false;
    std::size_t typeLength = 0;
    while (type[typeLength]) ++typeLength;
    const char* item = list;
    while (*item) {
        while (*item == '|') ++item;
        const char* end = item;
        while (*end && *end != '|') ++end;
        if (static_cast<std::size_t>(end - item) == typeLength) {
            std::size_t index = 0;
            while (index < typeLength) {
                const char value = type[index] >= 'a' && type[index] <= 'z'
                    ? static_cast<char>(type[index] - 'a' + 'A') : type[index];
                if (item[index] != value) break;
                ++index;
            }
            if (index == typeLength) return true;
        }
        item = end;
    }
    return false;
}

struct TypeChartRow { const char* defendingType; const char* weakTo; const char* resists; const char* immuneTo; };
// Extracted from the pinned PokemonType chart in src/data/type.ts.
const TypeChartRow kTypeChart[] = {
    {"NORMAL", "|FIGHTING|", "||", "|GHOST|"},
    {"FIGHTING", "|FLYING|PSYCHIC|FAIRY|", "|ROCK|BUG|DARK|", "||"},
    {"FLYING", "|ROCK|ELECTRIC|ICE|", "|FIGHTING|BUG|GRASS|", "|GROUND|"},
    {"POISON", "|GROUND|PSYCHIC|", "|FIGHTING|POISON|BUG|GRASS|FAIRY|", "||"},
    {"GROUND", "|WATER|GRASS|ICE|", "|POISON|ROCK|", "|ELECTRIC|"},
    {"ROCK", "|FIGHTING|GROUND|STEEL|WATER|GRASS|", "|NORMAL|FLYING|POISON|FIRE|", "||"},
    {"BUG", "|FLYING|ROCK|FIRE|", "|FIGHTING|GROUND|GRASS|", "||"},
    {"GHOST", "|GHOST|DARK|", "|POISON|BUG|", "|NORMAL|FIGHTING|"},
    {"STEEL", "|FIGHTING|GROUND|FIRE|", "|NORMAL|FLYING|ROCK|BUG|STEEL|GRASS|PSYCHIC|ICE|DRAGON|FAIRY|", "|POISON|"},
    {"FIRE", "|GROUND|ROCK|WATER|", "|BUG|STEEL|FIRE|GRASS|ICE|FAIRY|", "||"},
    {"WATER", "|GRASS|ELECTRIC|", "|STEEL|FIRE|WATER|ICE|", "||"},
    {"GRASS", "|FLYING|POISON|BUG|FIRE|ICE|", "|GROUND|WATER|GRASS|ELECTRIC|", "||"},
    {"ELECTRIC", "|GROUND|", "|FLYING|STEEL|ELECTRIC|", "||"},
    {"PSYCHIC", "|BUG|GHOST|DARK|", "|FIGHTING|PSYCHIC|", "||"},
    {"ICE", "|FIGHTING|ROCK|STEEL|FIRE|", "|ICE|", "||"},
    {"DRAGON", "|ICE|DRAGON|FAIRY|", "|FIRE|WATER|GRASS|ELECTRIC|", "||"},
    {"DARK", "|FIGHTING|BUG|FAIRY|", "|GHOST|DARK|", "|PSYCHIC|"},
    {"FAIRY", "|POISON|STEEL|", "|FIGHTING|BUG|DARK|", "|DRAGON|"},
};

bool sameText(const char* left, const char* right) {
    if (!left || !right) return false;
    while (*left && *right) {
        const char leftValue = *left >= 'a' && *left <= 'z' ? static_cast<char>(*left - 'a' + 'A') : *left;
        const char rightValue = *right >= 'a' && *right <= 'z' ? static_cast<char>(*right - 'a' + 'A') : *right;
        if (leftValue != rightValue) return false;
        ++left;
        ++right;
    }
    return *left == *right;
}

bool oneTypeEffectiveness(const char* attack, const char* defend, double& multiplier) {
    if (sameText(attack, "UNKNOWN") || sameText(defend, "UNKNOWN") || sameText(attack, "STELLAR") || sameText(defend, "STELLAR")) {
        multiplier = 1.0;
        return true;
    }
    for (const auto& row : kTypeChart) {
        if (!sameText(row.defendingType, defend)) continue;
        if (typeInList(attack, row.immuneTo)) multiplier = 0.0;
        else if (typeInList(attack, row.weakTo)) multiplier = 2.0;
        else if (typeInList(attack, row.resists)) multiplier = 0.5;
        else multiplier = 1.0;
        return true;
    }
    return false;
}
}

void derivePokemonIvsFromId(uint32_t pokemonId, uint8_t outputIvs[6]) {
    if (!outputIvs) return;
    outputIvs[0] = static_cast<uint8_t>((pokemonId & 0x3E000000u) >> 25);
    outputIvs[1] = static_cast<uint8_t>((pokemonId & 0x01F00000u) >> 20);
    outputIvs[2] = static_cast<uint8_t>((pokemonId & 0x000F8000u) >> 15);
    outputIvs[3] = static_cast<uint8_t>((pokemonId & 0x00007C00u) >> 10);
    outputIvs[4] = static_cast<uint8_t>((pokemonId & 0x000003E0u) >> 5);
    outputIvs[5] = static_cast<uint8_t>(pokemonId & 0x0000001Fu);
}

const char* resolvePokemonTypeSymbol(const char* type) {
    for (const auto& row : kTypeChart)
        if (sameText(type, row.defendingType)) return row.defendingType;
    return sameText(type, "STELLAR") ? "STELLAR" : nullptr;
}

bool getPokemonNatureModifiers(PokemonNature nature, PokemonNatureModifiers& output) {
    static const int8_t kRaised[25] = {
        -1, 1, 1, 1, 1, 2, -1, 2, 2, 2, 5, 5, -1, 5, 5,
        3, 3, 3, -1, 3, 4, 4, 4, 4, -1,
    };
    static const int8_t kLowered[25] = {
        -1, 2, 5, 3, 4, 1, -1, 5, 3, 4, 1, 2, -1, 3, 4,
        1, 2, 5, -1, 4, 1, 2, 5, 3, -1,
    };
    const auto index = static_cast<uint8_t>(nature);
    if (index >= 25) return false;
    output = {kRaised[index], kLowered[index]};
    return true;
}

PokemonNature selectPokemonNature(PokerogueRngAdapter& rng) {
    return static_cast<PokemonNature>(rng.randSeedInt(25));
}

void generatePokemonActorNature(PokemonActorIdentity& actor, PokerogueRngAdapter& rng) {
    actor.nature = selectPokemonNature(rng);
}

PokemonFormSelectionResult selectPokemonActorForm(
    uint16_t speciesDex,
    const PokemonFormSelectionContext& context,
    PokerogueRngAdapter& rng,
    PokemonActorIdentity& actor) {
    const auto* species = PokerogueContent::findSpeciesByDex(speciesDex);
    if (!species) return PokemonFormSelectionResult::MissingSpecies;

    std::size_t formCount = 0;
    for (const auto& form : PokerogueContent::kForms) {
        if (sameText(form.speciesId, species->id)) ++formCount;
    }
    if (!formCount) {
        actor.formId = nullptr;
        return PokemonFormSelectionResult::Ok;
    }

    const char* id = species->id;
    int32_t index = 0;
    bool selected = false;
    const auto randomForms = [&]() {
        index = rng.randSeedInt(static_cast<int32_t>(formCount));
        selected = true;
    };

    // Trainer specialty forms run before the species' ordinary selection rule.
    if (context.trainerBattle && !context.eggPhase && context.trainerSpecialtyType) {
        if (sameText(id, "wormadam")) {
            if (sameText(context.trainerSpecialtyType, "GROUND")) { index = 1; selected = true; }
            else if (sameText(context.trainerSpecialtyType, "STEEL")) { index = 2; selected = true; }
            else if (sameText(context.trainerSpecialtyType, "GRASS")) { index = 0; selected = true; }
        } else if (sameText(id, "rotom")) {
            if (sameText(context.trainerSpecialtyType, "FLYING")) { index = 4; selected = true; }
            else if (sameText(context.trainerSpecialtyType, "GHOST")) { index = 0; selected = true; }
            else if (sameText(context.trainerSpecialtyType, "FIRE")) { index = 1; selected = true; }
            else if (sameText(context.trainerSpecialtyType, "WATER")) { index = 2; selected = true; }
            else if (sameText(context.trainerSpecialtyType, "GRASS")) { index = 5; selected = true; }
            else if (sameText(context.trainerSpecialtyType, "ICE")) { index = 3; selected = true; }
        } else if (sameText(id, "oricorio")) {
            if (sameText(context.trainerSpecialtyType, "GHOST")) { index = 3; selected = true; }
            else if (sameText(context.trainerSpecialtyType, "FIRE")) { index = 0; selected = true; }
            else if (sameText(context.trainerSpecialtyType, "ELECTRIC")) { index = 1; selected = true; }
            else if (sameText(context.trainerSpecialtyType, "PSYCHIC")) { index = 2; selected = true; }
        } else if (sameText(id, "paldea_tauros")) {
            if (sameText(context.trainerSpecialtyType, "FIRE")) { index = 1; selected = true; }
            else if (sameText(context.trainerSpecialtyType, "WATER")) { index = 2; selected = true; }
        } else if (sameText(id, "silvally") || sameText(id, "arceus")) {
            static const char* types[] = {"NORMAL", "FIGHTING", "FLYING", "POISON", "GROUND", "ROCK",
                "BUG", "GHOST", "STEEL", "FIRE", "WATER", "GRASS", "ELECTRIC", "PSYCHIC", "ICE",
                "DRAGON", "DARK", "FAIRY"};
            for (std::size_t typeIndex = 0; typeIndex < sizeof(types) / sizeof(types[0]); ++typeIndex) {
                if (sameText(context.trainerSpecialtyType, types[typeIndex])) {
                    index = static_cast<int32_t>(typeIndex);
                    selected = true;
                    break;
                }
            }
        }
    }

    if (!selected) {
        if (sameText(id, "unown") || sameText(id, "shellos") || sameText(id, "gastrodon") ||
            sameText(id, "rotom") || sameText(id, "basculin") || sameText(id, "deerling") ||
            sameText(id, "sawsbuck") || sameText(id, "scatterbug") || sameText(id, "spewpa") ||
            sameText(id, "vivillon") || sameText(id, "flabebe") || sameText(id, "floette") ||
            sameText(id, "florges") || sameText(id, "furfrou") || sameText(id, "pumpkaboo") ||
            sameText(id, "gourgeist") || sameText(id, "oricorio") || sameText(id, "zarude") ||
            sameText(id, "squawkabilly") || sameText(id, "paldea_tauros")) {
            randomForms();
        } else if (sameText(id, "sinistea") || sameText(id, "polteageist") || sameText(id, "maushold") ||
                   sameText(id, "dudunsparce") || sameText(id, "poltchageist") || sameText(id, "sinistcha")) {
            index = rng.randSeedInt(16) ? 0 : 1; selected = true;
        } else if (sameText(id, "pichu")) {
            index = rng.randSeedInt(8) ? 0 : 1; selected = true;
        } else if (sameText(id, "pikachu")) {
            index = context.trainerBattle && context.waveIndex < 30 ? 0 : rng.randSeedInt(8);
            selected = true;
        } else if (sameText(id, "eevee")) {
            index = context.trainerBattle && context.waveIndex < 30 && !context.eggPhase
                ? 0 : rng.randSeedInt(2);
            selected = true;
        } else if (sameText(id, "magearna") || sameText(id, "urshifu")) {
            index = rng.randSeedInt(2); selected = true;
        } else if (sameText(id, "tatsugiri")) {
            index = rng.randSeedInt(3); selected = true;
        } else if (sameText(id, "zygarde")) {
            index = rng.randSeedInt(4); selected = true;
        } else if (sameText(id, "minior")) {
            index = rng.randSeedInt(7); selected = true;
        } else if (sameText(id, "alcremie")) {
            index = rng.randSeedInt(9); selected = true;
        } else if (sameText(id, "meowstic") || sameText(id, "indeedee") || sameText(id, "basculegion") ||
                   sameText(id, "oinkologne")) {
            index = actor.gender == PokemonGender::Female ? 1 : 0; selected = true;
        } else if (sameText(id, "toxtricity")) {
            static const uint8_t lowKeyNatures[] = {1, 5, 7, 10, 12, 15, 16, 17, 18, 20, 21, 23};
            for (const uint8_t nature : lowKeyNatures) {
                if (static_cast<uint8_t>(context.nature) == nature) { index = 1; break; }
            }
            selected = true;
        } else if (sameText(id, "gimmighoul")) {
            if (context.hasMysteryEncounters && !context.eggPhase) { index = 1; selected = true; }
            else randomForms();
        } else if (sameText(id, "burmy") || sameText(id, "wormadam")) {
            if (context.ignoreArena) randomForms();
            else if (sameText(context.biomeId, "beach")) { index = 1; selected = true; }
            else if (sameText(context.biomeId, "slum")) { index = 2; selected = true; }
            else { index = 0; selected = true; }
        } else if (sameText(id, "lycanroc")) {
            if (context.ignoreArena) randomForms();
            else if (sameText(context.timeOfDay, "DUSK")) { index = 2; selected = true; }
            else if (sameText(context.timeOfDay, "NIGHT")) { index = 1; selected = true; }
            else { index = 0; selected = true; }
        } else {
            index = 0;
            selected = true;
        }
    }

    if (index < 0 || static_cast<std::size_t>(index) >= formCount) {
        return PokemonFormSelectionResult::MissingForm;
    }
    std::size_t current = 0;
    for (const auto& form : PokerogueContent::kForms) {
        if (!sameText(form.speciesId, species->id)) continue;
        if (current++ == static_cast<std::size_t>(index)) {
            actor.formId = form.id;
            return PokemonFormSelectionResult::Ok;
        }
    }
    return PokemonFormSelectionResult::MissingForm;
}

PokemonActorIdentityResult generatePokemonActorIdentity(
    uint16_t speciesDex,
    uint16_t hiddenAbilityRate,
    PokerogueRngAdapter& rng,
    PokemonActorIdentity& output) {
    const auto* species = PokerogueContent::findSpeciesByDex(speciesDex);
    if (!species) return PokemonActorIdentityResult::MissingSpecies;
    if (hiddenAbilityRate == 0) return PokemonActorIdentityResult::InvalidHiddenRate;

    PokemonActorIdentity next{};
    // Pokemon.generateAbilityIndex runs before assigning this.id.
    const auto abilityResult = selectPokemonAbilityIndex(
        speciesDex, hiddenAbilityRate, rng, next.abilityIndex);
    if (abilityResult != PokemonAbilitySelectionResult::Ok) {
        return PokemonActorIdentityResult::InvalidHiddenRate;
    }
    next.pokemonId = rng.randSeedUint32();
    derivePokemonIvsFromId(next.pokemonId, next.ivs);
    const auto genderResult = selectPokemonGender(speciesDex, rng, next.gender);
    if (genderResult == PokemonGenderSelectionResult::MissingSpecies) {
        return PokemonActorIdentityResult::MissingSpecies;
    }
    if (genderResult != PokemonGenderSelectionResult::Ok) {
        return PokemonActorIdentityResult::InvalidGenderRatio;
    }
    output = next;
    return PokemonActorIdentityResult::Ok;
}

PokemonActorIdentityResult generatePokemonActorIdentityAndForm(
    uint16_t speciesDex,
    uint16_t hiddenAbilityRate,
    const PokemonFormSelectionContext& formContext,
    PokerogueRngAdapter& rng,
    PokemonActorIdentity& output) {
    PokemonActorIdentity next{};
    const auto identityResult = generatePokemonActorIdentity(
        speciesDex, hiddenAbilityRate, rng, next);
    if (identityResult != PokemonActorIdentityResult::Ok) return identityResult;
    const auto formResult = selectPokemonActorForm(speciesDex, formContext, rng, next);
    if (formResult == PokemonFormSelectionResult::MissingSpecies) {
        return PokemonActorIdentityResult::MissingSpecies;
    }
    if (formResult != PokemonFormSelectionResult::Ok) {
        return PokemonActorIdentityResult::MissingForm;
    }
    output = next;
    return PokemonActorIdentityResult::Ok;
}

PokemonActorIdentityResult generatePokemonActorForWildEncounter(
    uint16_t speciesDex,
    uint16_t hiddenAbilityRate,
    const PokemonFormSelectionContext& formContext,
    PokerogueRngAdapter& rng,
    PokemonActorIdentity& output) {
    const auto* species = PokerogueContent::findSpeciesByDex(speciesDex);
    if (!species) return PokemonActorIdentityResult::MissingSpecies;

    PokemonActorIdentity next{};
    const auto identityResult = generatePokemonActorIdentityAndForm(
        speciesDex, hiddenAbilityRate, formContext, rng, next);
    if (identityResult != PokemonActorIdentityResult::Ok) return identityResult;

    const auto* form = next.formId ? PokerogueContent::findFormById(next.formId) : nullptr;
    const char* primaryType = form ? form->type1 : species->type1;
    const char* secondaryType = form ? form->type2 : species->type2;
    if (!primaryType || !primaryType[0] || sameText(primaryType, "NONE")) {
        return PokemonActorIdentityResult::InvalidTypes;
    }
    const uint8_t typeCount = secondaryType && secondaryType[0] && !sameText(secondaryType, "NONE")
        ? 2 : 1;

    // Pokemon's constructor generates nature before the initial tera type.
    // Shiny variant generation is scoped to a derived seed and restores wave RNG.
    generatePokemonActorNature(next, rng);
    next.initialTeraTypeIndex = static_cast<uint8_t>(rng.randSeedInt(typeCount));
    next.initialTeraType = resolvePokemonTypeSymbol(next.initialTeraTypeIndex ? secondaryType : primaryType);
    if (!next.initialTeraType) return PokemonActorIdentityResult::InvalidTypes;
    next.initialTeraTypeResolved = true;
    output = next;
    return PokemonActorIdentityResult::Ok;
}

PokemonBattleInitResult initializePokemonBattleState(
    const PokemonBattleInit& input,
    PokemonBattleState& output) {
    const auto* species = PokerogueContent::findSpeciesByDex(input.speciesDex);
    if (!species) return PokemonBattleInitResult::MissingSpecies;
    const auto* form = input.formId
        ? PokerogueContent::findFormById(input.formId)
        : (species->firstFormId[0] ? PokerogueContent::findFormById(species->firstFormId) : nullptr);
    if (input.formId && !form) return PokemonBattleInitResult::InvalidForm;
    if (form) {
        const char* speciesId = species->id;
        const char* formSpeciesId = form->speciesId;
        while (*speciesId && *formSpeciesId && *speciesId == *formSpeciesId) { ++speciesId; ++formSpeciesId; }
        if (*speciesId != *formSpeciesId) return PokemonBattleInitResult::InvalidForm;
    }
    if (input.level == 0) return PokemonBattleInitResult::InvalidLevel;
    const bool speciesIsGenderless = species->malePercentTenths == 65534;
    if ((speciesIsGenderless && input.gender != PokemonGender::Genderless) ||
        (!speciesIsGenderless && (species->malePercentTenths > 1000 ||
            (input.gender != PokemonGender::Male && input.gender != PokemonGender::Female)))) {
        return PokemonBattleInitResult::InvalidGender;
    }
    uint8_t instanceIvs[6];
    if (input.deriveIvsFromPokemonId) derivePokemonIvsFromId(input.pokemonId, instanceIvs);
    else for (uint8_t i = 0; i < 6; ++i) instanceIvs[i] = input.ivs[i];
    for (uint8_t iv : instanceIvs) if (iv > 31) return PokemonBattleInitResult::InvalidIv;
    PokemonNatureModifiers natureModifiers{input.natureRaisedStat, input.natureLoweredStat};
    if (input.nature != PokemonNature::Unspecified && !getPokemonNatureModifiers(input.nature, natureModifiers)) {
        return PokemonBattleInitResult::InvalidNatureStat;
    }
    const auto validNatureStat = [](int8_t stat) { return stat == -1 || (stat >= 1 && stat <= 5); };
    if (!validNatureStat(natureModifiers.raisedStat) || !validNatureStat(natureModifiers.loweredStat)) {
        return PokemonBattleInitResult::InvalidNatureStat;
    }
    if (form ? !abilityBelongsToForm(*form, input.abilityId) : !abilityBelongsToSpecies(*species, input.abilityId)) return PokemonBattleInitResult::InvalidAbility;
    if (input.moveCount > 4) return PokemonBattleInitResult::InvalidMoveCount;

    PokemonBattleState next{};
    const PokerogueContent::SpeciesFriendshipProfile* friendship = nullptr;
    for (const auto& profile : PokerogueContent::kSpeciesFriendshipProfiles)
        if (profile.speciesDex == input.speciesDex) { friendship = &profile; break; }
    if (!friendship) return PokemonBattleInitResult::MissingSpecies;
    next.friendship = friendship->baseFriendship;
    next.speciesDex = input.speciesDex;
    next.formId = form ? form->id : nullptr;
    next.level = input.level;
    next.pokemonId = input.pokemonId;
    next.nature = input.nature;
    next.abilityId = input.abilityId;
    next.gender = input.gender;
    for (uint8_t i = 0; i < 6; ++i) next.ivs[i] = instanceIvs[i];
    next.ivsWereDerivedFromPokemonId = input.deriveIvsFromPokemonId;
    const uint8_t baseStats[6] = {form ? form->hp : species->hp, form ? form->atk : species->atk,
        form ? form->def : species->def, form ? form->spatk : species->spatk,
        form ? form->spdef : species->spdef, form ? form->speed : species->speed};
    for (uint8_t index = 0; index < 6; ++index) {
        const uint32_t value = calculatedStat(baseStats[index], instanceIvs[index], input.level,
            index, natureModifiers.raisedStat, natureModifiers.loweredStat);
        if (value > 65535u) return PokemonBattleInitResult::InvalidStatRange;
        next.stats[index] = static_cast<uint16_t>(value);
    }
    next.maxHp = next.stats[0];
    next.hp = next.maxHp;
    next.moveCount = input.moveCount;
    for (uint8_t index = 0; index < input.moveCount; ++index) {
        const auto* move = PokerogueContent::findMoveById(input.moveIds[index]);
        if (!move) return PokemonBattleInitResult::MissingMove;
        if (move->pp < 1 || move->pp > 255) return PokemonBattleInitResult::InvalidMovePp;
        next.moves[index] = {move->id, static_cast<uint8_t>(move->pp), static_cast<uint8_t>(move->pp)};
    }
    output = next;
    return PokemonBattleInitResult::Ok;
}

bool recalculatePokemonBattleLevel(PokemonBattleState& state, uint16_t level) {
    if (!state.statsAreBaseFormulaOnly || !state.maxHp || state.hp > state.maxHp ||
        state.moveCount > 4) return false;
    PokemonBattleInit input{};
    input.speciesDex = state.speciesDex;
    input.formId = state.formId;
    input.level = level;
    input.pokemonId = state.pokemonId;
    input.deriveIvsFromPokemonId = state.ivsWereDerivedFromPokemonId;
    input.nature = state.nature;
    input.gender = state.gender;
    input.abilityId = state.abilityId;
    input.moveCount = state.moveCount;
    for (uint8_t i = 0; i < 6; ++i) input.ivs[i] = state.ivs[i];
    for (uint8_t slot = 0; slot < state.moveCount; ++slot) {
        if (state.moves[slot].pp > state.moves[slot].maxPp) return false;
        input.moveIds[slot] = state.moves[slot].moveId;
    }
    PokemonBattleState calculated{};
    if (initializePokemonBattleState(input, calculated) != PokemonBattleInitResult::Ok) return false;
    // Pinned Pokemon.calculateStats changes stats/HP, not moves or summonData.
    PokemonBattleState next = state;
    next.level = calculated.level;
    next.maxHp = calculated.maxHp;
    for (uint8_t i = 0; i < 6; ++i) next.stats[i] = calculated.stats[i];
    uint32_t hp = state.hp;
    if (hp && next.maxHp > state.maxHp) hp += next.maxHp - state.maxHp;
    next.hp = static_cast<uint16_t>(hp > next.maxHp ? next.maxHp : hp);
    state = next;
    return true;
}

bool changePokemonBattleForm(PokemonBattleState& state, const char* targetFormId,
    uint16_t resolvedAbilityId, bool fullRestore) {
    if (!state.statsAreBaseFormulaOnly || !state.maxHp || state.hp > state.maxHp ||
        state.moveCount > 4 || !targetFormId) return false;
    PokemonBattleInit input{};
    input.speciesDex = state.speciesDex;
    for (int8_t stage : state.statStages) if (stage < -6 || stage > 6) return false;
    input.formId = targetFormId;
    input.level = state.level;
    input.pokemonId = state.pokemonId;
    input.deriveIvsFromPokemonId = state.ivsWereDerivedFromPokemonId;
    input.nature = state.nature;
    input.gender = state.gender;
    input.abilityId = resolvedAbilityId;
    input.moveCount = state.moveCount;
    for (uint8_t i = 0; i < 6; ++i) input.ivs[i] = state.ivs[i];
    for (uint8_t slot = 0; slot < state.moveCount; ++slot) {
        if (state.moves[slot].pp > state.moves[slot].maxPp) return false;
        input.moveIds[slot] = state.moves[slot].moveId;
    }
    PokemonBattleState next{};
    if (initializePokemonBattleState(input, next) != PokemonBattleInitResult::Ok) return false;
    for (uint8_t slot = 0; slot < state.moveCount; ++slot)
        if (state.moves[slot].maxPp != next.moves[slot].maxPp) return false;
    if (fullRestore) next.hp = next.maxHp;
    else if (!state.hp) next.hp = 0;
    else {
        uint32_t hp = state.hp;
        if (next.maxHp > state.maxHp) hp += next.maxHp - state.maxHp;
        next.hp = static_cast<uint16_t>(hp > next.maxHp ? next.maxHp : hp);
    }
    for (uint8_t slot = 0; slot < state.moveCount; ++slot)
        if (!fullRestore) next.moves[slot].pp = state.moves[slot].pp;
    for (uint8_t stat = 0; stat < 7; ++stat) next.statStages[stat] = state.statStages[stat];
    next.friendship = state.friendship;
    next.pauseEvolutions = state.pauseEvolutions;
    next.status = state.status;
    next.pendingStatus = state.pendingStatus;
    next.confusion = state.confusion;
    next.heldItemLostTags = state.heldItemLostTags;
    next.turnDamageDealt = state.turnDamageDealt;
    state = next;
    return true;
}

PokemonBattleInitResult initializePokemonBattleStateForActor(
    const PokemonBattleInit& nonIdentityInput,
    const PokemonActorIdentity& identity,
    PokemonBattleState& output) {
    const auto* species = PokerogueContent::findSpeciesByDex(nonIdentityInput.speciesDex);
    if (!species) return PokemonBattleInitResult::MissingSpecies;
    const auto* form = nonIdentityInput.formId
        ? PokerogueContent::findFormById(nonIdentityInput.formId)
        : (species->firstFormId[0] ? PokerogueContent::findFormById(species->firstFormId) : nullptr);
    const uint16_t ability1 = form && form->ability1 ? form->ability1 : species->ability1;
    const uint16_t ability2 = form && form->ability2 ? form->ability2 : ability1;
    const uint16_t abilityHidden = form && form->abilityHidden
        ? form->abilityHidden : species->abilityHidden;
    if (identity.abilityIndex > 2) return PokemonBattleInitResult::InvalidAbility;

    PokemonBattleInit input = nonIdentityInput;
    input.pokemonId = identity.pokemonId;
    input.deriveIvsFromPokemonId = true;
    input.gender = identity.gender;
    if (identity.nature != PokemonNature::Unspecified) input.nature = identity.nature;
    if (identity.formId) input.formId = identity.formId;
    input.abilityId = identity.abilityIndex == 2 ? abilityHidden
        : identity.abilityIndex == 1 ? ability2 : ability1;
    return initializePokemonBattleState(input, output);
}

PokemonBaseDamageResult calculatePokemonBaseDamage(
    const PokemonBattleState& attacker,
    const PokemonBattleState& defender,
    uint16_t moveId,
    double& outputBaseDamage, bool critical,
    const PokemonMoveWeatherContext* weatherContext) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move) return PokemonBaseDamageResult::MissingMove;
    if (move->category == PokerogueContent::MoveStatus || move->power <= 0) {
        return PokemonBaseDamageResult::NonDamagingMove;
    }

    const bool physical = move->category == PokerogueContent::MovePhysical;
    const uint8_t attackStat = physical ? 1 : 3;
    const uint8_t defenseStat = physical ? 2 : 4;
    double attackStage = 1.0, defenseStage = 1.0;
    if (!pokemonStatStageMultiplier(attacker, attackStat, critical, attackStage) ||
        !pokemonStatStageMultiplier(defender, defenseStat, critical, defenseStage) ||
        !attacker.stats[attackStat] || !defender.stats[defenseStat])
        return PokemonBaseDamageResult::InvalidStats;
    // Pokemon.getEffectiveStat floors after stage multiplication, minimum one.
    double weatherDefenseMultiplier = 1.0;
    if (weatherContext) {
        if (!weatherContext->resolved || static_cast<uint8_t>(weatherContext->effectiveWeather) > 9)
            return PokemonBaseDamageResult::UnresolvedWeather;
        const auto* species = PokerogueContent::findSpeciesByDex(defender.speciesDex);
        const auto* form = defender.formId ? PokerogueContent::findFormById(defender.formId) : nullptr;
        if (!species || (defender.formId && !form)) return PokemonBaseDamageResult::InvalidStats;
        const char* type1 = form ? form->type1 : species->type1;
        const char* type2 = form ? form->type2 : species->type2;
        // forDefend uses the opponent's effective weather. Apply after stage
        // scaling but before getEffectiveStat's final floor, not to floored stats.
        if ((physical && weatherContext->effectiveWeather == PokemonEffectiveWeather::Snow &&
                (sameText(type1, "ICE") || sameText(type2, "ICE"))) ||
            (!physical && weatherContext->effectiveWeather == PokemonEffectiveWeather::Sandstorm &&
                (sameText(type1, "ROCK") || sameText(type2, "ROCK"))))
            weatherDefenseMultiplier = 1.5;
    }
    double attackAbilityMultiplier = 1.0, defenseAbilityMultiplier = 1.0;
    for (const auto& profile : PokerogueContent::kDamageStatAbilityProfiles) {
        const bool attacking = profile.abilityId == attacker.abilityId && profile.stat == attackStat;
        const bool defending = profile.abilityId == defender.abilityId && profile.stat == defenseStat;
        if (!attacking && !defending) continue;
        if (profile.requiresCondition) return PokemonBaseDamageResult::UnsupportedAbilityCondition;
        if (attacking) attackAbilityMultiplier *= profile.multiplier;
        if (defending) defenseAbilityMultiplier *= profile.multiplier;
    }
    // StatMultiplierAbAttr precedes stages and weather; floor only at the end.
    uint32_t attack = static_cast<uint32_t>(attacker.stats[attackStat] * attackAbilityMultiplier * attackStage);
    uint32_t defense = static_cast<uint32_t>(defender.stats[defenseStat] * defenseAbilityMultiplier * defenseStage * weatherDefenseMultiplier);
    if (!attack) attack = 1;
    if (!defense) defense = 1;
    if (attacker.level == 0 || defense == 0 || attack == 0) return PokemonBaseDamageResult::InvalidStats;

    // Pinned Pokemon.getBaseDamage formula before modifiers (STAB, type,
    // weather, random factor, abilities, items and move attributes).
    const double levelMultiplier = (2.0 * attacker.level) / 5.0 + 2.0;
    double power = move->power;
    // LowHpMoveTypePowerBoostAbAttr inherits the 1.5 power multiplier and
    // checks getHpRatio() <= 0.33 (not one-third) before the base-damage +2.
    if (attacker.hp && attacker.maxHp &&
        static_cast<uint32_t>(attacker.hp) * 100u <= static_cast<uint32_t>(attacker.maxHp) * 33u) {
        for (const auto& profile : PokerogueContent::kLowHpTypePowerAbilities)
            if (profile.abilityId == attacker.abilityId && sameText(profile.type, move->type))
                power *= 1.5;
    }
    for (const auto& profile : PokerogueContent::kTypePowerAbilities) {
        if (profile.abilityId != attacker.abilityId || !sameText(profile.type, move->type)) continue;
        // Field-gated components require an inspected condition and resolved
        // arena context; an unknown condition must not become unconditional.
        if (profile.requiresCondition) {
            if (!weatherContext || !weatherContext->resolved ||
                static_cast<uint8_t>(weatherContext->cancellationWeather) > 9 ||
                !sameText(profile.conditionWeatherSymbol, "SANDSTORM"))
                return PokemonBaseDamageResult::UnsupportedAbilityCondition;
            // getWeatherCondition uses suppressed arena weather, not a
            // PreAttackWeatherOverrideAbAttr override for the attacker.
            if (weatherContext->cancellationWeather != PokemonEffectiveWeather::Sandstorm) continue;
        }
        if (attacker.hp) power *= profile.multiplier;
    }
    const double baseDamage = (levelMultiplier * power * attack) / defense / 50.0 + 2.0;
    outputBaseDamage = baseDamage;
    return PokemonBaseDamageResult::Ok;
}

PokemonTypeEffectivenessResult calculatePokemonAttackTypeEffectiveness(
    const char* attackType,
    const PokemonBattleState& defender,
    double& outputMultiplier) {
    if (!attackType || !*attackType) return PokemonTypeEffectivenessResult::InvalidType;
    const auto* species = PokerogueContent::findSpeciesByDex(defender.speciesDex);
    if (!species) return PokemonTypeEffectivenessResult::MissingSpecies;
    const auto* form = defender.formId ? PokerogueContent::findFormById(defender.formId) : nullptr;
    if (defender.formId && !form) return PokemonTypeEffectivenessResult::InvalidType;

    const char* type1 = form ? form->type1 : species->type1;
    const char* type2 = form ? form->type2 : species->type2;
    double first = 1.0;
    double second = 1.0;
    const bool hasSecondType = type2 && *type2 && !sameText(type2, "NONE");
    if (!oneTypeEffectiveness(attackType, type1, first) ||
        (hasSecondType && !oneTypeEffectiveness(attackType, type2, second))) {
        return PokemonTypeEffectivenessResult::InvalidType;
    }
    outputMultiplier = first * second;
    return PokemonTypeEffectivenessResult::Ok;
}

PokemonTypeEffectivenessResult calculatePokemonTypeEffectiveness(
    uint16_t moveId, const PokemonBattleState& defender, double& outputMultiplier) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move) return PokemonTypeEffectivenessResult::MissingMove;
    return calculatePokemonAttackTypeEffectiveness(move->type, defender, outputMultiplier);
}

bool selectPokemonBiomeWeather(const char* biomeId, bool duskOrNight,
    PokerogueRngAdapter& rng, PokemonEffectiveWeather& output,
    const uint16_t* resolvedEventWeights) {
    if (!biomeId || !*biomeId) return false;
    // Enum order matters: upstream builds the Map with getEnumValues, not
    // weatherPool property order. Generated catalog rows may sort differently.
    static constexpr const char* symbols[10] = {
        "NONE", "SUNNY", "RAIN", "SANDSTORM", "HAIL", "SNOW", "FOG",
        "HEAVY_RAIN", "HARSH_SUN", "STRONG_WINDS"
    };
    uint16_t weights[10]{};
    bool seen[10]{};
    bool foundBiome = false;
    for (const auto& entry : PokerogueContent::kBiomeWeatherPools) {
        if (!sameText(entry.biomeId, biomeId)) continue;
        foundBiome = true;
        uint8_t index = 0;
        while (index < 10 && !sameText(entry.weatherSymbol, symbols[index])) ++index;
        if (index == 10 || seen[index]) return false;
        seen[index] = true;
        weights[index] = entry.weight;
    }
    if (!foundBiome) return false;
    if (resolvedEventWeights)
        for (uint8_t i = 0; i < 10; ++i) weights[i] = resolvedEventWeights[i];
    if (duskOrNight) {
        weights[1] = 0;
        if (sameText(biomeId, "forest")) weights[6] = 1;
    }
    uint32_t total = 0;
    for (const auto weight : weights) total += weight;
    if (!total) { weights[0] = 1; total = 1; }
    PokerogueRngAdapter nextRng = rng;
    const uint32_t roll = static_cast<uint32_t>(nextRng.randSeedInt(static_cast<int32_t>(total)));
    uint32_t cumulative = 0;
    for (uint8_t i = 0; i < 10; ++i) {
        cumulative += weights[i];
        if (roll < cumulative) {
            output = static_cast<PokemonEffectiveWeather>(i);
            rng = nextRng;
            return true;
        }
    }
    return false;
}

bool pokemonWeatherIsImmutable(PokemonEffectiveWeather type) {
    return type == PokemonEffectiveWeather::HeavyRain || type == PokemonEffectiveWeather::HarshSun ||
        type == PokemonEffectiveWeather::StrongWinds;
}

bool setPokemonArenaWeather(PokemonArenaWeatherState& state,
    PokemonEffectiveWeather type, uint16_t resolvedDuration) {
    if (static_cast<uint8_t>(type) > 9 || static_cast<uint8_t>(state.type) > 9 ||
        state.type == type) return false;
    if (pokemonWeatherIsImmutable(state.type) && type != PokemonEffectiveWeather::None &&
        !pokemonWeatherIsImmutable(type)) return false;
    PokemonArenaWeatherState next{};
    next.type = type;
    if (type != PokemonEffectiveWeather::None && !pokemonWeatherIsImmutable(type)) {
        next.turnsLeft = resolvedDuration;
        next.maxDuration = resolvedDuration;
    }
    state = next;
    return true;
}

bool lapsePokemonArenaWeather(PokemonArenaWeatherState& state) {
    if (static_cast<uint8_t>(state.type) > 9) return false;
    if (pokemonWeatherIsImmutable(state.type) || !state.turnsLeft) return true;
    --state.turnsLeft;
    // Like Weather.lapse, this reports expiration; the phase clears weather.
    return state.turnsLeft != 0;
}

bool pokemonAbilityBlocksWeatherDamage(uint16_t abilityId,
    PokemonEffectiveWeather weather, bool& outputBlocked) {
    if (static_cast<uint8_t>(weather) > 9 ||
        !PokerogueContent::findAbilityMovegenProfile(abilityId)) return false;
    bool blocked = false;
    for (const auto& profile : PokerogueContent::kWeatherDamageAbilityProfiles)
        if (profile.abilityId == abilityId)
            blocked = blocked || profile.blocksIndirectDamage ||
                (profile.weatherMask & (1u << static_cast<uint8_t>(weather)));
    outputBlocked = blocked;
    return true;
}

bool applyPokemonWeatherResidualDamage(PokemonBattleState& target,
    const PokemonArenaWeatherState& arena, const PokemonWeatherDamagePolicy& policy,
    PokemonWeatherDamageEvent& output) {
    if (!policy.resolved || static_cast<uint8_t>(arena.type) > 9 ||
        !target.maxHp || target.hp > target.maxHp) return false;
    PokemonWeatherDamageEvent event{};
    if (!target.hp || policy.weatherSuppressed || policy.abilityBlocksDamage ||
        policy.underground || policy.underwater || policy.switchingOut ||
        (arena.type != PokemonEffectiveWeather::Sandstorm && arena.type != PokemonEffectiveWeather::Hail)) {
        output = event;
        return true;
    }
    if (!policy.type1 || !*policy.type1 || sameText(policy.type1, "NONE")) return false;
    const auto knownType = [](const char* type) {
        if (!type || !*type || sameText(type, "NONE")) return true;
        for (const auto& row : kTypeChart)
            if (sameText(row.defendingType, type)) return true;
        return false;
    };
    if (!knownType(policy.type1) || !knownType(policy.type2)) return false;
    const auto typeImmune = [&](const char* type) {
        if (arena.type == PokemonEffectiveWeather::Hail) return sameText(type, "ICE");
        return sameText(type, "GROUND") || sameText(type, "ROCK") || sameText(type, "STEEL");
    };
    if (typeImmune(policy.type1) || typeImmune(policy.type2)) {
        output = event;
        return true;
    }
    uint16_t damage = static_cast<uint16_t>(target.maxHp / 16);
    if (!damage) damage = 1;
    event.damageApplied = damage < target.hp ? damage : target.hp;
    target.hp = static_cast<uint16_t>(target.hp - event.damageApplied);
    event.fainted = target.hp == 0;
    output = event;
    return true;
}

bool advancePokemonArenaWeatherTurnEnd(PokemonArenaWeatherState& state,
    PokemonWeatherTurnEndEvent& output) {
    if (static_cast<uint8_t>(state.type) > 9 ||
        (state.type == PokemonEffectiveWeather::None && (state.turnsLeft || state.maxDuration)) ||
        (pokemonWeatherIsImmutable(state.type) && (state.turnsLeft || state.maxDuration)) ||
        state.turnsLeft > state.maxDuration) return false;
    PokemonArenaWeatherState next = state;
    PokemonWeatherTurnEndEvent event{};
    if (next.type != PokemonEffectiveWeather::None && !lapsePokemonArenaWeather(next)) {
        event.expired = true;
        event.previousWeather = next.type;
        event.requestWeatherFormReversion = true;
        if (!setPokemonArenaWeather(next, PokemonEffectiveWeather::None, 0)) return false;
    }
    state = next;
    output = event;
    return true;
}

// Pinned WeatherChangeAttr.getCondition/apply, MovePhase.usePP, Arena.trySetWeather.
PokemonWeatherChangeResult usePokemonWeatherChangeCommand(PokemonBattleState& user,
    PokemonArenaWeatherState& arena, uint8_t moveSlot,
    const PokemonWeatherChangePolicy& policy, PokemonWeatherChangeEvent& output) {
    if (!policy.resolved || !policy.weatherCallbacksResolved)
        return PokemonWeatherChangeResult::UnresolvedPolicy;
    if (!user.hp || !user.maxHp || user.hp > user.maxHp || moveSlot >= user.moveCount ||
        moveSlot >= 4 || !user.moves[moveSlot].pp ||
        user.moves[moveSlot].pp > user.moves[moveSlot].maxPp || !policy.ppCost)
        return PokemonWeatherChangeResult::InvalidState;
    auto checkedArena = arena;
    PokemonWeatherTurnEndEvent check{};
    if (!advancePokemonArenaWeatherTurnEnd(checkedArena, check))
        return PokemonWeatherChangeResult::InvalidState;
    const auto* move = PokerogueContent::findMoveById(user.moves[moveSlot].moveId);
    if (!move || move->category != PokerogueContent::MoveStatus || !sameText(move->target, "BOTH_SIDES") ||
        move->accuracy != -1 || move->attributeCount != 1 ||
        !PokerogueContent::moveHasAttribute(*move, "WeatherChangeAttr")) return PokemonWeatherChangeResult::UnsupportedMove;
    const PokerogueContent::MoveWeatherChangeProfile* profile = nullptr;
    for (const auto& candidate : PokerogueContent::kMoveWeatherChangeProfiles) {
        if (candidate.moveId != move->id) continue;
        if (profile || candidate.weatherType == 0 || candidate.weatherType > 9)
            return PokemonWeatherChangeResult::UnsupportedMove;
        profile = &candidate;
    }
    if (!profile) return PokemonWeatherChangeResult::UnsupportedMove;
    if (!policy.duration) return PokemonWeatherChangeResult::InvalidState;
    PokemonWeatherChangeEvent event{};
    event.previousWeather = event.nextWeather = arena.type;
    event.blocked = policy.blockedBeforeMove;
    auto nextUser = user;
    auto nextArena = arena;
    if (!policy.blockedBeforeMove) {
        event.ppSpent = nextUser.moves[moveSlot].pp < policy.ppCost
            ? nextUser.moves[moveSlot].pp : policy.ppCost;
        nextUser.moves[moveSlot].pp -= event.ppSpent;
        // Default attribute conditions run after PP consumption, including repeat weather.
        const auto requested = static_cast<PokemonEffectiveWeather>(profile->weatherType);
        event.failedCondition = arena.type == requested || pokemonWeatherIsImmutable(arena.type);
        if (!event.failedCondition) {
            if (!setPokemonArenaWeather(nextArena, requested, policy.duration))
                return PokemonWeatherChangeResult::InvalidState;
            event.changed = true;
            event.nextWeather = nextArena.type;
        }
    }
    user = nextUser;
    arena = nextArena;
    output = event;
    return PokemonWeatherChangeResult::Ok;
}

bool composePokemonWeatherResolutionPolicy(const PokemonWeatherAbilityComponent* components,
    std::size_t count, PokemonWeatherResolutionPolicy& output) {
    if (count && !components) return false;
    PokemonWeatherResolutionPolicy next{};
    next.resolved = true;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& component = components[i];
        if (!component.applies) continue;
        if (!PokerogueContent::findAbilityMovegenProfile(component.abilityId)) return false;
        for (const auto& profile : PokerogueContent::kWeatherAbilityProfiles) {
            if (profile.abilityId != component.abilityId) continue;
            if (profile.suppressesWeather) {
                next.suppressesOrdinaryWeather = true;
                next.suppressesImmutableWeather = next.suppressesImmutableWeather || profile.affectsImmutable;
            }
            if (component.belongsToAttacker && profile.overrideWeatherSymbol && *profile.overrideWeatherSymbol) {
                if (!sameText(profile.overrideWeatherSymbol, "SUNNY")) return false;
                next.attackerOverride = PokemonEffectiveWeather::Sunny;
            }
        }
    }
    output = next;
    return true;
}

bool resolvePokemonMoveWeatherContext(const PokemonArenaWeatherState& arena,
    const PokemonWeatherResolutionPolicy& policy, PokemonMoveWeatherContext& output) {
    if (!policy.resolved || static_cast<uint8_t>(arena.type) > 9 ||
        static_cast<uint8_t>(policy.attackerOverride) > 9) return false;
    PokemonMoveWeatherContext next{};
    next.resolved = true;
    const bool arenaSuppressed = pokemonWeatherIsImmutable(arena.type)
        ? policy.suppressesImmutableWeather : policy.suppressesOrdinaryWeather;
    next.cancellationWeather = arenaSuppressed ? PokemonEffectiveWeather::None : arena.type;
    // PreAttackWeatherOverrideAbAttr takes precedence over field suppression.
    if (policy.attackerOverride != PokemonEffectiveWeather::None) {
        next.effectiveWeather = policy.attackerOverride;
    } else {
        const bool suppressed = pokemonWeatherIsImmutable(arena.type)
            ? policy.suppressesImmutableWeather : policy.suppressesOrdinaryWeather;
        next.effectiveWeather = suppressed ? PokemonEffectiveWeather::None : arena.type;
    }
    output = next;
    return true;
}

bool pokemonWeatherEffectiveSpeed(const PokemonBattleState& state,
    const PokemonMoveWeatherContext& weather, uint32_t& output) {
    if (!weather.resolved || static_cast<uint8_t>(weather.cancellationWeather) > 9 || !state.stats[5]) return false;
    double stageMultiplier = 1.0, abilityMultiplier = 1.0;
    if (!pokemonStatStageMultiplier(state, 5, false, stageMultiplier)) return false;
    for (const auto& profile : PokerogueContent::kSpeedAbilityProfiles) {
        if (profile.abilityId != state.abilityId) continue;
        if (profile.requiresCondition) {
            if (!profile.weatherMask) return false;
            if (!(profile.weatherMask & (1u << static_cast<uint8_t>(weather.cancellationWeather)))) continue;
        }
        abilityMultiplier *= profile.multiplier;
    }
    if (!pokemonStatusStateValid(state.status)) return false;
    double value = state.stats[5] * abilityMultiplier * stageMultiplier;
    // Pokemon.getStat(SPD): paralysis truncates then halves before Unburden.
    if (state.status.present && state.status.effect == PokemonStatusEffect::Paralysis) {
        if (!(value >= 0.0) || value > 2147483647.0) return false;
        value = static_cast<uint32_t>(value) >> 1;
    }
    if (state.heldItemLostTags.unburden) {
        for (const auto& profile : PokerogueContent::kHeldItemTheftAbilityProfiles)
            if (profile.abilityId == state.abilityId && profile.appliesUnburden) { value *= 2.0; break; }
    }
    if (!(value >= 0.0) || value > 4294967295.0) return false;
    const auto speed = static_cast<uint32_t>(value);
    output = speed ? speed : 1;
    return true;
}

bool pokemonWeatherMoveAccuracy(uint16_t moveId,
    const PokemonMoveWeatherContext* context, int16_t& outputAccuracy) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move || move->accuracy < -1 || move->accuracy > 100) return false;
    const bool thunder = PokerogueContent::moveHasAttribute(*move, "ThunderAccuracyAttr");
    const bool storm = PokerogueContent::moveHasAttribute(*move, "StormAccuracyAttr");
    const bool blizzard = PokerogueContent::moveHasAttribute(*move, "BlizzardAccuracyAttr");
    if ((thunder || storm || blizzard) && (!context || !context->resolved ||
        static_cast<uint8_t>(context->effectiveWeather) > 9)) return false;
    int16_t accuracy = move->accuracy;
    if (thunder || storm || blizzard) {
        const auto weather = context->effectiveWeather;
        if (thunder && (weather == PokemonEffectiveWeather::Sunny || weather == PokemonEffectiveWeather::HarshSun))
            accuracy = 50;
        if ((thunder || storm) && (weather == PokemonEffectiveWeather::Rain || weather == PokemonEffectiveWeather::HeavyRain))
            accuracy = -1;
        if (blizzard && (weather == PokemonEffectiveWeather::Hail || weather == PokemonEffectiveWeather::Snow))
            accuracy = -1;
    }
    outputAccuracy = accuracy;
    return true;
}

bool pokemonMoveWeatherMultiplier(uint16_t moveId,
    const PokemonMoveWeatherContext& context, double& outputMultiplier) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move || !context.resolved ||
        static_cast<uint8_t>(context.effectiveWeather) > 9) return false;
    // Variable move types require their own resolver. Canonical weather
    // overrides precede the ordinary Fire/Water weather multiplier.
    if (move->upstreamFlags & PokerogueContent::MoveHasVariableMovegenType) return false;
    if (PokerogueContent::moveHasAttribute(*move, "OverrideWeatherMultiplierAttr")) {
        bool represented = false;
        for (const auto& profile : PokerogueContent::kMoveWeatherOverrides) {
            if (profile.moveId != moveId) continue;
            represented = true;
            if (profile.weatherId == static_cast<uint8_t>(context.effectiveWeather)) {
                outputMultiplier = 1.5;
                return true;
            }
        }
        if (!represented) return false;
    }
    double multiplier = 1.0;
    const auto weather = context.effectiveWeather;
    if (weather == PokemonEffectiveWeather::Sunny || weather == PokemonEffectiveWeather::HarshSun) {
        if (sameText(move->type, "FIRE")) multiplier = 1.5;
        else if (sameText(move->type, "WATER")) multiplier = 0.5;
    } else if (weather == PokemonEffectiveWeather::Rain || weather == PokemonEffectiveWeather::HeavyRain) {
        if (sameText(move->type, "FIRE")) multiplier = 0.5;
        else if (sameText(move->type, "WATER")) multiplier = 1.5;
    }
    outputMultiplier = multiplier;
    return true;
}

PokemonDamageCoreResult calculatePokemonDamageCore(
    const PokemonBattleState& attacker,
    const PokemonBattleState& defender,
    uint16_t moveId,
    bool moveIsTypeless,
    uint32_t& outputDamage,
    const PokemonMoveWeatherContext* weatherContext) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move) return PokemonDamageCoreResult::MissingMove;
    const auto* fixedDamage = pokemonFixedDamageMoveProfile(moveId);
    if (move->category == PokerogueContent::MoveStatus || (move->power <= 0 && !fixedDamage)) {
        return PokemonDamageCoreResult::NonDamagingMove;
    }
    const auto* attackerSpecies = PokerogueContent::findSpeciesByDex(attacker.speciesDex);
    const auto* defenderSpecies = PokerogueContent::findSpeciesByDex(defender.speciesDex);
    if (!attackerSpecies || !defenderSpecies) return PokemonDamageCoreResult::MissingSpecies;
    const auto* attackerForm = attacker.formId ? PokerogueContent::findFormById(attacker.formId) : nullptr;
    if (attacker.formId && !attackerForm) return PokemonDamageCoreResult::InvalidType;

    if (fixedDamage) {
        double effectiveness = 1.0;
        if (calculatePokemonTypeEffectiveness(moveId, defender, effectiveness) != PokemonTypeEffectivenessResult::Ok)
            return PokemonDamageCoreResult::InvalidType;
        if (effectiveness == 0.0) { outputDamage = 0; return PokemonDamageCoreResult::Ok; }
        if (!pokemonFixedDamageAbilityCapabilitiesResolved(attacker.abilityId, defender.abilityId))
            return PokemonDamageCoreResult::UnsupportedAbilityCondition;
        if (fixedDamage->userLevel && !attacker.level) return PokemonDamageCoreResult::InvalidStats;
        outputDamage = fixedDamage->targetHalfHp ? (defender.hp > 1 ? defender.hp / 2 : 1) :
            fixedDamage->userLevel ? attacker.level : fixedDamage->amount;
        return PokemonDamageCoreResult::Ok;
    }
    double baseDamage = 0.0;
    double weatherMultiplier = 1.0;
    if (weatherContext && !pokemonMoveWeatherMultiplier(moveId, *weatherContext, weatherMultiplier))
        return PokemonDamageCoreResult::UnresolvedWeather;
    const auto baseStatus = calculatePokemonBaseDamage(attacker, defender, moveId, baseDamage, false, weatherContext);
    if (baseStatus == PokemonBaseDamageResult::UnsupportedAbilityCondition)
        return PokemonDamageCoreResult::UnsupportedAbilityCondition;
    if (baseStatus == PokemonBaseDamageResult::UnresolvedWeather)
        return PokemonDamageCoreResult::UnresolvedWeather;
    if (baseStatus != PokemonBaseDamageResult::Ok) return PokemonDamageCoreResult::InvalidStats;
    double typeMultiplier = 1.0;
    if (calculatePokemonTypeEffectiveness(moveId, defender, typeMultiplier) != PokemonTypeEffectivenessResult::Ok) {
        return PokemonDamageCoreResult::InvalidType;
    }
    if (typeMultiplier == 0.0) {
        outputDamage = 0;
        return PokemonDamageCoreResult::Ok;
    }

    double stabMultiplier = 1.0;
    if (!moveIsTypeless && !sameText(move->type, "STELLAR")) {
        const char* attackerType1 = attackerForm ? attackerForm->type1 : attackerSpecies->type1;
        const char* attackerType2 = attackerForm ? attackerForm->type2 : attackerSpecies->type2;
        if (sameText(move->type, attackerType1) ||
            (attackerType2 && *attackerType2 && !sameText(attackerType2, "NONE") &&
                sameText(move->type, attackerType2))) {
            stabMultiplier = 1.5;
        }
    }

    // Mirrors the pinned deterministic/simulated core: critical=1, random=1,
    // Optional resolved weather is applied; remaining field/status/item
    // modifiers are neutral in this baseline. toDmgValue floors with min 1.
    const double adjusted = baseDamage * weatherMultiplier * stabMultiplier * typeMultiplier;
    if (adjusted > 4294967295.0) return PokemonDamageCoreResult::InvalidStats;
    const uint32_t rounded = static_cast<uint32_t>(adjusted);
    outputDamage = rounded ? rounded : 1;
    return PokemonDamageCoreResult::Ok;
}

bool composePokemonCriticalAbilityPolicy(const PokemonCriticalAbilityComponent* components,
    std::size_t count, bool ignoreDefenderAbilities, PokemonCriticalPolicy& output) {
    if (count && !components) return false;
    PokemonCriticalPolicy next{};
    next.resolved = true;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& component = components[i];
        if (!component.applies) continue;
        if (!PokerogueContent::findAbilityMovegenProfile(component.abilityId)) return false;
        for (const auto& profile : PokerogueContent::kCriticalAbilityProfiles) {
            if (profile.abilityId != component.abilityId) continue;
            if (component.belongsToAttacker) {
                const unsigned sum = next.bonusStages + profile.bonusStages;
                next.bonusStages = static_cast<uint8_t>(sum > 3 ? 3 : sum);
                next.damageMultiplier *= profile.criticalMultiplier;
            } else if (!(ignoreDefenderAbilities && profile.ignorable)) {
                next.blocked = next.blocked || profile.blocksCritical;
            }
        }
    }
    output = next;
    return true;
}

bool pokemonMoveCriticalDenominator(uint16_t moveId, uint8_t& outputDenominator,
    uint8_t resolvedBonusStages) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move || move->category == PokerogueContent::MoveStatus || move->power <= 0 ||
        move->attributeOffset > PokerogueContent::kMoveAttributeCount ||
        move->attributeCount > PokerogueContent::kMoveAttributeCount - move->attributeOffset) return false;
    uint8_t stage = resolvedBonusStages > 3 ? 3 : resolvedBonusStages;
    for (uint16_t i = 0; i < move->attributeCount; ++i)
        if (sameText(PokerogueContent::kMoveAttributes[move->attributeOffset + i].id, "HighCritAttr") && stage < 3)
            ++stage;
    static constexpr uint8_t denominators[4] = {24, 8, 2, 1};
    outputDenominator = PokerogueContent::moveHasAttribute(*move, "CritOnlyAttr") ? 1 : denominators[stage];
    return true;
}

bool composePokemonAlwaysHitPolicy(const PokemonWeatherAbilityComponent* components,
    std::size_t count, PokemonHitPolicy& output, uint16_t moveId,
    const PokemonMoveWeatherContext* weather) {
    if (count && !components) return false;
    PokemonHitPolicy next{};
    next.resolved = true;
    for (std::size_t i = 0; i < count; ++i) {
        if (!components[i].applies) continue;
        if (!PokerogueContent::findAbilityMovegenProfile(components[i].abilityId)) return false;
        if (!components[i].belongsToAttacker && moveId) {
            uint8_t moveMask = 0;
            for (const auto& row : PokerogueContent::kMoveImmunityFlags)
                if (row.moveId == moveId) moveMask = row.mask;
            for (const auto& profile : PokerogueContent::kMoveImmunityAbilityProfiles) {
                if (profile.abilityId != components[i].abilityId) continue;
                if (profile.requiresDispatcher) return false;
                next.blockedByAbility = next.blockedByAbility || (moveMask & profile.mask);
            }
        }
        for (const auto& profile : PokerogueContent::kAlwaysHitAbilityProfiles)
            if (profile.abilityId == components[i].abilityId) next.bypassAccuracy = true;
        for (const auto& profile : PokerogueContent::kAccuracyAbilityProfiles) {
            if (profile.abilityId != components[i].abilityId) continue;
            if (profile.requiresCondition) {
                if (!weather || !weather->resolved || !profile.weatherMask ||
                    static_cast<uint8_t>(weather->cancellationWeather) > 9) return false;
                if (!(profile.weatherMask & (1u << static_cast<uint8_t>(weather->cancellationWeather)))) continue;
            }
            if (profile.requiredCategory >= 0) {
                const auto* move = PokerogueContent::findMoveById(moveId);
                if (!move) return false;
                if (move->category != profile.requiredCategory) continue;
            }
            if (components[i].belongsToAttacker && profile.accuracy)
                next.accuracyMultiplier *= profile.multiplier;
            else if (!components[i].belongsToAttacker && !profile.accuracy)
                next.accuracyMultiplier /= profile.multiplier;
        }
    }
    output = next;
    return true;
}

PokemonMoveDamageResult resolveStandardPokemonMoveDamage(
    const PokemonBattleState& attacker,
    const PokemonBattleState& defender,
    uint16_t moveId,
    bool moveIsTypeless,
    PokerogueRngAdapter& battleRng,
    PokemonMoveDamageRoll& output,
    const PokemonMoveWeatherContext* weatherContext,
    const PokemonCriticalPolicy* criticalPolicy,
    const PokemonHitPolicy* hitPolicy, const PokemonBurnDamagePolicy* burnPolicy) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move) return PokemonMoveDamageResult::MissingMove;
    const auto* fixedDamage = pokemonFixedDamageMoveProfile(moveId);
    if (move->category == PokerogueContent::MoveStatus || (move->power <= 0 && !fixedDamage)) {
        return PokemonMoveDamageResult::NonDamagingMove;
    }
    if (move->accuracy < -1 || move->accuracy > 100) return PokemonMoveDamageResult::InvalidAccuracy;
    double burnMultiplier = 1.0;
    const PokemonBurnDamagePolicy unknownBurnPolicy{};
    if (!fixedDamage && !pokemonBurnDamageMultiplier(attacker, moveId, burnPolicy ? *burnPolicy : unknownBurnPolicy, burnMultiplier))
        return PokemonMoveDamageResult::UnsupportedAbilityCondition;
    const auto* attackerSpecies = PokerogueContent::findSpeciesByDex(attacker.speciesDex);
    const auto* defenderSpecies = PokerogueContent::findSpeciesByDex(defender.speciesDex);
    if (!attackerSpecies || !defenderSpecies) return PokemonMoveDamageResult::MissingSpecies;
    const auto* attackerForm = attacker.formId ? PokerogueContent::findFormById(attacker.formId) : nullptr;
    if (attacker.formId && !attackerForm) return PokemonMoveDamageResult::InvalidType;

    PokemonMoveDamageRoll next{};
    if (calculatePokemonTypeEffectiveness(moveId, defender, next.typeEffectiveness) !=
        PokemonTypeEffectivenessResult::Ok) return PokemonMoveDamageResult::InvalidType;
    // Upstream checks type immunity before accuracy, critical and damage RNG.
    if (next.typeEffectiveness == 0.0) {
        output = next;
        return PokemonMoveDamageResult::Ok;
    }

    double baseDamage = 0.0;
    double weatherMultiplier = 1.0;
    if (weatherContext && !pokemonMoveWeatherMultiplier(moveId, *weatherContext, weatherMultiplier))
        return PokemonMoveDamageResult::UnresolvedWeather;
    if (fixedDamage) {
        // Fixed damage skips offensive modifiers, but unported full-HP endure,
        // immunities and callbacks must still fail explicitly in this frontier.
        if (!pokemonFixedDamageAbilityCapabilitiesResolved(attacker.abilityId, defender.abilityId))
            return PokemonMoveDamageResult::UnsupportedAbilityCondition;
        if (fixedDamage->userLevel && !attacker.level) return PokemonMoveDamageResult::InvalidStats;
    }
    const auto baseStatus = fixedDamage ? PokemonBaseDamageResult::Ok :
        calculatePokemonBaseDamage(attacker, defender, moveId, baseDamage, false, weatherContext);
    if (baseStatus == PokemonBaseDamageResult::UnsupportedAbilityCondition)
        return PokemonMoveDamageResult::UnsupportedAbilityCondition;
    if (baseStatus == PokemonBaseDamageResult::UnresolvedWeather)
        return PokemonMoveDamageResult::UnresolvedWeather;
    if (baseStatus != PokemonBaseDamageResult::Ok) return PokemonMoveDamageResult::InvalidStats;

    int16_t weatherAccuracy = move->accuracy;
    if (!pokemonWeatherMoveAccuracy(moveId, weatherContext, weatherAccuracy))
        return PokemonMoveDamageResult::UnresolvedWeather;
    bool alwaysHits = false;
    // Unsuppressed primary abilities in the current baseline. Type immunity
    // was already checked above; No Guard does not bypass that check.
    for (const auto& profile : PokerogueContent::kAlwaysHitAbilityProfiles)
        if (profile.abilityId == attacker.abilityId || profile.abilityId == defender.abilityId)
            alwaysHits = true;
    double additionalAccuracyMultiplier = 1.0;
    if (hitPolicy) {
        if (!hitPolicy->resolved) return PokemonMoveDamageResult::UnsupportedAbilityCondition;
        if (!(hitPolicy->accuracyMultiplier > 0.0) || hitPolicy->accuracyMultiplier > 256.0)
            return PokemonMoveDamageResult::InvalidAccuracy;
        if (hitPolicy->blockedByAbility) {
            next.abilityBlocked = true;
            output = next;
            return PokemonMoveDamageResult::Ok;
        }
        alwaysHits = hitPolicy->bypassAccuracy;
        additionalAccuracyMultiplier = hitPolicy->accuracyMultiplier;
    }
    if (weatherAccuracy >= 0 && !alwaysHits) {
        double accuracyStage = 1.0;
        if (attacker.statStages[5] < -6 || attacker.statStages[5] > 6 ||
            defender.statStages[6] < -6 || defender.statStages[6] > 6) return PokemonMoveDamageResult::InvalidAccuracy;
        const int accuracy = hitPolicy && hitPolicy->ignoreAttackerAccuracyStage ? 0 : attacker.statStages[5];
        const int evasion = hitPolicy && hitPolicy->ignoreDefenderEvasionStage ? 0 : defender.statStages[6];
        if (!pokemonAccuracyStageMultiplier(accuracy, evasion, accuracyStage))
            return PokemonMoveDamageResult::InvalidAccuracy;
        next.accuracyWasRolled = true;
        next.accuracyRoll = static_cast<uint8_t>(battleRng.randSeedInt(100));
        if (next.accuracyRoll >= weatherAccuracy * accuracyStage * additionalAccuracyMultiplier) {
            output = next;
            return PokemonMoveDamageResult::Ok;
        }
    }
    next.hit = true;
    if (fixedDamage) {
        next.damage = fixedDamage->targetHalfHp ? (defender.hp > 1 ? defender.hp / 2 : 1) :
            fixedDamage->userLevel ? attacker.level : fixedDamage->amount;
        output = next;
        return PokemonMoveDamageResult::Ok;
    }

    // Baseline hitCheck -> getCriticalHitResult -> damage RNG ordering:
    // Canonical HighCrit raises the stage; CritOnly skips the critical draw.
    // Stage0 is1/24; random damage is inclusive [85,100].
    uint8_t bonusCriticalStages = 0;
    double abilityCriticalMultiplier = 1.0;
    bool abilityBlocksCritical = false;
    // Current baseline uses unsuppressed primary abilities. Passive, tag,
    // item, conditional and bypass effects require subsequent policy dispatch.
    for (const auto& profile : PokerogueContent::kCriticalAbilityProfiles) {
        if (profile.abilityId == attacker.abilityId) {
            bonusCriticalStages = profile.bonusStages;
            abilityCriticalMultiplier *= profile.criticalMultiplier;
        }
        if (profile.abilityId == defender.abilityId) abilityBlocksCritical = profile.blocksCritical;
    }
    if (criticalPolicy) {
        if (!criticalPolicy->resolved) return PokemonMoveDamageResult::UnsupportedAbilityCondition;
        if (!(criticalPolicy->damageMultiplier > 0.0) || criticalPolicy->damageMultiplier > 256.0)
            return PokemonMoveDamageResult::InvalidStats;
        abilityCriticalMultiplier = criticalPolicy->damageMultiplier;
        bonusCriticalStages = criticalPolicy->bonusStages;
        abilityBlocksCritical = criticalPolicy->blocked;
    }
    uint8_t criticalDenominator = 24;
    if (!pokemonMoveCriticalDenominator(moveId, criticalDenominator, bonusCriticalStages)) return PokemonMoveDamageResult::InvalidStats;
    next.critical = criticalDenominator == 1 || (criticalPolicy && criticalPolicy->alwaysCritical);
    if (!next.critical) {
        next.criticalWasRolled = true;
        next.criticalRoll = static_cast<uint8_t>(battleRng.randSeedInt(criticalDenominator));
        next.critical = next.criticalRoll == 0;
    }
    // BlockCritAbAttr is applied after the critical RNG/guarantee check.
    if (abilityBlocksCritical) next.critical = false;
    if (next.critical && calculatePokemonBaseDamage(attacker, defender, moveId,
            baseDamage, true, weatherContext) != PokemonBaseDamageResult::Ok)
        return PokemonMoveDamageResult::InvalidStats;
    next.randomDamagePercent = static_cast<uint8_t>(battleRng.randSeedIntRange(85, 100));

    double stabMultiplier = 1.0;
    if (!moveIsTypeless && !sameText(move->type, "STELLAR")) {
        const char* attackerType1 = attackerForm ? attackerForm->type1 : attackerSpecies->type1;
        const char* attackerType2 = attackerForm ? attackerForm->type2 : attackerSpecies->type2;
        if (sameText(move->type, attackerType1) ||
            (attackerType2 && *attackerType2 && !sameText(attackerType2, "NONE") &&
                sameText(move->type, attackerType2))) {
            stabMultiplier = 1.5;
        }
    }

    const double criticalMultiplier = next.critical ? 1.5 * abilityCriticalMultiplier : 1.0;
    const double damage = baseDamage * weatherMultiplier * criticalMultiplier
        * (static_cast<double>(next.randomDamagePercent) / 100.0)
        * stabMultiplier * next.typeEffectiveness * burnMultiplier;
    if (damage > 4294967295.0) return PokemonMoveDamageResult::InvalidStats;
    const uint32_t rounded = static_cast<uint32_t>(damage);
    next.damage = rounded ? rounded : 1;
    output = next;
    return PokemonMoveDamageResult::Ok;
}

bool pokemonActiveTargetsPpCost(const uint16_t* abilityIds, uint8_t count, uint8_t& output) {
    if (count && !abilityIds) return false;
    uint16_t cost = 1;
    // MovePhase.usePP: start at one, then apply IncreasePpUsedAbAttr per active target.
    for (uint8_t i = 0; i < count; ++i) {
        if (!PokerogueContent::findAbilityMovegenProfile(abilityIds[i])) return false;
        for (const auto& profile : PokerogueContent::kPpAbilityProfiles)
            if (profile.abilityId == abilityIds[i]) cost += profile.increase;
        if (cost > 255) return false;
    }
    output = static_cast<uint8_t>(cost);
    return true;
}

bool pokemonSingleOpponentPpCost(uint16_t opponentAbilityId, uint8_t& output) {
    return pokemonActiveTargetsPpCost(&opponentAbilityId, 1, output);
}

PokemonMoveActionStatus useStandardPokemonMove(
    PokemonBattleState& attacker,
    PokemonBattleState& defender,
    uint8_t moveSlot,
    bool moveIsTypeless,
    PokerogueRngAdapter& battleRng,
    PokemonMoveActionResult& output,
    const PokemonMoveWeatherContext* weatherContext,
    const PokemonCriticalPolicy* criticalPolicy,
    const PokemonHitPolicy* hitPolicy,
    const PokemonPpPolicy* ppPolicy,
    PokemonBossState* targetBossState, const PokemonBossDamagePolicy* bossDamagePolicy,
    PokerogueRngAdapter* bossGlobalRng, const PokemonBurnDamagePolicy* burnPolicy) {
    if (moveSlot >= attacker.moveCount || moveSlot >= 4 || attacker.moves[moveSlot].moveId == 0) {
        return PokemonMoveActionStatus::InvalidMoveSlot;
    }
    if ((targetBossState != nullptr) != (bossDamagePolicy != nullptr) ||
        (targetBossState && (!bossGlobalRng || bossGlobalRng == &battleRng || !bossDamagePolicy->resolved || !bossDamagePolicy->damageCallbacksResolved ||
            !targetBossState->segmentCount || targetBossState->segmentIndex >= targetBossState->segmentCount ||
            !defender.maxHp || defender.hp > defender.maxHp || &attacker == &defender)))
        return PokemonMoveActionStatus::UnresolvedBoss;
    if (ppPolicy && !ppPolicy->resolved) return PokemonMoveActionStatus::UnresolvedPp;
    const uint8_t ppCost = ppPolicy ? ppPolicy->cost : 1;
    if (attacker.moves[moveSlot].pp == 0 && ppCost) return PokemonMoveActionStatus::NoPp;
    if (attacker.moves[moveSlot].pp > attacker.moves[moveSlot].maxPp)
        return PokemonMoveActionStatus::InvalidMoveSlot;
    if (defender.hp == 0) return PokemonMoveActionStatus::TargetAlreadyFainted;

    PokemonMoveActionResult next{};
    next.ppConsumed = ppCost < attacker.moves[moveSlot].pp ? ppCost : attacker.moves[moveSlot].pp;
    if (weatherContext) {
        const auto* move = PokerogueContent::findMoveById(attacker.moves[moveSlot].moveId);
        if (!weatherContext->resolved || static_cast<uint8_t>(weatherContext->effectiveWeather) > 9 ||
            static_cast<uint8_t>(weatherContext->cancellationWeather) > 9)
            return PokemonMoveActionStatus::UnresolvedWeather;
        if (!move) return PokemonMoveActionStatus::DamageResolutionFailed;
        if (move->upstreamFlags & PokerogueContent::MoveHasVariableMovegenType)
            return PokemonMoveActionStatus::UnresolvedWeather;
        const bool cancelled = move->category != PokerogueContent::MoveStatus &&
            ((weatherContext->cancellationWeather == PokemonEffectiveWeather::HarshSun &&
                sameText(move->type, "WATER")) ||
             (weatherContext->cancellationWeather == PokemonEffectiveWeather::HeavyRain &&
                sameText(move->type, "FIRE")));
        if (cancelled) {
            // MovePhase.usePP precedes secondFailureCheck (primal weather).
            attacker.moves[moveSlot].pp = static_cast<uint8_t>(attacker.moves[moveSlot].pp - next.ppConsumed);
            next.weatherCancelled = true;
            output = next;
            return PokemonMoveActionStatus::Ok;
        }
    }
    // Commit RNG only after a resolved action, just like HP and PP.
    PokerogueRngAdapter nextRng = battleRng;
    // weightedPick in shield boosts uses Phaser.Math.RND, not Battle.randSeedInt.
    PokerogueRngAdapter nextGlobalRng;
    if (bossGlobalRng) nextGlobalRng = *bossGlobalRng;
    next.damageResolutionStatus = resolveStandardPokemonMoveDamage(
        attacker, defender, attacker.moves[moveSlot].moveId, moveIsTypeless, nextRng, next.damageRoll, weatherContext, criticalPolicy, hitPolicy, burnPolicy);
    if (next.damageResolutionStatus == PokemonMoveDamageResult::UnsupportedAbilityCondition)
        return PokemonMoveActionStatus::UnsupportedAbilityCondition;
    if (next.damageResolutionStatus == PokemonMoveDamageResult::UnresolvedWeather)
        return PokemonMoveActionStatus::UnresolvedWeather;
    if (next.damageResolutionStatus != PokemonMoveDamageResult::Ok) {
        return PokemonMoveActionStatus::DamageResolutionFailed;
    }

    auto nextDefender = defender;
    PokemonBossState nextBossState{};
    if (targetBossState) nextBossState = *targetBossState;
    if (next.damageRoll.hit && next.damageRoll.damage > 0) {
        if (targetBossState) {
            PokemonBossDamageEvent bossEvent{};
            if (!applyPokemonBossDamage(nextDefender, nextBossState, next.damageRoll.damage,
                    *bossDamagePolicy, nextGlobalRng, bossEvent)) return PokemonMoveActionStatus::UnresolvedBoss;
            next.damageApplied = bossEvent.damageApplied;
        } else {
            next.damageApplied = static_cast<uint16_t>(next.damageRoll.damage < nextDefender.hp
                ? next.damageRoll.damage : nextDefender.hp);
            nextDefender.hp -= next.damageApplied;
        }
        next.targetFainted = nextDefender.hp == 0;
    }
    if (attacker.turnDamageDealt > 0xFFFFFFFFu - next.damageApplied)
        return PokemonMoveActionStatus::DamageResolutionFailed;
    // Commit resolved PP, target/shields/stages and RNG together after all phases succeed.
    defender = nextDefender;
    if (targetBossState) {
        *targetBossState = nextBossState;
        *bossGlobalRng = nextGlobalRng;
    }
    attacker.turnDamageDealt += next.damageApplied;
    attacker.moves[moveSlot].pp = static_cast<uint8_t>(attacker.moves[moveSlot].pp - next.ppConsumed);
    battleRng = nextRng;
    output = next;
    return PokemonMoveActionStatus::Ok;
}

PokemonAbilitySelectionResult selectPokemonAbilityIndex(
    uint16_t speciesDex,
    uint16_t hiddenAbilityRate,
    PokerogueRngAdapter& rng,
    uint8_t& outputAbilityIndex) {
    const auto* species = PokerogueContent::findSpeciesByDex(speciesDex);
    if (!species) return PokemonAbilitySelectionResult::MissingSpecies;
    if (hiddenAbilityRate == 0) return PokemonAbilitySelectionResult::InvalidHiddenRate;

    const uint8_t regularAbilityIndex = species->ability2 == species->ability1
        ? 0 : static_cast<uint8_t>(rng.randSeedInt(2));
    const bool useHiddenAbility = species->abilityHidden != 0 && rng.randSeedInt(hiddenAbilityRate) == 0;
    outputAbilityIndex = useHiddenAbility ? 2 : regularAbilityIndex;
    return PokemonAbilitySelectionResult::Ok;
}

PokemonGenderSelectionResult selectPokemonGender(
    uint16_t speciesDex,
    PokerogueRngAdapter& rng,
    PokemonGender& outputGender) {
    const auto* species = PokerogueContent::findSpeciesByDex(speciesDex);
    if (!species) return PokemonGenderSelectionResult::MissingSpecies;

    // 65534 is the canonical encoding for upstream's null gender ratio;
    // 65535 means the importer had no value and must not become a fake gender.
    if (species->malePercentTenths == 65534) {
        outputGender = PokemonGender::Genderless;
        return PokemonGenderSelectionResult::Ok;
    }
    if (species->malePercentTenths > 1000) return PokemonGenderSelectionResult::InvalidGenderRatio;

    // Upstream consumes randSeedFloat for every non-null ratio, including 0/100.
    const double rollPercent = rng.frac() * 100.0;
    outputGender = rollPercent <= (static_cast<double>(species->malePercentTenths) / 10.0)
        ? PokemonGender::Male : PokemonGender::Female;
    return PokemonGenderSelectionResult::Ok;
}

bool planPokemonBossSegmentCleared(const PokemonBattleState& boss, uint16_t segmentCount,
    uint16_t currentSegmentIndex, uint16_t clearedSegmentIndex, bool hasTrainer,
    PokerogueRngAdapter& rng, PokemonBossSegmentClearEvent& output) {
    if (!segmentCount || currentSegmentIndex >= segmentCount || clearedSegmentIndex > currentSegmentIndex + 1)
        return false;
    for (uint8_t stat = 0; stat < 5; ++stat)
        if (!boss.stats[stat + 1] || boss.statStages[stat] < -6 || boss.statStages[stat] > 6) return false;
    auto nextRng = rng;
    PokemonBossSegmentClearEvent event{};
    event.nextSegmentIndex = currentSegmentIndex;
    uint8_t firstPending[5]{};
    bool boost = !hasTrainer;
    while (event.nextSegmentIndex && (!clearedSegmentIndex || event.nextSegmentIndex >= clearedSegmentIndex)) {
        --event.nextSegmentIndex;
        if (!boost) continue;
        uint32_t totalWeight = 0;
        for (uint8_t stat = 0; stat < 5; ++stat)
            if (boss.statStages[stat] + firstPending[stat] < 6) totalWeight += boss.stats[stat + 1];
        if (!totalWeight) { boost = false; continue; }
        const uint32_t roll = static_cast<uint32_t>(nextRng.randSeedInt(static_cast<int32_t>(totalWeight)));
        uint32_t cumulative = 0;
        uint8_t selected = 5;
        for (uint8_t stat = 0; stat < 5; ++stat) {
            if (boss.statStages[stat] + firstPending[stat] >= 6) continue;
            cumulative += boss.stats[stat + 1];
            if (roll < cumulative) { selected = stat; break; }
        }
        if (selected == 5) return false;
        const uint8_t stages = 1 + (segmentCount >= 3 && event.nextSegmentIndex == 0) +
            (segmentCount >= 5 && event.nextSegmentIndex == 1);
        // Upstream filters against changes.find: the first pending change for this stat.
        if (!firstPending[selected]) firstPending[selected] = stages;
        event.statStages[selected] += stages;
    }
    rng = nextRng;
    output = event;
    return true;
}

bool calculatePokemonBossSegmentDamage(uint32_t damage, uint16_t currentHp,
    uint16_t maxHp, uint16_t segmentCount, uint16_t currentSegmentIndex,
    uint16_t minimumSegmentIndex, PokemonBossSegmentDamage& output) {
    if (!maxHp || !currentHp || currentHp > maxHp || !segmentCount ||
        currentSegmentIndex >= segmentCount || minimumSegmentIndex > currentSegmentIndex)
        return false;
    PokemonBossSegmentDamage result{damage, 1};
    if (!currentSegmentIndex) { output = result; return true; }
    const double segmentHp = static_cast<double>(maxHp) / segmentCount;
    const double threshold = segmentHp * currentSegmentIndex;
    const uint32_t roundedThreshold = static_cast<uint32_t>(threshold + 0.5);
    const int64_t remaining = static_cast<int64_t>(currentHp) - roundedThreshold;
    const int64_t leftover = static_cast<int64_t>(damage) - remaining;
    if (leftover < 0) {
        result.clearedSegmentIndex = currentSegmentIndex + 1;
    } else if (!leftover) {
        result.clearedSegmentIndex = currentSegmentIndex;
    } else {
        // floor(log2(leftover / segmentHp)), clamped to [0, index - minimum].
        // Doubling avoids a libm logarithm on ARM11 and consumes no RNG.
        const uint16_t maximumBypass = currentSegmentIndex - minimumSegmentIndex;
        uint16_t bypass = 0;
        double boundary = segmentHp * 2.0;
        while (bypass < maximumBypass && static_cast<double>(leftover) >= boundary) {
            ++bypass;
            boundary *= 2.0;
        }
        const double adjusted = currentHp - threshold + segmentHp * bypass;
        if (adjusted > 4294967295.0) return false;
        result.adjustedDamage = adjusted < 1.0 ? 1 : static_cast<uint32_t>(adjusted);
        result.clearedSegmentIndex = currentSegmentIndex - bypass;
    }
    output = result;
    return true;
}

bool initializeClassicPokemonBossState(uint16_t speciesDex, uint16_t level, uint32_t wave,
    bool forceBoss, bool finalBossFirstPhase, PokemonBossState& output) {
    const auto* species = PokerogueContent::findSpeciesByDex(speciesDex);
    if (!species || !level || !wave || wave > PokerogueContent::kClassicFinalWave ||
        (finalBossFirstPhase && (!forceBoss || wave != PokerogueContent::kClassicFinalWave))) return false;
    PokemonBossState state{};
    const bool isBoss = forceBoss || wave % 10 == 0 || species->legendary == 1 ||
        species->subLegendary == 1 || species->mythical == 1;
    if (isBoss) {
        state.segmentCount = static_cast<uint16_t>(2 + (level >= 100) + (species->baseTotal >= 670) + wave / 250);
        state.segmentIndex = state.segmentCount - 1;
        state.classicFinalBossFirstPhase = finalBossFirstPhase;
    }
    output = state;
    return true;
}

bool distributePokemonDoubleBossSegments(PokemonBossState& first, uint16_t firstBaseTotal,
    PokemonBossState& second, uint16_t secondBaseTotal) {
    if (!first.segmentCount || !second.segmentCount) return true;
    if (!firstBaseTotal || !secondBaseTotal || first.segmentIndex >= first.segmentCount ||
        second.segmentIndex >= second.segmentCount || first.classicFinalBossFirstPhase ||
        second.classicFinalBossFirstPhase) return false;
    const uint32_t total = static_cast<uint32_t>(firstBaseTotal) + secondBaseTotal;
    const uint16_t firstCount = static_cast<uint16_t>(
        (static_cast<uint32_t>(first.segmentCount) * firstBaseTotal + total - 1) / total);
    const uint16_t secondCount = static_cast<uint16_t>(
        (static_cast<uint32_t>(second.segmentCount) * secondBaseTotal + total - 1) / total);
    first.segmentCount = firstCount;
    first.segmentIndex = firstCount - 1;
    second.segmentCount = secondCount;
    second.segmentIndex = secondCount - 1;
    return true;
}

bool applyPokemonBossDamage(PokemonBattleState& boss, PokemonBossState& state,
    uint32_t damage, const PokemonBossDamagePolicy& policy, PokerogueRngAdapter& rng,
    PokemonBossDamageEvent& output) {
    if (!policy.resolved || !policy.damageCallbacksResolved || !state.segmentCount ||
        state.segmentIndex >= state.segmentCount || !boss.maxHp || boss.hp > boss.maxHp)
        return false;
    PokemonBossDamageEvent event{};
    event.segments.nextSegmentIndex = state.segmentIndex;
    if (!boss.hp) { output = event; return true; }
    auto nextBoss = boss;
    auto nextState = state;
    auto nextRng = rng;
    uint16_t cleared = state.segmentIndex + 1;
    if (!policy.ignoreSegments) {
        PokemonBossSegmentDamage segmentDamage{};
        const uint16_t minimum = state.classicFinalBossFirstPhase && state.segmentIndex ? 1 : 0;
        if (!calculatePokemonBossSegmentDamage(damage, boss.hp, boss.maxHp,
                state.segmentCount, state.segmentIndex, minimum, segmentDamage)) return false;
        damage = segmentDamage.adjustedDamage;
        cleared = segmentDamage.clearedSegmentIndex;
    }
    // EnemyPokemon.damage prevents fainting before Eternamax transformation.
    if (state.classicFinalBossFirstPhase && !state.segmentIndex && damage >= boss.hp) {
        damage = boss.hp - 1;
        event.preventedFinalBossKo = true;
    }
    event.damageApplied = static_cast<uint16_t>(damage < boss.hp ? damage : boss.hp);
    nextBoss.hp -= event.damageApplied;
    if (policy.ignoreSegments) {
        cleared = static_cast<uint16_t>((static_cast<uint64_t>(nextBoss.hp) * state.segmentCount +
            boss.maxHp - 1) / boss.maxHp);
    }
    if (cleared <= state.segmentIndex) {
        if (!planPokemonBossSegmentCleared(nextBoss, state.segmentCount, state.segmentIndex,
                cleared, state.hasTrainer, nextRng, event.segments)) return false;
        nextState.segmentIndex = event.segments.nextSegmentIndex;
        for (uint8_t stat = 0; stat < 5; ++stat)
            if (event.segments.statStages[stat] && !setPokemonStatStage(nextBoss, stat + 1,
                    nextBoss.statStages[stat] + static_cast<int32_t>(event.segments.statStages[stat]))) return false;
    }
    boss = nextBoss;
    state = nextState;
    rng = nextRng;
    output = event;
    return true;
}

} // namespace Pokerogue3DS

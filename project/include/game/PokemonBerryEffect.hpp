#pragma once
#include "game/PokemonBattleState.hpp"
#include "game/PokerogueRngAdapter.hpp"
#include "game/PokerogueModifierReward.hpp"
#include "game/PokerogueBattleRng.hpp"
#include "game/PokemonHealingEffect.hpp"
#include "game/PokemonStatStageEffect.hpp"
#include <cmath>
#include <cstring>

namespace Pokerogue3DS {

// getBerryPredicate/getBerryEffectFunc produce requests. BerryPhase and the
// existing heal/stat/status phases own application, item loss and callbacks.
struct PokemonBerryEffectPolicy {
    bool callbacksResolved = false;
    bool active = true;
    bool attackHistoryResolved = false;
    bool superEffectiveHitReceived = false;
    bool criticalTagResolved = false;
    bool criticalBoostPresent = false;
    double thresholdMultiplier = 1.0;
    double effectMultiplier = 1.0;
};
struct PokemonBerryEffectPlan {
    uint16_t berryType = 0;
    uint32_t ownerPokemonId = 0;
    uint32_t healingRequested = 0;
    uint8_t stat = 0;
    int16_t stagesRequested = 0;
    bool cureStatus = false;
    bool cureConfusion = false;
    bool addCriticalBoost = false;
    int8_t ppSlot = -1;
    uint8_t ppAfter = 0;
};
enum class PokemonBerryEffectResult : uint8_t { Ready, NoEffect, UnresolvedPolicy, Unsupported, InvalidState, HistoryCapacityExceeded, PhaseCapacityExceeded };

inline const PokerogueContent::BerryEffectProfile* pokemonBerryEffectProfile(uint16_t id) {
    for (const auto& row : PokerogueContent::kBerryEffectProfiles)
        if (row.id == id) return &row;
    return nullptr;
}

inline const PokerogueContent::BerryAbilityProfile* pokemonBerryAbilityProfile(uint16_t id) {
    for (const auto& row : PokerogueContent::kBerryAbilityProfiles)
        if (row.abilityId == id) return &row;
    return nullptr;
}

// Caller resolves ability/passive applicability and suppression. Only constant
// imported threshold/effect callbacks are composed here; other Berry triggers
// keep their own phase and are not treated as executed by this resolver.
inline PokemonBerryEffectResult resolvePokemonBerryAbilityEffects(uint16_t berryType,
    const PokemonBattleState& actor, const uint16_t* abilityIds, size_t abilityCount,
    bool applicabilityResolved, PokemonBerryEffectPolicy& output) {
    if (!applicabilityResolved) return PokemonBerryEffectResult::UnresolvedPolicy;
    const auto* berry = pokemonBerryEffectProfile(berryType);
    if (!berry || !berry->resolved) return PokemonBerryEffectResult::Unsupported;
    if ((abilityCount && !abilityIds) || !actor.maxHp || actor.hp > actor.maxHp)
        return PokemonBerryEffectResult::InvalidState;
    auto next = output;
    next.thresholdMultiplier = next.effectMultiplier = 1;
    const double ratio = static_cast<double>(actor.hp) / actor.maxHp;
    for (size_t i = 0; i < abilityCount; ++i) {
        const auto* profile = pokemonBerryAbilityProfile(abilityIds[i]);
        if (!profile || !profile->resolved) return PokemonBerryEffectResult::UnresolvedPolicy;
        for (size_t j = 0; j < i; ++j)
            if (abilityIds[j] == abilityIds[i]) return PokemonBerryEffectResult::InvalidState;
        if (berry->doubledEffectCallback) next.effectMultiplier *= profile->effectMultiplier;
        // ReduceBerryUseThresholdAbAttr.canApply reads the current holder each
        // time, including the result of a preceding primary/passive callback.
        if (berry->thresholdCallback && berry->hpThreshold * next.thresholdMultiplier < ratio)
            next.thresholdMultiplier *= profile->thresholdMultiplier;
        if (!std::isfinite(next.effectMultiplier) || next.effectMultiplier <= 0 ||
            !std::isfinite(next.thresholdMultiplier) || next.thresholdMultiplier <= 0)
            return PokemonBerryEffectResult::InvalidState;
    }
    next.callbacksResolved = true;
    output = next;
    return PokemonBerryEffectResult::Ready;
}

inline PokemonBerryEffectResult resolvePokemonOpponentBerryBlock(const uint16_t* abilityIds,
    size_t abilityCount, bool applicabilityResolved, bool& output) {
    if (!applicabilityResolved) return PokemonBerryEffectResult::UnresolvedPolicy;
    if (abilityCount && !abilityIds) return PokemonBerryEffectResult::InvalidState;
    bool blocked = false;
    for (size_t i = 0; i < abilityCount; ++i) {
        const auto* profile = pokemonBerryAbilityProfile(abilityIds[i]);
        if (!profile || !profile->resolved) return PokemonBerryEffectResult::UnresolvedPolicy;
        blocked |= profile->preventsUse;
    }
    output = blocked;
    return PokemonBerryEffectResult::Ready;
}

// Evaluate one Berry at its position in the modifier list. Stat/heal requests
// are deferred, so later berries must see the state before queued phases run.
// Starf uses global randSeedInt, never the battle RNG stream.
inline PokemonBerryEffectResult planPokemonBerryEffect(uint16_t id, const PokemonBattleState& actor,
    const PokemonBerryEffectPolicy& policy, PokerogueRngAdapter& globalRng, PokemonBerryEffectPlan& output) {
    const auto* profile = pokemonBerryEffectProfile(id);
    if (!profile || !profile->resolved) return PokemonBerryEffectResult::Unsupported;
    if (!policy.callbacksResolved) return PokemonBerryEffectResult::UnresolvedPolicy;
    if (!actor.maxHp || actor.hp > actor.maxHp || actor.moveCount > 4 || actor.berryCriticalBoostStages > 2 ||
        !pokemonStatusStateValid(actor.status) || !validPokemonConfusionTag(actor.confusion) ||
        !std::isfinite(policy.thresholdMultiplier) || policy.thresholdMultiplier <= 0 ||
        !std::isfinite(policy.effectMultiplier) || policy.effectMultiplier <= 0)
        return PokemonBerryEffectResult::InvalidState;
    for (uint8_t i = 0; i < 7; ++i)
        if (actor.statStages[i] < -6 || actor.statStages[i] > 6) return PokemonBerryEffectResult::InvalidState;
    for (uint8_t i = 0; i < actor.moveCount; ++i)
        if (!actor.moves[i].maxPp || actor.moves[i].pp > actor.moves[i].maxPp) return PokemonBerryEffectResult::InvalidState;
    if (!policy.active || !actor.hp) return PokemonBerryEffectResult::NoEffect;
    bool eligible = false;
    const double ratio = static_cast<double>(actor.hp) / actor.maxHp;
    if (!std::strcmp(profile->predicate, "LOW_HP")) {
        // Pinned Starf uses literal .25 even after its threshold callback.
        eligible = ratio < profile->hpThreshold;
    } else if (!std::strcmp(profile->predicate, "LOW_HP_STAT")) {
        if (profile->stat < 1 || profile->stat > 7) return PokemonBerryEffectResult::Unsupported;
        eligible = ratio < profile->hpThreshold * policy.thresholdMultiplier && actor.statStages[profile->stat - 1] < 6;
    } else if (!std::strcmp(profile->predicate, "LOW_HP_NO_CRIT")) {
        if (!policy.criticalTagResolved) return PokemonBerryEffectResult::UnresolvedPolicy;
        // Lansat also ignores the computed threshold holder in this snapshot.
        eligible = ratio < profile->hpThreshold && !policy.criticalBoostPresent && !actor.berryCriticalBoostStages;
    } else if (!std::strcmp(profile->predicate, "STATUS_OR_CONFUSION")) {
        eligible = actor.status.present || actor.confusion.present;
    } else if (!std::strcmp(profile->predicate, "SUPER_EFFECTIVE_RECEIVED")) {
        if (!policy.attackHistoryResolved) return PokemonBerryEffectResult::UnresolvedPolicy;
        eligible = policy.superEffectiveHitReceived;
    } else if (!std::strcmp(profile->predicate, "EMPTY_PP")) {
        for (uint8_t i = 0; i < actor.moveCount; ++i) eligible |= actor.moves[i].pp == 0;
    } else return PokemonBerryEffectResult::Unsupported;
    if (!eligible) return PokemonBerryEffectResult::NoEffect;
    PokemonBerryEffectPlan plan{};
    plan.berryType = id;
    plan.ownerPokemonId = actor.pokemonId;
    auto nextRng = globalRng;
    if (!std::strcmp(profile->effect, "HEAL")) {
        if (!profile->amount) return PokemonBerryEffectResult::Unsupported;
        const double base = std::fmax(std::floor(static_cast<double>(actor.maxHp) / profile->amount), static_cast<double>(PokerogueContent::kBerryHealingMinimum));
        const double amount = base * policy.effectMultiplier;
        if (!std::isfinite(amount) || amount < 1 || amount > UINT32_MAX || std::floor(amount) != amount)
            return PokemonBerryEffectResult::InvalidState;
        plan.healingRequested = static_cast<uint32_t>(amount);
    } else if (!std::strcmp(profile->effect, "CURE_STATUS")) {
        plan.cureStatus = true;
        plan.cureConfusion = true;
    } else if (!std::strcmp(profile->effect, "CRIT_BOOST")) {
        plan.addCriticalBoost = true;
    } else if (!std::strcmp(profile->effect, "STAT") || !std::strcmp(profile->effect, "RANDOM_STAT")) {
        const double stages = profile->amount * policy.effectMultiplier;
        if (!std::isfinite(stages) || stages < 1 || stages > 32767 || std::floor(stages) != stages)
            return PokemonBerryEffectResult::InvalidState;
        plan.stat = profile->stat;
        if (!std::strcmp(profile->effect, "RANDOM_STAT")) {
            if (!PokerogueContent::kBerryRandomStatRange || PokerogueContent::kBerryRandomStatMinimum < 1 ||
                PokerogueContent::kBerryRandomStatMinimum + PokerogueContent::kBerryRandomStatRange - 1 > 7)
                return PokemonBerryEffectResult::Unsupported;
            plan.stat = static_cast<uint8_t>(nextRng.randSeedInt(PokerogueContent::kBerryRandomStatRange,
                PokerogueContent::kBerryRandomStatMinimum));
        }
        if (plan.stat < 1 || plan.stat > 7) return PokemonBerryEffectResult::Unsupported;
        plan.stagesRequested = static_cast<int16_t>(stages);
    } else if (!std::strcmp(profile->effect, "RESTORE_PP")) {
        for (uint8_t i = 0; i < actor.moveCount; ++i)
            if (!actor.moves[i].pp) { plan.ppSlot = static_cast<int8_t>(i); break; }
        if (plan.ppSlot < 0) return PokemonBerryEffectResult::InvalidState;
        const auto& move = actor.moves[plan.ppSlot];
        const uint32_t restored = move.pp + profile->amount;
        plan.ppAfter = restored > move.maxPp ? move.maxPp : static_cast<uint8_t>(restored);
    } else return PokemonBerryEffectResult::Unsupported;
    globalRng = nextRng;
    output = plan;
    return PokemonBerryEffectResult::Ready;
}
// Persistent preservation modifiers are visited in list order. The ||= in
// PreserveBerryModifier short-circuits subsequent RNG calls after success.
inline bool resolvePokemonBerryPreservation(const uint16_t* stacks, size_t count,
    PokerogueBattleRng& battleRng, bool& output) {
    if ((count && !stacks) || !PokerogueContent::kBerryPreserveRollRange ||
        !PokerogueContent::kBerryPreserveChancePerStack || !PokerogueContent::kBerryPreserveMaxStacks) return false;
    for (size_t i = 0; i < count; ++i)
        if (!stacks[i] || stacks[i] > PokerogueContent::kBerryPreserveMaxStacks) return false;
    auto nextRng = battleRng;
    bool preserved = false;
    for (size_t i = 0; i < count && !preserved; ++i) {
        int32_t roll = 0;
        if (!nextRng.randSeedInt(PokerogueContent::kBerryPreserveRollRange, roll)) return false;
        preserved = roll < stacks[i] * PokerogueContent::kBerryPreserveChancePerStack;
    }
    battleRng = nextRng;
    output = preserved;
    return true;
}
// Result of BerryModifier.apply followed by BerryPhase's loseHeldItem.
// The phase consumer must record this event in battle and turn history before
// accepting another command; preserved berries count as eaten, not harvested.
struct PokemonBerryConsumedEvent {
    uint16_t berryType = 0;
    uint32_t ownerPokemonId = 0;
    bool eaten = false;
    bool consumed = false;
    bool harvestEligible = false;
    uint16_t stacksBefore = 0;
    uint16_t stacksAfter = 0;
};
// Ordered lists correspond to PokemonBattleData.berriesEaten,
// PokemonTurnData.berriesEaten and PokemonSummonData.berriesEatenLast.
// Storage is supplied by the run owner; capacity is not a catalog limit.
struct PokemonBerryHistoryList {
    uint16_t* values = nullptr;
    size_t count = 0;
    size_t capacity = 0;
};
struct PokemonBerryHistoryView {
    uint32_t ownerPokemonId = 0;
    PokemonBerryHistoryList battleConsumed{};
    PokemonBerryHistoryList turnEaten{};
    PokemonBerryHistoryList lastTurnEaten{};
};
enum class PokemonBerryHistoryResult : uint8_t { Recorded, CapacityExceeded, InvalidState };
inline bool validPokemonBerryHistoryList(const PokemonBerryHistoryList& list) {
    if (list.count > list.capacity || (list.capacity && !list.values)) return false;
    for (size_t i = 0; i < list.count; ++i) if (!canonicalBerryType(list.values[i])) return false;
    return true;
}
inline PokemonBerryHistoryResult recordPokemonBerryHistory(PokemonBerryHistoryView& history,
    const PokemonBerryConsumedEvent& event) {
    if (!event.eaten || event.ownerPokemonId != history.ownerPokemonId || !canonicalBerryType(event.berryType) ||
        event.consumed != event.harvestEligible || !event.stacksBefore ||
        event.stacksAfter != event.stacksBefore - (event.consumed ? 1 : 0) ||
        !validPokemonBerryHistoryList(history.battleConsumed) || !validPokemonBerryHistoryList(history.turnEaten) ||
        !validPokemonBerryHistoryList(history.lastTurnEaten)) return PokemonBerryHistoryResult::InvalidState;
    if (history.turnEaten.count == history.turnEaten.capacity ||
        (event.consumed && history.battleConsumed.count == history.battleConsumed.capacity))
        return PokemonBerryHistoryResult::CapacityExceeded;
    // Duplicates retain their positions for Harvest's seeded selection.
    if (event.consumed) history.battleConsumed.values[history.battleConsumed.count++] = event.berryType;
    history.turnEaten.values[history.turnEaten.count++] = event.berryType;
    return PokemonBerryHistoryResult::Recorded;
}
// CudChewRecordBerryAbAttr runs only when its applicability is resolved.
// Its previous-turn list is replaced, not appended. Turn start clears turnData.
inline PokemonBerryHistoryResult recordPokemonBerryTurnEnd(PokemonBerryHistoryView& history,
    bool applicabilityResolved, bool cudChewActive) {
    if (!applicabilityResolved || !validPokemonBerryHistoryList(history.turnEaten) ||
        !validPokemonBerryHistoryList(history.lastTurnEaten)) return PokemonBerryHistoryResult::InvalidState;
    if (!cudChewActive) return PokemonBerryHistoryResult::Recorded;
    if (history.turnEaten.count > history.lastTurnEaten.capacity) return PokemonBerryHistoryResult::CapacityExceeded;
    for (size_t i = 0; i < history.turnEaten.count; ++i)
        history.lastTurnEaten.values[i] = history.turnEaten.values[i];
    history.lastTurnEaten.count = history.turnEaten.count;
    return PokemonBerryHistoryResult::Recorded;
}
inline void resetPokemonBerryTurnHistory(PokemonBerryHistoryView& history) { history.turnEaten.count = 0; }
inline void resetPokemonBerrySummonHistory(PokemonBerryHistoryView& history) {
    history.turnEaten.count = history.lastTurnEaten.count = 0;
}
// Pokemon.resetBattleAndWaveData: new biome/trainer/Mystery Encounter, not each wave.
inline void resetPokemonBerryArenaTransitionHistory(PokemonBerryHistoryView& history, PokemonBattleState& actor) {
    history.battleConsumed.count = history.turnEaten.count = history.lastTurnEaten.count = 0;
    actor.hasEatenBerry = false;
}

inline bool consumePokemonHeldBerry(NativeHeldModifierInstance* records, size_t capacity, size_t& count,
    size_t index, PokemonBattleState& holder, bool preserveResolved, bool preserved,
    const uint16_t* abilityIds, size_t abilityCount, bool applicabilityResolved,
    PokemonBerryConsumedEvent& output) {
    if (!preserveResolved || !records || count > capacity || index >= count || !holder.hp) return false;
    uint16_t type = 0;
    if (!heldBerryType(records[index], type) || records[index].ownerPokemonId != holder.pokemonId) return false;
    auto tags = holder.heldItemLostTags;
    const auto dispatch = [&]() {
        const auto result = applyHeldItemLostCallbacks(abilityIds, abilityCount, applicabilityResolved, false, tags);
        return result == HeldItemLostCallbackResult::Applied || result == HeldItemLostCallbackResult::NoChange;
    };
    // The pinned BerryModifier triggers PostItemLost even when Berry Pouch
    // preserves it. loseHeldItem triggers it again only after actual loss.
    if (!dispatch() || (!preserved && !dispatch())) return false;
    PokemonBerryConsumedEvent event{};
    event.berryType = type;
    event.ownerPokemonId = holder.pokemonId;
    event.eaten = true;
    event.consumed = event.harvestEligible = !preserved;
    event.stacksBefore = records[index].stackCount;
    event.stacksAfter = event.stacksBefore - (preserved ? 0 : 1);
    if (!preserved) {
        if (event.stacksAfter) records[index].stackCount = event.stacksAfter;
        else {
            for (size_t i = index + 1; i < count; ++i) records[i - 1] = records[i];
            records[--count] = {};
        }
    }
    holder.heldItemLostTags = tags;
    holder.hasEatenBerry = true; // recordEatenBerry also runs for preserved berries.
    output = event;
    return true;
}
struct PokemonBerryQueuedStatChange { uint8_t stat = 0; int16_t stages = 0; };
struct PokemonBerryStatQueue {
    uint32_t ownerPokemonId = 0;
    uint8_t count = 0;
    PokemonBerryQueuedStatChange changes[7]{};
};
inline bool appendPokemonBerryStatRequest(PokemonBerryStatQueue& queue, const PokemonBerryEffectPlan& plan) {
    const auto* profile = pokemonBerryEffectProfile(plan.berryType);
    if (!profile || !profile->resolved || (std::strcmp(profile->effect, "STAT") &&
        std::strcmp(profile->effect, "RANDOM_STAT")) || plan.stat < 1 || plan.stat > 7 ||
        plan.stagesRequested < 1 || plan.stagesRequested > 42 || queue.count > 7 ||
        (queue.count && queue.ownerPokemonId != plan.ownerPokemonId)) return false;
    if (!std::strcmp(profile->effect, "STAT") && profile->stat != plan.stat) return false;
    auto next = queue;
    uint8_t found = next.count;
    for (uint8_t i = 0; i < next.count; ++i) {
        if (next.changes[i].stat < 1 || next.changes[i].stat > 7 ||
            next.changes[i].stages < 1 || next.changes[i].stages > 42) return false;
        for (uint8_t j = 0; j < i; ++j) if (next.changes[j].stat == next.changes[i].stat) return false;
        if (next.changes[i].stat == plan.stat) found = i;
    }
    if (found == next.count) {
        if (next.count == 7) return false;
        next.changes[next.count++] = {plan.stat, plan.stagesRequested};
    } else {
        const int combined = next.changes[found].stages + plan.stagesRequested;
        // Existing stat phase owns this explicit bound; never truncate requests.
        if (combined > 42) return false;
        next.changes[found].stages = static_cast<int16_t>(combined);
    }
    next.ownerPokemonId = plan.ownerPokemonId;
    queue = next;
    return true;
}
struct PokemonBerryStatPhaseEvent {
    uint8_t count = 0;
    PokemonStatStageEffectEvent changes[7]{};
};
// Callback dispatch follows the aggregate event, once the queued phase ends.
// This mutation adapter uses the existing stat phase; it is not a new engine.
inline PokemonStatStageEffectResult applyPokemonBerryStatQueue(PokemonBattleState& actor,
    const PokemonBerryStatQueue& queue, const PokemonStatStageEffectPolicy& policy,
    PokemonBerryStatPhaseEvent& output) {
    if (!policy.resolved) return PokemonStatStageEffectResult::UnresolvedPolicy;
    if (queue.count > 7 || (queue.count && queue.ownerPokemonId != actor.pokemonId))
        return PokemonStatStageEffectResult::InvalidState;
    auto next = actor;
    PokemonBerryStatPhaseEvent event{};
    for (uint8_t i = 0; i < queue.count; ++i) {
        const auto& change = queue.changes[i];
        if (change.stat < 1 || change.stat > 7 || change.stages < 1 || change.stages > 42)
            return PokemonStatStageEffectResult::InvalidDefinition;
        for (uint8_t j = 0; j < i; ++j) if (queue.changes[j].stat == change.stat)
            return PokemonStatStageEffectResult::InvalidDefinition;
        const auto result = applyResolvedPokemonStatStagePhase(next, static_cast<uint8_t>(1u << (change.stat - 1)),
            change.stages, policy, event.changes[i]);
        if (result != PokemonStatStageEffectResult::Ok) return result;
    }
    event.count = queue.count;
    actor = next;
    output = event;
    return PokemonStatStageEffectResult::Ok;
}

struct PokemonBerryRecoveryPolicy {
    PokemonHealingPolicy healing{};
    bool statusReactionsResolved = false;
    bool hasNightmare = false;
    bool nightmareLapseResolved = false;
};
struct PokemonBerryRecoveryEvent {
    PokemonHealingEvent healing{};
    PokemonStatusCureEvent status{};
    int8_t ppSlot = -1;
    uint8_t ppBefore = 0;
    uint8_t ppAfter = 0;
};
// HP recovery executes in its queued phase. Lum resets status/confusion and
// Leppa restores PP immediately while scanning modifiers.
// Stat requests go to applyResolvedPokemonStatStagePhase after aggregation;
// critical tags go to their own summon-tag lifecycle/codec.
inline PokemonBerryEffectResult applyPokemonBerryRecovery(PokemonBattleState& actor,
    const PokemonBerryEffectPlan& plan, const PokemonBerryRecoveryPolicy& policy,
    PokemonBerryRecoveryEvent& output) {
    const auto* profile = pokemonBerryEffectProfile(plan.berryType);
    if (!profile || !profile->resolved) return PokemonBerryEffectResult::Unsupported;
    if (plan.ownerPokemonId != actor.pokemonId || !actor.maxHp || actor.hp > actor.maxHp || actor.moveCount > 4 ||
        !pokemonStatusStateValid(actor.status) || !validPokemonConfusionTag(actor.confusion))
        return PokemonBerryEffectResult::InvalidState;
    if (!actor.hp) return PokemonBerryEffectResult::NoEffect;
    auto next = actor;
    PokemonBerryRecoveryEvent event{};
    if (!std::strcmp(profile->effect, "HEAL")) {
        if (!policy.healing.resolved) return PokemonBerryEffectResult::UnresolvedPolicy;
        if (!plan.healingRequested || plan.stat || plan.stagesRequested || plan.cureStatus ||
            plan.cureConfusion || plan.addCriticalBoost || plan.ppSlot != -1 ||
            !std::isfinite(policy.healing.healingMultiplier) || policy.healing.healingMultiplier <= 0)
            return PokemonBerryEffectResult::InvalidState;
        const double amount = std::floor(plan.healingRequested * policy.healing.healingMultiplier);
        if (!std::isfinite(amount) || amount < 0) return PokemonBerryEffectResult::InvalidState;
        event.healing.hpBefore = event.healing.hpAfter = actor.hp;
        event.healing.blocked = policy.healing.healBlocked;
        event.healing.failedFullHp = !event.healing.blocked && actor.hp == actor.maxHp;
        if (!event.healing.failedFullHp && !event.healing.blocked) {
            const uint16_t missing = actor.maxHp - actor.hp;
            event.healing.healed = amount >= missing ? missing : static_cast<uint16_t>(amount);
            next.hp += event.healing.healed;
            event.healing.hpAfter = next.hp;
            event.healing.showAnimation = event.healing.healed != 0;
        }
    } else if (!std::strcmp(profile->effect, "CURE_STATUS")) {
        if (!policy.statusReactionsResolved || (policy.hasNightmare && !policy.nightmareLapseResolved))
            return PokemonBerryEffectResult::UnresolvedPolicy;
        if (!plan.cureStatus || !plan.cureConfusion || plan.healingRequested || plan.stat ||
            plan.stagesRequested || plan.addCriticalBoost || plan.ppSlot != -1)
            return PokemonBerryEffectResult::InvalidState;
        const auto cured = curePokemonStatusState(next.status, true, true, false, policy.hasNightmare,
            next.confusion.present, true, event.status);
        if (cured != PokemonStatusCureResult::Cleared && cured != PokemonStatusCureResult::NoEffect)
            return PokemonBerryEffectResult::InvalidState;
        if (event.status.lapseConfusion) next.confusion = {};
    } else if (!std::strcmp(profile->effect, "RESTORE_PP")) {
        if (plan.ppSlot < 0 || plan.ppSlot >= actor.moveCount || plan.healingRequested || plan.stat ||
            plan.stagesRequested || plan.cureStatus || plan.cureConfusion || plan.addCriticalBoost)
            return PokemonBerryEffectResult::InvalidState;
        auto& move = next.moves[plan.ppSlot];
        if (!move.maxPp || move.pp > move.maxPp) return PokemonBerryEffectResult::InvalidState;
        // Each Leppa modifies PP immediately in its effect function, unlike
        // heal/status/stat requests. The phase caller must preserve that order.
        const uint32_t restored = move.pp + profile->amount;
        const uint8_t after = restored > move.maxPp ? move.maxPp : static_cast<uint8_t>(restored);
        if (plan.ppAfter != after) return PokemonBerryEffectResult::InvalidState;
        event.ppSlot = plan.ppSlot;
        event.ppBefore = move.pp;
        event.ppAfter = move.pp = after;
    } else return PokemonBerryEffectResult::Unsupported;
    actor = next;
    output = event;
    return PokemonBerryEffectResult::Ready;
}

inline PokemonBerryEffectResult applyPokemonBerryCriticalTag(PokemonBattleState& actor,
    const PokemonBerryEffectPlan& plan, bool tagCallbacksResolved) {
    if (!tagCallbacksResolved) return PokemonBerryEffectResult::UnresolvedPolicy;
    const auto* profile = pokemonBerryEffectProfile(plan.berryType);
    if (!profile || !profile->resolved || std::strcmp(profile->effect, "CRIT_BOOST"))
        return PokemonBerryEffectResult::Unsupported;
    if (actor.pokemonId != plan.ownerPokemonId || actor.berryCriticalBoostStages > 2 ||
        !plan.addCriticalBoost || plan.healingRequested || plan.stat || plan.stagesRequested ||
        plan.cureStatus || plan.cureConfusion || plan.ppSlot != -1)
        return PokemonBerryEffectResult::InvalidState;
    if (!actor.hp || actor.berryCriticalBoostStages) return PokemonBerryEffectResult::NoEffect;
    if (!PokerogueContent::kBerryCriticalBoostStages || PokerogueContent::kBerryCriticalBoostStages > 2)
        return PokemonBerryEffectResult::Unsupported;
    actor.berryCriticalBoostStages = PokerogueContent::kBerryCriticalBoostStages;
    return PokemonBerryEffectResult::Ready;
}

// Caller-owned pending HP phases plus the one aggregated stat phase per actor.
// Storage bounds are explicit; HP remains unchanged during the modifier scan.
struct PokemonBerryAbilityHealPlan {
    uint32_t ownerPokemonId = 0;
    uint16_t abilityId = 0;
    uint32_t healingRequested = 0;
};
struct PokemonBerryPhaseRequests {
    uint32_t ownerPokemonId = 0;
    PokemonBerryEffectPlan* recoveryPlans = nullptr;
    size_t recoveryCount = 0;
    size_t recoveryCapacity = 0;
    PokemonBerryStatQueue statChanges{};
    PokemonBerryAbilityHealPlan* abilityHealPlans = nullptr;
    size_t abilityHealCount = 0;
    size_t abilityHealCapacity = 0;
};

inline bool validPokemonBerryPhaseRequests(const PokemonBerryPhaseRequests& queue) {
    if (queue.recoveryCount > queue.recoveryCapacity || (queue.recoveryCapacity && !queue.recoveryPlans) ||
        queue.statChanges.count > 7 || (queue.statChanges.count && queue.statChanges.ownerPokemonId != queue.ownerPokemonId))
        return false;
    for (size_t i = 0; i < queue.recoveryCount; ++i) {
        const auto& plan = queue.recoveryPlans[i];
        const auto* profile = pokemonBerryEffectProfile(plan.berryType);
        if (!profile || !profile->resolved || std::strcmp(profile->effect, "HEAL") ||
            plan.ownerPokemonId != queue.ownerPokemonId || !plan.healingRequested || plan.stat ||
            plan.stagesRequested || plan.cureStatus || plan.cureConfusion || plan.addCriticalBoost || plan.ppSlot != -1)
            return false;
    }
    for (size_t i = 0; i < queue.statChanges.count; ++i) {
        const auto& change = queue.statChanges.changes[i];
        if (change.stat < 1 || change.stat > 7 || change.stages < 1 || change.stages > 42) return false;
        for (size_t j = 0; j < i; ++j) if (queue.statChanges.changes[j].stat == change.stat) return false;
    }
    if (queue.abilityHealCount > queue.abilityHealCapacity ||
        (queue.abilityHealCapacity && !queue.abilityHealPlans)) return false;
    for (size_t i = 0; i < queue.abilityHealCount; ++i) {
        const auto& plan = queue.abilityHealPlans[i];
        const auto* profile = pokemonBerryAbilityProfile(plan.abilityId);
        if (!profile || !profile->resolved || profile->healFraction <= 0 ||
            plan.ownerPokemonId != queue.ownerPokemonId || !plan.healingRequested) return false;
        for (size_t j = 0; j < i; ++j)
            if (queue.abilityHealPlans[j].abilityId == plan.abilityId) return false;
    }
    return true;
}

// HealFromBerryUseAbAttr is called once after the modifier scan, including
// preserved berries. Cud Chew's repeated effects never call this adapter.
inline PokemonBerryEffectResult queuePokemonBerryUseAbilityHealing(const PokemonBattleState& actor,
    const uint16_t* abilityIds, size_t abilityCount, bool applicabilityResolved, bool ateThisRound,
    PokemonBerryPhaseRequests& requests) {
    if (!ateThisRound) return PokemonBerryEffectResult::NoEffect;
    if (!applicabilityResolved) return PokemonBerryEffectResult::UnresolvedPolicy;
    if ((abilityCount && !abilityIds) || !actor.maxHp || actor.hp > actor.maxHp ||
        requests.ownerPokemonId != actor.pokemonId || !validPokemonBerryPhaseRequests(requests))
        return PokemonBerryEffectResult::InvalidState;
    size_t required = 0;
    for (size_t i = 0; i < abilityCount; ++i) {
        const auto* profile = pokemonBerryAbilityProfile(abilityIds[i]);
        if (!profile || !profile->resolved) return PokemonBerryEffectResult::UnresolvedPolicy;
        for (size_t j = 0; j < i; ++j)
            if (abilityIds[j] == abilityIds[i]) return PokemonBerryEffectResult::InvalidState;
        if (profile->healFraction <= 0) continue;
        for (size_t j = 0; j < requests.abilityHealCount; ++j)
            if (requests.abilityHealPlans[j].abilityId == abilityIds[i]) return PokemonBerryEffectResult::InvalidState;
        if (!std::isfinite(profile->healFraction) || profile->healFraction > 1) return PokemonBerryEffectResult::InvalidState;
        ++required;
    }
    if (required > requests.abilityHealCapacity - requests.abilityHealCount)
        return PokemonBerryEffectResult::PhaseCapacityExceeded;
    for (size_t i = 0; i < abilityCount; ++i) {
        const auto* profile = pokemonBerryAbilityProfile(abilityIds[i]);
        if (profile->healFraction <= 0) continue;
        // Pinned utils/common.ts: toDmgValue = max(floor(value), 1).
        const auto amount = static_cast<uint32_t>(std::fmax(std::floor(actor.maxHp * profile->healFraction), static_cast<double>(PokerogueContent::kBerryHealingMinimum)));
        requests.abilityHealPlans[requests.abilityHealCount++] = {actor.pokemonId, abilityIds[i], amount};
    }
    return required ? PokemonBerryEffectResult::Ready : PokemonBerryEffectResult::NoEffect;
}

inline PokemonBerryEffectResult applyPokemonBerryAbilityHealing(PokemonBattleState& actor,
    const PokemonBerryAbilityHealPlan& plan, const PokemonHealingPolicy& policy, PokemonHealingEvent& output) {
    if (!policy.resolved) return PokemonBerryEffectResult::UnresolvedPolicy;
    const auto* profile = pokemonBerryAbilityProfile(plan.abilityId);
    if (!profile || !profile->resolved || profile->healFraction <= 0) return PokemonBerryEffectResult::Unsupported;
    if (plan.ownerPokemonId != actor.pokemonId || !plan.healingRequested || !actor.maxHp || actor.hp > actor.maxHp ||
        !std::isfinite(policy.healingMultiplier) || policy.healingMultiplier <= 0) return PokemonBerryEffectResult::InvalidState;
    PokemonHealingEvent event{};
    event.hpBefore = event.hpAfter = actor.hp;
    if (actor.hp) {
        // PokemonHealPhase checks Heal Block before the full-HP message.
        event.blocked = policy.healBlocked;
        event.failedFullHp = !event.blocked && actor.hp == actor.maxHp;
        if (!event.blocked && !event.failedFullHp) {
            const double amount = std::floor(plan.healingRequested * policy.healingMultiplier);
            if (!std::isfinite(amount) || amount < 0) return PokemonBerryEffectResult::InvalidState;
            const uint16_t missing = actor.maxHp - actor.hp;
            event.healed = amount >= missing ? missing : static_cast<uint16_t>(amount);
            event.hpAfter += event.healed;
            event.showAnimation = event.healed != 0;
        }
    }
    actor.hp = event.hpAfter;
    output = event;
    return PokemonBerryEffectResult::Ready;
}

struct PokemonHeldBerryUseEvent {
    PokemonBerryEffectPlan effect{};
    PokemonBerryConsumedEvent item{};
    bool effectExecutedImmediately = false; // Lum/Leppa/critical tag, unlike queued HP/stat.
};
// Bridge the modifier-list entry to deferred effect phases. Eligibility, pouch
// RNG, effect RNG, callbacks and stack publication form one transaction.
inline PokemonBerryEffectResult preparePokemonHeldBerryUse(NativeHeldModifierInstance* records,
    size_t capacity, size_t& count, size_t index, PokemonBattleState& holder,
    const PokemonBerryEffectPolicy& effectPolicy, bool opponentBlockResolved, bool opponentBlocksUse,
    const uint16_t* pouchStacks, size_t pouchCount, const uint16_t* activeAbilityIds, size_t abilityCount,
    bool applicabilityResolved, PokerogueBattleRng& battleRng, PokerogueRngAdapter& globalRng,
    PokemonHeldBerryUseEvent& output, const PokemonBerryRecoveryPolicy* statusPolicy = nullptr) {
    if (!opponentBlockResolved) return PokemonBerryEffectResult::UnresolvedPolicy;
    if (!records || count > capacity || index >= count) return PokemonBerryEffectResult::InvalidState;
    uint16_t type = 0;
    if (!heldBerryType(records[index], type) || records[index].ownerPokemonId != holder.pokemonId)
        return PokemonBerryEffectResult::InvalidState;
    auto nextGlobal = globalRng;
    auto nextBattle = battleRng;
    PokemonHeldBerryUseEvent event{};
    const auto result = planPokemonBerryEffect(type, holder, effectPolicy, nextGlobal, event.effect);
    if (result != PokemonBerryEffectResult::Ready) return result;
    if (opponentBlocksUse) return PokemonBerryEffectResult::NoEffect;
    bool preserved = false;
    if (!resolvePokemonBerryPreservation(pouchStacks, pouchCount, nextBattle, preserved))
        return PokemonBerryEffectResult::UnresolvedPolicy;
    auto nextHolder = holder;
    if (event.effect.cureStatus || event.effect.cureConfusion) {
        if (!statusPolicy) return PokemonBerryEffectResult::UnresolvedPolicy;
        PokemonBerryRecoveryEvent recovered{};
        const auto applied = applyPokemonBerryRecovery(nextHolder, event.effect, *statusPolicy, recovered);
        if (applied != PokemonBerryEffectResult::Ready) return applied;
        event.effectExecutedImmediately = true;
    }
    if (event.effect.ppSlot >= 0) {
        PokemonBerryRecoveryPolicy recovery{};
        PokemonBerryRecoveryEvent recovered{};
        const auto applied = applyPokemonBerryRecovery(nextHolder, event.effect, recovery, recovered);
        if (applied != PokemonBerryEffectResult::Ready) return applied;
        event.effectExecutedImmediately = true;
    }
    if (event.effect.addCriticalBoost) {
        const auto applied = applyPokemonBerryCriticalTag(nextHolder, event.effect, effectPolicy.criticalTagResolved);
        if (applied != PokemonBerryEffectResult::Ready) return applied;
        event.effectExecutedImmediately = true;
    }
    if (!consumePokemonHeldBerry(records, capacity, count, index, nextHolder, true, preserved,
            activeAbilityIds, abilityCount, applicabilityResolved, event.item))
        return PokemonBerryEffectResult::UnresolvedPolicy;
    holder = nextHolder;
    battleRng = nextBattle;
    globalRng = nextGlobal;
    output = event;
    return PokemonBerryEffectResult::Ready;
}
// Publish one modifier's use and its ordered history together. Staging a
// single list entry avoids copying the whole inventory or losing a consumed
// record if history storage is exhausted. The run owner may grow that storage.
inline PokemonBerryEffectResult usePokemonHeldBerryAndRecord(NativeHeldModifierInstance* records,
    size_t capacity, size_t& count, size_t index, PokemonBattleState& holder, PokemonBerryHistoryView& history,
    const PokemonBerryEffectPolicy& policy, bool opponentBlockResolved, bool opponentBlocksUse,
    const uint16_t* pouchStacks, size_t pouchCount, const uint16_t* abilityIds, size_t abilityCount,
    bool applicabilityResolved, PokerogueBattleRng& battleRng, PokerogueRngAdapter& globalRng,
    PokemonHeldBerryUseEvent& output, const PokemonBerryRecoveryPolicy* statusPolicy = nullptr,
    PokemonBerryPhaseRequests* deferred = nullptr) {
    if (!records || count > capacity || index >= count || history.ownerPokemonId != holder.pokemonId)
        return PokemonBerryEffectResult::InvalidState;
    auto stagedRecord = records[index];
    size_t stagedCount = 1;
    auto nextHolder = holder;
    auto nextBattle = battleRng;
    auto nextGlobal = globalRng;
    PokemonHeldBerryUseEvent event{};
    const auto result = preparePokemonHeldBerryUse(&stagedRecord, 1, stagedCount, 0, nextHolder, policy,
        opponentBlockResolved, opponentBlocksUse, pouchStacks, pouchCount, abilityIds, abilityCount,
        applicabilityResolved, nextBattle, nextGlobal, event, statusPolicy);
    if (result != PokemonBerryEffectResult::Ready) return result;
    PokemonBerryStatQueue nextStats{};
    bool queueRecovery = false;
    if (deferred) {
        if (deferred->ownerPokemonId != holder.pokemonId || !validPokemonBerryPhaseRequests(*deferred))
            return PokemonBerryEffectResult::InvalidState;
        nextStats = deferred->statChanges;
        if (event.effect.stat) {
            if (!appendPokemonBerryStatRequest(nextStats, event.effect)) return PokemonBerryEffectResult::PhaseCapacityExceeded;
        } else if (!event.effectExecutedImmediately) {
            if (!event.effect.healingRequested) return PokemonBerryEffectResult::Unsupported;
            if (deferred->recoveryCount == deferred->recoveryCapacity) return PokemonBerryEffectResult::PhaseCapacityExceeded;
            queueRecovery = true;
        }
    }
    const auto recorded = recordPokemonBerryHistory(history, event.item);
    if (recorded == PokemonBerryHistoryResult::CapacityExceeded) return PokemonBerryEffectResult::HistoryCapacityExceeded;
    if (recorded != PokemonBerryHistoryResult::Recorded) return PokemonBerryEffectResult::InvalidState;
    if (stagedCount) records[index] = stagedRecord;
    else {
        for (size_t i = index + 1; i < count; ++i) records[i - 1] = records[i];
        records[--count] = {};
    }
    if (deferred) {
        deferred->statChanges = nextStats;
        if (queueRecovery) deferred->recoveryPlans[deferred->recoveryCount++] = event.effect;
    }
    holder = nextHolder;
    battleRng = nextBattle;
    globalRng = nextGlobal;
    output = event;
    return PokemonBerryEffectResult::Ready;
}
struct PokemonBerryModifierScanEvent {
    size_t used = 0;
    bool blockedByOpponent = false;
};

// BerryPhase.eatBerries modifier-list portion. The enclosing engine transaction
// owns rollback if a later phase/callback fails. Requests stay deferred until
// every field actor has been scanned; immediate effects alter later predicates.
inline PokemonBerryEffectResult scanPokemonHeldBerryModifiersInPlace(
    NativeHeldModifierInstance* records, size_t capacity, size_t& count,
    PokemonBattleState& holder, PokemonBerryHistoryView& history, PokemonBerryPhaseRequests& requests,
    const PokemonBerryEffectPolicy& context, const uint16_t* abilityIds, size_t abilityCount,
    bool applicabilityResolved, bool opponentBlockResolved, bool opponentBlocksUse,
    const uint16_t* pouchStacks, size_t pouchCount, const PokemonBerryRecoveryPolicy& statusPolicy,
    PokerogueBattleRng& battleRng, PokerogueRngAdapter& globalRng, PokemonBerryModifierScanEvent& output) {
    if ((capacity && !records) || count > capacity || history.ownerPokemonId != holder.pokemonId ||
        requests.ownerPokemonId != holder.pokemonId || !validPokemonBerryPhaseRequests(requests))
        return PokemonBerryEffectResult::InvalidState;
    bool usable = false;
    for (size_t i = 0; i < count; ++i) {
        if (!validateHeldModifierInstance(records[i])) return PokemonBerryEffectResult::InvalidState;
        const auto* definition = heldModifierDefinition(records[i]);
        if (!definition) return PokemonBerryEffectResult::Unsupported;
        if (records[i].ownerPokemonId != holder.pokemonId || std::strcmp(definition->id, "BERRY")) continue;
        uint16_t type = 0;
        if (!heldBerryType(records[i], type)) return PokemonBerryEffectResult::InvalidState;
        auto policy = context;
        const auto resolved = resolvePokemonBerryAbilityEffects(type, holder, abilityIds, abilityCount,
            applicabilityResolved, policy);
        if (resolved != PokemonBerryEffectResult::Ready) return resolved;
        auto previewRng = globalRng; // Predicate inspection must not draw the effect RNG.
        PokemonBerryEffectPlan preview{};
        const auto result = planPokemonBerryEffect(type, holder, policy, previewRng, preview);
        if (result == PokemonBerryEffectResult::Ready) { usable = true; break; }
        if (result != PokemonBerryEffectResult::NoEffect) return result;
    }
    PokemonBerryModifierScanEvent event{};
    if (!usable) { output = event; return PokemonBerryEffectResult::NoEffect; }
    if (!opponentBlockResolved) return PokemonBerryEffectResult::UnresolvedPolicy;
    if (opponentBlocksUse) {
        event.blockedByOpponent = true;
        output = event;
        return PokemonBerryEffectResult::NoEffect;
    }
    for (size_t i = 0; i < count;) {
        const auto* definition = heldModifierDefinition(records[i]);
        if (!definition) return PokemonBerryEffectResult::InvalidState;
        if (records[i].ownerPokemonId != holder.pokemonId || std::strcmp(definition->id, "BERRY")) { ++i; continue; }
        uint16_t type = 0;
        if (!heldBerryType(records[i], type)) return PokemonBerryEffectResult::InvalidState;
        auto policy = context;
        const auto resolved = resolvePokemonBerryAbilityEffects(type, holder, abilityIds, abilityCount,
            applicabilityResolved, policy);
        if (resolved != PokemonBerryEffectResult::Ready) return resolved;
        const size_t beforeCount = count;
        PokemonHeldBerryUseEvent used{};
        const auto result = usePokemonHeldBerryAndRecord(records, capacity, count, i, holder, history, policy,
            true, false, pouchStacks, pouchCount, abilityIds, abilityCount, applicabilityResolved,
            battleRng, globalRng, used, &statusPolicy, &requests);
        if (result == PokemonBerryEffectResult::Ready) ++event.used;
        else if (result != PokemonBerryEffectResult::NoEffect) return result;
        // A removed record exposes the next one at this index. A remaining
        // stack is visited only once, including a Berry Pouch-preserved use.
        if (count == beforeCount) ++i;
    }
    const auto healed = queuePokemonBerryUseAbilityHealing(holder, abilityIds, abilityCount,
        applicabilityResolved, event.used != 0, requests);
    if (healed != PokemonBerryEffectResult::Ready && healed != PokemonBerryEffectResult::NoEffect) return healed;
    output = event;
    return event.used ? PokemonBerryEffectResult::Ready : PokemonBerryEffectResult::NoEffect;
}

} // namespace Pokerogue3DS

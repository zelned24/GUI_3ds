#pragma once

#include "content/PokerogueRuntimeContent.hpp"
#include <cstdint>
#include <cstddef>

namespace Pokerogue3DS {

class PokerogueRngAdapter;

// Pinned FaintPhase.start: GameOver takes precedence when no legal player
// remains. Enemy-field defeat permits VictoryPhase even if the active player
// fainted and a legal reserve still exists. Trainer reserves are handled later.
enum class PokemonBattleConclusion : uint8_t { Continue, PlayerVictory, PlayerDefeat };
inline PokemonBattleConclusion pokemonBattleConclusion(bool playerPartyDefeated, bool enemyFieldDefeated) {
    if (playerPartyDefeated) return PokemonBattleConclusion::PlayerDefeat;
    return enemyFieldDefeated ? PokemonBattleConclusion::PlayerVictory : PokemonBattleConclusion::Continue;
}

enum class PokemonGender : uint8_t { Unspecified = 0, Genderless, Male, Female };
enum class PokemonNature : uint8_t {
    Hardy = 0, Lonely, Brave, Adamant, Naughty,
    Bold, Docile, Relaxed, Impish, Lax,
    Timid, Hasty, Serious, Jolly, Naive,
    Modest, Mild, Quiet, Bashful, Rash,
    Calm, Gentle, Sassy, Careful, Quirky,
    Unspecified = 255,
};

struct PokemonNatureModifiers {
    int8_t raisedStat = -1; // HP=0 is never modified; ATK..SPD are 1..5.
    int8_t loweredStat = -1;
};

// Returns stable catalog-independent storage for a valid upstream type symbol.
const char* resolvePokemonTypeSymbol(const char* type);
bool getPokemonNatureModifiers(PokemonNature nature, PokemonNatureModifiers& output);
PokemonNature selectPokemonNature(PokerogueRngAdapter& rng);

struct PokemonActorIdentity {
    uint32_t pokemonId = 0;
    uint8_t ivs[6]{};
    uint8_t abilityIndex = 0;
    PokemonGender gender = PokemonGender::Unspecified;
    PokemonNature nature = PokemonNature::Unspecified;
    const char* formId = nullptr;
    const char* initialTeraType = nullptr; // Concrete constructor choice survives evolution.
    uint8_t initialTeraTypeIndex = 0;
    bool initialTeraTypeResolved = false;
};

struct PokemonFormSelectionContext {
    const char* biomeId = nullptr;
    const char* timeOfDay = nullptr;
    const char* trainerSpecialtyType = nullptr;
    PokemonNature nature = PokemonNature::Unspecified;
    uint16_t waveIndex = 0;
    bool trainerBattle = false;
    bool hasMysteryEncounters = false;
    bool eggPhase = false;
    bool ignoreArena = false;
};

enum class PokemonFormSelectionResult : uint8_t {
    Ok = 0, MissingSpecies, MissingForm
};

// Mirrors pinned BattleScene.getSpeciesFormIndex and stores the selected
// imported form ID. Call after actor identity/gender and before shiny/nature.
PokemonFormSelectionResult selectPokemonActorForm(
    uint16_t speciesDex,
    const PokemonFormSelectionContext& context,
    PokerogueRngAdapter& rng,
    PokemonActorIdentity& actor);

// Call after upstream form and shiny steps, which precede nature generation.
void generatePokemonActorNature(PokemonActorIdentity& actor, PokerogueRngAdapter& rng);

enum class PokemonActorIdentityResult : uint8_t {
    Ok = 0, MissingSpecies, InvalidHiddenRate, InvalidGenderRatio, MissingForm, InvalidTypes
};

// Mirrors the pinned Pokemon constructor's pre-form identity draws:
// ability index, 32-bit ID/IVs, then gender.
PokemonActorIdentityResult generatePokemonActorIdentity(
    uint16_t speciesDex,
    uint16_t hiddenAbilityRate,
    PokerogueRngAdapter& rng,
    PokemonActorIdentity& output);

// Runs identity draws and pinned form selection in constructor order, publishing
// the actor value only after both stages succeed. Shiny/variant and nature follow.
PokemonActorIdentityResult generatePokemonActorIdentityAndForm(
    uint16_t speciesDex,
    uint16_t hiddenAbilityRate,
    const PokemonFormSelectionContext& formContext,
    PokerogueRngAdapter& rng,
    PokemonActorIdentity& output);

// Generates the constructor fields and RNG draws used for a new, non-fused
// wild actor in a normal Classic encounter: identity/form, nature, then its
// initial tera-type pick. The caller must account for any upstream modifiers
// that can fuse/otherwise alter an actor before using this helper.
PokemonActorIdentityResult generatePokemonActorForWildEncounter(
    uint16_t speciesDex,
    uint16_t hiddenAbilityRate,
    const PokemonFormSelectionContext& formContext,
    PokerogueRngAdapter& rng,
    PokemonActorIdentity& output);

enum class PokemonBattleInitResult : uint8_t {
    Ok = 0,
    MissingSpecies,
    InvalidLevel,
    InvalidIv,
    InvalidNatureStat,
    InvalidAbility,
    InvalidForm,
    InvalidGender,
    InvalidMoveCount,
    MissingMove,
    InvalidMovePp,
    InvalidStatRange, // Current storage cannot represent this stat; never wrap it.
};

struct BattleMoveState {
    uint16_t moveId = 0;
    uint8_t pp = 0;
    uint8_t maxPp = 0;
};

// Supported permanent PP: pinned PokemonMove.getMovePp with zero to three PP Ups.
// Transform overrides and negative boss ppUp require separate explicit metadata.
inline bool pokemonPermanentMaxPpSupported(uint16_t moveId, uint8_t maximum) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move || move->pp < 1 || move->pp > 255) return false;
    const uint16_t increment = move->pp / 5 > 0 ? move->pp / 5 : 1;
    for (uint8_t boosts = 0; boosts <= 3; ++boosts)
        if (move->pp + boosts * increment == maximum) return true;
    return false;
}

// Caller supplies all RNG-derived state. This avoids fabricated IV/nature/move
// defaults until the pinned Pokemon generation sequence is integrated.
struct PokemonBattleInit {
    uint16_t speciesDex = 0;
    const char* formId = nullptr; // Null selects the imported base form.
    uint16_t level = 0;
    uint32_t pokemonId = 0;
    bool deriveIvsFromPokemonId = false;
    uint8_t ivs[6]{}; // Upstream permanent-stat order: HP, ATK, DEF, SPATK, SPDEF, SPD.
    PokemonNature nature = PokemonNature::Unspecified;
    int8_t natureRaisedStat = -1;
    int8_t natureLoweredStat = -1;
    uint16_t abilityId = 0;
    PokemonGender gender = PokemonGender::Unspecified;
    uint8_t moveCount = 0;
    uint16_t moveIds[4]{};
};

struct HeldItemLostTagState { bool unburden = false; };

// Pinned src/enums/status-effect.ts IDs. FAINT is preserved upstream metadata.
enum class PokemonStatusEffect : uint8_t {
    None = 0, Poison = 1, Toxic = 2, Paralysis = 3, Sleep = 4, Freeze = 5, Burn = 6, Faint = 7
};
struct PokemonStatusState {
    PokemonStatusEffect effect = PokemonStatusEffect::None;
    uint32_t toxicTurnCount = 0;
    uint32_t sleepTurnsRemaining = 0;
    uint32_t freezeTurnsRemaining = 0;
    bool present = false;
    bool hasSleepTurnsRemaining = false;
    bool hasFreezeTurnsRemaining = false;
};
inline bool pokemonStatusStateValid(const PokemonStatusState& status) {
    if (static_cast<uint8_t>(status.effect) > 7) return false;
    if (!status.hasSleepTurnsRemaining && status.sleepTurnsRemaining) return false;
    if (!status.hasFreezeTurnsRemaining && status.freezeTurnsRemaining) return false;
    return status.present || (status.effect == PokemonStatusEffect::None && !status.toxicTurnCount &&
        !status.hasSleepTurnsRemaining && !status.hasFreezeTurnsRemaining);
}
enum class PokemonStatusTickResult : uint8_t { Ok, NoStatus, InvalidStatus, CounterOverflow };
// Status.incrementTurn increments toxicTurnCount for every existing Status,
// and decrements optional sleep/freeze counters only when truthy. No RNG.
inline PokemonStatusTickResult incrementPokemonStatusTurn(PokemonStatusState& status) {
    if (!pokemonStatusStateValid(status)) return PokemonStatusTickResult::InvalidStatus;
    if (!status.present) return PokemonStatusTickResult::NoStatus;
    if (status.toxicTurnCount == UINT32_MAX) return PokemonStatusTickResult::CounterOverflow;
    auto next = status;
    ++next.toxicTurnCount;
    if (next.hasSleepTurnsRemaining && next.sleepTurnsRemaining) --next.sleepTurnsRemaining;
    if (next.hasFreezeTurnsRemaining && next.freezeTurnsRemaining) --next.freezeTurnsRemaining;
    status = next;
    return PokemonStatusTickResult::Ok;
}
inline bool pokemonStatusIsPostTurn(const PokemonStatusState& status) {
    return status.present && (status.effect == PokemonStatusEffect::Poison ||
        status.effect == PokemonStatusEffect::Toxic || status.effect == PokemonStatusEffect::Burn);
}
inline double pokemonStatusCatchRateMultiplier(const PokemonStatusState& status) {
    if (!status.present) return 1.0;
    switch (status.effect) {
    case PokemonStatusEffect::Poison: case PokemonStatusEffect::Toxic:
    case PokemonStatusEffect::Paralysis: case PokemonStatusEffect::Burn: return 1.5;
    case PokemonStatusEffect::Sleep: case PokemonStatusEffect::Freeze: return 2.5;
    default: return 1.0;
    }
}

struct PokemonBattleState {
    uint16_t speciesDex = 0;
    const char* formId = nullptr;
    uint16_t level = 0;
    uint32_t pokemonId = 0;
    uint16_t abilityId = 0;
    uint8_t friendship = 0; // Persistent Pokemon friendship, initialized from pinned species.
    PokemonStatusState status{}; // Persistent nonvolatile status, stored in run v15.
    PokemonStatusEffect pendingStatus = PokemonStatusEffect::None; // PokemonTurnData; queue must drain before checkpoint.
    HeldItemLostTagState heldItemLostTags{}; // Transient summon data.
    uint32_t turnDamageDealt = 0; // PokemonTurnData.totalDamageDealt; reset after turn effects.
    PokemonGender gender = PokemonGender::Unspecified;
    uint16_t maxHp = 0;
    uint16_t hp = 0;
    uint8_t ivs[6]{};
    PokemonNature nature = PokemonNature::Unspecified;
    uint16_t stats[6]{}; // Upstream permanent-stat order.
    int8_t statStages[7]{}; // Upstream Stat.ATK..Stat.EVA, indexed stat - 1.
    uint8_t moveCount = 0;
    BattleMoveState moves[4]{};
    bool ivsWereDerivedFromPokemonId = false;
    bool pauseEvolutions = false; // Persistent Pokemon option, independent of battle stages.
    bool statsAreBaseFormulaOnly = true;
};

struct PokemonStatusApplicationPolicy {
    bool resolved = false; // Live types, grounding, field, abilities and Safeguard resolved.
    bool overrideStatus = false;
    bool pendingStatus = false;
    bool ignoreField = false;
    bool grounded = false;
    bool mistyTerrain = false;
    bool electricTerrain = false;
    bool sunnyOrHarshSun = false;
    bool poisonType = false;
    bool steelType = false;
    bool electricType = false;
    bool iceType = false;
    bool fireType = false;
    bool hasSource = false;
    bool sourceIsTarget = false;
    bool sourceIgnoresPoisonImmunity = false;
    bool sourceIgnoresSteelImmunity = false;
    bool selfAbilityBlocks = false;
    bool allyAbilityBlocks = false;
    bool safeguardBlocks = false;
};
enum class PokemonStatusImmunityResult : uint8_t { Resolved, UnknownAbility, UnsupportedCondition, InvalidEffect, InvalidType };
inline PokemonStatusImmunityResult resolvePokemonStatusAbilityImmunity(uint16_t abilityId,
    PokemonStatusEffect effect, bool abilityActive, bool allyField, bool callbacksResolved, bool& output) {
    if (static_cast<uint8_t>(effect) > 7) return PokemonStatusImmunityResult::InvalidEffect;
    if (!callbacksResolved) return PokemonStatusImmunityResult::UnsupportedCondition;
    for (const auto& profile : PokerogueContent::kStatusImmunityAbilityProfiles) {
        if (profile.abilityId != abilityId) continue;
        if (abilityActive && !(allyField ? profile.allyResolved : profile.selfResolved))
            return PokemonStatusImmunityResult::UnsupportedCondition;
        output = abilityActive && ((allyField ? profile.allyMask : profile.selfMask) &
            (1u << static_cast<uint8_t>(effect)));
        return PokemonStatusImmunityResult::Resolved;
    }
    return PokemonStatusImmunityResult::UnknownAbility;
}

inline PokemonStatusImmunityResult resolvePokemonStatusTypeImmunityBypass(uint16_t abilityId,
    PokemonStatusEffect effect, const char* defenderType, bool abilityActive, bool callbacksResolved, bool& output) {
    if (static_cast<uint8_t>(effect) > 7) return PokemonStatusImmunityResult::InvalidEffect;
    if (!resolvePokemonTypeSymbol(defenderType)) return PokemonStatusImmunityResult::InvalidType;
    if (!callbacksResolved) return PokemonStatusImmunityResult::UnsupportedCondition;
    for (const auto& profile : PokerogueContent::kStatusTypeBypassProfiles) {
        if (profile.abilityId != abilityId) continue;
        if (abilityActive && !profile.resolved) return PokemonStatusImmunityResult::UnsupportedCondition;
        bool bypass = false;
        if (abilityActive) for (const auto& entry : PokerogueContent::kStatusTypeBypassEntries) {
            if (entry.abilityId != abilityId || !(entry.statusMask & (1u << static_cast<uint8_t>(effect)))) continue;
            const char* a = entry.defenderType;
            const char* b = defenderType;
            while (*a && *a == *b) { ++a; ++b; }
            bypass |= *a == *b;
        }
        output = bypass;
        return PokemonStatusImmunityResult::Resolved;
    }
    return PokemonStatusImmunityResult::UnknownAbility;
}

struct PokemonStatusFieldContext {
    bool resolved = false; // Live types, grounding, weather, terrain and Safeguard.
    const char* const* effectiveTypes = nullptr; // isOfType default view.
    size_t effectiveTypeCount = 0;
    const char* const* originalIfStellarTypes = nullptr; // getTypes(returnOriginalTypesIfStellar=true).
    size_t originalIfStellarTypeCount = 0;
    bool grounded = false;
    bool mistyTerrain = false;
    bool electricTerrain = false;
    bool sunnyOrHarshSun = false;
    bool safeguardBlocks = false;
    bool ignoreField = false;
    bool overrideStatus = false;
};
bool resolvePokemonStatusApplicationEnvironment(const PokemonBattleState& recipient,
    const PokemonBattleState* source, const PokemonStatusFieldContext& field,
    PokemonStatusApplicationPolicy& output);

struct PokemonStatusAbilityComponent {
    uint16_t abilityId = 0;
    bool active = false;
    bool callbacksResolved = false;
};
// Environment policy supplies resolved live types/grounding/terrain/Safeguard.
// Component lists include primary/passive and live allies in upstream order.
inline PokemonStatusImmunityResult composePokemonStatusApplicationPolicy(PokemonStatusEffect effect,
    const PokemonStatusApplicationPolicy& environment,
    const PokemonStatusAbilityComponent* own, size_t ownCount,
    const PokemonStatusAbilityComponent* allies, size_t allyCount,
    const PokemonStatusAbilityComponent* source, size_t sourceCount,
    PokemonStatusApplicationPolicy& output) {
    if (static_cast<uint8_t>(effect) > 7) return PokemonStatusImmunityResult::InvalidEffect;
    if (!environment.resolved || !ownCount || !own || (allyCount && !allies) || (sourceCount && !source) ||
        (environment.hasSource ? !sourceCount : sourceCount != 0) ||
        (!environment.hasSource && environment.sourceIsTarget)) return PokemonStatusImmunityResult::UnsupportedCondition;
    auto policy = environment;
    policy.selfAbilityBlocks = policy.allyAbilityBlocks = false;
    policy.sourceIgnoresPoisonImmunity = policy.sourceIgnoresSteelImmunity = false;
    for (size_t i = 0; i < ownCount; ++i) {
        bool blocked = false;
        const auto result = resolvePokemonStatusAbilityImmunity(own[i].abilityId, effect,
            own[i].active, false, own[i].callbacksResolved, blocked);
        if (result != PokemonStatusImmunityResult::Resolved) return result;
        policy.selfAbilityBlocks |= blocked;
    }
    for (size_t i = 0; i < allyCount; ++i) {
        bool blocked = false;
        const auto result = resolvePokemonStatusAbilityImmunity(allies[i].abilityId, effect,
            allies[i].active, true, allies[i].callbacksResolved, blocked);
        if (result != PokemonStatusImmunityResult::Resolved) return result;
        policy.allyAbilityBlocks |= blocked;
    }
    if (effect == PokemonStatusEffect::Poison || effect == PokemonStatusEffect::Toxic) {
        for (size_t i = 0; i < sourceCount; ++i) {
            bool poison = false, steel = false;
            auto result = resolvePokemonStatusTypeImmunityBypass(source[i].abilityId, effect, "POISON",
                source[i].active, source[i].callbacksResolved, poison);
            if (result != PokemonStatusImmunityResult::Resolved) return result;
            result = resolvePokemonStatusTypeImmunityBypass(source[i].abilityId, effect, "STEEL",
                source[i].active, source[i].callbacksResolved, steel);
            if (result != PokemonStatusImmunityResult::Resolved) return result;
            policy.sourceIgnoresPoisonImmunity |= poison;
            policy.sourceIgnoresSteelImmunity |= steel;
        }
    }
    output = policy;
    return PokemonStatusImmunityResult::Resolved;
}

enum class PokemonStatusEligibility : uint8_t {
    Allowed, InvalidState, UnsupportedPolicy, ExistingStatus, PendingStatus, MistyTerrain,
    PoisonType, SteelType, ElectricType, ElectricTerrain, IceType, SunnyWeather,
    FireType, SelfAbility, AllyAbility, Safeguard, NoEffect
};
// Pokemon.canSetStatus predicate only. trySetStatus's faint check, queued
// ObtainStatusEffectPhase, duration draws and reactions are separate stages.
inline PokemonStatusEligibility canPokemonSetStatus(const PokemonStatusState& current,
    PokemonStatusEffect requested, const PokemonStatusApplicationPolicy& policy) {
    if (!pokemonStatusStateValid(current) || static_cast<uint8_t>(requested) > 7)
        return PokemonStatusEligibility::InvalidState;
    if (requested != PokemonStatusEffect::Faint) {
        if (policy.overrideStatus ? current.present && current.effect == requested : current.present)
            return PokemonStatusEligibility::ExistingStatus;
        if (!policy.overrideStatus && policy.pendingStatus) return PokemonStatusEligibility::PendingStatus;
    }
    if (!policy.resolved) return PokemonStatusEligibility::UnsupportedPolicy;
    if (requested != PokemonStatusEffect::Faint && policy.grounded && !policy.ignoreField && policy.mistyTerrain)
        return PokemonStatusEligibility::MistyTerrain;
    switch (requested) {
    case PokemonStatusEffect::Poison: case PokemonStatusEffect::Toxic:
        if (policy.poisonType && (!policy.hasSource || !policy.sourceIgnoresPoisonImmunity))
            return PokemonStatusEligibility::PoisonType;
        if (policy.steelType && (!policy.hasSource || !policy.sourceIgnoresSteelImmunity))
            return PokemonStatusEligibility::SteelType;
        break;
    case PokemonStatusEffect::Paralysis:
        if (policy.electricType) return PokemonStatusEligibility::ElectricType;
        break;
    case PokemonStatusEffect::Sleep:
        // Pinned sleep branch does not consult ignoreField.
        if (policy.grounded && policy.electricTerrain) return PokemonStatusEligibility::ElectricTerrain;
        break;
    case PokemonStatusEffect::Freeze:
        if (policy.iceType) return PokemonStatusEligibility::IceType;
        if (!policy.ignoreField && policy.sunnyOrHarshSun) return PokemonStatusEligibility::SunnyWeather;
        break;
    case PokemonStatusEffect::Burn:
        if (policy.fireType) return PokemonStatusEligibility::FireType;
        break;
    default: break;
    }
    if (policy.selfAbilityBlocks) return PokemonStatusEligibility::SelfAbility;
    if (policy.allyAbilityBlocks) return PokemonStatusEligibility::AllyAbility;
    if (policy.hasSource && !policy.sourceIsTarget && policy.safeguardBlocks)
        return PokemonStatusEligibility::Safeguard;
    return PokemonStatusEligibility::Allowed;
}

enum class PokemonStatusObtainResult : uint8_t { Applied, Ineligible, Fainted, UnsupportedReactions };
// Actor portion of doSetStatus. Caller resolves pending-status queue, tags,
// move cancellation and callbacks before setting reactionsResolved=true.
PokemonStatusObtainResult obtainPokemonStatus(PokemonBattleState& actor, PokemonStatusEffect effect,
    const PokemonStatusApplicationPolicy& policy, bool reactionsResolved, PokerogueRngAdapter& rng,
    bool explicitSleepDuration = false, uint32_t sleepDuration = 0);

enum class PokemonMoveStatusApplicationResult : uint8_t {
    Requested, ChanceFailed, Ineligible, Fainted, UnsupportedMove, UnresolvedPolicy, InvalidState
};
struct PokemonMoveStatusApplicationEvent {
    PokemonStatusEffect effect = PokemonStatusEffect::None;
    PokemonStatusEligibility eligibility = PokemonStatusEligibility::Allowed;
    bool selfTarget = false;
    bool quiet = false;
    bool chanceRolled = false;
    uint8_t chanceRoll = 0;
    bool requestObtainStatusPhase = false;
};
// StatusEffectAttr.apply only: caller has resolved hit and effective move chance.
// Queuing/ObtainStatusEffectPhase belongs to the subsequent dispatcher step.
PokemonMoveStatusApplicationResult resolvePokemonMoveStatusApplication(
    const PokemonBattleState& recipient, uint16_t moveId, int16_t effectiveChance,
    bool chanceCallbacksResolved, const PokemonStatusApplicationPolicy& policy,
    PokerogueRngAdapter& userRng, PokemonMoveStatusApplicationEvent& output);

struct PokemonStatusMoveHitPolicy {
    bool resolved = false;
    bool blockedBeforeAccuracy = false;
    bool bypassAccuracy = false;
    double accuracyMultiplier = 1.0;
};
struct PokemonStatusMoveHitEvent { bool hit = false; bool accuracyRolled = false; uint8_t accuracyRoll = 0; };
bool resolvePokemonStatusMoveHit(const PokerogueContent::Move& move, bool self,
    const PokemonStatusMoveHitPolicy& policy, PokerogueRngAdapter& rng, PokemonStatusMoveHitEvent& output);
struct PokemonStatusEffectMovePolicy {
    PokemonStatusMoveHitPolicy hit{};
    PokemonStatusApplicationPolicy application{};
    bool chanceCallbacksResolved = false;
    int16_t effectiveChance = 100;
    uint8_t ppCost = 1;
};
struct PokemonStatusEffectMoveEvent {
    PokemonStatusMoveHitEvent hit{};
    PokemonMoveStatusApplicationEvent application{};
    PokemonMoveStatusApplicationResult applicationResult = PokemonMoveStatusApplicationResult::ChanceFailed;
    uint8_t ppConsumed = 0;
};
bool usePokemonStatusEffectMove(PokemonBattleState& user, const PokemonBattleState& target,
    uint8_t slot, const PokemonStatusEffectMovePolicy& policy, PokerogueRngAdapter& rng,
    PokemonStatusEffectMoveEvent& output);

struct PokemonQueuedStatusRequest {
    uint32_t recipientPokemonId = 0;
    uint32_t sourcePokemonId = 0;
    bool hasSource = false;
    PokemonStatusEffect effect = PokemonStatusEffect::None;
    bool explicitSleepDuration = false;
    uint32_t sleepDuration = 0;
};
struct PokemonSynchronizeReactionEvent {
    bool abilityActivates = false; // Upstream shows the ability even if eligibility later fails.
    bool requestStatus = false;
    PokemonQueuedStatusRequest request{};
};
inline PokemonStatusImmunityResult resolvePokemonSynchronizeReaction(uint16_t abilityId,
    bool abilityActive, bool callbacksResolved, const PokemonQueuedStatusRequest& applied,
    PokemonSynchronizeReactionEvent& output) {
    if (static_cast<uint8_t>(applied.effect) > 7) return PokemonStatusImmunityResult::InvalidEffect;
    if (!callbacksResolved) return PokemonStatusImmunityResult::UnsupportedCondition;
    for (const auto& profile : PokerogueContent::kSynchronizeAbilityProfiles) {
        if (profile.abilityId != abilityId) continue;
        if (abilityActive && !profile.resolved) return PokemonStatusImmunityResult::UnsupportedCondition;
        PokemonSynchronizeReactionEvent event{};
        if (abilityActive && applied.hasSource &&
            (profile.statusMask & (1u << static_cast<uint8_t>(applied.effect)))) {
            event.abilityActivates = event.requestStatus = true;
            event.request.recipientPokemonId = applied.sourcePokemonId;
            event.request.sourcePokemonId = applied.recipientPokemonId;
            event.request.hasSource = true;
            event.request.effect = applied.effect;
        }
        output = event;
        return PokemonStatusImmunityResult::Resolved;
    }
    return PokemonStatusImmunityResult::UnknownAbility;
}

// trySetStatus's normal (non-override) queue transition; no duration RNG here.
PokemonStatusEligibility enqueuePokemonStatusRequest(PokemonBattleState& recipient,
    const PokemonQueuedStatusRequest& request, const PokemonStatusApplicationPolicy& policy);
// ObtainStatusEffectPhase uses an already accepted request, never repeats canSetStatus.
// Caller resolves pendingStatus, hit cancellation, form changes and ability reactions.
PokemonStatusObtainResult applyPokemonQueuedStatus(PokemonBattleState& recipient,
    const PokemonQueuedStatusRequest& request, bool reactionsResolved, PokerogueRngAdapter& recipientRng);

struct PokemonStatusEffectCommandPolicy {
    PokemonStatusEffectMovePolicy move{};
    bool reactionsResolved = false;
};
// Complete one-target status command. Source and recipient streams may alias.
// Caller resolves callbacks/field/tags before declaring the policy complete.
bool executePokemonStatusEffectCommand(PokemonBattleState& user, PokemonBattleState& target,
    uint8_t slot, const PokemonStatusEffectCommandPolicy& policy, PokerogueRngAdapter& sourceRng,
    PokerogueRngAdapter& recipientRng, PokemonStatusEffectMoveEvent& output);

// Pinned src/data/battler-tags.ts ConfusedTag.lapse, PRE_MOVE only.
// Caller owns tag lifecycle and resolves effective stats/damage callbacks.
struct PokemonConfusionTagState {
    uint32_t turns = 0;
    bool present = false;
};
struct PokemonConfusionTagPolicy {
    bool resolved = false; // Own/ally immunity callbacks and terrain resolved.
    bool ownAbilityBlocks = false;
    bool allyAbilityBlocks = false;
    bool grounded = false;
    bool mistyTerrain = false;
};
enum class PokemonConfusionTagResult : uint8_t {
    Added, Overlap, OwnAbility, AllyAbility, MistyTerrain, Unsupported, Invalid,
};
// canAddTag's simulated probe deliberately omits ConfusedTag.canAdd terrain.
bool canPokemonAddConfusionTag(const PokemonConfusionTagState& tag,
    const PokemonConfusionTagPolicy& policy, bool& output);
PokemonConfusionTagResult addPokemonConfusionTag(PokemonConfusionTagState& tag,
    uint32_t turns, const PokemonConfusionTagPolicy& policy);
bool removePokemonConfusionTag(PokemonConfusionTagState& tag);
struct PokemonConfusionMovePolicy {
    bool resolved = false;
    double effectiveAttack = 0;
    double effectiveDefense = 0;
};
struct PokemonConfusionMoveEvent {
    bool removed = false;
    bool activationRolled = false;
    bool hurtItself = false;
    bool moveCancelled = false;
    uint32_t requestedDamage = 0;
    uint32_t hpLost = 0;
};
bool checkPokemonConfusionBeforeMove(PokemonBattleState& actor, PokemonConfusionTagState& tag,
    const PokemonConfusionMovePolicy& policy, PokerogueRngAdapter& actorRng,
    PokemonConfusionMoveEvent& output);

struct PokemonStatusConfusionReactionPolicy {
    bool resolved = false; // Callback activation and target.canAddTag(CONFUSED).
    bool abilityActive = false;
    bool targetCanAddConfusion = false;
    bool simulated = false;
};
struct PokemonStatusConfusionReactionEvent {
    bool requestConfusionTag = false;
    uint8_t turns = 0;
    uint32_t targetPokemonId = 0;
    uint32_t sourcePokemonId = 0;
};
PokemonStatusImmunityResult resolvePokemonStatusConfusionReaction(const PokemonBattleState& source,
    const PokemonBattleState& recipient, PokemonStatusEffect applied,
    const PokemonStatusConfusionReactionPolicy& policy, PokerogueRngAdapter& sourceRng,
    PokemonStatusConfusionReactionEvent& output);

struct PokemonStatusMoveCheckPolicy {
    bool resolved = false;
    bool bypassSleep = false;
    bool indirectSleepWake = false;
    bool indirectFreezeWake = false;
    bool deferredFreezeThawMove = false; // Qualified self-heal attribute, including Burn Up type check.
    bool freezeCureAfterIncrement = false; // Remaining self-heal branch; no random draw.
    uint32_t sleepDurationReduction = 0; // Resolved ReduceStatusEffectDurationAbAttr result.
};
struct PokemonStatusMoveCheckEvent {
    bool thawAfterFailureChecks = false; // Caller applies cure only after remaining checks pass.
    bool cancelled = false;
    bool cured = false;
    PokemonStatusEffect effect = PokemonStatusEffect::None;
};
enum class PokemonStatusMoveCheckResult : uint8_t { Ok, InvalidStatus, UnsupportedPolicy, CounterOverflow };
PokemonStatusMoveCheckResult checkPokemonStatusBeforeMove(PokemonStatusState& status,
    const PokemonStatusMoveCheckPolicy& policy, PokerogueRngAdapter& rng, PokemonStatusMoveCheckEvent& output);

struct PokemonStatusCureEvent {
    PokemonStatusEffect previousEffect = PokemonStatusEffect::None;
    bool cleared = false;
    bool lapseNightmare = false;
    bool lapseConfusion = false;
    bool reloadAssets = false;
    uint8_t animationFrameRate = 10;
};
enum class PokemonStatusCureResult : uint8_t { Cleared, NoEffect, InvalidStatus, UnsupportedReactions };
// Actor portion of clearStatus/resetStatus. Tag/asset dispatch is required by
// the caller; returned flags describe work, never silently remove unknown tags.
inline PokemonStatusCureResult curePokemonStatusState(PokemonStatusState& status,
    bool revive, bool clearConfusion, bool reloadAssets, bool hasNightmare, bool hasConfusion,
    bool reactionsResolved, PokemonStatusCureEvent& output) {
    if (!pokemonStatusStateValid(status)) return PokemonStatusCureResult::InvalidStatus;
    if (!revive && status.present && status.effect == PokemonStatusEffect::Faint)
        return PokemonStatusCureResult::NoEffect;
    if (!reactionsResolved) return PokemonStatusCureResult::UnsupportedReactions;
    PokemonStatusCureEvent event{};
    event.previousEffect = status.effect;
    event.cleared = status.present;
    event.lapseNightmare = status.present && status.effect == PokemonStatusEffect::Sleep && hasNightmare;
    event.lapseConfusion = clearConfusion && hasConfusion;
    event.reloadAssets = reloadAssets;
    status = {};
    output = event;
    return event.cleared || event.lapseConfusion || event.reloadAssets ?
        PokemonStatusCureResult::Cleared : PokemonStatusCureResult::NoEffect;
}

struct PokemonBurnDamagePolicy {
    bool resolved = false;
    bool ignoreSourceAbility = false;
    bool abilityBypassesReduction = false;
};
// Caller resolves suppression, passive/conditional callbacks and ignore flags.
inline bool resolvePokemonBurnDamagePolicy(uint16_t abilityId, bool callbacksResolved,
    bool abilityActive, bool ignoreSourceAbility, PokemonBurnDamagePolicy& output) {
    bool found = false;
    for (const auto& profile : PokerogueContent::kAbilityMovegenProfiles)
        if (profile.abilityId == abilityId) { found = true; break; }
    if (!found || !callbacksResolved) return false;
    PokemonBurnDamagePolicy next{};
    next.resolved = true;
    next.ignoreSourceAbility = ignoreSourceAbility;
    if (abilityActive && !ignoreSourceAbility)
        for (uint16_t id : PokerogueContent::kBurnReductionBypassAbilities)
            if (id == abilityId) { next.abilityBypassesReduction = true; break; }
    output = next;
    return true;
}
// Pokemon.getAttackDamage applies burn after STAB/type and before screens.
// This returns a multiplier, never modifies permanent attack stats.
inline bool pokemonBurnDamageMultiplier(const PokemonBattleState& source, uint16_t moveId,
    const PokemonBurnDamagePolicy& policy, double& output) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move || !pokemonStatusStateValid(source.status)) return false;
    if (move->category != PokerogueContent::MovePhysical || !source.status.present ||
        source.status.effect != PokemonStatusEffect::Burn ||
        PokerogueContent::moveHasAttribute(*move, "BypassBurnDamageReductionAttr")) {
        output = 1.0;
        return true;
    }
    if (!policy.resolved) return false;
    output = !policy.ignoreSourceAbility && policy.abilityBypassesReduction ? 1.0 : 0.5;
    return true;
}

struct PokemonStatusResidualPolicy {
    bool resolved = false; // Both block attributes and post-damage callbacks resolved.
    bool active = true;
    bool switchingOut = false;
    bool blockNonDirectDamage = false;
    bool blockStatusDamage = false;
    bool bossDamageNeedsDispatcher = false;
    uint16_t burnMultiplierNumerator = 1;
    uint16_t burnMultiplierDenominator = 1;
};
inline bool resolvePokemonStatusResidualPolicy(uint16_t abilityId, PokemonStatusEffect effect,
    bool abilityActive, bool callbacksResolved, PokemonStatusResidualPolicy& output) {
    if (!callbacksResolved || static_cast<uint8_t>(effect) > 7) return false;
    for (const auto& profile : PokerogueContent::kStatusResidualAbilityProfiles) {
        if (profile.abilityId != abilityId) continue;
        if (!profile.resolved) return false;
        PokemonStatusResidualPolicy policy{};
        policy.resolved = true;
        policy.blockNonDirectDamage = abilityActive && profile.blockNonDirectDamage;
        policy.blockStatusDamage = abilityActive && (profile.blockedStatusMask & (1u << static_cast<uint8_t>(effect)));
        policy.burnMultiplierNumerator = abilityActive ? profile.burnNumerator : 1;
        policy.burnMultiplierDenominator = abilityActive ? profile.burnDenominator : 1;
        output = policy;
        return true;
    }
    return false;
}
struct PokemonStatusResidualEvent {
    PokemonStatusEffect effect = PokemonStatusEffect::None;
    uint32_t toxicTurnCount = 0;
    uint64_t requestedDamage = 0;
    uint16_t appliedDamage = 0;
    uint16_t previousHp = 0;
    uint16_t remainingHp = 0;
    bool blocked = false;
    bool fainted = false;
};
enum class PokemonStatusResidualResult : uint8_t {
    Applied, NoEffect, Blocked, InvalidState, UnsupportedPolicy, CounterOverflow
};
// PostTurnStatusEffectPhase: increment precedes damage blockers. Endure/Sturdy
// do not prevent residual KO. Boss shields and callbacks require their dispatcher.
inline PokemonStatusResidualResult applyPokemonStatusResidual(PokemonBattleState& actor,
    const PokemonStatusResidualPolicy& policy, PokemonStatusResidualEvent& output) {
    if (!pokemonStatusStateValid(actor.status) || actor.hp > actor.maxHp)
        return PokemonStatusResidualResult::InvalidState;
    if (!policy.active || !actor.hp || policy.switchingOut || !pokemonStatusIsPostTurn(actor.status))
        return PokemonStatusResidualResult::NoEffect;
    if (!policy.resolved || policy.bossDamageNeedsDispatcher || !policy.burnMultiplierDenominator)
        return PokemonStatusResidualResult::UnsupportedPolicy;
    auto nextStatus = actor.status;
    const auto tick = incrementPokemonStatusTurn(nextStatus);
    if (tick == PokemonStatusTickResult::CounterOverflow) return PokemonStatusResidualResult::CounterOverflow;
    if (tick != PokemonStatusTickResult::Ok) return PokemonStatusResidualResult::InvalidState;
    PokemonStatusResidualEvent event{};
    event.effect = nextStatus.effect;
    event.toxicTurnCount = nextStatus.toxicTurnCount;
    event.previousHp = event.remainingHp = actor.hp;
    event.blocked = policy.blockNonDirectDamage || policy.blockStatusDamage;
    if (!event.blocked) {
        uint64_t damage = nextStatus.effect == PokemonStatusEffect::Poison ? actor.maxHp / 8 :
            nextStatus.effect == PokemonStatusEffect::Toxic ? uint64_t(actor.maxHp) * nextStatus.toxicTurnCount / 16 :
            actor.maxHp / 16;
        if (!damage) damage = 1; // toDmgValue
        if (nextStatus.effect == PokemonStatusEffect::Burn) {
            damage = damage * policy.burnMultiplierNumerator / policy.burnMultiplierDenominator;
            if (!damage) damage = 1; // ReduceBurnDamageAbAttr applies toDmgValue again.
        }
        event.requestedDamage = damage;
        event.appliedDamage = static_cast<uint16_t>(damage > actor.hp ? actor.hp : damage);
        event.remainingHp = actor.hp - event.appliedDamage;
        event.fainted = !event.remainingHp;
    }
    actor.status = nextStatus;
    actor.hp = event.remainingHp;
    output = event;
    return event.blocked ? PokemonStatusResidualResult::Blocked : PokemonStatusResidualResult::Applied;
}

// Pinned PokemonSpecies.getRootSpeciesId: follow registry prevolutions, optionally
// stopping at an eligible starter. Catalog size bounds cycles, not content capacity.
inline const PokerogueContent::Species* pokemonRootSpecies(uint16_t dex, bool forStarter = false) {
    const auto* current = PokerogueContent::findSpeciesByDex(dex);
    for (std::size_t depth = 0; current && depth < PokerogueContent::kSpeciesCount; ++depth) {
        if (!current->prevolutionDex || (forStarter && current->starterEligible)) return current;
        current = PokerogueContent::findSpeciesByDex(current->prevolutionDex);
    }
    return nullptr; // Missing reference or cycle; do not substitute the input species.
}

// Source Pokemon.setStatStage clamps to [-6, 6]. IDs follow upstream Stat;
// HP (0) has no battle stage. Effects/abilities must resolve their policy first.
inline bool setPokemonStatStage(PokemonBattleState& state, uint8_t stat, int32_t value) {
    if (!stat || stat > 7) return false;
    state.statStages[stat - 1] = static_cast<int8_t>(value < -6 ? -6 : value > 6 ? 6 : value);
    return true;
}

inline void resetPokemonStatStages(PokemonBattleState& state) {
    for (auto& stage : state.statStages) stage = 0;
}

// Supported summon/turn fields only. Persistent actor state survives a recall.
// Source: pinned Pokemon.resetSummonData; other tags/forms need their own dispatch.
inline void resetPokemonSummonState(PokemonBattleState& state) {
    resetPokemonStatStages(state);
    state.heldItemLostTags = {};
    state.turnDamageDealt = 0;
}

// Baseline getStatStageMultiplier without abilities/move overrides/held items.
// Critical hits ignore negative attack stages and positive defense stages.
inline bool pokemonStatStageMultiplier(const PokemonBattleState& state, uint8_t stat,
                                       bool critical, double& output) {
    if (!stat || stat > 5) return false;
    int stage = state.statStages[stat - 1];
    if (stage < -6 || stage > 6) return false;
    if (critical && ((stat == 1 || stat == 3) && stage < 0)) stage = 0;
    if (critical && ((stat == 2 || stat == 4) && stage > 0)) stage = 0;
    output = static_cast<double>(stage > 0 ? 2 + stage : 2) /
        (stage < 0 ? 2 - stage : 2);
    return true;
}

inline bool pokemonBaselineEffectiveStat(const PokemonBattleState& state, uint8_t stat,
                                         bool critical, uint32_t& output) {
    double multiplier = 1.0;
    if (!stat || stat > 5 || !state.stats[stat] ||
        !pokemonStatStageMultiplier(state, stat, critical, multiplier)) return false;
    if (!pokemonStatusStateValid(state.status)) return false;
    uint32_t effective = static_cast<uint32_t>(state.stats[stat] * multiplier);
    if (stat == 5 && state.status.present && state.status.effect == PokemonStatusEffect::Paralysis)
        effective >>= 1;
    output = effective ? effective : 1;
    return true;
}

inline bool pokemonAccuracyStageMultiplier(const PokemonBattleState& user,
    const PokemonBattleState& target, double& output) {
    const int accuracy = user.statStages[5];
    const int evasion = target.statStages[6];
    if (accuracy < -6 || accuracy > 6 || evasion < -6 || evasion > 6) return false;
    int difference = accuracy - evasion;
    if (difference > 6) difference = 6;
    if (difference < -6) difference = -6;
    output = difference >= 0 ? (3.0 + difference) / 3.0 : 3.0 / (3.0 - difference);
    return true;
}

// Pinned Pokemon constructor derives six five-bit IVs from the actor's
// 32-bit identity, in permanent-stat order.
void derivePokemonIvsFromId(uint32_t pokemonId, uint8_t outputIvs[6]);

PokemonBattleInitResult initializePokemonBattleState(
    const PokemonBattleInit& input,
    PokemonBattleState& output);

// Recalculates a real same-species form without replacing the actor identity.
// fullRestore models PokemonHealPhase HP/PP; status/tags belong to their own state.
// Recalculates base stats without reconstructing summon/turn state or PP.
bool recalculatePokemonBattleLevel(PokemonBattleState& state, uint16_t level);

bool changePokemonBattleForm(PokemonBattleState& state, const char* targetFormId,
    uint16_t resolvedAbilityId, bool fullRestore = false);

// Transfers generated identity into a validated battle state; non-identity
// inputs remain explicit until their pinned generators are ported.
PokemonBattleInitResult initializePokemonBattleStateForActor(
    const PokemonBattleInit& nonIdentityInput,
    const PokemonActorIdentity& identity,
    PokemonBattleState& output);

struct PokemonMoveWeatherContext;
enum class PokemonBaseDamageResult : uint8_t { Ok = 0, MissingMove, NonDamagingMove, InvalidStats, UnsupportedAbilityCondition, UnresolvedWeather };
PokemonBaseDamageResult calculatePokemonBaseDamage(
    const PokemonBattleState& attacker,
    const PokemonBattleState& defender,
    uint16_t moveId,
    double& outputBaseDamage, bool critical = false,
    const PokemonMoveWeatherContext* weatherContext = nullptr);

enum class PokemonTypeEffectivenessResult : uint8_t { Ok = 0, MissingSpecies, MissingMove, InvalidType };
// Baseline chart lookup for an attack type independent of a move ID.
// Ability/field effects are handled by their own resolved effect layer.
PokemonTypeEffectivenessResult calculatePokemonAttackTypeEffectiveness(
    const char* attackType, const PokemonBattleState& defender, double& outputMultiplier);
PokemonTypeEffectivenessResult calculatePokemonTypeEffectiveness(
    uint16_t moveId,
    const PokemonBattleState& defender,
    double& outputMultiplier);

// Pinned src/enums/weather-type.ts IDs. Caller resolves suppression and
// PreAttackWeatherOverrideAbAttr before supplying the effective weather.
enum class PokemonEffectiveWeather : uint8_t {
    None = 0, Sunny = 1, Rain = 2, Sandstorm = 3, Hail = 4,
    Snow = 5, Fog = 6, HeavyRain = 7, HarshSun = 8, StrongWinds = 9
};
// Weather duration is separate from per-user effective weather. Zero means
// indefinite, including biome weather, matching upstream Weather.lapse.
struct PokemonArenaWeatherState {
    PokemonEffectiveWeather type = PokemonEffectiveWeather::None;
    uint16_t turnsLeft = 0;
    uint16_t maxDuration = 0;
};
// Caller supplies the resolved time-of-day and any effective timed-event pool.
// An override contains ten weights in pinned WeatherType enum order.
bool selectPokemonBiomeWeather(const char* biomeId, bool duskOrNight,
    PokerogueRngAdapter& rng, PokemonEffectiveWeather& output,
    const uint16_t* resolvedEventWeights = nullptr);
bool pokemonWeatherIsImmutable(PokemonEffectiveWeather type);
bool setPokemonArenaWeather(PokemonArenaWeatherState& state,
    PokemonEffectiveWeather type, uint16_t resolvedDuration);
bool lapsePokemonArenaWeather(PokemonArenaWeatherState& state);
struct PokemonWeatherDamagePolicy {
    bool resolved = false;
    bool weatherSuppressed = false;
    bool abilityBlocksDamage = false;
    bool underground = false;
    bool underwater = false;
    bool switchingOut = false;
    // Caller supplies resolved current types, including form/type changes.
    const char* type1 = nullptr;
    const char* type2 = nullptr;
};
// Call only after ability applicability is resolved, including suppression.
bool pokemonAbilityBlocksWeatherDamage(uint16_t abilityId,
    PokemonEffectiveWeather weather, bool& outputBlocked);
struct PokemonWeatherDamageEvent {
    uint16_t damageApplied = 0;
    bool fainted = false;
};
bool applyPokemonWeatherResidualDamage(PokemonBattleState& target,
    const PokemonArenaWeatherState& arena, const PokemonWeatherDamagePolicy& policy,
    PokemonWeatherDamageEvent& output);
struct PokemonWeatherTurnEndEvent {
    bool expired = false;
    PokemonEffectiveWeather previousWeather = PokemonEffectiveWeather::None;
    bool requestWeatherFormReversion = false;
};
// Phase-level transition: unlike Weather.lapse, clears expired weather and
// returns the event needed by presentation and the form-change resolver.
bool advancePokemonArenaWeatherTurnEnd(PokemonArenaWeatherState& state,
    PokemonWeatherTurnEndEvent& output);

// WeatherChangeAttr command. Caller must resolve post-weather ability/form/tag hooks
// and FieldEffectModifier duration before publishing the command's state and event.
struct PokemonWeatherChangePolicy {
    bool resolved = false;
    bool weatherCallbacksResolved = false;
    bool blockedBeforeMove = false;
    uint16_t duration = 0;
    uint8_t ppCost = 0;
};
struct PokemonWeatherChangeEvent {
    bool blocked = false;
    bool failedCondition = false;
    bool changed = false;
    uint8_t ppSpent = 0;
    PokemonEffectiveWeather previousWeather = PokemonEffectiveWeather::None;
    PokemonEffectiveWeather nextWeather = PokemonEffectiveWeather::None;
};
enum class PokemonWeatherChangeResult : uint8_t { Ok, InvalidState, UnsupportedMove, UnresolvedPolicy };
PokemonWeatherChangeResult usePokemonWeatherChangeCommand(PokemonBattleState& user,
    PokemonArenaWeatherState& arena, uint8_t moveSlot,
    const PokemonWeatherChangePolicy& policy, PokemonWeatherChangeEvent& output);

struct PokemonWeatherResolutionPolicy {
    bool resolved = false;
    bool suppressesOrdinaryWeather = false;
    bool suppressesImmutableWeather = false;
    PokemonEffectiveWeather attackerOverride = PokemonEffectiveWeather::None;
};
struct PokemonWeatherAbilityComponent {
    uint16_t abilityId = 0;
    bool applies = false;
    bool belongsToAttacker = false;
};
// Caller supplies field membership and resolved primary/passive applicability.
bool composePokemonWeatherResolutionPolicy(const PokemonWeatherAbilityComponent* components,
    std::size_t count, PokemonWeatherResolutionPolicy& output);
struct PokemonMoveWeatherContext {
    bool resolved = false;
    PokemonEffectiveWeather effectiveWeather = PokemonEffectiveWeather::None;
    // Arena weather after suppression, independent of attacker overrides.
    PokemonEffectiveWeather cancellationWeather = PokemonEffectiveWeather::None;
};
bool resolvePokemonMoveWeatherContext(const PokemonArenaWeatherState& arena,
    const PokemonWeatherResolutionPolicy& policy, PokemonMoveWeatherContext& output);
bool pokemonWeatherEffectiveSpeed(const PokemonBattleState& state,
    const PokemonMoveWeatherContext& weather, uint32_t& output);
bool pokemonWeatherMoveAccuracy(uint16_t moveId,
    const PokemonMoveWeatherContext* context, int16_t& outputAccuracy);
bool pokemonMoveWeatherMultiplier(uint16_t moveId,
    const PokemonMoveWeatherContext& context, double& outputMultiplier);

enum class PokemonDamageCoreResult : uint8_t {
    Ok = 0, MissingMove, MissingSpecies, NonDamagingMove, InvalidStats, InvalidType, UnsupportedAbilityCondition, UnresolvedWeather
};
PokemonDamageCoreResult calculatePokemonDamageCore(
    const PokemonBattleState& attacker,
    const PokemonBattleState& defender,
    uint16_t moveId,
    bool moveIsTypeless,
    uint32_t& outputDamage,
    const PokemonMoveWeatherContext* weatherContext = nullptr);

struct PokemonCriticalPolicy {
    bool resolved = false;
    uint8_t bonusStages = 0;
    bool alwaysCritical = false;
    double damageMultiplier = 1.0;
    bool blocked = false;
};
struct PokemonCriticalAbilityComponent {
    uint16_t abilityId = 0;
    bool applies = false;
    bool belongsToAttacker = false;
};
bool composePokemonCriticalAbilityPolicy(const PokemonCriticalAbilityComponent* components,
    std::size_t count, bool ignoreDefenderAbilities, PokemonCriticalPolicy& output);
bool pokemonMoveCriticalDenominator(uint16_t moveId, uint8_t& outputDenominator,
    uint8_t resolvedBonusStages = 0);

struct PokemonHitPolicy {
    bool blockedByAbility = false;
    bool resolved = false;
    bool bypassAccuracy = false;
    double accuracyMultiplier = 1.0; // Additional resolved multiplier; stages remain separate.
};
bool composePokemonAlwaysHitPolicy(const PokemonWeatherAbilityComponent* components,
    std::size_t count, PokemonHitPolicy& output, uint16_t moveId = 0,
    const PokemonMoveWeatherContext* weather = nullptr);
struct PokemonMoveDamageRoll {
    bool abilityBlocked = false;
    bool hit = false;
    bool critical = false;
    bool accuracyWasRolled = false;
    uint8_t accuracyRoll = 0;
    uint8_t criticalRoll = 0;
    bool criticalWasRolled = false;
    uint8_t randomDamagePercent = 0;
    double typeEffectiveness = 1.0;
    uint32_t damage = 0;
};
enum class PokemonMoveDamageResult : uint8_t {
    Ok = 0, MissingMove, MissingSpecies, NonDamagingMove, InvalidAccuracy, InvalidStats, InvalidType, UnsupportedAbilityCondition, UnresolvedWeather
};
PokemonMoveDamageResult resolveStandardPokemonMoveDamage(
    const PokemonBattleState& attacker,
    const PokemonBattleState& defender,
    uint16_t moveId,
    bool moveIsTypeless,
    PokerogueRngAdapter& battleRng,
    PokemonMoveDamageRoll& output,
    const PokemonMoveWeatherContext* weatherContext = nullptr,
    const PokemonCriticalPolicy* criticalPolicy = nullptr,
    const PokemonHitPolicy* hitPolicy = nullptr,
    const PokemonBurnDamagePolicy* burnPolicy = nullptr);

struct PokemonPpPolicy {
    bool resolved = false;
    uint8_t cost = 1; // Zero represents a resolved ignore-PP execution mode.
};
bool pokemonActiveTargetsPpCost(const uint16_t* abilityIds, uint8_t count, uint8_t& output);
bool pokemonSingleOpponentPpCost(uint16_t opponentAbilityId, uint8_t& output);
struct PokemonMoveActionResult {
    PokemonMoveDamageRoll damageRoll{};
    uint16_t damageApplied = 0;
    bool targetFainted = false;
    bool weatherCancelled = false;
    uint8_t ppConsumed = 0;
    PokemonMoveDamageResult damageResolutionStatus = PokemonMoveDamageResult::Ok;
};
enum class PokemonMoveActionStatus : uint8_t {
    Ok = 0, InvalidMoveSlot, NoPp, TargetAlreadyFainted, DamageResolutionFailed,
    UnsupportedAbilityCondition, UnresolvedWeather, UnresolvedPp, UnresolvedBoss
};
struct PokemonBossState;
struct PokemonBossDamagePolicy;
PokemonMoveActionStatus useStandardPokemonMove(
    PokemonBattleState& attacker,
    PokemonBattleState& defender,
    uint8_t moveSlot,
    bool moveIsTypeless,
    PokerogueRngAdapter& battleRng,
    PokemonMoveActionResult& output,
    const PokemonMoveWeatherContext* weatherContext = nullptr,
    const PokemonCriticalPolicy* criticalPolicy = nullptr,
    const PokemonHitPolicy* hitPolicy = nullptr,
    const PokemonPpPolicy* ppPolicy = nullptr,
    PokemonBossState* targetBossState = nullptr,
    const PokemonBossDamagePolicy* bossDamagePolicy = nullptr,
    PokerogueRngAdapter* bossGlobalRng = nullptr,
    const PokemonBurnDamagePolicy* burnPolicy = nullptr);

enum class PokemonAbilitySelectionResult : uint8_t { Ok = 0, MissingSpecies, InvalidHiddenRate };
PokemonAbilitySelectionResult selectPokemonAbilityIndex(
    uint16_t speciesDex,
    uint16_t hiddenAbilityRate,
    PokerogueRngAdapter& rng,
    uint8_t& outputAbilityIndex);

enum class PokemonGenderSelectionResult : uint8_t { Ok = 0, MissingSpecies, InvalidGenderRatio };
PokemonGenderSelectionResult selectPokemonGender(
    uint16_t speciesDex,
    PokerogueRngAdapter& rng,
    PokemonGender& outputGender);

struct PokemonBossSegmentDamage {
    uint32_t adjustedDamage = 0;
    uint16_t clearedSegmentIndex = 0;
};
struct PokemonBossSegmentClearEvent {
    uint16_t nextSegmentIndex = 0;
    uint32_t statStages[5]{}; // ATK, DEF, SPATK, SPDEF, SPD; ignoreAbilities phase.
};
bool planPokemonBossSegmentCleared(const PokemonBattleState& boss, uint16_t segmentCount,
    uint16_t currentSegmentIndex, uint16_t clearedSegmentIndex, bool hasTrainer,
    PokerogueRngAdapter& rng, PokemonBossSegmentClearEvent& output);

// Pinned utils/damage.ts calculateBossSegmentDamage. Does not mutate HP or
// trigger shield stat boosts/forms; the boss phase owns those consequences.
bool calculatePokemonBossSegmentDamage(uint32_t damage, uint16_t currentHp,
    uint16_t maxHp, uint16_t segmentCount, uint16_t currentSegmentIndex,
    uint16_t minimumSegmentIndex, PokemonBossSegmentDamage& output);

struct PokemonBossState {
    uint16_t segmentCount = 0;
    uint16_t segmentIndex = 0;
    bool classicFinalBossFirstPhase = false;
    bool hasTrainer = false;
};
bool initializeClassicPokemonBossState(uint16_t speciesDex, uint16_t level, uint32_t wave,
    bool forceBoss, bool finalBossFirstPhase, PokemonBossState& output);
// EncounterPhase: proportional shields when both freshly generated enemies are bosses.
// Base totals belong to the resolved species forms, not calculated battle stats.
bool distributePokemonDoubleBossSegments(PokemonBossState& first, uint16_t firstBaseTotal,
    PokemonBossState& second, uint16_t secondBaseTotal);
struct PokemonBossDamagePolicy {
    bool resolved = false;
    bool damageCallbacksResolved = false;
    bool ignoreSegments = false;
};
struct PokemonBossDamageEvent {
    uint16_t damageApplied = 0;
    bool preventedFinalBossKo = false;
    PokemonBossSegmentClearEvent segments{};
};
// Owns only resolved direct damage + ignoreAbilities shield boosts. Caller must
// resolve Endure/PostDamage/form callbacks before publishing a battle command.
bool applyPokemonBossDamage(PokemonBattleState& boss, PokemonBossState& state,
    uint32_t damage, const PokemonBossDamagePolicy& policy, PokerogueRngAdapter& rng,
    PokemonBossDamageEvent& output);

} // namespace Pokerogue3DS

#pragma once

#include "content/PokerogueRuntimeContent.hpp"
#include <cstdint>
#include <cstddef>

namespace Pokerogue3DS {

class PokerogueRngAdapter;

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

bool getPokemonNatureModifiers(PokemonNature nature, PokemonNatureModifiers& output);
PokemonNature selectPokemonNature(PokerogueRngAdapter& rng);

struct PokemonActorIdentity {
    uint32_t pokemonId = 0;
    uint8_t ivs[6]{};
    uint8_t abilityIndex = 0;
    PokemonGender gender = PokemonGender::Unspecified;
    PokemonNature nature = PokemonNature::Unspecified;
    const char* formId = nullptr;
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

struct PokemonBattleState {
    uint16_t speciesDex = 0;
    const char* formId = nullptr;
    uint16_t level = 0;
    uint32_t pokemonId = 0;
    uint16_t abilityId = 0;
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
    bool statsAreBaseFormulaOnly = true;
};

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
    const uint32_t effective = static_cast<uint32_t>(state.stats[stat] * multiplier);
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
    const PokemonHitPolicy* hitPolicy = nullptr);

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
    UnsupportedAbilityCondition, UnresolvedWeather, UnresolvedPp
};
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
    const PokemonPpPolicy* ppPolicy = nullptr);

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

} // namespace Pokerogue3DS

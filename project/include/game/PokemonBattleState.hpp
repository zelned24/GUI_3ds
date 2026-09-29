#pragma once

#include "content/PokerogueRuntimeContent.hpp"
#include <cstdint>

namespace Pokerogue3DS {

class PokerogueRngAdapter;

enum class PokemonGender : uint8_t { Unspecified = 0, Genderless, Male, Female };

struct PokemonActorIdentity {
    uint32_t pokemonId = 0;
    uint8_t ivs[6]{};
    uint8_t abilityIndex = 0;
    PokemonGender gender = PokemonGender::Unspecified;
};

enum class PokemonActorIdentityResult : uint8_t {
    Ok = 0, MissingSpecies, InvalidHiddenRate, InvalidGenderRatio
};

// Mirrors the pinned Pokemon constructor's pre-form identity draws:
// ability index, 32-bit ID/IVs, then gender.
PokemonActorIdentityResult generatePokemonActorIdentity(
    uint16_t speciesDex,
    uint16_t hiddenAbilityRate,
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
    uint16_t stats[6]{}; // Upstream permanent-stat order.
    uint8_t moveCount = 0;
    BattleMoveState moves[4]{};
    bool ivsWereDerivedFromPokemonId = false;
    bool statsAreBaseFormulaOnly = true;
};

// Pinned Pokemon constructor derives six five-bit IVs from the actor's
// 32-bit identity, in permanent-stat order.
void derivePokemonIvsFromId(uint32_t pokemonId, uint8_t outputIvs[6]);

PokemonBattleInitResult initializePokemonBattleState(
    const PokemonBattleInit& input,
    PokemonBattleState& output);

enum class PokemonBaseDamageResult : uint8_t { Ok = 0, MissingMove, NonDamagingMove, InvalidStats };
PokemonBaseDamageResult calculatePokemonBaseDamage(
    const PokemonBattleState& attacker,
    const PokemonBattleState& defender,
    uint16_t moveId,
    double& outputBaseDamage);

enum class PokemonTypeEffectivenessResult : uint8_t { Ok = 0, MissingSpecies, MissingMove, InvalidType };
PokemonTypeEffectivenessResult calculatePokemonTypeEffectiveness(
    uint16_t moveId,
    const PokemonBattleState& defender,
    double& outputMultiplier);

enum class PokemonDamageCoreResult : uint8_t {
    Ok = 0, MissingMove, MissingSpecies, NonDamagingMove, InvalidStats, InvalidType
};
PokemonDamageCoreResult calculatePokemonDamageCore(
    const PokemonBattleState& attacker,
    const PokemonBattleState& defender,
    uint16_t moveId,
    bool moveIsTypeless,
    uint32_t& outputDamage);

struct PokemonMoveDamageRoll {
    bool hit = false;
    bool critical = false;
    bool accuracyWasRolled = false;
    uint8_t accuracyRoll = 0;
    uint8_t criticalRoll = 0;
    uint8_t randomDamagePercent = 0;
    double typeEffectiveness = 1.0;
    uint32_t damage = 0;
};
enum class PokemonMoveDamageResult : uint8_t {
    Ok = 0, MissingMove, MissingSpecies, NonDamagingMove, InvalidAccuracy, InvalidStats, InvalidType
};
PokemonMoveDamageResult resolveStandardPokemonMoveDamage(
    const PokemonBattleState& attacker,
    const PokemonBattleState& defender,
    uint16_t moveId,
    bool moveIsTypeless,
    PokerogueRngAdapter& battleRng,
    PokemonMoveDamageRoll& output);

struct PokemonMoveActionResult {
    PokemonMoveDamageRoll damageRoll{};
    uint16_t damageApplied = 0;
    bool targetFainted = false;
    PokemonMoveDamageResult damageResolutionStatus = PokemonMoveDamageResult::Ok;
};
enum class PokemonMoveActionStatus : uint8_t {
    Ok = 0, InvalidMoveSlot, NoPp, TargetAlreadyFainted, DamageResolutionFailed
};
PokemonMoveActionStatus useStandardPokemonMove(
    PokemonBattleState& attacker,
    PokemonBattleState& defender,
    uint8_t moveSlot,
    bool moveIsTypeless,
    PokerogueRngAdapter& battleRng,
    PokemonMoveActionResult& output);

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

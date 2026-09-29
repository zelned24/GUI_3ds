#pragma once

#include "content/PokerogueRuntimeContent.hpp"
#include <cstdint>

namespace Pokerogue3DS {

class PokerogueRngAdapter;

enum class PokemonGender : uint8_t { Unspecified = 0, Genderless, Male, Female };

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
    uint16_t abilityId = 0;
    PokemonGender gender = PokemonGender::Unspecified;
    uint16_t maxHp = 0;
    uint16_t hp = 0;
    uint16_t stats[6]{}; // Upstream permanent-stat order.
    uint8_t moveCount = 0;
    BattleMoveState moves[4]{};
    bool statsAreBaseFormulaOnly = true;
};

PokemonBattleInitResult initializePokemonBattleState(
    const PokemonBattleInit& input,
    PokemonBattleState& output);

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

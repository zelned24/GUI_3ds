#include "game/PokemonBattleState.hpp"
#include "game/PokerogueRngAdapter.hpp"

using Pokerogue3DS::PokemonBattleInit;
using Pokerogue3DS::PokemonBattleInitResult;
using Pokerogue3DS::PokemonBattleState;
using Pokerogue3DS::PokemonAbilitySelectionResult;
using Pokerogue3DS::PokemonGender;
using Pokerogue3DS::PokemonGenderSelectionResult;

extern "C" int runPokemonBattleStateChecks() {
    PokemonBattleInit input{};
    input.speciesDex = 1;
    input.level = 5;
    input.abilityId = 65;
    input.moveCount = 2;
    input.moveIds[0] = 33;
    input.moveIds[1] = 45;
    input.gender = PokemonGender::Male;

    PokemonBattleState state{};
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::Ok) return 1;
    if (state.maxHp != 19 || state.hp != 19 || state.stats[1] != 9 || state.stats[2] != 9) return 2;
    if (state.moveCount != 2 || state.moves[0].moveId != 33 || state.moves[0].pp != 35 ||
        state.moves[1].moveId != 45 || state.moves[1].pp != 40) return 3;
    if (state.gender != PokemonGender::Male) return 28;
    if (!state.statsAreBaseFormulaOnly) return 4;

    PokemonBattleState damageTarget = state;
    double baseDamage = -1.0;
    if (Pokerogue3DS::calculatePokemonBaseDamage(state, damageTarget, 33, baseDamage) !=
        Pokerogue3DS::PokemonBaseDamageResult::Ok || baseDamage < 5.199999999 || baseDamage > 5.200000001) return 32;
    baseDamage = -1.0;
    if (Pokerogue3DS::calculatePokemonBaseDamage(state, damageTarget, 45, baseDamage) !=
        Pokerogue3DS::PokemonBaseDamageResult::NonDamagingMove || baseDamage != -1.0) return 33;
    if (Pokerogue3DS::calculatePokemonBaseDamage(state, damageTarget, 65535, baseDamage) !=
        Pokerogue3DS::PokemonBaseDamageResult::MissingMove || baseDamage != -1.0) return 34;
    double typeEffectiveness = -1.0;
    if (Pokerogue3DS::calculatePokemonTypeEffectiveness(33, state, typeEffectiveness) !=
        Pokerogue3DS::PokemonTypeEffectivenessResult::Ok || typeEffectiveness != 1.0) return 35;
    damageTarget.speciesDex = 92; // Gastly: Ghost/Poison; Normal is immune.
    if (Pokerogue3DS::calculatePokemonTypeEffectiveness(33, damageTarget, typeEffectiveness) !=
        Pokerogue3DS::PokemonTypeEffectivenessResult::Ok || typeEffectiveness != 0.0) return 36;
    damageTarget.speciesDex = 304; // Aron: Steel/Rock; two 0.5 multipliers combine.
    if (Pokerogue3DS::calculatePokemonTypeEffectiveness(33, damageTarget, typeEffectiveness) !=
        Pokerogue3DS::PokemonTypeEffectivenessResult::Ok || typeEffectiveness != 0.25) return 37;
    typeEffectiveness = -1.0;
    if (Pokerogue3DS::calculatePokemonTypeEffectiveness(65535, damageTarget, typeEffectiveness) !=
        Pokerogue3DS::PokemonTypeEffectivenessResult::MissingMove || typeEffectiveness != -1.0) return 38;
    damageTarget.speciesDex = 7; // Squirtle: Vine Whip is super-effective.
    damageTarget.stats[2] = 12;
    uint32_t coreDamage = 0;
    const auto coreResult = Pokerogue3DS::calculatePokemonDamageCore(state, damageTarget, 22, false, coreDamage);
    if (coreResult != Pokerogue3DS::PokemonDamageCoreResult::Ok) return 39;
    if (coreDamage != 14) return 42;
    if (Pokerogue3DS::calculatePokemonDamageCore(state, damageTarget, 22, true, coreDamage) !=
        Pokerogue3DS::PokemonDamageCoreResult::Ok || coreDamage != 9) return 40;
    damageTarget.speciesDex = 92;
    if (Pokerogue3DS::calculatePokemonDamageCore(state, damageTarget, 33, false, coreDamage) !=
        Pokerogue3DS::PokemonDamageCoreResult::Ok || coreDamage != 0) return 41;

    input.speciesDex = 6;
    input.formId = "charizard:mega_x";
    input.abilityId = 181; // Tough Claws, from Charizard Mega X upstream form data.
    input.moveCount = 0;
    input.ivs[0] = input.ivs[1] = input.ivs[2] = input.ivs[3] = input.ivs[4] = input.ivs[5] = 0;
    input.natureRaisedStat = input.natureLoweredStat = -1;
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::Ok) return 15;
    if (state.formId == nullptr || state.formId[0] != 'c' || state.stats[1] != 18) return 16;
    input.formId = "venusaur:mega";
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::InvalidForm) return 17;
    input.formId = "charizard:mega_x";
    input.abilityId = 66;
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::InvalidAbility) return 18;

    input.speciesDex = 1;
    input.formId = nullptr;
    input.abilityId = 65;
    input.moveCount = 2;
    input.moveIds[0] = 33;
    input.moveIds[1] = 45;
    input.ivs[0] = input.ivs[1] = input.ivs[2] = input.ivs[3] = input.ivs[4] = input.ivs[5] = 0;

    input.ivs[0] = input.ivs[1] = input.ivs[2] = input.ivs[3] = input.ivs[4] = input.ivs[5] = 31;
    input.natureRaisedStat = 1;
    input.natureLoweredStat = 2;
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::Ok) return 5;
    if (state.stats[0] != 21 || state.stats[1] != 13 || state.stats[2] != 9) return 6;

    input.natureRaisedStat = 2;
    input.natureLoweredStat = 2;
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::Ok) return 13;
    if (state.stats[1] != 11 || state.stats[2] != 11) return 14;

    const PokemonBattleState preserved = state;
    input.gender = PokemonGender::Unspecified;
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::InvalidGender) return 29;
    if (state.gender != preserved.gender) return 30;
    input.gender = PokemonGender::Male;
    input.ivs[0] = 32;
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::InvalidIv) return 7;
    if (state.maxHp != preserved.maxHp || state.stats[1] != preserved.stats[1]) return 8;
    input.ivs[0] = 31;

    input.abilityId = 999;
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::InvalidAbility) return 9;
    input.abilityId = 65;
    input.moveIds[0] = 65535;
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::MissingMove) return 10;
    input.moveIds[0] = 33;
    input.moveCount = 5;
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::InvalidMoveCount) return 11;
    input.moveCount = 2;

    input.speciesDex = 0;
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::MissingSpecies) return 12;

    input.speciesDex = 81;
    input.abilityId = 42;
    input.gender = PokemonGender::Genderless;
    input.moveCount = 0;
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::Ok ||
        state.gender != PokemonGender::Genderless || state.speciesDex != 81) return 31;

    const uint16_t seed[] = {'a', 'b', 'i', 'l', 'i', 't', 'y'};
    Pokerogue3DS::PokerogueRngAdapter rng;
    rng.sow(seed, sizeof(seed) / sizeof(seed[0]));
    Pokerogue3DS::PokerogueRngAdapter expected = rng;
    const uint8_t expectedRegular = static_cast<uint8_t>(expected.randSeedInt(2));
    const bool expectedHidden = expected.randSeedInt(256) == 0;
    uint8_t abilityIndex = 9;
    if (Pokerogue3DS::selectPokemonAbilityIndex(16, 256, rng, abilityIndex) != PokemonAbilitySelectionResult::Ok) return 19;
    const auto actualRngState = rng.state();
    const auto expectedRngState = expected.state();
    if (abilityIndex != (expectedHidden ? 2 : expectedRegular) ||
        actualRngState.carry != expectedRngState.carry || actualRngState.s0 != expectedRngState.s0 ||
        actualRngState.s1 != expectedRngState.s1 || actualRngState.s2 != expectedRngState.s2) return 20;

    rng.sow(seed, sizeof(seed) / sizeof(seed[0]));
    expected = rng;
    const bool expectedSingleHidden = expected.randSeedInt(256) == 0;
    if (Pokerogue3DS::selectPokemonAbilityIndex(1, 256, rng, abilityIndex) != PokemonAbilitySelectionResult::Ok) return 21;
    const auto singleActualState = rng.state();
    const auto singleExpectedState = expected.state();
    if (abilityIndex != (expectedSingleHidden ? 2 : 0) || singleActualState.carry != singleExpectedState.carry ||
        singleActualState.s0 != singleExpectedState.s0 || singleActualState.s1 != singleExpectedState.s1 ||
        singleActualState.s2 != singleExpectedState.s2) return 22;
    abilityIndex = 9;
    if (Pokerogue3DS::selectPokemonAbilityIndex(1, 0, rng, abilityIndex) != PokemonAbilitySelectionResult::InvalidHiddenRate || abilityIndex != 9) return 23;

    // Numeric ratios consume exactly one upstream randSeedFloat; genderless consumes none.
    rng.sow(seed, sizeof(seed) / sizeof(seed[0]));
    expected = rng;
    const double genderRoll = expected.frac() * 100.0;
    PokemonGender gender = PokemonGender::Genderless;
    if (Pokerogue3DS::selectPokemonGender(6, rng, gender) != PokemonGenderSelectionResult::Ok) return 24;
    const auto genderActualState = rng.state();
    const auto genderExpectedState = expected.state();
    if (gender != (genderRoll <= 87.5 ? PokemonGender::Male : PokemonGender::Female) ||
        genderActualState.carry != genderExpectedState.carry || genderActualState.s0 != genderExpectedState.s0 ||
        genderActualState.s1 != genderExpectedState.s1 || genderActualState.s2 != genderExpectedState.s2) return 25;

    rng.sow(seed, sizeof(seed) / sizeof(seed[0]));
    expected = rng;
    if (Pokerogue3DS::selectPokemonGender(81, rng, gender) != PokemonGenderSelectionResult::Ok ||
        gender != PokemonGender::Genderless) return 26;
    const auto genderlessActualState = rng.state();
    const auto genderlessExpectedState = expected.state();
    if (genderlessActualState.carry != genderlessExpectedState.carry ||
        genderlessActualState.s0 != genderlessExpectedState.s0 || genderlessActualState.s1 != genderlessExpectedState.s1 ||
        genderlessActualState.s2 != genderlessExpectedState.s2) return 27;
    return 0;
}

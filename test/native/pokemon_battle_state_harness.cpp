#include "game/PokemonBattleState.hpp"
#include "game/PokerogueRngAdapter.hpp"

using Pokerogue3DS::PokemonBattleInit;
using Pokerogue3DS::PokemonBattleInitResult;
using Pokerogue3DS::PokemonBattleState;
using Pokerogue3DS::PokemonAbilitySelectionResult;
using Pokerogue3DS::PokemonGender;
using Pokerogue3DS::PokemonGenderSelectionResult;
using Pokerogue3DS::PokemonMoveDamageResult;
using Pokerogue3DS::PokemonMoveDamageRoll;
using Pokerogue3DS::PokemonMoveActionResult;
using Pokerogue3DS::PokemonMoveActionStatus;
using Pokerogue3DS::PokerogueRngAdapter;

extern "C" int runPokemonBattleStateChecks() {
    uint8_t derivedIvs[6]{};
    Pokerogue3DS::derivePokemonIvsFromId(0xFFFFFFFFu, derivedIvs);
    for (uint8_t iv : derivedIvs) if (iv != 31) return 57;
    Pokerogue3DS::derivePokemonIvsFromId(0, derivedIvs);
    for (uint8_t iv : derivedIvs) if (iv != 0) return 58;
    Pokerogue3DS::derivePokemonIvsFromId(0x12345678u, derivedIvs);
    const uint8_t expectedDerivedIvs[6] = {9, 3, 8, 21, 19, 24};
    for (uint8_t i = 0; i < 6; ++i) if (derivedIvs[i] != expectedDerivedIvs[i]) return 59;

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
    for (uint8_t iv : state.ivs) if (iv != 0) return 60;

    PokemonBattleInit actorIdInput = input;
    actorIdInput.pokemonId = 0x12345678u;
    actorIdInput.deriveIvsFromPokemonId = true;
    PokemonBattleState actorIdState{};
    if (Pokerogue3DS::initializePokemonBattleState(actorIdInput, actorIdState) != PokemonBattleInitResult::Ok ||
        actorIdState.pokemonId != actorIdInput.pokemonId || !actorIdState.ivsWereDerivedFromPokemonId) return 61;
    PokemonBattleInit equivalentExplicitInput = input;
    Pokerogue3DS::derivePokemonIvsFromId(actorIdInput.pokemonId, equivalentExplicitInput.ivs);
    PokemonBattleState equivalentExplicitState{};
    if (Pokerogue3DS::initializePokemonBattleState(equivalentExplicitInput, equivalentExplicitState) !=
        PokemonBattleInitResult::Ok) return 62;
    for (uint8_t i = 0; i < 6; ++i) {
        if (actorIdState.ivs[i] != equivalentExplicitState.ivs[i] ||
            actorIdState.stats[i] != equivalentExplicitState.stats[i]) return 63;
    }

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

    const uint16_t damageSeed[] = {'m', 'o', 'v', 'e'};
    Pokerogue3DS::PokerogueRngAdapter damageRng;
    damageRng.sow(damageSeed, sizeof(damageSeed) / sizeof(damageSeed[0]));
    Pokerogue3DS::PokerogueRngAdapter damageExpectedRng = damageRng;
    const uint8_t accuracyRoll = static_cast<uint8_t>(damageExpectedRng.randSeedInt(100));
    const uint8_t criticalRoll = static_cast<uint8_t>(damageExpectedRng.randSeedInt(24));
    const uint8_t randomPercent = static_cast<uint8_t>(damageExpectedRng.randSeedIntRange(85, 100));
    const double expectedRawDamage = 5.2 * (criticalRoll == 0 ? 1.5 : 1.0) *
        (static_cast<double>(randomPercent) / 100.0);
    const uint32_t expectedDamage = expectedRawDamage < 1.0 ? 1 : static_cast<uint32_t>(expectedRawDamage);
    PokemonBattleState sameSpeciesTarget = state;
    PokemonMoveDamageRoll damageRoll{};
    if (Pokerogue3DS::resolveStandardPokemonMoveDamage(state, sameSpeciesTarget, 33, false,
        damageRng, damageRoll) != PokemonMoveDamageResult::Ok) return 43;
    if (!damageRoll.hit || damageRoll.accuracyRoll != accuracyRoll || !damageRoll.accuracyWasRolled ||
        damageRoll.criticalRoll != criticalRoll || damageRoll.critical != (criticalRoll == 0) ||
        damageRoll.randomDamagePercent != randomPercent || damageRoll.damage != expectedDamage) return 44;
    const auto actualDamageRngState = damageRng.state();
    const auto expectedDamageRngState = damageExpectedRng.state();
    if (actualDamageRngState.carry != expectedDamageRngState.carry || actualDamageRngState.s0 != expectedDamageRngState.s0 ||
        actualDamageRngState.s1 != expectedDamageRngState.s1 || actualDamageRngState.s2 != expectedDamageRngState.s2) return 45;

    PokerogueRngAdapter missRng;
    PokerogueRngAdapter missExpectedRng;
    uint16_t missSeed = 0;
    bool foundMissSeed = false;
    for (uint16_t candidate = 1; candidate < 256 && !foundMissSeed; ++candidate) {
        missRng.sow(&candidate, 1);
        missExpectedRng = missRng;
        if (missExpectedRng.randSeedInt(100) >= 50) { missSeed = candidate; foundMissSeed = true; }
    }
    if (!foundMissSeed) return 46;
    missRng.sow(&missSeed, 1);
    missExpectedRng = missRng;
    const uint8_t expectedMissRoll = static_cast<uint8_t>(missExpectedRng.randSeedInt(100));
    damageTarget = state;
    damageRoll = {};
    if (Pokerogue3DS::resolveStandardPokemonMoveDamage(state, damageTarget, 192, false,
        missRng, damageRoll) != PokemonMoveDamageResult::Ok || damageRoll.hit ||
        !damageRoll.accuracyWasRolled || damageRoll.accuracyRoll != expectedMissRoll || damageRoll.damage != 0) return 47;
    const auto actualMissState = missRng.state();
    const auto expectedMissState = missExpectedRng.state();
    if (actualMissState.carry != expectedMissState.carry || actualMissState.s0 != expectedMissState.s0 ||
        actualMissState.s1 != expectedMissState.s1 || actualMissState.s2 != expectedMissState.s2) return 48;

    PokerogueRngAdapter immuneRng;
    immuneRng.sow(damageSeed, sizeof(damageSeed) / sizeof(damageSeed[0]));
    const auto beforeImmuneState = immuneRng.state();
    damageTarget.speciesDex = 92;
    damageRoll = {};
    if (Pokerogue3DS::resolveStandardPokemonMoveDamage(state, damageTarget, 33, false,
        immuneRng, damageRoll) != PokemonMoveDamageResult::Ok || damageRoll.hit || damageRoll.damage != 0) return 49;
    const auto afterImmuneState = immuneRng.state();
    if (beforeImmuneState.carry != afterImmuneState.carry || beforeImmuneState.s0 != afterImmuneState.s0 ||
        beforeImmuneState.s1 != afterImmuneState.s1 || beforeImmuneState.s2 != afterImmuneState.s2) return 50;

    PokemonBattleState moveActor = state;
    PokemonBattleState oneHpTarget = state;
    oneHpTarget.hp = 1;
    PokerogueRngAdapter actionRng;
    actionRng.sow(damageSeed, sizeof(damageSeed) / sizeof(damageSeed[0]));
    PokerogueRngAdapter actionExpectedRng = actionRng;
    PokemonMoveDamageRoll actionExpectedRoll{};
    if (Pokerogue3DS::resolveStandardPokemonMoveDamage(moveActor, oneHpTarget, 33, false,
        actionExpectedRng, actionExpectedRoll) != PokemonMoveDamageResult::Ok) return 51;
    PokemonMoveActionResult actionResult{};
    if (Pokerogue3DS::useStandardPokemonMove(moveActor, oneHpTarget, 0, false, actionRng,
        actionResult) != PokemonMoveActionStatus::Ok || !actionResult.damageRoll.hit ||
        actionResult.damageRoll.damage != actionExpectedRoll.damage || actionResult.damageApplied != 1 ||
        !actionResult.targetFainted || oneHpTarget.hp != 0 || moveActor.moves[0].pp != 34) return 52;
    const auto actualActionState = actionRng.state();
    const auto expectedActionState = actionExpectedRng.state();
    if (actualActionState.carry != expectedActionState.carry || actualActionState.s0 != expectedActionState.s0 ||
        actualActionState.s1 != expectedActionState.s1 || actualActionState.s2 != expectedActionState.s2) return 53;

    PokemonBattleState noPpActor = state;
    noPpActor.moves[0].pp = 0;
    PokemonBattleState unchangedTarget = state;
    PokerogueRngAdapter noPpRng;
    noPpRng.sow(damageSeed, sizeof(damageSeed) / sizeof(damageSeed[0]));
    const auto beforeNoPpState = noPpRng.state();
    actionResult = {};
    actionResult.damageApplied = 123;
    if (Pokerogue3DS::useStandardPokemonMove(noPpActor, unchangedTarget, 0, false, noPpRng,
        actionResult) != PokemonMoveActionStatus::NoPp || actionResult.damageApplied != 123 ||
        unchangedTarget.hp != state.hp || noPpActor.moves[0].pp != 0) return 54;
    const auto afterNoPpState = noPpRng.state();
    if (beforeNoPpState.carry != afterNoPpState.carry || beforeNoPpState.s0 != afterNoPpState.s0 ||
        beforeNoPpState.s1 != afterNoPpState.s1 || beforeNoPpState.s2 != afterNoPpState.s2) return 55;

    PokemonBattleState missActor = state;
    missActor.moves[0] = {192, 1, 1}; // Zap Cannon: deliberately seeded to miss.
    PokemonBattleState missTarget = state;
    PokerogueRngAdapter actionMissRng;
    actionMissRng.sow(&missSeed, 1);
    actionResult = {};
    if (Pokerogue3DS::useStandardPokemonMove(missActor, missTarget, 0, false, actionMissRng,
        actionResult) != PokemonMoveActionStatus::Ok || actionResult.damageRoll.hit ||
        actionResult.damageApplied != 0 || actionResult.targetFainted || missTarget.hp != state.hp ||
        missActor.moves[0].pp != 0) return 56;

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

    // Pinned Pokemon constructor draws abilityIndex before id, then derives
    // IVs from that full-width ID and draws gender.
    rng.sow(seed, sizeof(seed) / sizeof(seed[0]));
    expected = rng;
    const auto* charizard = PokerogueContent::findSpeciesByDex(6);
    const uint8_t actorRegular = charizard->ability2 == charizard->ability1
        ? 0 : static_cast<uint8_t>(expected.randSeedInt(2));
    const bool actorHidden = expected.randSeedInt(256) == 0;
    const uint32_t actorId = expected.randSeedUint32();
    uint8_t actorExpectedIvs[6]{};
    Pokerogue3DS::derivePokemonIvsFromId(actorId, actorExpectedIvs);
    const double actorGenderRoll = expected.frac() * 100.0;
    Pokerogue3DS::PokemonActorIdentity actorIdentity{};
    if (Pokerogue3DS::generatePokemonActorIdentity(6, 256, rng, actorIdentity) !=
        Pokerogue3DS::PokemonActorIdentityResult::Ok) return 64;
    if (actorIdentity.pokemonId != actorId) return 68;
    if (actorIdentity.abilityIndex != (actorHidden ? 2 : actorRegular)) return 69;
    if (actorIdentity.gender != (actorGenderRoll <= 87.5 ? PokemonGender::Male : PokemonGender::Female)) return 70;
    for (uint8_t i = 0; i < 6; ++i) if (actorIdentity.ivs[i] != actorExpectedIvs[i]) return 66;
    const auto actorActualRng = rng.state();
    const auto actorExpectedRng = expected.state();
    if (actorActualRng.carry != actorExpectedRng.carry || actorActualRng.s0 != actorExpectedRng.s0 ||
        actorActualRng.s1 != actorExpectedRng.s1 || actorActualRng.s2 != actorExpectedRng.s2) return 67;
    return 0;
}

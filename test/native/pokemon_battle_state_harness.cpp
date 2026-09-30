#include "game/PokemonBattleState.hpp"
#include "game/PokemonFreshProfile.hpp"
#include "game/PokemonLevelMovePool.hpp"
#include "game/PokemonWildMovesetGenerator.hpp"
#include "game/PokemonStarterMoveset.hpp"
#include "game/PokemonTrainerAi.hpp"
#include "game/PokemonStatStageEffect.hpp"
#include "game/PokerogueTurnOrder.hpp"
#include "game/PokemonExperience.hpp"
#include "game/PokerogueRngAdapter.hpp"

using Pokerogue3DS::PokemonBattleInit;
using Pokerogue3DS::PokemonBattleInitResult;
using Pokerogue3DS::PokemonBattleState;
using Pokerogue3DS::PokemonActorIdentity;
using Pokerogue3DS::PokemonFormSelectionContext;
using Pokerogue3DS::PokemonNature;
using Pokerogue3DS::PokemonAbilitySelectionResult;
using Pokerogue3DS::PokemonGender;
using Pokerogue3DS::PokemonGenderSelectionResult;
using Pokerogue3DS::PokemonMoveDamageResult;
using Pokerogue3DS::PokemonMoveDamageRoll;
using Pokerogue3DS::PokemonMoveActionResult;
using Pokerogue3DS::PokemonMoveActionStatus;
using Pokerogue3DS::PokerogueRngAdapter;

extern "C" int runPokemonBattleStateChecks() {
    uint32_t awardedExperience = 99;
    if (Pokerogue3DS::pokemonSingleParticipantExperience(52.2, false, awardedExperience) !=
        Pokerogue3DS::PokemonExperienceResult::Ok || awardedExperience != 52) return 198;
    if (Pokerogue3DS::pokemonSingleParticipantExperience(52.2, true, awardedExperience) !=
        Pokerogue3DS::PokemonExperienceResult::Ok || awardedExperience != 78) return 199;
    if (Pokerogue3DS::pokemonSingleParticipantExperience(-1.0, true, awardedExperience) !=
        Pokerogue3DS::PokemonExperienceResult::Overflow || awardedExperience != 0) return 200;
    if (Pokerogue3DS::pokemonSingleParticipantExperience(4294967295.0, true, awardedExperience) !=
        Pokerogue3DS::PokemonExperienceResult::Overflow) return 201;
    const uint16_t trainerAiSeed[] = {'t', 'r', 'a', 'i', 'n', 'e', 'r'};
    PokerogueRngAdapter trainerAiRng;
    trainerAiRng.sow(trainerAiSeed, 7);
    const double tiedScores[3] = {10.0, 10.0, -1.0};
    const uint8_t tiedSlots[3] = {3, 1, 2};
    PokerogueRngAdapter expectedTrainerAiRng = trainerAiRng;
    const uint8_t expectedTrainerSlot = expectedTrainerAiRng.randSeedInt(100) < 50 ? 1 : 3;
    uint8_t selectedTrainerSlot = 0;
    if (!Pokerogue3DS::selectSmartTrainerMoveSlot(tiedScores, tiedSlots, 3,
            trainerAiRng, selectedTrainerSlot) || selectedTrainerSlot != expectedTrainerSlot)
        return 176;
    const auto sameTrainerRng = [](const Pokerogue3DS::PokerogueRngState& left,
                                   const Pokerogue3DS::PokerogueRngState& right) {
        return left.carry == right.carry && left.s0 == right.s0 &&
            left.s1 == right.s1 && left.s2 == right.s2;
    };
    if (!sameTrainerRng(trainerAiRng.state(), expectedTrainerAiRng.state())) return 177;
    const double separatedScores[2] = {8.0, -2.0};
    const uint8_t separatedSlots[2] = {2, 0};
    const auto beforeNoDraw = trainerAiRng.state();
    if (!Pokerogue3DS::selectSmartTrainerMoveSlot(separatedScores, separatedSlots, 2,
            trainerAiRng, selectedTrainerSlot) || selectedTrainerSlot != 2 ||
        !sameTrainerRng(trainerAiRng.state(), beforeNoDraw)) return 178;
    Pokerogue3DS::PokemonTrainerMatchupInput matchup{};
    matchup.usableAttackCount = 1;
    matchup.attackEffectiveness[0] = 1.5;
    matchup.hpRatio = 0.1;
    matchup.active = true;
    matchup.outspeeds = true;
    double matchupScore = 0;
    if (!Pokerogue3DS::calculateTrainerMatchupScore(matchup, matchupScore) ||
        matchupScore != 2.5) return 183;
    matchup.active = false;
    if (!Pokerogue3DS::calculateTrainerMatchupScore(matchup, matchupScore) ||
        matchupScore != 0.3125) return 184;
    bool wantsSwitch = false;
    if (!Pokerogue3DS::shouldTrainerSwitch(1.0, 3.0, 0, false, wantsSwitch) ||
        !wantsSwitch) return 185;
    if (!Pokerogue3DS::shouldTrainerSwitch(1.0, 3.0, 1, false, wantsSwitch) ||
        wantsSwitch) return 186;
    const double reserveScores[3] = {2.0, 3.0, 3.0};
    const uint8_t reserveIndexes[3] = {1, 4, 2};
    PokerogueRngAdapter summonRng = trainerAiRng;
    PokerogueRngAdapter expectedSummonRng = summonRng;
    const uint8_t expectedSummon = expectedSummonRng.randSeedInt(2) ? 2 : 4;
    uint8_t nextSummon = 0;
    if (!Pokerogue3DS::selectTrainerSummonIndex(reserveScores, reserveIndexes, 3,
            summonRng, nextSummon) || nextSummon != expectedSummon ||
        !sameTrainerRng(summonRng.state(), expectedSummonRng.state())) return 187;
    double plainAttackScore = 0;
    if (!Pokerogue3DS::calculatePlainAttackAiScore(1.0, 20, 20, 40, 100,
            true, plainAttackScore) || plainAttackScore != 12.0) return 188;
    if (!Pokerogue3DS::calculatePlainAttackAiScore(1.0, 20, 20, 50, 80,
            false, plainAttackScore) || plainAttackScore != 8.0) return 189;
    if (!Pokerogue3DS::calculatePlainAttackAiScore(0.0, 20, 20, 40, 100,
            true, plainAttackScore) || plainAttackScore != -20.0) return 190;
    if (!Pokerogue3DS::calculatePlainAttackAiScore(2.0, 20, 10, 40, 100,
            false, plainAttackScore) || plainAttackScore != 24.0) return 191;
    const uint8_t koInputSlots[3] = {3, 0, 2};
    const uint32_t koDamage[3] = {9, 10, 12};
    uint8_t koSlots[4]{};
    uint8_t koCount = 0;
    if (!Pokerogue3DS::filterEnemyKoMoveSlots(koInputSlots, koDamage, 3, 10,
            koSlots, koCount) || koCount != 2 || koSlots[0] != 0 || koSlots[1] != 2)
        return 192;
    if (!Pokerogue3DS::filterEnemyKoMoveSlots(koInputSlots, koDamage, 3, 13,
            koSlots, koCount) || koCount != 3 || koSlots[0] != 3 || koSlots[2] != 2)
        return 193;
    if (Pokerogue3DS::filterEnemyKoMoveSlots(koInputSlots, koDamage, 3, 0,
            koSlots, koCount) || koCount) return 194;
    uint8_t derivedIvs[6]{};
    Pokerogue3DS::derivePokemonIvsFromId(0xFFFFFFFFu, derivedIvs);
    for (uint8_t iv : derivedIvs) if (iv != 31) return 57;
    Pokerogue3DS::derivePokemonIvsFromId(0, derivedIvs);
    for (uint8_t iv : derivedIvs) if (iv != 0) return 58;
    Pokerogue3DS::derivePokemonIvsFromId(0x12345678u, derivedIvs);
    const uint8_t expectedDerivedIvs[6] = {9, 3, 8, 21, 19, 24};
    for (uint8_t i = 0; i < 6; ++i) if (derivedIvs[i] != expectedDerivedIvs[i]) return 59;

    const int8_t expectedRaised[25] = {
        -1, 1, 1, 1, 1, 2, -1, 2, 2, 2, 5, 5, -1, 5, 5,
        3, 3, 3, -1, 3, 4, 4, 4, 4, -1,
    };
    const int8_t expectedLowered[25] = {
        -1, 2, 5, 3, 4, 1, -1, 5, 3, 4, 1, 2, -1, 3, 4,
        1, 2, 5, -1, 4, 1, 2, 5, 3, -1,
    };
    for (uint8_t i = 0; i < 25; ++i) {
        Pokerogue3DS::PokemonNatureModifiers nature{};
        if (!Pokerogue3DS::getPokemonNatureModifiers(static_cast<Pokerogue3DS::PokemonNature>(i), nature) ||
            nature.raisedStat != expectedRaised[i] || nature.loweredStat != expectedLowered[i]) return 74;
    }
    Pokerogue3DS::PokemonNatureModifiers invalidNature{};
    if (Pokerogue3DS::getPokemonNatureModifiers(Pokerogue3DS::PokemonNature::Unspecified, invalidNature)) return 75;
    const uint16_t natureSeed[] = {'n', 'a', 't', 'u', 'r', 'e'};
    PokerogueRngAdapter natureRng;
    natureRng.sow(natureSeed, sizeof(natureSeed) / sizeof(natureSeed[0]));
    PokerogueRngAdapter natureExpectedRng = natureRng;
    const auto expectedNature = static_cast<Pokerogue3DS::PokemonNature>(natureExpectedRng.randSeedInt(25));
    if (Pokerogue3DS::selectPokemonNature(natureRng) != expectedNature) return 76;
    const auto natureActualState = natureRng.state();
    const auto natureExpectedState = natureExpectedRng.state();
    if (natureActualState.carry != natureExpectedState.carry || natureActualState.s0 != natureExpectedState.s0 ||
        natureActualState.s1 != natureExpectedState.s1 || natureActualState.s2 != natureExpectedState.s2) return 77;

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

    PokemonBattleInit matchupOpponentInput{};
    matchupOpponentInput.speciesDex = 7;
    matchupOpponentInput.level = 5;
    matchupOpponentInput.abilityId = 67;
    matchupOpponentInput.gender = PokemonGender::Male;
    PokemonBattleState matchupOpponent{};
    if (Pokerogue3DS::initializePokemonBattleState(matchupOpponentInput, matchupOpponent) !=
        PokemonBattleInitResult::Ok) return 195;
    PokemonBattleState matchupActor = state;
    matchupActor.moves[0].moveId = 22; // Canonical Vine Whip against Squirtle.
    Pokerogue3DS::PokemonTrainerMatchupInput resolvedMatchup{};
    if (!Pokerogue3DS::buildBaselineTrainerMatchupInput(matchupActor, matchupOpponent,
            10, 9, true, resolvedMatchup) || resolvedMatchup.usableAttackCount != 1 ||
        resolvedMatchup.attackEffectiveness[0] != 3.0 ||
        resolvedMatchup.defensiveEffectiveness[0] != 0.5 || !resolvedMatchup.outspeeds)
        return 196;
    matchupActor.moves[0].pp = 0;
    if (!Pokerogue3DS::buildBaselineTrainerMatchupInput(matchupActor, matchupOpponent,
            9, 10, false, resolvedMatchup) || resolvedMatchup.usableAttackCount != 0 ||
        resolvedMatchup.outspeeds || resolvedMatchup.active) return 197;
    PokemonBattleState replacementParty[3] = {state, state, state};
    replacementParty[0].hp = 0;
    replacementParty[1].hp = 0;
    uint8_t replacementIndex = 99;
    PokerogueRngAdapter replacementRng;
    replacementRng.sow(trainerAiSeed, sizeof(trainerAiSeed) / sizeof(trainerAiSeed[0]));
    if (!Pokerogue3DS::selectBaselineTrainerReplacement(replacementParty, 3, 0,
            matchupOpponent, replacementRng, replacementIndex) || replacementIndex != 2)
        return 202;
    replacementParty[2].hp = 0;
    if (Pokerogue3DS::selectBaselineTrainerReplacement(replacementParty, 3, 0,
            matchupOpponent, replacementRng, replacementIndex)) return 203;
    const double reserveScores[] = {6.0};
    const uint8_t reserveIndexes[] = {1};
    Pokerogue3DS::TrainerSwitchDecision switchDecision{};
    if (!Pokerogue3DS::resolveTrainerSwitchDecision(2.0, reserveScores, reserveIndexes,
            1, 0, false, false, false, replacementRng, switchDecision) ||
        !switchDecision.switchPokemon || switchDecision.partyIndex != 1 ||
        switchDecision.nextSwitchCounter != 1) return 204;
    if (!Pokerogue3DS::resolveTrainerSwitchDecision(2.0, reserveScores, reserveIndexes,
            1, 1, false, false, false, replacementRng, switchDecision) ||
        switchDecision.switchPokemon || switchDecision.nextSwitchCounter) return 205;
    if (!Pokerogue3DS::resolveTrainerSwitchDecision(2.0, reserveScores, reserveIndexes,
            1, 2, true, true, false, replacementRng, switchDecision) ||
        switchDecision.switchPokemon || switchDecision.nextSwitchCounter != 1) return 206;
    if (!Pokerogue3DS::resolveTrainerSwitchDecision(2.0, reserveScores, reserveIndexes,
            1, 2, true, false, true, replacementRng, switchDecision) ||
        switchDecision.switchPokemon || switchDecision.nextSwitchCounter != 1) return 207;
    PokemonBattleState staged = state;
    double stageMultiplier = 0;
    if (!Pokerogue3DS::setPokemonStatStage(staged, 1, -9) || staged.statStages[0] != -6 ||
        !Pokerogue3DS::pokemonStatStageMultiplier(staged, 1, false, stageMultiplier) ||
        stageMultiplier != 0.25) return 208;
    if (!Pokerogue3DS::pokemonStatStageMultiplier(staged, 1, true, stageMultiplier) ||
        stageMultiplier != 1.0) return 209;
    if (!Pokerogue3DS::setPokemonStatStage(staged, 2, 9) ||
        !Pokerogue3DS::pokemonStatStageMultiplier(staged, 2, true, stageMultiplier) ||
        stageMultiplier != 1.0 || Pokerogue3DS::setPokemonStatStage(staged, 0, 1)) return 210;
    if (!Pokerogue3DS::setPokemonStatStage(staged, 6, 6) ||
        !Pokerogue3DS::setPokemonStatStage(matchupOpponent, 7, -6) ||
        !Pokerogue3DS::pokemonAccuracyStageMultiplier(staged, matchupOpponent, stageMultiplier) ||
        stageMultiplier != 3.0) return 211;
    const uint16_t retainedHp = staged.hp;
    Pokerogue3DS::resetPokemonStatStages(staged);
    for (int8_t stage : staged.statStages) if (stage) return 212;
    if (staged.hp != retainedHp || staged.moves[0].pp != state.moves[0].pp) return 213;
    double neutralStageDamage = 0, reducedStageDamage = 0, criticalStageDamage = 0;
    PokemonBattleState stagedDefender = state;
    if (Pokerogue3DS::calculatePokemonBaseDamage(state, stagedDefender, 33,
            neutralStageDamage) != Pokerogue3DS::PokemonBaseDamageResult::Ok ||
        !Pokerogue3DS::setPokemonStatStage(staged, 1, -6) ||
        !Pokerogue3DS::setPokemonStatStage(stagedDefender, 2, 6) ||
        Pokerogue3DS::calculatePokemonBaseDamage(staged, stagedDefender, 33,
            reducedStageDamage) != Pokerogue3DS::PokemonBaseDamageResult::Ok ||
        reducedStageDamage >= neutralStageDamage ||
        Pokerogue3DS::calculatePokemonBaseDamage(staged, stagedDefender, 33,
            criticalStageDamage, true) != Pokerogue3DS::PokemonBaseDamageResult::Ok ||
        criticalStageDamage != neutralStageDamage) return 214;
    PokemonBattleState fastActor = state, slowActor = state;
    fastActor.stats[5] = 20;
    slowActor.stats[5] = 10;
    if (!Pokerogue3DS::setPokemonStatStage(fastActor, 5, -6) ||
        Pokerogue3DS::resolveBaselineFirstMover(fastActor, slowActor, 33, 33,
            trainerAiSeed, sizeof(trainerAiSeed) / sizeof(trainerAiSeed[0]), 5, 1) !=
            Pokerogue3DS::BaselineFirstMover::Enemy) return 215;
    uint32_t effectiveSpeed = 0;
    fastActor.stats[5] = 65535;
    if (!Pokerogue3DS::setPokemonStatStage(fastActor, 5, 6) ||
        !Pokerogue3DS::pokemonBaselineEffectiveStat(fastActor, 5, false, effectiveSpeed) ||
        effectiveSpeed != 262140) return 216;
    bool sawGrowlEffect = false, sawSwordsDanceEffect = false;
    for (const auto& effect : PokerogueContent::kMoveStatStageEffects) {
        if (!PokerogueContent::findMoveById(effect.moveId) || !effect.statMask ||
            effect.statMask > 127 || effect.stages < -6 || effect.stages > 6) return 217;
        if (effect.moveId == 45) {
            if (effect.statMask != 1 || effect.stages != -1 || effect.selfTarget) return 218;
            sawGrowlEffect = true;
        }
        if (effect.moveId == 14) {
            if (effect.statMask != 1 || effect.stages != 2 || !effect.selfTarget) return 219;
            sawSwordsDanceEffect = true;
        }
    }
    if (!sawGrowlEffect || !sawSwordsDanceEffect) return 220;
    PokemonBattleState effectRecipient = state;
    Pokerogue3DS::PokemonStatStageEffectPolicy stagePolicy{};
    Pokerogue3DS::PokemonStatStageEffectEvent stageEvent{};
    const PokerogueContent::MoveStatStageEffect growlEffect{45, 1, -1, false};
    if (Pokerogue3DS::applyPokemonStatStageEffect(effectRecipient, growlEffect, stagePolicy,
            replacementRng, stageEvent) != Pokerogue3DS::PokemonStatStageEffectResult::UnresolvedPolicy ||
        effectRecipient.statStages[0]) return 221;
    stagePolicy.resolved = true;
    if (Pokerogue3DS::applyPokemonStatStageEffect(effectRecipient, growlEffect, stagePolicy,
            replacementRng, stageEvent) != Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
        effectRecipient.statStages[0] != -1 || stageEvent.changes[0] != -1 ||
        stageEvent.changedStatMask != 1) return 222;
    stagePolicy.stageMultiplier = -1;
    if (Pokerogue3DS::applyPokemonStatStageEffect(effectRecipient, growlEffect, stagePolicy,
            replacementRng, stageEvent) != Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
        effectRecipient.statStages[0] != 0 || stageEvent.changes[0] != 1) return 223;
    stagePolicy.cancelledStatMask = 1;
    if (Pokerogue3DS::applyPokemonStatStageEffect(effectRecipient, growlEffect, stagePolicy,
            replacementRng, stageEvent) != Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
        stageEvent.changedStatMask || effectRecipient.statStages[0]) return 224;
    const auto* simpleStageProfile = PokerogueContent::findAbilityStatStageProfile(86);
    const auto* contraryStageProfile = PokerogueContent::findAbilityStatStageProfile(126);
    const auto* clearBodyStageProfile = PokerogueContent::findAbilityStatStageProfile(29);
    const auto* metalBodyStageProfile = PokerogueContent::findAbilityStatStageProfile(230);
    if (!simpleStageProfile || simpleStageProfile->multiplier != 2 ||
        !contraryStageProfile || contraryStageProfile->multiplier != -1 ||
        !clearBodyStageProfile || clearBodyStageProfile->protectedMask != 127 ||
        !clearBodyStageProfile->ignorable || !metalBodyStageProfile ||
        metalBodyStageProfile->protectedMask != 127 || metalBodyStageProfile->ignorable)
        return 225;
    const Pokerogue3DS::ResolvedStatStageAbilityComponent clearBodyComponent[] = {
        {clearBodyStageProfile, true}
    };
    Pokerogue3DS::PokemonStatStageEffectPolicy protectedPolicy{};
    if (!Pokerogue3DS::composePokemonStatStageAbilityPolicy(growlEffect,
            clearBodyComponent, 1, false, protectedPolicy) ||
        protectedPolicy.cancelledStatMask != 1 || protectedPolicy.resolved) return 226;
    const Pokerogue3DS::ResolvedStatStageAbilityComponent contraryAndProtection[] = {
        {contraryStageProfile, true}, {clearBodyStageProfile, true}
    };
    protectedPolicy = {};
    if (!Pokerogue3DS::composePokemonStatStageAbilityPolicy(growlEffect,
            contraryAndProtection, 2, false, protectedPolicy) ||
        protectedPolicy.stageMultiplier != -1 || protectedPolicy.cancelledStatMask) return 227;
    protectedPolicy = {};
    if (!Pokerogue3DS::composePokemonStatStageAbilityPolicy(growlEffect,
            contraryAndProtection, 2, true, protectedPolicy) ||
        protectedPolicy.stageMultiplier != 1 || protectedPolicy.cancelledStatMask != 1) return 228;
    PokemonBattleState growlUser = state, growlTarget = state;
    growlUser.moves[0].moveId = 45;
    growlUser.moves[0].pp = 40;
    Pokerogue3DS::PokemonStatStageMovePolicy supportPolicy{};
    Pokerogue3DS::PokemonStatStageMoveEvent supportEvent{};
    if (Pokerogue3DS::usePokemonStatStageStatusMove(growlUser, growlTarget, 0,
            supportPolicy, replacementRng, supportEvent) !=
            Pokerogue3DS::PokemonStatStageEffectResult::UnresolvedPolicy ||
        growlUser.moves[0].pp != 40) return 229;
    supportPolicy.hitPolicyResolved = supportPolicy.stagePolicy.resolved = true;
    if (Pokerogue3DS::usePokemonStatStageStatusMove(growlUser, growlTarget, 0,
            supportPolicy, replacementRng, supportEvent) !=
            Pokerogue3DS::PokemonStatStageEffectResult::Ok || !supportEvent.hit ||
        !supportEvent.accuracyRolled || growlUser.moves[0].pp != 39 ||
        growlTarget.statStages[0] != -1 || growlTarget.hp != state.hp) return 230;
    supportPolicy.blockedBeforeAccuracy = true;
    if (Pokerogue3DS::usePokemonStatStageStatusMove(growlUser, growlTarget, 0,
            supportPolicy, replacementRng, supportEvent) !=
            Pokerogue3DS::PokemonStatStageEffectResult::Ok || supportEvent.hit ||
        supportEvent.accuracyRolled || growlUser.moves[0].pp != 38 ||
        growlTarget.statStages[0] != -1) return 231;
    double supportBenefit = 0;
    PokemonBattleState benefitTarget = state;
    if (!Pokerogue3DS::calculateStatStageTargetBenefit(state, benefitTarget,
            growlEffect, supportBenefit) || supportBenefit != -2) return 232;
    benefitTarget.statStages[0] = -6;
    if (!Pokerogue3DS::calculateStatStageTargetBenefit(state, benefitTarget,
            growlEffect, supportBenefit) || supportBenefit != 2) return 233;
    const PokerogueContent::MoveStatStageEffect swordsDanceEffect{14, 1, 2, true};
    if (!Pokerogue3DS::calculateStatStageTargetBenefit(state, state,
            swordsDanceEffect, supportBenefit) || supportBenefit != 6) return 234;
    PokemonBattleState specialOnly = state;
    specialOnly.moves[0].moveId = 55;
    const PokerogueContent::MoveStatStageEffect specialBoostEffect{417, 4, 2, true};
    if (!Pokerogue3DS::calculateStatStageTargetBenefit(specialOnly, specialOnly,
            specialBoostEffect, supportBenefit) || supportBenefit != 0) return 235;
    if (!Pokerogue3DS::calculateCanonicalStatStageStatusAiScore(state, state, 45,
            supportBenefit) || supportBenefit != 2 ||
        !Pokerogue3DS::calculateCanonicalStatStageStatusAiScore(state, state, 14,
            supportBenefit) || supportBenefit != 6) return 236;
    const uint8_t mixedSlots[] = {0, 1};
    const uint32_t mixedDamage[] = {0, 10};
    uint8_t mixedFiltered[4]{}, mixedCount = 0;
    if (!Pokerogue3DS::filterEnemyKoMoveSlots(mixedSlots, mixedDamage, 2, 10,
            mixedFiltered, mixedCount) || mixedCount != 1 || mixedFiltered[0] != 1) return 237;
    if (!Pokerogue3DS::filterEnemyKoMoveSlots(mixedSlots, mixedDamage, 2, 11,
            mixedFiltered, mixedCount) || mixedCount != 2 || mixedFiltered[0] != 0) return 238;
    const PokerogueContent::AbilityStatStageProfile* mirrorArmor = nullptr;
    for (const auto& profile : PokerogueContent::kAbilityStatStageProfiles)
        if (std::strcmp(profile.sourceSymbol, "AbilityId.MIRROR_ARMOR") == 0) mirrorArmor = &profile;
    if (!mirrorArmor || !mirrorArmor->reflectDrops) return 239;
    const Pokerogue3DS::ResolvedStatStageAbilityComponent mirrorComponent[] = {{mirrorArmor, true}};
    protectedPolicy = {};
    if (!Pokerogue3DS::composePokemonStatStageAbilityPolicy(growlEffect, mirrorComponent,
            1, false, protectedPolicy, true) || protectedPolicy.reflectedStatMask != 1 ||
        protectedPolicy.cancelledStatMask != 1) return 240;
    protectedPolicy.resolved = true;
    effectRecipient = state;
    if (Pokerogue3DS::applyPokemonStatStageEffect(effectRecipient, growlEffect, protectedPolicy,
            replacementRng, stageEvent) != Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
        effectRecipient.statStages[0] || stageEvent.reflectedStatMask != 1 ||
        stageEvent.reflectedStages != -1) return 241;
    PokemonBattleState reflectedSource = state;
    Pokerogue3DS::PokemonStatStageEffectPolicy reflectedPolicy{};
    Pokerogue3DS::PokemonStatStageEffectEvent reflectedEvent{};
    reflectedPolicy.resolved = true;
    reflectedPolicy.stageMultiplier = 2;
    if (Pokerogue3DS::applyReflectedPokemonStatStages(reflectedSource, stageEvent,
            reflectedPolicy, reflectedEvent) != Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
        reflectedSource.statStages[0] != -2 || reflectedEvent.changes[0] != -2 ||
        reflectedEvent.reflectedStatMask) return 242;
    reflectedPolicy.reflectedStatMask = 1;
    if (Pokerogue3DS::applyReflectedPokemonStatStages(reflectedSource, stageEvent,
            reflectedPolicy, reflectedEvent) != Pokerogue3DS::PokemonStatStageEffectResult::UnresolvedPolicy ||
        reflectedSource.statStages[0] != -2) return 243;
    reflectedPolicy.reflectedStatMask = 0;
    reflectedPolicy.stageMultiplier = -1;
    if (Pokerogue3DS::applyReflectedPokemonStatStages(reflectedSource, stageEvent,
            reflectedPolicy, reflectedEvent) != Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
        reflectedSource.statStages[0] != -1 || reflectedEvent.changes[0] != 1) return 244;
    const PokerogueContent::AbilityStatStageReaction* defiantReaction = nullptr;
    for (const auto& reaction : PokerogueContent::kAbilityStatStageReactions)
        if (std::strcmp(reaction.sourceSymbol, "AbilityId.DEFIANT") == 0) defiantReaction = &reaction;
    if (!defiantReaction || defiantReaction->stat != 1) return 245;
    Pokerogue3DS::PokemonStatStageEffectEvent requestedDrop{};
    requestedDrop.triggered = true;
    requestedDrop.processedStatMask = 3;
    requestedDrop.requestedStages = -1;
    Pokerogue3DS::PokemonStatStageReactionRequest reactionRequest{};
    if (!Pokerogue3DS::planPokemonStatStageDropReaction(*defiantReaction, requestedDrop,
            false, reactionRequest) || reactionRequest.stat != 1 || reactionRequest.stages != 4)
        return 246; // Source uses requested changes, even if applied changes are capped.
    if (!Pokerogue3DS::planPokemonStatStageDropReaction(*defiantReaction, requestedDrop,
            true, reactionRequest) || reactionRequest.stages) return 247;
    requestedDrop.processedStatMask = 0;
    if (!Pokerogue3DS::planPokemonStatStageDropReaction(*defiantReaction, requestedDrop,
            false, reactionRequest) || reactionRequest.stages) return 248;
    PokemonBattleState reactionActor = state;
    reactionActor.statStages[0] = -6;
    requestedDrop.processedStatMask = 3;
    Pokerogue3DS::PokemonStatStageEffectPolicy boostPolicy{};
    boostPolicy.resolved = true;
    if (Pokerogue3DS::applyPokemonStatStageDropReaction(reactionActor, *defiantReaction,
            requestedDrop, false, boostPolicy, reflectedEvent) !=
            Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
        reactionActor.statStages[0] != -2 || reflectedEvent.changes[0] != 4) return 249;
    boostPolicy.stageMultiplier = 2;
    if (Pokerogue3DS::applyPokemonStatStageDropReaction(reactionActor, *defiantReaction,
            requestedDrop, false, boostPolicy, reflectedEvent) !=
            Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
        reactionActor.statStages[0] != 6 || reflectedEvent.changes[0] != 8) return 250;
    if (Pokerogue3DS::applyPokemonStatStageDropReaction(reactionActor, *defiantReaction,
            requestedDrop, true, boostPolicy, reflectedEvent) !=
            Pokerogue3DS::PokemonStatStageEffectResult::Ok || reflectedEvent.triggered) return 251;
    growlUser = state;
    growlTarget = state;
    growlUser.moves[0].moveId = 45;
    growlUser.moves[0].pp = 40;
    Pokerogue3DS::PokemonStatStageCommandPolicy commandPolicy{};
    commandPolicy.postChangePoliciesResolved = true;
    commandPolicy.move.hitPolicyResolved = true;
    commandPolicy.move.stagePolicy.resolved = true;
    commandPolicy.recipientReactions[0] = defiantReaction;
    Pokerogue3DS::PokemonStatStageCommandEvent commandEvent{};
    const auto beforeCommandRng = replacementRng.state();
    if (Pokerogue3DS::usePokemonStatStageStatusCommand(growlUser, growlTarget, 0,
            commandPolicy, replacementRng, commandEvent) !=
            Pokerogue3DS::PokemonStatStageEffectResult::UnresolvedPolicy ||
        growlUser.moves[0].pp != 40 || growlTarget.statStages[0] ||
        replacementRng.state().s0 != beforeCommandRng.s0 ||
        replacementRng.state().s1 != beforeCommandRng.s1 ||
        replacementRng.state().s2 != beforeCommandRng.s2) return 252;
    commandPolicy.recipientReaction.resolved = true;
    if (Pokerogue3DS::usePokemonStatStageStatusCommand(growlUser, growlTarget, 0,
            commandPolicy, replacementRng, commandEvent) !=
            Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
        growlUser.moves[0].pp != 39 || growlTarget.statStages[0] != 1 ||
        commandEvent.move.stages.changes[0] != -1 ||
        commandEvent.recipientReactions[0].changes[0] != 2) return 253;
    const PokerogueContent::AbilityStatStageProfile* opportunist = nullptr;
    for (const auto& profile : PokerogueContent::kAbilityStatStageProfiles)
        if (std::strcmp(profile.sourceSymbol, "AbilityId.OPPORTUNIST") == 0) opportunist = &profile;
    if (!opportunist || !opportunist->copiesRaises) return 254;
    PokemonBattleState copyingActor = state;
    PokemonStatStageEffectEvent requestedRaise{};
    requestedRaise.triggered = true;
    requestedRaise.processedStatMask = 1;
    requestedRaise.requestedStages = 2;
    boostPolicy.stageMultiplier = 1;
    if (Pokerogue3DS::applyPokemonCopiedStatStageRaise(copyingActor, requestedRaise,
            false, boostPolicy, reflectedEvent) != Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
        copyingActor.statStages[0] != 2) return 255;
    if (Pokerogue3DS::applyPokemonCopiedStatStageRaise(copyingActor, requestedRaise,
            true, boostPolicy, reflectedEvent) != Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
        copyingActor.statStages[0] != 2 || reflectedEvent.triggered) return 256;
    PokemonBattleState herbHolder = state;
    herbHolder.statStages[0] = -3;
    herbHolder.statStages[1] = 2;
    uint8_t herbStack = 2;
    Pokerogue3DS::PokemonNegativeStageResetItemEvent herbEvent{};
    if (Pokerogue3DS::applyPokemonNegativeStageResetItem(herbHolder, "WHITE_HERB",
            herbHolder.pokemonId, herbStack, true, herbEvent) !=
            Pokerogue3DS::PokemonStatStageEffectResult::Ok || herbEvent.consumed ||
        herbHolder.statStages[0] != -3 || herbStack != 2) return 257;
    if (Pokerogue3DS::applyPokemonNegativeStageResetItem(herbHolder, "WHITE_HERB",
            herbHolder.pokemonId, herbStack, false, herbEvent) !=
            Pokerogue3DS::PokemonStatStageEffectResult::Ok || !herbEvent.consumed ||
        herbEvent.changes[0] != 3 || herbHolder.statStages[0] ||
        herbHolder.statStages[1] != 2 || herbStack != 1) return 258;
    if (Pokerogue3DS::applyPokemonNegativeStageResetItem(herbHolder, "WHITE_HERB",
            herbHolder.pokemonId, herbStack, false, herbEvent) !=
            Pokerogue3DS::PokemonStatStageEffectResult::Ok || herbEvent.consumed || herbStack != 1)
        return 259;
    PokemonBattleState lowHpAttacker = state;
    lowHpAttacker.abilityId = 65; // Imported Overgrow.
    lowHpAttacker.maxHp = 100;
    lowHpAttacker.hp = 34;
    double aboveThresholdDamage = 0, lowHpDamage = 0;
    if (Pokerogue3DS::calculatePokemonBaseDamage(lowHpAttacker, state, 22,
            aboveThresholdDamage) != Pokerogue3DS::PokemonBaseDamageResult::Ok) return 260;
    lowHpAttacker.hp = 33;
    if (Pokerogue3DS::calculatePokemonBaseDamage(lowHpAttacker, state, 22,
            lowHpDamage) != Pokerogue3DS::PokemonBaseDamageResult::Ok ||
        lowHpDamage != (aboveThresholdDamage - 2.0) * 1.5 + 2.0) return 261;
    lowHpAttacker.maxHp = 3;
    lowHpAttacker.hp = 1;
    if (Pokerogue3DS::calculatePokemonBaseDamage(lowHpAttacker, state, 22,
            lowHpDamage) != Pokerogue3DS::PokemonBaseDamageResult::Ok ||
        lowHpDamage != aboveThresholdDamage) return 262;
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

    const auto expectedActorNature = static_cast<Pokerogue3DS::PokemonNature>(expected.randSeedInt(25));
    Pokerogue3DS::generatePokemonActorNature(actorIdentity, rng);
    if (actorIdentity.nature != expectedActorNature) return 78;
    const auto actorNatureActualRng = rng.state();
    const auto actorNatureExpectedRng = expected.state();
    if (actorNatureActualRng.carry != actorNatureExpectedRng.carry ||
        actorNatureActualRng.s0 != actorNatureExpectedRng.s0 ||
        actorNatureActualRng.s1 != actorNatureExpectedRng.s1 ||
        actorNatureActualRng.s2 != actorNatureExpectedRng.s2) return 79;

    PokemonBattleInit actorBattleInput{};
    actorBattleInput.speciesDex = 6;
    actorBattleInput.level = 12;
    actorBattleInput.moveCount = 1;
    actorBattleInput.moveIds[0] = 33;
    actorBattleInput.nature = Pokerogue3DS::PokemonNature::Hardy;
    PokemonBattleState actorBattleState{};
    if (Pokerogue3DS::initializePokemonBattleStateForActor(
            actorBattleInput, actorIdentity, actorBattleState) != PokemonBattleInitResult::Ok) return 71;
    if (actorBattleState.pokemonId != actorIdentity.pokemonId || actorBattleState.nature != actorIdentity.nature ||
        actorBattleState.abilityId != charizard->ability1 || actorBattleState.gender != actorIdentity.gender ||
        !actorBattleState.ivsWereDerivedFromPokemonId || actorBattleState.level != actorBattleInput.level ||
        actorBattleState.moves[0].moveId != 33) return 72;
    PokemonBattleInit explicitNatureInput = actorBattleInput;
    explicitNatureInput.pokemonId = actorIdentity.pokemonId;
    explicitNatureInput.deriveIvsFromPokemonId = true;
    explicitNatureInput.gender = actorIdentity.gender;
    explicitNatureInput.abilityId = charizard->ability1;
    explicitNatureInput.nature = actorIdentity.nature;
    PokemonBattleState explicitNatureState{};
    if (Pokerogue3DS::initializePokemonBattleState(explicitNatureInput, explicitNatureState) !=
        PokemonBattleInitResult::Ok) return 80;
    for (uint8_t i = 0; i < 6; ++i) {
        if (actorBattleState.ivs[i] != actorIdentity.ivs[i] ||
            actorBattleState.stats[i] != explicitNatureState.stats[i]) return 73;
    }

    const auto dexFor = [](const char* id) -> uint16_t {
        for (const auto& species : PokerogueContent::kSpecies) {
            const char* a = species.id;
            const char* b = id;
            while (*a && *b && *a == *b) { ++a; ++b; }
            if (*a == *b) return species.dex;
        }
        return 0;
    };
    const auto formAt = [](const char* speciesId, std::size_t ordinal) -> const char* {
        for (const auto& form : PokerogueContent::kForms) {
            const char* a = form.speciesId;
            const char* b = speciesId;
            while (*a && *b && *a == *b) { ++a; ++b; }
            if (*a == *b) {
                if (!ordinal) return form.id;
                --ordinal;
            }
        }
        return nullptr;
    };
    const uint16_t formSeed[] = {'f', 'o', 'r', 'm'};
    PokerogueRngAdapter formRng;
    formRng.sow(formSeed, sizeof(formSeed) / sizeof(formSeed[0]));
    Pokerogue3DS::PokemonActorIdentity formActor{};
    Pokerogue3DS::PokemonFormSelectionContext formContext{};
    formActor.gender = PokemonGender::Female;
    if (Pokerogue3DS::selectPokemonActorForm(dexFor("meowstic"), formContext, formRng, formActor) !=
        Pokerogue3DS::PokemonFormSelectionResult::Ok ||
        formActor.formId != formAt("meowstic", 1)) return 81;
    formActor.gender = PokemonGender::Male;
    if (Pokerogue3DS::selectPokemonActorForm(dexFor("meowstic"), formContext, formRng, formActor) !=
        Pokerogue3DS::PokemonFormSelectionResult::Ok ||
        formActor.formId != formAt("meowstic", 0)) return 82;

    PokerogueRngAdapter formExpectedRng;
    formExpectedRng.sow(formSeed, sizeof(formSeed) / sizeof(formSeed[0]));
    const auto expectedUnownIndex = formExpectedRng.randSeedInt(28);
    if (Pokerogue3DS::selectPokemonActorForm(dexFor("unown"), formContext, formRng, formActor) !=
        Pokerogue3DS::PokemonFormSelectionResult::Ok) return 83;
    if (formActor.formId != formAt("unown", static_cast<std::size_t>(expectedUnownIndex))) return 84;
    const auto actualFormRngState = formRng.state();
    const auto expectedFormRngState = formExpectedRng.state();
    if (actualFormRngState.carry != expectedFormRngState.carry || actualFormRngState.s0 != expectedFormRngState.s0 ||
        actualFormRngState.s1 != expectedFormRngState.s1 || actualFormRngState.s2 != expectedFormRngState.s2) return 85;

    formContext = {};
    formContext.biomeId = "beach";
    if (Pokerogue3DS::selectPokemonActorForm(dexFor("wormadam"), formContext, formRng, formActor) !=
        Pokerogue3DS::PokemonFormSelectionResult::Ok ||
        formActor.formId != formAt("wormadam", 1)) return 86;
    formContext.trainerBattle = true;
    formContext.trainerSpecialtyType = "STEEL";
    if (Pokerogue3DS::selectPokemonActorForm(dexFor("wormadam"), formContext, formRng, formActor) !=
        Pokerogue3DS::PokemonFormSelectionResult::Ok ||
        formActor.formId != formAt("wormadam", 2)) return 87;
    formContext = {};
    formContext.timeOfDay = "DUSK";
    if (Pokerogue3DS::selectPokemonActorForm(dexFor("lycanroc"), formContext, formRng, formActor) !=
        Pokerogue3DS::PokemonFormSelectionResult::Ok ||
        formActor.formId != formAt("lycanroc", 2)) return 88;
    formContext = {};
    formContext.hasMysteryEncounters = true;
    if (Pokerogue3DS::selectPokemonActorForm(dexFor("gimmighoul"), formContext, formRng, formActor) !=
        Pokerogue3DS::PokemonFormSelectionResult::Ok ||
        formActor.formId != formAt("gimmighoul", 1)) return 89;
    formContext = {};
    formContext.trainerBattle = true;
    formContext.trainerSpecialtyType = "WATER";
    if (Pokerogue3DS::selectPokemonActorForm(dexFor("rotom"), formContext, formRng, formActor) !=
        Pokerogue3DS::PokemonFormSelectionResult::Ok ||
        formActor.formId != formAt("rotom", 2)) return 90;
    formContext = {};
    if (Pokerogue3DS::selectPokemonActorForm(dexFor("toxtricity"), formContext, formRng, formActor) !=
        Pokerogue3DS::PokemonFormSelectionResult::Ok ||
        formActor.formId != formAt("toxtricity", 0)) return 91;
    formContext.nature = Pokerogue3DS::PokemonNature::Timid;
    if (Pokerogue3DS::selectPokemonActorForm(dexFor("toxtricity"), formContext, formRng, formActor) !=
        Pokerogue3DS::PokemonFormSelectionResult::Ok ||
        formActor.formId != formAt("toxtricity", 1)) return 92;
    formContext = {};
    formContext.trainerBattle = true;
    formContext.waveIndex = 29;
    if (Pokerogue3DS::selectPokemonActorForm(dexFor("pikachu"), formContext, formRng, formActor) !=
        Pokerogue3DS::PokemonFormSelectionResult::Ok || formActor.formId != formAt("pikachu", 0)) return 93;
    formActor.gender = PokemonGender::Female;
    formActor.formId = formAt("meowstic", 1);
    PokemonBattleInit selectedFormInput{};
    selectedFormInput.speciesDex = dexFor("meowstic");
    selectedFormInput.level = 10;
    PokemonBattleState selectedFormState{};
    if (Pokerogue3DS::initializePokemonBattleStateForActor(
            selectedFormInput, formActor, selectedFormState) != PokemonBattleInitResult::Ok ||
        selectedFormState.formId != formActor.formId) return 94;

    formContext = {};
    PokerogueRngAdapter stagedActorRng;
    stagedActorRng.sow(formSeed, sizeof(formSeed) / sizeof(formSeed[0]));
    Pokerogue3DS::PokemonActorIdentity stagedActor{};
    if (Pokerogue3DS::generatePokemonActorIdentity(
            dexFor("unown"), 256, stagedActorRng, stagedActor) !=
        Pokerogue3DS::PokemonActorIdentityResult::Ok ||
        Pokerogue3DS::selectPokemonActorForm(
            dexFor("unown"), formContext, stagedActorRng, stagedActor) !=
        Pokerogue3DS::PokemonFormSelectionResult::Ok) return 95;
    PokerogueRngAdapter combinedActorRng;
    combinedActorRng.sow(formSeed, sizeof(formSeed) / sizeof(formSeed[0]));
    Pokerogue3DS::PokemonActorIdentity combinedActor{};
    if (Pokerogue3DS::generatePokemonActorIdentityAndForm(
            dexFor("unown"), 256, formContext, combinedActorRng, combinedActor) !=
        Pokerogue3DS::PokemonActorIdentityResult::Ok ||
        combinedActor.pokemonId != stagedActor.pokemonId ||
        combinedActor.abilityIndex != stagedActor.abilityIndex ||
        combinedActor.gender != stagedActor.gender || combinedActor.formId != stagedActor.formId) return 96;
    for (uint8_t i = 0; i < 6; ++i) if (combinedActor.ivs[i] != stagedActor.ivs[i]) return 97;
    const auto combinedRngState = combinedActorRng.state();
    const auto stagedRngState = stagedActorRng.state();
    if (combinedRngState.carry != stagedRngState.carry || combinedRngState.s0 != stagedRngState.s0 ||
        combinedRngState.s1 != stagedRngState.s1 || combinedRngState.s2 != stagedRngState.s2) return 98;

    PokerogueRngAdapter wildActorRng;
    wildActorRng.sow(formSeed, sizeof(formSeed) / sizeof(formSeed[0]));
    PokerogueRngAdapter stagedWildRng;
    stagedWildRng.sow(formSeed, sizeof(formSeed) / sizeof(formSeed[0]));
    PokemonFormSelectionContext wildContext{};
    wildContext.biomeId = "town";
    wildContext.timeOfDay = "DAY";
    wildContext.waveIndex = 1;
    PokemonActorIdentity wildActor{};
    if (Pokerogue3DS::generatePokemonActorForWildEncounter(
            dexFor("charizard"), 256, wildContext, wildActorRng, wildActor) !=
            Pokerogue3DS::PokemonActorIdentityResult::Ok ||
        !wildActor.initialTeraTypeResolved || wildActor.nature == PokemonNature::Unspecified) return 99;
    PokemonActorIdentity stagedWildActor{};
    if (Pokerogue3DS::generatePokemonActorIdentityAndForm(
            dexFor("charizard"), 256, wildContext, stagedWildRng, stagedWildActor) !=
        Pokerogue3DS::PokemonActorIdentityResult::Ok) return 100;
    Pokerogue3DS::generatePokemonActorNature(stagedWildActor, stagedWildRng);
    const auto* selectedWildForm = PokerogueContent::findFormById(stagedWildActor.formId);
    const char* wildSecondaryType = selectedWildForm ? selectedWildForm->type2 :
        PokerogueContent::findSpeciesByDex(dexFor("charizard"))->type2;
    const uint8_t wildTypeCount = wildSecondaryType && wildSecondaryType[0] ? 2 : 1;
    const uint8_t expectedTeraIndex = static_cast<uint8_t>(stagedWildRng.randSeedInt(wildTypeCount));
    const auto wildRngState = wildActorRng.state();
    const auto stagedWildRngState = stagedWildRng.state();
    if (wildActor.pokemonId != stagedWildActor.pokemonId ||
        wildActor.formId != stagedWildActor.formId || wildActor.nature != stagedWildActor.nature ||
        wildActor.initialTeraTypeIndex != expectedTeraIndex ||
        wildRngState.carry != stagedWildRngState.carry || wildRngState.s0 != stagedWildRngState.s0 ||
        wildRngState.s1 != stagedWildRngState.s1 || wildRngState.s2 != stagedWildRngState.s2) return 101;

    // Real pinned species and form learnsets feed a bounded native candidate
    // pool; test both the level gate and form-specific imported move records.
    Pokerogue3DS::PokemonLevelMoveCandidate learnset[128]{};
    std::size_t learnsetCount = 0;
    if (Pokerogue3DS::buildPokemonLevelMovePool(
            dexFor("pikachu"), "pikachu:gigantamax", 55, learnset, 128, learnsetCount) !=
            Pokerogue3DS::PokemonLevelMovePoolResult::Ok || learnsetCount < 5) return 102;
    bool hasGigantamaxLastMove = false;
    bool hasFutureLevelMove = false;
    for (std::size_t i = 0; i < learnsetCount; ++i) {
        if (learnset[i].moveId == 528 && learnset[i].sourceLevel == 55) hasGigantamaxLastMove = true;
    }
    if (Pokerogue3DS::buildPokemonLevelMovePool(
            dexFor("pikachu"), "pikachu:gigantamax", 20, learnset, 128, learnsetCount) !=
            Pokerogue3DS::PokemonLevelMovePoolResult::Ok) return 103;
    for (std::size_t i = 0; i < learnsetCount; ++i)
        if (learnset[i].sourceLevel > 20) hasFutureLevelMove = true;
    if (hasFutureLevelMove || !hasGigantamaxLastMove) return 104;
    if (Pokerogue3DS::buildPokemonLevelMovePool(
            dexFor("pikachu"), "charizard:base_0", 55, learnset, 128, learnsetCount) !=
            Pokerogue3DS::PokemonLevelMovePoolResult::InvalidForm || learnsetCount != 0) return 105;
    if (Pokerogue3DS::buildPokemonLevelMovePool(
            dexFor("charizard"), "charizard:base_0", 1, learnset, 128, learnsetCount) !=
        Pokerogue3DS::PokemonLevelMovePoolResult::Ok) return 106;
    // Current-species level-one reminders are excluded when the upstream
    // prevolution chain is present; Charmeleon's pool precedes Charizard.
    bool dragonClawReminderWeight = false;
    for (std::size_t i = 0; i < learnsetCount; ++i)
        if (learnset[i].moveId == 337 && learnset[i].weight == 50) dragonClawReminderWeight = true;
    if (dragonClawReminderWeight) return 107;
    if (PokerogueContent::findSpeciesByDex(dexFor("charizard"))->starterEligible ||
        !PokerogueContent::findSpeciesByDex(dexFor("pikachu"))->starterEligible ||
        !PokerogueContent::findSpeciesByDex(dexFor("bulbasaur"))->freshProfileStarter ||
        !PokerogueContent::findSpeciesByDex(dexFor("charmander"))->freshProfileStarter ||
        PokerogueContent::findSpeciesByDex(dexFor("pikachu"))->freshProfileStarter ||
        PokerogueContent::findSpeciesByDex(dexFor("pikachu"))->prevolutionDex != dexFor("pichu") ||
        PokerogueContent::findSpeciesByDex(dexFor("charizard"))->prevolutionDex != dexFor("charmeleon")) return 108;
    if (Pokerogue3DS::buildPokemonLevelMovePool(dexFor("pikachu"), nullptr, 200, learnset, 128, learnsetCount) !=
        Pokerogue3DS::PokemonLevelMovePoolResult::Ok || learnsetCount == 0) return 109;

    PokemonNature freshNature = PokemonNature::Unspecified;
    if (Pokerogue3DS::pokemonFreshProfileNature(dexFor("bulbasaur"), freshNature) !=
            Pokerogue3DS::PokemonFreshProfileResult::Ok ||
        PokerogueContent::findSpeciesByDex(dexFor("bulbasaur"))->freshProfileStarterOrdinal != 0 ||
        PokerogueContent::findSpeciesByDex(dexFor("charmander"))->freshProfileStarterOrdinal != 1 ||
        (freshNature != PokemonNature::Hardy && freshNature != PokemonNature::Docile &&
         freshNature != PokemonNature::Serious && freshNature != PokemonNature::Bashful &&
         freshNature != PokemonNature::Quirky) ||
        Pokerogue3DS::pokemonFreshProfileNature(dexFor("pikachu"), freshNature) !=
            Pokerogue3DS::PokemonFreshProfileResult::NotDefaultStarter) return 121;

    uint16_t starterMoves[4]{};
    uint8_t starterMoveCount = 0;
    if (Pokerogue3DS::selectPokemonStarterMoveset(dexFor("bulbasaur"), nullptr, 0,
            nullptr, 0, starterMoves, starterMoveCount) != Pokerogue3DS::PokemonStarterMovesetResult::Ok ||
        starterMoveCount != 3 || starterMoves[0] != 33 || starterMoves[1] != 45 || starterMoves[2] != 22) return 118;
    const uint16_t preferredStarterMoves[] = {202, 33};
    if (Pokerogue3DS::selectPokemonStarterMoveset(dexFor("bulbasaur"), nullptr, 1,
            preferredStarterMoves, 2, starterMoves, starterMoveCount) != Pokerogue3DS::PokemonStarterMovesetResult::Ok ||
        starterMoveCount != 4 || starterMoves[0] != 202 || starterMoves[1] != 33 ||
        starterMoves[2] != 45 || starterMoves[3] != 22) return 119;
    if (Pokerogue3DS::selectPokemonStarterMoveset(dexFor("bulbasaur"), nullptr, 0x10,
            nullptr, 0, starterMoves, starterMoveCount) != Pokerogue3DS::PokemonStarterMovesetResult::InvalidEggMoveMask) return 120;
    PokemonBattleInit freshStarterInput{};
    freshStarterInput.speciesDex = dexFor("bulbasaur");
    freshStarterInput.level = 5;
    freshStarterInput.abilityId = PokerogueContent::findSpeciesByDex(freshStarterInput.speciesDex)->ability1;
    freshStarterInput.gender = Pokerogue3DS::PokemonGender::Male;
    freshStarterInput.nature = freshNature;
    freshStarterInput.moveCount = starterMoveCount;
    for (uint8_t i = 0; i < 6; ++i) freshStarterInput.ivs[i] = 15;
    for (uint8_t i = 0; i < starterMoveCount; ++i) freshStarterInput.moveIds[i] = starterMoves[i];
    PokemonBattleState freshStarterState{};
    if (Pokerogue3DS::initializePokemonBattleState(freshStarterInput, freshStarterState) !=
            PokemonBattleInitResult::Ok || freshStarterState.hp == 0 ||
        freshStarterState.ivs[0] != 15 || freshStarterState.nature != freshNature ||
        freshStarterState.moveCount != 3 || freshStarterState.moves[0].moveId != 33 ||
        freshStarterState.moves[0].pp != PokerogueContent::findMoveById(33)->pp) return 122;

    // Real ability callbacks feed the move-generation adapter. Supported
    // declarative effects remain typed; unported callbacks fail closed.
    const auto* compoundEyes = PokerogueContent::findAbilityMovegenProfile(14);
    const auto* skillLink = PokerogueContent::findAbilityMovegenProfile(92);
    const auto* drizzle = PokerogueContent::findAbilityMovegenProfile(2);
    const auto* hustle = PokerogueContent::findAbilityMovegenProfile(55);
    const auto* analytic = PokerogueContent::findAbilityMovegenProfile(148);
    if (!compoundEyes || compoundEyes->flags != 0 || compoundEyes->accuracyMultiplier != 1.3 ||
        !compoundEyes->sourcePath || !compoundEyes->sourceSymbol || !compoundEyes->sourceHash) return 110;
    if (!skillLink || skillLink->flags != PokerogueContent::AbilityMovegenMaxMultiHit ||
        !skillLink->sourcePath || !skillLink->sourceSymbol || !skillLink->sourceHash) return 111;
    if (!drizzle || !(drizzle->flags & PokerogueContent::AbilityMovegenUnsupported)) return 112;
    if (!hustle || hustle->flags != PokerogueContent::AbilityMovegenHustle ||
        !analytic || analytic->flags != PokerogueContent::AbilityMovegenAnalytic) return 116;
    Pokerogue3DS::PokemonWildMoveRuntimeMetadata moveMetadata{};
    if (!Pokerogue3DS::buildPokemonWildMoveRuntimeMetadata(33, 14, moveMetadata) ||
        moveMetadata.type == nullptr || moveMetadata.power.power != 40 ||
        moveMetadata.ability.accuracyMultiplier != 1.3) return 113;
    if (!Pokerogue3DS::buildPokemonWildMoveRuntimeMetadata(292, 92, moveMetadata) ||
        !moveMetadata.ability.maxMultiHitHolderPresent || !moveMetadata.ability.maxMultiHitValue) return 114;
    if (!Pokerogue3DS::buildPokemonWildMoveRuntimeMetadata(33, 55, moveMetadata) ||
        moveMetadata.ability.powerMultiplier != 1.5 ||
        moveMetadata.ability.accuracyMultiplier != 0.8) return 179;
    if (!Pokerogue3DS::buildPokemonWildMoveRuntimeMetadata(55, 55, moveMetadata) ||
        moveMetadata.ability.powerMultiplier != 1.0 ||
        moveMetadata.ability.accuracyMultiplier != 1.0) return 180;
    if (!Pokerogue3DS::buildPokemonWildMoveRuntimeMetadata(233, 148, moveMetadata) ||
        moveMetadata.ability.powerMultiplier != 1.3) return 181;
    if (!Pokerogue3DS::buildPokemonWildMoveRuntimeMetadata(33, 148, moveMetadata) ||
        moveMetadata.ability.powerMultiplier != 1.0) return 182;
    if (Pokerogue3DS::buildPokemonWildMoveRuntimeMetadata(33, 2, moveMetadata) ||
        Pokerogue3DS::buildPokemonWildMoveRuntimeMetadata(33, 65535, moveMetadata)) return 115;
    const auto checkRealMoveset = [](uint16_t dex, uint16_t abilityId) {
        const auto* species = PokerogueContent::findSpeciesByDex(dex);
        if (!species || (abilityId != species->ability1 && abilityId != species->ability2 &&
                         abilityId != species->abilityHidden)) return false;
        PokemonBattleInit actorInput{};
        actorInput.speciesDex = dex;
        actorInput.level = 5;
        actorInput.abilityId = abilityId;
        actorInput.gender = PokemonGender::Male;
        PokemonBattleState actorState{};
        if (Pokerogue3DS::initializePokemonBattleState(actorInput, actorState) != PokemonBattleInitResult::Ok)
            return false;
        Pokerogue3DS::PokemonLevelMoveCandidate pool[128]{};
        std::size_t poolCount = 0;
        if (Pokerogue3DS::buildPokemonLevelMovePool(dex, nullptr, 5, pool, 128, poolCount) !=
                Pokerogue3DS::PokemonLevelMovePoolResult::Ok || poolCount == 0 || poolCount > 128) return false;
        Pokerogue3DS::PokemonWildMoveRuntimeMetadata candidates[128]{};
        for (std::size_t i = 0; i < poolCount; ++i)
            if (!Pokerogue3DS::buildPokemonWildMoveRuntimeMetadata(pool[i].moveId, abilityId, candidates[i])) return false;
        const uint16_t seed[] = {'m', 'o', 'v', static_cast<uint16_t>(dex)};
        PokerogueRngAdapter firstRng;
        firstRng.sow(seed, sizeof(seed) / sizeof(seed[0]));
        uint16_t firstMoves[4]{};
        uint8_t firstCount = 0;
        if (Pokerogue3DS::generateWildMovesetFromLearnset(dex, nullptr, 5,
                actorState.stats[1], actorState.stats[3], actorState.stats[2],
                species->type1, species->type2, candidates, poolCount,
                firstRng, firstMoves, firstCount) != Pokerogue3DS::PokemonWildMovesetResult::Ok ||
            firstCount == 0 || firstCount > 4) return false;
        PokerogueRngAdapter replayRng;
        replayRng.sow(seed, sizeof(seed) / sizeof(seed[0]));
        uint16_t replayMoves[4]{};
        uint8_t replayCount = 0;
        if (Pokerogue3DS::generateWildMovesetFromLearnset(dex, nullptr, 5,
                actorState.stats[1], actorState.stats[3], actorState.stats[2],
                species->type1, species->type2, candidates, poolCount,
                replayRng, replayMoves, replayCount) != Pokerogue3DS::PokemonWildMovesetResult::Ok ||
            replayCount != firstCount) return false;
        const auto firstState = firstRng.state();
        const auto replayState = replayRng.state();
        if (firstState.carry != replayState.carry || firstState.s0 != replayState.s0 ||
            firstState.s1 != replayState.s1 || firstState.s2 != replayState.s2) return false;
        for (uint8_t i = 0; i < firstCount; ++i) {
            if (firstMoves[i] != replayMoves[i] || !PokerogueContent::findMoveById(firstMoves[i])) return false;
            for (uint8_t j = 0; j < i; ++j) if (firstMoves[i] == firstMoves[j]) return false;
        }
        actorInput.moveCount = firstCount;
        for (uint8_t i = 0; i < firstCount; ++i) actorInput.moveIds[i] = firstMoves[i];
        PokemonBattleState battleReady{};
        if (Pokerogue3DS::initializePokemonBattleState(actorInput, battleReady) != PokemonBattleInitResult::Ok ||
            battleReady.moveCount != firstCount) return false;
        for (uint8_t i = 0; i < firstCount; ++i) {
            const auto* canonicalMove = PokerogueContent::findMoveById(firstMoves[i]);
            if (!canonicalMove || battleReady.moves[i].moveId != firstMoves[i] ||
                battleReady.moves[i].maxPp != canonicalMove->pp ||
                battleReady.moves[i].pp != battleReady.moves[i].maxPp) return false;
        }
        return true;
    };
    if (!checkRealMoveset(dexFor("bulbasaur"), 65) || !checkRealMoveset(dexFor("pikachu"), 9)) return 117;
    return 0;
}

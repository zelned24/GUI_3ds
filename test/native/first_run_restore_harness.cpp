#include "game/FirstRunRuntime.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include <cstring>
#include "storage/IntegritySha256.hpp"
#include "storage/NativeStarterCandyProfile.hpp"
#include "game/PokemonExperience.hpp"
#include "game/PokemonWeatherPhase.hpp"
#include "game/PokerogueClassicWaveSchedule.hpp"
#include "game/PokerogueEncounterResolver.hpp"
#include "game/PokerogueTrainerPartyLevels.hpp"
#include "game/PokemonWildMovesetGenerator.hpp"

// Standalone host regression for atomic checkpoint application. This requires
// the standard C++ library; it is not the freestanding WASM parity harness.
static int checkTrainerExperienceReplay() {
    using namespace Pokerogue3DS;
    for (uint32_t seed = 1; seed <= 256; ++seed) {
        FirstRunRuntime game(seed);
        bool eligible = true;
        for (uint16_t wave = 1; wave <= 4; ++wave) {
            const auto& context = game.presentation();
            if (classifyClassicWave(wave) == ClassicWaveKind::TrainerChanceRequired &&
                    context.trainerPartyCount && context.enemy.actorIdentityResolved) {
                if (!context.trainerPartyBattleStatesResolved ||
                    !context.trainerPartyMovesetsResolved || !context.trainerPartyIvsResolved ||
                    context.activeTrainerPartyIndex >= context.trainerPartyCount ||
                    context.enemy.dex != context.trainerParty[context.activeTrainerPartyIndex].dex) return 228;
                for (uint8_t member = 0; member < context.trainerPartyCount; ++member)
                    if (!context.trainerParty[member].actorIdentityResolved ||
                        !context.trainerParty[member].battleState.moveCount) return 229;
            }
            if (!context.enemy.actorIdentityResolved || context.secondEnemy.dex ||
                context.trainerPartyCount) { eligible = false; break; }
            NativeRunSave won{};
            game.captureNativeRunSave(won);
            won.stage = NativeSaveStage::BattleWon;
            won.encounterDex = context.enemy.dex;
            won.playerHp = context.player.battleState.hp;
            won.enemyHp = 0;
            won.battleTurn = 1;
            won.playerMoveCount = context.player.battleState.moveCount;
            won.enemyMoveCount = context.enemy.battleState.moveCount;
            for (uint8_t slot = 0; slot < won.playerMoveCount; ++slot) {
                won.playerMoveIds[slot] = context.player.battleState.moves[slot].moveId;
                won.playerPp[slot] = context.player.battleState.moves[slot].pp;
            }
            for (uint8_t slot = 0; slot < won.enemyMoveCount; ++slot) {
                won.enemyMoveIds[slot] = context.enemy.battleState.moves[slot].moveId;
                won.enemyPp[slot] = context.enemy.battleState.moves[slot].pp;
            }
            if (!game.restoreNativeRunSave(won) || !game.advanceBattleTurn() ||
                !game.skipVictoryReward()) { eligible = false; break; }
        }
        if (!eligible || game.presentation().trainerPartyCount != 2 ||
            !game.presentation().trainerPartyBattleStatesResolved) continue;
        NativeRunSave checkpoint{};
        game.captureNativeRunSave(checkpoint);
        if (checkpoint.stage != NativeSaveStage::BattleActive) continue;
        NativeRunSave memberWon = checkpoint;
        memberWon.stage = NativeSaveStage::BattleWon;
        memberWon.enemyHp = 0;
        memberWon.trainerParty[memberWon.activeTrainerMember].hp = 0;
        if (!game.restoreNativeRunSave(memberWon) ||
            game.victoryPlan().contains(ClassicVictoryStep::SelectModifier) ||
            game.victoryPlan().contains(ClassicVictoryStep::NewBattle) ||
            game.skipVictoryReward() || game.run().wave != 5) return 28;
        if (!game.advanceBattleTurn() || game.battleFinished() ||
            game.presentation().activeTrainerPartyIndex != 1 || game.run().wave != 5) return 29;
        if (!game.restoreNativeRunSave(checkpoint)) return 30;
        const auto& context = game.presentation();
        const auto* defeated = PokerogueContent::findSpeciesByDex(context.trainerParty[0].dex);
        const auto* starter = PokerogueContent::findSpeciesByDex(context.player.dex);
        const auto* form = context.trainerParty[0].formId
            ? PokerogueContent::findFormById(context.trainerParty[0].formId) : nullptr;
        double raw = 0;
        uint32_t award = 0;
        PokemonExperienceProgress progress{};
        if (!defeated || !starter || pokemonExperienceForDefeat(*defeated,
                context.trainerParty[0].level, raw, form) != PokemonExperienceResult::Ok ||
            pokemonSingleParticipantExperience(raw, true, award) != PokemonExperienceResult::Ok ||
            applyPokemonExperience(starter->growthRate, checkpoint.playerLevel,
                checkpoint.playerExperience, award, classicExperienceLevelCap(5), progress) !=
                PokemonExperienceResult::Ok) return 8;
        checkpoint.trainerParty[0].hp = 0;
        checkpoint.activeTrainerMember = 1;
        checkpoint.encounterDex = checkpoint.trainerParty[1].speciesDex;
        checkpoint.enemyHp = checkpoint.trainerParty[1].hp;
        checkpoint.enemyMoveCount = checkpoint.trainerParty[1].moveCount;
        for (uint8_t slot = 0; slot < 4; ++slot) {
            checkpoint.enemyMoveIds[slot] = checkpoint.trainerParty[1].moveIds[slot];
            checkpoint.enemyPp[slot] = checkpoint.trainerParty[1].pp[slot];
        }
        checkpoint.playerLevel = progress.level;
        checkpoint.playerExperience = progress.totalExperience;
        checkpoint.playerHp = 1;
        checkpoint.battleTurn = 2;
        if (!game.restoreNativeRunSave(checkpoint)) return 9;
        NativeRunSave restored{};
        game.captureNativeRunSave(restored);
        if (restored.playerExperience != checkpoint.playerExperience ||
            restored.activeTrainerMember != 1 || restored.trainerParty[0].hp ||
            restored.battleTurn != 2) return 10;
        NativeRunSave won = restored;
        won.stage = NativeSaveStage::BattleWon;
        won.enemyHp = 0;
        won.trainerParty[1].hp = 0;
        if (!game.restoreNativeRunSave(won) || !game.advanceBattleTurn()) return 12;
        NativeRunSave finalExperience{};
        game.captureNativeRunSave(finalExperience);
        if (finalExperience.stage != NativeSaveStage::ExperienceGranted ||
            !game.restoreNativeRunSave(finalExperience)) return 13;
        NativeRunSave roundtrip{};
        game.captureNativeRunSave(roundtrip);
        if (roundtrip.playerExperience != finalExperience.playerExperience ||
            roundtrip.playerLevel != finalExperience.playerLevel ||
            roundtrip.stage != NativeSaveStage::ExperienceGranted) return 14;
        return 0;
    }
    return 11; // No reconstructable pinned party: do not silently skip.
}

static int checkResolvedActionFieldLifecycle() {
    using namespace Pokerogue3DS;
    for (uint32_t seed = 1; seed <= 512; ++seed) {
        FirstRunRuntime game(seed);
        for (unsigned slot = 0; slot < 4 && !game.battleInputSupported(); ++slot)
            game.selectBattleMove(1);
        if (!game.battleInputSupported() || !game.advanceBattleTurn()) continue;
        NativeRunSave active{};
        game.captureNativeRunSave(active);
        if (active.stage != NativeSaveStage::BattleActive) continue;
        active.trickRoomTurnsLeft = 3;
        active.trickRoomMaxDuration = 5;
        active.trickRoomSourceMoveId = 433;
        active.trickRoomSourcePokemonId = game.presentation().player.battleState.pokemonId;
        if (!game.restoreNativeRunSave(active)) return 28;
        for (unsigned slot = 0; slot < 4 && !game.battleInputSupported(); ++slot)
            game.selectBattleMove(1);
        if (!game.battleInputSupported()) continue;
        const uint8_t moveSlot = game.selectedBattleMove();
        const auto& player = game.presentation().player.battleState;
        uint8_t cost = 0;
        if (!pokemonSingleOpponentPpCost(game.presentation().enemy.battleState.abilityId, cost)) return 29;
        const uint8_t initialPp = player.moves[moveSlot].pp;
        if (!game.advanceBattleTurn()) return 30;
        NativeRunSave after{};
        game.captureNativeRunSave(after);
        if (after.trickRoomTurnsLeft != 2 || after.trickRoomSourceMoveId != 433) return 31;
        if (after.weatherType != static_cast<uint8_t>(game.arenaWeather().type) ||
            after.weatherTurnsLeft != game.arenaWeather().turnsLeft ||
            after.weatherMaxDuration != game.arenaWeather().maxDuration) return 34;
        if (game.presentation().player.battleState.hp && after.playerPp[moveSlot] !=
                initialPp - (cost < initialPp ? cost : initialPp)) return 32;
        if (!pokemonWeatherLifecycleSupported(game.presentation().player.battleState.abilityId) ||
            !pokemonWeatherLifecycleSupported(game.presentation().enemy.battleState.abilityId)) continue;
        active.weatherType = 2;
        active.weatherTurnsLeft = active.weatherMaxDuration = 1;
        active.trickRoomTurnsLeft = active.trickRoomMaxDuration = 0;
        active.trickRoomSourceMoveId = active.trickRoomSourcePokemonId = 0;
        if (!game.restoreNativeRunSave(active)) return 37;
        for (unsigned slot = 0; slot < 4 && !game.battleInputSupported(); ++slot) game.selectBattleMove(1);
        if (!game.battleInputSupported()) continue;
        const auto* selected = PokerogueContent::findMoveById(
            game.presentation().player.battleState.moves[game.selectedBattleMove()].moveId);
        if (!selected || selected->category == PokerogueContent::MoveStatus) continue;
        if (!game.advanceBattleTurn()) return 38;
        NativeRunSave expired{};
        game.captureNativeRunSave(expired);
        if (game.arenaWeather().type != PokemonEffectiveWeather::None ||
            expired.weatherType || expired.weatherTurnsLeft || expired.weatherMaxDuration) return 39;
        return 0;
    }
    return 33; // No supported real encounter: never silently skip integration coverage.
}

static int checkTrainerInteractiveBattle() {
    using namespace Pokerogue3DS;
    for (uint32_t seed = 1; seed <= 256; ++seed) {
        FirstRunRuntime game(seed);
        bool eligible = true;
        for (uint16_t wave = 1; wave <= 4; ++wave) {
            const auto& context = game.presentation();
            if (!context.enemy.actorIdentityResolved || context.secondEnemy.dex ||
                context.trainerPartyCount) { eligible = false; break; }
            NativeRunSave won{};
            game.captureNativeRunSave(won);
            won.stage = NativeSaveStage::BattleWon;
            won.encounterDex = context.enemy.dex;
            won.playerHp = context.player.battleState.hp;
            won.enemyHp = 0;
            won.battleTurn = 1;
            won.playerMoveCount = context.player.battleState.moveCount;
            won.enemyMoveCount = context.enemy.battleState.moveCount;
            for (uint8_t slot = 0; slot < won.playerMoveCount; ++slot) {
                won.playerMoveIds[slot] = context.player.battleState.moves[slot].moveId;
                won.playerPp[slot] = context.player.battleState.moves[slot].pp;
            }
            for (uint8_t slot = 0; slot < won.enemyMoveCount; ++slot) {
                won.enemyMoveIds[slot] = context.enemy.battleState.moves[slot].moveId;
                won.enemyPp[slot] = context.enemy.battleState.moves[slot].pp;
            }
            if (!game.restoreNativeRunSave(won) || !game.advanceBattleTurn() ||
                !game.skipVictoryReward()) { eligible = false; break; }
        }
        if (!eligible || game.presentation().trainerPartyCount != 2 ||
            !game.presentation().trainerPartyBattleStatesResolved) continue;

        // At wave 5, verify trainer battle is supported and interactive
        if (!game.trainerBattleSupported()) return 40;
        if (!game.battleInputSupported()) return 41;

        // Execute a battle turn interactively against the trainer
        const uint16_t initialPlayerHp = game.presentation().player.battleState.hp;
        const uint16_t initialEnemyHp = game.presentation().enemy.battleState.hp;
        if (!game.advanceBattleTurn()) return 42;
        const auto& activeAfter = game.presentation();
        if (activeAfter.enemy.battleState.hp == initialEnemyHp &&
            activeAfter.player.battleState.hp == initialPlayerHp &&
            activeAfter.activeTrainerPartyIndex == 0) return 43;

        // Defeat first trainer Pokemon and transition to second
        NativeRunSave firstDefeated{};
        game.captureNativeRunSave(firstDefeated);
        firstDefeated.stage = NativeSaveStage::BattleWon;
        firstDefeated.enemyHp = 0;
        firstDefeated.trainerParty[firstDefeated.activeTrainerMember].hp = 0;
        if (!game.restoreNativeRunSave(firstDefeated)) return 44;
        if (!game.advanceBattleTurn()) return 45;
        // Active trainer member must now be the reserve (index 1)
        if (game.presentation().activeTrainerPartyIndex != 1) return 46;
        if (game.battleFinished()) return 47;
        if (!game.trainerBattleSupported() || !game.battleInputSupported()) return 48;

        // Execute interactive turn against the second Pokemon
        if (!game.advanceBattleTurn()) return 49;

        // Defeat the second Pokemon
        NativeRunSave secondDefeated{};
        game.captureNativeRunSave(secondDefeated);
        secondDefeated.stage = NativeSaveStage::BattleWon;
        secondDefeated.enemyHp = 0;
        secondDefeated.trainerParty[secondDefeated.activeTrainerMember].hp = 0;
        if (!game.restoreNativeRunSave(secondDefeated)) return 50;
        if (!game.advanceBattleTurn()) return 51;
        // Both trainer Pokémon fainted: battle is won, experience granted
        if (!game.battleFinished() || !game.playerWon() || !game.experienceGranted()) return 52;
        // Advancing now generates rewards or allows skipping
        if (!game.skipVictoryReward()) return 53;
        // Wave progresses to wave 6
        if (game.run().wave != 6) return 54;
        return 0;
    }
    return 55;
}

static int checkModifierRewardGenerationAndClaim() {
    using namespace Pokerogue3DS;
    // Rejected real encounters must not publish partial combat state or RNG draws.
    unsigned rejectedTurns = 0;
    for (uint32_t seed = 1; seed <= 64; ++seed) {
        FirstRunRuntime game(seed);
        const auto beforePlayer = game.presentation().player.battleState;
        const auto beforeEnemy = game.presentation().enemy.battleState;
        const auto beforeSecond = game.presentation().secondEnemy.battleState;
        const auto beforeRng = game.battleRng().state();
        if (game.advanceBattleTurn()) continue;
        ++rejectedTurns;
        const auto afterRng = game.battleRng().state();
        if (beforeRng.carry != afterRng.carry || beforeRng.s0 != afterRng.s0 ||
            beforeRng.s1 != afterRng.s1 || beforeRng.s2 != afterRng.s2) return 217;
        const PokemonBattleState* before[] = {&beforePlayer, &beforeEnemy, &beforeSecond};
        const PokemonBattleState* after[] = {&game.presentation().player.battleState,
            &game.presentation().enemy.battleState, &game.presentation().secondEnemy.battleState};
        for (unsigned actor = 0; actor < 3; ++actor) {
            if (before[actor]->hp != after[actor]->hp ||
                before[actor]->moveCount != after[actor]->moveCount) return 218;
            for (unsigned slot = 0; slot < before[actor]->moveCount; ++slot)
                if (before[actor]->moves[slot].pp != after[actor]->moves[slot].pp) return 219;
        }
    }
    if (!rejectedTurns) return 220; // The capability boundary must be exercised.
    for (uint32_t seed = 1; seed <= 64; ++seed) {
        FirstRunRuntime game(seed);
        const auto& context = game.presentation();
        if (!context.enemy.actorIdentityResolved || context.secondEnemy.dex) continue;
        NativeRunSave won{};
        game.captureNativeRunSave(won);
        won.stage = NativeSaveStage::BattleWon;
        won.enemyHp = 0;
        if (!game.restoreNativeRunSave(won)) return 60;
        if (!game.advanceBattleTurn()) return 61;
        if (!game.experienceGranted() || !game.battleFinished() || !game.playerWon()) return 62;
        if (!game.advanceBattleTurn()) return 63; // Triggers reward generation
        if (!game.rewardsPending() || game.rewardChoiceCount() != 3) return 64;
        if (game.selectedRewardChoice() != 0) return 65;
        if (!game.selectBattleMove(1) || game.selectedRewardChoice() != 1) return 66;
        if (!game.selectBattleMove(-1) || game.selectedRewardChoice() != 0) return 67;
        // Check a canonically generated choice with no current native reward adapter.
        auto unsupportedRewardGame = game;
        for (uint8_t option = 0; option < unsupportedRewardGame.rewardChoiceCount(); ++option) {
            const auto* candidate = unsupportedRewardGame.rewardChoice(unsupportedRewardGame.selectedRewardChoice());
            const char* id = candidate && candidate->poolEntry ? candidate->poolEntry->itemId : nullptr;
            if (id && (std::strcmp(id, "BERRY") == 0 || std::strcmp(id, "RARE_CANDY") == 0)) {
                const auto beforeWave = unsupportedRewardGame.run().wave;
                const auto beforeHp = unsupportedRewardGame.presentation().player.battleState.hp;
                if (unsupportedRewardGame.claimRewardChoice() || !unsupportedRewardGame.rewardsPending() ||
                    unsupportedRewardGame.run().wave != beforeWave ||
                    unsupportedRewardGame.presentation().player.battleState.hp != beforeHp) return 447;
            }
            unsupportedRewardGame.selectRewardChoice(1);
        }
        const auto* choice0 = game.rewardChoice(0);
        if (!choice0 || !choice0->poolEntry || !choice0->poolEntry->itemId) return 68;
        if (!game.claimRewardChoice()) return 69;
        if (game.run().wave != 2 || game.rewardsPending()) return 70;
        return 0;
    }
    return 71;
}

static int checkBiomeTransitionProgression() {
    using namespace Pokerogue3DS;
    for (uint32_t seed = 1; seed <= 64; ++seed) {
        FirstRunRuntime game(seed);
        bool eligible = true;
        for (uint16_t wave = 1; wave <= 9; ++wave) {
            NativeRunSave skipSave{};
            game.captureNativeRunSave(skipSave);
            skipSave.stage = NativeSaveStage::BattleWon;
            skipSave.enemyHp = 0;
            if (skipSave.trainerPartyCount) {
                for (uint8_t i = 0; i < skipSave.trainerPartyCount; ++i)
                    skipSave.trainerParty[i].hp = 0;
            }
            if (!game.restoreNativeRunSave(skipSave) || !game.advanceBattleTurn() ||
                !game.skipVictoryReward()) { eligible = false; break; }
        }
        if (!eligible || game.run().wave != 10) continue;
        if (std::strcmp(game.run().biomeId, "town") != 0) return 80;
        NativeRunSave wave10Won{};
        game.captureNativeRunSave(wave10Won);
        wave10Won.stage = NativeSaveStage::BattleWon;
        wave10Won.enemyHp = 0;
        if (!game.restoreNativeRunSave(wave10Won) || !game.advanceBattleTurn()) return 81;
        if (!game.skipVictoryReward()) return 82;
        if (game.run().wave != 11) return 83;
        if (std::strcmp(game.run().biomeId, "town") == 0) return 84;
        if (!game.presentation().biomeName || !*game.presentation().biomeName) return 85;
        return 0;
    }
    return 86;
}

static int checkDoubleBattleTargetingAndMechanics() {
    using namespace Pokerogue3DS;
    for (uint32_t seed = 1; seed <= 256; ++seed) {
        FirstRunRuntime game(seed);
        if (!game.doubleBattle()) continue;
        const auto& pres = game.presentation();
        if (!pres.enemy.actorIdentityResolved || !pres.secondEnemy.actorIdentityResolved) return 100;
        if (game.selectedTarget() != 0) return 101;
        if (!game.cycleTarget(1) || game.selectedTarget() != 1) return 102;
        if (!game.cycleTarget(1) || game.selectedTarget() != 0) return 103;
        if (!game.cycleTarget(-1) || game.selectedTarget() != 1) return 104;
        if (!game.cycleTarget(-1) || game.selectedTarget() != 0) return 105;

        if (!game.doubleBattleSupported()) continue;
        const uint8_t initialHp0 = pres.enemy.battleState.hp;
        const uint8_t initialHp1 = pres.secondEnemy.battleState.hp;
        const uint8_t playerHp = pres.player.battleState.hp;
        if (!initialHp0 || !initialHp1 || !playerHp) return 106;

        const auto& scene = game.scene();
        if (!scene.nodeCount || !scene.nodes[4].text || !std::strchr(scene.nodes[4].text, '>')) return 107;

        if (!game.advanceBattleTurn()) return 108;
        return 0;
    }
    return 109;
}

static int checkWave200FinalBossAndGameClear() {
    using namespace Pokerogue3DS;
    if (classifyClassicWave(200) != ClassicWaveKind::FinalBoss) return 110;
    ClassicVictoryPlan plan{};
    if (!planClassicVictory(200, plan)) return 111;
    if (!plan.contains(ClassicVictoryStep::GameClear) || plan.nextWave != 0) return 112;

    PokerogueRngAdapter rng;
    uint16_t seed[PokerogueRngAdapter::kMaxSeedCodeUnits] = {'t', 'e', 's', 't'};
    rng.sow(seed, 4);
    if (PokerogueEncounterResolver::bossLevelForWave(200, rng) != 200) return 113;
    const auto* firstPhase = findPokemonFixedEnemyMoveset(890, 0);
    const auto* secondPhase = findPokemonFixedEnemyMoveset(890, 1);
    if (!firstPhase || !secondPhase || findPokemonFixedEnemyMoveset(1, 0)) return 335;
    const uint16_t firstExpected[] = {795, 188, 53, 322};
    const uint16_t secondExpected[] = {744, 440, 53, 105};
    for (uint8_t slot = 0; slot < 4; ++slot)
        if (firstPhase->moveIds[slot] != firstExpected[slot] ||
            secondPhase->moveIds[slot] != secondExpected[slot]) return 336;
    const auto* finalSpecies = PokerogueContent::findSpeciesByDex(890);
    PokemonBattleInit bossInput{};
    bossInput.speciesDex = 890;
    bossInput.formId = finalSpecies->firstFormId;
    bossInput.level = 200;
    bossInput.abilityId = finalSpecies->ability1;
    bossInput.gender = PokemonGender::Genderless;
    bossInput.nature = PokemonNature::Hardy;
    bossInput.moveCount = 4;
    for (uint8_t slot = 0; slot < 4; ++slot) bossInput.moveIds[slot] = secondPhase->moveIds[slot];
    PokemonBattleState bossActor{};
    if (initializePokemonBattleState(bossInput, bossActor) != PokemonBattleInitResult::Ok ||
        !applyPokemonFixedEnemyMovePp(*secondPhase, bossActor) ||
        bossActor.moves[3].maxPp != 1 || bossActor.moves[3].pp != 1) return 337;
    PokemonBattleState phaseOneActor{};
    for (uint8_t slot = 0; slot < 4; ++slot) bossInput.moveIds[slot] = firstPhase->moveIds[slot];
    if (initializePokemonBattleState(bossInput, phaseOneActor) != PokemonBattleInitResult::Ok) return 339;
    phaseOneActor.hp = 1;
    phaseOneActor.moves[0].pp = 0;
    phaseOneActor.statStages[0] = 2;
    PokemonBossState phaseOneBoss{};
    if (!initializeClassicPokemonBossState(890, 200, 200, true, true, phaseOneBoss)) return 340;
    const auto unreachedBoss = phaseOneBoss;
    if (preparePokemonFinalBossSecondPhase(200, phaseOneActor, phaseOneBoss) ||
        phaseOneActor.hp != 1 || phaseOneBoss.segmentIndex != unreachedBoss.segmentIndex) return 341;
    phaseOneBoss.segmentIndex = 0;
    if (preparePokemonFinalBossSecondPhase(199, phaseOneActor, phaseOneBoss) || phaseOneActor.hp != 1)
        return 342;
    if (!preparePokemonFinalBossSecondPhase(200, phaseOneActor, phaseOneBoss) ||
        phaseOneActor.hp != phaseOneActor.maxHp || std::strcmp(phaseOneActor.formId, "eternatus:eternamax") ||
        phaseOneBoss.segmentCount != 5 || phaseOneBoss.segmentIndex != 4 ||
        phaseOneBoss.classicFinalBossFirstPhase || phaseOneActor.statStages[0] != 2 ||
        phaseOneActor.moves[3].maxPp != 1 || phaseOneActor.moves[3].pp != 1) return 343;
    for (uint8_t slot = 0; slot < 4; ++slot)
        if (phaseOneActor.moves[slot].moveId != secondPhase->moveIds[slot]) return 344;
    if (preparePokemonFinalBossSecondPhase(200, phaseOneActor, phaseOneBoss) ||
        phaseOneBoss.segmentIndex != 4) return 345;
    HeldItemStackTransferEvent transfer{};
    if (calculateHeldItemStackTransfer(3, true, 2, 5, 1, transfer) !=
            HeldItemStackTransferResult::Transferred || transfer.transferred != 1 ||
        transfer.sourceRemaining != 2 || transfer.targetStack != 3 || transfer.removeSource) return 346;
    if (calculateHeldItemStackTransfer(3, true, 4, 5, 3, transfer) !=
            HeldItemStackTransferResult::Transferred || transfer.transferred != 1 ||
        transfer.sourceRemaining != 2 || transfer.targetStack != 5) return 347;
    const auto unchangedTransfer = transfer;
    if (calculateHeldItemStackTransfer(1, true, 5, 5, 1, transfer) !=
            HeldItemStackTransferResult::NoCapacity || transfer.targetStack != unchangedTransfer.targetStack ||
        transfer.transferred != unchangedTransfer.transferred) return 348;
    if (calculateHeldItemStackTransfer(1, false, 0, 1, 1, transfer) !=
            HeldItemStackTransferResult::Transferred || transfer.sourceRemaining ||
        transfer.targetStack != 1 || !transfer.removeSource) return 349;
    const auto successfulTransfer = transfer;
    if (calculateHeldItemStackTransfer(0, false, 0, 1, 1, transfer) !=
            HeldItemStackTransferResult::InvalidState || transfer.targetStack != successfulTransfer.targetStack ||
        calculateHeldItemStackTransfer(1, false, 1, 2, 1, transfer) !=
            HeldItemStackTransferResult::InvalidState) return 350;
    const uint32_t theftOpponents[] = {1001, 1002};
    const HeldItemTransferCandidate theftInventory[] = {
        {1001, 11, true}, {1002, 19, false}, {1001, 13, true}, {1002, 20, true}
    };
    auto expectedTheftRng = rng;
    auto actualTheftRng = rng;
    const auto chosenOpponent = static_cast<size_t>(expectedTheftRng.randSeedInt(2));
    const auto chosenItem = expectedTheftRng.randSeedInt(chosenOpponent ? 1 : 2);
    const size_t expectedInventoryIndex = chosenOpponent ? 20 : chosenItem ? 13 : 11;
    HeldItemTransferSelection theftSelection{};
    if (selectHeldItemTransferAttempt(theftOpponents, 2, theftInventory, 4, 1,
            actualTheftRng, theftSelection) != HeldItemTransferSelectionResult::Selected ||
        !theftSelection.itemFound || theftSelection.opponentIndex != chosenOpponent ||
        theftSelection.inventoryIndex != expectedInventoryIndex) return 351;
    auto actualTheftState = actualTheftRng.state();
    auto expectedTheftState = expectedTheftRng.state();
    if (actualTheftState.carry != expectedTheftState.carry || actualTheftState.s0 != expectedTheftState.s0 ||
        actualTheftState.s1 != expectedTheftState.s1 || actualTheftState.s2 != expectedTheftState.s2) return 352;
    expectedTheftRng.randSeedInt(2);
    if (selectHeldItemTransferAttempt(theftOpponents, 2, nullptr, 0, 1,
            actualTheftRng, theftSelection) != HeldItemTransferSelectionResult::NoItem || theftSelection.itemFound)
        return 353;
    actualTheftState = actualTheftRng.state();
    expectedTheftState = expectedTheftRng.state();
    if (actualTheftState.carry != expectedTheftState.carry || actualTheftState.s0 != expectedTheftState.s0 ||
        actualTheftState.s1 != expectedTheftState.s1 || actualTheftState.s2 != expectedTheftState.s2) return 354;
    const auto beforeInvalidTheft = actualTheftRng.state();
    if (selectHeldItemTransferAttempt(nullptr, 2, theftInventory, 4, 1,
            actualTheftRng, theftSelection) != HeldItemTransferSelectionResult::InvalidState ||
        actualTheftRng.state().s0 != beforeInvalidTheft.s0 ||
        selectHeldItemTransferAttempt(nullptr, 0, nullptr, 0, 1,
            actualTheftRng, theftSelection) != HeldItemTransferSelectionResult::NoOpponent ||
        actualTheftRng.state().s0 != beforeInvalidTheft.s0) return 355;
    expectedTheftRng.randSeedInt(2);
    if (selectHeldItemTransferAttempt(theftOpponents, 2, theftInventory, 4, 0,
            actualTheftRng, theftSelection) != HeldItemTransferSelectionResult::NoTransferCount ||
        theftSelection.itemFound) return 356;
    actualTheftState = actualTheftRng.state();
    expectedTheftState = expectedTheftRng.state();
    if (actualTheftState.carry != expectedTheftState.carry || actualTheftState.s0 != expectedTheftState.s0 ||
        actualTheftState.s1 != expectedTheftState.s1 || actualTheftState.s2 != expectedTheftState.s2) return 357;
    NativeHeldModifierInstance blackHoleInstance{};
    if (initializeHeldModifierInstance("MINI_BLACK_HOLE", 89001, 1, false, "[]", blackHoleInstance) !=
            HeldModifierStorageResult::Ok || !heldModifierDefinition(blackHoleInstance) ||
        std::strcmp(heldModifierDefinition(blackHoleInstance)->id, "MINI_BLACK_HOLE") ||
        blackHoleInstance.transferable || blackHoleInstance.ownerPokemonId != 89001 ||
        std::strcmp(blackHoleInstance.rawArguments, "[]")) return 358;
    NativeHeldModifierInstance heldRecords[2]{};
    size_t heldCount = 0;
    if (appendHeldModifierInstance(heldRecords, 2, heldCount, blackHoleInstance) !=
            HeldModifierStorageResult::Ok || heldCount != 1) return 359;
    auto otherHolder = blackHoleInstance;
    otherHolder.ownerPokemonId = 89002;
    otherHolder.transferable = true;
    if (appendHeldModifierInstance(heldRecords, 2, heldCount, otherHolder) != HeldModifierStorageResult::Ok ||
        appendHeldModifierInstance(heldRecords, 2, heldCount, otherHolder) !=
            HeldModifierStorageResult::CapacityExceeded || heldCount != 2) return 360;
    if (!removeHeldModifierInstance(heldRecords, 2, heldCount, 0) || heldCount != 1 ||
        heldRecords[0].ownerPokemonId != 89002 || heldRecords[1].stackCount) return 361;
    const auto heldBeforeInvalid = heldRecords[0];
    if (initializeHeldModifierInstance("MISSING_CANONICAL_ITEM", 0, 1, true, nullptr, heldRecords[0]) !=
            HeldModifierStorageResult::MissingItem || heldRecords[0].ownerPokemonId != heldBeforeInvalid.ownerPokemonId)
        return 362;
    char hugeArguments[129]{};
    for (size_t i = 0; i < 128; ++i) hugeArguments[i] = 'x';
    if (initializeHeldModifierInstance("MINI_BLACK_HOLE", 0, 1, true, hugeArguments, heldRecords[0]) !=
            HeldModifierStorageResult::CapacityExceeded || heldRecords[0].ownerPokemonId != heldBeforeInvalid.ownerPokemonId)
        return 363;
    NativeHeldModifierInstance transferRecords[3]{};
    size_t transferRecordCount = 2;
    transferRecords[0] = otherHolder;
    transferRecords[0].ownerPokemonId = 1001;
    transferRecords[0].stackCount = 2;
    transferRecords[1] = otherHolder;
    transferRecords[1].ownerPokemonId = 1002;
    HeldItemTheftPolicy theftPolicy{};
    theftPolicy.resolved = true;
    theftPolicy.matchingTargetIndex = 1;
    theftPolicy.targetMaxStack = 3;
    HeldItemInventoryTransferEvent inventoryTransfer{};
    if (applySelectedHeldItemTheft(transferRecords, 3, transferRecordCount, 0, 1002, theftPolicy,
            inventoryTransfer) != HeldItemInventoryTransferResult::Transferred || transferRecordCount != 2 ||
        transferRecords[0].stackCount != 1 || transferRecords[1].stackCount != 2 ||
        inventoryTransfer.stacks.transferred != 1 || inventoryTransfer.sourcePokemonId != 1001 ||
        inventoryTransfer.targetPokemonId != 1002 || inventoryTransfer.resultingInventoryIndex != 1) return 364;
    if (applySelectedHeldItemTheft(transferRecords, 3, transferRecordCount, 0, 1002, theftPolicy,
            inventoryTransfer) != HeldItemInventoryTransferResult::Transferred || transferRecordCount != 1 ||
        transferRecords[0].ownerPokemonId != 1002 || transferRecords[0].stackCount != 3 ||
        transferRecords[1].stackCount || !inventoryTransfer.stacks.removeSource) return 365;
    transferRecords[1] = otherHolder;
    transferRecords[1].ownerPokemonId = 1001;
    transferRecordCount = 2;
    theftPolicy.matchingTargetIndex = 0;
    const auto savedInventoryTransfer = inventoryTransfer;
    if (applySelectedHeldItemTheft(transferRecords, 3, transferRecordCount, 1, 1002, theftPolicy,
            inventoryTransfer) != HeldItemInventoryTransferResult::NoCapacity || transferRecordCount != 2 ||
        transferRecords[1].stackCount != 1 || inventoryTransfer.sourcePokemonId != savedInventoryTransfer.sourcePokemonId)
        return 366;
    theftPolicy.resolved = false;
    if (applySelectedHeldItemTheft(transferRecords, 3, transferRecordCount, 1, 1002, theftPolicy,
            inventoryTransfer) != HeldItemInventoryTransferResult::UnresolvedPolicy) return 367;
    transferRecords[1].transferable = false;
    if (applySelectedHeldItemTheft(transferRecords, 3, transferRecordCount, 1, 1002, theftPolicy,
            inventoryTransfer) != HeldItemInventoryTransferResult::ProtectedItem || transferRecords[1].stackCount != 1)
        return 368;
    transferRecords[1].transferable = true;
    theftPolicy.resolved = true;
    theftPolicy.blockedByAbility = true;
    if (applySelectedHeldItemTheft(transferRecords, 3, transferRecordCount, 1, 1002, theftPolicy,
            inventoryTransfer) != HeldItemInventoryTransferResult::BlockedByAbility || transferRecordCount != 2)
        return 369;
    theftPolicy.blockedByAbility = false;
    theftPolicy.matchingTargetIndex = static_cast<size_t>(-1);
    transferRecords[1].stackCount = 2;
    if (applySelectedHeldItemTheft(transferRecords, 2, transferRecordCount, 1, 1003, theftPolicy,
            inventoryTransfer) != HeldItemInventoryTransferResult::StorageCapacity || transferRecordCount != 2 ||
        transferRecords[1].stackCount != 2 || transferRecords[1].ownerPokemonId != 1001) return 370;
    NativeHeldModifierInstance nativeTheftRecords[3]{};
    nativeTheftRecords[0] = otherHolder;
    nativeTheftRecords[0].ownerPokemonId = 1001;
    nativeTheftRecords[1] = blackHoleInstance; // The boss-held copy cannot be stolen.
    nativeTheftRecords[1].ownerPokemonId = 1001;
    nativeTheftRecords[2] = otherHolder;
    nativeTheftRecords[2].ownerPokemonId = 1002;
    actualTheftRng = rng;
    expectedTheftRng = rng;
    const auto nativeOpponent = static_cast<size_t>(expectedTheftRng.randSeedInt(2));
    expectedTheftRng.randSeedInt(1);
    if (selectNativeHeldItemTransferAttempt(theftOpponents, 2, nativeTheftRecords, 3, 3, 1,
            actualTheftRng, theftSelection) != HeldItemTransferSelectionResult::Selected ||
        !theftSelection.itemFound || theftSelection.opponentIndex != nativeOpponent ||
        theftSelection.inventoryIndex != (nativeOpponent ? 2u : 0u)) return 371;
    actualTheftState = actualTheftRng.state();
    expectedTheftState = expectedTheftRng.state();
    if (actualTheftState.carry != expectedTheftState.carry || actualTheftState.s0 != expectedTheftState.s0 ||
        actualTheftState.s1 != expectedTheftState.s1 || actualTheftState.s2 != expectedTheftState.s2) return 372;
    nativeTheftRecords[0].stackCount = 0;
    const auto beforeInvalidInventorySelection = actualTheftRng.state();
    if (selectNativeHeldItemTransferAttempt(theftOpponents, 2, nativeTheftRecords, 3, 3, 1,
            actualTheftRng, theftSelection) != HeldItemTransferSelectionResult::InvalidState ||
        actualTheftRng.state().s0 != beforeInvalidInventorySelection.s0 ||
        selectNativeHeldItemTransferAttempt(theftOpponents, 2, nativeTheftRecords, 2, 3, 1,
            actualTheftRng, theftSelection) != HeldItemTransferSelectionResult::InvalidState) return 373;
    auto heldForCodec = blackHoleInstance;
    std::strcpy(heldForCodec.rawArguments, "{\n\"unknown\": [1, 2]\n}");
    char heldPayload[1024]{};
    size_t heldPayloadSize = 0;
    NativeHeldModifierInstance decodedHeld{};
    if (encodeNativeHeldModifier(heldForCodec, heldPayload, sizeof(heldPayload), heldPayloadSize) !=
            NativeSaveResult::Ok || decodeNativeHeldModifier(heldPayload, heldPayloadSize, decodedHeld) !=
            NativeSaveResult::Ok || decodedHeld.ownerPokemonId != heldForCodec.ownerPokemonId ||
        decodedHeld.stackCount != heldForCodec.stackCount || decodedHeld.transferable ||
        std::strcmp(decodedHeld.rawArguments, heldForCodec.rawArguments) ||
        std::strcmp(heldModifierDefinition(decodedHeld)->id, "MINI_BLACK_HOLE")) return 379;
    char repeatedHeldPayload[1024]{};
    size_t repeatedHeldSize = 0;
    if (encodeNativeHeldModifier(decodedHeld, repeatedHeldPayload, sizeof(repeatedHeldPayload), repeatedHeldSize) !=
            NativeSaveResult::Ok || repeatedHeldSize != heldPayloadSize ||
        std::memcmp(repeatedHeldPayload, heldPayload, heldPayloadSize)) return 380;
    const auto beforeInvalidHeldDecode = decodedHeld;
    if (decodeNativeHeldModifier(heldPayload, heldPayloadSize - 1, decodedHeld) != NativeSaveResult::InvalidFormat ||
        decodedHeld.ownerPokemonId != beforeInvalidHeldDecode.ownerPokemonId) return 381;
    heldPayload[7] = 'X'; // Unknown canonical ID cannot silently resolve to another catalog row.
    if (decodeNativeHeldModifier(heldPayload, heldPayloadSize, decodedHeld) != NativeSaveResult::InvalidRecord ||
        decodedHeld.canonicalItemIndex != beforeInvalidHeldDecode.canonicalItemIndex) return 382;
    size_t tooSmallHeldSize = 99;
    if (encodeNativeHeldModifier(heldForCodec, repeatedHeldPayload, 1, tooSmallHeldSize) !=
            NativeSaveResult::TooLarge || tooSmallHeldSize) return 383;
    HeldItemTheftPolicy canonicalTheftPolicy{};
    if (resolveTurnHeldItemTransferMatchPolicy(otherHolder, nullptr, 0, 1234, false, false,
            canonicalTheftPolicy) != HeldItemMatchPolicyResult::UnresolvedAbilities || canonicalTheftPolicy.resolved)
        return 389;
    if (resolveTurnHeldItemTransferMatchPolicy(otherHolder, nullptr, 0, 1234, true, false,
            canonicalTheftPolicy) != HeldItemMatchPolicyResult::Resolved || !canonicalTheftPolicy.resolved ||
        canonicalTheftPolicy.targetMaxStack != 1 || canonicalTheftPolicy.matchingTargetIndex != static_cast<size_t>(-1))
        return 390;
    NativeHeldModifierInstance matchingBlackHole = blackHoleInstance;
    matchingBlackHole.ownerPokemonId = 1234;
    if (resolveTurnHeldItemTransferMatchPolicy(otherHolder, &matchingBlackHole, 1, 1234, true, false,
            canonicalTheftPolicy) != HeldItemMatchPolicyResult::Resolved || canonicalTheftPolicy.matchingTargetIndex != 0)
        return 391;
    auto opaqueSource = otherHolder;
    std::strcpy(opaqueSource.rawArguments, "{\"future\":true}");
    const auto policyBeforeOpaque = canonicalTheftPolicy;
    if (resolveTurnHeldItemTransferMatchPolicy(opaqueSource, nullptr, 0, 1234, true, false,
            canonicalTheftPolicy) != HeldItemMatchPolicyResult::UnresolvedArguments ||
        canonicalTheftPolicy.matchingTargetIndex != policyBeforeOpaque.matchingTargetIndex) return 392;
    if (resolveTurnHeldItemTransferMatchPolicy(otherHolder, nullptr, 0, 1234, true, true,
            canonicalTheftPolicy) != HeldItemMatchPolicyResult::Resolved || !canonicalTheftPolicy.blockedByAbility)
        return 393;
    uint16_t blockingTheftAbility = 0, pendingLostAbility = 0;
    for (const auto& profile : PokerogueContent::kHeldItemTheftAbilityProfiles) {
        if (profile.blocksTheft && !profile.conditionalCallbacks) blockingTheftAbility = profile.abilityId;
        if (profile.requiresPostLostDispatcher && !profile.blocksTheft && !profile.conditionalCallbacks)
            pendingLostAbility = profile.abilityId;
    }
    if (!blockingTheftAbility || !pendingLostAbility) return 394;
    bool canonicalBlocked = false;
    if (resolveHeldItemTheftAbilityPolicy(&blockingTheftAbility, 1, false, canonicalBlocked) !=
            HeldItemTheftAbilityPolicyResult::UnresolvedApplicability || canonicalBlocked) return 395;
    if (resolveHeldItemTheftAbilityPolicy(&blockingTheftAbility, 1, true, canonicalBlocked) !=
            HeldItemTheftAbilityPolicyResult::Resolved || !canonicalBlocked) return 396;
    if (resolveHeldItemTheftAbilityPolicy(&pendingLostAbility, 1, true, canonicalBlocked) !=
            HeldItemTheftAbilityPolicyResult::UnresolvedCallbacks || !canonicalBlocked) return 397;
    const uint16_t combinedTheftAbilities[] = {blockingTheftAbility, pendingLostAbility};
    if (resolveHeldItemTheftAbilityPolicy(combinedTheftAbilities, 2, true, canonicalBlocked) !=
            HeldItemTheftAbilityPolicyResult::Resolved || !canonicalBlocked) return 398;
    const uint16_t pressureTheftAbility = 46;
    if (resolveHeldItemTheftAbilityPolicy(&pressureTheftAbility, 1, true, canonicalBlocked) !=
            HeldItemTheftAbilityPolicyResult::Resolved || canonicalBlocked) return 399;
    const uint16_t unknownTheftAbility = 65535;
    if (resolveHeldItemTheftAbilityPolicy(&unknownTheftAbility, 1, true, canonicalBlocked) !=
            HeldItemTheftAbilityPolicyResult::UnknownAbility || canonicalBlocked) return 400;
    HeldItemLostTagState lostTags{};
    if (applyHeldItemLostCallbacks(&pendingLostAbility, 1, true, true, lostTags) !=
            HeldItemLostCallbackResult::NoChange || lostTags.unburden) return 401;
    if (applyHeldItemLostCallbacks(&pendingLostAbility, 1, true, false, lostTags) !=
            HeldItemLostCallbackResult::Applied || !lostTags.unburden) return 402;
    if (applyHeldItemLostCallbacks(&pendingLostAbility, 1, true, false, lostTags) !=
            HeldItemLostCallbackResult::NoChange) return 403;
    const uint16_t invalidLostSet[] = {pendingLostAbility, 65535};
    lostTags = {};
    if (applyHeldItemLostCallbacks(invalidLostSet, 2, true, false, lostTags) !=
            HeldItemLostCallbackResult::UnknownAbility || lostTags.unburden) return 404;
    NativeHeldModifierInstance callbackRecords[2]{};
    callbackRecords[0] = otherHolder;
    callbackRecords[0].ownerPokemonId = 1001;
    callbackRecords[0].stackCount = 1;
    size_t callbackCount = 1;
    PokemonBattleState callbackActor{};
    callbackActor.pokemonId = 1001;
    HeldItemTheftPolicy callbackPolicy{};
    callbackPolicy.resolved = true;
    callbackPolicy.targetMaxStack = 1;
    HeldItemInventoryTransferEvent callbackEvent{};
    if (applyHeldItemTheftWithCallbacks(callbackRecords, 2, callbackCount, 0, callbackActor, 1002,
            callbackPolicy, &pendingLostAbility, 1, false, true, callbackEvent) !=
            HeldItemInventoryTransferResult::UnresolvedPolicy || callbackActor.heldItemLostTags.unburden ||
        callbackRecords[0].ownerPokemonId != 1001) return 410;
    if (applyHeldItemTheftWithCallbacks(callbackRecords, 2, callbackCount, 0, callbackActor, 1002,
            callbackPolicy, &blockingTheftAbility, 1, true, true, callbackEvent) !=
            HeldItemInventoryTransferResult::BlockedByAbility || callbackActor.heldItemLostTags.unburden ||
        callbackRecords[0].ownerPokemonId != 1001) return 411;
    callbackRecords[1] = callbackRecords[0];
    callbackRecords[1].ownerPokemonId = 1002;
    callbackCount = 2;
    callbackPolicy.matchingTargetIndex = 1;
    if (applyHeldItemTheftWithCallbacks(callbackRecords, 2, callbackCount, 0, callbackActor, 1002,
            callbackPolicy, &pendingLostAbility, 1, true, true, callbackEvent) !=
            HeldItemInventoryTransferResult::NoCapacity || callbackActor.heldItemLostTags.unburden ||
        callbackCount != 2 || callbackRecords[0].ownerPokemonId != 1001) return 413;
    callbackCount = 1;
    callbackRecords[1] = {};
    callbackPolicy.matchingTargetIndex = static_cast<size_t>(-1);
    if (applyHeldItemTheftWithCallbacks(callbackRecords, 2, callbackCount, 0, callbackActor, 1002,
            callbackPolicy, &pendingLostAbility, 1, true, true, callbackEvent) !=
            HeldItemInventoryTransferResult::Transferred || !callbackActor.heldItemLostTags.unburden ||
        callbackRecords[0].ownerPokemonId != 1002 || callbackEvent.sourcePokemonId != 1001) return 412;
    HeldAbilityApplicabilityContext abilityContext{};
    HeldApplicableAbilitySet applicableSet{};
    if (resolveHeldApplicableAbilities(60, 84, true, abilityContext, applicableSet) !=
        HeldAbilitySetResult::UnresolvedContext) return 414;
    abilityContext.resolved = true;
    abilityContext.alive = false;
    if (resolveHeldApplicableAbilities(60, 84, true, abilityContext, applicableSet) !=
        HeldAbilitySetResult::Resolved || applicableSet.count != 2) return 415;
    abilityContext.ignoreAbilitiesFromOtherActor = true;
    if (resolveHeldApplicableAbilities(60, 84, true, abilityContext, applicableSet) !=
        HeldAbilitySetResult::Resolved || applicableSet.count != 1 || applicableSet.ids[0] != 84) return 416;
    abilityContext.suppressed = true;
    if (resolveHeldApplicableAbilities(60, 84, true, abilityContext, applicableSet) !=
        HeldAbilitySetResult::Resolved || applicableSet.count) return 417;
    abilityContext = {};
    abilityContext.resolved = true;
    if (resolveHeldApplicableAbilities(84, 84, true, abilityContext, applicableSet) !=
        HeldAbilitySetResult::Resolved || applicableSet.count != 1) return 418;
    callbackRecords[0].ownerPokemonId = callbackActor.pokemonId;
    callbackActor.heldItemLostTags = {};
    const uint16_t sameSideAbilities[] = {blockingTheftAbility, pendingLostAbility};
    if (applyHeldItemTheftWithCallbacks(callbackRecords, 2, callbackCount, 0, callbackActor, 1002,
            callbackPolicy, sameSideAbilities, 2, true, true, callbackEvent, false) !=
            HeldItemInventoryTransferResult::Transferred || !callbackActor.heldItemLostTags.unburden)
        return 419;
    callbackRecords[0].ownerPokemonId = callbackActor.pokemonId;
    callbackActor.heldItemLostTags = {};
    if (applyHeldItemTheftWithCallbacks(callbackRecords, 2, callbackCount, 0, callbackActor, 1002,
            callbackPolicy, &pendingLostAbility, 1, true, false, callbackEvent, false) !=
            HeldItemInventoryTransferResult::Transferred || callbackActor.heldItemLostTags.unburden)
        return 420;
    auto activationHolder = callbackActor;
    activationHolder.pokemonId = 1002;
    activationHolder.hp = 1;
    auto activationItem = otherHolder;
    activationItem.ownerPokemonId = 1002;
    activationItem.stackCount = 1;
    activationItem.rawArguments[0] = 0;
    callbackRecords[0] = activationItem;
    callbackRecords[0].ownerPokemonId = callbackActor.pokemonId;
    callbackCount = 1;
    HeldTransferOpponent activationOpponent{};
    activationOpponent.actor = &callbackActor;
    activationOpponent.applicabilityResolved = true;
    activationOpponent.abilities.ids[0] = pendingLostAbility;
    activationOpponent.abilities.count = 1;
    PokerogueRngAdapter activationRng{};
    const auto miniMatch = [](const NativeHeldModifierInstance& source,
        const NativeHeldModifierInstance* inventory, size_t inventoryCount, uint32_t target,
        HeldItemTheftPolicy& result) {
        return resolveTurnHeldItemTransferMatchPolicy(source, inventory, inventoryCount, target, true, false, result);
    };
    callbackActor.heldItemLostTags = {};
    if (activateTurnHeldItemTransfer(activationItem, activationHolder, &activationOpponent, 1,
            callbackRecords, 2, callbackCount, activationRng, callbackEvent, miniMatch) !=
            TurnHeldTransferResult::Transferred || !callbackActor.heldItemLostTags.unburden ||
        callbackRecords[0].ownerPokemonId != 1002) return 421;
    callbackRecords[0].ownerPokemonId = callbackActor.pokemonId;
    activationOpponent.abilities.ids[0] = blockingTheftAbility;
    auto expectedActivationRng = activationRng;
    expectedActivationRng.randSeedInt(1);
    expectedActivationRng.randSeedInt(1);
    callbackActor.heldItemLostTags = {};
    if (activateTurnHeldItemTransfer(activationItem, activationHolder, &activationOpponent, 1,
            callbackRecords, 2, callbackCount, activationRng, callbackEvent, miniMatch) !=
            TurnHeldTransferResult::Blocked || callbackRecords[0].ownerPokemonId != callbackActor.pokemonId ||
        callbackActor.heldItemLostTags.unburden || activationRng.state().s0 != expectedActivationRng.state().s0 ||
        activationRng.state().s1 != expectedActivationRng.state().s1 ||
        activationRng.state().s2 != expectedActivationRng.state().s2 ||
        activationRng.state().carry != expectedActivationRng.state().carry) return 423;
    activationHolder.hp = 0;
    if (activateTurnHeldItemTransfer(activationItem, activationHolder, &activationOpponent, 1,
            callbackRecords, 2, callbackCount, activationRng, callbackEvent, miniMatch) !=
            TurnHeldTransferResult::HolderFainted) return 422;
    NativeHeldModifierInstance healingItems[3]{};
    if (initializeHeldModifierInstance("LEFTOVERS", 1001, 2, true, nullptr, healingItems[0]) !=
            HeldModifierStorageResult::Ok ||
        initializeHeldModifierInstance("LEFTOVERS", 1002, 3, true, nullptr, healingItems[1]) !=
            HeldModifierStorageResult::Ok ||
        initializeHeldModifierInstance("SHELL_BELL", 1002, 1, true, nullptr, healingItems[2]) !=
            HeldModifierStorageResult::Ok) return 424;
    HeldItemTheftPolicy healingMatch{};
    if (resolveKnownHeldModifierMatchPolicy(healingItems[0], healingItems, 3, 1002, healingMatch) !=
        HeldItemMatchPolicyResult::Resolved || healingMatch.matchingTargetIndex != 1 ||
        healingMatch.targetMaxStack != 4) return 425;
    size_t healingCount = 3;
    if (applySelectedHeldItemTheft(healingItems, 3, healingCount, 0, 1002, healingMatch, callbackEvent) !=
        HeldItemInventoryTransferResult::Transferred || healingCount != 3 ||
        healingItems[0].stackCount != 1 || healingItems[2].stackCount != 4 ||
        std::strcmp(heldModifierDefinition(healingItems[1])->id, "SHELL_BELL")) return 426;
    PokemonBattleState heldHealActor{};
    heldHealActor.pokemonId = 1001;
    heldHealActor.maxHp = 31;
    heldHealActor.hp = 10;
    PokemonHealingPolicy heldHealPolicy{};
    PokemonHealingEvent heldHealEvent{};
    if (applyHeldHealingModifier(healingItems[0], heldHealActor, 0, true, heldHealPolicy, heldHealEvent) !=
        HeldHealingResult::UnresolvedPolicy || heldHealActor.hp != 10) return 427;
    heldHealPolicy.resolved = true;
    healingItems[0].stackCount = 4;
    if (applyHeldHealingModifier(healingItems[0], heldHealActor, 0, true, heldHealPolicy, heldHealEvent) !=
        HeldHealingResult::Resolved || heldHealEvent.healed != 4 || heldHealActor.hp != 14) return 428;
    healingItems[1].ownerPokemonId = 1001;
    healingItems[1].stackCount = 3;
    if (applyHeldHealingModifier(healingItems[1], heldHealActor, 5, true, heldHealPolicy, heldHealEvent) !=
        HeldHealingResult::Resolved || heldHealEvent.healed != 1 || heldHealActor.hp != 15) return 429;
    heldHealPolicy.healBlocked = true;
    if (applyHeldHealingModifier(healingItems[0], heldHealActor, 0, true, heldHealPolicy, heldHealEvent) !=
        HeldHealingResult::Resolved || !heldHealEvent.blocked || heldHealActor.hp != 15) return 430;
    heldHealPolicy.healBlocked = false;
    heldHealActor.hp = 10;
    if (applyHeldTurnHealingPhase(healingItems, 3, heldHealActor, true, heldHealPolicy, heldHealEvent) !=
        HeldHealingResult::Resolved || heldHealActor.hp != 14 || heldHealEvent.healed != 4) return 431;
    healingItems[2] = healingItems[0];
    healingItems[2].stackCount = 5;
    if (applyHeldTurnHealingPhase(healingItems, 3, heldHealActor, true, heldHealPolicy, heldHealEvent) !=
        HeldHealingResult::InvalidState || heldHealActor.hp != 14) return 432;
    heldHealActor.hp = 10;
    heldHealActor.turnDamageDealt = 19;
    healingItems[2] = {};
    if (applyHeldMoveHealingPhase(healingItems, 2, heldHealActor, true, heldHealPolicy, heldHealEvent) !=
        HeldHealingResult::Resolved || heldHealEvent.healed != 7 || heldHealActor.hp != 17 ||
        heldHealActor.turnDamageDealt != 19) return 433;
    // Move-end healing reads accumulated turn damage, even if this move added none.
    if (applyHeldMoveHealingPhase(healingItems, 2, heldHealActor, true, heldHealPolicy, heldHealEvent) !=
        HeldHealingResult::Resolved || heldHealEvent.healed != 7 || heldHealActor.hp != 24) return 434;
    heldHealActor.turnDamageDealt = 0;
    if (applyHeldMoveHealingPhase(healingItems, 2, heldHealActor, true, heldHealPolicy, heldHealEvent) !=
        HeldHealingResult::Resolved || heldHealEvent.healed || heldHealActor.hp != 24) return 435;
    if (!heldHealingInventorySupported(healingItems, 2)) return 436;
    healingItems[2] = healingItems[0];
    if (heldHealingInventorySupported(healingItems, 3)) return 437;
    healingItems[2] = activationItem;
    if (heldHealingInventorySupported(healingItems, 3)) return 438;
    healingItems[2] = healingItems[0];
    healingItems[2].ownerPokemonId = 2000;
    size_t retainedCount = 3;
    const uint32_t retainedPartyIds[] = {1001};
    if (!retainPartyHeldInventory(healingItems, 3, retainedCount, retainedPartyIds, 1) ||
        retainedCount != 2 || healingItems[0].ownerPokemonId != 1001 ||
        healingItems[1].ownerPokemonId != 1001 || healingItems[2].stackCount) return 442;
    NativeHeldModifierInstance rewardItems[2]{};
    size_t rewardItemCount = 0;
    auto healingReward = healingItems[0];
    healingReward.stackCount = 1;
    if (addKnownHealingHeldReward(rewardItems, 2, rewardItemCount, healingReward) !=
        HeldRewardAddResult::Added || rewardItemCount != 1) return 443;
    if (addKnownHealingHeldReward(rewardItems, 2, rewardItemCount, healingReward) !=
        HeldRewardAddResult::Merged || rewardItems[0].stackCount != 2 || rewardItemCount != 1) return 444;
    healingReward.stackCount = 3;
    if (addKnownHealingHeldReward(rewardItems, 2, rewardItemCount, healingReward) !=
        HeldRewardAddResult::FullStackNeedsReplacement || rewardItems[0].stackCount != 2) return 445;
    const auto* hyperRestore = hpRestoreItemProfile("HYPER_POTION");
    const auto* fullRestore = hpRestoreItemProfile("FULL_RESTORE");
    if (!hyperRestore || !fullRestore || hyperRestore->points != 200 || hyperRestore->percent != 50)
        return 448;
    auto potionActor = heldHealActor;
    potionActor.maxHp = 1000;
    potionActor.hp = 1;
    uint16_t potionHealed = 0;
    if (!applyPokemonHpRestoreItem(potionActor, *hyperRestore, 1.5, true, potionHealed) ||
        potionHealed != 500 || potionActor.hp != 501) return 449;
    if (applyPokemonHpRestoreItem(potionActor, *fullRestore, 1.0, false, potionHealed) ||
        potionActor.hp != 501) return 450;
    const auto* etherRestore = ppRestoreItemProfile("ETHER");
    const auto* maxElixirRestore = ppRestoreItemProfile("MAX_ELIXIR");
    if (!etherRestore || !maxElixirRestore || etherRestore->allMoves || !maxElixirRestore->allMoves)
        return 451;
    potionActor.moveCount = 2;
    potionActor.moves[0].maxPp = 20; potionActor.moves[0].pp = 5;
    potionActor.moves[1].maxPp = 10; potionActor.moves[1].pp = 0;
    if (!applyPokemonPpRestoreItem(potionActor, *etherRestore, 0) || potionActor.moves[0].pp != 15 ||
        potionActor.moves[1].pp) return 452;
    if (applyPokemonPpRestoreItem(potionActor, *etherRestore, 2) || potionActor.moves[0].pp != 15) return 453;
    if (!applyPokemonPpRestoreItem(potionActor, *maxElixirRestore, 255) || potionActor.moves[0].pp != 20 ||
        potionActor.moves[1].pp != 10) return 454;
    const auto* reviveProfile = reviveItemProfile("REVIVE");
    if (!reviveProfile || reviveProfile->percent != 50) return 456;
    potionActor.maxHp = 31; potionActor.hp = 0;
    if (applyPokemonReviveItem(potionActor, *reviveProfile, false, false, true) || potionActor.hp)
        return 457;
    if (!applyPokemonReviveItem(potionActor, *reviveProfile, true, false, true) || potionActor.hp != 15)
        return 458;
    if (applyPokemonReviveItem(potionActor, *reviveProfile, true, false, true) || potionActor.hp != 15)
        return 459;
    const auto* ashProfile = reviveItemProfile("SACRED_ASH");
    if (!ashProfile || !ashProfile->allParty) return 460;
    auto faintedReserve = potionActor;
    faintedReserve.hp = 0;
    PokemonBattleState* revivalParty[] = {&potionActor, &faintedReserve};
    if (!applyPokemonPartyReviveItem(revivalParty, 2, *ashProfile, true, false, true) ||
        potionActor.hp != 15 || faintedReserve.hp != faintedReserve.maxHp) return 461;
    faintedReserve.hp = 0;
    potionActor.hp = potionActor.maxHp + 1;
    if (applyPokemonPartyReviveItem(revivalParty, 2, *ashProfile, true, false, true) || faintedReserve.hp)
        return 462;
    const auto* ballReward = pokeballRewardProfile("POKEBALL");
    if (!ballReward || ballReward->count != 5) return 463;
    uint16_t rewardBallCounts[5] = {97, 1, 2, 3, 4};
    if (!applyPokeballReward(*ballReward, rewardBallCounts, 5) || rewardBallCounts[0] != 99 ||
        rewardBallCounts[1] != 1) return 464;
    auto invalidBallReward = *ballReward;
    invalidBallReward.ballSymbol = "UNKNOWN_FUTURE_BALL";
    if (applyPokeballReward(invalidBallReward, rewardBallCounts, 5) || rewardBallCounts[0] != 99) return 465;
    const auto preservedBoss = bossActor;
    if (applyPokemonFixedEnemyMovePp(*firstPhase, bossActor) ||
        bossActor.moves[3].maxPp != preservedBoss.moves[3].maxPp) return 338;


    return 0;
}

static int checkExtendedWaveAndBiomeSaveValidation() {
    using namespace Pokerogue3DS;
    NativeRunSave save{};
    save.saveVersion = kNativeSaveVersion;
    save.runtimeVersion = kNativeSaveRuntimeVersion;
    save.stage = NativeSaveStage::BattleActive;
    save.seed = 12345;
    save.starterDex = 1;
    save.playerLevel = 5;
    const auto* starter = PokerogueContent::findSpeciesByDex(1);
    if (!starter || pokemonTotalExperienceForLevel(starter->growthRate, 5, save.playerExperience) != PokemonExperienceResult::Ok) return 120;
    std::memcpy(save.contentHash, PokerogueContent::kContentHash, 64);
    std::strcpy(save.modeId, "classic");
    std::strcpy(save.biomeId, "plains");
    save.wave = 1;
    save.encounterDex = 16;
    save.playerHp = 20;
    save.enemyHp = 20;
    save.battleTurn = 1;
    save.playerMoveCount = 1;
    save.playerMoveIds[0] = 33;
    save.playerPp[0] = 35;
    save.enemyMoveCount = 1;
    save.enemyMoveIds[0] = 33;
    save.enemyPp[0] = 35;

    if (validateNativeRunSave(save, PokerogueContent::kContentHash) != NativeSaveResult::Ok) return 121;

    save.wave = 50;
    save.playerLevel = classicExperienceLevelCap(50);
    pokemonTotalExperienceForLevel(starter->growthRate, save.playerLevel, save.playerExperience);
    std::strcpy(save.biomeId, "forest");
    if (validateNativeRunSave(save, PokerogueContent::kContentHash) != NativeSaveResult::Ok) return 122;

    save.wave = 200;
    save.playerLevel = 100;
    pokemonTotalExperienceForLevel(starter->growthRate, save.playerLevel, save.playerExperience);
    std::strcpy(save.biomeId, "end");
    if (validateNativeRunSave(save, PokerogueContent::kContentHash) != NativeSaveResult::Ok) return 123;

    save.wave = 201;
    if (validateNativeRunSave(save, PokerogueContent::kContentHash) == NativeSaveResult::Ok) return 124;

    save.wave = 100;
    std::strcpy(save.biomeId, "nonexistent_biome_123");
    if (validateNativeRunSave(save, PokerogueContent::kContentHash) == NativeSaveResult::Ok) return 125;

    return 0;
}

static int checkPokeballCaptureMechanics() {
    using namespace Pokerogue3DS;
    if (getPokeballCatchMultiplier(PokeballType::Pokeball) != 1.0) return 130;
    if (getPokeballCatchMultiplier(PokeballType::GreatBall) != 1.5) return 131;
    if (getPokeballCatchMultiplier(PokeballType::UltraBall) != 2.0) return 132;
    if (getPokeballCatchMultiplier(PokeballType::RogueBall) != 3.0) return 133;
    if (getPokeballCatchMultiplier(PokeballType::MasterBall) != -1.0) return 134;

    if (speciesCatchRate(1) != 45) return 135;   // Bulbasaur
    if (speciesCatchRate(16) != 255) return 136; // Pidgey
    if (speciesCatchRate(150) != 3) return 137;  // Mewtwo

    // Direct executeCaptureAttempt blockers:
    PokemonBattleState targetState{};
    targetState.speciesDex = 16;
    targetState.hp = 20;
    targetState.maxHp = 20;

    PokerogueRngAdapter rng;
    uint16_t seed[PokerogueRngAdapter::kMaxSeedCodeUnits] = {'c', 'a', 'p', 't'};
    rng.sow(seed, 4);

    PokemonCaptureEvent outEvent{};
    // Trainer battle blocker
    if (executeCaptureAttempt(targetState, PokeballType::Pokeball, true, false, false, false, rng, outEvent) ||
        outEvent.blocker != CaptureBlocker::TrainerBattle) return 138;

    // Double battle multiple foes blocker
    if (executeCaptureAttempt(targetState, PokeballType::Pokeball, false, true, false, false, rng, outEvent) ||
        outEvent.blocker != CaptureBlocker::MultipleEnemies) return 139;

    // Target fainted blocker
    PokemonBattleState faintedState = targetState;
    faintedState.hp = 0;
    if (executeCaptureAttempt(faintedState, PokeballType::Pokeball, false, false, false, false, rng, outEvent) ||
        outEvent.blocker != CaptureBlocker::TargetFainted) return 140;

    // Wave 200 final boss blocker
    if (executeCaptureAttempt(targetState, PokeballType::Pokeball, false, false, false, true, rng, outEvent) ||
        outEvent.blocker != CaptureBlocker::FinalBossUncatchable) return 141;

    // Boss shield blocker
    if (executeCaptureAttempt(targetState, PokeballType::Pokeball, false, false, true, false, rng, outEvent) ||
        outEvent.blocker != CaptureBlocker::BossShieldActive) return 142;

    PokemonBossState shieldState{3, 2, false, false};
    uint16_t wonderGuardId = 0, ordinaryAbilityId = 0;
    for (const auto& profile : PokerogueContent::kAbilityMovegenProfiles) {
        if (std::strcmp(profile.sourceSymbol, "AbilityId.WONDER_GUARD") == 0) wonderGuardId = profile.abilityId;
        else if (!ordinaryAbilityId) ordinaryAbilityId = profile.abilityId;
    }
    bool blocked = false;
    if (!wonderGuardId || !ordinaryAbilityId ||
        !pokemonBossShieldCaptureBlocked(shieldState, ordinaryAbilityId, PokeballType::Pokeball, blocked) || !blocked) return 256;
    if (!pokemonBossShieldCaptureBlocked(shieldState, ordinaryAbilityId, PokeballType::MasterBall, blocked) || blocked) return 257;
    if (!pokemonBossShieldCaptureBlocked(shieldState, wonderGuardId, PokeballType::Pokeball, blocked) || blocked) return 258;
    shieldState.segmentIndex = 0;
    if (!pokemonBossShieldCaptureBlocked(shieldState, ordinaryAbilityId, PokeballType::Pokeball, blocked) || blocked) return 259;
    if (!executeCaptureAttempt(targetState, PokeballType::MasterBall, false, false, true, false, rng, outEvent) ||
        !outEvent.caught) return 260;

    // Master ball guaranteed catch
    if (!executeCaptureAttempt(targetState, PokeballType::MasterBall, false, false, false, false, rng, outEvent) ||
        !outEvent.caught || outEvent.shakeCount != 3) return 143;

    // Runtime-level tests:
    FirstRunRuntime game(1);
    if (game.pokeballCount(PokeballType::Pokeball) != 5) return 144;
    if (game.pokeballCount(PokeballType::GreatBall) != 0) return 145;
    if (game.pokeballCount(PokeballType::MasterBall) != 0) return 146;

    // Capture blocked in trainer battle (wave 5)
    NativeRunSave trainerSave{};
    game.captureNativeRunSave(trainerSave);
    trainerSave.wave = 5;
    trainerSave.stage = NativeSaveStage::BattleActive;
    if (game.restoreNativeRunSave(trainerSave)) {
        if (game.throwPokeball(PokeballType::Pokeball)) return 147;
        if (game.pokeballCount(PokeballType::Pokeball) != 5) return 148;
    }

    // Capture blocked against Wave 200 final boss
    FirstRunRuntime bossGame(1);
    NativeRunSave bossSave{};
    bossGame.captureNativeRunSave(bossSave);
    bossSave.wave = 200;
    bossSave.playerLevel = 100;
    bossSave.stage = NativeSaveStage::BattleActive;
    std::strcpy(bossSave.biomeId, "end");
    if (bossGame.restoreNativeRunSave(bossSave)) {
        if (bossGame.throwPokeball(PokeballType::Pokeball)) return 149;
    }

    // Wild catch test with standard Poké Ball on weakened wild mon:
    FirstRunRuntime wildGame(1);
    NativeRunSave wildSave{};
    wildGame.captureNativeRunSave(wildSave);
    wildSave.wave = 1;
    wildSave.stage = NativeSaveStage::BattleActive;
    wildSave.encounterDex = wildGame.presentation().enemy.dex;
    wildSave.enemyHp = 1;
    if (!wildGame.restoreNativeRunSave(wildSave)) return 150;
    const uint8_t initialPartyCount = wildGame.playerPartyCount();
    if (initialPartyCount != 1) return 151;
    auto heldCaptureGame = wildGame;
    NativeHeldModifierInstance captureHeld{};
    const uint32_t capturePid = wildGame.presentation().enemy.battleState.pokemonId;
    if (initializeHeldModifierInstance("LEFTOVERS", capturePid, 1, true, nullptr, captureHeld) !=
            HeldModifierStorageResult::Ok || !heldCaptureGame.restoreHeldModifierInventory(&captureHeld, 1) ||
        !heldCaptureGame.throwPokeball(PokeballType::Pokeball) || heldCaptureGame.playerPartyCount() != 2 ||
        heldCaptureGame.playerPartyMember(1)->battleState.pokemonId != capturePid ||
        heldCaptureGame.heldModifierCount() != 1 || heldCaptureGame.heldModifier(0)->ownerPokemonId != capturePid)
        return 441;
    if (!wildGame.throwPokeball(PokeballType::Pokeball)) return 152;
    if (wildGame.pokeballCount(PokeballType::Pokeball) != 4) return 153;
    if (!wildGame.battleFinished() || !wildGame.playerWon()) return 154;
    if (wildGame.playerPartyCount() != 2) return 155;
    if (wildGame.playerPartyMember(1)->dex != wildSave.encounterDex) return 156;
    if (wildGame.playerPartyMember(1)->battleState.hp != 1) return 224;
    if (wildGame.presentation().enemy.battleState.hp != 0) return 225;

    return 0;
}

static int checkPlayerPartyManagementAndSwitching() {
    using namespace Pokerogue3DS;
    FirstRunRuntime game(1);
    if (game.playerPartyCount() != 1) return 160;
    if (game.activePlayerPartyIndex() != 0) return 161;
    if (game.playerPartyMember(0) == nullptr) return 162;
    if (game.playerPartyMember(1) != nullptr) return 163;
    if (game.playerPartyDefeated()) return 164;

    // Invalid switch attempts:
    if (game.switchPlayerPokemon(0)) return 165; // Already active
    if (game.switchPlayerPokemon(1)) return 166; // Index out of bounds

    // Setup wild capture to grow party to 2:
    NativeRunSave wildSave{};
    game.captureNativeRunSave(wildSave);
    wildSave.wave = 1;
    wildSave.stage = NativeSaveStage::BattleActive;
    wildSave.encounterDex = game.presentation().enemy.dex;
    wildSave.enemyHp = 1;
    if (!game.restoreNativeRunSave(wildSave)) return 167;
    if (!game.throwPokeball(PokeballType::Pokeball)) return 168;
    if (game.playerPartyCount() != 2) return 169;
    const uint16_t caughtDex = game.playerPartyMember(1)->dex;
    if (caughtDex != wildSave.encounterDex) return 170;

    // Advance turn & skip reward into wave 2:
    if (!game.advanceBattleTurn()) return 171;
    if (!game.skipVictoryReward()) return 172;
    if (game.run().wave != 2) return 173;
    if (game.playerPartyCount() != 2) return 174;
    if (game.activePlayerPartyIndex() != 0) return 175;

    NativeRunSave capturedPartySave{};
    game.captureNativeRunSave(capturedPartySave);
    if (capturedPartySave.playerPartyCount != 2 || capturedPartySave.activePlayerMember != 0 ||
        validateNativeRunSave(capturedPartySave, PokerogueContent::kContentHash) != NativeSaveResult::Ok)
        return 290;
    FirstRunRuntime restoredPartyGame(7);
    if (!restoredPartyGame.restoreNativeRunSave(capturedPartySave) || restoredPartyGame.playerPartyCount() != 2 ||
        restoredPartyGame.run().wave != 2) return 291;
    for (uint8_t member = 0; member < 2; ++member) {
        const auto* before = game.playerPartyMember(member);
        const auto* after = restoredPartyGame.playerPartyMember(member);
        if (!before || !after || before->dex != after->dex || before->level != after->level ||
            before->totalExperience != after->totalExperience ||
            before->battleState.hp != after->battleState.hp ||
            before->actor.pokemonId != after->actor.pokemonId) return 292;
        for (uint8_t slot = 0; slot < before->battleState.moveCount; ++slot)
            if (before->battleState.moves[slot].moveId != after->battleState.moves[slot].moveId ||
                before->battleState.moves[slot].pp != after->battleState.moves[slot].pp) return 293;
    }
    NativeRunSave recapturedPartySave{};
    restoredPartyGame.captureNativeRunSave(recapturedPartySave);
    if (recapturedPartySave.playerPartyCount != 2 ||
        recapturedPartySave.playerParty[1].experience != capturedPartySave.playerParty[1].experience)
        return 294;
    // Restore a captured actor as active without granting it the starter's EXP.
    NativeRunSave reserveActiveSave = capturedPartySave;
    reserveActiveSave.activePlayerMember = 1;
    const auto& activeReserve = reserveActiveSave.playerParty[1];
    reserveActiveSave.playerLevel = activeReserve.level;
    reserveActiveSave.playerExperience = activeReserve.experience;
    reserveActiveSave.playerHp = activeReserve.hp;
    reserveActiveSave.playerMoveCount = activeReserve.moveCount;
    for (uint8_t slot = 0; slot < 4; ++slot) {
        reserveActiveSave.playerMoveIds[slot] = activeReserve.moveIds[slot];
        reserveActiveSave.playerPp[slot] = activeReserve.pp[slot];
    }
    for (uint8_t stat = 0; stat < 7; ++stat)
        reserveActiveSave.playerStatStages[stat] = activeReserve.statStages[stat];
    if (!restoredPartyGame.restoreNativeRunSave(reserveActiveSave) ||
        restoredPartyGame.activePlayerPartyIndex() != 1 ||
        restoredPartyGame.presentation().player.dex != caughtDex ||
        restoredPartyGame.presentation().player.totalExperience != activeReserve.experience) return 295;
    auto invalidReserveSave = reserveActiveSave;
    const auto* reserveSpecies = PokerogueContent::findSpeciesByDex(invalidReserveSave.playerParty[0].speciesDex);
    if (!reserveSpecies || pokemonTotalExperienceForLevel(reserveSpecies->growthRate,
            invalidReserveSave.playerParty[0].level + 1, invalidReserveSave.playerParty[0].experience) !=
            PokemonExperienceResult::Ok) return 296;
    if (validateNativeRunSave(invalidReserveSave, PokerogueContent::kContentHash) !=
            NativeSaveResult::InvalidRecord || restoredPartyGame.restoreNativeRunSave(invalidReserveSave) ||
        restoredPartyGame.presentation().player.dex != caughtDex) return 297;
    const auto starterExperience = game.presentation().player.totalExperience;
    const auto reserveExperience = game.playerPartyMember(1)->totalExperience;
    const auto* caughtSpecies = PokerogueContent::findSpeciesByDex(caughtDex);
    uint32_t expectedReserveExperience = 0;
    if (!caughtSpecies || pokemonTotalExperienceForLevel(caughtSpecies->growthRate,
            game.playerPartyMember(1)->level, expectedReserveExperience) != PokemonExperienceResult::Ok ||
        reserveExperience != expectedReserveExperience) return 221;

    auto healingSwitchGame = game;
    NativeHeldModifierInstance switchHealing[2]{};
    const uint32_t outgoingPid = game.presentation().player.battleState.pokemonId;
    const uint32_t incomingPid = game.playerPartyMember(1)->battleState.pokemonId;
    if (initializeHeldModifierInstance("LEFTOVERS", outgoingPid, 1, true, nullptr, switchHealing[0]) !=
            HeldModifierStorageResult::Ok ||
        initializeHeldModifierInstance("SHELL_BELL", incomingPid, 1, true, nullptr, switchHealing[1]) !=
            HeldModifierStorageResult::Ok || !healingSwitchGame.restoreHeldModifierInventory(switchHealing, 2))
        return 439;
    if (!healingSwitchGame.switchPlayerPokemon(1) || healingSwitchGame.activePlayerPartyIndex() != 1 ||
        healingSwitchGame.heldModifierCount() != 2 ||
        healingSwitchGame.heldModifier(0)->ownerPokemonId != outgoingPid ||
        healingSwitchGame.heldModifier(1)->ownerPokemonId != incomingPid ||
        healingSwitchGame.presentation().player.battleState.turnDamageDealt) return 440;
    const auto heldRecipientWave = healingSwitchGame.run().wave;
    if (healingSwitchGame.claimHeldRewardChoice(255) ||
        healingSwitchGame.claimHeldRewardChoice(0) || healingSwitchGame.run().wave != heldRecipientWave ||
        healingSwitchGame.heldModifierCount() != 2 ||
        healingSwitchGame.heldModifier(0)->ownerPokemonId != outgoingPid) return 446;
    if (healingSwitchGame.claimRecoveryRewardChoice(255, 0) ||
        healingSwitchGame.claimRecoveryRewardChoice(0, 255) ||
        healingSwitchGame.run().wave != heldRecipientWave || healingSwitchGame.heldModifierCount() != 2)
        return 455;
    // Switch to reserve member (index 1):
    if (!game.switchPlayerPokemon(1)) return 176;
    if (game.activePlayerPartyIndex() != 1) return 177;
    if (game.presentation().player.dex != caughtDex) return 178;
    if (game.presentation().player.totalExperience != reserveExperience) return 222;

    // Switch back to starter (index 0):
    if (!game.switchPlayerPokemon(0)) return 179;
    if (game.activePlayerPartyIndex() != 0) return 180;
    if (game.presentation().player.totalExperience != starterExperience ||
        game.playerPartyMember(1)->totalExperience != reserveExperience) return 223;

    return 0;
}

static int checkLevelUpMoveLearningAndEvolution() {
    using namespace Pokerogue3DS;

    // 1. Check evolution queries
    const auto* evo1 = checkSpeciesLevelEvolution("bulbasaur", 15, 16);
    if (!evo1 || std::strcmp(evo1->targetSpeciesId, "ivysaur") != 0) return 190;

    const auto* evoNone = checkSpeciesLevelEvolution("bulbasaur", 14, 15);
    if (evoNone != nullptr) return 191;

    const auto* evo2 = checkSpeciesLevelEvolution("ivysaur", 31, 32);
    if (!evo2 || std::strcmp(evo2->targetSpeciesId, "venusaur") != 0) return 192;

    const auto* evoBug = checkSpeciesLevelEvolution("caterpie", 6, 7);
    if (!evoBug || std::strcmp(evoBug->targetSpeciesId, "metapod") != 0) return 193;

    // 2. Check applySpeciesEvolution
    PokemonBattleInit bulbaInit{};
    bulbaInit.speciesDex = 1;
    bulbaInit.level = 16;
    bulbaInit.abilityId = PokerogueContent::findSpeciesByDex(1)->ability1;
    bulbaInit.gender = PokemonGender::Male;
    bulbaInit.nature = PokemonNature::Hardy;
    bulbaInit.moveCount = 1;
    bulbaInit.moveIds[0] = 33; // Tackle
    for (uint8_t i = 0; i < 6; ++i) bulbaInit.ivs[i] = 15;
    PokemonBattleState bulbaState{};
    if (initializePokemonBattleState(bulbaInit, bulbaState) != PokemonBattleInitResult::Ok) return 194;

    auto taggedEvolution = bulbaState;
    taggedEvolution.friendship = 173;
    taggedEvolution.heldItemLostTags.unburden = true;
    taggedEvolution.turnDamageDealt = 19;
    taggedEvolution.pauseEvolutions = true;
    taggedEvolution.moves[0].pp = 1;
    taggedEvolution.moves[0].maxPp = 42; // Tackle with one PP Up.
    EvolutionResult taggedEvolutionResult{};
    if (!applySpeciesEvolution(1, "ivysaur", taggedEvolution, taggedEvolutionResult) ||
        !taggedEvolutionResult.evolved || taggedEvolution.friendship != 173 ||
        !taggedEvolution.heldItemLostTags.unburden || taggedEvolution.turnDamageDealt != 19 ||
        taggedEvolution.pauseEvolutions || taggedEvolution.moves[0].pp != 1 ||
        taggedEvolution.moves[0].maxPp != 42) return 468;
    auto invalidEvolution = bulbaState;
    invalidEvolution.moves[0].pp = invalidEvolution.moves[0].maxPp + 1;
    EvolutionResult invalidEvolutionResult{};
    if (applySpeciesEvolution(1, "ivysaur", invalidEvolution, invalidEvolutionResult) ||
        invalidEvolution.speciesDex != 1 || invalidEvolutionResult.evolved) return 469;
    const uint16_t bulbaHp = bulbaState.hp;
    const uint16_t bulbaMaxHp = bulbaState.maxHp;
    EvolutionResult evoRes{};
    std::string evoFb;
    PokemonActorIdentity evolutionIdentity{};
    evolutionIdentity.pokemonId = bulbaState.pokemonId;
    evolutionIdentity.gender = bulbaState.gender;
    evolutionIdentity.nature = bulbaState.nature;
    evolutionIdentity.formId = bulbaState.formId;
    evolutionIdentity.initialTeraTypeResolved = true;
    for (uint8_t i = 0; i < 6; ++i) evolutionIdentity.ivs[i] = bulbaState.ivs[i];
    if (!applySpeciesEvolution(1, "ivysaur", bulbaState, evoRes, &evoFb, &evolutionIdentity)) return 195;
    if (!evolutionIdentity.formId || std::strcmp(evolutionIdentity.formId, bulbaState.formId) ||
        evolutionIdentity.abilityIndex != 0) return 298;
    uint32_t evolvedExperience = 0;
    const auto* evolvedSpecies = PokerogueContent::findSpeciesByDex(2);
    NativePokemonSave evolvedSnapshot{};
    if (!evolvedSpecies || pokemonTotalExperienceForLevel(evolvedSpecies->growthRate, 16, evolvedExperience) !=
            PokemonExperienceResult::Ok || !captureNativePokemonActorSave(bulbaState, evolutionIdentity,
                evolvedExperience, evolvedSnapshot)) return 299;
    const auto* ppUpProfile = ppUpItemProfile("PP_UP");
    const auto* ppMaxProfile = ppUpItemProfile("PP_MAX");
    if (!ppUpProfile || !ppMaxProfile || ppUpProfile->upPoints != 1 || ppMaxProfile->upPoints != 3 ||
        !ppUpProfile->sourceHash || ppUpItemProfile("UNKNOWN_ITEM")) return 474;
    auto ppUpActor = bulbaState;
    ppUpActor.moves[0].pp = 1;
    if (!applyPokemonPpUpItem(ppUpActor, *ppUpProfile, 0) || ppUpActor.moves[0].maxPp != 42 ||
        ppUpActor.moves[0].pp != 8) return 475;
    if (!applyPokemonPpUpItem(ppUpActor, *ppMaxProfile, 0) || ppUpActor.moves[0].maxPp != 56 ||
        ppUpActor.moves[0].pp != 22) return 476;
    if (applyPokemonPpUpItem(ppUpActor, *ppUpProfile, 0) || ppUpActor.moves[0].maxPp != 56 ||
        ppUpActor.moves[0].pp != 22 || applyPokemonPpUpItem(ppUpActor, *ppUpProfile, 1)) return 477;
    ppUpActor.moves[0].maxPp = 41;
    if (applyPokemonPpUpItem(ppUpActor, *ppUpProfile, 0) || ppUpActor.moves[0].maxPp != 41)
        return 478;
    auto boostedPpActor = bulbaState;
    boostedPpActor.moves[0].maxPp = 42;
    boostedPpActor.moves[0].pp = 40;
    NativePokemonSave boostedPpSave{};
    NativePokemonSave decodedPpSave{};
    PokemonBattleState restoredPpActor{};
    PokemonActorIdentity restoredPpIdentity{};
    char boostedPpBytes[512]{};
    size_t boostedPpSize = 0;
    if (!captureNativePokemonActorSave(boostedPpActor, evolutionIdentity, evolvedExperience, boostedPpSave) ||
        !boostedPpSave.maxPpResolved || boostedPpSave.maxPp[0] != 42 ||
        encodeNativePokemonSave(boostedPpSave, boostedPpBytes, sizeof(boostedPpBytes), boostedPpSize) !=
            NativeSaveResult::Ok || decodeNativePokemonSave(boostedPpBytes, boostedPpSize, decodedPpSave) !=
            NativeSaveResult::Ok || !restoreNativePokemonActorSave(decodedPpSave, restoredPpActor, restoredPpIdentity) ||
        restoredPpActor.moves[0].maxPp != 42 || restoredPpActor.moves[0].pp != 40) return 470;
    boostedPpBytes[8] = '5';
    if (decodeNativePokemonSave(boostedPpBytes, boostedPpSize - 12, decodedPpSave) !=
            NativeSaveResult::InvalidRecord) return 471; // Legacy base PP cannot hold 40/35.
    boostedPpActor.moves[0].maxPp = 41;
    if (captureNativePokemonActorSave(boostedPpActor, evolutionIdentity, evolvedExperience, boostedPpSave))
        return 472;
    if (!evoRes.evolved || evoRes.newDex != 2 || std::strcmp(evoRes.newSpeciesId, "ivysaur") != 0) return 196;
    if (bulbaState.speciesDex != 2) return 197;
    // Ivysaur has higher base stats and max HP than Bulbasaur
    if (bulbaState.maxHp <= bulbaMaxHp || bulbaState.hp <= bulbaHp) return 198;

    auto replacementActor = bulbaState;
    replacementActor.moveCount = 4;
    replacementActor.moves[0] = {33, 1, 35};
    replacementActor.moves[1] = {45, 1, 40};
    replacementActor.moves[2] = {22, 1, 25};
    replacementActor.moves[3] = {73, 1, 10};
    const auto* newMove = PokerogueContent::findMoveById(75);
    if (!newMove || learnPokemonMoveAtSlot(replacementActor, 75, 2) !=
            PokemonLearnMoveResult::Learned || replacementActor.moveCount != 4 ||
        replacementActor.moves[2].moveId != 75 || replacementActor.moves[2].pp != newMove->pp ||
        replacementActor.moves[0].pp != 1 || replacementActor.moves[3].pp != 1) return 303;
    if (learnPokemonMoveAtSlot(replacementActor, 75, 0) != PokemonLearnMoveResult::AlreadyKnown ||
        replacementActor.moves[0].moveId != 33) return 304;
    if (learnPokemonMoveAtSlot(replacementActor, 14, 4) != PokemonLearnMoveResult::InvalidSlot ||
        replacementActor.moves[2].moveId != 75) return 305;
    const PokerogueContent::Move* unimplementedMove = nullptr;
    for (const auto& move : PokerogueContent::kMoves)
        if (move.upstreamFlags & PokerogueContent::MoveIsUnimplemented) { unimplementedMove = &move; break; }
    if (!unimplementedMove || learnPokemonMoveAtSlot(replacementActor, unimplementedMove->id, 0) != PokemonLearnMoveResult::UpstreamUnimplemented ||
        replacementActor.moves[0].moveId != 33) return 306;
    PokemonPendingLevelMoves pendingMoves{};
    uint16_t queuedIds[4]{};
    uint8_t queuedCount = replacementActor.moveCount;
    (void)learnNewLevelMoves(replacementActor.speciesDex, 1, 100, replacementActor,
        queuedIds, queuedCount, nullptr, &pendingMoves);
    if (!pendingMoves.count || pendingMoves.overflow || replacementActor.moveCount != 4) return 307;
    for (uint16_t index = 0; index < pendingMoves.count; ++index) {
        const auto* queued = PokerogueContent::findMoveById(pendingMoves.moveIds[index]);
        if (!queued || (queued->upstreamFlags & PokerogueContent::MoveIsUnimplemented)) return 308;
        for (uint16_t prior = 0; prior < index; ++prior)
            if (pendingMoves.moveIds[prior] == pendingMoves.moveIds[index]) return 309;
        for (uint8_t slot = 0; slot < replacementActor.moveCount; ++slot)
            if (replacementActor.moves[slot].moveId == pendingMoves.moveIds[index]) return 310;
    }
    // A chosen replacement survives the subsequent evolution phase.
    PokemonBattleInit sequenceInput = bulbaInit;
    sequenceInput.moveCount = 4;
    sequenceInput.moveIds[0] = 33;
    sequenceInput.moveIds[1] = 45;
    sequenceInput.moveIds[2] = 22;
    sequenceInput.moveIds[3] = 73;
    PokemonBattleState sequenceActor{};
    if (initializePokemonBattleState(sequenceInput, sequenceActor) != PokemonBattleInitResult::Ok ||
        learnPokemonMoveAtSlot(sequenceActor, 75, 2) != PokemonLearnMoveResult::Learned) return 311;
    auto sequenceIdentity = evolutionIdentity;
    sequenceIdentity.formId = sequenceActor.formId;
    EvolutionResult sequenceEvolution{};
    if (!applySpeciesEvolution(1, "ivysaur", sequenceActor, sequenceEvolution, nullptr, &sequenceIdentity) ||
        sequenceActor.speciesDex != 2 || sequenceActor.moves[2].moveId != 75 ||
        sequenceActor.moves[2].pp != newMove->pp) return 312;
    bool evolutionMoveCovered = false;
    for (const auto& species : PokerogueContent::kSpecies) {
        const auto* rows = PokerogueContent::levelMovesFor(species);
        for (uint16_t i = 0; rows && i < species.learnsetCount; ++i) {
            const auto* move = PokerogueContent::findMoveById(rows[i].moveId);
            if (rows[i].level != 0 || !move || (move->upstreamFlags & PokerogueContent::MoveIsUnimplemented)) continue;
            PokemonBattleState learner{};
            learner.speciesDex = species.dex;
            PokemonPendingLevelMoves queue{};
            if (!learnPokemonEvolutionMoves(learner, queue) || !learner.moveCount) return 313;
            bool found = false;
            for (uint8_t slot = 0; slot < learner.moveCount; ++slot) found |= learner.moves[slot].moveId == move->id;
            if (!found) return 314;
            if (!learnPokemonEvolutionMoves(learner, queue)) return 315;
            evolutionMoveCovered = true;
            break;
        }
        if (evolutionMoveCovered) break;
    }
    if (!evolutionMoveCovered) return 316;
    // A real Onix -> Steelix type change must retain its original Rock Tera type.
    const auto* onix = PokerogueContent::findSpeciesByDex(95);
    const auto* steelix = PokerogueContent::findSpeciesByDex(208);
    if (!onix || !steelix) return 330;
    PokemonBattleInit onixInput = bulbaInit;
    onixInput.speciesDex = onix->dex;
    onixInput.formId = onix->firstFormId;
    onixInput.abilityId = onix->ability1;
    PokemonBattleState onixState{};
    if (initializePokemonBattleState(onixInput, onixState) != PokemonBattleInitResult::Ok) return 331;
    PokemonActorIdentity onixIdentity{};
    onixIdentity.pokemonId = onixState.pokemonId;
    onixIdentity.gender = onixState.gender;
    onixIdentity.nature = onixState.nature;
    onixIdentity.formId = onixState.formId;
    onixIdentity.initialTeraTypeResolved = true;
    onixIdentity.initialTeraType = resolvePokemonTypeSymbol(onix->type1);
    for (uint8_t i = 0; i < 6; ++i) onixIdentity.ivs[i] = onixState.ivs[i];
    EvolutionResult steelixEvolution{};
    if (!applySpeciesEvolution(onix->dex, steelix->id, onixState, steelixEvolution, nullptr, &onixIdentity) ||
        !onixIdentity.initialTeraType || std::strcmp(onixIdentity.initialTeraType, onix->type1) ||
        !std::strcmp(onixIdentity.initialTeraType, steelix->type1)) return 332;
    uint32_t steelixExp = 0;
    NativePokemonSave steelixSave{};
    PokemonBattleState restoredSteelix{};
    PokemonActorIdentity restoredSteelixIdentity{};
    if (pokemonTotalExperienceForLevel(steelix->growthRate, onixState.level, steelixExp) !=
            PokemonExperienceResult::Ok || !captureNativePokemonActorSave(onixState, onixIdentity,
                steelixExp, steelixSave) || !restoreNativePokemonActorSave(steelixSave, restoredSteelix,
                restoredSteelixIdentity) || std::strcmp(restoredSteelixIdentity.initialTeraType, onix->type1))
        return 333;
    std::strcpy(steelixSave.initialTeraType, "INVALID_TYPE");
    if (restoreNativePokemonActorSave(steelixSave, restoredSteelix, restoredSteelixIdentity) ||
        std::strcmp(restoredSteelixIdentity.initialTeraType, onix->type1)) return 334;
    // 3. Check learnNewLevelMoves
    // Bulbasaur learns Vine Whip (id 22) or Leech Seed at early levels
    PokemonBattleState learnState = bulbaState;
    learnState.speciesDex = 1; // Bulbasaur
    learnState.moveCount = 1;
    learnState.moves[0].moveId = 33; // Tackle
    uint16_t moveIds[4] = { 33, 0, 0, 0 };
    uint8_t moveCount = 1;
    std::string learnFb;
    const uint8_t learned = learnNewLevelMoves(1, 1, 10, learnState, moveIds, moveCount, &learnFb);
    if (learned == 0 || learnState.moveCount <= 1) return 199;
    if (learnState.moves[1].pp == 0) return 200;

    return 0;
}

static int checkBossDamageAbilityCapabilities() {
    unsigned inspected = 0;
    for (const auto& profile : PokerogueContent::kAbilityMovegenProfiles) {
        if (std::strcmp(profile.sourceSymbol, "AbilityId.PRESSURE") == 0) {
            if (!profile.bossDamageCallbacksResolved) return 261;
            ++inspected;
        }
        if (std::strcmp(profile.sourceSymbol, "AbilityId.STURDY") == 0) {
            if (profile.bossDamageCallbacksResolved) return 262;
            ++inspected;
        }
    }
    return inspected == 2 ? 0 : 263;
}

static int checkTrainerParentEvolutionThresholds() {
    const PokerogueContent::TrainerPartySegment* normal = nullptr;
    for (const auto& segment : PokerogueContent::kTrainerPartySegments)
        if (segment.evolutionThresholdKind && std::strcmp(segment.evolutionThresholdKind, "NORMAL") == 0)
            normal = &segment;
    if (!normal) return 252;
    unsigned compounds = 0;
    for (const auto& party : PokerogueContent::kTrainerPartyTemplates) {
        if (party.isCompound) {
            if (party.parentEvolutionThresholdKindId != normal->evolutionThresholdKindId) return 253;
            ++compounds;
        } else {
            const auto* segments = PokerogueContent::trainerPartySegmentsFor(party);
            if (!segments || party.parentEvolutionThresholdKindId != segments[0].evolutionThresholdKindId) return 254;
        }
    }
    return compounds ? 0 : 255;
}

static int checkCanonicalTrainerSpecialtyTypes() {
    unsigned inspected = 0;
    for (const auto& trainer : PokerogueContent::kTrainerTypes) {
        if (std::strcmp(trainer.key, "brock") == 0 || std::strcmp(trainer.key, "misty") == 0) {
            const char* expected = std::strcmp(trainer.key, "brock") == 0 ? "ROCK" : "WATER";
            if (!trainer.specialtyTypeResolved || !trainer.specialtyType ||
                std::strcmp(trainer.specialtyType, expected)) return 249;
            ++inspected;
        }
        if (!trainer.specialtyTypeResolved || !trainer.specialtyType || !*trainer.specialtyType) continue;
        bool canonicalType = false;
        for (const auto& species : PokerogueContent::kSpecies)
            if ((species.type1 && std::strcmp(species.type1, trainer.specialtyType) == 0) ||
                (species.type2 && std::strcmp(species.type2, trainer.specialtyType) == 0)) canonicalType = true;
        if (!canonicalType) return 250;
    }
    return inspected == 2 ? 0 : 251;
}

static int checkTrainerBalancedTypes() {
    using namespace Pokerogue3DS;
    const auto* species = PokerogueContent::findSpeciesByDex(1);
    if (!species || !species->type1) return 242;
    const auto* form = PokerogueContent::findFormById(species->firstFormId);
    if (!form) return 243;
    TrainerPartyMemberTypes prior[] = {{form->type1, form->type2, true}};
    bool overlap = false;
    if (!trainerBalancedTypeOverlap(*species, prior, 1, overlap) || !overlap) return 244;
    if (!trainerBalancedTypeOverlap(*species, nullptr, 0, overlap) || overlap) return 245;
    prior[0].resolved = false;
    overlap = true;
    if (trainerBalancedTypeOverlap(*species, prior, 1, overlap) || !overlap ||
        trainerBalancedTypeOverlap(*species, nullptr, 1, overlap)) return 246;
    unsigned disjoint = 0;
    prior[0].resolved = true;
    for (const auto& other : PokerogueContent::kSpecies) {
        const bool expected = std::strcmp(other.type1, form->type1) == 0 ||
            (form->type2 && std::strcmp(form->type2, "NONE") && std::strcmp(other.type1, form->type2) == 0) ||
            (other.type2 && std::strcmp(other.type2, "NONE") &&
                (std::strcmp(other.type2, form->type1) == 0 ||
                 (form->type2 && std::strcmp(form->type2, "NONE") && std::strcmp(other.type2, form->type2) == 0)));
        if (!trainerBalancedTypeOverlap(other, prior, 1, overlap) || overlap != expected) return 247;
        if (!overlap) ++disjoint;
    }
    return disjoint ? 0 : 248;
}

static int checkReservedTrainerSpecies() {
    using namespace Pokerogue3DS;
    unsigned checked = 0;
    for (const auto& trainer : PokerogueContent::kTrainerTypes) {
        if (!trainer.signatureCount) continue;
        if (trainer.signatureOffset + trainer.signatureCount > PokerogueContent::kTrainerSignatureChoiceCount) return 237;
        for (uint8_t slot = 0; slot < trainer.signatureCount; ++slot) {
            const auto& choice = PokerogueContent::kTrainerSignatureChoices[trainer.signatureOffset + slot];
            for (uint8_t i = 0; i < choice.speciesCount; ++i) {
                const auto* species = trainerPartySpeciesById(
                    PokerogueContent::kTrainerSignatureSpecies[choice.speciesOffset + i].speciesId);
                if (!species) return 238;
                bool duplicate = false;
                if (!trainerReservedSpeciesDuplicate(trainer, trainerPartyRootDex(*species), duplicate) ||
                    !duplicate) return 239;
                ++checked;
            }
        }
        bool sentinel = true;
        if (trainerReservedSpeciesDuplicate(trainer, 65535, sentinel) || !sentinel) return 240;
    }
    return checked ? 0 : 241;
}

static int checkTrainerPoolEvolutionDraws() {
    using namespace Pokerogue3DS;
    unsigned checked = 0;
    for (const auto& trainer : PokerogueContent::kTrainerTypes) {
        if (!trainer.speciesPoolCount || trainer.signatureCount || !trainer.specialtyTypeResolved ||
                (trainer.specialtyType && *trainer.specialtyType)) continue;
        const uint16_t seed[] = {'p', 'o', 'o', 'l'};
        PokerogueRngAdapter templateRng;
        templateRng.sow(seed, 4);
        const auto choice = selectTrainerPartyTemplate(trainer, 25, templateRng);
        if (!choice.supported || !choice.value) continue;
        const auto slot = trainerPartyMemberTemplate(*choice.value, 0);
        if (!slot.supported || slot.balanced || slot.sameSpecies) continue;
        PokerogueRngAdapter expectedRng, actualRng;
        expectedRng.sow(seed, 4);
        actualRng.sow(seed, 4);
        const char* expected = nullptr;
        for (uint8_t attempt = 0; attempt <= 10; ++attempt) {
            expected = nullptr;
            const auto pool = PokerogueEncounterResolver::resolveTrainerPoolSpecies(trainer, expectedRng);
            if (!pool.valid) break;
            const auto* base = trainerPartySpeciesById(pool.speciesId);
            if (!base) break;
            expected = PokerogueEncounterResolver::resolveTrainerSpeciesForLevel(base->id,
                25, choice.value->parentEvolutionThresholdKindId, false, expectedRng);
            if (!expected) break;
            if (base->prevolutionDex && std::strcmp(expected, base->id) && attempt < 10) continue;
            break;
        }
        if (!expected) continue;
        const auto actual = resolveSimpleTrainerPoolMember(trainer, *choice.value, 0, 25,
            25, nullptr, 0, actualRng);
        if (!actual.supported || !actual.species || std::strcmp(expected, actual.species->id)) return 234;
        const auto a = actualRng.state(), e = expectedRng.state();
        if (a.carry != e.carry || a.s0 != e.s0 || a.s1 != e.s1 || a.s2 != e.s2) return 235;
        ++checked;
    }
    return checked ? 0 : 236;
}

static int checkCanonicalSameSpeciesTrainerMembers() {
    using namespace Pokerogue3DS;
    unsigned checked = 0;
    for (const auto& trainer : PokerogueContent::kTrainerTypes) {
        if (!trainer.speciesPoolCount || trainer.signatureCount) continue;
        PokerogueRngAdapter templateRng;
        const uint16_t seed[] = {'s', 'a', 'm', 'e'};
        templateRng.sow(seed, 4);
        const auto choice = selectTrainerPartyTemplate(trainer, 25, templateRng);
        if (!choice.supported || !choice.value) continue;
        const auto levels = resolveClassicTrainerPartyLevels(*choice.value, 25, false);
        if (!levels.supported) continue;
        const PokerogueContent::Species* previous[6]{};
        for (uint8_t slot = 0; slot < levels.count; ++slot) {
            PokerogueRngAdapter memberRng;
            memberRng.sow(seed, 4);
            const auto member = resolveSimpleTrainerPoolMember(trainer, *choice.value,
                slot, levels.values[slot], 25, previous, slot, memberRng);
            if (!member.supported || !member.species) break;
            const auto metadata = trainerPartyMemberTemplate(*choice.value, slot);
            if (metadata.sameSpecies && slot > metadata.segmentStart) {
                const char* expected = PokerogueEncounterResolver::resolveTrainerSpeciesForLevel(
                    previous[metadata.segmentStart]->id, levels.values[slot],
                    choice.value->parentEvolutionThresholdKindId, false, memberRng, true, false);
                if (!expected || std::strcmp(expected, member.species->id)) return 232;
                ++checked;
            }
            previous[slot] = member.species;
        }
    }
    return checked ? 0 : 233;
}

static int checkCanonicalTrainerSignatureSlots() {
    using namespace Pokerogue3DS;
    unsigned checked = 0;
    for (const auto& trainer : PokerogueContent::kTrainerTypes) {
        if (!(trainer.flags & 32U) || !trainer.signatureCount || trainer.signatureCount > 6) continue;
        const uint8_t partySize = 6;
        for (uint8_t distance = 1; distance <= trainer.signatureCount; ++distance) {
            PokerogueRngAdapter rng;
            const uint16_t seed[] = {'s', 'i', 'g'};
            rng.sow(seed, 3);
            const char* id = trainerSignatureSpeciesForMember(trainer, partySize,
                static_cast<uint8_t>(partySize - distance), rng);
            if (!id || !trainerPartySpeciesById(id)) return 230;
            ++checked;
        }
    }
    return checked ? 0 : 231; // Real catalog coverage, not a synthetic trainer.
}

int main() {
    const int bossCapabilitiesCheck = checkBossDamageAbilityCapabilities();
    if (bossCapabilitiesCheck) return bossCapabilitiesCheck;
    const int parentThresholdCheck = checkTrainerParentEvolutionThresholds();
    if (parentThresholdCheck) return parentThresholdCheck;
    const int specialtyCheck = checkCanonicalTrainerSpecialtyTypes();
    if (specialtyCheck) return specialtyCheck;
    const int balancedTypesCheck = checkTrainerBalancedTypes();
    if (balancedTypesCheck) return balancedTypesCheck;
    const int reservedSpeciesCheck = checkReservedTrainerSpecies();
    if (reservedSpeciesCheck) return reservedSpeciesCheck;
    const int poolDrawCheck = checkTrainerPoolEvolutionDraws();
    if (poolDrawCheck) return poolDrawCheck;
    const int sameSpeciesCheck = checkCanonicalSameSpeciesTrainerMembers();
    if (sameSpeciesCheck) return sameSpeciesCheck;
    const int signatureCheck = checkCanonicalTrainerSignatureSlots();
    if (signatureCheck) return signatureCheck;
    // A high new level must not wrap to negative and hide real low-level moves.
    {
        Pokerogue3DS::PokemonBattleState learner{};
        learner.speciesDex = PokerogueContent::kSpecies[0].dex;
        uint16_t ids[4]{};
        uint8_t count = 0;
        if (!Pokerogue3DS::learnNewLevelMoves(learner.speciesDex, 1, 128, learner, ids, count) ||
            !count || count != learner.moveCount) return 214;
        bool formMoveCovered = false;
        for (const auto& form : PokerogueContent::kForms) {
            const auto* species = Pokerogue3DS::findSpeciesById(form.speciesId);
            const auto* rows = PokerogueContent::levelMovesFor(form);
            if (!species || !rows) continue;
            for (uint16_t i = 0; i < form.learnsetCount; ++i) {
                if (rows[i].level < 2) continue;
                const uint16_t level = static_cast<uint16_t>(rows[i].level);
                bool speciesHasMove = false;
                const auto* baseRows = PokerogueContent::levelMovesFor(*species);
                for (uint16_t j = 0; baseRows && j < species->learnsetCount; ++j)
                    speciesHasMove |= baseRows[j].moveId == rows[i].moveId;
                if (speciesHasMove) continue;
                Pokerogue3DS::PokemonBattleState formLearner{};
                formLearner.speciesDex = species->dex;
                formLearner.formId = form.id;
                uint16_t formIds[4]{};
                uint8_t formMoveCount = 0;
                if (!Pokerogue3DS::learnNewLevelMoves(species->dex, level - 1, level,
                        formLearner, formIds, formMoveCount)) return 300;
                bool found = false;
                for (uint8_t slot = 0; slot < formMoveCount; ++slot) found |= formIds[slot] == rows[i].moveId;
                if (!found) return 301;
                formMoveCovered = true;
                break;
            }
            if (formMoveCovered) break;
        }
        if (!formMoveCovered) return 302;
        const auto* alreadyEligible = Pokerogue3DS::checkSpeciesLevelEvolution("bulbasaur", 20, 21);
        if (!alreadyEligible || alreadyEligible->level != 16) return 215;
        // A level alone must not select an item/condition/form evolution.
        for (const auto& edge : PokerogueContent::kSpeciesEvolutions) {
            const auto* selected = Pokerogue3DS::checkSpeciesLevelEvolution(edge.sourceSpeciesId, 1, 100);
            if (!selected) continue;
            bool eligible = false;
            for (const auto& row : PokerogueContent::kSimpleLevelEvolutionProfiles)
                if (row.sourceOrder == selected->sourceOrder &&
                    Pokerogue3DS::pokemonEvolutionTextEqual(row.speciesId, selected->sourceSpeciesId)) eligible = true;
            if (!eligible) return 216;
        }
    }
    // Catch rates must be generated for the full real canonical species catalog.
    if (sizeof(PokerogueContent::kSpeciesCatchProfiles) / sizeof(PokerogueContent::kSpeciesCatchProfiles[0]) !=
            PokerogueContent::kSpeciesCount) return 210;
    for (const auto& species : PokerogueContent::kSpecies) {
        bool found = false;
        for (const auto& row : PokerogueContent::kSpeciesCatchProfiles) {
            if (row.speciesDex != species.dex) continue;
            if (!row.sourceHash || !*row.sourceHash || !row.sourcePath || !*row.sourcePath) return 211;
            found = true;
        }
        if (!found) return 212;
    }
    {
        Pokerogue3DS::FirstRunRuntime game(1);
        const auto beforeRng = game.battleRng().state();
        const auto beforeDex = game.presentation().player.dex;
        const auto beforeHp = game.presentation().player.battleState.hp;
        if (game.switchPlayerPokemon(255) || game.presentation().player.dex != beforeDex ||
            game.presentation().player.battleState.hp != beforeHp ||
            game.pokeballCount(Pokerogue3DS::PokeballType::Pokeball) != 5) return 213;
        using namespace Pokerogue3DS;
        NativeRunSave beforeTransition{}, afterTransition{};
        game.captureNativeRunSave(beforeTransition);
        if (game.skipVictoryReward() || game.claimRewardChoice()) return 226;
        game.captureNativeRunSave(afterTransition);
        uint8_t beforeBytes[kNativeSaveMaxBytes]{}, afterBytes[kNativeSaveMaxBytes]{};
        size_t beforeLength = 0, afterLength = 0;
        if (encodeNativeRunSave(beforeTransition, beforeBytes, sizeof(beforeBytes), beforeLength) != NativeSaveResult::Ok ||
            encodeNativeRunSave(afterTransition, afterBytes, sizeof(afterBytes), afterLength) != NativeSaveResult::Ok ||
            beforeLength != afterLength || std::memcmp(beforeBytes, afterBytes, beforeLength)) return 227;
        (void)beforeRng; // Invalid command must not publish a party or inventory mutation.
    }

    using namespace Pokerogue3DS;
    for (uint32_t seed = 1; seed <= 64; ++seed) {
        FirstRunRuntime game(seed);
        const auto& context = game.presentation();
        if (!context.enemy.actorIdentityResolved || context.secondEnemy.dex) continue;
        NativeRunSave setup{};
        game.captureNativeRunSave(setup);
        NativeRunSave active = setup;
        active.stage = NativeSaveStage::BattleActive;
        active.encounterDex = context.enemy.dex;
        active.playerHp = context.player.battleState.hp;
        active.enemyHp = context.enemy.battleState.hp;
        active.battleTurn = 1;
        active.playerMoveCount = context.player.battleState.moveCount;
        active.enemyMoveCount = context.enemy.battleState.moveCount;
        for (uint8_t slot = 0; slot < active.playerMoveCount; ++slot) {
            active.playerMoveIds[slot] = context.player.battleState.moves[slot].moveId;
            active.playerPp[slot] = context.player.battleState.moves[slot].pp;
        }
        for (uint8_t slot = 0; slot < active.enemyMoveCount; ++slot) {
            active.enemyMoveIds[slot] = context.enemy.battleState.moves[slot].moveId;
            active.enemyPp[slot] = context.enemy.battleState.moves[slot].pp;
        }
        if (validateNativeRunSave(active, PokerogueContent::kContentHash) != NativeSaveResult::Ok)
            return 1;
        active.playerStatStages[0] = -2;
        active.enemyStatStages[4] = 3;
        NativeRunSave lateInvalid = active;
        lateInvalid.enemyHp = 65535; // Structurally valid, fails reconstructed max HP.
        if (validateNativeRunSave(lateInvalid, PokerogueContent::kContentHash) != NativeSaveResult::Ok ||
            game.restoreNativeRunSave(lateInvalid)) return 2;
        NativeRunSave afterRejected{};
        game.captureNativeRunSave(afterRejected);
        char beforeBytes[kNativeSaveMaxBytes]{}, afterBytes[kNativeSaveMaxBytes]{};
        size_t beforeSize = 0, afterSize = 0;
        if (encodeNativeRunSave(setup, beforeBytes, sizeof(beforeBytes), beforeSize) != NativeSaveResult::Ok ||
            encodeNativeRunSave(afterRejected, afterBytes, sizeof(afterBytes), afterSize) != NativeSaveResult::Ok ||
            beforeSize != afterSize || std::memcmp(beforeBytes, afterBytes, beforeSize)) return 3;
        active.starterProfileGeneration = 17;
        active.participantHistoryResolved = true;
        active.participantCount = 1;
        active.participantIds[0] = context.player.battleState.pokemonId;
        auto foreignParticipant = active;
        foreignParticipant.participantIds[0] ^= 0xffffffffU;
        if (game.restoreNativeRunSave(foreignParticipant)) return 520;
        NativeRunSave unchangedParticipants{};
        game.captureNativeRunSave(unchangedParticipants);
        if (!unchangedParticipants.participantHistoryResolved || unchangedParticipants.participantCount)
            return 521;
        if (!game.restoreNativeRunSave(active)) return 4;
        if (!game.scene().nodes || !game.scene().nodeCount ||
            !game.scene().nodes[0].text || std::strcmp(game.scene().nodes[0].text,
                "POKEROGUE 3DS - CLASSIC 1")) return 5;
        NativeRunSave loaded{};
        game.captureNativeRunSave(loaded);
        if (loaded.stage != NativeSaveStage::BattleActive || loaded.enemyHp != active.enemyHp ||
            loaded.seed != seed || loaded.playerStatStages[0] != -2 ||
            loaded.enemyStatStages[4] != 3 || loaded.starterProfileGeneration != 17 || !loaded.participantHistoryResolved ||
            loaded.participantCount != 1 || loaded.participantIds[0] != active.participantIds[0]) return 6;
        NativePokemonSave actorSnapshot{};
        const auto& currentActor = game.presentation().player;
        if (!captureNativePokemonSave(currentActor.battleState, currentActor.totalExperience,
                actorSnapshot)) return 269;
        PokemonBattleState restoredActor{};
        if (!restoreNativePokemonSave(actorSnapshot, restoredActor) ||
            restoredActor.pokemonId != currentActor.battleState.pokemonId ||
            restoredActor.hp != currentActor.battleState.hp ||
            restoredActor.formId == actorSnapshot.formId ||
            std::strcmp(restoredActor.formId, actorSnapshot.formId)) return 270;
        for (uint8_t stat = 0; stat < 6; ++stat)
            if (restoredActor.stats[stat] != currentActor.battleState.stats[stat] ||
                restoredActor.ivs[stat] != currentActor.battleState.ivs[stat]) return 271;
        for (uint8_t slot = 0; slot < restoredActor.moveCount; ++slot)
            if (restoredActor.moves[slot].moveId != currentActor.battleState.moves[slot].moveId ||
                restoredActor.moves[slot].pp != currentActor.battleState.moves[slot].pp) return 272;
        // Real species + pinned candy plan with an explicit uncapped-limit override.
        const auto* candySpecies = PokerogueContent::findSpeciesByDex(actorSnapshot.speciesDex);
        PokemonLevelIncrementPlan candyPlan{};
        if (!candySpecies || planPokemonLevelIncrement(candySpecies->growthRate,
                actorSnapshot.level, actorSnapshot.experience, 0, true, actorSnapshot.level, candyPlan) !=
                PokemonExperienceResult::Ok || candyPlan.progress.experience != actorSnapshot.experience)
            return 501;
        auto candyActor = currentActor.battleState;
        if (!recalculatePokemonBattleLevel(candyActor, candyPlan.progress.level)) return 502;
        NativePokemonSave candySnapshot{}, decodedCandy{};
        if (!captureNativePokemonSave(candyActor, candyPlan.progress.experience, candySnapshot)) return 503;
        char candyPayload[1024]{};
        size_t candyPayloadSize = 0;
        if (encodeNativePokemonSave(candySnapshot, candyPayload, sizeof(candyPayload), candyPayloadSize) !=
                NativeSaveResult::Ok || decodeNativePokemonSave(candyPayload, candyPayloadSize, decodedCandy) !=
                NativeSaveResult::Ok || decodedCandy.experience != actorSnapshot.experience ||
            decodedCandy.level != candyPlan.progress.level) return 504;
        PokemonBattleState candyRestored{};
        if (!restoreNativePokemonSave(decodedCandy, candyRestored) ||
            candyRestored.level != candyActor.level || candyRestored.maxHp != candyActor.maxHp ||
            candyRestored.hp != candyActor.hp || candyRestored.friendship != candyActor.friendship)
            return 505;
        auto friendshipActor = currentActor.battleState;
        const auto* friendshipRoot = pokemonRootSpecies(friendshipActor.speciesDex);
        if (!friendshipRoot) return 506;
        NativeStarterCandyRecord friendshipRecord{friendshipRoot->dex, 0, 0};
        StarterCandyAwardEvent friendshipEvent{};
        PokemonFriendshipPolicy friendshipPolicy{};
        friendshipActor.friendship = 70;
        if (applyNativePokemonFriendship(friendshipActor, friendshipRecord, 3, friendshipPolicy, false,
                friendshipEvent) != NativeFriendshipApplyResult::UnresolvedPolicy ||
            friendshipActor.friendship != 70 || friendshipRecord.friendship) return 507;
        friendshipPolicy.resolved = true;
        friendshipPolicy.candyMultiplier = PokerogueContent::kClassicCandyFriendshipMultiplier;
        if (applyNativePokemonFriendship(friendshipActor, friendshipRecord,
                PokerogueContent::kFriendshipGainFromBattle, friendshipPolicy, false, friendshipEvent) !=
                NativeFriendshipApplyResult::Applied || friendshipActor.friendship != 73 ||
            friendshipRecord.friendship != 9 || friendshipRecord.candyCount) return 508;
        friendshipActor.friendship = 254;
        const auto savedCandyFriendship = friendshipRecord.friendship;
        if (applyNativePokemonFriendship(friendshipActor, friendshipRecord, 3, friendshipPolicy, false,
                friendshipEvent) != NativeFriendshipApplyResult::UnresolvedPolicy ||
            friendshipActor.friendship != 254 || friendshipRecord.friendship != savedCandyFriendship) return 509;
        if (applyNativePokemonFriendship(friendshipActor, friendshipRecord, 3, friendshipPolicy, true,
                friendshipEvent) != NativeFriendshipApplyResult::Applied || friendshipActor.friendship != 255 ||
            friendshipRecord.friendship != savedCandyFriendship + 9) return 510;
        const auto beforeLossProgress = friendshipRecord.friendship;
        PokemonFriendshipPolicy unresolvedLossPolicy{};
        if (applyNativePokemonFriendship(friendshipActor, friendshipRecord,
                -static_cast<int32_t>(PokerogueContent::kFriendshipLossFromFaint), unresolvedLossPolicy, false,
                friendshipEvent) != NativeFriendshipApplyResult::Applied || friendshipActor.friendship != 250 ||
            friendshipRecord.friendship != beforeLossProgress || friendshipEvent.requestedAward) return 511;
        friendshipRecord.speciesDex = 0;
        if (applyNativePokemonFriendship(friendshipActor, friendshipRecord, 3, friendshipPolicy, true,
                friendshipEvent) != NativeFriendshipApplyResult::RootMismatch ||
            friendshipActor.friendship != 250) return 512;
        PokemonParticipantExperiencePolicy expPolicy{};
        uint32_t memberAward = 99;
        if (pokemonParticipantExperience(101.9, true, expPolicy, memberAward) !=
                PokemonExperienceResult::UnresolvedPolicy || memberAward != 99) return 513;
        expPolicy.resolved = true;
        expPolicy.eligible = true;
        expPolicy.participated = true;
        expPolicy.participantCount = 2;
        if (pokemonParticipantExperience(101.9, true, expPolicy, memberAward) !=
                PokemonExperienceResult::Ok || memberAward != 76) return 514;
        expPolicy.multipleParticipantBonusStacks = 1;
        expPolicy.pokerus = true;
        if (pokemonParticipantExperience(101.9, true, expPolicy, memberAward) !=
                PokemonExperienceResult::Ok || memberAward != 159) return 515;
        expPolicy.participated = false;
        expPolicy.pokerus = false;
        expPolicy.expShareStacks = 2;
        if (pokemonParticipantExperience(101.9, true, expPolicy, memberAward) !=
                PokemonExperienceResult::Ok || memberAward != 30) return 516;
        expPolicy.hasMultiplierOverride = true;
        expPolicy.multiplierOverride = 2.0;
        expPolicy.boosterMultiplier = 1.5;
        if (pokemonParticipantExperience(101.9, true, expPolicy, memberAward) !=
                PokemonExperienceResult::Ok || memberAward != 456) return 517;
        expPolicy.eligible = false;
        if (pokemonParticipantExperience(101.9, true, expPolicy, memberAward) !=
                PokemonExperienceResult::Ok || memberAward) return 518;
        expPolicy.eligible = true;
        expPolicy.participantCount = 0;
        if (pokemonParticipantExperience(101.9, true, expPolicy, memberAward) !=
                PokemonExperienceResult::Ok || memberAward) return 519;
        auto invalidActor = actorSnapshot;
        invalidActor.hp = 65535;
        const uint16_t beforeInvalidHp = restoredActor.hp;
        if (restoreNativePokemonSave(invalidActor, restoredActor) || restoredActor.hp != beforeInvalidHp)
            return 273;
        invalidActor = actorSnapshot;
        invalidActor.nature = 255;
        if (restoreNativePokemonSave(invalidActor, restoredActor)) return 274;
        invalidActor = actorSnapshot;
        invalidActor.ivs[0] = 32;
        if (restoreNativePokemonSave(invalidActor, restoredActor)) return 275;
        invalidActor = actorSnapshot;
        for (auto& ch : invalidActor.formId) ch = 'a';
        if (restoreNativePokemonSave(invalidActor, restoredActor)) return 276;
        if (!captureNativePokemonActorSave(currentActor.battleState, currentActor.actor,
                currentActor.totalExperience, actorSnapshot)) return 277;
        PokemonActorIdentity restoredIdentity{};
        if (!restoreNativePokemonActorSave(actorSnapshot, restoredActor, restoredIdentity) ||
            restoredIdentity.abilityIndex != currentActor.actor.abilityIndex ||
            restoredIdentity.initialTeraTypeIndex != currentActor.actor.initialTeraTypeIndex ||
            !restoredIdentity.initialTeraTypeResolved) return 278;
        auto invalidIdentity = actorSnapshot;
        invalidIdentity.abilityIndex = 3;
        const auto preservedIdentity = restoredIdentity;
        if (restoreNativePokemonActorSave(invalidIdentity, restoredActor, restoredIdentity) ||
            restoredIdentity.pokemonId != preservedIdentity.pokemonId) return 279;
        invalidIdentity = actorSnapshot;
        invalidIdentity.initialTeraTypeResolved = false;
        if (restoreNativePokemonActorSave(invalidIdentity, restoredActor, restoredIdentity)) return 280;
        char actorBytes[512]{};
        size_t actorSize = 0;
        if (encodeNativePokemonSave(actorSnapshot, actorBytes, sizeof(actorBytes), actorSize) !=
                NativeSaveResult::Ok) return 281;
        NativePokemonSave decodedActor{};
        if (decodeNativePokemonSave(actorBytes, actorSize, decodedActor) != NativeSaveResult::Ok ||
            decodedActor.pokemonId != actorSnapshot.pokemonId || decodedActor.experience != actorSnapshot.experience ||
            std::strcmp(decodedActor.formId, actorSnapshot.formId)) return 282;
        char actorBytesAgain[512]{};
        size_t actorSizeAgain = 0;
        if (encodeNativePokemonSave(decodedActor, actorBytesAgain, sizeof(actorBytesAgain), actorSizeAgain) !=
                NativeSaveResult::Ok || actorSizeAgain != actorSize ||
            std::memcmp(actorBytes, actorBytesAgain, actorSize)) return 283;
        const uint32_t preservedActorId = decodedActor.pokemonId;
        if (decodeNativePokemonSave(actorBytes, actorSize - 1, decodedActor) != NativeSaveResult::InvalidFormat ||
            decodedActor.pokemonId != preservedActorId) return 284;
        if (decodeNativePokemonSave(actorBytes, 513, decodedActor) != NativeSaveResult::InvalidFormat) return 285;
        size_t rejectedSize = 99;
        if (encodeNativePokemonSave(actorSnapshot, actorBytesAgain, 1, rejectedSize) != NativeSaveResult::TooLarge ||
            rejectedSize) return 286;
        auto pausedState = currentActor.battleState;
        pausedState.pauseEvolutions = true;
        NativePokemonSave pausedActor{};
        if (!captureNativePokemonActorSave(pausedState, currentActor.actor,
                currentActor.totalExperience, pausedActor) || !pausedActor.pauseEvolutions) return 320;
        char pauseBytes[512]{};
        size_t pauseSize = 0;
        if (encodeNativePokemonSave(pausedActor, pauseBytes, sizeof(pauseBytes), pauseSize) !=
                NativeSaveResult::Ok || decodeNativePokemonSave(pauseBytes, pauseSize, decodedActor) !=
                NativeSaveResult::Ok || !decodedActor.pauseEvolutions ||
            !restoreNativePokemonActorSave(decodedActor, restoredActor, restoredIdentity) ||
            !restoredActor.pauseEvolutions) return 321;
        auto tagState = pausedState;
        tagState.heldItemLostTags.unburden = true;
        NativePokemonSave tagSnapshot{};
        char tagBytes[512]{};
        size_t tagSize = 0;
        if (!captureNativePokemonActorSave(tagState, currentActor.actor, currentActor.totalExperience, tagSnapshot) ||
            !tagSnapshot.unburdenTag || encodeNativePokemonSave(tagSnapshot, tagBytes, sizeof(tagBytes), tagSize) !=
            NativeSaveResult::Ok || decodeNativePokemonSave(tagBytes, tagSize, decodedActor) != NativeSaveResult::Ok ||
            !restoreNativePokemonActorSave(decodedActor, restoredActor, restoredIdentity) ||
            !restoredActor.heldItemLostTags.unburden) return 405;
        tagState.friendship = 173;
        if (!captureNativePokemonActorSave(tagState, currentActor.actor, currentActor.totalExperience, tagSnapshot) ||
            encodeNativePokemonSave(tagSnapshot, tagBytes, sizeof(tagBytes), tagSize) != NativeSaveResult::Ok ||
            decodeNativePokemonSave(tagBytes, tagSize, decodedActor) != NativeSaveResult::Ok ||
            !decodedActor.friendshipResolved || decodedActor.friendship != 173 ||
            !restoreNativePokemonActorSave(decodedActor, restoredActor, restoredIdentity) ||
            restoredActor.friendship != 173) return 466;
        tagBytes[8] = '5';
        if (decodeNativePokemonSave(tagBytes, tagSize - 12, decodedActor) != NativeSaveResult::Ok ||
            decodedActor.maxPpResolved || !decodedActor.friendshipResolved || decodedActor.friendship != 173)
            return 473;
        tagBytes[8] = '4';
        if (decodeNativePokemonSave(tagBytes, tagSize - 15, decodedActor) != NativeSaveResult::Ok ||
            decodedActor.friendshipResolved || !decodedActor.unburdenTag ||
            !restoreNativePokemonActorSave(decodedActor, restoredActor, restoredIdentity) ||
            restoredActor.friendship != currentActor.battleState.friendship) return 467;
        tagBytes[8] = '3';
        if (decodeNativePokemonSave(tagBytes, tagSize - 18, decodedActor) != NativeSaveResult::Ok ||
            decodedActor.unburdenTag) return 406;
        tagBytes[8] = '6';
        tagBytes[tagSize - 17] = '2';
        if (decodeNativePokemonSave(tagBytes, tagSize, decodedActor) != NativeSaveResult::InvalidFormat ||
            decodedActor.unburdenTag) return 407;
        // Member schema 1 has no pause field: preserve its exact former layout.
        const size_t concreteTypeBytes = std::strlen(pausedActor.initialTeraType) + 1;
        if (pauseSize < concreteTypeBytes + 6) return 328;
        const size_t version2Size = pauseSize - concreteTypeBytes - 18;
        pauseBytes[8] = '2';
        if (decodeNativePokemonSave(pauseBytes, version2Size, decodedActor) != NativeSaveResult::Ok ||
            !decodedActor.pauseEvolutions) return 329;
        pauseBytes[8] = '1';
        if (decodeNativePokemonSave(pauseBytes, version2Size - 3, decodedActor) !=
                NativeSaveResult::Ok || decodedActor.pauseEvolutions) return 322;
        pauseBytes[8] = '2';
        pauseBytes[version2Size - 2] = '2';
        if (decodeNativePokemonSave(pauseBytes, version2Size, decodedActor) !=
                NativeSaveResult::InvalidFormat || decodedActor.pauseEvolutions) return 323;
        NativeRunSave explicitParty = loaded;
        explicitParty.playerPartyCount = 1;
        explicitParty.activePlayerMember = 0;
        explicitParty.playerParty[0] = actorSnapshot;
        char partyPayload[kNativeSaveMaxBytes]{};
        size_t partyPayloadSize = 0;
        NativeRunSave decodedParty{};
        if (encodeNativeRunSave(explicitParty, partyPayload, sizeof(partyPayload), partyPayloadSize) !=
                NativeSaveResult::Ok || decodeNativeRunSave(partyPayload, partyPayloadSize,
                PokerogueContent::kContentHash, decodedParty) != NativeSaveResult::Ok ||
            decodedParty.playerPartyCount != 1 || decodedParty.activePlayerMember ||
            decodedParty.playerParty[0].pokemonId != actorSnapshot.pokemonId) return 287;
        auto taggedRun = decodedParty;
        taggedRun.playerParty[0].unburdenTag = true;
        FirstRunRuntime taggedRuntime(7);
        NativeRunSave taggedRecaptured{};
        if (!taggedRuntime.restoreNativeRunSave(taggedRun)) return 408;
        taggedRuntime.captureNativeRunSave(taggedRecaptured);
        if (taggedRecaptured.playerPartyCount != 1 || !taggedRecaptured.playerParty[0].unburdenTag)
            return 409;
        FirstRunRuntime singleSnapshotRuntime(7);
        if (!singleSnapshotRuntime.restoreNativeRunSave(decodedParty)) return 317;
        NativeRunSave singleRecaptured{};
        singleSnapshotRuntime.captureNativeRunSave(singleRecaptured);
        if (singleRecaptured.playerPartyCount != 1 || singleRecaptured.activePlayerMember ||
            singleRecaptured.playerParty[0].pokemonId != actorSnapshot.pokemonId ||
            singleRecaptured.playerParty[0].experience != actorSnapshot.experience ||
            singleRecaptured.playerParty[0].speciesDex != actorSnapshot.speciesDex) return 318;
        for (uint8_t slot = 0; slot < actorSnapshot.moveCount; ++slot)
            if (singleRecaptured.playerParty[0].moveIds[slot] != actorSnapshot.moveIds[slot] ||
                singleRecaptured.playerParty[0].pp[slot] != actorSnapshot.pp[slot]) return 319;
        bool actorHasEvolutions = false;
        for (const auto& edge : PokerogueContent::kSpeciesEvolutions)
            actorHasEvolutions |= pokemonEvolutionTextEqual(edge.sourceSpeciesId,
                singleSnapshotRuntime.presentation().player.speciesId);
        if (singleSnapshotRuntime.togglePlayerEvolutionPause(6) ||
            singleSnapshotRuntime.togglePlayerEvolutionPause(0) != actorHasEvolutions) return 324;
        NativeRunSave toggledPauseSave{};
        singleSnapshotRuntime.captureNativeRunSave(toggledPauseSave);
        if (toggledPauseSave.playerPartyCount != 1 ||
            toggledPauseSave.playerParty[0].pauseEvolutions != actorHasEvolutions ||
            toggledPauseSave.battleTurn != singleRecaptured.battleTurn ||
            toggledPauseSave.wave != singleRecaptured.wave ||
            toggledPauseSave.playerHp != singleRecaptured.playerHp ||
            toggledPauseSave.playerExperience != singleRecaptured.playerExperience) return 325;
        if (!singleSnapshotRuntime.restoreNativeRunSave(toggledPauseSave) ||
            singleSnapshotRuntime.presentation().player.battleState.pauseEvolutions != actorHasEvolutions)
            return 326;
        if (actorHasEvolutions && (!singleSnapshotRuntime.togglePlayerEvolutionPause(0) ||
            singleSnapshotRuntime.presentation().player.battleState.pauseEvolutions)) return 327;
        NativeHeldModifierInstance runtimeHeld{};
        if (initializeHeldModifierInstance("MINI_BLACK_HOLE",
                singleSnapshotRuntime.presentation().player.battleState.pokemonId, 1, false, "[]", runtimeHeld) !=
                HeldModifierStorageResult::Ok || !singleSnapshotRuntime.restoreHeldModifierInventory(&runtimeHeld, 1) ||
            singleSnapshotRuntime.heldModifierCount() != 1 || !singleSnapshotRuntime.heldModifier(0) ||
            singleSnapshotRuntime.heldModifier(1)) return 374;
        auto invalidRuntimeHeld = runtimeHeld;
        invalidRuntimeHeld.canonicalItemIndex = PokerogueContent::kItemCount;
        if (singleSnapshotRuntime.restoreHeldModifierInventory(&invalidRuntimeHeld, 1) ||
            singleSnapshotRuntime.heldModifierCount() != 1 ||
            singleSnapshotRuntime.restoreHeldModifierInventory(&runtimeHeld,
                FirstRunRuntime::kHeldModifierStorageCapacity + 1)) return 375;
        const auto beforeUnsupportedHeldTurn = singleSnapshotRuntime.presentation().player.battleState.hp;
        if (singleSnapshotRuntime.advanceBattleTurn() ||
            singleSnapshotRuntime.presentation().player.battleState.hp != beforeUnsupportedHeldTurn) return 376;
        NativeRunSave unsupportedHeldSave{};
        singleSnapshotRuntime.captureNativeRunSave(unsupportedHeldSave);
        if (validateNativeRunSave(unsupportedHeldSave, PokerogueContent::kContentHash) != NativeSaveResult::Ok ||
            unsupportedHeldSave.heldModifierCount != 1 || !unsupportedHeldSave.playerPartyCount) return 377;
        char heldRunPayload[kNativeSaveMaxBytes]{};
        size_t heldRunSize = 0;
        NativeRunSave decodedHeldRun{};
        if (encodeNativeRunSave(unsupportedHeldSave, heldRunPayload, sizeof(heldRunPayload), heldRunSize) !=
                NativeSaveResult::Ok || decodeNativeRunSave(heldRunPayload, heldRunSize,
                PokerogueContent::kContentHash, decodedHeldRun) != NativeSaveResult::Ok ||
            decodedHeldRun.heldModifierCount != 1 || decodedHeldRun.heldModifiers[0].ownerPokemonId !=
                runtimeHeld.ownerPokemonId || decodedHeldRun.heldModifiers[0].transferable) return 384;
        FirstRunRuntime restoredHeldGame(7);
        if (!restoredHeldGame.restoreNativeRunSave(decodedHeldRun) || restoredHeldGame.heldModifierCount() != 1 ||
            std::strcmp(restoredHeldGame.heldModifier(0)->rawArguments, runtimeHeld.rawArguments)) return 385;
        // Historical v10 ends after explicit player members, before held inventory.
        char versionTenPayload[kNativeSaveMaxBytes]{};
        const char* heldSection = std::strstr(heldRunPayload, "heldModifierCount=");
        if (!heldSection) return 386;
        size_t versionTenSize = static_cast<size_t>(heldSection - heldRunPayload);
        std::memcpy(versionTenPayload, heldRunPayload, versionTenSize);
        char* tenSaveVersion = std::strstr(versionTenPayload, "saveVersion=000d");
        char* tenRuntimeVersion = std::strstr(versionTenPayload, "runtimeVersion=000d");
        if (!tenSaveVersion || !tenRuntimeVersion) return 387;
        tenSaveVersion[std::strlen("saveVersion=") + 3] = 'a';
        tenRuntimeVersion[std::strlen("runtimeVersion=") + 3] = 'a';
        char tenHash[65]{};
        IntegritySha256::hashHex(versionTenPayload, versionTenSize, tenHash);
        std::memcpy(versionTenPayload + versionTenSize, "sha256=", 7);
        std::memcpy(versionTenPayload + versionTenSize + 7, tenHash, 64);
        versionTenPayload[versionTenSize + 71] = '\n';
        versionTenSize += 72;
        NativeRunSave migratedTen{};
        if (decodeNativeRunSave(versionTenPayload, versionTenSize, PokerogueContent::kContentHash, migratedTen) !=
                NativeSaveResult::Ok || migratedTen.saveVersion != kNativeSaveVersion || migratedTen.runtimeVersion != kNativeSaveRuntimeVersion ||
            migratedTen.heldModifierCount || migratedTen.playerPartyCount != unsupportedHeldSave.playerPartyCount ||
            migratedTen.playerParty[0].pokemonId != unsupportedHeldSave.playerParty[0].pokemonId) return 388;
        if (!singleSnapshotRuntime.restoreSetup(singleRecaptured.seed, singleRecaptured.starterDex) ||
            singleSnapshotRuntime.heldModifierCount()) return 378;
        explicitParty.activePlayerMember = 1;
        if (validateNativeRunSave(explicitParty, PokerogueContent::kContentHash) !=
                NativeSaveResult::InvalidRecord) return 288;
        explicitParty.activePlayerMember = 0;
        ++explicitParty.playerParty[0].hp;
        if (validateNativeRunSave(explicitParty, PokerogueContent::kContentHash) !=
                NativeSaveResult::InvalidRecord) return 289;
        NativeRunSave inventorySnapshot = loaded;
        inventorySnapshot.pokeballCounts[0] = 0;
        inventorySnapshot.pokeballCounts[1] = 3;
        inventorySnapshot.pokeballCounts[2] = 99;
        inventorySnapshot.pokeballCounts[3] = 1;
        inventorySnapshot.pokeballCounts[4] = 2;
        if (!game.restoreNativeRunSave(inventorySnapshot)) return 264;
        NativeRunSave inventoryRoundtrip{};
        game.captureNativeRunSave(inventoryRoundtrip);
        if (validateNativeRunSave(inventoryRoundtrip, PokerogueContent::kContentHash) != NativeSaveResult::Ok)
            return 265;
        for (uint8_t ball = 0; ball < 5; ++ball)
            if (game.pokeballCount(static_cast<PokeballType>(ball)) != inventorySnapshot.pokeballCounts[ball] ||
                inventoryRoundtrip.pokeballCounts[ball] != inventorySnapshot.pokeballCounts[ball]) return 266;
        NativeRunSave invalidInventory = inventorySnapshot;
        invalidInventory.pokeballCounts[2] = 100;
        if (game.restoreNativeRunSave(invalidInventory) || game.pokeballCount(PokeballType::UltraBall) != 99)
            return 267;
        if (!game.restoreNativeRunSave(loaded)) return 268;
        if (loaded.weatherType || loaded.weatherTurnsLeft || loaded.weatherMaxDuration) return 23;
        NativeRunSave unsupportedWeather = loaded;
        unsupportedWeather.weatherType = 2;
        unsupportedWeather.weatherTurnsLeft = 3;
        unsupportedWeather.weatherMaxDuration = 5;
        if (validateNativeRunSave(unsupportedWeather, PokerogueContent::kContentHash) != NativeSaveResult::Ok) return 24;
        const bool weatherEligible = pokemonWeatherLifecycleSupported(game.presentation().player.battleState.abilityId) &&
            pokemonWeatherLifecycleSupported(game.presentation().enemy.battleState.abilityId);
        if (game.restoreNativeRunSave(unsupportedWeather) != weatherEligible) return 35;
        NativeRunSave afterWeatherRestore{};
        game.captureNativeRunSave(afterWeatherRestore);
        if (afterWeatherRestore.weatherType != (weatherEligible ? 2 : 0) ||
            afterWeatherRestore.weatherTurnsLeft != (weatherEligible ? 3 : 0) ||
            afterWeatherRestore.weatherMaxDuration != (weatherEligible ? 5 : 0) ||
            afterWeatherRestore.enemyHp != loaded.enemyHp || afterWeatherRestore.playerHp != loaded.playerHp ||
            afterWeatherRestore.battleTurn != loaded.battleTurn) return 25;
        if (!game.restoreNativeRunSave(loaded)) return 36;
        NativeRunSave roomCheckpoint = loaded;
        roomCheckpoint.trickRoomTurnsLeft = 3;
        roomCheckpoint.trickRoomMaxDuration = 5;
        roomCheckpoint.trickRoomSourceMoveId = 433;
        roomCheckpoint.trickRoomSourcePokemonId = context.player.battleState.pokemonId;
        if (!game.restoreNativeRunSave(roomCheckpoint)) return 26;
        NativeRunSave roomRestored{};
        game.captureNativeRunSave(roomRestored);
        if (roomRestored.trickRoomTurnsLeft != 3 || roomRestored.trickRoomMaxDuration != 5 ||
            roomRestored.trickRoomSourceMoveId != 433 ||
            roomRestored.trickRoomSourcePokemonId != roomCheckpoint.trickRoomSourcePokemonId) return 27;
        const int fieldCheck = checkResolvedActionFieldLifecycle();
        if (fieldCheck) return fieldCheck;
        const int replayCheck = checkTrainerExperienceReplay();
        if (replayCheck) return replayCheck;
        const int trainerCheck = checkTrainerInteractiveBattle();
        if (trainerCheck) return trainerCheck;
        const int rewardCheck = checkModifierRewardGenerationAndClaim();
        if (rewardCheck) return rewardCheck;
        const int biomeCheck = checkBiomeTransitionProgression();
        if (biomeCheck) return biomeCheck;
        const int doubleCheck = checkDoubleBattleTargetingAndMechanics();
        if (doubleCheck) return doubleCheck;
        const int bossCheck = checkWave200FinalBossAndGameClear();
        if (bossCheck) return bossCheck;
        const int captureCheck = checkPokeballCaptureMechanics();
        if (captureCheck) return captureCheck;
        const int partyCheck = checkPlayerPartyManagementAndSwitching();
        if (partyCheck) return partyCheck;
        const int evoCheck = checkLevelUpMoveLearningAndEvolution();
        if (evoCheck) return evoCheck;
        return checkExtendedWaveAndBiomeSaveValidation();
    }
    return 7; // No supported canonical encounter found: do not silently skip.
}

#include "game/FirstRunRuntime.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include <cstring>
#include "game/PokemonExperience.hpp"
#include "game/PokemonWeatherPhase.hpp"
#include "game/PokerogueClassicWaveSchedule.hpp"
#include "game/PokerogueEncounterResolver.hpp"
#include "game/PokerogueTrainerPartyLevels.hpp"

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

    const auto starterExperience = game.presentation().player.totalExperience;
    const auto reserveExperience = game.playerPartyMember(1)->totalExperience;
    const auto* caughtSpecies = PokerogueContent::findSpeciesByDex(caughtDex);
    uint32_t expectedReserveExperience = 0;
    if (!caughtSpecies || pokemonTotalExperienceForLevel(caughtSpecies->growthRate,
            game.playerPartyMember(1)->level, expectedReserveExperience) != PokemonExperienceResult::Ok ||
        reserveExperience != expectedReserveExperience) return 221;

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
    bulbaInit.moveCount = 1;
    bulbaInit.moveIds[0] = 33; // Tackle
    for (uint8_t i = 0; i < 6; ++i) bulbaInit.ivs[i] = 15;
    PokemonBattleState bulbaState{};
    if (initializePokemonBattleState(bulbaInit, bulbaState) != PokemonBattleInitResult::Ok) return 194;

    const uint16_t bulbaHp = bulbaState.hp;
    const uint16_t bulbaMaxHp = bulbaState.maxHp;
    EvolutionResult evoRes{};
    std::string evoFb;
    if (!applySpeciesEvolution(1, "ivysaur", bulbaState, evoRes, &evoFb)) return 195;
    if (!evoRes.evolved || evoRes.newDex != 2 || std::strcmp(evoRes.newSpeciesId, "ivysaur") != 0) return 196;
    if (bulbaState.speciesDex != 2) return 197;
    // Ivysaur has higher base stats and max HP than Bulbasaur
    if (bulbaState.maxHp <= bulbaMaxHp || bulbaState.hp <= bulbaHp) return 198;

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
        if (!game.restoreNativeRunSave(active)) return 4;
        if (!game.scene().nodes || !game.scene().nodeCount ||
            !game.scene().nodes[0].text || std::strcmp(game.scene().nodes[0].text,
                "POKEROGUE 3DS - CLASSIC 1")) return 5;
        NativeRunSave loaded{};
        game.captureNativeRunSave(loaded);
        if (loaded.stage != NativeSaveStage::BattleActive || loaded.enemyHp != active.enemyHp ||
            loaded.seed != seed || loaded.playerStatStages[0] != -2 ||
            loaded.enemyStatStages[4] != 3) return 6;
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

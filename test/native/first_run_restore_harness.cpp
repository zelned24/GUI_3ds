#include "game/FirstRunRuntime.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include <cstring>
#include "game/PokemonExperience.hpp"

// Standalone host regression for atomic checkpoint application. This requires
// the standard C++ library; it is not the freestanding WASM parity harness.
static int checkTrainerExperienceReplay() {
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
        NativeRunSave checkpoint{};
        game.captureNativeRunSave(checkpoint);
        if (checkpoint.stage != NativeSaveStage::BattleActive) continue;
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

int main() {
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
        if (loaded.weatherType || loaded.weatherTurnsLeft || loaded.weatherMaxDuration) return 23;
        NativeRunSave unsupportedWeather = loaded;
        unsupportedWeather.weatherType = 2;
        unsupportedWeather.weatherTurnsLeft = 3;
        unsupportedWeather.weatherMaxDuration = 5;
        if (validateNativeRunSave(unsupportedWeather, PokerogueContent::kContentHash) != NativeSaveResult::Ok ||
            game.restoreNativeRunSave(unsupportedWeather)) return 24;
        NativeRunSave afterWeatherReject{};
        game.captureNativeRunSave(afterWeatherReject);
        if (afterWeatherReject.weatherType || afterWeatherReject.enemyHp != loaded.enemyHp ||
            afterWeatherReject.playerHp != loaded.playerHp || afterWeatherReject.battleTurn != loaded.battleTurn) return 25;
        return checkTrainerExperienceReplay();
    }
    return 7; // No supported canonical encounter found: do not silently skip.
}

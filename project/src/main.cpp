#include "game/FirstRunRuntime.hpp"
#include "gfx/renderer2d.hpp"
#include "runtime/ScenePlayer.hpp"
#include "runtime/PokemonAtlasPresenter.hpp"
#include "storage/NativeRunSave.hpp"
#include "storage/NativeStarterCandyStore.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include <3ds.h>
#if defined(POKEROGUE_ENABLE_QUICKJS)
#include "runtime/QuickJSBridge.hpp"
#include <cstdio>
#include <new>
#endif

namespace {
bool canReplaySave(const Pokerogue3DS::NativeRunSave& save) {
    Pokerogue3DS::FirstRunRuntime replay(save.seed);
    return replay.restoreNativeRunSave(save);
}
}

int main() {
    gfxInitDefault();
    const bool romfsReady = R_SUCCEEDED(romfsInit());
    Renderer2D renderer;
    if (!renderer.init()) {
        if (romfsReady) romfsExit();
        gfxExit();
        return 1;
    }

#if defined(POKEROGUE_ENABLE_QUICKJS)
    Pokerogue3DS::QuickJSBridge bridge;
    bool bridgeReady = false;
    if (romfsReady && bridge.init(renderer)) {
        FILE* file = std::fopen("romfs:/js/bundle.js", "rb");
        if (file) {
            if (std::fseek(file, 0, SEEK_END) == 0) {
                const long size = std::ftell(file);
                if (size > 0 && size < 4 * 1024 * 1024 && std::fseek(file, 0, SEEK_SET) == 0) {
                    char* bytes = new (std::nothrow) char[static_cast<size_t>(size) + 1];
                    if (bytes) {
                        const size_t read = std::fread(bytes, 1, static_cast<size_t>(size), file);
                        const bool complete = read == static_cast<size_t>(size) && !std::ferror(file);
                        bytes[read] = 0;
                        if (complete) bridgeReady = bridge.evaluate(bytes, read, "bundle.js");
                        delete[] bytes;
                    }
                }
            }
            std::fclose(file);
        }
    }
    // An unavailable/invalid bundle retains the existing native path.
#endif
    Pokerogue3DS::FirstRunRuntime game(0x3D5C0DEu);
#if defined(POKEROGUE_ENABLE_QUICKJS)
    bridge.bindRuntime(game);
#endif
    Pokerogue3DS::SdNativeSaveStorage saveStorage;
    Pokerogue3DS::NativeRunSaveStore saves(saveStorage);
    // Catalog-sized journal workspace lives outside the ARM11 stack.
    static char profileScratch[2 * Pokerogue3DS::kStarterCandyProfileMaxBytes]{};
    Pokerogue3DS::SdNativeStarterCandyStorage profileStorage;
    Pokerogue3DS::NativeStarterCandyStore profiles(profileStorage, profileScratch, sizeof(profileScratch));
    saves.bindStarterProfiles(profiles);
#if defined(POKEROGUE_ENABLE_QUICKJS)
    bridge.bindSaveStore(saves);
#endif
    Pokerogue3DS::NativeRunSave restored{};
    const auto loaded = saves.load(PokerogueContent::kContentHash, restored);
    if (loaded == Pokerogue3DS::NativeSaveResult::Ok) {
#if defined(POKEROGUE_ENABLE_QUICKJS)
        bridge.setJournalGeneration(restored.generation);
#endif
        game.setStorageFeedback(game.restoreNativeRunSave(restored)
            ? "Progress restored  X: save  Y: export"
            : "Saved state does not match pinned content");
    } else if (loaded == Pokerogue3DS::NativeSaveResult::NotFound) {
        game.setStorageFeedback("X: save  Y: export  L: load  R: import");
    } else {
        game.setStorageFeedback(Pokerogue3DS::nativeSaveResultName(loaded));
    }
    Citro2D::ScenePlayer player(game.scene());
    Pokerogue3DS::PokemonAtlasPresenter pokemonSprites;
    player.enter();
    player.setLoop(true);

    while (aptMainLoop()) {
        hidScanInput();
        const uint32_t pressed = hidKeysDown();
        bool changed = false;
#if defined(POKEROGUE_ENABLE_QUICKJS)
        const bool jsCommands = bridgeReady && bridge.healthy();
        if (jsCommands) changed = bridge.processPendingAction();
        if (!jsCommands) {
#endif
        if (pressed & KEY_LEFT) changed = game.cycleStarter(-1);
        else if (pressed & KEY_RIGHT) changed = game.cycleStarter(1);
        else if (pressed & KEY_UP) { game.selectBattleMove(-1); changed = true; }
        else if (pressed & KEY_DOWN) { game.selectBattleMove(1); changed = true; }
        else if (pressed & KEY_A) { game.advanceBattleTurn(); changed = true; }
        else if (pressed & KEY_B) { game.skipVictoryReward(); changed = true; }
#if defined(POKEROGUE_ENABLE_QUICKJS)
        }
        // Starter navigation and SD operations remain host-owned; no duplicate battle input.
        // JS queues starter navigation and native save/load outside rendering.
#endif
        if (pressed & KEY_SELECT)
            changed = game.togglePlayerEvolutionPause(game.activePlayerPartyIndex()) || changed;
        uint32_t hostStorageKeys = pressed & (KEY_X | KEY_Y | KEY_L | KEY_R);
#if defined(POKEROGUE_ENABLE_QUICKJS)
        if (jsCommands) hostStorageKeys &= KEY_X | KEY_Y; // L/R now belong to bridge save/load.
#endif
        if (hostStorageKeys) {
            using namespace Pokerogue3DS;
            NativeSaveResult result = NativeSaveResult::Ok;
            if (pressed & (KEY_X | KEY_Y)) {
                NativeRunSave snapshot{};
                game.captureNativeRunSave(snapshot);
                result = validateNativeRunSave(snapshot, PokerogueContent::kContentHash);
                if (result == NativeSaveResult::Ok) result = saves.save(snapshot);
                if (result == NativeSaveResult::Ok && (pressed & KEY_Y)) result = saves.exportLatest(PokerogueContent::kContentHash);
            } else {
                if (pressed & KEY_R) {
                    char bytes[kNativeSaveMaxBytes];
                    std::size_t length = 0;
                    result = saveStorage.readExport(bytes, sizeof(bytes), length);
                    if (result == NativeSaveResult::Ok)
                        result = decodeNativeRunSave(bytes, length, PokerogueContent::kContentHash, restored);
                    if (result == NativeSaveResult::Ok && !canReplaySave(restored))
                        result = NativeSaveResult::InvalidRecord;
                    if (result == NativeSaveResult::Ok)
                        result = saves.importExport(PokerogueContent::kContentHash);
                }
                if (result == NativeSaveResult::Ok) result = saves.load(PokerogueContent::kContentHash, restored);
                if (result == NativeSaveResult::Ok && !canReplaySave(restored)) result = NativeSaveResult::InvalidRecord;
                if (result == NativeSaveResult::Ok && !game.restoreNativeRunSave(restored)) result = NativeSaveResult::InvalidRecord;
            }
            game.setStorageFeedback(result == NativeSaveResult::Ok
                ? ((pressed & KEY_Y) ? "Progress exported: /3ds/pokerogue/exports/progress.p3save" : "Progress saved/restored")
                : nativeSaveResultName(result));
            changed = true;
        }
        if (changed) player.load(game.scene());
#if defined(POKEROGUE_ENABLE_QUICKJS)
        if (bridgeReady && bridge.healthy()) {
            Pokerogue3DS::NativeRunSave snapshot{};
            game.captureNativeRunSave(snapshot);
            const auto& state = game.presentation();
            bridge.setPokemonPresentation(state.player, state.enemy, osGetTime());
            const auto& actor = state.player.battleState;
            const auto& opponent = state.enemy.battleState;
            const uint8_t slot = game.selectedBattleMove();
            const auto* selected = slot < actor.moveCount && slot < 4
                ? PokerogueContent::findMoveById(actor.moves[slot].moveId) : nullptr;
            uint16_t moveIds[4]{};
            uint8_t movePp[4]{};
            for (uint8_t i = 0; i < actor.moveCount && i < 4; ++i) {
                moveIds[i] = actor.moves[i].moveId;
                movePp[i] = actor.moves[i].pp;
            }
            char stateJson[768];
            const int length = std::snprintf(stateJson, sizeof(stateJson),
                "{\"wave\":%u,\"playerHp\":%u,\"playerMaxHp\":%u,"
                "\"enemyHp\":%u,\"enemyMaxHp\":%u,\"playerDex\":%u,"
                "\"enemyDex\":%u,\"stage\":%u,\"moveId\":%u,\"pp\":%u,"
                "\"supported\":%s,\"finished\":%s,\"selectedMove\":%u,"
                "\"playerMoves\":[%u,%u,%u,%u],\"playerPP\":[%u,%u,%u,%u],"
                "\"playerWon\":%s,\"experienceGranted\":%s,\"runStarted\":%s,"
                "\"starterDex\":%u,\"generation\":%u,\"finalWave\":%u,\"rewardPending\":%s}",
                unsigned(game.run().wave), unsigned(actor.hp), unsigned(actor.maxHp),
                unsigned(opponent.hp), unsigned(opponent.maxHp), unsigned(state.player.dex),
                unsigned(state.enemy.dex), unsigned(snapshot.stage), unsigned(selected ? selected->id : 0),
                unsigned(selected ? actor.moves[slot].pp : 0),
                game.battleInputSupported() ? "true" : "false", game.battleFinished() ? "true" : "false", unsigned(slot),
                unsigned(moveIds[0]), unsigned(moveIds[1]), unsigned(moveIds[2]), unsigned(moveIds[3]),
                unsigned(movePp[0]), unsigned(movePp[1]), unsigned(movePp[2]), unsigned(movePp[3]),
                game.playerWon() ? "true" : "false", game.experienceGranted() ? "true" : "false",
                game.runStarted() ? "true" : "false", unsigned(bridge.restartStarterDex()),
                unsigned(bridge.journalGeneration()), unsigned(PokerogueContent::kClassicFinalWave),
                game.battleFinished() && game.playerWon() && game.experienceGranted() &&
                    game.victoryPlan().contains(Pokerogue3DS::ClassicVictoryStep::SelectModifier) ? "true" : "false");
            if (length > 0 && static_cast<size_t>(length) < sizeof(stateJson))
                bridge.setBattleStateJson(stateJson, static_cast<size_t>(length));
        }
#endif

        renderer.beginFrame();
#if defined(POKEROGUE_ENABLE_QUICKJS)
        if (bridgeReady && bridge.healthy()) {
            bridge.tick(pressed);
            renderer.endFrame();
            gspWaitForVBlank();
            continue; // Native drawing must not overwrite the JS diagnostic frame.
        }
#endif
        renderer.beginTop();
        if (romfsReady) {
            const uint64_t animationTimeMs = osGetTime();
            pokemonSprites.draw(renderer, game.presentation().player, true,
                30.0f, 112.0f, 96.0f, 96.0f, animationTimeMs);
            pokemonSprites.draw(renderer, game.presentation().enemy, false,
                267.0f, 66.0f, 96.0f, 96.0f, animationTimeMs);
        }
        player.renderTop(renderer);
        renderer.beginBottom();
        player.renderBottom(renderer);
        renderer.endFrame();
        gspWaitForVBlank();
    }

    player.exit();
    pokemonSprites.invalidate();
#if defined(POKEROGUE_ENABLE_QUICKJS)
    bridge.fini();
#endif
    renderer.fini();
    if (romfsReady) romfsExit();
    gfxExit();
    return 0;
}

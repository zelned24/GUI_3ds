#include "game/FirstRunRuntime.hpp"
#include "gfx/renderer2d.hpp"
#include "runtime/ScenePlayer.hpp"
#include "runtime/PokemonAtlasPresenter.hpp"
#include "storage/NativeRunSave.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include <3ds.h>

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

    Pokerogue3DS::FirstRunRuntime game(0x3D5C0DEu);
    Pokerogue3DS::SdNativeSaveStorage saveStorage;
    Pokerogue3DS::NativeRunSaveStore saves(saveStorage);
    Pokerogue3DS::NativeRunSave restored{};
    const auto loaded = saves.load(PokerogueContent::kContentHash, restored);
    if (loaded == Pokerogue3DS::NativeSaveResult::Ok) {
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
        if (pressed & KEY_LEFT) changed = game.cycleStarter(-1);
        else if (pressed & KEY_RIGHT) changed = game.cycleStarter(1);
        else if (pressed & KEY_UP) { game.selectBattleMove(-1); changed = true; }
        else if (pressed & KEY_DOWN) { game.selectBattleMove(1); changed = true; }
        else if (pressed & KEY_A) { game.advanceBattleTurn(); changed = true; }
        if (pressed & (KEY_X | KEY_Y | KEY_L | KEY_R)) {
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

        renderer.beginFrame();
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
    renderer.fini();
    if (romfsReady) romfsExit();
    gfxExit();
    return 0;
}

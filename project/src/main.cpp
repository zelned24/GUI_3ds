#include "content/WindowTexture.hpp"
#include "game/FirstRunRuntime.hpp"
#include "gfx/renderer2d.hpp"
#include "storage/NativeRunSave.hpp"
#include "storage/NativePresentationSettings.hpp"
#include "storage/NativeStarterCandyStore.hpp"
#include "storage/NativeProgressStore.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include "runtime/ScenePlayer.hpp"
#include "runtime/PokemonAtlasPresenter.hpp"
#include "runtime/ArenaPresenter.hpp"
#include "runtime/BattleHudPresenter.hpp"
#include "runtime/MoveMenuPresenter.hpp"
#include "runtime/BattleCommandMenuPresenter.hpp"
#include "runtime/SetupPresenter.hpp"
#include "runtime/FrontendMenuPresenter.hpp"
#include "runtime/PartyMenuPresenter.hpp"
#include "runtime/RewardMenuPresenter.hpp"
#include "runtime/DecisionMenuPresenter.hpp"
#include "game/PokerogueModifierReward.hpp"
#include "content/IntroCinematicData.hpp"
#include <3ds.h>
#if defined(POKEROGUE_ENABLE_QUICKJS)
#include "runtime/QuickJSBridge.hpp"
#include <cstdio>
#include <new>
#endif

// Define 256 KB stack for Old 3DS (ARM11 MPCore @ 268 MHz).
// Default is only 32 KB, which overflows when deep battle engine routines
// and complex UI trees execute. Actual available memory still requires measurement.
#if defined(__3DS__) || defined(_3DS)
extern "C" {
u32 __stacksize__ = 256 * 1024;
}
#endif

namespace {
Pokerogue3DS::FirstRunRuntime& progressReplay() {
    // Native/JS preflights run serially and share workspace outside the ARM11 stack.
    static Pokerogue3DS::FirstRunRuntime replay(1);
    return replay;
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

    // Double-buffered pure black clear immediately on boot so uninitialized VRAM is never shown
    for (int b = 0; b < 2; ++b) {
        renderer.beginFrame();
        renderer.beginTop();
        renderer.clear(0xff000000);
        renderer.beginBottom();
        renderer.clear(0xff000000);
        renderer.endFrame();
        gspWaitForVBlank();
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
    // The live runtime includes a catalog-sized starter ledger.
    static Pokerogue3DS::FirstRunRuntime game(0x3D5C0DEu);
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
    Pokerogue3DS::NativeProgressStore progress(saves, profiles);
    static char bundleScratch[2 * Pokerogue3DS::kNativeProgressBundleMaxBytes]{};
    static Pokerogue3DS::NativeStarterCandyRecord profileStaging[PokerogueContent::kSpeciesCount]{};
    Pokerogue3DS::PokemonFriendshipPolicy offlineFriendship{};
    offlineFriendship.resolved = true; // Explicit offline baseline: no timed event/fusion/boosters.
    offlineFriendship.candyMultiplier = PokerogueContent::kClassicCandyFriendshipMultiplier;
#if defined(POKEROGUE_ENABLE_QUICKJS)
    bridge.bindSaveStore(saves);
    bridge.bindProgressStore(progress, profiles, profileStaging, PokerogueContent::kSpeciesCount, offlineFriendship);
    bridge.bindProgressBundle(saveStorage, bundleScratch, sizeof(bundleScratch), progressReplay());
#endif
    static Pokerogue3DS::NativeRunSave restored{};
    auto loaded = game.loadNativeProgress(saves, profiles, profileStaging,
        PokerogueContent::kSpeciesCount, offlineFriendship, &restored);
    if (loaded == Pokerogue3DS::NativeSaveResult::Ok) {
#if defined(POKEROGUE_ENABLE_QUICKJS)
        bridge.setJournalGeneration(restored.generation);
#endif
        game.setStorageFeedback("Progress/profile restored  X: save  Y: export");
    } else if (loaded == Pokerogue3DS::NativeSaveResult::NotFound) {
        size_t count = 0;
        uint32_t generation = 0;
        auto profileLoaded = profiles.load(PokerogueContent::kContentHash, profileStaging,
            PokerogueContent::kSpeciesCount, count, generation);
        if (profileLoaded == Pokerogue3DS::NativeSaveResult::NotFound) {
            profileLoaded = game.initializeFreshStarterProfile(offlineFriendship)
                ? Pokerogue3DS::NativeSaveResult::Ok : Pokerogue3DS::NativeSaveResult::InvalidRecord;
        } else if (profileLoaded == Pokerogue3DS::NativeSaveResult::Ok &&
            !game.restoreStarterCandyProfile(profileStaging, count, 0, offlineFriendship)) {
            profileLoaded = Pokerogue3DS::NativeSaveResult::InvalidRecord;
        }
        game.setStorageFeedback(profileLoaded == Pokerogue3DS::NativeSaveResult::Ok
            ? "X: save profile/run  Y: export  L: load"
            : Pokerogue3DS::nativeSaveResultName(profileLoaded));
    } else {
        game.setStorageFeedback(Pokerogue3DS::nativeSaveResultName(loaded));
    }
    Citro2D::ScenePlayer player(game.scene());
    // Presenters are declared static to prevent putting 8+ KB on the ARM11 runtime stack,
    // avoiding stack overflow and maximizing L1 data cache efficiency on Old 3DS.
    static Pokerogue3DS::PokemonAtlasPresenter pokemonSprites;
    static Pokerogue3DS::PokemonAtlasPresenter secondEnemySprites;
    static Pokerogue3DS::ArenaPresenter arena;
    static Pokerogue3DS::BattleHudPresenter battleHud;
    static Pokerogue3DS::SetupPresenter setup;
    static Pokerogue3DS::PartyMenuPresenter partyMenu;
    static Pokerogue3DS::BattleCommandMenuPresenter battleMenu;
    static Pokerogue3DS::RewardMenuPresenter rewardMenu;
    static Pokerogue3DS::DecisionMenuPresenter decisionMenu;
    player.enter();
    player.setLoop(true);

    static Pokerogue3DS::FrontendMenuPresenter frontend(loaded==Pokerogue3DS::NativeSaveResult::Ok);
    if(loaded!=Pokerogue3DS::NativeSaveResult::Ok && loaded!=Pokerogue3DS::NativeSaveResult::NotFound)
        frontend.feedback(Pokerogue3DS::nativeSaveResultName(loaded));
    Pokerogue3DS::SdNativePresentationStorage uiStorage;
    Pokerogue3DS::NativePresentationSettingsStore uiSettings(uiStorage);
    Pokerogue3DS::NativePresentationSettings preferences;
    bool recoveredPreferences=false;
    const auto preferenceResult=uiSettings.load(preferences,&recoveredPreferences);
    if(preferenceResult==Pokerogue3DS::NativeSaveResult::Ok) {
        if(!renderer.setWindowStyle(preferences.windowStyle)) frontend.feedback("No se pudo cargar el marco guardado.");
        else if(recoveredPreferences) frontend.feedback("Ajustes recuperados del respaldo SD.");
    } else if(preferenceResult!=Pokerogue3DS::NativeSaveResult::NotFound)
        frontend.feedback(Pokerogue3DS::nativeSaveResultName(preferenceResult));
    bool titleVisible = true;
    bool isPaused = false;
    const char* pauseFeedback = nullptr;
    const auto saveAndReturnToTitle = [&]() {
        const auto result = game.saveNativeProgress(progress);
        if (result != Pokerogue3DS::NativeSaveResult::Ok) {
            pauseFeedback = Pokerogue3DS::nativeSaveResultName(result);
            return false;
        }
        const auto readback = saves.load(PokerogueContent::kContentHash, restored);
        if (readback != Pokerogue3DS::NativeSaveResult::Ok) {
            pauseFeedback = Pokerogue3DS::nativeSaveResultName(readback);
            return false;
        }
        loaded = readback;
        frontend.setHasSave(true);
        titleVisible = true;
        isPaused = false;
        return true;
    };
    bool introActive = true;
    unsigned pauseSelection = 0;
    uint64_t m_renderTicks = 0;

    const auto isRecoveryReward = [](const char* itemId) -> bool {
        if (!itemId) return false;
        return (Pokerogue3DS::hpRestoreItemProfile(itemId) != nullptr ||
                Pokerogue3DS::ppRestoreItemProfile(itemId) != nullptr ||
                Pokerogue3DS::ppUpItemProfile(itemId) != nullptr ||
                Pokerogue3DS::reviveItemProfile(itemId) != nullptr);
    };
    const auto isPartyTargetReward = [&](const char* itemId) -> bool {
        if (!itemId) return false;
        if (isRecoveryReward(itemId)) return true;
        Pokerogue3DS::NativeHeldModifierInstance heldReward{};
        return Pokerogue3DS::initializeHeldModifierInstance(itemId, 0, 1, true, nullptr, heldReward) ==
            Pokerogue3DS::HeldModifierStorageResult::Ok;
    };

    while (aptMainLoop()) {
        ++m_renderTicks;
        const uint64_t frameAnimationTimeMs = m_renderTicks * 1000 / 60;
        hidScanInput();
        uint32_t rawPressed = hidKeysDown();
        if (titleVisible) {
            if (introActive) {
                if (rawPressed & (KEY_A | KEY_B | KEY_START | KEY_TOUCH)) {
                    setup.skipIntro();
                    introActive = false;
                    rawPressed = 0; // Skipping the intro must not activate the title selection.
                } else if (frameAnimationTimeMs >= (Pokerogue3DS::kIntroTotalDurationMs + 300)) {
                    introActive = false;
                }
            }
            if (introActive) {
                renderer.beginFrame();
                renderer.beginTop();
                renderer.clear(0xff000000);
                if (romfsReady) arena.draw(renderer,game.run().biomeId,frameAnimationTimeMs,false);
                setup.drawTop(renderer,game,false,frameAnimationTimeMs);
                renderer.beginBottom();
                renderer.clear(0xff000000);
                renderer.endFrame();
                gspWaitForVBlank();
                continue;
            }
            // The menu consumes input before the battle/JS command loop.
            touchPosition titleTouch{};if(rawPressed & KEY_TOUCH) hidTouchRead(&titleTouch);
            const auto command=frontend.input(rawPressed,titleTouch.px,titleTouch.py);
            if(command==Pokerogue3DS::FrontendCommand::Continue || command==Pokerogue3DS::FrontendCommand::Load) {
                const auto result=game.loadNativeProgress(saves,profiles,profileStaging,
                    PokerogueContent::kSpeciesCount,offlineFriendship,&restored);
                loaded=result;
                if(result==Pokerogue3DS::NativeSaveResult::Ok) {
                    frontend.setHasSave(true);
                    frontend.feedback(nullptr);
                    player.load(game.scene());
                    titleVisible=false;
#if defined(POKEROGUE_ENABLE_QUICKJS)
                    bridge.setJournalGeneration(restored.generation);
#endif
                } else {
                    frontend.feedback(Pokerogue3DS::nativeSaveResultName(result));
                }
            }
            else if(command==Pokerogue3DS::FrontendCommand::NextWindowStyle || command==Pokerogue3DS::FrontendCommand::PreviousWindowStyle) {
                unsigned next=Pokerogue3DS::kWindowTextures[0].id;
                for(std::size_t i=0;i<Pokerogue3DS::kWindowTextureCount;++i)
                    if(Pokerogue3DS::kWindowTextures[i].id==renderer.windowStyle())
                        next=Pokerogue3DS::kWindowTextures[(i+Pokerogue3DS::kWindowTextureCount+
                            (command==Pokerogue3DS::FrontendCommand::PreviousWindowStyle ? -1 : 1))%Pokerogue3DS::kWindowTextureCount].id;
                if(!renderer.setWindowStyle(next)) frontend.feedback("No se pudo cargar el marco.");
                else {
                    const auto saved=uiSettings.save(next);
                    frontend.feedback(saved==Pokerogue3DS::NativeSaveResult::Ok ? nullptr :
                        "Marco aplicado; no se pudieron guardar los ajustes SD.");
                }
            }
            else if(command==Pokerogue3DS::FrontendCommand::NewClassic) {
                const uint16_t starterDex = game.run().starterDex ? game.run().starterDex : 1;
                // Reuse the explicit run seed; gameplay must not depend on the clock.
                const uint32_t seed = game.run().seed;
                if(game.restoreSetup(seed, starterDex)) {
                    player.load(game.scene());
                    titleVisible=false;
                } else frontend.feedback("No se pudo iniciar la partida.");
            } else if(command==Pokerogue3DS::FrontendCommand::DeleteSave) {
                const auto delResult = saves.deleteSave();
                if(delResult == Pokerogue3DS::NativeSaveResult::Ok) {
                    loaded = Pokerogue3DS::NativeSaveResult::NotFound;
                    restored = {};
                    frontend.setHasSave(false);
                    frontend.feedback("Partida guardada eliminada.");
                } else {
                    frontend.feedback("Error al eliminar la partida.");
                }
            }
            renderer.beginFrame();
            renderer.beginTop();
            renderer.clear(0xff281f22);
            if (romfsReady) arena.draw(renderer,game.run().biomeId,frameAnimationTimeMs,false);
            setup.drawTop(renderer,game,false,frameAnimationTimeMs);
            renderer.beginBottom();
            frontend.draw(renderer,loaded==Pokerogue3DS::NativeSaveResult::Ok ? &restored : nullptr);
            renderer.endFrame();
            gspWaitForVBlank();
            continue;
        }
        if (!partyMenu.available(game)) partyMenu.open=false;
        const bool setupInput=game.presentationStage()==Pokerogue3DS::NativeSaveStage::RunSetup;
        if (!setupInput && (rawPressed & KEY_START)) {
            isPaused = !isPaused;
            pauseSelection = 0;
            pauseFeedback = nullptr;
            rawPressed &= ~KEY_START; // Do not immediately close the pause we just opened.
        }
        if (isPaused) {
            if (rawPressed & (KEY_UP | KEY_CPAD_UP)) {
                pauseSelection = (pauseSelection + 2) % 3;
            } else if (rawPressed & (KEY_DOWN | KEY_CPAD_DOWN)) {
                pauseSelection = (pauseSelection + 1) % 3;
            } else if (rawPressed & (KEY_B | KEY_START)) {
                isPaused = false;
            } else if (rawPressed & KEY_TOUCH) {
                touchPosition touch{};
                hidTouchRead(&touch);
                if (touch.px >= 24 && touch.px <= 296) {
                    if (touch.py >= 42 && touch.py < 84) {
                        pauseSelection = 0;
                        isPaused = false;
                    } else if (touch.py >= 84 && touch.py < 126) {
                        pauseSelection = 1;
                        saveAndReturnToTitle();
                    } else if (touch.py >= 126 && touch.py < 172) {
                        pauseSelection = 2;
                        titleVisible = true;
                        isPaused = false;
                    }
                }
            } else if (rawPressed & KEY_A) {
                if (pauseSelection == 0) {
                    isPaused = false;
                } else if (pauseSelection == 1) {
                    saveAndReturnToTitle();
                } else if (pauseSelection == 2) {
                    titleVisible = true;
                    isPaused = false;
                }
            }
            rawPressed = 0; // Pause owns this input, including the resume button.
        }
        if (titleVisible) continue; // A menu action cannot also become a battle action.
        if(!partyMenu.available(game)) partyMenu.open=false;
        const bool partyInput=!isPaused && (partyMenu.open || ((rawPressed & KEY_SELECT) && partyMenu.available(game)));
        const bool battleMenuInput=!isPaused && !partyInput && Pokerogue3DS::MoveMenuPresenter::visible(game) && !game.capturePartyChoicePending();
        const bool rewardInput=!isPaused && game.rewardsPending() && !game.moveLearningPending() && !game.evolutionPending();
        const uint32_t pressed=(isPaused || partyInput) ? 0 : (setupInput || battleMenuInput) ? rawPressed & (KEY_X | KEY_Y)
            : rewardInput ? rawPressed & (KEY_X | KEY_Y | KEY_L | KEY_R) : rawPressed;
        bool changed = false;
        if (!isPaused) {
        if(setupInput && setup.confirmStart) {
            if(rawPressed & (KEY_LEFT | KEY_RIGHT | KEY_CPAD_LEFT | KEY_CPAD_RIGHT)) setup.confirmYes=!setup.confirmYes;
            if(rawPressed & KEY_B) setup.confirmStart=false;
            if(rawPressed & KEY_TOUCH) {
                touchPosition touch{};hidTouchRead(&touch);
                if(touch.py>=116 && touch.py<153) {
                    if(touch.px<160) {
                        changed=game.startRun();
                        setup.confirmStart=false;
                    } else {
                        setup.confirmStart=false;
                    }
                }
            }
            if(rawPressed & KEY_A) {
                if(setup.confirmYes) changed=game.startRun();
                setup.confirmStart=false;
            }
        } else if(setupInput && setup.formsOpen) {
            const unsigned count=setup.formCount(game);
            if(count && (rawPressed & (KEY_UP | KEY_CPAD_UP))) setup.selectedForm=(setup.selectedForm+count-1)%count;
            if(count && (rawPressed & (KEY_DOWN | KEY_CPAD_DOWN))) setup.selectedForm=(setup.selectedForm+1)%count;
            if(rawPressed & KEY_TOUCH) {
                touchPosition touch{};hidTouchRead(&touch);
                if(touch.px>=24 && touch.px<296 && touch.py>=43 && touch.py<181) {
                    const unsigned row=setup.selectedForm/6*6+(touch.py-43)/23;
                    if(row<count) {
                        if(setup.selectedForm==row) {
                            const auto result=game.selectSetupStarterForm(game.selectedSetupStarterDex(),setup.formIndexAt(game,setup.selectedForm),progress);
                            changed=result==Pokerogue3DS::NativeSaveResult::Ok;
                            if(changed) setup.formsOpen=false;
                            else setup.formFeedback="Forma no disponible o guardado fallido.";
                        } else {
                            setup.selectedForm=row;
                        }
                    }
                } else if(touch.py>=180) {
                    setup.formsOpen=false;
                }
            }
            if(rawPressed & KEY_B) setup.formsOpen=false;
            if(rawPressed & KEY_A) {
                const auto result=game.selectSetupStarterForm(game.selectedSetupStarterDex(),setup.formIndexAt(game,setup.selectedForm),progress);
                changed=result==Pokerogue3DS::NativeSaveResult::Ok;
                if(changed) setup.formsOpen=false;
                else setup.formFeedback="Forma no disponible o guardado fallido.";
            }
        } else if(setupInput) {
            if(rawPressed & KEY_SELECT) {setup.formsOpen=true;setup.selectedForm=0;setup.formFeedback=nullptr;setup.feedback=nullptr;}
            else if(rawPressed & (KEY_LEFT | KEY_CPAD_LEFT)) {changed=setup.move(game,-1);setup.feedback=nullptr;}
            else if(rawPressed & (KEY_RIGHT | KEY_CPAD_RIGHT)) {changed=setup.move(game,1);setup.feedback=nullptr;}
            else if(rawPressed & (KEY_UP | KEY_CPAD_UP)) {changed=setup.move(game,-6);setup.feedback=nullptr;}
            else if(rawPressed & (KEY_DOWN | KEY_CPAD_DOWN)) {changed=setup.move(game,6);setup.feedback=nullptr;}
            else if(rawPressed & KEY_L) {changed=setup.move(game,-int(Pokerogue3DS::kStarterGridPageSize));setup.feedback=nullptr;}
            else if(rawPressed & KEY_R) {changed=setup.move(game,int(Pokerogue3DS::kStarterGridPageSize));setup.feedback=nullptr;}
            else if(rawPressed & KEY_TOUCH) {
                touchPosition touch{};hidTouchRead(&touch);
                if(touch.py>=29 && touch.py<183) {
                    const int cell=Pokerogue3DS::starterGridAt(touch.px,touch.py);
                    if(cell>=0) {
                        const auto* species=Pokerogue3DS::SetupPresenter::at(Pokerogue3DS::SetupPresenter::selectedOrdinal(game)/Pokerogue3DS::kStarterGridPageSize*Pokerogue3DS::kStarterGridPageSize+unsigned(cell));
                        if(species) {
                            if(species->dex==game.selectedSetupStarterDex()) {
                                if(!game.starterUnlocked(species->dex)) {
                                    setup.feedback="Pokemon bloqueado: capturalo para usarlo.";
                                } else if(!game.toggleSetupStarter()) {
                                    setup.feedback="Supera coste (10 pts) o equipo lleno.";
                                } else {
                                    setup.feedback=nullptr;
                                    changed=true;
                                }
                            } else {
                                changed=game.selectSetupStarter(species->dex);
                                setup.feedback=nullptr;
                            }
                        }
                    }
                } else if(touch.py>=186 && touch.py<212) {
                    for(unsigned slot=0;slot<6;++slot) {
                        if(touch.px>=8+slot*50 && touch.px<8+slot*50+46) {
                            if(slot<game.presentation().playerPartyCount) {
                                changed=game.selectSetupStarter(game.presentation().playerParty[slot].dex);
                                setup.feedback=nullptr;
                            }
                            break;
                        }
                    }
                } else if(touch.py>=212) {
                    if(touch.px<90) {
                        setup.formsOpen=true;setup.selectedForm=0;setup.formFeedback=nullptr;setup.feedback=nullptr;
                    } else if(touch.px>=90 && touch.px<=220) {
                        setup.confirmStart=true;setup.confirmYes=true;setup.feedback=nullptr;
                    } else {
                        titleVisible=true;setup.feedback=nullptr;
                    }
                }
            }
            else if(rawPressed & KEY_A) {
                const uint16_t currentDex = game.selectedSetupStarterDex();
                if(!game.starterUnlocked(currentDex)) {
                    setup.feedback="Pokemon bloqueado: capturalo para usarlo.";
                } else if(!game.toggleSetupStarter()) {
                    setup.feedback="Supera coste (10 pts) o equipo lleno.";
                } else {
                    setup.feedback=nullptr;
                    changed=true;
                }
            }
            else if(rawPressed & KEY_START) {setup.confirmStart=true;setup.confirmYes=true;setup.feedback=nullptr;}
            else if(rawPressed & KEY_B) {titleVisible=true;setup.feedback=nullptr;}
        }
        if(battleMenuInput) {
            touchPosition touch{};if(rawPressed & KEY_TOUCH) hidTouchRead(&touch);
            const auto command=battleMenu.input(rawPressed,game.doubleBattle(),touch.px,touch.py);
            switch(command) {
            case Pokerogue3DS::BattleMenuCommand::MovePrevious:changed=game.selectBattleMove(-1);break;
            case Pokerogue3DS::BattleMenuCommand::MoveNext:changed=game.selectBattleMove(1);break;
            case Pokerogue3DS::BattleMenuCommand::MoveRowToggle: {
                const unsigned cur = game.selectedBattleMove();
                const unsigned target = cur ^ 2;
                if (target < game.presentation().player.moveCount) {
                    const int delta = int(target) - int(cur);
                    for (int steps = delta < 0 ? -delta : delta; steps > 0; --steps)
                        game.selectBattleMove(delta < 0 ? -1 : 1);
                    changed = true;
                }
                break;
            }
            case Pokerogue3DS::BattleMenuCommand::MoveColToggle: {
                const unsigned cur = game.selectedBattleMove();
                const unsigned target = cur ^ 1;
                if (target < game.presentation().player.moveCount) {
                    const int delta = int(target) - int(cur);
                    for (int steps = delta < 0 ? -delta : delta; steps > 0; --steps)
                        game.selectBattleMove(delta < 0 ? -1 : 1);
                    changed = true;
                }
                break;
            }
            case Pokerogue3DS::BattleMenuCommand::TargetPrevious:changed=game.cycleTarget(-1);break;
            case Pokerogue3DS::BattleMenuCommand::TargetNext:changed=game.cycleTarget(1);break;
            case Pokerogue3DS::BattleMenuCommand::ExecuteMove:changed=game.advanceBattleTurn();battleMenu.reset();break;
            case Pokerogue3DS::BattleMenuCommand::Party:partyMenu.open=true;partyMenu.selected=game.activePlayerPartyIndex();break;
            case Pokerogue3DS::BattleMenuCommand::ThrowBall:changed=game.throwPokeball(battleMenu.ballType());battleMenu.reset();break;
            case Pokerogue3DS::BattleMenuCommand::Flee:changed=game.fleeBattle();battleMenu.reset();break;
            default:break;
            }
        }
        if (partyInput) {
            if (rawPressed & KEY_SELECT) {
                partyMenu.open=!partyMenu.open;
                partyMenu.selected=game.activePlayerPartyIndex();
            } else if (rawPressed & KEY_TOUCH) {
                touchPosition touch{};hidTouchRead(&touch);
                const int selected=Pokerogue3DS::partyButtonAt(touch.px,touch.py,game.presentation().playerPartyCount);
                if (selected>=0) {
                    if(partyMenu.selected==unsigned(selected)) {
                        changed=game.switchPlayerPokemon(partyMenu.selected);
                        if(changed) partyMenu.open=false;
                    } else {
                        partyMenu.selected=unsigned(selected);
                    }
                } else if(touch.py>=200) {
                    partyMenu.open=false;
                }
            } else if (rawPressed & KEY_B) partyMenu.open=false;
            else if (rawPressed & (KEY_UP | KEY_CPAD_UP)) partyMenu.move(-1,game.presentation().playerPartyCount);
            else if (rawPressed & (KEY_DOWN | KEY_CPAD_DOWN)) partyMenu.move(1,game.presentation().playerPartyCount);
            else if (rawPressed & KEY_A) {
                changed=game.switchPlayerPokemon(partyMenu.selected);
                if (changed) partyMenu.open=false;
            }
        }
        // Native reward selection owns buttons and touch together, including when
        // QuickJS is healthy. UI dispatches the existing transactional commands.
        if(rewardInput && rawPressed) {
            touchPosition touch{};
            if(rawPressed & KEY_TOUCH) hidTouchRead(&touch);
            const auto* choice=game.rewardChoice(game.selectedRewardChoice());
            const char* itemId=choice && choice->poolEntry ? choice->poolEntry->itemId : nullptr;
            const auto claimForMember=[&]() {
                const auto member=static_cast<uint8_t>(rewardMenu.partyPresenter().selected);
                const auto* restore=Pokerogue3DS::ppRestoreItemProfile(itemId);
                if(Pokerogue3DS::ppUpItemProfile(itemId) || (restore && !restore->allMoves)) {
                    rewardMenu.setMoveSelectionMode(true);
                    return;
                }
                const bool applied=isRecoveryReward(itemId) ? game.claimRecoveryRewardChoice(member)
                    : game.claimHeldRewardChoice(member);
                if(applied) rewardMenu.resetSelection();
            };
            const auto chooseReward=[&]() {
                if(isPartyTargetReward(itemId)) {
                    rewardMenu.partyPresenter().selected=game.activePlayerPartyIndex();
                    rewardMenu.setPartySelectionMode(true);
                } else if(game.claimRewardChoice()) rewardMenu.resetSelection();
            };
            if(rewardMenu.moveSelectionMode()) {
                const auto& field=game.presentation();
                const unsigned member=rewardMenu.partyPresenter().selected;
                if(member>=field.playerPartyCount) rewardMenu.resetSelection();
                else {
                    const auto& actor=member==field.activePlayerPartyIndex ? field.player : field.playerParty[member];
                    auto& selection=rewardMenu.moveSelection();
                    bool confirm=(rawPressed & KEY_A)!=0;
                    if(rawPressed & KEY_B) {rewardMenu.setPartySelectionMode(true);confirm=false;}
                    else if(rawPressed & (KEY_UP | KEY_CPAD_UP)) selection.move(-1,actor.battleState.moveCount);
                    else if(rawPressed & (KEY_DOWN | KEY_CPAD_DOWN)) selection.move(1,actor.battleState.moveCount);
                    else if(rawPressed & KEY_TOUCH) {
                        const int slot=Pokerogue3DS::RewardMoveSelection::hit(touch.px,touch.py,actor.battleState.moveCount);
                        if(slot>=0) {confirm=selection.selected==unsigned(slot);selection.selected=unsigned(slot);}
                        else if(touch.py>=200) rewardMenu.setPartySelectionMode(true);
                    }
                    if(confirm && game.claimRecoveryRewardChoice(static_cast<uint8_t>(member),static_cast<uint8_t>(selection.selected))) rewardMenu.resetSelection();
                }
            } else if(rewardMenu.partySelectionMode()) {
                if(rawPressed & KEY_B) rewardMenu.resetSelection();
                else if(rawPressed & (KEY_UP | KEY_CPAD_UP)) rewardMenu.partyPresenter().move(-1,game.playerPartyCount());
                else if(rawPressed & (KEY_DOWN | KEY_CPAD_DOWN)) rewardMenu.partyPresenter().move(1,game.playerPartyCount());
                else if(rawPressed & KEY_A) claimForMember();
                else if(rawPressed & KEY_TOUCH) {
                    const int member=Pokerogue3DS::partyButtonAt(touch.px,touch.py,game.playerPartyCount());
                    if(member>=0) {
                        if(rewardMenu.partyPresenter().selected==unsigned(member)) claimForMember();
                        else rewardMenu.partyPresenter().selected=unsigned(member);
                    } else if(touch.py>=200) rewardMenu.resetSelection();
                }
            } else if(rawPressed & KEY_B) game.skipVictoryReward();
            else if(rawPressed & (KEY_LEFT | KEY_CPAD_LEFT)) game.selectRewardChoice(-1);
            else if(rawPressed & (KEY_RIGHT | KEY_CPAD_RIGHT)) game.selectRewardChoice(1);
            else if(rawPressed & KEY_A) chooseReward();
            else if(rawPressed & KEY_TOUCH) {
                const int choiceIndex=Pokerogue3DS::RewardMenuPresenter::hitTest(touch.px,touch.py,game.rewardChoiceCount());
                if(choiceIndex>=0) {
                    if(game.selectedRewardChoice()==unsigned(choiceIndex)) chooseReward();
                    else {
                        const int delta=choiceIndex-int(game.selectedRewardChoice());
                        for(int steps=delta<0 ? -delta : delta;steps>0;--steps) game.selectRewardChoice(delta<0 ? -1 : 1);
                    }
                } else if(Pokerogue3DS::kRewardClaimButtonRect.contains(touch.px,touch.py)) chooseReward();
                else if(Pokerogue3DS::kRewardSkipButtonRect.contains(touch.px,touch.py)) game.skipVictoryReward();
            }
            changed=true;
            rawPressed=0; // Reward input cannot also trigger a battle/touch command.
        }
#if defined(POKEROGUE_ENABLE_QUICKJS)
        const bool jsCommands = bridgeReady && bridge.healthy();
        if (jsCommands) changed = bridge.processPendingAction() || changed;
        if (!jsCommands) {
#endif
        if (pressed & KEY_SELECT)
            changed = game.togglePlayerEvolutionPause(game.activePlayerPartyIndex()) || changed;
        if (!rewardInput && !game.rewardsPending()) {
            if (pressed & (KEY_LEFT | KEY_CPAD_LEFT)) changed = game.cycleStarter(-1);
            else if (pressed & (KEY_RIGHT | KEY_CPAD_RIGHT)) changed = game.cycleStarter(1);
            else if (pressed & (KEY_UP | KEY_CPAD_UP)) { game.selectBattleMove(-1); changed = true; }
            else if (pressed & (KEY_DOWN | KEY_CPAD_DOWN)) { game.selectBattleMove(1); changed = true; }
            else if (pressed & KEY_A) { game.advanceBattleTurn(); changed = true; }
            else if (pressed & KEY_B) { game.skipVictoryReward(); changed = true; }
        }
#if defined(POKEROGUE_ENABLE_QUICKJS)
        }
        // Native presenters own battle/setup/rewards; JS retains the remaining queued controls.
        // JS queues storage/game commands outside rendering.
#endif
        uint32_t hostStorageKeys = pressed & (KEY_X | KEY_Y | KEY_L | KEY_R);
#if defined(POKEROGUE_ENABLE_QUICKJS)
        if (jsCommands) hostStorageKeys = 0; // Bridge queues all SD actions outside rendering.
#endif
        if (hostStorageKeys) {
            using namespace Pokerogue3DS;
            NativeSaveResult result = NativeSaveResult::Ok;
            if (pressed & (KEY_X | KEY_Y)) {
                result = game.saveNativeProgress(progress);
                if (result == NativeSaveResult::Ok && (pressed & KEY_Y))
                    result = progress.exportBundle(saveStorage, PokerogueContent::kContentHash,
                        bundleScratch, sizeof(bundleScratch), profileStaging, PokerogueContent::kSpeciesCount);
            } else {
                if (pressed & KEY_R) {
                    size_t count = 0;
                    result = progress.readBundleCandidate(saveStorage, PokerogueContent::kContentHash,
                        bundleScratch, sizeof(bundleScratch), restored, profileStaging,
                        PokerogueContent::kSpeciesCount, count);
                    if (result == NativeSaveResult::Ok && !progressReplay().restoreNativeRunSave(
                            restored, profileStaging, count, &offlineFriendship))
                        result = NativeSaveResult::InvalidRecord;
                    if (result == NativeSaveResult::Ok)
                        result = progress.commitImported(restored, profileStaging, count);
                }
                if (result == NativeSaveResult::Ok) result = game.loadNativeProgress(saves, profiles,
                    profileStaging, PokerogueContent::kSpeciesCount, offlineFriendship, &restored);
            }
            game.setStorageFeedback(result == NativeSaveResult::Ok
                ? ((pressed & KEY_Y) ? "Exported progress.p3progress" : "Progress saved/restored")
                : nativeSaveResultName(result));
            changed = true;
        }
        if ((rawPressed & KEY_TOUCH) && battleMenuInput && battleMenu.movesOpen()) {
            touchPosition touch{};
            hidTouchRead(&touch);
            const int slot=Pokerogue3DS::pokemonMovePpExhausted(game.presentation().player.battleState)
                ? -1 : Pokerogue3DS::MoveMenuPresenter::hitTest(touch.px,touch.py);
            if (slot>=0 && slot<game.presentation().player.moveCount) {
                if (game.selectedBattleMove() == unsigned(slot)) {
                    changed=game.advanceBattleTurn();
                    battleMenu.reset();
                } else {
                    const int delta=slot-int(game.selectedBattleMove());
                    for (int steps=delta<0 ? -delta : delta; steps>0; --steps)
                        game.selectBattleMove(delta<0 ? -1 : 1);
                    changed=true;
                }
            } else if (game.doubleBattle()) {
                const int target=Pokerogue3DS::targetButtonAt(touch.px,touch.py);
                if (target>=0 && target!=game.selectedTarget())
                    changed=game.cycleTarget(target>game.selectedTarget() ? 1 : -1) || changed;
            } else if (touch.py >= 200) {
                battleMenu.reset();
            }
        }
        if((rawPressed & KEY_TOUCH) && game.capturePartyChoicePending()) {
            touchPosition touch{};hidTouchRead(&touch);
            const int choice=Pokerogue3DS::partyButtonAt(touch.px,touch.py,game.presentation().playerPartyCount);
            if(choice>=0) {
                if (game.selectedCapturePartyChoice() == unsigned(choice)) {
                    changed=game.resolveCapturePartyChoice(choice);
                } else {
                    for(unsigned i=0;i<6 && game.selectedCapturePartyChoice()!=unsigned(choice);++i) game.selectBattleMove(1);
                    changed=true;
                }
            } else if (touch.py >= 200 && touch.px > 160) {
                changed=game.resolveCapturePartyChoice(-1);
            }
        } else if((rawPressed & KEY_TOUCH) && game.moveLearningPending()) {
            touchPosition touch{};hidTouchRead(&touch);
            const int slot=Pokerogue3DS::moveButtonAt(touch.px,touch.py);
            if(slot>=0) {
                if (game.selectedBattleMove() == unsigned(slot)) {
                    changed=game.resolvePendingLearnMove(slot);
                } else {
                    for(unsigned i=0;i<4 && game.selectedBattleMove()!=unsigned(slot);++i) game.selectBattleMove(1);
                    changed=true;
                }
            } else if (touch.py >= 195 && touch.px > 160) {
                changed=game.resolvePendingLearnMove(-1);
            }
        } else if((rawPressed & KEY_TOUCH) && game.evolutionPending()) {
            touchPosition touch{};hidTouchRead(&touch);
            if (touch.py >= 130 && touch.py < 170) {
                if (touch.px < 160) changed=game.advanceBattleTurn();
                else changed=game.skipVictoryReward();
            }
        } else if((rawPressed & KEY_TOUCH) && game.battleFinished()) {
            touchPosition touch{};hidTouchRead(&touch);
            if (touch.py >= 130 && touch.py < 170) {
                if (game.playerWon()) changed=game.advanceBattleTurn();
                else {
                    const uint16_t starterDex = game.run().starterDex ? game.run().starterDex : 1;
                    if(game.restoreSetup(game.run().seed, starterDex)) {
                        titleVisible=true;
                        changed=true;
                    }
                }
            }
        }
        } // !isPaused
        if (changed) player.load(game.scene());
        if (!game.rewardsPending()) rewardMenu.resetSelection();
#if defined(POKEROGUE_ENABLE_QUICKJS)
        if (!isPaused && bridgeReady && bridge.healthy()) {
            // Presentation reads live state; checkpoint construction belongs to storage commands.
            const auto& state = game.presentation();
            bridge.setPokemonPresentation(state.player, state.enemy, frameAnimationTimeMs);
            const auto& actor = state.player.battleState;
            const auto& moveActor = game.moveLearningPending() ? game.progressionPokemon().battleState : actor;
            const auto& opponent = state.enemy.battleState;
            const uint8_t slot = game.selectedBattleMove();
            const auto* selected = slot < moveActor.moveCount && slot < 4
                ? PokerogueContent::findMoveById(moveActor.moves[slot].moveId) : nullptr;
            uint16_t moveIds[4]{};
            uint8_t movePp[4]{};
            for (uint8_t i = 0; i < moveActor.moveCount && i < 4; ++i) {
                moveIds[i] = moveActor.moves[i].moveId;
                movePp[i] = moveActor.moves[i].pp;
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
                unsigned(state.enemy.dex), unsigned(game.presentationStage()), unsigned(selected ? selected->id : 0),
                unsigned(selected ? moveActor.moves[slot].pp : 0),
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
        if (!isPaused && bridgeReady && bridge.healthy()) {
            bridge.tick(pressed);
#if defined(POKEROGUE_QUICKJS_DIAGNOSTIC_RENDER)
            renderer.endFrame();
            gspWaitForVBlank();
            continue;
#endif
        }
#endif
        renderer.beginTop();
        // Native presentation owns the top screen; do not draw diagnostic scene nodes.
        renderer.clear(0xff281f22);
        if (romfsReady && !game.rewardsPending()) {
            const uint64_t animationTimeMs = frameAnimationTimeMs;
            if (game.presentationStage()==Pokerogue3DS::NativeSaveStage::RunSetup) {
                setup.drawBackground(renderer);
                Pokerogue3DS::ResolvedPokemon selected{};
                selected.dex=game.selectedSetupStarterDex();
                const auto* form=PokerogueContent::findFormByUpstreamIndex(selected.dex,
                    game.setupStarterFormIndex(selected.dex));
                if (form) selected.formId=form->id;
                secondEnemySprites.draw(renderer,selected,false,22,55,112,112,animationTimeMs);
            } else {
            arena.draw(renderer, game.run().biomeId, animationTimeMs);
            // Player Pokémon anchored on player grass platform
            pokemonSprites.drawAnchored(renderer, game.presentation().player, true,
                105.0f, 185.0f, 0.0f, animationTimeMs);

            if (game.doubleBattle()) {
                pokemonSprites.drawAnchored(renderer, game.presentation().enemy, false,
                    230.0f, 82.0f, 0.0f, animationTimeMs);
                secondEnemySprites.drawAnchored(renderer, game.presentation().secondEnemy, false,
                    295.0f, 84.0f, 0.0f, animationTimeMs);
            } else {
                // Enemy Pokémon anchored directly on stone platform
                pokemonSprites.drawAnchored(renderer, game.presentation().enemy, false,
                    265.0f, 82.0f, 0.0f, animationTimeMs);
            }
            }
        }
        if (game.presentationStage() == Pokerogue3DS::NativeSaveStage::RunSetup) {
            setup.drawTop(renderer, game);
        } else if (game.rewardsPending()) {
            rewardMenu.drawTop(renderer, game);
        } else {
            // Enemy HUD in top-left
            battleHud.draw(renderer, game.presentation().enemy, false, 12.0f, 20.0f,
                game.hasCaughtSpecies(game.presentation().enemy.dex));
            if (game.doubleBattle()) {
                battleHud.draw(renderer, game.presentation().secondEnemy, false, 12.0f, 58.0f,
                    game.hasCaughtSpecies(game.presentation().secondEnemy.dex));
            }
            // Player HUD in bottom-right
            battleHud.draw(renderer, game.presentation().player, true, 226.0f, 155.0f);

            // Field / Biome info in top-right
            renderer.drawWindow(280.0f, 6.0f, 114.0f, 32.0f);
            char fieldLine[48];
            std::snprintf(fieldLine, sizeof(fieldLine), "%s - %u",
                game.presentation().biomeName ? game.presentation().biomeName : "Pradera",
                unsigned(game.run().wave));
            renderer.drawTextFitted(fieldLine, 286.0f, 11.0f, 0.28f, 102.0f, C2D_Color32(255, 255, 255, 255));

            // Battle dialogue banner on top screen only when outside active battle
            if (!game.battleFeedback().empty() && game.presentationStage() != Pokerogue3DS::NativeSaveStage::BattleActive) {
                renderer.drawWindow(16.0f, 194.0f, 368.0f, 40.0f);
                renderer.drawTextFitted(game.battleFeedback().c_str(), 28.0f, 204.0f, 0.38f, 344.0f,
                    C2D_Color32(255, 255, 255, 255));
            }
        }
        renderer.beginBottom();
        if (isPaused) {
            renderer.clear(0xff3a303d);
            renderer.drawText("PAUSA", 16, 8, 0.45f, 0xffffffff);
            renderer.drawWindow(24, 34, 272, 168);
            const char* pauseOptions[] = {
                "Continuar",
                "Guardar y salir",
                "Salir al menú principal"
            };
            for (unsigned i = 0; i < 3; ++i) {
                const float y = 52.0f + i * 36.0f;
                renderer.drawText(pauseOptions[i], 56.0f, y, 0.42f, 0xffffffff);
                if (pauseSelection == i) {
                    frontend.drawCursor(renderer, 36.0f, y, 0.42f);
                }
            }
            renderer.drawTextFitted(pauseFeedback ? pauseFeedback : "A: Seleccionar   B / START: Continuar", 16, 214, 0.30f, 288, 0xffffffff);
        } else if(game.capturePartyChoicePending() || game.moveLearningPending() || game.evolutionPending()) decisionMenu.draw(renderer,game);
        else if (game.rewardsPending()) rewardMenu.draw(renderer,game,rewardMenu.partySelectionMode());
        else if (partyMenu.open) partyMenu.draw(renderer,game);
        else if (game.presentationStage()==Pokerogue3DS::NativeSaveStage::RunSetup)
            setup.drawBottom(renderer,game);
        else if (Pokerogue3DS::MoveMenuPresenter::visible(game))
            battleMenu.draw(renderer,game);
        else decisionMenu.draw(renderer,game);
        renderer.endFrame();
        gspWaitForVBlank();
    }

    player.exit();
    decisionMenu.clear();
    rewardMenu.clear();
    battleMenu.clear();
    partyMenu.clear();
    frontend.clear();
    setup.clear();
    battleHud.clear();
    arena.clear();
    secondEnemySprites.invalidate();
    pokemonSprites.invalidate();
#if defined(POKEROGUE_ENABLE_QUICKJS)
    bridge.fini();
#endif
    renderer.fini();
    if (romfsReady) romfsExit();
    gfxExit();
    return 0;
}

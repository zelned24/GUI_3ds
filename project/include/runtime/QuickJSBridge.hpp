#pragma once
// Optional target: enable POKEROGUE_ENABLE_QUICKJS and provide pinned quickjs.h/library.
#include <quickjs.h>
#include "runtime/PokemonAtlasPresenter.hpp"
#include <cstddef>
#include <cstdint>
class Renderer2D;
namespace Pokerogue3DS {
class FirstRunRuntime;
class NativeRunSaveStore;
class NativeProgressStore;
class NativeProgressBundleStorage;
class NativeStarterCandyStore;
struct NativeStarterCandyRecord;
struct PokemonFriendshipPolicy;
class QuickJSBridge {
public:
    QuickJSBridge() = default;
    ~QuickJSBridge();
    QuickJSBridge(const QuickJSBridge&) = delete;
    QuickJSBridge& operator=(const QuickJSBridge&) = delete;
    bool init(Renderer2D& renderer);
    // Host supplies a trusted, bundled script; no implicit downloads or fake gameplay.
    // Script must define globalThis._3ds_tick(input). Evaluate outside rendering.
    bool evaluate(const char* source, std::size_t length, const char* sourceName);
    void tick(uint32_t keysDown);
    void fini();
    void setPokemonPresentation(const ResolvedPokemon& player, const ResolvedPokemon& enemy,
                                uint64_t animationTimeMs);
    static JSValue drawStarter(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue drawPokemon(JSContext*, JSValueConst, int, JSValueConst*);
    void bindRuntime(FirstRunRuntime& game) { m_game = &game; }
    void bindSaveStore(NativeRunSaveStore& saves) { m_saves = &saves; }
    void bindProgressStore(NativeProgressStore& progress, NativeStarterCandyStore& profiles,
        NativeStarterCandyRecord* staging, size_t capacity, const PokemonFriendshipPolicy& policy) {
        m_progress = &progress; m_profiles = &profiles; m_profileStaging = staging;
        m_profileCapacity = capacity; m_friendshipPolicy = &policy;
    }
    // Borrowed bounded transport/workspace and independent replay runtime.
    void bindProgressBundle(NativeProgressBundleStorage& storage, char* workspace, size_t capacity,
        FirstRunRuntime& replay) {
        m_bundleStorage = &storage; m_bundleWorkspace = workspace;
        m_bundleCapacity = capacity; m_progressReplay = &replay;
    }
    void setJournalGeneration(uint32_t generation) { m_journalGeneration = generation; }
    uint32_t journalGeneration() const { return m_journalGeneration; }
    uint16_t restartStarterDex() const;
    static JSValue resetRun(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue cycleStarterBinding(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue getPresentationInfo(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue getMoveName(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue getStarterName(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue cycleStarterForm(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue toggleStarterTeam(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue purchaseStarterCost(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue saveNative(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue loadNative(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue exportNative(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue importNative(JSContext*, JSValueConst, int, JSValueConst*);
    bool processPendingAction(); // Call before beginFrame; returns whether a command was attempted.
    static JSValue submitAction(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue skipReward(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue getCombatLog(JSContext*, JSValueConst, int, JSValueConst*);
    bool setBattleStateJson(const char* json, std::size_t length);
    static JSValue getBattleState(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue drawText(JSContext*, JSValueConst, int, JSValueConst*);
    bool healthy() const { return m_healthy; }
    const char* lastError() const { return m_error; }
private:
    JSRuntime* m_runtime = nullptr;
    JSContext* m_context = nullptr;
    Renderer2D* m_renderer = nullptr; // borrowed; owns GPU/resources independently
    JSValue m_input = JS_UNDEFINED;
    JSValue m_tick = JS_UNDEFINED;
    JSAtom m_keys[12]{};
    char* m_saveBuffer = nullptr;
    bool m_healthy = false;
    bool m_inTick = false;
    uint16_t m_screenWidth = 0;
    unsigned m_interruptBudget = 0;
    char m_error[192]{};
    char m_battleStateJson[2048]{};
    PokemonAtlasPresenter m_presenterPlayer;
    PokemonAtlasPresenter m_presenterEnemy;
    PokemonAtlasPresenter m_presenterSecondEnemy;
    const ResolvedPokemon* m_player = nullptr;
    const ResolvedPokemon* m_enemy = nullptr;
    uint64_t m_animationTimeMs = 0;
    FirstRunRuntime* m_game = nullptr; // borrowed, never deleted
    NativeRunSaveStore* m_saves = nullptr;
    NativeProgressStore* m_progress = nullptr;
    NativeStarterCandyStore* m_profiles = nullptr;
    NativeStarterCandyRecord* m_profileStaging = nullptr;
    size_t m_profileCapacity = 0;
    const PokemonFriendshipPolicy* m_friendshipPolicy = nullptr;
    NativeProgressBundleStorage* m_bundleStorage = nullptr;
    char* m_bundleWorkspace = nullptr;
    size_t m_bundleCapacity = 0;
    FirstRunRuntime* m_progressReplay = nullptr;
    uint16_t m_restartStarter = 0;
    uint32_t m_journalGeneration = 0;
    char m_actionFeedback[128]{};
    int m_pendingAction = -999; // 0..3 attack, -1/100 cursor, 200 reward
    void captureException();
    static int interrupt(JSRuntime*, void*);
    static JSValue beginTop(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue beginBottom(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue clear(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue drawImage(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue preload(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue saveGame(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue loadGame(JSContext*, JSValueConst, int, JSValueConst*);
};
}

// Kept optional until a pinned QuickJS ARM11 library is integrated into the build.
#if defined(POKEROGUE_ENABLE_QUICKJS)
#include "runtime/QuickJSBridge.hpp"
#include "runtime/RuntimeAssetManager.hpp"
#include "gfx/renderer2d.hpp"
#include "game/FirstRunRuntime.hpp"
#include "storage/NativeProgressStore.hpp"
#include <3ds.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <new>
#include <sys/stat.h>
#include <unistd.h>

namespace Pokerogue3DS {
namespace {
constexpr size_t kSaveLimit = 1024 * 1024;
constexpr const char* kSavePath = "sdmc:/3ds/pokerogue/saves/session.json";
constexpr const char* kPendingPath = "sdmc:/3ds/pokerogue/saves/session.pending.json";
const char* kNames[] = {"A", "B", "X", "Y", "L", "R", "up", "down", "left", "right", "start", "select"};
const uint32_t kMasks[] = {KEY_A, KEY_B, KEY_X, KEY_Y, KEY_L, KEY_R, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_START, KEY_SELECT};
bool directory(const char* path) {
    if (!mkdir(path, 0777)) return true;
    struct stat info{};
    return errno == EEXIST && !stat(path, &info) && S_ISDIR(info.st_mode);
}
bool number(JSContext* ctx, JSValueConst value, double& out) {
    return JS_ToFloat64(ctx, &out, value) == 0 && std::isfinite(out);
}
}
QuickJSBridge::~QuickJSBridge() { fini(); }
int QuickJSBridge::interrupt(JSRuntime*, void* opaque) {
    auto* bridge = static_cast<QuickJSBridge*>(opaque);
    if (!bridge->m_interruptBudget) return 1;
    --bridge->m_interruptBudget;
    return 0; // bounded engine checkpoints, not a hardware frame-time guarantee
}
void QuickJSBridge::captureException() {
    JSValue exception = JS_GetException(m_context);
    const char* text = JS_ToCString(m_context, exception);
    std::snprintf(m_error, sizeof(m_error), "%s", text ? text : "QuickJS exception");
    if (text) JS_FreeCString(m_context, text);
    JS_FreeValue(m_context, exception);
    m_healthy = false;
}
bool QuickJSBridge::init(Renderer2D& renderer) {
    fini();
    m_error[0] = 0;
    m_runtime = JS_NewRuntime();
    if (!m_runtime) return false;
    JS_SetMemoryLimit(m_runtime, 12 * 1024 * 1024);
    JS_SetMaxStackSize(m_runtime, 256 * 1024);
    JS_SetInterruptHandler(m_runtime, interrupt, this);
    m_context = JS_NewContext(m_runtime);
    if (!m_context) { fini(); return false; }
    m_renderer = &renderer;
    JS_SetContextOpaque(m_context, this);
    m_saveBuffer = new (std::nothrow) char[kSaveLimit + 1];
    if (!m_saveBuffer) { fini(); return false; }
    m_input = JS_NewObject(m_context);
    if (JS_IsException(m_input)) { captureException(); m_input = JS_UNDEFINED; fini(); return false; }
    for (unsigned i = 0; i < 12; ++i) {
        m_keys[i] = JS_NewAtom(m_context, kNames[i]);
        if (!m_keys[i] || JS_SetProperty(m_context, m_input, m_keys[i], JS_FALSE) < 0) {
            captureException(); fini(); return false;
        }
    }
    JSValue global = JS_GetGlobalObject(m_context);
    struct Binding { const char* name; JSCFunction* callback; int arguments; };
    const Binding bindings[] = {
        {"_3ds_beginTop", beginTop, 0}, {"_3ds_beginBottom", beginBottom, 0},
        {"_3ds_clear", clear, 1}, {"_3ds_drawImage", drawImage, 6},
        {"_3ds_resetRun", resetRun, 0}, {"_3ds_cycleStarter", cycleStarterBinding, 1},
        {"_3ds_getStarterName", getStarterName, 0}, {"_3ds_getMoveName", getMoveName, 1},
        {"_3ds_getPresentationInfo", getPresentationInfo, 0},
        {"_3ds_saveNative", saveNative, 0}, {"_3ds_loadNative", loadNative, 0},
        {"_3ds_exportNative", exportNative, 0}, {"_3ds_importNative", importNative, 0},
        {"_3ds_submitAction", submitAction, 1}, {"_3ds_skipReward", skipReward, 0},
        {"_3ds_getCombatLog", getCombatLog, 0},
        {"_3ds_drawPokemon", drawPokemon, 5},
        {"_3ds_getBattleState", getBattleState, 0}, {"_3ds_drawText", drawText, 5},
        {"_3ds_preload", preload, 1}, {"_3ds_saveGame", saveGame, 1}, {"_3ds_loadGame", loadGame, 0}
    };
    bool ok = true;
    for (const auto& binding : bindings) {
        JSValue function = JS_NewCFunction(m_context, binding.callback, binding.name, binding.arguments);
        if (JS_IsException(function)) { ok = false; break; }
        if (JS_SetPropertyStr(m_context, global, binding.name, function) < 0) { ok = false; break; }
    }
    JS_FreeValue(m_context, global);
    if (!ok) { captureException(); fini(); return false; }
    // No host time/unseeded RNG exposed to gameplay. The bundle supplies its pinned seeded RNG.
    const char* deterministicHost =
        "Object.defineProperty(Math, 'random', {value: function(){throw new Error('Seeded RNG required');}, writable:false, configurable:false});"
        "Object.defineProperty(globalThis, 'Date', {value: undefined, writable:false, configurable:false});";
    m_interruptBudget = 1000;
    JSValue setup = JS_Eval(m_context, deterministicHost, std::strlen(deterministicHost),
        "3ds-host-policy", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(setup)) { captureException(); fini(); return false; }
    JS_FreeValue(m_context, setup);
    m_healthy = true;
    return true;
}
bool QuickJSBridge::evaluate(const char* source, size_t length, const char* sourceName) {
    if (!m_healthy || m_inTick || !source || !sourceName) return false;
    m_interruptBudget = 10000;
    JSValue result = JS_Eval(m_context, source, length, sourceName, JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) { captureException(); return false; }
    JS_FreeValue(m_context, result);
    JSValue global = JS_GetGlobalObject(m_context);
    JSValue callback = JS_GetPropertyStr(m_context, global, "_3ds_tick");
    JS_FreeValue(m_context, global);
    if (JS_IsException(callback)) { captureException(); return false; }
    if (!JS_IsFunction(m_context, callback)) {
        JS_FreeValue(m_context, callback);
        std::snprintf(m_error, sizeof(m_error), "Bundle must define _3ds_tick(input)");
        return false;
    }
    JS_FreeValue(m_context, m_tick);
    m_tick = callback;
    return true;
}
void QuickJSBridge::tick(uint32_t keysDown) {
    if (!m_healthy || !JS_IsFunction(m_context, m_tick) || m_inTick) return;
    m_interruptBudget = 1000;
    // Existing object/atoms reused. QuickJS and JS code may allocate within their heap limit.
    for (unsigned i = 0; i < 12; ++i) {
        if (JS_SetProperty(m_context, m_input, m_keys[i], JS_NewBool(m_context, (keysDown & kMasks[i]) != 0)) < 0) {
            captureException(); return;
        }
    }
    m_inTick = true;
    m_screenWidth = 0;
    JSValue result = JS_Call(m_context, m_tick, JS_UNDEFINED, 1, &m_input);
    m_inTick = false;
    if (JS_IsException(result)) captureException();
    else JS_FreeValue(m_context, result);
    // Async jobs intentionally not executed: bundle contract is synchronous per frame.
}
bool QuickJSBridge::setBattleStateJson(const char* json, size_t length) {
    // Never truncate a JSON record into an invalid snapshot; preserve the last valid one.
    if (!json || length >= sizeof(m_battleStateJson) || std::memchr(json, 0, length)) return false;
    std::memcpy(m_battleStateJson, json, length);
    m_battleStateJson[length] = 0;
    return true;
}
JSValue QuickJSBridge::getBattleState(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    auto* bridge = static_cast<QuickJSBridge*>(JS_GetContextOpaque(ctx));
    if (!bridge) return JS_ThrowInternalError(ctx, "Bridge unavailable");
    return JS_NewString(ctx, bridge->m_battleStateJson[0] ? bridge->m_battleStateJson : "{}");
}
JSValue QuickJSBridge::drawText(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* bridge = static_cast<QuickJSBridge*>(JS_GetContextOpaque(ctx));
    if (!bridge || !bridge->m_inTick || !bridge->m_screenWidth || argc != 5 || !JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "drawText requires active screen and five arguments");
    double x, y, scale; uint32_t color;
    if (!number(ctx, argv[1], x) || !number(ctx, argv[2], y) || !number(ctx, argv[3], scale))
        return JS_ThrowTypeError(ctx, "Text coordinates and scale must be finite");
    if (JS_ToUint32(ctx, &color, argv[4]) < 0) return JS_EXCEPTION;
    if (x < 0 || x >= bridge->m_screenWidth || y < 0 || y >= 240 || scale <= 0 || scale > 2)
        return JS_ThrowRangeError(ctx, "Text outside screen contract");
    size_t length;
    const char* text = JS_ToCStringLen(ctx, &length, argv[0]);
    if (!text) return JS_EXCEPTION;
    if (length > 512) { JS_FreeCString(ctx, text); return JS_ThrowRangeError(ctx, "Text exceeds HUD line limit"); }
    bridge->m_renderer->drawText(text, static_cast<float>(x), static_cast<float>(y),
        static_cast<float>(scale), color);
    JS_FreeCString(ctx, text);
    return JS_UNDEFINED;
}
void QuickJSBridge::setPokemonPresentation(const ResolvedPokemon& player,
    const ResolvedPokemon& enemy, uint64_t animationTimeMs) {
    // Borrow canonical resolved values owned by FirstRunRuntime; main outlives each tick.
    m_player = &player;
    m_enemy = &enemy;
    m_animationTimeMs = animationTimeMs; // Visual time only, never game/RNG state.
}
JSValue QuickJSBridge::drawPokemon(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* bridge = static_cast<QuickJSBridge*>(JS_GetContextOpaque(ctx));
    if (!bridge || !bridge->m_inTick || bridge->m_screenWidth != 400 || (argc != 5 && argc != 6))
        return JS_ThrowTypeError(ctx, "drawPokemon requires top screen and five or six arguments");
    double dexNumber, x, y, scale;
    if (!number(ctx, argv[0], dexNumber) || !number(ctx, argv[2], x) ||
        !number(ctx, argv[3], y) || !number(ctx, argv[4], scale))
        return JS_ThrowTypeError(ctx, "Finite species/coordinates/scale required");
    if (dexNumber < 1 || dexNumber > 65535 || std::floor(dexNumber) != dexNumber ||
        x < 0 || x >= 400 || y < 0 || y >= 240 || scale <= 0 || scale > 2)
        return JS_ThrowRangeError(ctx, "Invalid Pokemon draw parameters");
    const int back = JS_ToBool(ctx, argv[1]);
    if (back < 0) return JS_EXCEPTION;
    bool second = false;
    if (argc == 6) {
        double slot;
        if (!number(ctx, argv[5], slot) || slot != 2 || back || !bridge->m_game || !bridge->m_game->doubleBattle())
            return JS_ThrowRangeError(ctx, "Sixth argument selects active second enemy slot 2 only");
        second = true;
    }
    const auto* pokemon = second ? &bridge->m_game->presentation().secondEnemy
        : back ? bridge->m_player : bridge->m_enemy;
    if (!pokemon || pokemon->dex != static_cast<uint16_t>(dexNumber) ||
        !PokerogueContent::findSpeciesByDex(pokemon->dex))
        return JS_ThrowRangeError(ctx, "Sprite must reference the resolved active actor");
    auto& presenter = second ? bridge->m_presenterSecondEnemy
        : back ? bridge->m_presenterPlayer : bridge->m_presenterEnemy;
    // Presenter owns metadata/cache/pages. It loads on key/page changes, not every frame.
    // Internal filesystem/texture operations may allocate; callback has no explicit new/malloc.
    presenter.draw(*bridge->m_renderer, *pokemon, back, static_cast<float>(x),
        static_cast<float>(y), static_cast<float>(96 * scale), static_cast<float>(96 * scale),
        bridge->m_animationTimeMs);
    return JS_UNDEFINED;
}
JSValue QuickJSBridge::submitAction(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* bridge = static_cast<QuickJSBridge*>(JS_GetContextOpaque(ctx));
    if (!bridge || !bridge->m_game || !bridge->m_inTick || argc != 1)
        return JS_ThrowTypeError(ctx, "submitAction requires runtime and one command");
    double value;
    if (!number(ctx, argv[0], value) || std::floor(value) != value ||
        !((value >= 0 && value <= 3) || value == -1 || value == 100 || (value >= 210 && value <= 216) || (value >= 220 && value <= 225)))
        return JS_ThrowRangeError(ctx, "Use slots 0..3, cursor -1/+100, capture 210, party 211..216 or evolution pause 220..225");
    if (bridge->m_pendingAction != -999) return JS_FALSE;
    if (value >= 0 && value <= 3 && !bridge->m_game->battleFinished() &&
        value >= bridge->m_game->presentation().player.battleState.moveCount) return JS_FALSE;
    bridge->m_pendingAction = static_cast<int>(value);
    return JS_TRUE; // Accepted for processing before the next frame, not a claimed successful turn.
}
JSValue QuickJSBridge::skipReward(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    auto* bridge = static_cast<QuickJSBridge*>(JS_GetContextOpaque(ctx));
    if (!bridge || !bridge->m_game || !bridge->m_inTick || argc != 0)
        return JS_ThrowTypeError(ctx, "skipReward requires bound runtime");
    if (bridge->m_pendingAction != -999 || !bridge->m_game->battleFinished() ||
        !bridge->m_game->playerWon() || !bridge->m_game->experienceGranted()) return JS_FALSE;
    bridge->m_pendingAction = 200;
    return JS_TRUE;
}
JSValue QuickJSBridge::getCombatLog(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    auto* bridge = static_cast<QuickJSBridge*>(JS_GetContextOpaque(ctx));
    return JS_NewString(ctx, bridge && bridge->m_actionFeedback[0] ? bridge->m_actionFeedback :
        bridge && bridge->m_game ? bridge->m_game->battleFeedback().c_str() : "");
}
uint16_t QuickJSBridge::restartStarterDex() const {
    return m_restartStarter ? m_restartStarter : m_game ? m_game->run().starterDex : 0;
}
JSValue QuickJSBridge::resetRun(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    auto* b = static_cast<QuickJSBridge*>(JS_GetContextOpaque(ctx));
    if (!b || !b->m_game || !b->m_inTick || argc != 0) return JS_FALSE;
    if (b->m_pendingAction != -999 || !b->m_game->battleFinished() || b->m_game->playerWon()) return JS_FALSE;
    b->m_pendingAction = 201; return JS_TRUE;
}
JSValue QuickJSBridge::cycleStarterBinding(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* b = static_cast<QuickJSBridge*>(JS_GetContextOpaque(ctx));
    double direction;
    if (!b || !b->m_game || !b->m_inTick || argc != 1) return JS_FALSE;
    if (!number(ctx, argv[0], direction) || (direction != -1 && direction != 1))
        return JS_ThrowRangeError(ctx, "Starter direction must be -1 or +1");
    if (b->m_pendingAction != -999) return JS_FALSE;
    if (b->m_game->runStarted() && !(b->m_game->battleFinished() && !b->m_game->playerWon()) &&
        !(b->m_game->doubleBattle() && !b->m_game->battleFinished())) return JS_FALSE;
    b->m_pendingAction = direction < 0 ? 202 : 203; return JS_TRUE;
}
JSValue QuickJSBridge::getPresentationInfo(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    auto* b = static_cast<QuickJSBridge*>(JS_GetContextOpaque(ctx));
    if (!b || !b->m_game) return JS_NULL;
    const auto& view = b->m_game->presentation();
    // Presentation labels follow PokemonEffectiveWeather / pinned WeatherType enum order.
    static const char* weatherNames[] = {"NONE", "SUN", "RAIN", "SANDSTORM", "HAIL",
        "SNOW", "FOG", "HEAVY RAIN", "HARSH SUN", "STRONG WINDS"};
    const unsigned weather = static_cast<unsigned>(b->m_game->arenaWeather().type);
    JSValue info = JS_NewObject(ctx);
    if (JS_IsException(info)) return JS_EXCEPTION;
    auto set = [&](const char* key, JSValue value) {
        if (JS_IsException(value)) return false;
        return JS_SetPropertyStr(ctx, info, key, value) >= 0;
    };
    if (!set("classicClearPending", JS_NewBool(ctx, b->m_game->battleFinished() && b->m_game->playerWon() &&
            b->m_game->experienceGranted() && b->m_game->run().wave == PokerogueContent::kClassicFinalWave &&
            b->m_game->victoryPlan().completedWave == b->m_game->run().wave &&
            b->m_game->victoryPlan().contains(ClassicVictoryStep::GameClear))) ||
        !set("playerName", JS_NewString(ctx, view.player.localizedName ? view.player.localizedName : "")) ||
        !set("enemyName", JS_NewString(ctx, view.enemy.localizedName ? view.enemy.localizedName : "")) ||
        !set("secondEnemyName", JS_NewString(ctx, view.secondEnemy.localizedName ? view.secondEnemy.localizedName : "")) ||
        !set("secondEnemyHp", JS_NewUint32(ctx, view.secondEnemy.battleState.hp)) ||
        !set("secondEnemyMaxHp", JS_NewUint32(ctx, view.secondEnemy.battleState.maxHp)) ||
        !set("trainerTypeId", JS_NewUint32(ctx, view.trainerTypeId)) ||
        !set("trainerName", JS_NewString(ctx, view.trainerName ? view.trainerName : "")) ||
        !set("trainerPartyCount", JS_NewUint32(ctx, view.trainerPartyCount)) ||
        !set("doubleBattle", JS_NewBool(ctx, b->m_game->doubleBattle())) ||
        !set("selectedTarget", JS_NewUint32(ctx, b->m_game->selectedTarget())) ||
        !set("weatherType", JS_NewUint32(ctx, weather)) ||
        !set("weatherName", JS_NewString(ctx, weather < 10 ? weatherNames[weather] : "UNSUPPORTED")) ||
        !set("secondEnemyDex", JS_NewUint32(ctx, b->m_game->doubleBattle() ? view.secondEnemy.dex : 0)) ||
        !set("activeTrainerMember", JS_NewUint32(ctx, view.activeTrainerPartyIndex)) ||
        !set("biomeId", JS_NewString(ctx, b->m_game->run().biomeId ? b->m_game->run().biomeId : "")) ||
        !set("biomeName", JS_NewString(ctx, view.biomeName ? view.biomeName : "")) ||
        !set("playerPartyCount", JS_NewUint32(ctx, b->m_game->playerPartyCount())) ||
        !set("activePlayerPartyIndex", JS_NewUint32(ctx, b->m_game->activePlayerPartyIndex()))) {
        JS_FreeValue(ctx, info); return JS_EXCEPTION;
    }
    JSValue playerParty = JS_NewArray(ctx);
    if (JS_IsException(playerParty)) { JS_FreeValue(ctx, info); return JS_EXCEPTION; }
    for (unsigned i = 0; i < view.playerPartyCount && i < 6; ++i) {
        if (JS_SetPropertyUint32(ctx, playerParty, i, JS_NewUint32(ctx, view.playerParty[i].dex)) < 0) {
            JS_FreeValue(ctx, playerParty); JS_FreeValue(ctx, info); return JS_EXCEPTION;
        }
    }
    if (!set("playerParty", playerParty)) { JS_FreeValue(ctx, info); return JS_EXCEPTION; }
    JSValue pokeballs = JS_NewArray(ctx);
    if (JS_IsException(pokeballs)) { JS_FreeValue(ctx, info); return JS_EXCEPTION; }
    for (unsigned i = 0; i < 6; ++i) {
        if (JS_SetPropertyUint32(ctx, pokeballs, i, JS_NewUint32(ctx, b->m_game->pokeballCount(static_cast<PokeballType>(i)))) < 0) {
            JS_FreeValue(ctx, pokeballs); JS_FreeValue(ctx, info); return JS_EXCEPTION;
        }
    }
    if (!set("pokeballs", pokeballs)) { JS_FreeValue(ctx, info); return JS_EXCEPTION; }
    JSValue party = JS_NewArray(ctx);
    if (JS_IsException(party)) { JS_FreeValue(ctx, info); return JS_EXCEPTION; }
    for (unsigned i = 0; i < view.trainerPartyCount && i < 6; ++i) {
        if (JS_SetPropertyUint32(ctx, party, i, JS_NewUint32(ctx, view.trainerParty[i].dex)) < 0) {
            JS_FreeValue(ctx, party); JS_FreeValue(ctx, info); return JS_EXCEPTION;
        }
    }
    if (!set("trainerParty", party)) { JS_FreeValue(ctx, info); return JS_EXCEPTION; }
    return info;
}
JSValue QuickJSBridge::getMoveName(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    double id;
    if (argc != 1 || !number(ctx, argv[0], id) || id < 0 || id > 65535 || std::floor(id) != id)
        return JS_ThrowRangeError(ctx, "Move ID must be a canonical integer");
    const auto* move = PokerogueContent::findMoveById(static_cast<uint16_t>(id));
    return JS_NewString(ctx, move && move->name ? move->name : "---");
}
JSValue QuickJSBridge::getStarterName(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    auto* b = static_cast<QuickJSBridge*>(JS_GetContextOpaque(ctx));
    const auto* species = b ? PokerogueContent::findSpeciesByDex(b->restartStarterDex()) : nullptr;
    return JS_NewString(ctx, species ? species->name : "?");
}
JSValue QuickJSBridge::saveNative(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    auto* b = static_cast<QuickJSBridge*>(JS_GetContextOpaque(ctx));
    if (!b || !b->m_game || !b->m_saves || !b->m_inTick || argc || b->m_pendingAction != -999) return JS_FALSE;
    b->m_pendingAction = 204; return JS_TRUE;
}
JSValue QuickJSBridge::loadNative(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    auto* b = static_cast<QuickJSBridge*>(JS_GetContextOpaque(ctx));
    if (!b || !b->m_game || !b->m_saves || !b->m_inTick || argc || b->m_pendingAction != -999) return JS_FALSE;
    b->m_pendingAction = 205; return JS_TRUE;
}
JSValue QuickJSBridge::exportNative(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    auto* b = static_cast<QuickJSBridge*>(JS_GetContextOpaque(ctx));
    if (!b || !b->m_game || !b->m_progress || !b->m_bundleStorage || !b->m_bundleWorkspace ||
        !b->m_profileStaging || !b->m_inTick || argc || b->m_pendingAction != -999) return JS_FALSE;
    b->m_pendingAction = 206; return JS_TRUE;
}
JSValue QuickJSBridge::importNative(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    auto* b = static_cast<QuickJSBridge*>(JS_GetContextOpaque(ctx));
    if (!b || !b->m_game || !b->m_progress || !b->m_saves || !b->m_profiles ||
        !b->m_friendshipPolicy || !b->m_bundleStorage || !b->m_bundleWorkspace ||
        !b->m_profileStaging || !b->m_progressReplay || b->m_progressReplay == b->m_game ||
        !b->m_inTick || argc || b->m_pendingAction != -999) return JS_FALSE;
    b->m_pendingAction = 207; return JS_TRUE;
}
bool QuickJSBridge::processPendingAction() {
    const int action = m_pendingAction;
    m_pendingAction = -999;
    if (!m_game || !m_healthy || m_inTick || action == -999) return false;
    m_actionFeedback[0] = 0;
    if (action == 201) {
        auto* stream = m_game->battleRng().currentStream();
        const auto* species = PokerogueContent::findSpeciesByDex(restartStarterDex());
        bool ok = false;
        if (stream && species && species->freshProfileStarter) {
            auto next = *stream;
            uint32_t seed = 0;
            for (unsigned i = 0; i < 8 && !seed; ++i) seed = next.randSeedUint32();
            if (seed) ok = m_game->restoreSetup(seed, species->dex);
        }
        if (ok) { m_restartStarter = 0; m_presenterPlayer.invalidate(); m_presenterEnemy.invalidate(); m_presenterSecondEnemy.invalidate(); }
        std::snprintf(m_actionFeedback, sizeof(m_actionFeedback), "%s", ok ? "New run ready" : "Restart failed: invalid starter/RNG");
    } else if (action == 202 || action == 203) {
        if (!m_game->runStarted()) m_game->cycleStarter(action == 202 ? -1 : 1);
        else if (m_game->doubleBattle() && !m_game->battleFinished()) {
            m_game->cycleTarget(action == 202 ? -1 : 1);
        }
        else if (m_game->battleFinished() && !m_game->playerWon()) {
            size_t index = 0;
            for (; index < PokerogueContent::kSpeciesCount; ++index)
                if (PokerogueContent::kSpecies[index].dex == restartStarterDex()) break;
            if (index < PokerogueContent::kSpeciesCount) {
                for (size_t scanned = 0; scanned < PokerogueContent::kSpeciesCount; ++scanned) {
                    index = action == 202 ? (index + PokerogueContent::kSpeciesCount - 1) % PokerogueContent::kSpeciesCount
                        : (index + 1) % PokerogueContent::kSpeciesCount;
                    if (PokerogueContent::kSpecies[index].freshProfileStarter) {
                        m_restartStarter = PokerogueContent::kSpecies[index].dex; break;
                    }
                }
            }
        }
    } else if (action == 206 || action == 207) {
        NativeSaveResult status = NativeSaveResult::InvalidRecord;
        NativeRunSave save{};
        if (m_progress && m_bundleStorage && m_bundleWorkspace && m_profileStaging) {
            if (action == 206) {
                status = m_game->saveNativeProgress(*m_progress);
                if (status == NativeSaveResult::Ok)
                    status = m_progress->exportBundle(*m_bundleStorage, PokerogueContent::kContentHash,
                        m_bundleWorkspace, m_bundleCapacity, m_profileStaging, m_profileCapacity);
            } else if (m_saves && m_profiles && m_friendshipPolicy && m_progressReplay && m_progressReplay != m_game) {
                size_t count = 0;
                status = m_progress->readBundleCandidate(*m_bundleStorage, PokerogueContent::kContentHash,
                    m_bundleWorkspace, m_bundleCapacity, save, m_profileStaging, m_profileCapacity, count);
                if (status == NativeSaveResult::Ok && !m_progressReplay->restoreNativeRunSave(
                        save, m_profileStaging, count, m_friendshipPolicy))
                    status = NativeSaveResult::InvalidRecord;
                if (status == NativeSaveResult::Ok)
                    status = m_progress->commitImported(save, m_profileStaging, count);
                if (status == NativeSaveResult::Ok)
                    status = m_game->loadNativeProgress(*m_saves, *m_profiles, m_profileStaging,
                        m_profileCapacity, *m_friendshipPolicy, &save);
                if (status == NativeSaveResult::Ok) {
                    m_restartStarter = 0;
                    m_presenterPlayer.invalidate(); m_presenterEnemy.invalidate(); m_presenterSecondEnemy.invalidate();
                }
            }
            // Export saves first: refresh journal metadata even if the export failed.
            if (m_saves && m_saves->load(PokerogueContent::kContentHash, save) == NativeSaveResult::Ok)
                m_journalGeneration = save.generation;
        }
        std::snprintf(m_actionFeedback, sizeof(m_actionFeedback), "%s: %s",
            action == 206 ? "Export" : "Import", nativeSaveResultName(status));
    } else if (action == 204 || action == 205) {
        NativeSaveResult status = NativeSaveResult::NotFound;
        NativeRunSave save{};
        if (m_saves) {
            if (action == 204) {
                m_game->captureNativeRunSave(save);
                status = validateNativeRunSave(save, PokerogueContent::kContentHash);
                if (status == NativeSaveResult::Ok)
                    status = m_progress ? m_game->saveNativeProgress(*m_progress) : m_saves->save(save);
                if (status == NativeSaveResult::Ok) {
                    NativeRunSave stored{};
                    if (m_saves->load(PokerogueContent::kContentHash, stored) == NativeSaveResult::Ok)
                        m_journalGeneration = stored.generation;
                }
            } else {
                if (m_progress && m_profiles && m_friendshipPolicy)
                    status = m_game->loadNativeProgress(*m_saves, *m_profiles, m_profileStaging,
                        m_profileCapacity, *m_friendshipPolicy, &save);
                else {
                    status = m_saves->load(PokerogueContent::kContentHash, save);
                    if (status == NativeSaveResult::Ok && !m_game->restoreNativeRunSave(save))
                        status = NativeSaveResult::InvalidRecord;
                }
                if (status == NativeSaveResult::Ok) { m_journalGeneration = save.generation; m_restartStarter = 0; }
            }
        }
        std::snprintf(m_actionFeedback, sizeof(m_actionFeedback), "%s: %s", action == 204 ? "Save" : "Load", nativeSaveResultName(status));
    } else if (action == -1 || action == 100) m_game->selectBattleMove(action == -1 ? -1 : 1);
    else if (action == 200) {
        const bool skipped = m_game->skipVictoryReward();
        std::snprintf(m_actionFeedback, sizeof(m_actionFeedback), "%s", skipped
            ? "Reward skipped; no item granted" : "Reward transition blocked");
    }
    else if (action >= 0 && action <= 3) {
        if (m_game->moveLearningPending()) return m_game->resolvePendingLearnMove(action);
        if (!m_game->battleFinished()) {
            const auto count = m_game->presentation().player.battleState.moveCount;
            if (action >= count) return false;
            for (unsigned i = 0; i < 4 && m_game->selectedBattleMove() != action; ++i)
                m_game->selectBattleMove(1);
            if (m_game->selectedBattleMove() != action) return false;
        }
        // Also advances the engine's post-victory EXP/replacement phase; at most once.
        m_game->advanceBattleTurn();
    } else if (action == 210) {
        m_game->throwPokeball(PokeballType::Pokeball);
    } else if (action >= 211 && action <= 216) {
        m_game->switchPlayerPokemon(static_cast<uint8_t>(action - 211));
    } else if (action >= 220 && action <= 225) {
        return m_game->togglePlayerEvolutionPause(static_cast<uint8_t>(action - 220));
    }
    return true;
}
void QuickJSBridge::fini() {
    m_pendingAction = -999;
    m_game = nullptr;
    m_saves = nullptr;
    m_progress = nullptr;
    m_profiles = nullptr;
    m_profileStaging = nullptr;
    m_profileCapacity = 0;
    m_friendshipPolicy = nullptr;
    m_bundleStorage = nullptr;
    m_bundleWorkspace = nullptr;
    m_bundleCapacity = 0;
    m_progressReplay = nullptr;
    m_restartStarter = 0;
    m_journalGeneration = 0;
    m_actionFeedback[0] = 0;
    m_presenterPlayer.invalidate();
    m_presenterEnemy.invalidate();
    m_presenterSecondEnemy.invalidate();
    m_player = m_enemy = nullptr;
    m_animationTimeMs = 0;
    if (m_context) {
        JS_FreeValue(m_context, m_tick);
        JS_FreeValue(m_context, m_input);
        for (auto atom : m_keys) if (atom) JS_FreeAtom(m_context, atom);
        JS_SetContextOpaque(m_context, nullptr);
        JS_FreeContext(m_context);
    }
    m_context = nullptr;
    if (m_runtime) JS_FreeRuntime(m_runtime);
    m_runtime = nullptr;
    delete[] m_saveBuffer;
    m_saveBuffer = nullptr;
    m_renderer = nullptr;
    m_input = m_tick = JS_UNDEFINED;
    for (auto& atom : m_keys) atom = 0;
    m_inTick = m_healthy = false;
    // Renderer2D owns RuntimeAssetManager + C2D/C3D teardown. Never free shared GPU here.
}
JSValue QuickJSBridge::beginTop(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    auto* b = static_cast<QuickJSBridge*>(JS_GetContextOpaque(ctx));
    if (!b || !b->m_inTick) return JS_ThrowInternalError(ctx, "Rendering requires an active frame tick");
    b->m_renderer->beginTop(); b->m_screenWidth = 400; return JS_UNDEFINED;
}
JSValue QuickJSBridge::beginBottom(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    auto* b = static_cast<QuickJSBridge*>(JS_GetContextOpaque(ctx));
    if (!b || !b->m_inTick) return JS_ThrowInternalError(ctx, "Rendering requires an active frame tick");
    b->m_renderer->beginBottom(); b->m_screenWidth = 320; return JS_UNDEFINED;
}
JSValue QuickJSBridge::clear(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* b = static_cast<QuickJSBridge*>(JS_GetContextOpaque(ctx)); uint32_t color;
    if (!b || !b->m_inTick || !b->m_screenWidth || argc != 1) return JS_ThrowTypeError(ctx, "clear requires screen and color");
    if (JS_ToUint32(ctx, &color, argv[0]) < 0) return JS_EXCEPTION;
    b->m_renderer->clear(color); return JS_UNDEFINED;
}
JSValue QuickJSBridge::preload(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* b = static_cast<QuickJSBridge*>(JS_GetContextOpaque(ctx));
    if (!b || b->m_inTick || argc != 1) return JS_ThrowTypeError(ctx, "Preload assets outside tick");
    const char* id = JS_ToCString(ctx, argv[0]); if (!id) return JS_EXCEPTION;
    const bool ok = Citro2D::getRuntimeAssetManager().preload(id);
    JS_FreeCString(ctx, id); return JS_NewBool(ctx, ok);
}
JSValue QuickJSBridge::drawImage(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* b = static_cast<QuickJSBridge*>(JS_GetContextOpaque(ctx));
    if (!b || !b->m_inTick || !b->m_screenWidth || argc != 6) return JS_ThrowTypeError(ctx, "drawImage requires screen and six arguments");
    double v[5];
    for (unsigned i = 0; i < 5; ++i) if (!number(ctx, argv[i + 1], v[i])) return JS_ThrowTypeError(ctx, "Finite coordinates/scale/angle required");
    if (v[0] < 0 || v[0] >= b->m_screenWidth || v[1] < 0 || v[1] >= 240 || v[2] <= 0 || v[2] > 16 || v[3] <= 0 || v[3] > 16)
        return JS_ThrowRangeError(ctx, "Draw origin/scale outside screen contract");
    const char* id = JS_ToCString(ctx, argv[0]); if (!id) return JS_EXCEPTION;
    const auto* asset = Citro2D::getRuntimeAssetManager().get(id);
    JS_FreeCString(ctx, id);
    if (!asset || !asset->loaded) return JS_ThrowInternalError(ctx, "Asset must be preloaded using its manifest ID");
    // Real API uses width/height, rotation, opacity, flips; the pasted signature was inaccurate.
    b->m_renderer->drawImageDirect(asset->image, v[0], v[1], asset->width * v[2], asset->height * v[3], v[4]);
    return JS_UNDEFINED;
}
JSValue QuickJSBridge::saveGame(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0])) return JS_ThrowTypeError(ctx, "saveGame requires JSON text");
    size_t length = 0;
    const char* bytes = JS_ToCStringLen(ctx, &length, argv[0]); if (!bytes) return JS_EXCEPTION;
    if (length > kSaveLimit) { JS_FreeCString(ctx, bytes); return JS_ThrowRangeError(ctx, "Session exceeds 1 MiB"); }
    JSValue parsed = JS_ParseJSON(ctx, bytes, length, "session.json");
    if (JS_IsException(parsed)) { JS_FreeCString(ctx, bytes); return JS_EXCEPTION; }
    JS_FreeValue(ctx, parsed);
    bool ok = directory("sdmc:/3ds") && directory("sdmc:/3ds/pokerogue") && directory("sdmc:/3ds/pokerogue/saves");
    FILE* file = ok ? std::fopen(kPendingPath, "wb") : nullptr;
    if (file) {
        ok = std::fwrite(bytes, 1, length, file) == length;
        if (std::fflush(file) || fsync(fileno(file))) ok = false;
        if (std::fclose(file)) ok = false;
        if (ok) ok = std::rename(kPendingPath, kSavePath) == 0;
    } else ok = false;
    JS_FreeCString(ctx, bytes);
    if (!ok) return JS_ThrowInternalError(ctx, "Session write failed; prior session retained");
    return JS_TRUE;
}
JSValue QuickJSBridge::loadGame(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    auto* b = static_cast<QuickJSBridge*>(JS_GetContextOpaque(ctx));
    if (!b || !b->m_saveBuffer) return JS_ThrowInternalError(ctx, "Bridge not initialized");
    FILE* file = std::fopen(kSavePath, "rb");
    if (!file) return errno == ENOENT ? JS_NULL : JS_ThrowInternalError(ctx, "Session read failed");
    const size_t size = std::fread(b->m_saveBuffer, 1, kSaveLimit, file);
    const int extra = std::fgetc(file);
    bool ok = !std::ferror(file) && extra == EOF;
    if (std::fclose(file)) ok = false;
    if (!ok) return JS_ThrowInternalError(ctx, "Session unreadable or exceeds limit");
    b->m_saveBuffer[size] = 0;
    JSValue parsed = JS_ParseJSON(ctx, b->m_saveBuffer, size, "session.json");
    if (JS_IsException(parsed)) return JS_EXCEPTION;
    JS_FreeValue(ctx, parsed);
    return JS_NewStringLen(ctx, b->m_saveBuffer, size);
}
}
#endif

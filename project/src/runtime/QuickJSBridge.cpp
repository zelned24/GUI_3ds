// Kept optional until a pinned QuickJS ARM11 library is integrated into the build.
#if defined(POKEROGUE_ENABLE_QUICKJS)
#include "runtime/QuickJSBridge.hpp"
#include "runtime/RuntimeAssetManager.hpp"
#include "gfx/renderer2d.hpp"
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
void QuickJSBridge::fini() {
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

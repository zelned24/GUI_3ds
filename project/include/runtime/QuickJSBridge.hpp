#pragma once
// Optional target: enable POKEROGUE_ENABLE_QUICKJS and provide pinned quickjs.h/library.
#include <quickjs.h>
#include <cstddef>
#include <cstdint>
class Renderer2D;
namespace Pokerogue3DS {
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

# Fase 1: Auditoría de `GUI_3ds`, build de PokéRogue, fusión y emulación

1. **Preparar el entorno de compilación**

   ```bash
   sudo apt-get update
   sudo apt-get install -y git make cmake python3 nodejs npm \
       devkitpro-pacman

   sudo dkp-pacman -Syu
   sudo dkp-pacman -S 3ds-dev 3ds-cmake citro2d citro3d
   ```

   Verificar que las variables del entorno estén disponibles:

   ```bash
   echo "$DEVKITPRO"
   echo "$DEVKITARM"
   arm-none-eabi-gcc --version
   make --version
   node --version
   npm --version
   ```

   Fijar versiones reproducibles:

   ```bash
   node --version > toolchain-version.txt
   npm --version >> toolchain-version.txt
   git --version >> toolchain-version.txt
   ```

   > **Warning:** El rendimiento y la memoria disponible de Old 3DS son muy inferiores a los de New 3DS. El proyecto debe compilar inicialmente con un perfil conservador para Old 3DS, aunque el destino final sea New 3DS.

2. **Clonar ambos repositorios en un árbol de trabajo común**

   ```bash
   mkdir -p ~/work/pokerogue-3ds
   cd ~/work/pokerogue-3ds

   git clone https://github.com/zelned24/GUI_3ds.git native
   git clone <URL_OFICIAL_DEL_REPOSITORIO_POKEROGUE> pokerogue
   ```

   Registrar las revisiones exactas:

   ```bash
   cd native
   git rev-parse HEAD > ../native-revision.txt

   cd ../pokerogue
   git rev-parse HEAD > ../pokerogue-revision.txt
   ```

   No debe utilizarse una copia modificada o un mirror no verificable del repositorio original. Antes de empaquetar el resultado, comprobar las licencias del código, fuentes, música, sprites y demás assets.

3. **Auditar la estructura de `GUI_3ds`**

   ```bash
   cd ~/work/pokerogue-3ds/native

   find . -maxdepth 3 -type f | sort
   grep -RInE 'C2D_|C3D_|gfxInit|aptMainLoop|main\(' .
   grep -RInE 'romfs|sdmc|httpc|Makefile|CMakeLists' .
   ```

   Identificar, como mínimo:

   1. Punto de entrada de la aplicación.
   2. Inicialización de `gfxInitDefault`, `C3D_Init` y `C2D_Init`.
   3. Bucle principal.
   4. Código que dibuja la interfaz actual.
   5. Gestión de entrada táctil, botones y teclado.
   6. Lugar donde se cargan texturas y fuentes.
   7. Configuración de `romfs`.
   8. Configuración de `sdmc`.
   9. Reglas del `Makefile`.
   10. Herramientas de empaquetado `.3dsx` y, si existe, `.cia`.

   Crear un documento de auditoría:

   ```text
   docs/gui-3ds-audit.md
   ```

   Debe contener una tabla semejante a:

   | Subsistema | Archivo | Función | Acción |
   |---|---|---|---|
   | Entrada | `...` | `main()` | Conservar |
   | Render | `...` | `render_frame()` | Extender |
   | Texturas | `...` | `load_texture()` | Reutilizar |
   | Almacenamiento | `...` | `sdmc` | Reutilizar |
   | Red | inexistente | — | Añadir |
   | JavaScript | inexistente | — | Añadir |

4. **Construir el juego web original sin modificarlo inicialmente**

   ```bash
   cd ~/work/pokerogue-3ds/pokerogue

   npm ci
   npm run build
   ```

   Si el repositorio proporciona un script específico para producción, utilizarlo:

   ```bash
   npm run build -- --mode production
   ```

   Verificar la salida:

   ```bash
   find dist -type f | sort
   du -sh dist
   ```

   El resultado debe contener el HTML de entrada, JavaScript, hojas de estilo y assets. No se debe asumir que el bundle de Vite es ejecutable en 3DS: inicialmente solo sirve como fuente para el runtime embebido.

5. **Generar un bundle estático determinista**

   En `package.json`, comprobar que el build use rutas relativas:

   ```json
   {
     "scripts": {
       "build:3ds": "vite build --base=./"
     }
   }
   ```

   Ejecutar:

   ```bash
   npm run build:3ds
   ```

   Comprobar que no haya referencias absolutas:

   ```bash
   grep -RInE 'src="/|href="/|url\(/|https?://' dist || true
   ```

   Generar un inventario de archivos:

   ```bash
   find dist -type f -printf '%P\t%s\n' | sort > dist-manifest.txt
   sha256sum $(find dist -type f | sort) > dist-sha256.txt
   ```

   Copiar el contenido al proyecto nativo como recurso inicial:

   ```bash
   rm -rf ~/work/pokerogue-3ds/native/romfs/web
   mkdir -p ~/work/pokerogue-3ds/native/romfs/web
   cp -a dist/. ~/work/pokerogue-3ds/native/romfs/web/
   ```

6. **Determinar el alcance real del runtime JavaScript**

   No debe intentarse ejecutar un navegador completo. QuickJS o Duktape no proporcionan por sí mismos:

   - DOM.
   - Canvas 2D.
   - `window`.
   - `document`.
   - `Image`.
   - `fetch`.
   - IndexedDB.
   - CSS.
   - Audio web.
   - Web Workers.

   Por ello, antes de escribir el bridge, inspeccionar las APIs utilizadas:

   ```bash
   cd ~/work/pokerogue-3ds/pokerogue

   grep -RohE '[A-Za-z_$][A-Za-z0-9_$]*\.(canvas|fillRect|drawImage|localStorage|indexedDB|fetch|Audio|Image)' \
       src dist | sort -u

   grep -RInE 'document\.|window\.|canvas|getContext|indexedDB|localStorage|fetch\(' \
       src
   ```

   Crear una matriz de compatibilidad:

   ```text
   docs/js-api-matrix.md
   ```

   Cada API debe clasificarse como:

   1. Implementable directamente.
   2. Implementable mediante shim.
   3. Requiere modificación del código web.
   4. No viable en el hardware actual.

   Si PokéRogue depende de un framework gráfico web grande, no se debe continuar con la integración ciega. Será necesario crear un modo de renderizado específico para 3DS o sustituir el frontend por una capa compatible.

7. **Añadir QuickJS o Duktape como dependencia aislada**

   La elección debe hacerse antes de implementar el bridge:

   - QuickJS: mejor compatibilidad con JavaScript moderno, mayor consumo de memoria.
   - Duktape: menor consumo, pero requiere revisar compatibilidad con sintaxis y APIs modernas.

   El siguiente ejemplo utiliza QuickJS. Integrar sus fuentes en un directorio separado, por ejemplo:

   ```text
   native/
     third_party/quickjs/
     source/js_runtime.cpp
     source/js_runtime.hpp
     source/js_canvas.cpp
     source/js_storage.cpp
   ```

   Añadir las fuentes al `Makefile` del proyecto sin reemplazar las reglas existentes:

   ```make
   CXXFLAGS += -I$(CURDIR)/third_party/quickjs
   CXXFLAGS += -Isource

   SOURCES += source/js_runtime.cpp
   SOURCES += source/js_canvas.cpp
   SOURCES += source/js_storage.cpp

   LIBS += -lctru -lcitro2d -lcitro3d
   ```

   Ajustar los nombres de variables a los que realmente use `GUI_3ds`.

8. **Crear el runtime JavaScript mínimo**

   `source/js_runtime.hpp`:

   ```cpp
   #pragma once

   #include <3ds.h>
   #include <string>
   #include <vector>

   struct JsRuntime {
       void* runtime;
       void* context;
       std::vector<std::string> loadedScripts;

       bool init();
       bool loadFile(const char* path);
       bool eval(const char* source, const char* filename);
       void shutdown();
   };

   bool js_runtime_init(JsRuntime* js);
   bool js_runtime_load_web_bundle(JsRuntime* js, const char* entry);
   ```

   `source/js_runtime.cpp`:

   ```cpp
   #include "js_runtime.hpp"
   #include <cstdio>
   #include <cstdlib>

   /*
    * Los tipos y llamadas concretas deben corresponder a la versión
    * de QuickJS integrada en third_party/quickjs.
    */
   #include "quickjs.h"

   static std::string read_file(const char* path) {
       FILE* f = std::fopen(path, "rb");
       if (!f) return {};

       std::fseek(f, 0, SEEK_END);
       long size = std::ftell(f);
       std::rewind(f);

       std::string data;
       data.resize(size);
       std::fread(data.data(), 1, size, f);
       std::fclose(f);
       return data;
   }

   bool JsRuntime::init() {
       runtime = JS_NewRuntime();
       if (!runtime) return false;

       context = JS_NewContext(static_cast<JSRuntime*>(runtime));
       if (!context) {
           JS_FreeRuntime(static_cast<JSRuntime*>(runtime));
           runtime = nullptr;
           return false;
       }

       return true;
   }

   bool JsRuntime::eval(const char* source, const char* filename) {
       JSContext* ctx = static_cast<JSContext*>(context);

       JSValue result = JS_Eval(
           ctx,
           source,
           std::strlen(source),
           filename,
           JS_EVAL_TYPE_GLOBAL
       );

       if (JS_IsException(result)) {
           JSValue exception = JS_GetException(ctx);
           const char* text = JS_ToCString(ctx, exception);

           if (text) {
               printf("JS exception in %s: %s\n", filename, text);
               JS_FreeCString(ctx, text);
           }

           JS_FreeValue(ctx, exception);
           JS_FreeValue(ctx, result);
           return false;
       }

       JS_FreeValue(ctx, result);
       return true;
   }

   bool JsRuntime::loadFile(const char* path) {
       std::string data = read_file(path);
       if (data.empty()) {
           printf("Cannot read JS file: %s\n", path);
           return false;
       }

       return eval(data.c_str(), path);
   }

   void JsRuntime::shutdown() {
       if (context) {
           JS_FreeContext(static_cast<JSContext*>(context));
           context = nullptr;
       }

       if (runtime) {
           JS_FreeRuntime(static_cast<JSRuntime*>(runtime));
           runtime = nullptr;
       }
   }

   bool js_runtime_init(JsRuntime* js) {
       return js && js->init();
   }

   bool js_runtime_load_web_bundle(JsRuntime* js, const char* entry) {
       return js && js->loadFile(entry);
   }
   ```

   La ruta `romfs:/web/index.js` solo funcionará si el bundle está integrado en `romfs`. Si los assets se actualizan dinámicamente, la ruta final deberá ser `sdmc:/3ds/pokerogue/web/index.js`.

9. **Implementar un shim de `window`, `document` y `canvas`**

   El shim debe implementarse antes de evaluar el bundle principal:

   ```javascript
   globalThis.window = globalThis;

   globalThis.performance = {
     now: function () {
       return Date.now();
     }
   };

   globalThis.requestAnimationFrame = function (callback) {
     return __native_request_frame(callback);
   };

   globalThis.cancelAnimationFrame = function (id) {
     __native_cancel_frame(id);
   };

   globalThis.document = {
     createElement: function (type) {
       if (type === "canvas") {
         return __native_create_canvas();
       }
       throw new Error("Unsupported element: " + type);
     },

     getElementById: function (id) {
       return __native_get_element(id);
     },

     body: {
       appendChild: function () {}
     }
   };
   ```

   El shim debe compilarse como un archivo pequeño y cargarse en este orden:

   ```text
   1. runtime JS
   2. funciones nativas
   3. web_shim.js
   4. polyfills
   5. bundle principal
   ```

10. **Crear el bridge de Canvas hacia Citro2D**

    El bridge no debe intentar convertir cada operación JavaScript en una llamada de GPU inmediatamente. Debe registrar comandos en un buffer de comandos por frame.

    `source/js_canvas.hpp`:

    ```cpp
    #pragma once

    #include <citro2d.h>
    #include <vector>

    enum class DrawCommandType {
        Clear,
        FillRect,
        DrawTexture
    };

    struct DrawCommand {
        DrawCommandType type;
        float x;
        float y;
        float w;
        float h;
        u32 color;
        C2D_Image image;
    };

    struct CanvasState {
        std::vector<DrawCommand> commands;
        float width;
        float height;
    };

    void canvas_begin_frame(CanvasState* canvas);
    void canvas_render(const CanvasState* canvas);
    ```

    Implementación conceptual:

    ```cpp
    #include "js_canvas.hpp"

    void canvas_begin_frame(CanvasState* canvas) {
        canvas->commands.clear();
    }

    void canvas_render(const CanvasState* canvas) {
        for (const DrawCommand& command : canvas->commands) {
            switch (command.type) {
                case DrawCommandType::Clear:
                    C2D_TargetClear(nullptr, command.color);
                    break;

                case DrawCommandType::FillRect:
                    C2D_DrawRectSolid(
                        nullptr,
                        command.x,
                        command.y,
                        0.0f,
                        command.w,
                        command.h,
                        command.color
                    );
                    break;

                case DrawCommandType::DrawTexture:
                    C2D_DrawImageAt(
                        command.image,
                        command.x,
                        command.y,
                        0.0f,
                        nullptr,
                        1.0f,
                        1.0f
                    );
                    break;
            }
        }
    }
    ```

    Las funciones registradas en QuickJS deben añadir comandos a `CanvasState`, no dibujar fuera del ciclo de renderizado:

    ```cpp
    static JSValue js_canvas_fill_rect(
        JSContext* ctx,
        JSValueConst this_val,
        int argc,
        JSValueConst* argv
    ) {
        double x, y, w, h;

        if (argc < 4 ||
            JS_ToFloat64(ctx, &x, argv[0]) < 0 ||
            JS_ToFloat64(ctx, &y, argv[1]) < 0 ||
            JS_ToFloat64(ctx, &w, argv[2]) < 0 ||
            JS_ToFloat64(ctx, &h, argv[3]) < 0) {
            return JS_ThrowTypeError(ctx, "fillRect requires x,y,w,h");
        }

        CanvasState* canvas = get_current_canvas();

        canvas->commands.push_back({
            DrawCommandType::FillRect,
            static_cast<float>(x),
            static_cast<float>(y),
            static_cast<float>(w),
            static_cast<float>(h),
            0xffffffff,
            {}
        });

        return JS_UNDEFINED;
    }
    ```

    Implementar progresivamente, en este orden:

    1. `fillRect`.
    2. `clearRect`.
    3. `drawImage`.
    4. Carga de imágenes.
    5. Transformaciones.
    6. Transparencia.
    7. Texto.
    8. Composición y filtros, solo si son imprescindibles.

    No implementar una API Canvas completa hasta demostrar que el juego usa esas operaciones.

11. **Adaptar el bucle principal de `GUI_3ds`**

    La estructura debe conservar la inicialización existente de `GUI_3ds`:

    ```cpp
    int main(int argc, char** argv) {
        gfxInitDefault();
        romfsInit();

        C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
        C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
        C2D_Prepare();

        C3D_RenderTarget* top =
            C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);

        JsRuntime js{};
        if (!js_runtime_init(&js)) {
            printf("JS runtime initialization failed\n");
            goto shutdown;
        }

        if (!js_runtime_load_web_bundle(&js, "romfs:/web/web_shim.js")) {
            goto shutdown_js;
        }

        if (!js_runtime_load_web_bundle(&js, "romfs:/web/index.js")) {
            goto shutdown_js;
        }

        while (aptMainLoop()) {
            hidScanInput();

            u32 kDown = hidKeysDown();
            if (kDown & KEY_START) break;

            canvas_begin_frame(get_current_canvas());
            js_pump_events(&js);
            js_run_pending_animation_frames(&js);

            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
            C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
            C2D_SceneBegin(top);

            canvas_render(get_current_canvas());

            C3D_FrameEnd(0);
        }

    shutdown_js:
        js.shutdown();

    shutdown:
        C2D_Fini();
        C3D_Fini();
        romfsExit();
        gfxExit();
        return 0;
    }
    ```

    El nombre y la ubicación del `main()` deben ajustarse a la estructura real de `GUI_3ds`.

12. **Compilar la integración mínima antes de cargar PokéRogue**

    Primero ejecutar solamente:

    ```javascript
    document.createElement("canvas");
    requestAnimationFrame(function () {
      console.log("frame");
    });
    ```

    Después añadir una prueba gráfica mínima:

    ```javascript
    const canvas = document.createElement("canvas");
    const ctx = canvas.getContext("2d");
    ctx.fillRect(10, 10, 100, 50);
    ```

    No integrar el bundle completo hasta que el runtime pueda:

    1. Inicializarse.
    2. Evaluar JavaScript.
    3. Crear un canvas.
    4. Ejecutar un frame.
    5. Dibujar un rectángulo.
    6. Cerrar sin fugas ni crashes.

13. **Automatizar la compilación y el arranque de Azahar**

    No fijar una ruta arbitraria del emulador. Utilizar una variable configurable:

    ```make
    AZAHAR ?= azahar
    AZAHAR_USER_DIR ?= $(CURDIR)/.azahar
    AZAHAR_SD_DIR ?= $(CURDIR)/.azahar/sdmc

    run-azahar: $(TARGET).3dsx
        mkdir -p "$(AZAHAR_SD_DIR)/3ds/pokerogue"
        cp "$(TARGET).3dsx" \
           "$(AZAHAR_SD_DIR)/3ds/pokerogue/$(TARGET).3dsx"
        "$(AZAHAR)" \
           --user-directory "$(AZAHAR_USER_DIR)" \
           --sdmc-dir "$(AZAHAR_SD_DIR)" \
           "$(TARGET).3dsx"

    build-run: all
        $(MAKE) run-azahar
    ```

    Si la versión de Azahar instalada no acepta esos argumentos, localizar sus opciones reales:

    ```bash
    azahar --help
    ```

    Alternativamente, utilizar el directorio de SD configurado manualmente en Azahar y automatizar solo la copia:

    ```make
    run-azahar: $(TARGET).3dsx
        cp "$(TARGET).3dsx" "$(AZAHAR_SD_DIR)/3ds/pokerogue/"
        "$(AZAHAR)"
    ```

    El ciclo obligatorio será:

    ```bash
    make clean
    make -j$(nproc)
    make run-azahar
    ```

    Prohibir pruebas en consola física durante las fases de desarrollo. Toda regresión debe reproducirse primero en Azahar.

14. **Definir criterios de finalización de la Fase 1**

    No avanzar hasta que se cumpla todo lo siguiente:

    1. `GUI_3ds` compila sin warnings nuevos críticos.
    2. El `.3dsx` arranca en Azahar.
    3. El runtime JavaScript ejecuta el shim.
    4. El bundle web se carga desde `romfs`.
    5. El canvas muestra al menos una escena de prueba.
    6. La entrada táctil y los botones llegan al JavaScript.
    7. Se han identificado todas las APIs web no implementadas.
    8. El juego no depende de `eval` dinámico sin control, salvo que el runtime lo soporte expresamente.
    9. El consumo de memoria se mide en ejecución.
    10. Se dispone de logs reproducibles de Azahar.

> **Warning:** Un bundle web minimizado no implica que sea viable en 3DS. El código JavaScript, el heap de QuickJS, los buffers de Canvas, las texturas descomprimidas y el sistema de archivos compiten por la misma memoria limitada.

# Fase 2: Intercepción de I/O y persistencia en tarjeta SD

1. **Localizar el sistema de persistencia original**

   ```bash
   cd ~/work/pokerogue-3ds/pokerogue

   grep -RInE 'localStorage|sessionStorage|indexedDB|Dexie|idb|save|serialize|deserialize' \
       src public dist
   ```

   Registrar:

   1. Claves utilizadas.
   2. Estructura JSON o binaria.
   3. Operaciones síncronas y asíncronas.
   4. Frecuencia de guardado.
   5. Migraciones de versión.
   6. Datos temporales que no deben persistirse.
   7. Datos que contienen blobs o imágenes.

2. **No emular IndexedDB completo si no es necesario**

   Crear una capa de compatibilidad que exponga únicamente las operaciones realmente utilizadas. Por ejemplo:

   ```javascript
   globalThis.localStorage = {
     getItem: function (key) {
       return __native_storage_get(String(key));
     },

     setItem: function (key, value) {
       __native_storage_set(String(key), String(value));
     },

     removeItem: function (key) {
       __native_storage_remove(String(key));
     },

     clear: function () {
       __native_storage_clear();
     },

     key: function (index) {
       return __native_storage_key(index);
     },

     get length() {
       return __native_storage_length();
     }
   };
   ```

   Si el juego usa IndexedDB, proporcionar una interfaz asíncrona compatible:

   ```javascript
   globalThis.indexedDB = {
     open: function (name, version) {
       return __native_idb_open(String(name), Number(version));
     }
   };
   ```

   El agente debe adaptar este shim a las llamadas concretas encontradas durante la auditoría. No debe crear una implementación ficticia que devuelva éxito y descarte datos.

3. **Crear el directorio de almacenamiento de prueba**

   Durante el desarrollo utilizar:

   ```text
   sdmc:/3ds/pokerogue-test/saves/
   ```

   En la ejecución final utilizar:

   ```text
   sdmc:/3ds/pokerogue/saves/
   ```

   Definir una constante compilable:

   ```cpp
   #ifdef POKEROGUE_TEST_STORAGE
   static constexpr const char* SAVE_DIR =
       "sdmc:/3ds/pokerogue-test/saves/";
   #else
   static constexpr const char* SAVE_DIR =
       "sdmc:/3ds/pokerogue/saves/";
   #endif
   ```

4. **Inicializar `sdmc` una sola vez**

   ```cpp
   Result storage_init() {
       Result rc = fsInit();
       if (R_FAILED(rc)) return rc;

       rc = fsdevMountSdmc();
       if (R_FAILED(rc)) {
           fsExit();
           return rc;
       }

       mkdir("sdmc:/3ds", 0777);
       mkdir("sdmc:/3ds/pokerogue", 0777);
       mkdir(SAVE_DIR, 0777);

       return 0;
   }

   void storage_shutdown() {
       fsdevUnmountDevice("sdmc");
       fsExit();
   }
   ```

   Si `GUI_3ds` ya inicializa `fs` o monta SD, reutilizar esa inicialización y evitar montarla dos veces.

5. **Implementar escritura atómica**

   `source/js_storage.cpp`:

   ```cpp
   #include <3ds.h>
   #include <cstdio>
   #include <cerrno>
   #include <sys/stat.h>
   #include <string>

   static const char* save_path(const char* key) {
       static char path[512];
       std::snprintf(
           path,
           sizeof(path),
           "%s%s.json",
           SAVE_DIR,
           key
       );
       return path;
   }

   static bool write_atomic(
       const char* final_path,
       const char* data,
       size_t length
   ) {
       char temp_path[512];
       std::snprintf(
           temp_path,
           sizeof(temp_path),
           "%s.tmp",
           final_path
       );

       FILE* f = std::fopen(temp_path, "wb");
       if (!f) return false;

       bool ok = std::fwrite(data, 1, length, f) == length;
       if (std::fflush(f) != 0) ok = false;
       std::fclose(f);

       if (!ok) {
           std::remove(temp_path);
           return false;
       }

       std::remove(final_path);
       return std::rename(temp_path, final_path) == 0;
   }

   bool storage_set(
       const char* key,
       const char* value,
       size_t length
   ) {
       if (!key || !value) return false;

       const char* path = save_path(key);
       return write_atomic(path, value, length);
   }
   ```

   El nombre de la clave debe validarse para impedir `../` y separadores de directorio:

   ```cpp
   static bool valid_key(const char* key) {
       if (!key || !*key) return false;

       for (const char* p = key; *p; ++p) {
           if (*p == '/' || *p == '\\' || *p == '.' ||
               *p == ':' || *p < 0x20) {
               return false;
           }
       }

       return true;
   }
   ```

6. **Implementar lectura y eliminación**

   ```cpp
   std::string storage_get(const char* key) {
       if (!valid_key(key)) return {};

       FILE* f = std::fopen(save_path(key), "rb");
       if (!f) return {};

       std::fseek(f, 0, SEEK_END);
       long size = std::ftell(f);
       std::rewind(f);

       if (size <= 0 || size > 4 * 1024 * 1024) {
           std::fclose(f);
           return {};
       }

       std::string value(size, '\0');
       std::fread(value.data(), 1, value.size(), f);
       std::fclose(f);
       return value;
   }

   bool storage_remove(const char* key) {
       if (!valid_key(key)) return false;
       return std::remove(save_path(key)) == 0;
   }
   ```

   Limitar el tamaño de cada entrada y rechazar JSON inválido antes de entregarlo al runtime JavaScript.

7. **Registrar las funciones nativas en QuickJS**

   ```cpp
   static JSValue js_storage_get(
       JSContext* ctx,
       JSValueConst,
       int argc,
       JSValueConst* argv
   ) {
       if (argc < 1) {
           return JS_ThrowTypeError(ctx, "missing key");
       }

       const char* key = JS_ToCString(ctx, argv[0]);
       if (!key) return JS_EXCEPTION;

       std::string value = storage_get(key);
       JS_FreeCString(ctx, key);

       if (value.empty()) return JS_NULL;
       return JS_NewStringLen(ctx, value.data(), value.size());
   }

   static JSValue js_storage_set(
       JSContext* ctx,
       JSValueConst,
       int argc,
       JSValueConst* argv
   ) {
       if (argc < 2) {
           return JS_ThrowTypeError(ctx, "missing key/value");
       }

       const char* key = JS_ToCString(ctx, argv[0]);
       size_t length = 0;
       const char* value = JS_ToCStringLen(ctx, &length, argv[1]);

       if (!key || !value) {
           if (key) JS_FreeCString(ctx, key);
           if (value) JS_FreeCString(ctx, value);
           return JS_EXCEPTION;
       }

       bool ok = storage_set(key, value, length);

       JS_FreeCString(ctx, key);
       JS_FreeCString(ctx, value);

       if (!ok) {
           return JS_ThrowInternalError(ctx, "save failed");
       }

       return JS_UNDEFINED;
   }
   ```

   Registrar las funciones bajo el objeto global:

   ```cpp
   JSValue global = JS_GetGlobalObject(ctx);

   JS_SetPropertyStr(
       ctx,
       global,
       "__native_storage_get",
       JS_NewCFunction(ctx, js_storage_get, "__native_storage_get", 1)
   );

   JS_SetPropertyStr(
       ctx,
       global,
       "__native_storage_set",
       JS_NewCFunction(ctx, js_storage_set, "__native_storage_set", 2)
   );

   JS_FreeValue(ctx, global);
   ```

8. **Añadir control de corrupción y versiones**

   En lugar de guardar únicamente el valor original, envolverlo:

   ```json
   {
     "format": 1,
     "timestamp": 0,
     "payload": {},
     "sha256": "..."
   }
   ```

   Mantener copias rotativas:

   ```text
   profile.json
   profile.json.bak1
   profile.json.bak2
   ```

   Flujo de guardado:

   1. Validar JSON.
   2. Crear copia de seguridad del archivo anterior.
   3. Escribir `.tmp`.
   4. Forzar `fflush`.
   5. Renombrar `.tmp` al nombre final.
   6. Verificar que el archivo puede leerse.
   7. Solo entonces eliminar la copia más antigua.

9. **Probar persistencia exclusivamente en Azahar**

   Configurar el directorio de SD del emulador:

   ```text
   .azahar/sdmc/3ds/pokerogue-test/saves/
   ```

   Ejecutar una prueba desde JavaScript:

   ```javascript
   localStorage.setItem("test", JSON.stringify({
     value: 123,
     text: "azahar"
   }));

   const value = localStorage.getItem("test");
   if (!value) throw new Error("storage read failed");
   ```

   Reiniciar el `.3dsx` en Azahar y verificar que el valor sobrevive. Después probar:

   1. Cierre normal.
   2. Cierre mientras se guarda.
   3. Archivo truncado.
   4. JSON corrupto.
   5. Falta de espacio.
   6. Clave inválida.
   7. Dos guardados seguidos.
   8. Reinicio del emulador.

10. **Definir criterios de finalización de la Fase 2**

    No avanzar hasta que:

    1. Se hayan identificado todos los métodos de persistencia originales.
    2. `localStorage` funcione sobre el directorio virtual de Azahar.
    3. IndexedDB esté implementado o haya sido reemplazado por un adaptador compatible.
    4. Los guardados sean atómicos.
    5. Existan copias de seguridad.
    6. Los datos corruptos no bloqueen el arranque.
    7. El formato tenga número de versión.
    8. La ruta de producción sea `sdmc:/3ds/pokerogue/saves/`.
    9. No se haya escrito todavía en una consola física.

> **Warning:** Serializar grandes estructuras JSON repetidamente puede agotar el heap de QuickJS y producir pausas visibles. Preferir guardados incrementales, limitar el tamaño de los buffers temporales y liberar explícitamente los objetos JavaScript tras cada operación.

# Fase 3: Sistema de autoactualización OTA integrado en `GUI_3ds`

1. **Definir un manifiesto versionado**

   No descargar archivos arbitrarios directamente desde una rama mutable. Publicar o consumir un manifiesto firmado o, como mínimo, validado por SHA-256:

   ```json
   {
     "format": 1,
     "version": "1.2.3",
     "channel": "stable",
     "files": [
       {
         "path": "index.js",
         "url": "https://example.invalid/pokerogue/1.2.3/index.js",
         "size": 123456,
         "sha256": "..."
       },
       {
         "path": "assets/sprites.pak",
         "url": "https://example.invalid/pokerogue/1.2.3/assets/sprites.pak",
         "size": 456789,
         "sha256": "..."
       }
     ]
   }
   ```

   El repositorio original puede ser la fuente de versiones, pero es preferible que el proceso CI genere un artefacto estable. No depender de HTML de GitHub, nombres de ramas ni URLs que cambien.

2. **Guardar la versión local**

   ```text
   sdmc:/3ds/pokerogue/web/version.json
   ```

   Ejemplo:

   ```json
   {
     "version": "1.2.2",
     "manifest_sha256": "..."
   }
   ```

3. **Inicializar red mediante `httpc`**

   El código debe reutilizar cualquier módulo de red ya presente en `GUI_3ds`. Si no existe:

   ```cpp
   Result net_init() {
       Result rc = amInit();
       if (R_FAILED(rc)) return rc;

       rc = httpcInit(4 * 1024);
       if (R_FAILED(rc)) {
           amExit();
           return rc;
       }

       return 0;
   }

   void net_shutdown() {
       httpcExit();
       amExit();
   }
   ```

   La implementación exacta puede variar según la versión de libctru y los headers disponibles.

4. **Implementar descarga a archivo temporal**

   ```cpp
   bool http_download(
       const char* url,
       const char* temporary_path
   ) {
       httpcContext context;
       Result rc = httpcOpenDefault(
           &context,
           reinterpret_cast<const u8*>(url),
           0
       );

       if (R_FAILED(rc)) return false;

       rc = httpcSetSSLOpt(&context, SSLCOPT_DisableVerify);
       /*
        * Esta opción no debe usarse en una versión de distribución.
        * Para producción debe configurarse validación TLS o fijación
        * de certificados según las capacidades del entorno.
        */

       if (R_FAILED(rc)) {
           httpcCloseContext(&context);
           return false;
       }

       rc = httpcAddRequestHeaderField(
           &context,
           "User-Agent",
           "PokeRogue-3DS"
       );

       if (R_FAILED(rc)) {
           httpcCloseContext(&context);
           return false;
       }

       rc = httpcBeginRequest(&context);
       if (R_FAILED(rc)) {
           httpcCloseContext(&context);
           return false;
       }

       u32 status = 0;
       httpcGetResponseStatusCode(&context, &status);

       if (status != 200) {
           httpcCloseContext(&context);
           return false;
       }

       FILE* output = std::fopen(temporary_path, "wb");
       if (!output) {
           httpcCloseContext(&context);
           return false;
       }

       u8 buffer[16 * 1024];
       bool ok = true;

       while (true) {
           u32 downloaded = 0;

           rc = httpcReceiveData(
               &context,
               buffer,
               sizeof(buffer)
           );

           if (R_FAILED(rc)) {
               if (rc == HTTPC_RESULTCODE_DOWNLOADPENDING) {
                   continue;
               }

               ok = false;
               break;
           }

           /*
            * El tamaño real debe obtenerse mediante la API disponible
            * en la versión de libctru utilizada.
            */
           downloaded = sizeof(buffer);

           if (std::fwrite(buffer, 1, downloaded, output) != downloaded) {
               ok = false;
               break;
           }
       }

       std::fflush(output);
       std::fclose(output);
       httpcCloseContext(&context);

       return ok;
   }
   ```

   El agente debe adaptar el manejo de fin de descarga al comportamiento real de la versión instalada de `httpc`. No debe asumir que cada llamada devuelve exactamente el tamaño del buffer.

5. **No desactivar TLS en producción**

   El ejemplo anterior muestra una ruta de desarrollo, pero una versión distribuible debe:

   1. Usar HTTPS.
   2. Validar certificados.
   3. Verificar el hash de cada archivo.
   4. Verificar la firma del manifiesto si está disponible.
   5. Rechazar redirecciones a dominios no autorizados.
   6. Imponer un tamaño máximo por archivo.
   7. Cancelar descargas incompletas.

   Una implementación mínima de lista permitida:

   ```cpp
   static bool allowed_host(const char* url) {
       return std::strstr(url, "https://updates.example.invalid/") == url;
   }
   ```

6. **Comparar versiones locales y remotas**

   Implementar un comparador semántico sencillo:

   ```cpp
   struct Version {
       int major;
       int minor;
       int patch;
   };

   static bool version_newer(
       const Version& remote,
       const Version& local
   ) {
       if (remote.major != local.major)
           return remote.major > local.major;

       if (remote.minor != local.minor)
           return remote.minor > local.minor;

       return remote.patch > local.patch;
   }
   ```

   El flujo debe ser:

   ```text
   1. Leer version.json local.
   2. Descargar manifest.json a manifest.tmp.
   3. Validar HTTP 200.
   4. Validar tamaño.
   5. Parsear JSON.
   6. Comparar versión.
   7. Mostrar al usuario el tamaño total.
   8. Descargar cada archivo a un .tmp.
   9. Verificar SHA-256.
   10. Mover el paquete a un directorio staging.
   11. Escribir update-complete.marker.
   12. Cambiar el puntero de versión activa.
   13. Reiniciar el runtime JavaScript.
   ```

7. **Utilizar actualización por directorios, no sobrescritura directa**

   Estructura recomendada:

   ```text
   sdmc:/3ds/pokerogue/web/releases/1.2.2/
   sdmc:/3ds/pokerogue/web/releases/1.2.3/
   sdmc:/3ds/pokerogue/web/current.txt
   ```

   El archivo `current.txt` contendrá:

   ```text
   1.2.3
   ```

   La actualización será:

   ```text
   releases/1.2.3.tmp/
   releases/1.2.3/
   current.txt.tmp
   current.txt
   ```

   Si la aplicación se interrumpe, el runtime continuará utilizando la versión anterior.

8. **Implementar la descarga de assets**

   La lista de archivos debe incluir únicamente recursos realmente consumidos por el runtime:

   ```text
   index.js
   web_shim.js
   styles.css
   assets/*.json
   assets/*.png
   assets/*.bin
   ```

   Evitar descargar:

   1. Código fuente TypeScript.
   2. Mapas `.map`.
   3. Assets destinados exclusivamente al navegador.
   4. Duplicados.
   5. Resoluciones superiores a las soportadas por 3DS.

9. **Recargar los assets dinámicamente**

   No reemplazar archivos que estén siendo usados por el runtime. El mecanismo seguro es:

   ```cpp
   bool reload_web_version(JsRuntime* js, const char* version) {
       std::string base = "sdmc:/3ds/pokerogue/web/releases/";
       base += version;
       base += "/";

       /*
        * 1. Detener el bucle de juego.
        * 2. Cancelar callbacks pendientes.
        * 3. Liberar texturas y handles.
        * 4. Crear un nuevo contexto JS.
        * 5. Cargar shim desde base.
        * 6. Cargar bundle desde base.
        * 7. Ejecutar bootstrap.
        * 8. Si falla, restaurar la versión anterior.
        */
       return true;
   }
   ```

   Si QuickJS no permite liberar de forma fiable todos los objetos de la aplicación, destruir el contexto completo y crear uno nuevo. No ejecutar dos bundles completos simultáneamente.

10. **Exponer el estado de red al juego**

    El JavaScript solo debe recibir resultados de alto nivel:

    ```javascript
    const update = await __native_check_update();

    if (update.available) {
      console.log("New version:", update.version);
      await __native_download_update();
      await __native_activate_update();
      __native_restart_runtime();
    }
    ```

    No exponer directamente handles de `httpc` ni punteros nativos al bundle.

11. **Probar OTA en Azahar**

    Crear un servidor local o entorno de staging:

    ```bash
    cd release-server
    python3 -m http.server 8080
    ```

    En el emulador, probar al menos:

    1. Sin conexión.
    2. DNS fallido.
    3. HTTPS inválido.
    4. Manifiesto malformado.
    5. Versión remota menor.
    6. Archivo con hash incorrecto.
    7. Descarga interrumpida.
    8. Falta de espacio.
    9. Reinicio durante staging.
    10. Activación correcta.
    11. Rollback a la versión anterior.
    12. Recarga del bundle sin duplicar memoria.

    La SD virtual debe contener:

    ```text
    .azahar/sdmc/3ds/pokerogue/web/
    ```

12. **Medir memoria durante la descarga y la recarga**

    Registrar:

    ```cpp
    printf(
        "heap available before update: %lu\n",
        osGetMemRegionFree(MEMREGION_APPLICATION)
    );
    ```

    Liberar buffers de red antes de inicializar el nuevo runtime. Nunca conservar simultáneamente:

    1. Bundle comprimido.
    2. Bundle descomprimido.
    3. Copia del bundle anterior.
    4. Nuevo contexto JS.
    5. Texturas antiguas.
    6. Texturas nuevas.

> **Warning:** En Old 3DS, una actualización que funciona en escritorio o New 3DS puede fallar al recargar por duplicación temporal de memoria. Usar descargas por archivo, liberar el contexto anterior antes de activar el nuevo y mantener los sprites comprimidos en SD hasta que sean estrictamente necesarios.

13. **Optimizar sprites y uso de VRAM**

    Aplicar estas reglas:

    1. Redimensionar sprites a las resoluciones realmente utilizadas.
    2. Convertir PNG a formatos compatibles con Citro2D.
    3. Evitar cargar todo el atlas al inicio.
    4. Implementar caché LRU de texturas.
    5. Liberar texturas al cambiar de escena.
    6. No almacenar simultáneamente PNG comprimido y bitmap descomprimido si no es necesario.
    7. Agrupar sprites en atlases pequeños.
    8. Usar streaming desde SD para contenido infrecuente.
    9. Limitar la cantidad de texturas residentes en VRAM.
    10. Comprobar alineamiento y dimensiones requeridas por la GPU.

14. **Definir criterios de finalización de la Fase 3**

    El sistema OTA estará terminado cuando:

    1. Consulte una fuente de actualización configurable.
    2. Compare la versión local con la remota.
    3. Descargue únicamente archivos declarados.
    4. Verifique tamaño y SHA-256.
    5. Utilice staging y activación atómica.
    6. Mantenga una versión anterior recuperable.
    7. Funcione sin conexión sin bloquear el juego.
    8. Recargue el runtime JavaScript sin fugas observables.
    9. Haya sido validado en Azahar con interrupciones y errores.
    10. La validación TLS no esté desactivada en la build de distribución.

15. **Generar el artefacto final**

    Construir una versión limpia:

    ```bash
    cd ~/work/pokerogue-3ds/native

    make clean
    make -j$(nproc)
    ```

    Verificar los artefactos:

    ```bash
    file *.3dsx
    find . -maxdepth 2 -type f \( -name '*.3dsx' -o -name '*.cia' \) -print
    ```

    Si el proyecto ya contiene reglas para `.cia`, ejecutarlas únicamente después de validar el `.3dsx`:

    ```bash
    make cia
    ```

    Si no existen reglas `.cia`, no inventar una cadena de empaquetado incompatible. Entregar el `.3dsx` funcional y añadir el empaquetado CIA mediante las herramientas oficiales de devkitPro y la estructura de banner, icono, metadata y título requerida por el proyecto.

16. **Validación final obligatoria en Azahar**

    Ejecutar una matriz limpia:

    ```bash
    rm -rf .azahar
    make build-run
    ```

    Validar:

    1. Primer arranque sin datos de usuario.
    2. Carga de la escena inicial.
    3. Entrada de botones.
    4. Entrada táctil.
    5. Carga de sprites.
    6. Guardado.
    7. Reinicio y restauración.
    8. Actualización OTA.
    9. Rollback.
    10. Falta de red.
    11. Falta de espacio.
    12. Cierre mediante `START`.
    13. Ausencia de errores críticos en el log.
    14. Memoria disponible después de una sesión prolongada.

17. **Fase final de hardware físico**

    Solo después de superar toda la matriz en Azahar:

    1. Copiar el `.3dsx` o instalar el `.cia`.
    2. Copiar únicamente la versión final de `sdmc:/3ds/pokerogue/`.
    3. Realizar un primer arranque sin datos de prueba.
    4. Probar guardado y restauración.
    5. Probar red.
    6. Conservar el build de Azahar utilizado como referencia.
    7. No actualizar el bundle durante la primera prueba física.
    8. Registrar diferencias de rendimiento entre Old 3DS y New 3DS.

> **Warning:** La ejecución final en hardware no debe utilizarse para descubrir problemas básicos de runtime, memoria, rutas SD, persistencia o renderizado. Todos esos problemas deben quedar resueltos y reproducidos en Azahar antes de transferir el artefacto a la consola física.

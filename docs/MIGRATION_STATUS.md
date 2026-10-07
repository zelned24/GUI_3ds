# Estado de PokéRogue para Old 3DS XL

## Revisión del avance local

El avance se publica como trabajo en curso. Classic completo y la fidelidad visual final no están verificados.

1. PokéRogue: `8555c08c823b856cbec4eb99ca84ea52a955836d`.
2. Assets: `056a1f408f26a3be4fef243f7462cb43608c7928`.
3. Locales: `23aea1cb0da5a0b15b836f3c243791591cc42303`.
4. Hash canónico: `b1b5821edb39a088e641682ae5f5eae9567eef03ffbc2aa3a257c148d0106edf`.

## Implementación presente

1. Presentación Citro2D de título/submenús, selección de starters/formas, equipo, acciones, movimientos, recompensas y decisiones de progreso. Ajustes e historial tienen interfaz parcial; no constituyen sistemas terminados.
2. Assets originales convertidos: arenas y capas, HUD, ventanas, fuente, cursor, 1500 iconos Pokémon en 8 páginas, 528 frames de objetos y 306 sprites de entrenador. Hay 249 mappings de tipos de entrenador; esto no demuestra cobertura completa de encuentros.
3. Intro adaptada a 16 frames muestreados y crossfade; no es reproducción completa del vídeo ni incluye audio.
4. Datos y resolvers de bayas, críticos, clima, turnos, entrenadores y almacenamiento extendido. Su presencia no implica paridad completa ni funcionamiento de todos los casos.
5. Boundary producción/fixtures y provenance del catálogo conservados. El conversor de presentación produce manifests bajo `build/native-presentation/`.

## Correcciones de esta revisión

1. Pausa consume START al abrirse y consume las acciones al salir; un comando del menú no se procesa como acción de combate.
2. Guardar y salir comprueba el resultado y lee el checkpoint confirmado antes de ofrecer Continuar; muestra el error y permanece en pausa si falla.
3. Nueva partida utiliza la semilla explícita de la run, sin introducir el reloj en resultados de juego. La selección de una nueva semilla por el usuario sigue pendiente.
4. Retirado saldo de dinero ficticio del HUD. Iconos y entrenadores rechazan claves sin entrada en el índice generado. Retirados 21 iconos sustitutos inventados para familias dinámicas (por ejemplo BERRY→Sitrus): 88 referencias literales válidas, 21 explícitamente pendientes.
5. Corregidos tipo numérico incompatible con ARM y crossfade que oscurecía la intro; su duración se obtiene del vídeo y su decodificación se valida.
6. Captura muestra iconos de la forma real y setup toma los tipos/estadísticas/primera habilidad de la forma seleccionada; actualizado descriptor de aplicación. El marco rojo sigue la captura de referencia del usuario. Las marcas de completo sin evidencia fueron reabiertas.

## Evidencia ejecutada

1. `npm test`: **36 passed, 0 failed**, 36 suites. La suite FirstRunRuntime contiene **51 passing, 0 failing, de 51**. No se desactivaron casos ni se debilitaron assertions.
2. `npm run native-parity`: **126/126 PASS**. Compara el contrato C++ con JS; no prueba todo el upstream.
3. `npm run native-test`: compilación PASS; el ejecutor informa explícitamente que no ejecutó hardware/emulador.
4. Compilación directa `make -f Makefile.3ds 3ds`: ELF y 3DSX producidos con devkitARM. No equivale a validación en Old 3DS.
5. `python scripts/verify_presentation_media.py`: archivos físicos y tablas de presentación PASS; no prueba fidelidad visual ni efectos de objetos.
6. Regresiones de navegación, geometría, índice de iconos, guards de pausa/guardado y ownership QuickJS PASS. Los guards de main son comprobaciones estáticas, no interacción real en Azahar.
7. Logs reproducibles de esta revisión: `build/form-hud-npm-test.log`, `build/form-hud-parity.log`, `build/review-native-test.log`, `build/form-hud-arm.log`, `build/window-style-media.log`. No se versionan binarios ni logs.

## Nitidez de presentación

1. Fuente convertida: alpha A4 binario (umbral 8/15), sin alterar métricas ni mapeo de glifos; 3267 bytes de alpha intermedio corregidos en el asset actual.
2. Posiciones de texto ajustadas a píxeles enteros; se mantiene NEAREST para texturas y fuente.
3. Azahar local tenía xBRZ activo y modo New 3DS. `scripts/prepare_azahar_preview.py` prepara un perfil aislado Old 3DS, resolución nativa, sin xBRZ y con muestreo nearest/display sin filtro; no modifica la configuración global ni inicia/cierra procesos.
4. `python test/pixel_font_tests.py`: 3 PASS (alpha/métricas/determinismo, rechazo de corrupción, perfil aislado). Compilación ARM PASS; comprobación visual conjunta pendiente.

## Replay y tipos verificados

1. Retirada la regla de anti-repetición de especies y su estado global: no existen en `Arena.randomSpecies` / `EncounterPhase` del pin de juego. El replay mantiene especies, identidades, movimientos, habilidades y RNG entre runtimes independientes, para 32 semillas reales, con especies distintas y campos dobles.
2. Especies sin formas conservan la segunda habilidad regular en `initializePokemonBattleStateForActor`; regresión explícita Purrloin/Limber. Fuente: `src/data/pokemon-species.ts::getAbility` y `src/field/pokemon.ts::getAbility` del pin de juego.
3. `NONE` representa ausencia de segundo tipo; no entra como tipo de combate. Inmunidades de estados normalizan nombres canónicos (Grass/Dark); resolver de estados acepta la especie base cuando no existe forma.
4. Huida: `CommandPhase.handleRunCommand` restringe End y entrenadores; un jefe ordinario no prohíbe huir por ser wave múltiplo de diez. `AttemptRunPhase` distingue boss por ambos enemigos y `BattleEndPhase(false)` no otorga victoria, EXP ni recompensa. Regresión verifica el roll seeded, la wave siguiente y el feedback.
5. Los 18 fallos registrados en la revisión anterior ya no se reproducen. Esto no demuestra cobertura completa: trampas, callbacks de huida, persistencia del contador de intentos y segundo jugador activo siguen pendientes.
6. TypeSafe instalado para Codex en `.agents/skills/typesafe-ai/` mediante npx. Se aplicó su instrucción de conservar reglas conocidas y ejecución en código; no se añadió un servicio de IA al runtime.

## Continuar y cargar

1. Continuar y Cargar ejecutan la misma lectura transaccional de progreso desde SD; no abren el estado no guardado que quede en memoria tras iniciar otra run.
2. Un fallo mantiene el título abierto y muestra `nativeSaveResultName` del resultado, incluido checksum, contenido incompatible o error de I/O. El arranque conserva el error de carga para mostrarlo en el título.
3. Cargar permite reintentar incluso sin un guardado detectado al arrancar. La navegación y los guards de dispatch se prueban; la lectura/restore usa la suite nativa existente. Interacción completa de SD en Azahar y consola sigue pendiente.

## Marcos de ventana

1. Se importan los cinco IDs de `UiWindowStyle` desde la revisión fijada (`src/enums/ui-window-style.ts`) y sus PNG 24×24 originales; el índice generado conserva ID/símbolo/ruta. El reporte `build/native-presentation/window-provenance.json` registra ambas revisiones y hashes.
2. Ajustes → Pantalla → Ventana emite un comando; el renderer carga el siguiente marco y lo aplica a todas las ventanas nativas. Conserva el anterior ante fallos y evita liberar texturas durante un frame activo.
3. El valor mostrado es el ID upstream activo. `settings-ui-items.ts` también muestra los IDs numéricos. El marco se restaura al arrancar desde `ui0.p3prefs`/`ui1.p3prefs`; A o izquierda/derecha lo cambian. No se cierra GUI-07.

## Persistencia de presentación

1. `NativePresentationSettingsStore` reutiliza la interfaz de almacenamiento y SHA-256 existentes, con un envelope pequeño independiente del guardado de partida. Escribe el slot alterno y comprueba los bytes leídos; no serializa structs nativos ni modifica el catálogo.
2. Un slot truncado/corrupto permite recuperar el otro y se informa al arrancar. Errores I/O, versiones futuras, generaciones contradictorias y agotamiento bloquean escritura; un fallo de guardado se muestra como cambio aplicado sin persistencia.
3. La prueba C++ host cubre guardado/restauración, determinismo byte a byte, corrupción de cada byte, interrupción, verificación tras escritura, conflicto y versión futura. El build ARM conecta los archivos SD fijos. Reinicio y fallos reales de SD en Azahar/consola siguen pendientes.

## Controles táctiles

1. Ajustes → Mando → Control táctil muestra el valor activo y emite un comando. Desactivar pide confirmación, como `enableTouchControls` en `src/ui/settings/settings-ui-items.ts` upstream fijado. Old 3DS siempre dispone de pantalla táctil: el modo automático upstream se adapta como activado/desactivado, conservando botones físicos.
2. El filtro se aplica antes de intro, título, submenús, pausa y comandos de juego/QuickJS; solo elimina KEY_TOUCH. La preferencia se guarda junto con el marco en el envelope v2; v1 conserva su marco y toma táctil activado como default upstream.
3. Pruebas host cubren confirmación/cancelación, reactivación física, los 32 bits de entrada, persistencia on/off y migración v1. La pregunta usa C2D_WordWrap con escala legible, sin reducir todo el texto a una única línea diminuta. Comparación visual y acción real con SD/táctil en Azahar/consola siguen pendientes.

## Arranque de la presentación

1. La fuente PokéRogue convertida y el marco original son requeridos en 3DS. Si faltan o no cargan, el renderer falla explícitamente y muestra un diagnóstico en consola inferior; no continúa con la fuente del sistema ni una interfaz sin marcos.
2. Se libera inicialización parcial ante fallos de targets, buffer, fuente o ventana; `fini` es idempotente y permite reintentar. La altura del cursor no consulta un font nulo; reiniciar el renderer restablece la identidad del marco cargado.
3. El harness ejecuta el renderer C++ real con backend GPU simulado: siete fallos de arranque, limpieza/reintento, nearest, coordenadas enteras, wrapped/fitted text y bloqueo de cambio de textura durante un frame. No prueba lectura real de RomFS ni nitidez en pantalla.

## Tipos de la forma en el HUD

1. Las insignias consumen tipos de la forma canónica real y validan su pertenencia a la especie. Una forma inválida no se sustituye silenciosamente por datos base; NONE, segundo tipo vacío o duplicado no producen otra insignia.
2. La prueba host recorre las 1084 especies y 609 formas, comprueba tipos importados y rechaza referencias cruzadas/desconocidas. Los nombres de las insignias se ajustan a sus límites.
3. Esto representa tipos canónicos de especie/forma; no implementa cambios temporales de tipo ni Teracristalización. La revisión visual sigue pendiente. Validación completa: npm test 35/35, paridad 126/126 y build ARM PASS; logs `build/form-hud-npm-test.log`, `build/form-hud-arm.log`, `build/form-hud-parity.log`.

## Texto del HUD y cursores

1. El HUD ajusta el nombre dentro del espacio anterior al nivel y coloca el género usando el ancho medido por Citro2D. Se elimina la estimación por bytes UTF-8; `BattleInfo.updateName` upstream también utiliza displayWidth. Los números de HP quedan acotados al marco.
2. Cursores de movimientos, sustitución tras captura y aprendizaje usan el tamaño realmente dibujado después del ajuste. El harness del renderer comprueba ancho sin reducción y con reducción; los guards comprueban la conexión al HUD.
3. Tests host, paridad y build ARM pasan. No se cierra GUI-14: tamaño mínimo de nombres largos, glifos y composición final requieren capturas en Azahar/Old 3DS.

## Texto de menús

1. Título, modos, carga, confirmación de borrado, historial y ajustes usan anchos explícitos dentro de cada ventana. Ajustes reserva una columna para valores.
2. El cursor de las listas toma el tamaño devuelto por el texto ajustado; se mantiene el filtrado nearest y las coordenadas de texto enteras del renderer.
3. Los guards de presentación comprueban esta conexión y la compilación ARM la integra. La comparación visual de textos largos y glifos en las dos pantallas sigue pendiente; ajustes e historial conservan sus limitaciones funcionales.

## Escala y caché de sprites

1. `drawAnchored` conserva escalas explícitas positivas, incluidas 1× y 2×. Cero solicita la adaptación automática existente; `main.cpp` utiliza esa ruta sin repetir una clasificación por wave.
2. La clave de caché de entrenadores incluye tipo y variante femenina; cambiar de variante recarga el asset indexado correspondiente.
3. El resolver de atlas comprueba especie y pertenencia de forma antes de construir rutas; no sustituye una forma inválida por el sprite base. Regresión sobre las 1084 especies y 609 formas canónicas, formas cruzadas, IDs inexistentes y ausencia explícita de forma. La resolución de clave no demuestra disponibilidad física de cada sprite.
4. Regresiones cubren selección de escala explícita/automática y el guard de identidad de caché. La adaptación automática de tamaños sigue siendo una política de presentación 3DS; no se declara equivalente a todo `Pokemon.getSpriteScale` upstream.

## Selección de recompensas en ambas pantallas

1. Citro2D y el input nativo comparten la selección: recompensa → miembro del equipo → movimiento, si el perfil importado de PP lo requiere. No se duplica el cursor oculto de QuickJS.
2. Táctil y botones usan los mismos rectángulos visibles. Se eligen movimientos existentes del destinatario, con nombre y PP; los elixires para todos los movimientos no exigen un slot.
3. Aplicación conserva los comandos transaccionales de recuperación/held del runtime. Un rechazo mantiene el selector y muestra su feedback; B vuelve al equipo o a las recompensas. El menú de cambio libera foco cuando ya no está disponible.
4. Pruebas de navegación y geometría cubren 0–4 movimientos, 0–3 recompensas, límites y overflow; los guards de main comprueban ownership y envío de recipient/slot. No sustituyen interacción real en Azahar.
5. No se dibuja el campo ni se cargan sprites de batalla detrás de la pantalla de recompensas. No se declara una mejora de FPS sin medirla.

## Pendiente

1. Verificar Classic de principio a fin, todas las reglas/effects, cuatro actores en dobles y callbacks de habilidades/items.
2. Cerrar ajustes, historial, idiomas y fidelidad de todos los submenús. Historial upstream (`RunEntry`/`saveRunHistory`) conserva una sesión completa y hasta 25 entradas; necesita identidad persistente de run y captura terminal verificable para evitar duplicados al cargar finales. La selección de recompensa/destinatario/movimiento ya usa un único estado nativo con QuickJS habilitado o deshabilitado; la revisión visual conjunta sigue pendiente.
3. Actualización desde consola con catálogo cargable y firma; compatibilidad y export/import completos de estados pendientes.
4. Audio, recursos residentes/VRAM, cargas de atlas y rendimiento en Old 3DS física.
5. Una revisión visual conjunta en Azahar cuando el conjunto de menús esté listo; no se solicita captura por cada cambio.

## Etiquetas originales de tipos

1. Importadas las 20 etiquetas del atlas `images/types_es-ES.png/json` del pin de assets, usadas por `starter-summary.ts::setupPokemonPermanentInfoContainer` upstream. No se confundieron con `type_bgs`, que contiene solo fondos.
2. `scripts/type_badges.py` valida tamaño, claves, rotación, bounds y canvas; genera `TypeLabels.hpp`, textura RGBA8 y `build/native-presentation/type-label-provenance.json` con hashes de PNG, JSON y conversión. El pipeline de presentación invoca el mismo importador.
3. HUD utiliza etiquetas de 32×14 a resolución original; el detalle de movimientos centra la etiqueta sin estirarla. Renderer conserva aspecto, aplica NEAREST, posee una única textura y la libera al cerrar. Si falta o es inválida, devuelve fallo y el presenter conserva su fallback textual anterior; esa degradación no demuestra fidelidad visual.
4. Harness renderer prueba las 20 etiquetas, case-insensitive, tamaño nativo/reducido, una carga reutilizada, ausencia de archivo, dimensiones inválidas y rechazo de tipos desconocidos. Índice canónico comprueba cobertura de tipos de todas las especies/formas. Verificador físico compara bounds y hashes del atlas con el header generado.
5. Solo español es-ES en esta adaptación; selección de idioma y comparación visual de todas las pantallas permanecen pendientes. Etiquetas de tipos en el HUD son adaptación 3DS: upstream usa iconos compactos en BattleInfo, por lo que no se declara paridad de esa composición.

6. Gates de esta etapa: `npm test` 35/35, `npm run native-parity` 126/126, build ARM ELF/3DSX y `git diff --check` PASS. Logs: `build/type-label-npm-test.log`, `build/type-label-parity.log`, `build/type-label-arm.log`, `build/type-label-media.log`. Regeneración doble conserva byte a byte header, reporte y textura. No se abrió Azahar ni se validó Old 3DS física.

## Iconos compactos de tipos en BattleInfo

1. Sustituidas las etiquetas textuales del HUD por las seis variantes originales `pbinfo_player_type`, `type1`, `type2` y sus equivalentes enemy, según `BattleInfo.setTypes` en el pin de juego. Las etiquetas es-ES se conservan en el detalle de movimientos.
2. `scripts/hud_type_icons.py` reutiliza la validación de atlas, convierte sin resampling y genera `HudTypeIcons.hpp`. `build/native-presentation/hud-type-provenance.json` conserva fuentes PNG/JSON, hashes, revisión, bounds, canvas y offsets. Todos los 120 frames permanecen accesibles.
3. Iconos a escala nativa 1×, posiciones enteras y NEAREST, en el borde izquierdo del jugador/derecho del enemigo. La fila interior queda disponible para el estado alterado y no comparte rectángulos de tipos. Posiciones adaptadas a 400×240; no se declara comparación visual upstream completa.
4. Renderer posee un máximo de seis hojas, con carga diferida y liberación al cerrar; rechaza tipos, slots, archivos y dimensiones inválidos. Sin sustitutos gráficos para iconos ausentes. La ausencia de asset no se interpreta como ausencia de tipo canónico.
5. Harness ejecuta los 120 frames contra renderer real con SDK simulado: escala 1×, variantes únicas/dobles, faltantes, dimensiones incorrectas, slots inválidos; el índice verifica los tipos de todas las especies y formas en seis variantes. Verificador físico compara PNG/JSON/header/t3x y hashes. Tests Python rechazan rotación, duplicados, keys inválidas, bounds y trim fuera del canvas.
6. Quedan pendientes atlas originales de estados, captura y otros indicadores; paneles HUD a 1,25×, barras fraccionarias, formas/tipos temporales y comparación conjunta en Azahar/Old 3DS. Los iconos no cierran esos criterios.

7. Gates de iconos HUD: `npm test` 36/36, `npm run native-parity` 126/126, build ARM ELF/3DSX, verificador físico y `git diff --check` PASS. Reimportación idéntica byte a byte para seis t3x, header y reporte. Logs `build/hud-icons-npm-test.log`, `build/hud-icons-parity.log`, `build/hud-icons-arm.log`, `build/hud-icons-media.log`; no equivalen a comparación visual ni prueba en hardware.

## Estados alterados y marca de captura originales

1. El HUD usa el atlas español `images/statuses_es-ES.png/json` para poison, toxic, paralysis, sleep, freeze y burn. Conserva los ocho frames originales, incluyendo pokerus/faint, cuya conexión al runtime sigue pendiente. Fuente: `BattleInfo.updateStatusIcon` del pin de juego.
2. La marca de captura usa el PNG físico `images/ui/icon_owned.png`, de 7×7, según `EnemyBattleInfo.constructor`; retirada la mini Poké Ball sintetizada con rectángulos. El estado conserva su fila bajo el nombre; la marca owned se desplaza 22 píxeles cuando hay estado para evitar superposición.
3. El importador HUD existente incorpora ambas hojas; reporta PNG/JSON/converted hashes y source path/symbol por familia. Para owned, canvas/frame se derivan de las dimensiones físicas del PNG, sin inventar un manifest upstream.
4. Renderer usa escala 1×, posiciones enteras y NEAREST. Cache acotada a ocho hojas HUD, carga diferida, rechazo de claves/hojas incompatibles/dimensiones inválidas y liberación verificable de todos los recursos. No dibuja una etiqueta sustituta cuando falta la hoja original.
5. Harness renderer ejecuta los ocho frames de estado y la marca owned, además de los 120 frames de tipos; prueba faltantes, claves cruzadas, dimensiones inválidas, filtros, escala y cleanup. Verificador físico coteja las ocho hojas con PNG/JSON/header/hashes. Un fallo inicial del harness identificó dimensiones simuladas incorrectas de owned (8×8); se corrigieron a las dimensiones reales 7×7, preservando el assert.
6. Quedan pendientes composición visual completa, Pokérus/faint, otros indicadores, paneles y barras de HUD, Azahar y Old 3DS física. No se declara paridad visual por pruebas con SDK simulado.

7. Gates: `npm test` 36/36, `npm run native-parity` 126/126, ARM ELF/3DSX, verificador físico y `git diff --check` PASS. Ocho t3x, header y reporte reproducibles byte a byte. Logs: `build/hud-indicators-npm-test.log`, `build/hud-indicators-parity.log`, `build/hud-indicators-arm.log`, `build/hud-indicators-media.log`.

## HUD y barras a resolución nativa

1. Paneles originales a escala 1×: jugador/enemigo 130 píxeles, jefe 178. Recalculadas posiciones con los orígenes upstream de PlayerBattleInfo/EnemyBattleInfo: HP jugador `(69,20)`, enemigo `(59,20)`, jefe `(69,18)`; EXP `(32,39)`. Ajustadas columnas de nombre/género/nivel/HP y fila de estado. El nivel se limita por el ancho disponible, incluyendo valores largos.
2. HUD jugador en `(258,146)`, termina en `(388,188)`, separado del banner de feedback que comienza en y194. Texturas compactas de tipos permanecen 1× y su posición sigue los orígenes upstream. Composición de doble pantalla y legibilidad pendientes de captura.
3. HP usa `overlay_hp`/`overlay_hp_boss` con sus frames high/medium/low; thresholds upstream >0,5 y >0,25. EXP usa `overlay_exp` original con patrón de píxeles. Se recorta el ancho, sin estirar la textura: floor del ancho visible, mínimo un píxel para una fracción positiva; es adaptación visual 3DS, no regla de batalla.
4. `project/data/assets/presentation-overrides.json` documenta la discrepancia pinned de overlay_hp_boss: JSON declara 96×12, PNG y frames son 86×12. Override requiere revisión y SHA-256 de ambas fuentes, conserva el JSON original y valida cada frame contra el PNG físico. Fuentes nuevas o diferentes fallan hasta revisar el override.
5. Retirados los ticks cyan inventados de jefe: divisores de un píxel dentro de la barra, posición según HP/segmento redondeado y color blanco/gris según índice, siguiendo `EnemyBattleInfo.updateBossSegmentDividers`; no modifican estado ni reglas del jefe.
6. Harness renderer verifica atlas, umbrales exactos, recorte sin scaling, cero/negativos/overflow, NaN/infinito, mínimo un píxel, archivo/dimensiones inválidos y liberación de 13 hojas. Tests de import validan override ligado a hash/revisión y preservación de metadata original. Verificador físico incluye 11 hojas HUD. Animación temporal HP/EXP, labels/números originales y comparación Azahar/Old 3DS siguen pendientes.

7. Gates de HUD nativo: `npm test` 36/36, `npm run native-parity` 126/126, build ARM ELF/3DSX, verificador físico y `git diff --check` PASS. Reimportación de 11 t3x/header/reporte idéntica byte a byte. Divisores probados con HP 1–100, 2–16 segmentos, límites y monotonicidad; redondeo al píxel se acota al ancho de la barra. Logs: `build/hud-bars-npm-test.log`, `build/hud-bars-parity.log`, `build/hud-bars-arm.log`, `build/hud-bars-media.log`. No se abrió Azahar ni se verificó Old 3DS física.

## Dígitos y etiquetas originales del HUD

1. Nivel y HP consumen `images/ui/numbers.png/json`, con dígitos y slash de 8×8 a escala 1×. Nivel reserva 8 píxeles adicionales por cada dígito sobre tres, siguiendo `BattleInfo.setLevelDisplay`; HP mantiene orden y alineación derecha según `PlayerBattleInfo.setHpNumbers`. No se emite el nivel como texto escalado ni HP con espacios añadidos.
2. Etiquetas `overlay_lv`, `overlay_hp_label`, `overlay_hp_label_boss`, `overlay_exp_label` proceden de `images/ui/text_images/es-ES/battle_ui/` en el pin de assets. Se conservan dimensiones nativas y transparencia, posiciones derivadas de los orígenes upstream, y datos del actor separados de presentación.
3. Se preserva también `numbers_red` y sus frames; conectar nivel capped al estado/política resueltos sigue pendiente. La importación y el harness de esa textura no equivalen a haber implementado la decisión de color.
4. Se reutiliza el importador HUD: 17 hojas y provenance por asset/PNG/JSON, con source path/symbol de juego. El parser admite keys numéricas/slash únicamente con permiso explícito para esos dos atlases; mantiene rechazo de paths, keys inválidas y bounds/trim fuera del canvas.
5. Renderer carga y reutiliza hasta seis hojas de dígitos/labels, sin interpolación ni estirado, y las libera al cerrar. Harness recorre 22 frames de dígitos y cuatro etiquetas, errores de clave/archivo/dimensiones, escala 1× y cleanup de 19 hojas del renderer. Verificador físico coteja las 17 hojas con las fuentes y hashes. Comparación visual, color de límite de nivel, nombres largos y animación temporal siguen pendientes.

6. Gates de dígitos/labels: `npm test` 36/36, `npm run native-parity` 126/126, ARM ELF/3DSX, verificador físico y `git diff --check` PASS. Dos importaciones conservan byte a byte 17 t3x/header/reporte. Logs: `build/hud-digits-npm-test.log`, `build/hud-digits-parity.log`, `build/hud-digits-arm.log`, `build/hud-digits-media.log`; no prueban legibilidad en Azahar ni Old 3DS física.

## Nombres largos del HUD sin reducir la fuente

1. `BattleHudPresenter` aplica la abreviación con punto de `BattleInfo.updateNameText` del pin de juego: retira símbolos ♂/♀ del nombre, conserva el símbolo de género aparte, elimina caracteres completos y whitespace final antes del punto. Nombre canónico/localizado no se modifica.
2. La adaptación recorta por codepoint UTF-8, preservando acentos y caracteres fuera de BMP; evita generar secuencias inválidas al acortar. Rechaza overlong encoding, surrogates UTF-8, código fuera de rango y secuencias truncadas. La política de espacio final reproduce JS trimEnd.
3. Renderer mide candidatos con C2D_TextFontParse/GetDimensions en un buffer independiente de 256 glyphs creado al iniciar; no limpia el buffer compartido de texto pendiente ni asigna memoria durante la abreviación. API admite output de hasta 256 bytes; HUD utiliza 128. No se reduce escala de nombre para caber.
4. Si el punto no cabe o el input es inválido, la función devuelve fallo y salida vacía. Ese fallo no se convierte en un nombre inventado. El buffer de medición es requerido: su fallo de asignación produce diagnóstico de arranque, cleanup y retry.
5. Tests puros verifican abreviación, UTF-8/emoji/acento, género, whitespace, capacidad pequeña y datos inválidos. Harness del renderer verifica anchura medida, tamaño constante, buffer independiente, límites inválidos y octavo fallo de inicialización. No prueban métricas de la fuente real ni legibilidad en Azahar/Old 3DS; esas comparaciones siguen pendientes.

6. Gates de abreviación: `npm test` 36/36, `npm run native-parity` 126/126, build ARM ELF/3DSX y `git diff --check` PASS. Harness confirma que la medición no limpia texto compartido ni asigna buffers por llamada, y libera ambos buffers al cerrar. Logs `build/hud-names-npm-test.log`, `build/hud-names-parity.log`, `build/hud-names-arm.log`; la comparación visual conjunta sigue pendiente.

## Color de nivel limitado por EXP en Classic

1. `FirstRunRuntime.experienceLevelCap()` expone la misma política `classicExperienceLevelCap` que ya utiliza el runtime para EXP y elegibilidad. Classic válido (waves 1–200) resuelve el cap; modos/waves no soportados devuelven 0. No se añadieron overrides ni reglas nuevas de experiencia.
2. El binding de main pasa ese valor resuelto al HUD del jugador. `hudLevelDigitAtlas` selecciona `numbers_red` cuando player level ≥ cap, según `PlayerBattleInfo.setLevelDisplay`; enemigos y cap sin resolver conservan `numbers`. HP sigue usando los dígitos normales.
3. Tests de proyección cubren debajo/igual/encima, enemigo y cap 0. Nuevo caso FirstRunRuntime comprueba el valor inicial y la fórmula upstream en las 200 waves Classic, con rechazo fuera de rango. No verifica otros modos ni overrides de desarrollo upstream.
4. Comparación visual y cambios de cap/colores durante partida en Azahar/Old 3DS siguen pendientes.

5. Gates: npm test 36/36 (FirstRunRuntime 51/51), native-parity 126/126, ARM ELF/3DSX y git diff --check PASS. Logs build/hud-cap-recheck.log, build/hud-cap-parity.log y build/hud-cap-arm.log. El primer intento detectó un namespace faltante en la nueva prueba; se corrigió sin alterar sus assertions.

## Fuentes nativas sin reducción fraccionaria

1. `prepare_pixel_fonts.py` genera 8/10/12/16 puntos desde la TTF de assets pinned, conserva whitelist y registra hashes, cellHeight, lineFeed y sheets. Cuatro fuentes y reporte resultaron byte a byte idénticos al repetir la conversión.
2. `NativeTextRaster` selecciona fuentes y múltiplos enteros. El renderer compensa `30/cellHeight` de Citro2D para que el transform final sea 1×/2×/etc.; un argumento API 1.0 no daba 1:1. Referencia: [font.c](https://github.com/devkitPro/citro2d/blob/master/source/font.c), [text.c](https://github.com/devkitPro/citro2d/blob/master/source/text.c).
3. Dibujo normal/wrapped, abreviación y fitted usan la misma métrica. Fitted baja a rasters nativos disponibles y abrevia al mínimo si no cabe; ya no aplica scale*=fit. Cursores usan lineFeed real en lugar de FINF.height, que permanece 26 en estas conversiones. Paginación de mensajes y comparación visual siguen pendientes.
4. Harness reproduce la normalización de Citro2D y verifica transform final entero, cuatro tamaños, Unicode, anchos/abreviación, 11 fallos de arranque más métricas inválidas, cleanup y retry. Policy comprueba 4096 tamaños contra todas las alternativas. Verificador físico comprueba alpha binario, hashes y métricas; no prueba legibilidad en consola.
5. Gates: npm test 36/36 (FirstRunRuntime 51/51), presentación 7/7, parity 126/126, ARM ELF/3DSX y diff-check PASS. Logs build/native-font-npm-test.log, build/native-font-parity.log, build/native-font-arm-final.log y build/native-font-media.log.
6. Las cuatro sheets A4 suman 2,097,152 bytes y los archivos 2,100,240 bytes. No se declara ahorro de memoria ni mayor FPS; compactación de sheets, perfil completo y comparación conjunta en Azahar/Old 3DS quedan abiertos.

## Compactación de fuentes A4

1. `compact_font` conserva el ancho/stride del atlas y recorta únicamente filas inferiores transparentes de una sheet. Mantiene píxeles, métricas y glyph IDs; ajusta tamaño, altura, número de filas y offsets FINF/CWDH/CMAP. Sheets múltiples permanecen intactas para conservar sus índices. Formatos/sections desconocidos, referencias inválidas/cíclicas y píxeles no transparentes a descartar fallan explícitamente.
2. Sheets de las cuatro fuentes: 2,097,152 → 294,912 bytes (288 KiB, reducción 85.94%). Archivos completos: 2,100,240 → 298,000 bytes. Comparación contra cuatro conversiones originales confirma 137 mappings Unicode por fuente, widths, métricas y bytes de píxeles retenidos sin cambios. Esto no mide memoria total ni FPS de Old 3DS.
3. Conversión repetida conserva cuatro archivos/reporte byte a byte. Tests Python: 7/7, con CMAP direct/table/scan, offsets, métricas, idempotencia, multi-sheet, unknown sections, ciclos y rechazo de pérdida de píxeles. npm test 36/36, FirstRunRuntime 51/51, parity 126/126, verificador físico y diff-check PASS; logs build/compact-font-*.
4. Makefile ahora reempaqueta RomFS cada vez que se solicita 3ds. Antes, modificar solo fuentes dejaba el 3DSX anterior. Dos empaquetados reales confirman los cuatro archivos compactados dentro del 3DSX; el segundo no recompila C++. El ELF no cambió; carga visual en Azahar y rendimiento en consola siguen pendientes.
5. Inspección de 1451 atlas por orientación: 524 front exceden 72px de altura y 17 back exceden 100px; alturas máximas 145/146. Incluyendo anchura, 543 atlas requieren adaptación. Esta medida corresponde al canvas, no necesariamente a tinta visible.

### Canvas Pokémon y escalas enteras

1. Override pinned explícito: front 112×72, back 200×100. `native_sprite_pixels.py` conserva PNG/metadata originales, reconstruye un canvas común por animación y redimensiona nearest una vez; empaqueta hasta cuatro páginas RGBA4 recortadas a dimensiones de potencia de dos. Política genera `NativeSpritePolicy.hpp`; importador rechaza reportes con pin/política/conversor obsoletos, referencias duplicadas o paths inesperados.
2. Presenter C++ usa 1×/2× automático estable por atlas; selección preserva proporciones y orígenes enteros. Rechaza canvas que exceden la política con diagnóstico explícito. El API conserva escalas explícitas existentes; los callers de combate Pokémon piden escala automática.
3. Verificador reconstruye todos los overrides: 2902 atlas, 189096 frames, 2918 texturas físicas, 543 adaptaciones. Valida hashes, nombres/duración/provenance, canvas, páginas y píxeles derivados; el parser C++ carga los 2902. Cinco tests del adaptador/parser pasan. Repetición produce el mismo reporte; reconversión independiente de Thundurus Therian front/back y Zygarde Mega produce texturas byte a byte idénticas.
4. Comparación visual completa, mensajes largos/paginación y medición de memoria/CPU/GPU en Old 3DS continúan pendientes. No se declara un aumento de FPS ni cierre de GUI-14 a partir de estos gates.
5. Gates ejecutados: npm test 37/37, cinco casos del adaptador/parser, ocho suites específicas de presentación, native-parity 126/126, verificador de media y compilación/empaquetado ARM PASS. Inventario repetido idéntico; metadata/texturas adaptadas de tres casos presentes físicamente en el 3DSX. Logs build/native-pixels-*; Azahar no se abrió para esta entrega.

### Páginas de diálogo nativas

1. `TextPageLayout` conserva caracteres UTF-8 y palabras entre páginas; limita el almacenamiento a doce líneas de 256 bytes y rechaza tamaños/mediciones inválidos. Cada línea se mide con el raster nativo elegido. `DialoguePresenter` prepara/cacha la página cuando cambia el mensaje; cambios de contexto reinician el banner.
2. Banner de progreso: dos líneas a 12 puntos, SELECT recorre páginas. Setup conserva su ruta anterior porque SELECT controla formas. Combate activo: doce líneas en el panel izquierdo, SELECT o toque en su pie. Ya no se generan vectores de líneas por frame ni se descarta el texto después de seis líneas.
3. El menú devuelve None al solicitar una página; el avance visual no ejecuta ataques/capturas/huida. Rectángulo táctil independiente de los cuatro comandos. Tests host comprueban Unicode, palabras largas, CR/LF, ancho/capacidad, páginas de dos/doce líneas, cambios/reset/caché y transform entero en el renderer.
4. Sigue pendiente tratar todos los textos de otros submenús, glyphs/locales completos, texto progresivo y prompts/audio de MessageUiHandler upstream. Referencia local pinned: src/ui/handlers/message-ui-handler.ts y battle-message-ui-handler.ts. Layout de Old 3DS explícito; no se declara paridad completa con esos handlers ni validación visual.
5. Gates: npm test 38/38, nueve suites específicas de presentación, native-parity 126/126, compilación y empaquetado ARM PASS. Logs build/dialogue-*; no se abrió Azahar en esta entrega. GUI-10 y GUI-14 permanecen abiertos.

### Cajas de texto y tipos en setup

1. `Renderer2D::drawTextBox` mide páginas con el raster nativo y dibuja únicamente si todo el contenido cabe. Rechaza ancho/líneas/tamaño/coordenadas inválidos y overflow sin dibujar parcialmente. Usa el mismo buffer de medición independiente y los orígenes enteros del texto normal.
2. Setup usa las etiquetas físicas es-ES 32×14 a 1× para tipos de especie/forma, y caja de dos líneas a 10 puntos para habilidad. Rewards ofrece tres líneas a 10 puntos para nombres. Si no caben, conservan fitted: no se considera cerrada la presentación íntegra de textos largos en todos los submenús.
3. Harness del renderer comprueba límites, ausencia de dibujos tras overflow, posiciones/escala entera y todos los nombres importados de habilidades con sus métricas simuladas. Estas métricas permiten probar el algoritmo; no demuestran el ajuste de cada nombre con los glifos físicos. Comparación visual y medición de los fonts reales siguen pendientes.
4. Gates ejecutados: npm test 38/38, nueve suites específicas, native-parity 126/126, build ARM/3DSX y diff-check PASS. Logs build/text-box-*; sin nueva validación visual en Azahar ni hardware.

# Estado de PokéRogue para Old 3DS XL

## Avance local de presentación y apariencias (sin validación nativa)

- Catálogo normal convertido: 3.112 atlas / 3.129 páginas `.t3x`; 210 identidades femeninas normales indexadas. Los 61.629.673 bytes corresponden al catálogo en disco, no a memoria residente.
- Catálogo shiny: materialización terminada exit 0 con 7.570 apariencias, cero ausencias y cero registros no soportados; staging completo iniciado, conversión e índice ampliado pendientes. Bouffalant aplica el fallback negro de `rgbHexToRgba` upstream.
- Submenú global: conserva la opción de origen al regresar desde las nueve rutas; prueba nativa escrita, sin ejecutar.
- Staging shiny: rechaza reportes parciales y exige coincidencia de identidad/hash PNG con el catálogo final y hash del reporte compatible con Python; ausencias pinned sin duplicados ni conflictos; guard JS PASS.
- Estadísticas: cuatro métricas de descubrimiento calculadas en gameplay a partir del perfil real y catálogo canónico; etiquetas pinned importadas. Los contadores históricos siguen pendientes y no se presentan como cero. C++ escrito sin ejecución.
- Discrepancia pendiente confirmada en slots de habilidades: el helper legacy interpreta ability2=NONE crudo como ausencia de slot 1, pero upstream lo normaliza a ability1 y reserva slot 2 para oculta. Corrección C++ escrita para default oculto, captura y forma compartida con UI; casos nativos añadidos sin ejecutar. Verificación y compatibilidad de saves pendientes. La ficha refleja hoy el resolver local, sin afirmar paridad.
- Habilidad de la ficha: consume slot por defecto desbloqueado del perfil y forma seleccionada mediante gameplay; comparación con actor escrita sin ejecución, selector manual pendiente.
- Selector: indicadores shiny originales, perfiles vistos/capturados/desconocidos, filtros; títulos/submenús/Pokédex y exportación/importación con confirmación tienen rutas C++ escritas.
- Sprites: caché de fallos por página del atlas y última identidad de entrenador para evitar I/O repetido cada frame; reset explícito permite recuperación. Guards estáticos PASS; comportamiento GPU no ejecutado.
- Fuentes: cobertura de glifos especiales, texto UTF-8 y rasters físicos comprobados. Cache de iconos y paginación reducen recorridos; rendimiento de hardware no medido.
- Gates permitidos: guards JS de menú e índice de apariencias, metadata de género pinned; tests Python de cobertura de glifos, indicadores y paletas: PASS. `git diff --check`: PASS.
- Compilación, suite nativa, Azahar y hardware aplazados por instrucción del usuario. No considerar completo Classic, todos los submenús ni la migración.


## Revisión del avance local

El avance se publica como trabajo en curso. Classic completo y la fidelidad visual final no están verificados.

1. PokéRogue: `8555c08c823b856cbec4eb99ca84ea52a955836d`.
2. Assets: `056a1f408f26a3be4fef243f7462cb43608c7928`.
3. Locales: `23aea1cb0da5a0b15b836f3c243791591cc42303`.
4. Hash canónico: `b1b5821edb39a088e641682ae5f5eae9567eef03ffbc2aa3a257c148d0106edf`.

## Trabajo actual sin compilación

La instrucción más reciente del usuario aplaza compilaciones y Azahar hasta nueva autorización. Los resultados históricos siguientes no validan estos cambios nuevos.

1. Selector: catálogo elegible con estados capturado/visto/desconocido, filtro de captura y costes importados; registro de observación de encuentros y evolución conectado al perfil. Validación nativa pendiente.
2. Perfil P3CANDY9 conserva atributos de apariencia observada/capturada y mantiene desconocidos en perfiles antiguos. Actor `pokemon=e` conserva apariencia explícita.
3. Partida v27 conserva apariencia del enemigo principal y equipo de entrenador; v26 migra sin inventar shiny. El equipo jugador exige snapshot si su apariencia está resuelta. Casos de codec/migración escritos, sin ejecución.
4. Resolver de apariencia inicial sigue `GameData.getSpeciesDefaultDexAttrProps` del pin: shiny capturado y variante más alta. Falta conectar generación, preferencias de selección y assets shiny completos.
5. Fuentes: la conversión actual rasteriza el TTF pinned en monocromo a tamaños nativos; reemplaza el antiguo umbral A4 que borraba trazos débiles. Comparación visual final pendiente.

6. Reemplazo e invalidación de atlas Pokémon/trainers retiran texturas hasta SYNCDRAW; un trainer sin mapping limpia el anterior. Guard estático JS PASS, sin validación nativa/GPU.

7. Materializador de variantes usa `_masterlist.json`, PNG/atlas y paletas de la revisión pinned, con provenance del shader y datos; cuatro pruebas Python PASS. Salida marcada `SOURCE_MATERIALIZED_NOT_YET_T3X`; falta staging, conversión e integración de runtime.

8. Submenú global C++ con nueve etiquetas originales pinned, navegación X/táctil/D-pad, panel inferior y overlay superior. Ajustes conectado; servicios restantes aún pendientes y explícitos. Guard estático JS PASS; casos C++ y comparación visual pendientes.

9. Pokédex básica conectada al submenú y perfil real: catálogo canónico, iconos 1×, estados capturado/visto/desconocido y páginas. Guard estático JS PASS; navegación C++ escrita sin ejecución. Filtros de generación y descubrimiento conectados a datos/perfil, información conocida de tipos/estadísticas base en pantalla superior. Filtros restantes, detalles completos, formas/shiny y paridad visual pendientes.

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

1. El método inicial de umbral A4 8/15 borraba trazos de algunos glifos. Fue sustituido por rasterización monocroma desde el TTF pinned, preservando avances y línea base; el ancho de bitmap se amplía cuando el hinting lo requiere.
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

### Confirmaciones y decisiones

1. Confirmar starters y evolución usa caja medida de dos líneas; movimientos reemplazables usan tres líneas a 10 puntos con cursor a la altura del primer renglón. Overflow conserva fitted y no demuestra que todos los nombres se muestren completos.
2. Feedback de setup y formas ahora tiene un ancho máximo explícito: no se dibuja fuera del panel por ausencia de restricciones. Sigue pendiente exponer detalles completos para los textos abreviados y revisar capturas.
3. Logo pinned físico 150×33: título cambia de 270px (1,8×) a 300×66 centrado (2×). No se modifica el PNG fuente. Comparación visual todavía pendiente.
4. Gates: npm test 38/38, nueve suites de presentación, native-parity 126/126, build ARM/3DSX y diff-check PASS. Logs build/decision-layout-*; Azahar no se abrió.

### Iconos de objetos a escala nativa

1. Recompensas pasa de iconos 36×36 (1,125×) a 32×32 centrados. Inventario de Poké Balls pasa de 24×24 (0,75×) a 32×32; las etiquetas/cantidades permanecen acotadas a sus columnas.
2. Las filas de Poké Balls ahora comparten `ballMenuRectangle` para dibujo/táctil. Antes usaban y=24+28i al dibujar y y=34+27i al tocar. Cinco rectángulos de 32px con separación de 2px caben en el panel sin invadir el footer.
3. Harness valida canvas/trim de los 528 frames del catálogo y todos los 76800 píxeles de la pantalla táctil contra solapamientos. Nueve suites específicas, parity 126/126, build ARM/3DSX y diff-check PASS; suite completa en ejecución. Logs build/reward-icons-* y build/ball-icons-*; comparación Azahar/Old 3DS pendiente.
4. Primera ejecución completa: 37 PASS/1 FAIL. Había cargado la invocación antigua del compilador antes de añadir el include del catálogo al harness: faltaba project/generated/include. Se corrigió el argumento; focused actualizado y reejecución completa 38/38 PASS. Resultado final en build/ball-icons-npm-test-final.log; se conserva el log del fallo anterior.

### Reloj de presentación

1. `main.cpp` antes calculaba timestamp=m_renderTicks*1000/60; a 30 FPS entregaba medio segundo de tiempo visual por segundo real. Ahora `svcGetSystemTick` y `SYSCLOCK_ARM11` de libctru convierten ticks transcurridos en milisegundos. Se comparte el timestamp visual existente, sin alimentar RNG, contenido o reglas de batalla.
2. `PresentationClock` divide antes de multiplicar el total de ticks; rechaza frecuencia cero y retroceso respecto al inicio. Harness compara diez segundos muestreados a 15/30/60 FPS, monotonía y truncamiento de máximo 1ms. Nueve suites específicas, npm test 38/38, native-parity 126/126, build ARM/3DSX y diff-check PASS.
3. EXP sigue avanzando por frame dentro del HUD; conectar el reloj es una dependencia previa, no un port completo de su tween. Pausa/suspensión, audio y capturas Azahar/Old 3DS aún pendientes. Logs build/presentation-clock-*.

### Identidad EXP y filtro de intro

1. El HUD identifica la caché EXP por pokemonId, conserva cero como valor y reinicia al título. Harness C++ dibuja dos actores de igual especie y comprueba cambio de identidad, EXP cero y reset. Mock C2D_Color32 ahora admite constantes como la API Citro2D.
2. BattleHudGeometry prepara duración/easing upstream; todavía no alimenta la animación. EXP continúa interpolando por frame: tramos de nivel, pausas y cadencias siguen abiertos.
3. Intro usa GPU_NEAREST; conserva escala 400x200 y crossfade de frames muestreados. No se declara vídeo completo ni escala entera.
4. Suite completa 38/38 PASS; nueve suites enfocadas posteriores, native-parity 126/126, compilación ARM/3DSX y diff-check PASS. Primer intento de suite EXP: 37/1 por mock no constexpr; se conserva build/exp-timing-npm-test.log. Logs finales build/exp-identity-*. Preview copiado a build/Pokerogue-pixel-preview.3dsx y proceso Azahar iniciado; capturas pendientes. El parámetro --user no aisló el perfil de esta versión: conserva configuración global, que puede aplicar suavizado adicional.

### Diagnóstico del preview Azahar

1. Capturas del usuario muestran deformación de letras/sprites; tamaño de Pokémon aprobado. Configuración global comprobada: resolution_factor=4, texture_filter=4, texture_sampling=0, filter_mode=true, escala entera desactivada. El filtro del emulador se aplica además de GPU_NEAREST del juego.
2. Instancia portátil preparada en build/azahar-pixel-runtime con user/config independiente. El log de arranque confirma Renderer_UseResolutionFactor=1, Renderer_FilterMode=false, Renderer_TextureFilter=None, Renderer_TextureSampling=NearestNeighbor y System_IsNew3ds=false. Configuración global conservada. Captura comparativa y alineación de cursor/HUD siguen pendientes; no se afirma que esto resuelva todos los defectos.

### Origen de tinta de fuentes

1. BCFNT tenía margen transparente superior de 9/10/12/16 px en la mayúscula C de las fuentes 8/10/12/16. El renderer ubicaba la celda completa en y; el cursor se centraba con lineFeed. El pipeline ahora extrae margen/altura visible desde el A4 pinned y genera NativeFontMetrics.hpp; no resamplea ni modifica glifos.
2. drawText, wrapped y fitted descuentan ese margen. Fitted usa las métricas del raster finalmente elegido. Cursor usa altura visible de mayúsculas; acentos/descendentes conservan su baseline relativo. Paginación mantiene lineFeed.
3. Pruebas de bounds A4, referencias inválidas/vacías, offsets del renderer y raster fitted pasaron. Preparación repetida produjo iguales hashes de metadatos/header; verificación de assets, native-parity y build ARM/3DSX pasaron. Suite completa 38/38 PASS. Comparación visual de offsets pendiente. Logs build/font-ink-*.

4. Usuario confirmó mejora visual al retirar filtro Azahar: tamaño de Pokémon y sprites aprobados; todavía observa fallos en letras. La corrección de origen de tinta se compiló después de esa confirmación y necesita captura específica.

### Preparación reproducible de preview

1. prepare_azahar_preview.py --portable copia ejecutable/librerías/plugins a build/azahar-pixel-runtime y escribe user/config. Rechaza rutas solapadas con instalación; preserva saves existentes y no copia configuración privada. Nueve pruebas Python y npm test 38/38 PASS. Verificador de presentación compara ahora tinta A4 real con provenance y NativeFontMetrics.hpp. Diff-check PASS. Logs build/portable-preview-*.

### Selección de starters y pantalla inferior

1. Fuente inspeccionada: PokéRogue pin 8555c08c823b856cbec4eb99ca84ea52a955836d, src/data/species-data-registry.ts, isStarter/getAllStarters; src/ui/handlers/starter-select-ui-handler.ts, filtros fitsGen/fitsCaught. isStarter usa starterCost: el catálogo conserva esa elegibilidad canónica y combina starterUnlocked, sin inventar una exclusión universal por etapa evolutiva.
2. Vista muestra solo desbloqueados. Filtro de generación Y/táctil recorre generaciones presentes y vuelve a Todas. Nueva partida y selección desde el equipo retiran el filtro anterior. Otros filtros upstream aún pendientes.
3. Pantalla inferior: cabecera/coste, selector generación, 18 iconos, seis slots, botones Formas/Jugar/Volver y pie con fuente nativa de 10 px. Grid/footer/filter comparten rectángulos de dibujo y input. Overload starterUnlocked(Species) reutiliza la política evitando búsquedas por dex redundantes; no cambia desbloqueos.
4. Suite completa 38/38 PASS y nueve suites enfocadas posteriores PASS; casos catálogo vacío, todos los gen IDs uint8, recorrido/cierre del filtro, equivalencia de overload y 76800 píxeles de táctil. Native-parity 126/126, compilación ARM/3DSX y diff-check PASS. Logs build/starter-filter-*. Capturas y presupuesto de rendimiento pendientes; EXP por tramos no implementada en este bloque.

### Submenú global: destinos explícitos (sin compilación)

1. Las nueve opciones conservan las etiquetas de locales pinned y comparten geometría de cursor/táctil. Ajustes y Pokédex abren sus vistas existentes.
2. Logros, estadísticas, huevos, gacha, gestión de datos, comunidad y cierre de sesión abren una pantalla que explica la integración pendiente. B regresa al submenú. Estas pantallas no fabrican contadores, huevos ni sesiones y no modifican el progreso.
3. Casos nativos de navegación añadidos, pendientes de ejecución. Solo guards estáticos JS y revisión de diff autorizados en este bloque; no se compiló ni abrió Azahar.

### Assets de apariencias: preparación e índice (en curso)

1. La preparación completa incluye `--appearances`; evita perder atlas derivados al regenerar el inventario. El staging verifica fuentes pinned y distingue SHA original de la imagen usada para convertir.
2. `generate_pokemon_appearance_index.mjs` genera referencias C++ solo tras verificar metadatos y cada página física convertida. Rechaza identidades duplicadas o incoherentes. Pendiente ejecutar tras finalizar la conversión activa; no implica catálogo shiny completo ni selección/runtime conectados.
3. Limpieza del cursor del menú utiliza el retiro diferido del renderer cuando se proporciona. Guard estático PASS; validación GPU pendiente de autorización para compilar y usar Azahar.

4. Índice de apariencias: pruebas JS aisladas PASS para determinismo, revisión pinned, identidad, rutas, SHA, duplicados y catálogo vacío. Se exige formato P3ATLAS1/2, número de páginas referenciadas y rutas idénticas a las construidas por el renderer; se rechazan páginas fuera de su capacidad actual. Estas pruebas no compilan C++ ni prueban la carga GPU. Conversión real aún activa.

5. Pruebas del índice ampliadas: P3ATLAS2 con rutas `-pN`, rechazo de rutas de hoja no paginada, metadatos truncados y página fuera de las cuatro admitidas por PokemonAtlasPresenter. PASS en JS; conexión de shiny al presenter pendiente, incluidas diferencias por género/formas de `PokemonSpecies.getBaseSpriteKey` upstream (no inferirlas de ser hembra).

### Elegibilidad visual por género (importer)

1. Fuente pinned: `src/data/pokemon-species.ts`, `PokemonSpecies.getBaseSpriteKey`, usa genderDiffs y excluye ciertas formas de mega/primal/gigantamax. No basta con que el actor sea hembra.
2. Importer extrae genderDiffs del constructor de especie; CanonicalModels conserva booleano o null para desconocido. No confundirlo con declaraciones posteriores de formas. Declaraciones no literales fallan claramente.
3. Test JS con generation-01.ts y SpeciesId del commit pinned: Bulbasaur false, Venusaur/Pikachu true; ausencia desconocida y valores inválidos rechazados. PASS. Catálogo de producción aún no regenerado, hash canónico sin cambio; consumo C++ y overrides por forma pendientes.

### Resultado de conversión de assets (sin build del programa)

1. Proceso de preparación terminó exit 0: 2905 atlas convertidos y 2921 páginas `.t3x`; 55255765 bytes de archivos de textura en total (no memoria simultánea). Inventario SHA-256 `8f42de9f7806a3b5ad9c75913bb3115bd2f31bd4281fc1f94e299ca5c1b7f44e`.
2. Índice generado con tres apariencias materializadas reales: Bulbasaur front variante 1, back variante 2 e Ivysaur front variante 1. Comprobados hashes de metadatos/páginas físicas. Regeneración byte idéntica: header SHA-256 `3c995e3490d2315c12f714f3005a06b296616a26db5e34cda7c247e3a0585f44`. No sustituye el catálogo shiny completo.
3. Reimportación canónica activa para publicar genderDiffs y generar tabla C++ separada que conserva el ABI Species existente. Conexión de apariencias a renderer y validación nativa/GPU aún pendientes. No se compiló el programa ni abrió Azahar.

### Metadata visual de formas

1. Importer de PokemonForm conserva genderDiffs como booleano/null mediante el mismo parser validado que especies. El proceso de importación actualmente activo cargó el módulo anterior: publicará especies, pero requiere importación posterior para esta nueva metadata de formas.
2. Prueba inicial que suponía todas las declaraciones falló. Inspección de los 609 fragmentos upstream demuestra 201 sin campo, incluidos charizard:mega_x/mega_y. Expectativa corregida por evidencia para exigir null al faltar y booleano al declararse; la ausencia se distingue de valor inválido.
3. Exclusiones visuales de getBaseSpriteKey y consumo C++ por forma siguen pendientes.

### Resultado de reimportación de metadata de especies

1. Importación terminó exit 0; dos ejecuciones pinned produjeron hash idéntico `0f3f4fc91c6b458f14b1b4f85c2d60923d24d1e1bfcd4155af3ddf08eff7d140`. Generada tabla SpeciesGenderVisual separada; catálogo conserva 1084 especies/609 formas. No compilación C++.
2. Cambios posteriores (metadata de formas y exclusiones visuales) aún no publicados: el siguiente import incorporará pokemon-species.ts como fuente pinned, preservará SHA/símbolo getBaseSpriteKey y resolverá enum keys. Prueba JS del parser de exclusiones PASS. Se detectó en revisión una dependencia de inicialización de metadata y se movió la creación de reglas detrás de ese helper.
3. El hash canónico cambió al añadir metadata. Compatibilidad de saves/paquetes vinculados al hash anterior requiere revisar la política de actualización; no se declara migración de esos datos verificada.

### Conexión de apariencias al presenter (sin compilar)

1. draw/drawAnchored resuelven facing y apariencia del actor; índices físicos shiny se consultan por identidad exacta. Metadata de género/formas/exclusiones se publica mediante el import activo (62 fuentes pinned), aún pendiente de terminar.
2. Apariencia legacy sin resolver conserva la ruta existente. Identidad inválida o asset faltante de una apariencia conocida produce diagnóstico NOT_YET_SUPPORTED_POKEMON_APPEARANCE, sin dibujar un normal/male como sustituto. Catálogo femenino normal y shiny completo siguen pendientes.
3. Guard JS de conexiones PASS; casos de lookup exacto/variantes inválidas añadidos al harness nativo y no ejecutados. Generador preserva ausencia del campo en canonical como null; en runtime reproduce el valor falsy del constructor upstream para formas que omiten genderDiffs. No compilar ni abrir Azahar hasta autorización.

### Materialización completa de apariencias (activa)

1. materialize_pokemon_appearance_catalog.py recorre árbol/masterlist del commit pinned; determina 7570 identidades front/back, formas, femenino y variantes declaradas. No usa un catálogo limitado a las especies de ejemplo.
2. Reutiliza materialize/apply_palette. Reporte catalog-report.json guarda hashes, materializados, MISSING_IN_PINNED_UPSTREAM, INVALID_IMPORT y NOT_YET_SUPPORTED_BY_IMPORTER por separado; un error git sobre un path existente permanece fatal. Sin timestamp.
3. Preparación completa ejecuta materialización antes del staging; pruebas de enumeración determinista/formas/género/variantes añadidas. Cinco tests Python PASS en ese checkpoint. Materialización ahora terminada: 7570/7570, cero ausencias/unsupported; conversión `.t3x` ampliada e índice/runtime para todo el catálogo pendientes. No compilar programa ni abrir Azahar.

### Reimportación visual completa de metadata (terminada)

1. Import de 62 fuentes terminó exit 0. Dos imports iguales: hash canónico `c797658f2c13c494bd83eeb1b6ea8177e9c25b73d09abe5d1c653b7655dadf2b`. Verificados 1084 registros species.genderDiffs, 609 forms.genderDiffs y 10 exclusiones getBaseSpriteKey con provenance real.
2. Regeneración en build/determinism/gender-runtime-content.hpp produjo bytes idénticos al header de producción: SHA `843be89458a9d82e8be128030fb5158f16daff5d57f0a21f6190493851cea12d`. No se compiló C++. Pruebas JS de metadata de género y exclusiones PASS; diff-check PASS.
3. Materialización de 7570 apariencias terminada; staging ampliado activo. Las tablas C++ y consulta del presenter están conectadas en código; todavía falta validar ejecución nativa/GPU, completar assets ampliados y selección. Compatibilidad de progreso previo vinculado a hashes antiguos sigue sin verificarse.

### Apariencia predeterminada de starters (sin compilar)

1. resolveStarterFromDex aplica nativeStarterDefaultAppearance cuando el perfil tiene caughtAppearanceAttr; genera appearanceResolved/shiny/variant del actor desde metadata capturada, sin consumir RNG. Legacy sin datos queda desconocido; datos declarados inválidos fallan.
2. Guard JS PASS. Casos nativos normal, shiny epic y legacy añadidos, comparando PID/Tera para impedir sorteos extra; no ejecutados por prohibición de compilar. Selección manual de variantes y normal/female assets completos pendientes.
3. Materializador completo continúa en proceso; no se afirma cobertura runtime completa ni validación visual.

### Auditoría de fuentes y medios físicos (sin compilación)

1. Renderer mantiene raster nativo entero, GPU_NEAREST, posiciones redondeadas y origen de tinta; inspección de drawText/drawTextWrapped/drawTextFitted no encontró un escalado fraccionario añadido. No se infiere que todos los layouts sean correctos.
2. pixel_font_tests.py: 12 PASS. verify_presentation_media.py: PASS, incluyendo cuatro fuentes con alpha binario/métricas/hashes físicos, cinco ventanas, 20 tipos y 17 hojas HUD, items/trainers/cinemática. No compila C++ ni lanza Azahar.
3. Comparación visual del texto y render en dispositivo pendiente. En aquel checkpoint la materialización continuaba activa; el resultado final posterior confirmó 7570/7570 sin faltantes ni unsupported.

### Resolver heredado: eliminación de verificación ficticia

1. PokemonSpriteResolver contenía seis ejemplos con hashes/dimensiones inventados, incluido SHA de una fixture, devueltos como exists:true. Retirados; el registro inicial queda vacío.
2. Registro requiere paths/hashes/dimensiones explícitos; no fabrica defaults. TEST_FIXTURE y physicalVerified:true del llamador fallan claramente; metadata registrada queda UNVERIFIED. Guard JS PASS y diff-check PASS.
3. No está conectado al índice físico real. Sus consumidores legacy (Adapter, AssetResolver, exportadores) aún contienen fallbacks que deben auditarse; el presenter C++ usa las tablas canónicas/índice separado ya documentados. No se ejecutó la suite completa ni compiló.

### Referencias de sprites desconocidas (corrección de modelo/adapter)

1. CanonicalModels conserva atlasPath/icon/atlas/frame y capacidades shiny/género/variantes como null cuando no se importaron. PokerogueAdapter elimina rutas sintetizadas y frameIndex cero; resolveSprite sin binding físico devuelve estado explícito NOT_YET_SUPPORTED_BY_JS_ASSET_BINDING.
2. Metadata suministrada explícitamente se conserva, incluido hasShiny:false. Guard JS PASS; diff-check PASS. Reimportación activa para publicar este cambio en el catálogo físico, sin compilar el programa.
3. Otros consumidores del resolver heredado y su conexión al índice físico siguen pendientes. El catálogo de 7570 apariencias ya terminó su materialización; staging y conversión ampliada pendientes.

### Recuperación de bordes en atlas de apariencias

El adaptador de padding resuelve el PNG materializado para variantes shiny, incluido género femenino, en lugar de sustituirlo por el PNG upstream. Conserva la procedencia de materialización y distingue el hash original, el materializado y el derivado con padding. Los originales ausentes en el checkout parcial se consultan mediante `git show` en la revisión fijada.

Verificación ejecutada: `python test/sprite_padding_source_tests.py` (2 pruebas offline) y resolución del PNG real `269-shiny-v0` de espalda con hashes coincidentes. La recuperación completa durante conversión, compilación C++ y comprobación visual permanecen pendientes. No se compiló ni se abrió Azahar.

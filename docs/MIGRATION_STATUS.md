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

1. `npm test`: **34 passed, 0 failed**, 34 suites. La suite FirstRunRuntime contiene **50 passing, 0 failing, de 50**. No se desactivaron casos ni se debilitaron assertions.
2. `npm run native-parity`: **126/126 PASS**. Compara el contrato C++ con JS; no prueba todo el upstream.
3. `npm run native-test`: compilación PASS; el ejecutor informa explícitamente que no ejecutó hardware/emulador.
4. Compilación directa `make -f Makefile.3ds 3ds`: ELF y 3DSX producidos con devkitARM. No equivale a validación en Old 3DS.
5. `python scripts/verify_presentation_media.py`: archivos físicos y tablas de presentación PASS; no prueba fidelidad visual ni efectos de objetos.
6. Regresiones de navegación, geometría, índice de iconos, guards de pausa/guardado y ownership QuickJS PASS. Los guards de main son comprobaciones estáticas, no interacción real en Azahar.
7. Logs reproducibles de esta revisión: `build/touch-settings-npm-test.log`, `build/touch-settings-parity.log`, `build/review-native-test.log`, `build/touch-settings-arm.log`, `build/window-style-media.log`. No se versionan binarios ni logs.

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
2. Cerrar ajustes, historial, idiomas y fidelidad de todos los submenús. La selección de recompensa/destinatario/movimiento ya usa un único estado nativo con QuickJS habilitado o deshabilitado; la revisión visual conjunta sigue pendiente.
3. Actualización desde consola con catálogo cargable y firma; compatibilidad y export/import completos de estados pendientes.
4. Audio, recursos residentes/VRAM, cargas de atlas y rendimiento en Old 3DS física.
5. Una revisión visual conjunta en Azahar cuando el conjunto de menús esté listo; no se solicita captura por cada cambio.

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
7. Logs reproducibles de esta revisión: `build/reward-final-npm-test.log`, `build/reward-native-parity.log`, `build/review-native-test.log`, `build/reward-arm-build.log`, `build/review-media.log`. No se versionan binarios ni logs.

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

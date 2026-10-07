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

1. `npm test`: **32 passed, 1 failed**, 33 suites. La suite FirstRunRuntime contiene **31 casos passing y 18 failing, de 49**. No se desactivaron casos ni se debilitaron assertions.
2. `npm run native-parity`: **126/126 PASS**. Compara el contrato C++ con JS; no prueba todo el upstream.
3. `npm run native-test`: compilación PASS; el ejecutor informa explícitamente que no ejecutó hardware/emulador.
4. Compilación directa `make -f Makefile.3ds 3ds`: ELF y 3DSX producidos con devkitARM. No equivale a validación en Old 3DS.
5. `python scripts/verify_presentation_media.py`: archivos físicos y tablas de presentación PASS; no prueba fidelidad visual ni efectos de objetos.
6. Regresiones de navegación, geometría, índice de iconos, guards de pausa/guardado y ownership QuickJS PASS. Los guards de main son comprobaciones estáticas, no interacción real en Azahar.
7. Logs reproducibles de esta revisión: `build/review-final-npm-test.log`, `build/review-native-parity.log`, `build/review-native-test.log`, `build/review-arm-build.log`, `build/review-media.log`. No se versionan binarios ni logs.

## Fallos abiertos de FirstRunRuntime

- `checkBattleFleeMechanicsAndRestrictions`: `10306`.
- `checkLegacyFirstRunRestore`: `408`.
- `checkDoublePlainAreaDamage`: `10376`.
- `checkDoubleSingleTargetSleepCheckpoint`: `10354`.
- `checkDoubleStatusResidualCheckpoint`: `10344`.
- `checkDoublePartialExperienceCheckpoint`: `10304`.
- `checkDoubleCheckpointRoundtrip`: `10283`.
- `checkExhaustedPpStruggleReplay`: `10222`.
- `checkStatusActionAdmission`: `9381`.
- `checkModifierRewardGenerationAndClaim`: `60`.
- `checkFirstRivalEncounterTraceability`: `11008`.
- `checkCanonicalBerryEffects`: `10364`.
- `checkCanonicalBerryGeneration`: `10263`.
- `checkFlinchTurnLifecycle`: `10236`.
- `checkBiomeTransitionProgression`: `86`.
- `checkPokeballCaptureMechanics`: `150`.
- `checkPlayerPartyManagementAndSwitching`: `167`.
- `checkInitialTeamFirstTurnRoundtrip`: `671`.

Los fallos incluyen restore/checkpoints, recompensas, bayas, capturas, cambios, dobles y una expectativa de huida. Su origen requiere distinguir regresiones de expectativas antiguas mediante upstream; no se declaran corregidos.

## Pendiente

1. Resolver los fallos anteriores y verificar Classic de principio a fin, todas las reglas/effects, cuatro actores en dobles y callbacks de habilidades/items.
2. Cerrar ajustes, historial, selección de destinatario y movimiento de recompensas en la ruta QuickJS, idiomas y fidelidad de todos los submenús. La ruta nativa y la ruta QuickJS no están completamente alineadas.
3. Actualización desde consola con catálogo cargable y firma; compatibilidad y export/import completos de estados pendientes.
4. Audio, recursos residentes/VRAM, cargas de atlas y rendimiento en Old 3DS física.
5. Una revisión visual conjunta en Azahar cuando el conjunto de menús esté listo; no se solicita captura por cada cambio.

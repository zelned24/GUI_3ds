# Estado actual y evidencia pendiente

## CÃ³digo existente, pendiente de validaciÃ³n final

1. Pins: PokÃ©Rogue 8555c08c823b856cbec4eb99ca84ea52a955836d; assets 056a1f408f26a3be4fef243f7462cb43608c7928; locales 23aea1cb0da5a0b15b836f3c243791591cc42303. Autoridad: tools/js/data/PokerogueSource.js.
2. Conteos, hash y provenance: project/data/pokerogue/import-report.json y canonical-content.json. No son lÃ­mites del engine.
3. Runtime limitado: encounters, RNG, daÃ±o, etapas, clima/crÃ­ticos/precisiÃ³n/PP y familias migradas. Self-target stat buffs y Trick Room conectados; inmunidades sonora/polvo con metadata upstream y callbacks desconocidos explÃ­citos.
4. Guardado v8, journal dual y export .p3save. Validator limitado a waves 1â€“9/bioma inicial; restaurar clima no neutral exige capacidades de clima soportadas en ambos actores.
5. ConversiÃ³n reportada de 2902 atlases a 2903 pÃ¡ginas t3x, incluida espalda de Thundurus Therian en dos pÃ¡ginas. Inventario fÃ­sico en build/upstream-assets y build/romfs; animaciones upstream 10 FPS. Hardware no verificado.
6. ContentUpdateStore y lecturas indexadas acotadas existen; catÃ¡logos siguen compilados. OTA completa no disponible.

## Ãšltima ampliaciÃ³n de combate â€” pendiente de ejecuciÃ³n

1. `HealAttr` constante: constructor importado â†’ `MoveHealProfile` generado â†’ `PokemonHealingEffect.hpp` â†’ turno real en `FirstRunRuntime.cpp` y score de IA. Fuente pinned: `src/data/moves/move.ts`, `HealAttr`; PP: `src/phases/move-phase.ts`, `usePP`; curaciÃ³n: `src/phases/pokemon-heal-phase.ts`, `getHealAmount`.
2. CuraciÃ³n propia con un Ãºnico atributo: redondeo base half-up, multiplicador de HealingBooster resuelto por policy y redondeado hacia abajo, lÃ­mite de HP, fallo a HP completo tras consumo de PP, Heal Block y cancelaciÃ³n previa diferenciados. USER cuesta un PP sin Pressure. Sin draws RNG.
3. El runtime actual conecta actores sin items/passives/tags, multiplicadores neutrales. Rest, VariableHealAttr, curaciÃ³n aliada y movimientos con efectos adicionales permanecen no soportados. Regresiones nativas escritas (400â€“410), sin ejecutar. Entrenadores continÃºan bloqueados.

4. `HitHealAttr` basado en daÃ±o: ratios constantes generados (incluido default 0.5), ataque+drenaje atÃ³micos en el turno y beneficio de IA. Fuente pinned `src/data/moves/move.ts`, `HitHealAttr`; daÃ±o aplicado segÃºn `src/phases/move-effect-phase.ts`. MÃ­nimo uno/floor, cap de HP y multiplicadores posteriores. No se ejecuta al fallar, inmunidad o daÃ±o cero.
5. Resolver de Liquid Ooze conserva reversiÃ³n, redondeo negativo de PokemonHealPhase y bloqueo de daÃ±o indirecto mediante policy; el flujo real todavÃ­a rechaza esta habilidad hasta conectar post-defend/indirect-damage completo. Strength Sap y atributos adicionales no se sustituyen por drain genÃ©rico. Regresiones 411â€“418 escritas, no ejecutadas.

6. `RecoilAttr` constante: ratios/defaults y `useHp/unblockable` generados; resolver C++ + ataques simples conectados atÃ³micamente al turno y score de IA. Rock Head/Magic Guard desde `BlockRecoilDamageAttr`/`BlockNonDirectDamageAbAttr` pinned. Fuente `src/data/moves/move.ts`, `RecoilAttr`, y `src/data/abilities/ab-attrs.ts`. Pruebas 419â€“426 escritas; sin ejecuciÃ³n. Struggle tiene perfil de retroceso preservado pero su ataque completo (typeless, target, fallback PP) sigue pendiente.
7. CaÃ­da simultÃ¡nea en el equipo actual de un solo PokÃ©mon no se marca como victoria; `FaintPhase` upstream lleva a GameOver cuando no quedan PokÃ©mon legales. Equipo completo, faint queue y estados de derrota/summary siguen pendientes.

## No completado

1. Equipos completos, todos los efectos/status/abilities/items, dobles y entrenadores jugables. battleInputSupported mantiene bloqueo de entrenadores.
2. Rewards, captura, mapa/biomas, progresiÃ³n hasta wave 200, combate final y resumen.
3. PresentaciÃ³n web adaptada a ambas pantallas, audio y rendimiento Old 3DS.
4. Save completo, migraciones de contenido y actualizaciÃ³n firmada desde consola.
5. EjecuciÃ³n final de pruebas, compilaciÃ³n 3dsx, Azahar y hardware.

## Limpieza aprobada por el usuario

1. Retirados servidor, HTML/CSS y editor/shell/preview web. Dependencias del pipeline trasladadas a tools/js, sin reconstruir el Studio.
2. Retiradas suites monolÃ­ticas antiguas del Studio; conservadas migraciÃ³n/RNG/battle/native. Nuevo ejecutor las registra sin modificar sus asserts; no se ejecutÃ³.
3. Documentos histÃ³ricos consolidados en este estado, lista de pendientes y contrato OTA; mapa semÃ¡ntico obsoleto retirado. Provenance sigue en los datos canÃ³nicos y cÃ³digo.
4. Fallos histÃ³ricos de validaciÃ³n del exporter y del preview pertenecen al Studio retirado, no se declaran arreglados. Suites actuales sin resultado validado todavÃ­a.
5. Historial Git, assets, clones upstream y runtime/presentaciÃ³n C++ permanecen. Plan y lista exacta de retiros disponibles en el commit de limpieza.

## Continuación autorizada: flujo de victoria de entrenadores

1. El usuario autorizó ampliar cambios a gameplay, almacenamiento y pruebas. Tests y compilación siguen aplazados.
2. El plan BattleEnd/rewards/next-wave se crea únicamente al derrotar todo el equipo enemigo, tanto en combate como al restaurar un checkpoint. Fuente pinned: `src/phases/victory-phase.ts`, `VictoryPhase.start`.
3. Regresión añadida en `test/native/first_run_restore_harness.cpp`: KO del primer miembro conserva wave, no ofrece recompensas y el avance de EXP envía la reserva. Pendiente de ejecución.
4. Esto corrige el límite entre KO individual y victoria del equipo; no habilita todavía la IA, entry effects ni todos los combates de entrenador.

## Clima: conexión del reloj de campo

1. `finishBattleTurn` consulta el resolver existente de duración de clima, siguiendo `src/phases/turn-end-phase.ts` y `src/data/weather.ts` pinned. Los cambios de ambos relojes de campo se preparan antes de publicar cualquiera.
2. El checkpoint captura weatherType/turnsLeft/maxDuration reales. La restauración no neutral permanece rechazada; no se neutraliza silenciosamente.
3. La expiración que solicita cambios de forma permanece explícitamente bloqueada hasta conectar ese resolver. Efectos residuales, post-weather abilities, movimientos que cambian clima y selección de clima del bioma siguen pendientes.
4. Regresión de correspondencia entre campo y save añadida; no ejecutada. No se declara P5 completo.

## Movimientos que cambian clima: datos y comando

1. Parámetros de `WeatherChangeAttr` generados desde metadata canónica real, con sourcePath/symbol/SHA-256. Snapshot actual: Sandstorm, Rain Dance, Sunny Day, Hail y Snowscape; sin IDs paralelos ni límite de catálogo.
2. `usePokemonWeatherChangeCommand` en PokemonBattleState prepara PP y clima de manera atómica: fallo por clima repetido/inmutable después de PP, bloqueo temprano sin PP, duración y coste de PP resueltos por el caller. Sin draws RNG ni asignaciones de heap.
3. Los callbacks de clima deben estar resueltos explícitamente antes de ejecutar el comando. El turno de FirstRunRuntime todavía no permite estos movimientos: faltan residual phase, post-weather callbacks y cambios de forma. Esta tabla y comando no se presentan como P5 completo.
4. Regresiones 427–434 con todos los registros reales importados, pendientes de ejecución.

## Clima conectado al turno para actores soportados

1. `PokemonWeatherPhase.hpp` conecta WeatherEffectPhase pinned para los cinco climas ordinarios: supresión del campo, inmunidad de tipos/formas, blockers de habilidades y daño indirecto; el HP de ambos actores se prepara antes de publicar los cambios. Omite residuals durante el interludio X0, siguiendo `PhaseManager.onInterlude`.
2. `FirstRunRuntime` ejecuta WeatherChangeAttr con duración inicial cinco (sin FieldEffectModifier en el estado actual), coste PP/Pressure, condición de clima repetido/inmutable y reloj de TurnEndPhase. La IA conserva el beneficio cero heredado de MoveEffectAttr; no inventa un bonus de utilidad.
3. Guardado/restauración conserva duración y tipo no neutral para actores soportados. Expiración a NONE se permite cuando no existen callbacks de forma/clima pendientes; en otros casos falla explícitamente.
4. Tabla generada de capacidades por cada ability ID canónico. Hooks de clima no migrados, como Rain Dish, Forecast e Ice Face, mantienen bloqueo; un ID desconocido nunca se trata como habilidad sin efectos.
5. Regresiones adicionales: daño residual real, inmunidad Rock, interludio, atomicidad frente a ID desconocido, restore de clima y expiración en un encuentro real. Todas pendientes de ejecución. No se ha compilado ni validado en Azahar.
6. Sigue pendiente extender los callbacks de clima, pasivas/items/tags, efectos de formas y selección de clima al entrar a nuevos biomas. Esta conexión no demuestra P5 completo ni Classic completo.

## Movimientos de alteración de etapas sobre oponentes

1. `PokemonStatStageEffect.hpp` y `PokemonTrainerAi.hpp` admiten los tres tipos de objetivos individuales hacia el oponente: `NEAR_OTHER` (p. ej. Sand Attack, Screech, Smokescreen, Flash, Charm), `NEAR_ENEMY` y `ALL_NEAR_ENEMIES` (Growl, Tail Whip, Leer, String Shot, Sweet Scent, etc.). La puntuación de IA (`calculateCanonicalStatStageStatusAiScore`) se habilita para estos movimientos sin sesgos ficticios.
2. `supportsPokemonStatStageMove` en `FirstRunRuntime.cpp` sustituye el filtro anterior exclusivo de usuario propio, ampliando la cobertura a 54 movimientos de cambio de etapas con un único `StatStageChangeAttr`.
3. `executeActiveBattleMove` resuelve atómicamente el hit check contra el oponente (incluyendo bloqueo previo por inmunidades sonoras y de polvo mediante `composePokemonAlwaysHitPolicy`), el coste de PP con `Pressure`, y la composición de políticas de habilidades del receptor y fuente (protección con `Clear Body`, reflexión con `Mirror Armor`, reacciones con `Defiant`/`Competitive` y copia con `Opportunist`).
4. Regresiones 441–450 en `pokemon_battle_state_harness.cpp` verifican: objetivos canónicos, puntuación IA, reducción efectiva de estadísticas de oponente, bloqueo por habilidad sonora (Soundproof), protección total (Clear Body), reflexión de caída (Mirror Armor) y coste con Pressure. Todas escritas y pendientes de ejecución según la instrucción del usuario.

## Desbloqueo y ejecución de combates de entrenador (P1)

1. `trainerBattleSupported()` en `FirstRunRuntime` verifica la validez estructural y operativa del equipo rival completo (1 a 6 integrantes con movimientos válidos y habilidades compatibles con el ciclo de vida de clima).
2. `battleInputSupported()` desbloquea la entrada del jugador durante combates contra entrenadores cuando `trainerBattleSupported()` es verificado, eliminando el bloqueo estático anterior.
3. El bucle de combate de entrenador resuelve turnos interactivos: selección de movimientos SMART por la IA enemiga (`selectSmartTrainerMoveSlot`), cambio táctico por ventaja (`resolveTrainerSwitchDecision`) donde el ataque del jugador impacta al entrante, debilitamiento del actor activo, concesión individual de experiencia (`grantVictoryExperience`), relevo ordenado hacia la reserva (`advanceTrainerAfterDefeat`), y transición a la fase de victoria al derrotar al último miembro.
4. Regresiones escritas en `test/native/first_run_restore_harness.cpp` (`checkTrainerInteractiveBattle`): comprueban que el combate de entrenador de wave 5 es interactivo, ejecuta comandos de ataque, rota la reserva al debilitarse el primer Pokémon y progresa a la siguiente wave tras vencer al equipo completo.

## Recompensas de victoria y modificadores canónicos (P2)

1. `generateVictoryRewards()` en `FirstRunRuntime` consulta `rollPlayerModifierReward` con la provisión canónica `InitialClassicRewardWeights` y el flujo pseudoaleatorio determinista de la wave, generando hasta 3 opciones de modificador de `ModifierPool`.
2. Interfaz y controles de recompensas: navegación entre las 3 opciones con UP/DOWN, confirmación/reclamación con A (`claimRewardChoice()`) y descarte/salto voluntario con B (`skipVictoryReward()`).
3. La reclamación de consumibles aplica atómicamente los efectos correspondientes (restauración de HP para Potion, Super Potion, Hyper Potion, Max Potion y Full Restore; recuperación de PP para Ether, Max Ether, Elixir y Max Elixir) antes de avanzar a la siguiente wave.
4. Regresiones escritas en `test/native/first_run_restore_harness.cpp` (`checkModifierRewardGenerationAndClaim`): verifican la generación de 3 opciones canónicas tras vencer un combate, la navegación bidireccional y la aplicación/reclamación con progreso a la wave siguiente.

## Transición de biomas y progresión por segmentos (P3)

1. Conexión de `PokerogueBiomeTransition.hpp` en `resolve()`: cada 10 waves en Classic (`(wave - 1) % 10 == 0`), se invoca `resolveClassicNextBiome` con las conexiones canónicas (`BiomeLinks`), determinando el destino mediante la semilla de wave completada.
2. Actualización de contexto: el identificador de bioma de la run (`m_run.biomeId`) y su localización de texto (`m_context.biomeName`) se actualizan dinámicamente con el bioma entrante.
3. El clima inicial del nuevo bioma se evalúa e inicializa mediante `selectPokemonBiomeWeather` y el validador de ciclo de clima en los actores.
4. Regresiones escritas en `test/native/first_run_restore_harness.cpp` (`checkBiomeTransitionProgression`): verifican que al superar la wave 10 en Town y avanzar a la wave 11, el runtime conmuta deterministamente al bioma conectado correspondiente según la tabla canónica.

## Combates dobles y resolución multitarget (P4)

1. `doubleBattleSupported()` en `FirstRunRuntime` valida la presencia y viabilidad de ambos oponentes (`enemy` y `secondEnemy`), compatibilidad del ciclo de vida de clima y PP de movimientos.
2. Selección y alternancia de objetivo: `cycleTarget(int direction)` conmuta interactivamente `m_selectedTarget` entre 0 y 1 mediante los controles L/R o las acciones 202/203 en QuickJS, forzando automáticamente el oponente con vida si uno se debilita. Marcador de objetivo `>` visible en la pantalla superior.
3. Orden de turno multi-actor canónico en `advanceBattleTurn`: cálculo de velocidades efectivas (con reversión bajo Trick Room), prioridad de ataques y desempate determinista mediante Fisher-Yates shuffle con semilla derivada `waveSeed + turn * 1000 + activeCount`.
4. Ejecución multi-actor de movimientos: `executeActiveBattleMove(userIndex, targetIndex, ...)` resuelve ataques individuales y de área (`ALL_NEAR_ENEMIES`, `ALL_ENEMIES`, `ALL_OTHERS`), redirigiendo ataques simples si el objetivo inicial fue debilitado en el mismo turno.
5. `applyPokemonMultiWeatherPhase` procesa el daño residual de clima simultáneamente sobre los tres combatientes activos.
6. Experiencia de victoria compartida: `grantVictoryExperience` recompensa la derrota de ambos Pokémon enemigos al concluir el combate doble.
7. Regresiones escritas en `test/native/first_run_restore_harness.cpp` (`checkDoubleBattleTargetingAndMechanics`): comprueban detección de combates dobles, rotación de objetivos y ejecución de turnos.

## Jefes mayores y combate final de wave 200 (P5)

1. `PokerogueEncounterResolver::resolveBoss`: implementa el algoritmo upstream de selección de jefes de bioma con tirada 0–63 (boss, boss_rare, boss_super_rare, boss_ultra_rare), descensos de tier y validación de LegendLike según la wave.
2. `PokerogueEncounterResolver::bossLevelForWave`: aplica la fórmula de nivel de jefes (`baseLevel * 1.2` con compensación gaussiana acotada), fijando nivel exacto 200 en la wave 200.
3. Wave 200 FinalBoss: genera el encuentro singular contra Eternatus (dex 890, nivel 200) en el bioma "end".
4. `ClassicVictoryPlan` y GameClear: al derrotar a Eternatus en la wave 200, `planClassicVictory` genera la etapa `ClassicVictoryStep::GameClear`, concluyendo la run con feedback "Game Clear! Classic run completed".
5. Regresiones escritas en `test/native/first_run_restore_harness.cpp` (`checkWave200FinalBossAndGameClear`): comprueban clasificación, nivel fijado 200 y plan de victoria GameClear sin wave subsiguiente.

## Compatibilidad de entrenadores fijos y variantes de género (P6)

1. `FirstRunRuntime::resolve` desacopla la inicialización de plantillas de combate fijo (`selectTrainerPartyTemplate`) del indicador de variante binaria de género, admitiendo tanto entrenadores con sorteo de género (Youngster) como entrenadores fijos sin sorteo (Rivales 1 a 6).
2. Generación determinista de equipos de rivales mediante `resolveSimpleTrainerPoolMember`, learnsets canónicos, ponderación de movimientos y cálculo de IVs por wave.

## Validación de guardado extendida (P7)

1. `validateNativeRunSave` en `NativeRunSave.cpp`: ampliado el rango de waves permitidas de wave 1–9 a `wave <= PokerogueContent::kClassicFinalWave` (wave 200).
2. Validación de bioma: verificación contra el registro canónico completo (`findBiomeById`), admitiendo todos los biomas visitables a lo largo de Classic.
3. Regresiones escritas en `test/native/first_run_restore_harness.cpp` (`checkExtendedWaveAndBiomeSaveValidation`): verifican saves válidos en waves 1, 50 y 200 en biomas variados, y rechazo estricto de wave 201 o biomas no registrados.

## Captura de Pokémon y gestión de Poké Balls (P8)

1. `PokemonCapturePhase.hpp`: Implementación canónica completa de ratios de captura para todas las especies registradas (`kSpeciesCatchRates` con 1026 entradas fijas y mapeo de alta dex para formas regionales e introducciones tardías), multiplicadores de Poké Balls (`PokeballType`: Poké Ball 1.0, Great Ball 1.5, Ultra Ball 2.0, Rogue Ball 3.0, Master Ball garantizada), fórmula Gen 6 / upstream de ratio modificado (`modifiedCatchRate = round((3*maxHp - 2*hp) * catchRate * ballMultiplier / (3*maxHp))`), probabilidad de sacudida (`shakeProbability = round(65536 / ((255 / modifiedCatchRate)^0.1875))`) y 3 comprobaciones pseudoaleatorias deterministas con `PokerogueRngAdapter`.
2. Bloqueadores canónicos: bloqueo estricto en combates contra entrenadores (`CaptureBlocker::TrainerBattle`), combates dobles con ambos rivales vivos (`CaptureBlocker::MultipleEnemies`), objetivo debilitado (`CaptureBlocker::TargetFainted`), escudos de jefes (`CaptureBlocker::BossShieldActive`) y jefe final de wave 200 (`CaptureBlocker::FinalBossUncatchable`).
3. Gestión de inventario en `FirstRunRuntime`: provisión inicial de 5 Poké Balls estándar al arrancar una run; recarga dinámica de Poké Balls, Super Balls, Ultra Balls, Rogue Balls y Master Balls al reclamar modificadores canónicos tras la victoria.
4. Lanzamiento interactivo de Poké Ball (`throwPokeball`): deduce la bola del inventario, evalúa el intento de captura, incorpora inmediatamente al Pokémon capturado al equipo si hay cupo disponible (< 6 miembros), restaura sus PS a tope y limpia estados temporales, o ejecuta los contraataques del turno enemigo si el Pokémon escapa de las sacudidas.
5. Regresiones escritas en `test/native/first_run_restore_harness.cpp` (`checkPokeballCaptureMechanics`): comprueban multiplicadores, ratios canónicos, bloqueos en entrenador/final boss/dobles/fainted, captura garantizada con Master Ball y captura exitosa en combate salvaje con reducción de inventario e incorporación a la party.

## Gestión de equipo (Party de 6 integrantes) y relevo de combate (P9)

1. `PresentationContext` y `FirstRunRuntime`: soporte completo de party de jugador con `playerParty[6]`, `playerPartyCount` (iniciando en 1 con el inicial elegido) y `activePlayerPartyIndex`.
2. Relevo voluntario en combate: `switchPlayerPokemon(uint8_t targetIndex)` conmuta el Pokémon activo con cualquier miembro de reserva con PS > 0, reinicia modificadores de características temporales para el entrante y procesa los ataques del oponente contra el nuevo combatiente activo.
3. Relevo forzado por debilitamiento: `advancePlayerAfterDefeat()` detecta cuando el combatiente activo cae a 0 PS y envía automáticamente al primer suplente con vida, permitiendo continuar el combate sin decretar derrota si aún quedan aliados vivos.
4. Evaluación de derrota del equipo: `playerPartyDefeated()` certifica si la totalidad de los miembros del equipo han sido debilitados antes de declarar el fin de la run.
5. Persistencia y sincronización continua: el miembro activo `playerParty[activePlayerPartyIndex]` se sincroniza automáticamente ante cambios de PS, subidas de nivel por experiencia obtenida y curaciones por ítems de recompensa, manteniéndose a través de las transiciones entre waves.
6. Regresiones escritas en `test/native/first_run_restore_harness.cpp` (`checkPlayerPartyManagementAndSwitching`): comprueban inicialización con 1 miembro, crecimiento a 2 tras captura, persistencia entre waves consecutivas, alternancia interactiva entre activo y reserva, y detección de derrota.

## Aprendizaje de movimientos y evolución por nivel (P10)

1. `PokemonEvolutionPhase.hpp`: Implementa el resolver canónico de evoluciones por nivel `checkSpeciesLevelEvolution` consultando `kSpeciesEvolutions` (origen, umbral `level > 1`, y salto de `oldLevel` a `newLevel`), `learnNewLevelMoves` para aprender movimientos disponibles del learnset canónico (`kSpeciesLevelMoves`) en huecos vacíos (< 4) asignando PP máximo/actual, y `applySpeciesEvolution` para transformar la especie, reinicializar las características de combate según las estadísticas base de la forma evolucionada, preservar la ganancia de PS máximos, PS actuales, PP e IVs, y actualizar la identidad visible del Pokémon.
2. Integración en `FirstRunRuntime::grantVictoryExperience`: al subir de nivel tras un combate ganado, el Pokémon activo aprende movimientos de su learnset correspondientes a los niveles alcanzados y comprueba si alcanza el nivel de evolución canónico (ej. Bulbasaur → Ivysaur al 16, Ivysaur → Venusaur al 32, Charmander → Charmeleon al 16, etc.), actualizando atómicamente la identidad y el equipo en `playerParty`.
3. Regresiones escritas en `test/native/first_run_restore_harness.cpp` (`checkLevelUpMoveLearningAndEvolution`): comprueban detección de evoluciones canónicas por nivel, rechazo si el nivel es insuficiente, aplicación de evolución con incremento coherente de estadísticas/PS máximos, y aprendizaje de nuevos movimientos de nivel con PP asignado.




## Correcciones tras revisión de 8d80d88

1. QuickJS acepta captura 210 y los seis slots 211–216. B captura; Select abre equipo, Up/Down selecciona, A cambia y B cancela; Left/Right cambia objetivo en dobles. L/R conservan save/load.
2. Respuestas tras captura fallida/cambio reutilizan el selector SMART/SMART_RANDOM del turno normal y comprueban resultados. Comandos completos aplicados sobre candidato y publicados solo en éxito. Reinicio restablece inventario inicial.
3. Catch rates manuales retirados: generación desde upstreamRawRecord canónico para todas las especies con sourcePath/symbol/SHA-256; desconocidos no reciben 45 arbitrario.
4. Save v8 rechaza estados que no puede serializar: dobles, equipo capturado, inventario modificado, waves posteriores a 9 y entrenador distinto de wave 5. Guarda temporal; persistencia extendida sigue pendiente.
5. Whitespace corregido. Regresiones de catálogo completo y rechazo de cambio inválido añadidas, no ejecutadas.
6. Pendientes: save completo, fases Eternatus/Eternamax, GameClear/profile, fidelidad completa de captura y orden de respuestas dobles. No se declara Classic completo.

## Evolución y aprendizaje: correcciones de fidelidad

1. Aprendizaje compara niveles como uint16_t; no trunca 128+ a int8_t. Prueba de aprendizaje real hasta 128 añadida, pendiente de ejecución.
2. Solo evoluciones de nivel sin item, condición ni cambio de forma pueden usar el resolver simple. Capacidad generada desde edge canónico y sourceOrder; no se descartan las demás evoluciones del catálogo.
3. Elegibilidad tras subir de nivel admite Pokémon que ya superaron el mínimo, siguiendo Pokemon.getEvolution/validate; no exige cruzar exactamente el umbral.
4. Inicialización de la especie evolucionada usa forma de destino y slot de habilidad correspondiente; conserva HP/PP/stages. Referencias de presentación/form/assets actualizadas.
5. Pendientes: elección/cancelación de evolución, condiciones/items/formas, reemplazo cuando hay cuatro movimientos, identidad de habilidad ambigua y soporte de niveles superiores a 100 en el estado de batalla/EXP/save. No se declara P10 completo.

## Niveles tardíos de Classic

1. Eliminado el techo local de 100 en inicialización de BattleState y concesión de EXP. BattleScene.getMaxExpLevel pinned permite cap 200 en wave 200; la fórmula de Pokemon.calculateStats es la misma por encima de 100.
2. Cálculos de stats usan uint32_t antes de publicar uint16_t. Stats fuera del rango de almacenamiento fallan con InvalidStatRange, sin overflow silencioso. No es una limitación de catálogo ni un cap de gameplay de 100.
3. Regresiones 460–463: actor real nivel 200, HP según fórmula, EXP 199→200 y rechazo atómico de stats demasiado grandes. Pendientes de ejecución.
4. La progresión/save completa sigue pendiente; aceptar nivel 200 no demuestra combate final, fases Eternamax ni Classic completo.

## Turnos dobles: errores y clima

1. advanceBattleTurn publica el candidato solo si el turno completo tiene éxito. Un fallo conserva estado de combate y RNG; solo cambia feedback.
2. Las cinco rutas de ataque doble comprueban el resultado. Velocidad usa pokemonWeatherEffectiveSpeed; política de clima incluye al tercer actor vivo, además de atacante y objetivo.
3. Scope RNG inválido y overflow del offset de turno fallan explícitamente.
4. Regresión 217–220 recorre encuentros reales y comprueba HP/PP/RNG de turnos rechazados. Escrita, pendiente de ejecución. No demuestra fallos inyectados a mitad del turno.
5. Pendientes: dobles 2vs2 completos, semántica de ataques de área/PP, orden de respuestas tras captura/cambio, save extendido y fases Eternamax. Classic sigue incompleto.

## PP de movimientos de estado de área

1. Fuente inspeccionada: PokéRogue pinned, src/phases/move-phase.ts, MovePhase.usePP y getActiveTargetPokemon. Un gasto base más IncreasePpUsedAbAttr de cada objetivo activo, una vez por movimiento.
2. Ruta doble de StatStageChangeAttr calcula el coste conjunto antes de ejecutar objetivos. El segundo objetivo usa modo ignore-PP resuelto, incluso si el primero agotó los PP. No se cobra de nuevo por objetivo.
3. pokemonActiveTargetsPpCost reutiliza kPpAbilityProfiles importados; referencias desconocidas fallan sin publicar coste. El resolver de un oponente delega en él.
4. Regresiones 464–467 escritas para dos Pressure, un objetivo, lista vacía e IDs inválidos; modo ignore-PP de stat stages ya tiene regresión. Pendientes de ejecución, sin compilación.
5. Daño de área, 2vs2 y efectos posteriores completos siguen pendientes. Esta corrección no los declara implementados.

## EXP individual del equipo

1. Pokemon.exp upstream (src/field/pokemon.ts, constructor y addExp) pertenece al Pokémon. Retirado acumulador único del runtime: ResolvedPokemon conserva totalExperience y la copia del miembro activo transporta su EXP en cambios y reemplazos por derrota.
2. Concesión de EXP, replay y guardado del actor activo leen el valor individual. Captura inicializa EXP desde growthRate y nivel canónicos; datos inválidos rechazan la captura completa sin publicar el candidato.
3. Regresiones 221–223 comprueban EXP de capturado, cambio y retorno al starter. Escritas, pendientes de ejecución.
4. Distribución de EXP entre participantes/EXP Share y persistencia del equipo completo siguen pendientes. La corrección no declara esas capas completas.

## Captura: HP y fallos de progresión

1. Fuente pinned inspeccionada: AttemptCapturePhase.addToParty/removePokemon y EnemyPokemon.addToParty; constructor Pokemon copia dataSource.hp. Capturado conserva HP y PP; retirada del enemigo ocurre después de copiarlo. Eliminada curación a máximo inventada.
2. Captura comprueba concesión de EXP y plan de victoria. Un fallo rechaza el candidato entero: no publica balón gastado, equipo ni victoria parciales.
3. EXP de dobles rechaza segunda especie/forma inválida, cálculo no soportado y suma fuera de uint32_t, sin omitir silenciosamente al segundo enemigo.
4. Regresiones 224–225 escritas para captura a 1 HP y retirada del enemigo. Pendientes de ejecución. Estado/efectos de captura, EXP por participante y equipo lleno siguen pendientes.

## Transiciones y limpieza de arena

1. Reclamar y saltar recompensa ejecutan sobre candidato: si no resuelven el siguiente encuentro, conservan wave, recompensa, inventario y estado del equipo anteriores. Solo feedback puede cambiar.
2. BattleScene.doPostBattleCleanup y ReturnPhase.resetSummonData pinned justifican reinicio de stat stages y Trick Room al entrar en entrenador, nuevo bioma o jefe final. Se sincroniza la copia del miembro activo; HP/PP/EXP persisten.
3. Regresiones 226–227 escritas para rechazo de transición fuera de victoria sin alterar snapshot. No cubren aún fallo inyectado de generación en una transición válida. Tests/compilación pendientes.
4. Callbacks PostBattleInit, tags, Tera, formas y biomas/clima completos siguen pendientes; no se declara limpieza upstream completa.

## Entrenadores aleatorios: construcción compartida

1. Fuente pinned: BattleScene.handleNonFixedBattle/generateNewBattleTrainer. Tras seleccionar trainer pool, género y plantilla continúan en el RNG de wave; fixed getTrainer usa scope wave<<8.
2. Selección aleatoria simple conecta con la construcción canónica existente de equipos/actores/moves/IVs, en lugar de detenerse incondicionalmente antes de construir. No duplica catálogo ni crea especies ficticias.
3. Configs hasDouble/doubleOnly siguen rechazadas explícitamente: falta el resolver completo de variantes. Capacidades de constructor y movimientos continúan validándose por miembro; esta conexión no prueba todos los entrenadores jugables.
4. Si la decisión de entrenador es negativa, la wave sigue la ruta wild y su selección de dobles. Validación dinámica/tests/compilación pendientes.

5. Regresión 228–229 añadida al recorrido de seeds/encuentros: si alcanza un entrenador aleatorio resuelto, comprueba equipo/IVs/moves y actor activo. Pendiente de ejecución; no demuestra cobertura de entrenador aleatorio si el recorrido no lo alcanza.

## Slots característicos de entrenadores

1. TrainerConfig initFor* registra setPartyMemberFunc(-(s+1), getRandomPartyMemberFunc(...)); getRandomPartyMemberFunc resuelve especie/evolución antes del constructor. Fuentes pinned inspeccionadas en src/data/trainers/trainer-config.ts.
2. Constructor de equipos llama al resolver existente resolveTrainerSignatureMemberSpecies para los slots finales canónicos en orden inverso. Mantiene el scope RNG por miembro durante selección y construcción del actor.
3. Los slots normales de un equipo mixto conservan el resolver de pool simple; este rechaza slots signature para impedir aplicar una selección de pool incorrecta.
4. Regresión 230–231 recorre todos los trainers con metadata signature soportada y verifica IDs reales de los slots finales. Escrita, no ejecutada. No prueba aún combate completo ni todos los callbacks especiales del entrenador.

## Segmentos sameSpecies de entrenador

1. Trainer.genPartyMember pinned consume genNewPartyMemberSpecies antes de sustituir por la especie del primer actor del segmento. getTrainerSpeciesForLevel permite prevolución requerida y desactiva evolución ascendente en esta sustitución.
2. Resolver de pool conserva esos sorteos y aplica la sustitución usando segmentStart canónico y previousSpecies. Ya no rechaza todos los segmentos sameSpecies. Balanced sigue requiriendo contexto real de formas/tipos.
3. Regresión 232–233 recorre plantillas del catálogo real y exige alcanzar al menos un slot sameSpecies resuelto. Comprueba especie según el primer miembro del segmento. Escrita, pendiente de ejecución; parity RNG completo sigue pendiente.

4. sameSpecies compuesto continúa bloqueado: upstream usa el threshold del template padre, que la tabla actual no representa por separado. No sustituirlo por el threshold del segmento.

## Umbral padre de templates compuestos

1. TrainerPartyCompoundTemplate llama super(totalSize, AVERAGE); TrainerPartyTemplate usa NORMAL por defecto. Tabla runtime conserva parentEvolutionThresholdKindId separado de los segmentos, derivando el ID NORMAL del catálogo importado.
2. sameSpecies compuesto usa el umbral padre y retira su bloqueo temporal. Regresión 232–233 actualizada para consultar este campo.
3. Generación de contenido permitida; tests y compilación siguen pendientes. Balanced y callbacks especiales aún requieren portado.

## Sorteos de evolución del pool de entrenador

1. Trainer.genNewPartyMemberSpecies retorna ret tras evolución y rerolls. Trainer.genPartyMember no vuelve a evolucionar el pool ordinario; la segunda transformación solo corresponde a newSpeciesPool. Retirado sorteo adicional local.
2. sameSpecies conserva su sustitución posterior explícita. Corrección afecta especies y estado RNG consumido antes de crear actor, movimientos e IVs.
3. Regresión 234–236 recorre trainers reales simples sin signature y compara especie y carry/s0/s1/s2 con la secuencia explícita del pool upstream. Escrita, pendiente de ejecución. Specialty filters, tipos balanced y firmas reservadas para evitar duplicados siguen pendientes.

## Especies reservadas del entrenador

1. Trainer.checkDuplicateSpecies pinned incluye raíces de signatureSpecies además de las especies presentes en el equipo. Resolver de pool consulta todas las opciones canónicas de signature sin consumir RNG y suma esa causa de reroll hasta el límite upstream de diez.
2. Referencias/rangos inválidos fallan; no se ignoran firmas desconocidas. Comparación usa la especie base candidata frente a las raíces reservadas, como upstream.
3. Regresión 237–241 recorre opciones signature reales, verifica reserva por raíz y rechazo de especie inválida sin publicar salida. Escrita, pendiente de ejecución. No demuestra todavía todos los equipos de entrenador jugables.

## Equipos balanced: tipos de formas actuales

1. Trainer.genNewPartyMemberSpecies pinned compara ret.type1/type2 contra enemyParty.getTypes antes de otros checks de duplicados. Resolver aplica ese reroll con contexto de tipos resueltos; falta de contexto falla explícitamente.
2. Constructor pasa tipos canónicos de la forma real de cada actor ya construido; no los sustituye por los tipos base de su especie. Límite de diez rerolls conservado.
3. Regresión 242–248 comprueba forma real, catálogo completo, coincidencias y ausencia de contexto. Escrita, no ejecutada. Falta parity de la construcción completa de equipos balanced, filtros specialty y reglas de cambios de tipo/Tera.

## Especialidad de tipo de entrenadores

1. Importer conserva specialtyType y specialtyTypeStatus desde setSpecialtyType y parámetro specialtyType de inicializadores inspeccionados. Expresiones desconocidas quedan preservadas en raw y marcadas UNSUPPORTED_EXPRESSION. Tabla C++ conserva tipo y resolución.
2. Trainer.genNewPartyMemberSpecies: si no existe otra causa de reroll y la evolución no tiene tipo especializado, vuelve a resolver evolución de la misma especie base hasta diez veces usando el umbral padre. Después aplica checks de duplicados.
3. Regresión 249–251 comprueba Brock/ROCK, Misty/WATER y referencias de tipos del catálogo. Regresión de pool ordinario excluye especialidades porque tienen otra secuencia. Escritas, pendientes de ejecución.
4. Pinned import regenerado; el hash cambia por los nuevos campos normalizados. Guardados ligados al hash anterior no se aceptan automáticamente; migración de saves sigue pendiente. Filtros speciesFilter generales y parity completo de entrenadores siguen pendientes.

5. Resultado del reimport: 126 entrenadores con especialidad; 272 trainerRules con specialtyTypeStatus RESOLVED. Dos importaciones pinned producen hash 9475c38f55fde778b84ea3c0d3b454c7c2ef3be4e1f61203efbd66b0392df611. Se conserva catálogo 1084 especies/609 formas/920 movimientos/320 habilidades.

## Primera evolución del pool compuesto

1. Trainer.genNewPartyMemberSpecies pinned llama baseSpecies.getTrainerSpeciesForLevel con template.evoLevelThresholdKind, no getEvoThresholdKind del segmento. Primera resolución del pool usa ahora parentEvolutionThresholdKindId, igual que el reroll specialty. Los callbacks signature conservan el umbral del slot que sí reciben upstream.
2. Regresión de secuencia RNG actualizada para el umbral padre. Regresión 252–255 verifica NORMAL canónico en todos los templates compuestos y umbral propio en templates simples. Escritas, no ejecutadas.
3. Revisado PartyMemberStrength: el snapshot pinned lo transporta como parámetro de determineEnemySpecies, pero no lo usa al decidir evolución. No existe un efecto adicional que portar en esa función. Sigue pendiente comprobar la construcción completa contra upstream; no se declara fidelidad completa de equipos.

## Auditoría de strength en evolución

1. Fuente: src/data/pokemon-species.ts, PokemonSpecies.getTrainerSpeciesForLevel/getSpeciesForLevel; src/ai/ai-species-gen.ts, determineEnemySpecies, revisión 8555c08c823b856cbec4eb99ca84ea52a955836d. strength solo aparece en la declaración y en la llamada recursiva; calcEvoChance y selección aleatoria dependen de nivel y encounterKind.
2. Esta evidencia corrige el pendiente anterior, sin introducir un multiplicador de fuerza inventado. PartyMemberStrength sí afecta los niveles del equipo en Trainer.getPartyLevels; esa ruta es distinta.
3. Pendientes reales: filtros generales de especies, selección fija de gyms/rivales, dobles 2vs2, reglas de movimientos/habilidades completas, persistencia del equipo, Eternamax, presentación, OTA y validación final de Azahar/Old 3DS.

## Cálculo de daño contra segmentos de jefe

1. Portado calculateBossSegmentDamage de src/utils/damage.ts pinned a calculatePokemonBossSegmentDamage en el módulo BattleState existente. Conserva umbral Math.round, bypass log2 acotado y toDmgValue floor/min1, sin RNG. Duplicación sucesiva evita libm log en ARM11.
2. Regresiones 468–474 escritas para daño insuficiente/exacto, daño excedente limitado, bypass, mínimo de segmento de primera fase final, HP no divisibles e inputs inválidos sin mutación de salida. Pendientes de ejecución.
3. Resolver puro todavía no conectado a los comandos: faltan estado de segmentos, stat boosts, transformación Eternamax y evento/render. No se declara combate de jefe ni GameClear completo.

## Planificación de boosts por segmentos

1. Portado EnemyPokemon.handleBossSegmentCleared y weightedPick pinned (src/field/pokemon.ts y src/utils/random.ts) a planPokemonBossSegmentCleared. Pesos usan stats permanentes, filtro stages + primer cambio pendiente, RNG randSeedInt y boosts de últimos shields. Entrenadores/no stats elegibles no consumen sorteos.
2. Resultado es evento agregado por stat con nuevo índice de segmento; no muta BattleState. RNG/salida se publican solo si la planificación completa tiene éxito. Phase futura debe aplicar cambios con ignoreAbilities como upstream.
3. Regresiones 475–482 escritas para stats maxed, último shield +2, entrenador sin boosts e inputs inválidos sin avanzar RNG. No ejecutadas. Conexión de segmentos al daño, aplicación de boosts, callbacks y Eternamax siguen pendientes.

## Operación de daño de jefe

1. applyPokemonBossDamage une cálculo de segmentos, nuevo HP, planificación RNG de boosts y aplicación de stat stages ignoreAbilities, publicando actor/estado/RNG/evento conjuntamente. Requiere policy resuelta y damageCallbacksResolved.
2. Fuente EnemyPokemon.damage pinned: ignoreSegments recalcula índice desde HP restante; primera fase Classic con índice cero limita daño a hp-1. Protección no se aplica a fases posteriores.
3. Regresiones 483–488 escritas para daño limitado, primera fase a 1 HP, fase posterior KO, ignoreSegments y callbacks sin resolver sin mutación. No ejecutadas.
4. Aún falta conectar a useStandardPokemonMove/FirstRunRuntime, inicializar segmentos canónicos, resolver Endure/PostDamage/form callbacks y Eternamax. Esta operación aislada no demuestra jefe jugable.

## Comando de movimiento con segmentos

1. useStandardPokemonMove acepta contexto opcional de jefe y aplica applyPokemonBossDamage tras resolver hit/daño, antes de publicar PP/target/RNG. Primera fase no se reporta como faint al llegar a 1 HP. Daño aplicado real queda en MoveActionResult para drain/recoil futuros.
2. Contexto parcial/callbacks sin resolver produce UnresolvedBoss antes de gastar PP/sortear; fallos posteriores conservan target, shields y RNG. Comandos normales conservan parámetros por defecto.
3. Regresiones 489–491 escritas para movimiento real contra primera fase a 1 HP y rechazo sin mutación de callbacks no resueltos. Pendientes de ejecución.
4. Falta que FirstRunRuntime inicialice/pase este contexto, resolver callbacks y transformar Eternamax. No se declara jefe final jugable.

## Inicialización de segmentos Classic

1. initializeClassicPokemonBossState porta BattleScene.getEncounterBossSegments y EnemyPokemon.setBoss pinned: wave X0/legend-like/forceBoss, 2 segmentos + nivel100 + BST670 + floor(wave/250), índice inicial count-1. Sin overrides de debug ni reglas Daily/random bosses Endless.
2. ResolvedPokemon conserva bossState; resolución de actores wild inicializa desde especies/niveles canónicos. Capturado pierde estado de boss del enemigo. Save v8 rechaza estados de jefe que no puede serializar.
3. Regresiones 492–496 escritas para actor no boss, X0, nivel100, Eternatus real con cuatro segmentos e ID inválido sin mutación. No ejecutadas.
4. Pendientes: asignación proporcional en encuentros múltiples, callbacks, pasar contexto al comando, Eternamax y save. Inicializar segmentos no demuestra jefe jugable.

## Captura y shields de jefes

1. CommandPhase.handleBallCommand pinned: boss con segmentIndex>=1 bloquea bolas normales, con excepciones Master Ball y hasAbility(WONDER_GUARD,false,true). Runtime consulta estado real de segmentos antes de gastar bolas o RNG. Referencia Wonder Guard se resuelve mediante metadata de perfil canónico, sin ID numérico paralelo.
2. executeCaptureAttempt también permite Master Ball frente a shields ordinarios. Regresiones 256–260 escritas para bloqueo, Master, Wonder Guard, último segmento y captura garantizada. No ejecutadas.
3. El jefe final conserva bloqueo temporal porque falta perfil/dex completo que permite su captura upstream; no se declara esa regla terminada. Shields aún deben avanzar mediante daño del runtime y callbacks completos.

## Contexto de jefe conectado al runtime

1. AbilityMovegenProfile conserva bossDamageCallbacksResolved generado desde raw upstream y provenance. Solo attrs inspeccionados sin callbacks de daño (Pressure y NonSuperEffectiveImmunity, o declaración vacía) se aceptan; atributos/conditions desconocidos/unimplemented quedan sin capacidad.
2. FirstRunRuntime pasa bossState y política al comando estándar de daño para ataques plain/recoil/drain. Las rutas de recoil/drain publican estado de segmentos después de resolver su efecto; fallo se rechaza sin publicación parcial del comando completo.
3. Regresión 261–263 comprueba Pressure soportada y Sturdy bloqueada por callbacks aún no portados. Escrita, pendiente de ejecución.
4. Jefes aún incompletos: daño residual, callbacks adicionales, forma Eternamax, asignación de segmentos en dobles y guardado. No se declara Classic completo.

## Reparto de segmentos en encuentros dobles

1. EncounterPhase pinned (src/phases/encounter-phase.ts) reduce segmentos únicamente cuando existen varios jefes generados: ceil(segmentos * BST de forma / BST total). Runtime aplica esa proporción con aritmética entera y referencias a formas canónicas; no usa stats de combate.
2. Regresiones 497–500 escritas para proporciones desiguales, redondeo, entrada inválida sin mutación y encuentro con un solo jefe. Pendientes de ejecución por instrucción del usuario.
3. Continúan pendientes dobles 2vs2, callbacks completos, daño residual de jefes, Eternamax y persistencia. No se declara Classic completo.

## RNG de shields separado del RNG de turno

1. Auditoría pinned: utils/random.ts weightedPick llama utils/common.ts randSeedInt (Phaser.Math.RND); Battle.randSeedInt conserva otro stream por turno. La conexión anterior consumía boosts desde el stream de daño: corregido.
2. Comando estándar exige stream global distinto para contexto de jefe, copia ambos y publica HP/PP/shields/RNG conjuntamente. FirstRunRuntime retiene el stream de wave tras la construcción resuelta del encuentro. Save v8 ya rechaza jefes y no pretende serializar este estado.
3. Queda pendiente parity del stream global completo: modifiers, fases y rewards aún no portados también consumen draws upstream. Esta separación corrige la mezcla, sin afirmar fidelidad RNG de una partida completa.
4. Eternamax inspeccionado: BattleScene.initFinalBossPhaseTwo se invoca al terminar DamageAnimPhase/PostTurnStatusEffectPhase; QuietFormChangePhase restaura HP/status/PP, borra tags, establece 5 segmentos y cancela movimientos del jefe. Incluye moveset de segunda fase, Mini Black Hole y dos actores del jugador; conexión completa pendiente.

5. Regresiones 501-504 escritas: igualdad del stream de hit/crit/roll frente a comando ordinario, consumo separado de boosts, rechazo de stream ausente/alias sin gastar PP. No ejecutadas.

## Daño climático de jefes conectado

1. WeatherEffectPhase pinned llama damageAndUpdate con ignoreSegments:true. PokemonWeatherPhase aplica ahora el daño residual a través del contexto real de jefe, recalcula shields y boosts en el stream global separado. Inmunidades no consumen boosts; callbacks no resueltos se rechazan.
2. Actor, shields y RNG se copian y publican al completar la fase. FirstRunRuntime integra ambos enemigos y conserva el evento del segundo. Regresiones 505–508 escritas para Eternatus real, cruce de shield y ausencia de contexto RNG sin mutación; no ejecutadas.
3. Esto no implementa la transformación Eternamax ni todos los tags/abilities de clima. Siguen pendientes para Classic completo y validación final.

4. Revision estatica del harness: los casos de shields ahora construyen su actor con movimiento real; antes reutilizaban state, que otra prueba habia reemplazado por un actor sin moves. Asserts conservados, arreglo del setup, pendiente de ejecucion.

## Inventario de bolas persistente — schema v9

1. NativeRunSave v9 conserva las cinco entradas upstream utilizadas de PokeballType (src/enums/pokeball.ts); Luxury Ball está declarada pero excluida del inventario por BattleScene.reset. Valida MAX_PER_TYPE_POKEBALLS=99 (src/data/pokeball.ts). Setup solo admite inventario inicial.
2. Journal dual/checksum/export existentes se reutilizan. Decoder migra v8 en memoria conservando Trick Room y los campos anteriores; inventario inicial es válido para producción v8 porque el runtime rechazaba stock modificado. Saves de runtime futuro siguen bloqueando fallback.
3. FirstRunRuntime captura y restaura stock con el candidato atómico; ya no rechaza únicamente por bolas gastadas/recibidas. Regresiones de codec 40–46 y runtime 264–268 escritas para stock, límite, rechazo atómico y v8. Fixtures de versiones previas derivadas del writer ahora parten de encabezado v9; asserts conservados. No ejecutadas.
4. Equipo capturado, dobles, jefes, historial de modifiers y progresión fuera de la frontera soportada siguen sin persistencia completa. No se declara save completo ni Classic jugable.

5. Capacidad acotada del envelope ampliada a 4096 bytes: etiquetas/hex de seis miembros + stages/campos + stock requieren más que el buffer anterior en el peor caso. Journal y backend usan la constante común. Regresión existente de seis miembros se conserva; memoria/validación final siguen pendientes.

## Snapshot portable de Pokémon — preparación del equipo persistente

1. NativePokemonSave en el módulo de storage existente conserva species/form IDs, nivel, ID/IVs, habilidad, género/naturaleza, EXP, HP, moves/PP y stages. No serializa punteros ni stats calculados.
2. Capture/restore reconstruyen stats desde catálogo canónico y validan referencias, forma/especie, habilidad, IVs derivados, naturaleza, EXP mínima, PP, HP y stages antes de publicar salida. Form pointer pertenece al catálogo, no al buffer de save. Rechaza stats fuera del modelo base soportado; el validador de run debe resolver cap de EXP.
3. Regresiones 269–276 escritas para roundtrip de actor real y rechazo atómico de HP/naturaleza/IV/form inválidos. No ejecutadas.
4. Esta representación todavía no entra en el payload v9: faltan codec de equipo, identidad auxiliar del actor (abilityIndex/Tera), active slot, validación de contexto y restauración sin replay. El guardado de capturados continúa rechazado explícitamente hasta integrar esas capas.

## Identidad auxiliar del snapshot

1. Snapshot de actor conserva abilityIndex y ordinal initialTeraTypeIndex/resolved además del BattleState. Capture verifica coherencia entre identidad y estado; restore valida slot de habilidad contra forma/especie y publica ambos juntos con referencias al catálogo.
2. Regresiones 277–280 escritas para actor real y rechazo de índice de habilidad/Tera no resuelto. No ejecutadas. Esta capacidad cubre actores actuales sin transformaciones; Tera real persistente tras evolución necesita identidad de tipo explícita, no ordinal recalculado.
3. Codec de equipo y restauración de partidas capturadas siguen pendientes; no se cambia payload v9 ni se retira su bloqueo de equipo.

## Codec acotado del miembro persistente

1. encode/decodeNativePokemonSave reutilizan Writer/Reader del journal para un payload determinista de actor. IDs canónicos y campos mutables se escriben sin punteros/stats calculados; referencias e identidad se validan antes de aceptar el registro. Decoder limita a 512 bytes y rechaza bytes extra/truncados y flags inválidos.
2. El envelope de run será responsable de versión/hash/checksum; este payload no es por sí solo un archivo de progreso importable. Regresiones 281–286 escritas para roundtrip byte a byte, truncamiento sin mutar salida y capacidad insuficiente. No ejecutadas.
3. Pendiente inmediato: arrays de miembros y active slot en schema de run, migración v9 y conexión de capture/restore sin replay de capturas. Persistencia completa y objetivo jugable siguen sin demostración.

## Equipo en payload v10

1. NativeRunSave v10 incluye hasta seis snapshots explícitos y slot activo, con registros de longitud acotada dentro del envelope checksum existente. V9 migra sin inventar miembros: count=0 conserva el camino legacy. Versiones anteriores y fixtures de migración conservadas.
2. Valida identidad canónica de cada miembro, IDs duplicados y coherencia de HP/EXP/level/moves/PP/stages del activo con campos anteriores; growth rate corresponde al actor activo. Buffer acotado 8192 cubre hasta seis registros de jugador y seis de entrenador.
3. Regresiones 287–289 escritas para codec de equipo, slot inválido y discrepancia de HP; no ejecutadas. Runtime aún no emite ni restaura estos arrays: la conexión sin replay y snapshot de contexto de capturas sigue pendiente. No se declara persistencia de capturados completada.

## Equipo capturado conectado a save/restore

1. Runtime captura snapshots explícitos al guardar equipos de varios miembros; activo toma su estado vivo y reservas su estado propio. Restore reconstruye identidad/form/moves/EXP de cada miembro, resuelve el encuentro actual y superpone stages del checkpoint, sin replay de EXP/capturas. Candidato completo continúa publicándose atómicamente.
2. Camino legacy de un solo miembro se conserva. EXP usa growth del activo y admite niveles capturados inferiores a 5. Reserva no recibe experiencia duplicada al reconstruir. Regresiones 290–294 escritas para captura real, avance wave2 y restore de ambos actores/PP/EXP. No ejecutadas.
3. Sigue limitado a waves1–9 y bioma inicial, sin dobles/jefes/modifiers completos ni contexto histórico de rewards. Identidades transformadas/auxiliares no soportadas se rechazan; solo actores compatibles con el snapshot actual. No se declara guardado completo ni Classic terminado.

## Validación de reservas y miembro capturado activo

1. Validador de run aplica level cap y umbral de EXP a todos los miembros explícitos, incluidos suplentes; conserva EXP excedente únicamente al cap. Rechaza BattleLost si queda un miembro vivo y partidas no perdidas sin ningún vivo.
2. Regresiones 295–297 escritas: restaurar capturado como activo conserva su especie/EXP independiente; EXP incoherente de reserva se rechaza sin reemplazar runtime. Pendientes de ejecución.
3. Estos controles complementan el snapshot y no completan historial de modifiers, RNG de fases, dobles, transformaciones ni waves posteriores. Validación final y Classic completo siguen pendientes.

## Slot de habilidad de evolución conectado

1. applySpeciesEvolution acepta identidad real y preserva abilityIndex frente a IDs duplicados; porta PlayerPokemon.evolve/getAbilityCount pinned: slot oculto pasa a slot1 si destino no tiene hidden. Runtime conecta identidad a evolución y preserva flag IVs derivados al recalcular nivel/stats.
2. Regresiones 298–299 escritas para forma/slot de evolución y snapshot de Ivysaur real. Setup previo completado con habilidad/género/naturaleza explícitos; asserts conservados. No ejecutadas.
3. Tera tras cambio de tipos sigue requiriendo identidad de tipo explícita; transformaciones, condiciones y evolución cancelable siguen pendientes. No se declara evolución completa.

## Aprendizaje de formas por nivel

1. learnNewLevelMoves combina learnset de especie y forma seleccionada, conforme SpeciesDataRegistry.getLevelMoves y Pokemon.getLevelMoves pinned; orden ascendente estable por nivel, species antes de form en empates, sin pool dinámico. Rechaza forma de otra especie y evita moves duplicados.
2. Regresiones 300–302 escritas buscan una forma real con move exclusivo y comprueban aprendizaje en su nivel. No ejecutadas. Reemplazo de cuatro slots y diálogo de rechazo/aceptación siguen pendientes; el helper todavía solo llena huecos.

## Operación de aprendizaje/reemplazo de movimiento

1. learnPokemonMoveAtSlot en el módulo existente porta LearnMovePhase.learnMove/Pokemon.setMove: slot seleccionado se reemplaza con PP completos, otros slots conservan PP. Duplicados, slot inválido y MoveIsUnimplemented se rechazan sin mutar; aprender en huecos reutiliza la operación.
2. Regresiones 303–306 escritas para reemplazo, duplicado, slot inválido y movimiento upstream no implementado. No ejecutadas.
3. Falta cola de decisiones por nivel y conexión UI de aceptar/rechazar/reemplazar; esta operación no declara ese flujo completo.

## Cola interactiva de aprendizaje conectada

1. Cuando se llena el moveset, movimientos nuevos elegibles se conservan en cola acotada de 128 IDs, deduplicada y ordenada por nivel/fuente. Overflow hace fallar el comando; no se descarta silenciosamente. Regresiones 307–310 escritas para catálogo real y aislamiento de moves conocidos/no implementados. No ejecutadas.
2. Runtime pausa rewards/reemplazo de rival tras EXP: UP/DOWN selecciona cualquier slot, A reemplaza, B rechaza; bridge de acciones 0–3 acepta selección directa. Presentación muestra nombre localizado del movimiento propuesto. Siguiente decisión continúa hasta vaciar la cola.
3. Save rechaza decisiones pendientes porque no están serializadas. Evolución todavía ocurre antes de resolver la cola y faltan confirmación de rechazo/configuración upstream y triggers form-change por move aprendido. No se declara secuencia de fases completa.

## Aprendizaje antes de evolución

1. LevelUpPhase.end pinned encola LearnMovePhase antes de EvolutionPhase; PhaseManager.unshiftPhase documenta FIFO. Runtime difiere la evolución simple hasta resolver la última decisión de aprendizaje. Sin decisiones, evoluciona tras aprender los huecos.
2. Resolución de decisión copia el runtime y publica solo al completar aprendizaje/evolución; fallo conserva cola, actor y EXP. Evolución actualiza nombre localizado y referencias de forma/assets desde catálogo. Save rechaza evolución pendiente no serializada.
3. Falta aprendizaje de EVOLVE_MOVE tras evolución, cancelación/pausa de evolución y callbacks. Regresiones existentes siguen pendientes de ejecución; no se declara secuencia completa.

4. Regresiones 311-312 escritas para reemplazo elegido seguido de evolucion real y preservacion de move/PP. Prueba del flujo interactivo completo y ejecucion siguen pendientes.

## Movimientos exclusivos de evolución conectados

1. finishPendingEvolution solicita filas EVOLVE_MOVE=0 después de cambiar especie, como EvolutionPhase.postEvolve pinned. Combina especie/forma; aprende huecos o encola reemplazos, sin duplicados ni moves upstream no implementados. Actor/cola se publican conjuntamente.
2. Regresiones 313–316 escritas con movimiento real marcado EVOLVE_MOVE del catálogo. No ejecutadas. Cancelación/pausa de evolución, callbacks y secuencia completa de UI siguen pendientes.

## Cancelación del intento de evolución

1. Tras aprendizaje, runtime conserva target canónico hasta A (aplicar) o B (cancelar intento). Cancelar no revierte EXP ni moves aprendidos; no concede reward ni avanza wave. Nombre destino usa locales. Otros comandos y save quedan bloqueados mientras la decisión no serializada esté pendiente.
2. Esto adapta la cancelación de EvolutionPhase a una frontera antes de animación. No implementa pauseEvolutions persistente, animación cancelable temporal ni diálogo/configuración upstream completo. Regresiones del helper conservadas; prueba end-to-end del prompt aún pendiente. Tests/compilación siguen aplazados.

## Decisiones del jugador y restauración legacy

1. Replay legacy falla si EXP requiere aprendizaje o evolución pendiente; no borra decisiones ni inventa respuestas al reconstruir waves.
2. Resolver aprendizaje, aceptar/cancelar evolución o restaurar un equipo explícito obliga a mantener snapshots incluso con un solo miembro. La marca se conserva entre waves y se reinicia con un nuevo starter. Guardar decisiones pendientes sigue bloqueado.
3. Regresiones 317–319 escritas para restore/recapture de un actor explícito, identidad, especie, EXP, moves y PP. Tests y compilación aplazados; límite de restore wave 9 y otras restricciones permanecen.

## Pausa persistente de evoluciones

1. Referencia pinned 8555c08c823b856cbec4eb99ca84ea52a955836d: src/phases/evolution-phase.ts showPauseEvolutionConfirmation y src/phases/level-up-phase.ts end. Cancelar ahora pregunta si se deben pausar futuros intentos: A sí, B no. No avanza wave ni concede rewards mientras la confirmación está pendiente; guardar queda bloqueado.
2. PokemonBattleState conserva pauseEvolutions al recalcular nivel/cambiar especie; LevelUp no propone evolución mientras esté activo. Se sincroniza al miembro del equipo y se conserva mediante snapshot explícito.
3. Codec de actor pokemon=2 añade bool validado; lector acepta pokemon=1 con false. El contenedor de run mantiene v10 y su checksum; lectores antiguos rechazan el nuevo miembro en vez de descartar el campo. Regresiones 320–323 escritas para captura, codec/restore, migración v1 y flag inválido sin mutación.
4. Falta control de reactivación en el menú del Pokémon y animación cancelable con fidelidad temporal. Tests y compilación aplazados; no se declara Classic completo.

## Control de pausa/reactivación por miembro

1. Porta src/ui/handlers/party-ui-handler.ts processUnpauseEvolutionOption y elegibilidad hasEvolutions del upstream pinned. Comando togglePlayerEvolutionPause opera sobre miembro activo o reserva, bloquea decisiones pendientes/índice inválido/especie sin evoluciones y sincroniza actor activo.
2. SELECT alterna el actor activo desde el host nativo; bridge admite acciones 220–225 para los seis miembros. No consume turnos, concede EXP ni inicia evolución inmediata. El cambio obliga a snapshot explícito y conserva la opción en restore.
3. Regresiones 324–327 escritas para índice inválido, elegibilidad real, pausa/restore/reactivación y ausencia de avance de wave/turno/EXP/HP. Tests y compilación aplazados. Menú visual completo de equipo y animación de evolución siguen pendientes.

## Tipo Tera concreto conservado tras evolución

1. Pokemon constructor pinned src/field/pokemon.ts almacena teraType concreto al crear actor y lo copia desde dataSource; un ordinal de tipo no equivale a ese estado tras cambiar especie. Actor C++ conserva símbolo concreto, resuelto con la tabla de tipos existente sin consumir RNG adicional.
2. Evolución mantiene el tipo original; captura/restore no lo recalcula a partir de la forma nueva. Codec pokemon=3 incluye tipo; lectores aceptan v1/v2 y resuelven una vez el antiguo ordinal de la forma guardada. Datos históricos que ya perdieron el tipo original no son recuperables sin evidencia adicional.
3. Regresiones 328–334 escritas para migración v2, Onix→Steelix conservando ROCK frente a STEEL y rechazo de tipo inválido sin mutar actor. Esto verifica identidad/persistencia; no porta todavía trigger de Metal Coat ni combate Terastal. Tests/compilación aplazados.

## Recálculo de forma para la transición final

1. changePokemonBattleForm reutiliza initializePokemonBattleState con forma real de la misma especie, habilidad resuelta y actor original. Conserva PID, IVs/procedencia, nature, stages y pausa; restaura HP/PP cuando la fase lo pide y publica únicamente tras validar. Cambio sin curación mantiene faint/PP y ajusta HP por aumento de max HP.
2. Regresiones 510–515 escritas con Eternatus→Eternamax del catálogo pinned, incremento de HP, restauración de PP, identidad/IVs, rechazo de forma ajena y faint sin curación. Tests/compilación aplazados.
3. Este helper todavía no está conectado a initFinalBossPhaseTwo. Faltan moveset de fase 2, Mini Black Hole, limpiar status/tags, cancelar moves encolados y dos posiciones de jugador; el combate final sigue incompleto. Fuentes: src/battle-scene.ts initFinalBossPhaseTwo; src/phases/quiet-form-change-phase.ts end; src/data/balance/species/generation-08.ts ETERNATUS.forms.

4. Regresiones 516-517 adicionales para stages invalidos y PP maximos modificados no soportados; el cambio falla sin publicar un actor parcialmente recalculado. Pendientes de ejecucion.

## Movesets fijos de Eternatus importados

1. Importer inspecciona src/field/pokemon.ts EnemyPokemon.generateAndPopulateMoveset:ETERNATUS del snapshot pinned; conserva ambos arrays, ppUsed/ppUp, raw y provenance/SHA-256. IDs se resuelven contra movimientos canónicos. Overrides Inverse Battle quedan preservados explícitamente como no soportados.
2. Generated Runtime expone kFixedEnemyMovesets. Constructor enemigo usa esos cuatro IDs para Eternatus, sin consumir selección/ponderación salvaje. PokemonMove.getMovePp aplica ppUp negativo: Recover de fase 2 tiene máximo 1 PP. Transición completa a fase 2 todavía pendiente.
3. Regresiones de importación y 335–338 nativas escritas para ambos sets, Recover y rechazo sin mutación. Importación doble verifica reproducibilidad del contenido; no constituye ejecución de tests ni compilación. Classic y transición final no se declaran completos.

## Preparación atómica del actor Eternamax

1. preparePokemonFinalBossSecondPhase porta condición de wave final, actor vivo, primera forma/boss y último segmento despejado. Reutiliza forma y moveset canónicos, restaura HP/PP, conserva identidad/stages y fija cinco segmentos/index cuatro como QuietFormChangePhase.end. Validación fallida no publica actor ni boss.
2. Regresiones 339–345 escritas para trigger prematuro, wave incorrecta, Eternamax real, ambos movesets/Recover, cinco segmentos y rechazo de repetición. No ejecutadas.
3. Helper todavía no entra en el turno nativo: Mini Black Hole, status/tags, cancelación del move pendiente y segunda posición del jugador deben conectarse antes de declarar initFinalBossPhaseTwo completo. Tests y compilación aplazados; Classic sigue incompleto.

## Transferencia de stacks de objetos sostenidos

1. Catalog contiene MINI_BLACK_HOLE y provenance reales. No existe todavía inventario nativo de held modifiers; no se sustituye por una lista UI ni se afirma que el objeto funciona en batalla.
2. calculateHeldItemStackTransfer en el módulo existente de modifiers porta cantidades/remoción de BattleScene.tryTransferHeldItemModifier: cantidad predeterminada del caller uno, límite del receptor con modifier coincidente y remoción del stack agotado. Capacidad ausente/estado incoherente se distinguen de receptor lleno; evento solo se publica al resolver.
3. Regresiones 346–350 escritas para unidad transferida, límite, lleno sin mutación, agotamiento e inválidos. Falta inventario/matchType, selección seeded de oponente/item, BlockItemTheftAbAttr/PostItemLostAbAttr y conexión TurnEndPhase; Mini Black Hole/Eternamax siguen incompletos. Tests/compilación aplazados.

## Selección seeded de intento de transferencia

1. selectHeldItemTransferAttempt en el módulo existente porta la activación de un objeto (máximo de Mini Black Hole): usa battle RNG del holder para elegir oponente y luego ordinal de held modifier transferible. Filtra por owner PID y conserva orden del inventario sin crear pool heap.
2. Oponente se sortea antes de verificar count/objetos, incluso sin objeto transferible; sin oponentes no consume RNG. Datos inválidos no publican RNG/selección. Activaciones de stacks mayores a uno se marcan inválidas hasta portar el loop y eliminación/reintentos upstream.
3. Regresiones 351–357 escritas para selección/filtrado, estado completo RNG y ausencia de consumo indebido. Inventario runtime, callbacks y conexión TurnEndPhase siguen pendientes; no se declara Mini Black Hole funcional. Tests/compilación aplazados.

## Registro nativo de held modifier instances

1. NativeHeldModifierInstance en el módulo existente referencia catálogo canónico (sin IDs paralelos), owner PID, stack, transferibilidad y argumentos raw. Mantiene definición separada de instancia y no supone que ID igual implique matchType.
2. Primitivas de almacenamiento sobre buffer del caller conservan orden, permiten remoción compacta y fallan claramente por capacidad/ID inexistente/argumentos demasiado largos, sin truncar ni publicar datos falsos. La capacidad de buffer no limita el catálogo. Índices corresponden al content hash; futura serialización debe conservar identidad canónica y validar actualización.
3. Regresiones 358–363 escritas con MINI_BLACK_HOLE real y bandera no transferible como el boss upstream, límites/remoción/metadata. Falta conectar inventario a runtime/rewards/save, políticas matchType y callbacks; no se declara efecto de batalla funcional. Tests/compilación aplazados.

## Transferencia conectada al buffer de instancias

1. applySelectedHeldItemTheft conecta cálculo de stacks y almacenamiento existente: clona fuente para receptor, conserva catálogo/raw args/transferibilidad, remueve source agotada y modifier coincidente, añade receptor al final y emite evento de pérdida para dispatcher. Valida capacidad y datos antes de mutar.
2. Policy explícita exige matchType/capacidad y capacidad del dispatcher de habilidades resueltos; no supone igualdad de IDs. Objetos protegidos, habilidad bloqueante, receptor lleno, policy pendiente y almacenamiento insuficiente tienen resultados distintos. El caller conserva RNG consumido por selección aunque la transferencia sea bloqueada.
3. Regresiones 364–370 escritas para stacks/orden/remoción, lleno sin mutación, policy pendiente y objeto protegido. Faltan ownership del inventario en runtime, callbacks activos, rewards/save y llamada TurnEndPhase; Mini Black Hole/Eternamax no están completos. Tests/compilación aplazados.

## Selección seeded sobre inventario nativo

1. selectNativeHeldItemTransferAttempt conecta registros reales al selector ya existente mediante acceso sin pool temporal/heap. Conserva índices y orden de inventario, owner PID y bandera transferible; las dos interfaces comparten el mismo algoritmo RNG.
2. Valida capacidad, referencias canónicas, stacks y raw metadata antes de consumir RNG. Mini Black Hole no transferible del boss queda excluido por la misma bandera almacenada.
3. Regresiones 371–373 escritas para selección por propietario, exclusión de copia protegida, estado completo RNG e inventario inválido sin consumo. Falta ownership del inventario en FirstRunRuntime/serialización, policies de habilidades y TurnEndPhase. Tests/compilación aplazados.

## Ownership del inventario en FirstRunRuntime

1. FirstRunRuntime conserva buffer de held modifiers y consulta read-only; hidratación valida catálogo/raw/stacks y owner PID entre actores resueltos del equipo/enemigos, con publicación atómica. Reiniciar setup elimina inventario del run anterior. Presupuesto provisional 32 registros (~4.6 KiB) falla explícitamente al excederse; requiere revisión de memoria y ampliación para catálogo completo.
2. Guardados rechazan inventario no serializado. Comandos de combate/captura/cambio/rewards rechazan efectos todavía sin dispatcher; no ejecutan turnos ignorando los objetos. La hidratación no concede un reward ni afirma que un efecto ya funciona.
3. Regresiones 374–378 escritas para ownership, índice/capacidad inválidos, aislamiento del turno, frontera de save y reset. Faltan codec, policies matchType/abilities y conexión Mini Black Hole/TurnEndPhase. Tests/compilación aplazados; Classic sigue incompleto.

## Codec de held modifier independiente del orden del catálogo

1. encode/decodeNativeHeldModifier reutiliza Writer/Reader de NativeRunSave: held=1, ID canónico textual, owner PID, stack, transferibilidad y raw arguments codificados por bytes. Decodifica ID contra catálogo pinned y solo publica registro válido; no guarda índices susceptibles a reordenamiento.
2. Regresiones 379–383 escritas para roundtrip de metadata desconocida con saltos de línea, determinismo byte a byte, truncamiento/ID inexistente sin mutación y capacidad insuficiente. El componente no aporta checksum propio; lo debe envolver el journal de run.
3. Falta incluir registros en payload de run y migrar schema, ampliar/verificar presupuesto y conectar dispatcher. Save de inventario todavía se rechaza explícitamente. Tests/compilación aplazados; Classic sigue incompleto.

## Guardado de inventario en schema v11

1. NativeRunSave v11 añade count y componentes held=1 dentro del payload protegido por SHA-256/content hash. IDs canónicos se resuelven al decodificar; límite 32 registros y envelope 8192 bytes fallan explícitamente por capacidad. No se aumentó silenciosamente el presupuesto de stack del journal; su perfil de memoria sigue pendiente de hardware.
2. Migración v10 preserva actores explícitos y añade inventario vacío; rutas v1–v9 se conservan. Runtime capture fuerza snapshot de equipo cuando hay objetos, restore valida propietarios contra actores reconstruidos antes de publicar y setup no admite inventario. Efectos no soportados siguen bloqueados al jugar.
3. Regresiones 384–388 escritas para run roundtrip, restauración de owner/metadata y fixture histórica v10 con checksum válido. Fixtures de versiones anteriores identifican ahora el writer v11 antes de construir su layout histórico. Tests/compilación aplazados; dispatcher y Classic completo siguen pendientes.

## MatchType y capacidad de Mini Black Hole

1. Generador obtiene constructor TurnHeldItemTransferModifierType desde raw canónico de items y publica perfil con provenance. Adapter del módulo existente porta matchType de TurnHeldItemTransferModifier y getMaxHeldItemCount=1, según src/modifier/modifier.ts pinned. No usa nombres UI ni supone que clases desconocidas tengan el mismo comportamiento.
2. resolveTurnHeldItemTransferMatchPolicy exige capacidad del dispatcher de habilidades resuelta, respeta cancelación previa a matching y rechaza argumentos/clases pendientes o stacks incoherentes sin publicar policy. Perfil false significa clase sin adapter, no certeza de ausencia de hooks.
3. Regresiones 389–393 escritas para policy pendiente, cap/matching real, metadata desconocida y bloqueo. Regeneración del header permitida; tests/compilación aplazados. Falta dispatcher de habilidades y conexión del efecto en turnos; Classic sigue incompleto.

## Política de habilidades para robo de objetos

1. Generador deriva perfiles de todos los abilities desde upstreamAttributes canónicos, con provenance: BlockItemTheftAbAttr bloquea, PostItemLostApplyBattlerTagAbAttr requiere dispatcher pendiente y condiciones desconocidas hacen fallar. Adapter porta CancelInteractionAbAttr y orden previo a PostItemLost, según ab-attrs.ts/BattleScene.tryTransferHeldItemModifier pinned.
2. Resolver exige conjunto de abilities/passives aplicables ya resuelto (supresión, ignorable, faint y fusión pertenecen al dispatcher); no confunde ID desconocido con ausencia de hooks. Sticky Hold cancela transferencia, incluso si otro callback post-loss estaría pendiente, porque no ocurre pérdida. Unburden sin bloqueo queda explícitamente pendiente hasta portar su tag.
3. Regresiones 394–400 escritas para aplicación pendiente, bloqueo real, callback pendiente, combinación, Pressure sin hooks de robo e ID desconocido. Regeneración permitida; tests/compilación aplazados. Falta conexión del efecto en turnos y lifecycle Unburden; Classic sigue incompleto.

## Callback nativo PostItemLost para Unburden

1. Perfil generado reconoce el atributo y argumento UNBURDEN desde raw canónico. Dispatcher del módulo de modifiers valida todos los IDs antes de publicar el tag, respeta simulated y no repite un tag existente. Callbacks/condiciones desconocidos fallan explícitamente.
2. Regresiones 401–404 escritas; tests/compilación aplazados. Estado transitorio separado: falta conexión al turno, reset/serialización de tags y multiplicador de velocidad; no se habilita todavía la policy de transferencia con Unburden.

## Estado y velocidad Unburden

1. Tag PostItemLost pertenece al PokemonBattleState; velocidad consume tag y perfil canonico de habilidad. Cambio manual limpia tag; cambio de forma lo conserva.
2. Captura v11 rechaza tag activo: falta payload de summon tags. Otros reset de campo, pasivas/supresion y callback en turnos siguen pendientes. Regresiones 518-520 escritas; tests/compilacion aplazados.

## Persistencia Unburden en actor v4

1. Subformato pokemon=4 agrega flag Unburden; lectores 1-3 migran tag ausente. Envelope run v11 conserva compatibilidad estructural; runtime antiguo rechaza actor v4. Snapshot explicito requerido con tags; restore aplica tag despues de reconstruir encuentro.
2. Regresiones 405-407 escritas para roundtrip, migracion v3 y flag invalido; fixtures v1/v2 conservan layout previo. Tests/compilacion aplazados. Sigue pendiente callback dentro del turno y resto del lifecycle.

## Transferencia y callback atomicos

1. applyHeldItemTheftWithCallbacks valida owner y policy, prepara PostItemLost en copia y publica tags/evento solo tras transferencia. Unburden conocido resuelve callbacks; metadata desconocida sigue rechazada.
2. Regresiones 410-413 escritas para aplicabilidad pendiente, Sticky Hold y transferencia con Unburden. Falta conexion al fin de turno y aplicabilidad completa. Tests/compilacion aplazados.

## Aplicabilidad de habilidades para inventario

1. Perfiles canonicos preservan bypassFaint, ignorable, unsuppressable, condiciones y restricciones fusion/transform. Resolver ordena main/passive, elimina duplicado y aplica flags. Contexto requiere Neutralizing Gas/exenciones ya resueltos; predicados desconocidos fallan explicitamente. No es dispatcher general completo.
2. Regresiones 414-418 escritas; generacion permitida, tests/compilacion aplazados. Conexion al fin de turno y resolucion del contexto de campo pendientes.

## Transferencias dentro del equipo

1. Wrapper distingue crossSide: BlockItemTheft cancela solo entre bandos, segun BattleScene.tryTransferHeldItemModifier pinned. PostItemLost sigue controlado por itemLost tambien dentro del equipo.
2. Regresiones 419-420 escritas para Sticky Hold + Unburden dentro del equipo y itemLost=false. Tests/compilacion aplazados. Dispatcher de fin de turno sigue pendiente.

## Activacion nativa TurnHeldItemTransfer

1. Une holder vivo, seleccion seeded de rival/objeto, policy de habilidad, matching especifico del objeto robado y transferencia/callback. Matching es inyectado: el objeto robado puede pertenecer a otra clase. RNG se conserva tras cancelacion soportada/capacidad agotada/ausencia de objeto; capacidades pendientes no publican draws.
2. Regresiones 421-423 escritas; no ejecutadas. Maximo dos rivales corresponde posiciones del campo, no limite de catalogo. Aun no conectado a finishBattleTurn ni habilitados objetos desconocidos; falta dispatcher de clases y contexto real.

## Matching de Leftovers y Shell Bell

1. Perfiles derivados del constructor upstream identifican TurnHealModifier/HitHealModifier; adapters portan matchType por clase y cap 4 inspeccionados en modifier.ts pinned. Resolver permite coexistencia de clases conocidas; desconocidas/argumentos no resueltos fallan. No implementa curacion.
2. Regresiones 424-426 escritas para matching, coexistencia y transferencia parcial hasta cap. Generacion permitida; tests/compilacion aplazados. Efectos y conexion al turno pendientes.

## Curacion de objetos equipados

1. Adapter existente de inventario reutiliza PokemonHealingPolicy/Event. Leftovers redondea maxHP/16 antes de stacks; Shell Bell redondea dano*stacks/8. HealingBooster se aplica despues; Heal Block y actividad son entradas resueltas. No revive ni consume PP.
2. Regresiones 427-430 escritas para policy pendiente, ambos redondeos y bloqueo. Tests/compilacion aplazados; dispatcher, acumulador real de dano y presentacion de eventos pendientes.

## Acumulador de dano del turno

1. useStandardPokemonMove suma damageApplied confirmado en turnDamageDealt, con overflow antes de commit. Recoil/drain preservan el contador via copias existentes. finishBattleTurn limpia tras consumidores; cambio de forma conserva.
2. Actor save rechaza contador no cero hasta portar payload turnData. Checkpoints lo limpian. Tests/compilacion aplazados; falta conexion Shell Bell, sustitutos y multihit.

## Fase de curacion Leftovers conectada

1. finishBattleTurn ejecuta fase TurnHealModifier para actores activos fuera de interlude, antes de limpiar turnData. Fase conserva orden de inventario y prepara actor en copia: error posterior no publica curacion parcial. Politica baseline sin tags/charms solo corresponde al frontier actual.
2. Regresiones 431-432 escritas para filtrar Shell Bell y fallo atomico. El bloqueo de comandos con inventario sigue activo hasta dispatcher completo: conexion no demuestra jugabilidad con items. Shell Bell pertenece al fin de MoveEffectPhase, no a TurnEndPhase; aun pendiente. Tests/compilacion aplazados.

## Shell Bell al final del movimiento

1. Fase compartida de curacion filtra HitHealModifier al fin del movimiento; runtime la conecta en ataques normales, recoil y drain antes de commit de actores/RNG/boss. Usa turnDamageDealt acumulado y conserva el contador hasta TurnEnd.
2. Regresion 433 escrita; tests/compilacion aplazados. Inventario sigue bloqueado hasta dispatcher completo; status/multihit y politica de tags/charms pendientes.

## Cierre de movimientos de estado con Shell Bell

1. Rutas nativas self-heal, clima, stat-stage y Trick Room pasan por el mismo cierre HitHealModifier que los ataques. Conserva turnDamageDealt acumulado; movimiento sin dano nuevo no sustituye el contador por cero. Fallos se propagan al comando candidato.
2. Regresiones 434-435 escritas para acumulador conservado y ausencia de dano; tests/compilacion aplazados. Inventario sigue bloqueado hasta resolver dispatcher completo, tags/charms y multihit.

## Cancelacion previa a MoveEffectPhase

1. Shell Bell no cura tras cancelacion por clima primordial: MovePhase.secondFailureCheck termina antes de crear MoveEffectPhase. Rutas normal/recoil/drain respetan weatherCancelled. Conserva dano acumulado para consumidores posteriores.
2. Revision estatica; tests/compilacion aplazados. Otras cancelaciones previas siguen pendientes.

## Turnos con inventario de curacion

1. advanceBattleTurn reemplaza bloqueo global por validacion de clases de curacion ya conectadas: TurnHeal/HitHeal, stacks canonicos, argumentos resueltos y unicidad por owner/clase. Mini Black Hole y otras clases siguen rechazados. Reward/capture/switch con inventario siguen pendientes y mantienen sus gates.
2. Regresiones 436-438 escritas para clases conocidas, duplicado y clase pendiente. No demuestra run completa; tags/charms, rewards, multihit y validacion final pendientes. Tests/compilacion aplazados.

## Cambio de equipo con inventario curativo

1. switchPlayerPokemon permite clases curativas ya conectadas; mantiene owner PID y ejecuta respuesta enemiga y fin de turno por fases existentes. Limpia summon tag/turnDamageDealt de actor saliente/entrante. Clases desconocidas siguen rechazadas.
2. Regresiones 439-440 escritas para cambio con objetos de ambos miembros y ownership estable. Tests/compilacion aplazados; capture/reward y efectos restantes pendientes.

## Captura con inventario curativo

1. Captura admite clases curativas portadas con registros ligados al jugador o target. Conserva PID del capturado y sus objetos; limpia summon tags/turnData del nuevo miembro. Antes de consumir ball rechaza equipo lleno y objetos de otros enemigos hasta politica de reemplazo/cleanup.
2. Semantica inspeccionada en attempt-capture-phase.addToParty y EnemyPokemon.addToParty pinned. Tests/compilacion aplazados; no afirma captura completa de todos los casos.
3. Regresion 441 escrita para captura real seeded con Leftovers del enemigo y PID conservado; pendiente de ejecucion.

## Inventario entre encuentros

1. resolve(carryPlayer) retiene registros por PID de equipo antes de retirar enemigos anteriores. Capturados conservan sus objetos; orden de inventario estable. skipVictoryReward acepta clases curativas soportadas; elegir rewards con inventario sigue pendiente.
2. Regresion 442 escrita para cleanup enemigo y retencion de equipo. Fuente BattleEndPhase.clearEnemyHeldItemModifiers pinned. Tests/compilacion aplazados; callbacks PostBattle y reward weights completos pendientes.

## Recompensas equipadas curativas

1. addKnownHealingHeldReward porta PersistentModifier.add/incrementStack: fusion por clase/owner, sin truncar exceso. Stack lleno retorna FullStackNeedsReplacement hasta portar fallback. claimRewardChoice conecta recompensa canonica curativa al inventario del actor seleccionado actualmente; inventario curativo previo ya no bloquea claim.
2. Regresiones 443-445 escritas; tests/compilacion aplazados. Selector de miembro, pesos reward completos, fallback y efectos restantes pendientes. No certifica Classic completo.

## Destinatario de recompensa equipada

1. claimHeldRewardChoice(member) emite comando atomico para PID del miembro elegido; indice invalido/recompensa no equipada soportada/cap lleno no avanzan wave. claimRewardChoice conserva compatibilidad con activo. Fuente SelectModifierPhase party selection y PokemonHeldItemModifierType.selectFilter pinned.
2. Regresion 446 escrita para comando fuera de fase e indice invalido sin cambio de inventario/wave; tests/compilacion aplazados. Presentacion del selector y cobertura end-to-end con reward real pendientes.

## Recompensas pendientes sin sustitucion falsa

1. claimRewardChoice rechaza items sin adapter en vez de consumir eleccion y avanzar sin efecto. Retiradas ramas Berry de curacion/PP inmediata: upstream BerryModifier es held con berryType/consumed y triggers. No se desactivaron asserts previos; prueba historica que supone cualquier reward portado podria revelar fallo en validacion final.
2. Regresion 447 escrita para elecciones reales BERRY/RARE_CANDY cuando aparecen; cobertura condicional, no prueba exhaustiva. Tests/compilacion aplazados. Portar adapters y pesos reales sigue pendiente.

## Pociones desde perfiles canonicos

1. Generador extrae PokemonHpRestoreModifierType(points,percent,healStatus) del raw canonico con provenance. Claim usa max(floor(points*mult),floor(percent*maxHP)), minimo uno y cap HP segun modifier.ts pinned; reemplaza constantes falsas que ignoraban porcentaje.
2. Regresiones 448-450 escritas para Hyper Potion y politica de status pendiente. Politica baseline sin status/charms; motor de status real aun pendiente. Tests/compilacion aplazados.

## Recuperacion PP canonica

1. Generador extrae points/allMoves de PokemonPpRestoreModifierType/PokemonAllMovePpRestoreModifierType con provenance. Adapter valida slots/PP antes de mutar, usa -1 para restauracion completa y conserva otros slots de Ether. Claim reutiliza perfiles en vez de listas de constantes.
2. Regresiones 451-454 escritas; generacion permitida, tests/compilacion aplazados. Selector de destinatario/movimiento en presentacion y PP Ups persistentes pendientes.

## Destinatario y slot de recuperacion

1. claimRecoveryRewardChoice(member,move) agrega comando atomico para HP/PP restore canonico; reserva se modifica por referencia a su actor y activo conserva alias correcto. Ether usa slot explicito; Elixir aplica todos. Otras clases se rechazan sin consumir reward.
2. Regresion 455 escrita para comando fuera de fase/indice invalido. Selector visual y demostracion end-to-end con reward real pendientes; tests/compilacion aplazados.

## Revive canonico por destinatario

1. Perfiles derivados PokemonReviveModifierType (50/100) con provenance; claimRecoveryRewardChoice aplica al miembro elegido, no primer fainted implicitamente. Adapter exige challenge/status-reset resueltos, rechaza vivo y no aplica Healing Charm. Formula floor(percent*maxHP), minimo uno segun PokemonHpRestoreModifier.
2. Regresiones 456-459 escritas; generacion permitida, tests/compilacion aplazados. Politica baseline Classic sin challenge/status no portado; desafios, status completo y selector visual pendientes.

## Sacred Ash canonico

1. Perfil AllPokemonFullReviveModifierType porta allParty/100%; adapter valida todo antes de escribir, usa activo real y solo revive debilitados. Vivos conservan HP.
2. Regresiones 460-462 escritas; tests/compilacion aplazados. Politica baseline sin status/desafios no portados.

## Recompensas Pokeball canonicas

1. Generador extrae AddPokeballModifierType simbolo/cantidad con provenance. Adapter resuelve enums nativos segun pokeball.ts pinned y cap 99 segun data/pokeball.ts. Claim reemplaza constantes de cantidades por perfiles; simbolo futuro desconocido falla sin mutar.
2. Regresiones 463-465 escritas; generacion permitida, tests/compilacion aplazados. Classic completo sigue pendiente.

## Subida de nivel: conservar el estado del actor

1. `recalculatePokemonBattleLevel` actualiza únicamente nivel, estadísticas y HP, siguiendo `Pokemon.calculateStats` del snapshot pinned (`src/field/pokemon.ts`). El flujo de EXP de victoria utiliza este helper.
2. Conserva Unburden, daño acumulado, etapas, opción de pausa de evoluciones y PP actual/máximo. Un Pokémon debilitado permanece con HP cero; entradas inválidas no publican cambios.
3. Regresiones 522–524 escritas, pendientes de ejecución. Rare Candy sigue pendiente de amistad y de su flujo de decisiones; no se trata como una recompensa soportada.

## EXP después de Rare Candy

1. `applyPokemonExperience` acepta niveles superiores al cap de wave y EXP previa inferior al umbral del nivel actual, estados posibles tras `PokemonLevelIncrementModifier.apply`. Replica `PlayerPokemon.addExp` pinned: conserva el nivel y, al alcanzar/superar el cap, fija EXP a `max(umbral del nivel actual, EXP previa)`.
2. Regresiones 525–526 escritas para esos estados, sin ejecutar. Esto elimina un rechazo incompatible; todavía no habilita Rare Candy sin amistad, progreso de caramelos y decisiones pendientes.

## Recompensas de nivel: catálogo y planificación

1. Rare Candy/Rarer Candy generan `LevelIncrementItemProfile` desde sus constructores canónicos reales, con ID, alcance individual/equipo y provenance. No se infiere el alcance desde IDs locales.
2. `planPokemonLevelIncrement` aplica incremento de uno más stacks de Candy Jar (máximo upstream 99), calcula EXP bajo el límite sin cap solicitado y conserva EXP por encima de ese límite. La policy del booster debe estar resuelta. Las salidas indican amistad y LevelUpPhase pendientes; no consume el item.
3. Fuentes pinned inspeccionadas: `src/modifier/modifier-type.ts`, `PokemonLevelIncrementModifierType`/`AllPokemonLevelIncrementModifierType`; `src/modifier/modifier.ts`, `PokemonLevelIncrementModifier.apply`/`LevelIncrementBoosterModifier.apply`; `src/battle-scene.ts`, `getMaxExpLevel`. Representación de EXP sigue limitada a uint32; overflow no fabrica un resultado.
4. Regresiones 527–531 escritas, pendientes de ejecución. Recompensas de nivel todavía no habilitadas en `claimRewardChoice`: falta amistad persistente, starter candy y cola de decisiones por destinatario.

## Amistad del Pokémon: inicialización y persistencia

1. Todas las 1084 especies generan `SpeciesFriendshipProfile` desde `baseFriendship` del registro upstream preservado. Se conserva provenance; un dato ausente/inválido detiene la generación.
2. El actor inicializa amistad desde ese perfil. Subida de nivel, evolución y cambio de forma conservan su valor, en lugar de reiniciarlo al valor de la nueva especie.
3. Payload de Pokémon `pokemon=5` añade amistad. Lee versiones 1–4 con migración explícita a amistad base pinned; esos formatos nunca guardaron el valor. Captura/restauración de actor preserva la amistad. Un único miembro con amistad modificada exige snapshot explícito de equipo.
4. Regresiones de v5/v4 y conservación al subir de nivel escritas, pendientes de ejecución. Todavía faltan ganancias/pérdidas de amistad, starter candy, amistad de fusión y condiciones evolutivas; Rare Candy permanece bloqueado.

## Amistad y caramelos: reglas separadas

1. `planPokemonFriendshipChange` conserva la semántica de `Pokemon.addFriendship` y `PokemonFriendshipBoosterModifier.apply`: pérdida sin multiplicadores/caramelos, booster con floor, cap sin reducir amistad ya superior, límite 255 y señal de callbacks de amistad máxima. El progreso de caramelos usa la ganancia completa después del booster, no la diferencia de amistad del actor.
2. `planStarterCandyProgress` prepara premios múltiples y residuo; si `GameData.addStarterCandy` rechaza el premio, conserva `cap - 1`. Policy desconocida y overflow fallan sin publicar resultados. Fuentes pinned: `src/field/pokemon.ts`, `addFriendship`; `src/modifier/modifier.ts`, `PokemonFriendshipBoosterModifier.apply`.
3. Son planes para una transacción, no persistencia del perfil de jugador. Faltan ledger de starter candy, raíces de fusión/timed events, awards/ribbons y conexión a victoria/faint/Rare Candy. Regresiones 532–540 escritas, pendientes de ejecución.

## Evolución: estado conservado y pausa upstream

1. `applySpeciesEvolution` conserva amistad, tags de pérdida de item, daño acumulado y el moveset con PP actual/máximo. La evolución exitosa desactiva `pauseEvolutions`, siguiendo `PlayerPokemon.evolve` (`src/field/pokemon.ts` pinned); la opción continúa siendo persistente antes de aceptar una evolución.
2. Entradas con HP/PP/etapas inválidos o stats no soportadas se rechazan antes de publicar el actor. Regresiones 468–469 escritas, pendientes de ejecución. El PP máximo se conserva como estado; esto no habilita todavía el consumo de PP Up.
3. No existe aún almacenamiento completo del perfil de jugador; `PokemonFreshProfile` solo resuelve naturalezas iniciales. Starter candy sigue pendiente de ledger persistente, no se declara conectado por los resolvers de planificación.

## PP máximo: snapshot explícito

1. Payload de Pokémon `pokemon=6` almacena PP máximo por slot. Captura y restore validan los valores permanentes derivados de cero a tres PP Ups según `PokemonMove.getMovePp` y `toDmgValue` pinned (`src/data/moves/pokemon-move.ts`, `src/utils/common.ts`). Slots vacíos requieren cero; PP restante nunca supera su máximo.
2. Lectura de v1–v5 conserva su antiguo significado de PP base. Un miembro con máximo modificado solicita snapshot explícito al guardar, evitando perderlo por replay de semilla.
3. Regresiones escritas: Tackle 40/42 persiste, v5 no acepta ese PP sobre el máximo base 35, máximo 41 se rechaza, v5 base conserva amistad. Fixtures v1–v4 mantienen sus layouts anteriores. Tests y compilación pendientes.
4. Esto no habilita aún PP Up/PP Max como recompensas ni overrides de Transform. Cambio de forma con PP aumentado sigue limitado por su propio validator; snapshots especiales de jefes requieren metadata ppUp/override explícita.

## PP Up y PP Max: recompensas canónicas

1. `PpUpItemProfile` genera `upPoints` desde `PokemonPpUpModifierType` upstream con provenance. PP Up y PP Max utilizan el mismo helper, sin lógica basada en sus IDs en UI.
2. `applyPokemonPpUpItem` reproduce el filtro de selección y `PokemonPpUpModifier.apply`: PP base de al menos cinco, máximo tres mejoras y PP gastado conservado. Overrides no representados y entradas inválidas se rechazan.
3. `claimRewardChoiceInPlace` aplica el perfil al miembro/slot seleccionado mediante `claimRecoveryRewardChoice`; el payload v6 conserva el resultado. El selector de presentación sigue pendiente de conexión por la otra IA.
4. Regresiones 474–478 escritas, sin ejecutar. Solo generación y `git diff --check`; compilación y tests permanecen aplazados.

## Debilitamiento: pérdida de amistad canónica

1. El importer incorpora `src/data/balance/starters.ts` pinned y preserva las constantes de ganancia por combate/Rare Candy, pérdida por faint y cap de Rare Candy en `pokemonFriendshipRules`, con repository/revision/ruta/símbolo/SHA-256. El generador produce los valores C++ desde esa extensión canónica.
2. `finishBattleTurn` aplica la pérdida de amistad al jugador con HP cero antes de publicar el estado y enviar la reserva o finalizar el combate. El frontier actual excluye instant-revive modifiers, cuya policy futura debe preceder a la pérdida. Fuente: `FaintPhase.doFaint`, `src/phases/faint-phase.ts`.
3. `applyPokemonFaintFriendship` utiliza la rama negativa de `addFriendship`: mínimo cero, sin boosters, timed events ni modificación de starter candy. El valor resultante persiste mediante el payload del actor.
4. Regresiones 541–543 escritas, pendientes de ejecución. No aplica una pérdida durante la restauración de un checkpoint ya debilitado. Ganancias positivas y perfil persistente de starter candy siguen pendientes.

5. Reimportación local completa ejecutada dos veces con los mismos pins: hash `480b1150389ea156949fad9e04ea31bb80f026d4ddbe4703c7846b14a7dacef3`, igualdad comprobada por el pipeline. El hash cambió al añadir reglas canónicas; la migración de saves ligados al hash anterior sigue pendiente. No se ejecutaron suites ni se compiló el programa.

## Especie raíz para progreso de caramelos

1. `pokemonRootSpecies` reproduce `PokemonSpecies.getRootSpeciesId` pinned (`src/data/pokemon-species.ts`) usando las preevoluciones canónicas de SpeciesDataRegistry. La opción `forStarter` termina ante un starter elegible; amistad usa el recorrido completo, como `addFriendship`.
2. `trainerPartyRootDex` reutiliza ese resolver y deja de imponer 16 pasos. Referencias ausentes/ciclos fallan; el límite de recorrido viene del número de registros del catálogo. `pokemonFriendshipStarterSpecies` expone la clave canónica para el futuro ledger del perfil.
3. Regresiones 544–545 escritas sobre Ivysaur/Bulbasaur y todas las especies reales, pendientes de ejecución. Esto identifica destinatarios de progreso; el almacenamiento persistente de starter candy y la raíz de fusión siguen pendientes.

## Formato separado de starter candy

1. `NativeStarterCandyProfile.hpp` define registros por SpeciesId raíz, cantidad de caramelos y progreso de amistad, independientes de una run. El codec sin heap conserva orden canónico estricto, hash del catálogo, generación y SHA-256. Su tamaño depende del catálogo, no del límite de 8 KiB del journal de runs.
2. Decode valida checksum, versiones, referencias raíz, duplicados, límites y capacidad antes de publicar registros. El caller debe resolver el límite pinned de caramelos; no se presupone en producción. No acepta solapamiento entre buffer y registros.
3. Regresiones 47–50 escritas en el harness de almacenamiento: roundtrip, corrupción y duplicados; sin ejecutar. El formato existe, pero journal/SD, límite canónico generado y consumidor de gameplay siguen pendientes. No se declara perfil persistido en consola.

## Journal y SD del perfil de caramelos

1. `NativeStarterCandyStore` reutiliza `NativeSaveStorage`: dos slots, generación monotónica, SHA-256, slot inactivo, readback y exportación del último perfil validado. Scratch pertenece al caller y no se reserva en la pila del store; errores no publican generación ni registros. Slots íntegros con igual generación y contenido distinto fallan como ambiguos.
2. `SdNativeStarterCandyStorage` reutiliza read/write/flush/fsync existentes, con archivos separados `saves/starters0.p3profile`, `saves/starters1.p3profile` y `exports/starters.p3profile` bajo `sdmc:/3ds/pokerogue`. El límite de bytes del perfil deriva del catálogo; los saves de run conservan su límite anterior.
3. Regresiones 51–56 escritas: escritura interrumpida, recuperación del slot anterior, siguiente generación, exportación, corrupción y ambigüedad. Pendientes de ejecución; SD física sin validar.
4. Falta el consumidor de gameplay, límite/umbrales de caramelos generados desde upstream, coordinación transaccional con la run e importación del perfil exportado. El journal no demuestra por sí solo que Rare Candy esté conectado.

## Límites y premios de caramelos desde upstream

1. `starterCandyRules` normaliza `MAX_STARTER_CANDY_COUNT` desde `src/constants/game-constants.ts`, el multiplicador Classic y la tabla/fallback de `getStarterValueFriendshipCap` desde `src/data/balance/starters.ts`, preservando repository/revision/ruta/símbolo/SHA-256 y cuerpo original. El parser reconoce fallthrough y rechaza sintaxis no representada, sin fabricar thresholds.
2. Las tablas C++ generan el límite, multiplicador y umbrales por coste. Los overloads de producción del journal usan el límite generado; APIs con policy explícita siguen disponibles para overrides/fixtures.
3. `applyNativeStarterCandyFriendship` actualiza un registro raíz según el coste canónico: premios múltiples, residuo, cap de inventario y progreso `cap - 1` si no se acepta el premio. Emite cantidad solicitada/aplicada; errores no publican registro/evento.
4. Regresiones de importación/provenance y nativas 57–63 escritas, sin ejecutar. Falta consumidor de gameplay, timed events/fusión, coordinación durable con la run y callbacks; aún no se habilita Rare Candy por la existencia del store.

5. Reimportación completa duplicada con pins intactos: hash `90f3866eb2663f7c79773f01288b2aa9cf56d4fa14bbf611c881464ed4ac2863`, reproducible entre ambas pasadas. El cambio de contenido sigue requiriendo migración explícita de saves/perfiles con hash anterior; no se relaja esa validación.

## Importar el perfil exportado

1. `NativeStarterCandyStore.importExport` usa `readExport`, valida checksum/catálogo/raíces/límites y prepara registros en staging del caller. Después escribe mediante el journal y readback existentes. La generación del archivo externo se reemplaza por la siguiente generación local.
2. Staging no es el perfil vivo: el consumidor solo debe publicar gameplay tras éxito. Count/generación de salida permanecen sin cambios si falla la operación; una exportación corrupta no modifica el journal.
3. Regresiones 64–66 escritas: roundtrip, segunda importación con generación local creciente y rechazo de export corrupto conservando el perfil previo. Sin ejecución ni validación de SD física. Falta menú de importación y coordinación con la run antes de habilitar ganancias en gameplay.

## Perfil comprometido y candidato pendiente

1. `loadGeneration` carga la generación concreta referenciada por una run, sin sustituirla por el perfil más reciente. Si no existe, falla explícitamente.
2. `prepareFromCommitted` escribe un candidato en el slot opuesto al perfil comprometido. Si una run aún referencia generación 1 y existe candidato 2, un reintento escribe candidato 3 sobre el slot pendiente, conservando 1. Esto evita destruir el perfil necesario para recuperar la run anterior.
3. Regresiones 67–72 escritas: interrupción entre perfil/run simulada, reintento y recuperación de generaciones exactas; sin ejecutar. Falta persistir la referencia en NativeRunSave y conectar el coordinador al arranque/checkpoints/exportación. Todavía no se declara transacción conjunta completa.

## Referencia del perfil en el guardado de la run

1. Envelope/runtime v12 almacena `starterProfileGeneration` dentro del contenido protegido por SHA-256. Cero identifica una run legacy o todavía sin perfil asociado; una referencia positiva exige cargar esa generación exacta.
2. Lectura v1–v11 conservada; v11 mantiene inventario y migra la referencia a cero. El codec no consulta SD ni sustituye una generación ausente.
3. Regresiones 73–75 escritas para roundtrip de la referencia y migración v11, pendientes de ejecución. Falta conectar el coordinador al arranque/checkpoints/exportación y preservar la referencia al capturar snapshots de gameplay; no se declara transacción conjunta completa.

## Commit conjunto de run y perfil

1. `NativeProgressStore` compone los journals existentes: valida snapshot, comprueba la referencia confirmada en disco, prepara perfil preservando la generación anterior y publica la nueva referencia mediante el journal de run. Rechaza una referencia obsoleta antes de escribir. Un único owner debe serializar estas operaciones.
2. `load` resuelve exactamente el perfil de la run; una run sin referencia exige bootstrap explícito y no recibe recompensas retroactivas. Tras un error de I/O se debe recargar antes del reintento, porque el error puede ocurrir después de una escritura durable.
3. FirstRunRuntime conserva la referencia al restaurar/capturar, incluso para snapshots de setup. El coordinador todavía no sustituye los consumidores SD/QuickJS del programa; la exportación conjunta y el perfil vivo de gameplay siguen pendientes.
4. Regresiones 76–83 escritas: bootstrap, interrupción entre perfil/run, reintento, referencia obsoleta y fallo de perfil con recuperación anterior. Roundtrip de runtime amplía la prueba existente. Sin ejecución ni compilación por instrucción del usuario.

5. `exportGeneration` exporta el perfil confirmado por referencia, aunque exista un candidato más reciente, y verifica el archivo leído después de escribirlo. Regresiones 84–86 escritas, no ejecutadas. Esto no hace atómica una exportación de dos archivos: falta empaquetar/importar ambos como una unidad portable y conectar consumidores.
6. La fixture de migración v10 ahora parte de una cabecera v12 y comprueba las constantes de versión actuales; conserva las comprobaciones del inventario y del Pokémon anterior.

## EXP del actor y policy de caramelos

1. Inspección pinned: `src/modifier/modifier.ts`, `PokemonLevelIncrementModifier.apply`, conserva EXP si el nivel supera `getMaxExpLevel(true)`. `src/battle-scene.ts`, `getMaxExpLevel`, retorna normalmente Number.MAX_SAFE_INTEGER con ignoreLevelCap; un override positivo cambia ese límite. No se presenta la conservación de EXP como regla normal de Classic.
2. El codec de Pokémon conserva nivel/EXP independientes y sigue validando especie, fórmula representable, identidad, HP, IVs y PP. El validator de run mantiene sus capacidades/límites actuales; no se habilitan overrides ni Rare Candy mediante esta corrección.
3. Regresiones 501–505 escritas: especie real, plan con override explícito, recalculación de stats, captura, encode/decode y restauración sin perder EXP/HP/friendship. Pendientes de ejecución; sin compilación.

## Validación de perfil conectada a SD y QuickJS

1. El host crea `SdNativeStarterCandyStorage`/journal con scratch estático derivado del catálogo y lo enlaza a `NativeRunSaveStore`, compartido por comandos nativos y QuickJS. Loads y saves con referencia positiva verifican la generación exacta, checksum, hash y registros del perfil; sin journal enlazado fallan explícitamente. La salida de load no se publica si falla el perfil.
2. Partidas legacy con referencia cero siguen legibles. El coordinador enlaza ambos stores al construirse. El guardado actual del juego todavía no aplica ganancias de perfil ni llama al commit conjunto; no se declara Rare Candy conectado.
3. Regresiones 87–89 escritas: inspección exacta, perfil ausente sin publicar run/escribir checkpoint y consumidor sin binding. Tests, compilación, Azahar y SD física permanecen pendientes.

4. Una referencia ausente se comunica como InvalidRecord al consumidor de run, no NotFound: evita que el arranque la confunda con la ausencia de partida e inicie silenciosamente un progreso nuevo.

## Operación conjunta de amistad y caramelos

1. Fuente pinned inspeccionada: `src/battle-scene.ts` reparto de EXP concede amistad a participantes vivos con pokemonDefeated=true, incluso cuando no reciben EXP por el cap. Captura no equivale a derrota. `src/field/pokemon.ts`, `Pokemon.addFriendship`, resuelve boost, callbacks de amistad máxima, timed events/fusión y actualiza raíces.
2. `applyNativePokemonFriendship` reutiliza planners/ledger existentes y prepara actor+registro raíz antes de publicar cualquiera. Policy no resuelta, raíz incorrecta, overflow o callbacks máximos pendientes rechazan el cambio; pérdidas no alteran el progreso de caramelos. El caller debe preparar ambas raíces para fusiones antes de publicar.
3. Regresiones 506–512 escritas sobre un actor real: policy pendiente, ganancia Classic, callbacks de máximo, pérdida y raíz inválida. Sin ejecutar. Esta operación todavía no se invoca desde reparto de EXP ni recompensas: falta seguimiento completo de participantes, perfil vivo y conexión durable; no se concede amistad falsa a capturas ni se declara Rare Candy conectado.

## Distribución numérica de EXP entre participantes

1. `pokemonParticipantExperience` sigue `src/battle-scene.ts`, `applyPartyExp`: floor previo del bonus trainer, división por número de participantes, bonus múltiple/EXP Share, Pokérus, override, booster y floor por receptor. Eligibility y policies deben estar resueltas; cero participantes o receptor excluido concede cero. ExpBalance es una pasada posterior todavía pendiente.
2. El resolver single-participant usado por gameplay delega en el mismo cálculo con policy neutral explícita. No se conecta todavía el reparto a toda la party ni la amistad: falta registrar/persistir participantes y preparar todas las colas de aprendizaje/evolución antes de publicar.
3. Regresiones 513–519 escritas: policy pendiente, dos participantes, bonus/Pokérus, reserva con EXP Share, override/booster, receptor capped y ausencia de participantes. Sin ejecutar ni compilar. El seguimiento upstream añade actores en TurnInitPhase y gestiona altas/bajas en FaintPhase; no equivale simplemente a marcar cada switch como participación inmediata.

## Persistencia del historial de participantes

1. Envelope/runtime v13 añade historial resuelto, count y IDs de identidad Pokémon ordenados; no índices de party ni especies. Valida orden único, capacidad de seis miembros, referencias a snapshots explícitos y datos sobrantes. ID cero es una identidad válida cuando está dentro del count.
2. v1–v12 continúan legibles: el historial se migra como desconocido, sin inventar qué reservas participaron. v12 conserva la referencia de perfil. El registro de TurnInit/Faint y su consumo por el reparto quedan pendientes de conectar al runtime.
3. Regresiones 90–94 escritas para roundtrip, duplicados, historial inconsistente y migración v12; fixtures anteriores ahora parten de cabecera v13. Sin ejecutar ni compilar por instrucción del usuario.

4. FirstRunRuntime registra el actor activo al aceptar ataques, captura o switch (TurnInit conceptual antes del comando); agrega el activo ante faint de cualquier actor y lo retira si cae el player. Conserva el set durante sustituciones de enemigos de entrenador y lo reinicia con la nueva batalla. IDs ordenados se capturan/restauran; restore también valida pertenencia contra actores reconstruidos. Historial legacy desconocido no se transforma en un set inventado.
5. Roundtrip de runtime ampliado con identidad real. Falta conectar la distribución a todos los participantes y colas de aprendizaje/evolución; los nuevos IDs no se presentan como reparto completo. Tests/compilación permanecen pendientes.

6. Regresiones 520–521 añaden rechazo de una identidad ajena durante reconstrucción de la run y verifican que el historial anterior no se publica parcialmente. Pendientes de ejecución.

## EXP de equipo y decisiones por destinatario

1. `grantVictoryExperience` prepara una copia del equipo, distribuye por IDs del historial, recalcula cada receptor vivo bajo el cap y publica EXP/stats/moves solo tras preparar todos los miembros. Sin EXP Share, reservas no participantes reciben cero; actuales policies neutrales no habilitan modificadores todavía no representados. Historial legacy conserva el replay anterior, explícitamente sin demostrar reparto antiguo exacto.
2. Cola acotada a los seis miembros existentes: decisiones de movimientos y evolución apuntan al destinatario; aceptar/rechazar/pausar en una reserva no cambia el actor activo de combate. Scene y lista de moves enviada a QuickJS consultan el destinatario durante aprendizaje. Checkpoints continúan rechazados mientras haya decisiones pendientes.
3. Replays implícitos de saves anteriores siguen reconstruyendo el camino single-starter; al terminar restore se publica el historial persistido. En nuevo gameplay se utiliza la identidad real de participantes.
4. Regresiones 522–529 escritas con encuentro reconstruido y equipo capturado real: dos participantes, EXP esperada por ambos, drenaje de decisiones y restore de ExperienceGranted sin repetir EXP. Sin ejecutar/compilar. Falta amistad/perfil vivo durable, EXP modifiers/Pokérus de actor/ExpBalance, dobles por derrotas individuales y validación completa de todas las colas en consola.

5. Un historial legacy desconocido con varios miembros bloquea la concesión de EXP antes de publicar cambios: no inventa al participante activo como sustituto de todo el historial. La lectura de la run se conserva, pero necesita una migración explícita para conceder recompensas pendientes con ese estado.
6. Fixtures de KO anteriores declaran ahora al actor real participante al fabricar BattleWon; sus asserts se conservan. Regresiones 530–531 añaden reserva no participante y rechazo atómico de historial desconocido con dos miembros. Todas pendientes de ejecución.

## Regresión de aprendizaje de una reserva

1. Regresiones 532–540 escritas a partir de una identidad capturada y su learnset canónico: reserva a un EXP del próximo movimiento, cuatro slots ocupados y actor activo en cap. La cola debe seleccionar al miembro 1; reemplazar su slot no cambia el moveset ni EXP del actor activo. Las decisiones restantes se drenan por los comandos existentes.
2. No se cambia ningún assert previo; los niveles y movimientos se resuelven del catálogo. La policy pauseEvolutions de esta fixture aísla la selección del destinatario de los callbacks de evolución.
3. Historial multiparty desconocido ahora muestra feedback explícito al rechazar la concesión. Regresiones sin ejecutar; tests, compilación y validación Azahar/Old 3DS siguen pendientes.

## Perfil vivo en la progresión de combate

1. FirstRunRuntime puede adjuntar un perfil validado por raíces/orden/límites y generación exacta. Al conceder EXP prepara copia de actores y ledger; participantes vivos reciben amistad por derrota incluso en el cap de EXP. Raíces ausentes se insertan ordenadas; callbacks máximos no migrados rechazan la operación completa. Sin perfil adjunto el camino anterior permanece como bridge y no se presenta como progreso permanente completo.
2. Captura llama a la concesión con pokemonDefeated=false y no añade amistad por derrota. El policy de amistad debe resolver boosters, timed events y fusión; el host todavía debe seleccionar y adjuntar esa policy.
3. `saveNativeProgress` captura checkpoint y usa NativeProgressStore para guardar ledger y run; confirma la nueva referencia solo después de éxito. Esto todavía no sustituye llamadas de SD/QuickJS del host; falta carga/adjunción/exportación/importación conjunta en el programa.
4. Regresiones 541–543 escritas: dos participantes vivos con ledger de raíces, amistad+caramelos y rechazo atómico por callback de máximo pendiente. Sin ejecutar/compilar. La copia del ledger deriva del catálogo y aumenta el uso de stack en transacciones: debe revisarse en la etapa de compilación y memoria Old 3DS antes de declarar jugabilidad verificada.

5. Restore de run desadjunta siempre el perfil vivo: conservarlo por igualdad de generación podría arrastrar ganancias todavía no guardadas al recargar actores antiguos y duplicarlas. El consumidor debe cargar de nuevo el perfil durable exacto y adjuntarlo después de restaurar.

6. Regresión 544 escrita: rollback de actores tras ganar amistad desadjunta el ledger no confirmado y restaura la amistad anterior. Pendiente de ejecución.

## Perfil durable conectado al host SD y QuickJS

1. Arranque y load nativo/QuickJS usan `loadNativeProgress`: leen run y perfil exacto, preparan staging fuera de la pila del host y publican actores+ledger juntos. Restore sin perfil sigue disponible para el bridge/test; producción adjunta perfil. Saves nativos/QuickJS llaman `saveNativeProgress` y confirman la referencia después del commit conjunto.
2. El host declara policy offline explícita con multiplicador Classic generado, sin timed events, fusión ni boosters no representados. Perfil inexistente de una nueva/legacy run se inicializa vacío; corrupción/content mismatch no se convierte en perfil falso.
3. Exportación escribe y verifica el perfil confirmado y la run (`starters.p3profile` + `progress.p3save`); todavía son dos archivos, no un bundle portable atómico. Import linked aislado se rechaza: secuencia extranjera puede coincidir con un perfil local distinto. Import legacy sin referencia permanece disponible; empaquetado/import conjunto pendiente.
4. Regresiones 545–554 escritas: derrota real con dos participantes → amistad/ledger → commit → reload, interrupción entre perfil/run → perfil anterior, export de generación confirmada e import linked aislado rechazado. Sin ejecutar/compilar; SD física y consumo de stack pendientes de validar en Old 3DS.

## Reducción del estado persistente en la pila del host

1. El runtime vivo, el envelope usado por el host y el runtime de preflight se mantienen en almacenamiento estático; el preflight es serial en el loop principal. El ledger derivado del catálogo no queda ocupado permanentemente en la pila de main.
2. NativeProgressStore.commit reutiliza un solo envelope para estado previo, candidato y readback; conserva el orden perfil → run → readback y la publicación de referencia solo tras éxito. No añade heap ni modifica el formato de SD.
3. Siguen existiendo snapshots, actores candidatos y buffers de journal temporales; este cambio no prueba el máximo real de pila. Las regresiones de commit/interrupción escritas anteriormente cubren la semántica que debe conservarse; ejecución, compilación y medición Old 3DS siguen aplazadas.

## Formato portable conjunto de run y perfil

1. `NativeProgressBundle.hpp` define P3PROG01: longitudes acotadas, payloads existentes de run/perfil y SHA-256 exterior que vincula ambos. Conserva los checksums internos, exige content hash esperado y generación de perfil coincidente con la run; no usa reloj, heap ni IDs alternativos.
2. El inspector devuelve vistas prestadas solo después de validar toda la pareja; el envelope de scratch es workspace mutable, no estado vivo. El encoder rechaza solapamientos y escribe bytes deterministas con buffers del caller. Límite total deriva de los límites de ambos componentes.
3. Regresiones 95–102 escritas: pareja real codificada, inspección, reproducibilidad byte a byte, corrupción sin publicar vistas, hash incompatible, generaciones distintas, versión desconocida y longitud fuera de rango. Sin ejecutar/compilar.
4. El formato todavía no está conectado a SD/export/import del host. Falta archivo portable único, readback, preflight de reconstrucción de run, rebase a generación local y commit conjunto. No se declara importación portable completa por la existencia del codec.

## Progreso portable emparejado — pendiente de validación final

1. `NativeProgressStore` conecta codec `P3PROG01` con SD: `sdmc:/3ds/pokerogue/exports/progress.p3progress`, partida y perfil exacto bajo un digest. Exporta y verifica lectura completa; journal local conserva autoridad ante exportación interrumpida.
2. Importación nativa con R: valida hash de contenido, checksum y referencia de perfil, reconstruye runtime antes de escribir y reasigna generaciones locales. Perfil preparado primero; partida confirma la pareja. Archivo corrupto no dispara fallback silencioso al export antiguo.
3. Y exporta el bundle tras guardar. En modo QuickJS sano L/R todavía pertenecen al bridge save/load: falta exponer importación portable en su menú. No se declara interoperabilidad con el save web ni OTA firmada.
4. Regresiones 103–111 escritas para transporte, generación extranjera, corrupción, exportación interrumpida y commit de partida interrumpido. Tests y compilación siguen aplazados; uso real de SD/Azahar/Old 3DS no verificado.

## Bridge QuickJS y progreso portable — pendiente de ejecución

1. Bindings `_3ds_exportNative` / `_3ds_importNative` encolan comandos 206/207. I/O ocurre en `processPendingAction`, antes del render; staging y replay son prestados por el host, sin filesystem ni mutación de combate desde JS.
2. HUD diagnóstico: L guarda, R carga, Y exporta y X importa `.p3progress`. El host evita procesar esos mismos botones cuando QuickJS está sano. Modo nativo conserva X guardar/Y exportar/L cargar/R importar.
3. Preflight compartido nativo/QuickJS reconstruye partida y adjunta el perfil validado antes del commit. La generación extranjera se reasigna localmente y los presenters se invalidan tras importación exitosa.
4. Cuatro regresiones del script real verifican prioridad de almacenamiento sobre ataque y ausencia de repetición sin nuevos pulsos. Escritas, sin ejecutar. La validación SD/QuickJS/Azahar/Old 3DS y Classic completo siguen pendientes.

## Caída simultánea con reserva viva — pendiente de ejecución

1. `finishBattleTurn` determina victoria/derrota por equipo legal restante y enemigos activos. Si activo y último enemigo caen pero queda reserva viva, reemplaza al activo y entra en victoria pendiente; no inicia un turno adicional contra el enemigo caído.
2. Sin reservas legales mantiene derrota, incluso con ambos campos caídos. Fuente inspeccionada: PokéRogue pinned `src/phases/faint-phase.ts`, `FaintPhase.start` (`getPokemonAllowedInBattle`, `GameOverPhase`, `VictoryPhase`). Reservas de entrenador conservan su resolución posterior.
3. Cuatro regresiones de la decisión final escritas (555–558); sin ejecutar. No prueban todavía la cola completa de fases/animación ni los efectos de faint que siguen pendientes.

4. Regresión de integración 559–564 escrita: equipo capturado real + Take Down canónico, HP de checkpoint reducido para provocar KO por recoil; exige victoria con reserva y checkpoint sin turno extra. Explora índices de turno deterministas por precisión/orden y falla si no encuentra el escenario. Pendiente de ejecución.

## Limpieza de estado de summon — pendiente de ejecución

1. `resetPokemonSummonState` reutiliza la limpieza de stages y limpia Unburden/daño del turno; no cambia HP, PP, amistad, identidad, forma persistente ni pausa de evolución. Fuente pinned: `src/field/pokemon.ts`, `Pokemon.resetSummonData`.
2. Conectado a cambios voluntarios/forzados, entrada y retirada de entrenadores y recalls en transiciones de arena. La reserva saliente conserva su estado persistente y pierde los campos transitorios soportados.
3. Regresiones 565–568 escritas para limpieza, preservación e idempotencia. Pendientes de ejecución; tags/transformaciones y callbacks completos siguen pendientes.

## Pesos de recuperación para el equipo — pendiente de ejecución

1. `InitialClassicRewardWeights` recibe los actores reales del equipo en `generateVictoryRewards`; Potion/Super/Hyper/Max, Ether/Elixir y variantes cuentan miembros elegibles hasta tres, con umbrales pinned.
2. Revive pesa nueve por caído hasta tres; Max Revive tres por caído hasta tres; Sacred Ash se habilita desde ceil(tamaño/2) caídos. Fuente inspeccionada: `src/modifier/init-modifier-pools.ts`, `initCommonModifierPool` / `initGreatModifierPool`.
3. Regresiones 569–574 escritas con entradas canónicas reales, activo sano/reservas heridas, PP bajos, cap de tres, revives y estado inválido. Sin ejecutar.
4. Pesos de PP asumen ausencia de Leppa, coherente con el inventario held actualmente soportado. Status, lures existentes, economía, otras familias y supresión por límite de stacks siguen pendientes; el provider no es aún el pool upstream completo.

## Pool e inventario de balls — pendiente de ejecución

1. Pesos Common de Poké Ball y Great de Great Ball leen inventario real: peso cero al máximo Classic, seis por debajo; inventario superior al límite falla explícitamente.
2. Límite 99 compartido con la aplicación del reward desde `src/data/pokeball.ts`, `MAX_PER_TYPE_POKEBALLS`; decisión de pool desde `src/modifier/init-modifier-pools.ts`, `hasMaximumBalls`. No es capacidad máxima del engine ni regla aplicada a otros modos.
3. Regresiones 575–577 escritas para 98/99, independencia por tipo e inventario inválido. Sin ejecutar. Pesos completos Ultra/Rogue/Master siguen pendientes junto a sus otros modificadores.

## Recompensas interactivas QuickJS — pendiente de ejecución

1. Confirmar tras EXP abre el pool nativo en lugar de saltarlo. El HUD proyecta IDs de opciones y selección desde el runtime; Up/Down selecciona, B salta explícitamente.
2. Recuperación soportada abre elección de miembro y slot: Up/Down elige actor, Left/Right slot, A encola 300–323. El host llama `claimRecoveryRewardChoice`; valida/aplica atómicamente y conserva el reward ante destinatario inválido.
3. Regresión JS escrita sobre el script real: generación sin skip automático, reserve + slot, cancelación y skip explícito. Bundle regenerado sin compilar; pruebas pendientes.
4. HUD sigue diagnóstico: nombres/locales finales, información detallada de HP/PP para cada destinatario, selección de destinatarios held y todas las familias de rewards todavía requieren integración.

## Destinatarios de held rewards QuickJS — pendiente de ejecución

1. El snapshot clasifica rewards held mediante `initializeHeldModifierInstance` + el dispatch soportado, igual que el engine. No se clasifican por strings dentro del HUD.
2. Confirmar abre el selector de miembro y encola 330–335; el host ejecuta `claimHeldRewardChoice`, conservando propietario por identidad, stacks y aplicación atómica existentes. Left/Right no elige moves para held items.
3. Regresión del script real escrita para selección de reserva, cancelación y skip explícito; sin ejecutar. Familias held sin dispatch, reemplazo de stacks y presentación final permanecen pendientes.

## Estado de destinatarios en el HUD — pendiente de ejecución

1. Snapshot QuickJS expone nombre localizado, HP/maxHP y moves/PP/maxPP por miembro desde actores reales. El activo usa su estado de campo, no una copia de reserva potencialmente atrasada.
2. Selector de rewards muestra HP del equipo y nombre/PP del slot elegido. Son datos de presentación; validación de elegibilidad permanece en el comando C++.
3. Regresión del script ampliada para comprobar HP y PP de una reserva. Sin ejecutar; bundle regenerado. Diseño visual definitivo y validación de memoria/rendimiento en consola siguen pendientes.

## Fallos de transición de bioma — pendiente de ejecución

1. `resolve` deja el encuentro sin resolver si la transición canónica falla, con motivo explícito; no conserva silenciosamente el bioma anterior. Los comandos de reward/skip conservan su rollback por candidato existente.
2. Claim no sustituye un error de transición por el mensaje de éxito. Diferencia ruta/bioma faltante, modo no soportado, elección pendiente/inválida y entrada inválida.
3. Regresiones 578–580 escritas para ruta faltante, modo no soportado y motivos distintos de elección; sin ejecutar. Guardado completo de historia de rutas y elección Map aún pendientes.

## Amistad por enemigos derrotados en dobles — pendiente de ejecución

1. El frontier actual agrega EXP al final de dobles, pero ahora aplica una ganancia de amistad por enemigo derrotado: dos derrotas generan dos ganancias; derrotar uno y capturar el último genera una. Captura simple no genera amistad de derrota.
2. Fuente pinned inspeccionada: `src/phases/victory-phase.ts`, `VictoryPhase.start` → `applyPartyExp(expValue, true)`; captura ya utiliza `grantVictoryExperience(false)`. La captura con ambos enemigos vivos permanece bloqueada, por lo que el caso captura final tiene exactamente una derrota previa.
3. Ganancias preparadas una a una en actor/ledger candidatos; fallo en callbacks conserva atomicidad. Regresiones 581–584 escritas para conteo y actor real + ledger Classic; sin ejecutar.
4. Distribución temporal de EXP/amistad al ocurrir cada faint, cambios de participantes entre caídas y cola completa de fases permanecen pendientes. No se declara paridad completa de dobles.

5. Se conserva snapshot de participantes en la primera caída de un doble. Si cambian antes de la segunda, la agregación falla explícitamente en vez de conceder al nuevo participante la derrota anterior; resolver ese caso requiere la cola por faint pendiente. Dobles aún no son checkpoints soportados.

## EXP de la primera caída en dobles — pendiente de ejecución

1. Una caída intermedia concede EXP/amistad del enemigo caído al finalizar el turno, usando los participantes actuales. Un bit por enemigo evita conceder otra vez esa derrota al caer/capturar el último.
2. Elimina el bloqueo previo por cambios posteriores de participantes: la derrota anterior ya fue aplicada. Decisiones de aprendizaje/evolución se procesan antes del siguiente comando de combate; rewards permanecen pendientes solo tras terminar el combate.
3. Regresión 585 escrita para máscaras pendientes/ya concedidas y ambos órdenes; sin ejecutar. Dobles siguen sin checkpoint soportado.
4. La cola por faint dentro del mismo turno, antes de movimientos posteriores, aún requiere refactor de fases; dos caídas simultáneas siguen usando la agregación existente. No se declara paridad completa de EXP/dobles con upstream.

5. Revisión estática corrigió la copia de equipo para EXP: `PresentationContext.playerParty` es array C; se prepara ahora un `std::array` independiente y se publica miembro a miembro después de resolver todo. Evita decaimiento a puntero, asignación inválida de arrays y mutaciones prematuras. Las regresiones de rollback de EXP existentes siguen pendientes de ejecución.

## Decisiones de nivel durante combate QuickJS — pendiente de ejecución

1. Snapshot identifica aprendizaje/evolución y confirmación de pausa desde el runtime. HUD da prioridad a esos estados antes de captura/cambio/ataque, incluso en la caída intermedia de dobles.
2. Slots de aprendizaje no se filtran según moveset del activo: `resolvePendingLearnMove` valida el destinatario de la progresión. B puede rechazar aprendizajes/evoluciones durante combate a través del comando 200 existente.
3. Regresiones JS escritas para ambas decisiones durante combate, rechazo sin captura y confirmación del slot elegido. Sin ejecutar. Presentación final de evoluciones y fase completa upstream aún pendientes.

## Propiedad del botón SELECT — pendiente de ejecución

1. El host ya no cambia `pauseEvolutions` al abrir el menú de equipo QuickJS. El toggle nativo de SELECT queda dentro del bloque de fallback sin bridge sano.
2. Regresión del script escrita para apertura sin mutación y confirmación de cambio; comprobación estática complementaria sitúa el toggle bajo el guard del host. No sustituye prueba del ejecutable, pendiente al final.

## Captura con equipo lleno — bloqueo explícito pendiente de reemplazo

1. Se detectó captura que eliminaba al enemigo sin añadirlo ni pedir liberar/reemplazar cuando el equipo tenía seis miembros. El comando ahora falla antes de consumir ball/RNG o modificar el combate.
2. Fuente pinned inspeccionada: `src/phases/attempt-capture-phase.ts`, `AttemptCapturePhase`, rama `PLAYER_PARTY_MAX_SIZE` / `addToPartyMenuConfig` / `PartyUiMode.RELEASE`. Implementar esa decisión sigue pendiente; el bloqueo no se declara soporte completo de captura.
3. Regresiones 586–588 escritas con party checkpoint de seis actores y comparación de inventario, enemigo, wave y próximo draw RNG; sin ejecutar.

## Corrección de semántica pinned: amistad tras captura

1. La inspección completa de `src/phases/attempt-capture-phase.ts`, callback `end`, mostró que captura exitosa encola `VictoryPhase`. `src/phases/victory-phase.ts`, `VictoryPhase.start`, llama `applyPartyExp(expValue, true)` sin excepción por captura.
2. Corrige la decisión anterior de `grantVictoryExperience(false)`: ahora aplica la ganancia de amistad y ledger del participante también tras captura exitosa. En dobles el mask ya concedido evita repetir la primera derrota.
3. Retirado helper numérico sin consumidor cuya hipótesis "captura no da amistad" era incorrecta; referencias de tests sustituidas por máscaras actuales. Regresiones 589–590 escritas sobre captura real + profile adjunto, sin ejecutar.
4. Las notas anteriores que excluían amistad por captura quedan supersedidas por esta evidencia pinned. Decisión con equipo lleno, estadísticas de capturas y callback completo de captura aún pendientes.

## Decisión tras captura con equipo lleno — pendiente de ejecución

1. Sustituye el bloqueo anterior para inventario sin held modifiers: captura exitosa consume una ball y pausa antes de retirar al enemigo. A confirma reemplazo de uno de los seis miembros; B rechaza incorporación y completa la captura. No descarta silenciosamente al capturado.
2. Runtime C++ conserva el actor capturado, decide mediante comandos y termina Victory/EXP una sola vez. QuickJS proyecta nombres/HP y selección nativa; no decide gameplay. Guardado rechaza este estado intermedio; cambio, captura adicional y mutación de held/evolution pause quedan bloqueados.
3. Regresiones nativas 586–588 y 591–593 y routing JS escritas, sin ejecutar. Fuente pinned: AttemptCapturePhase / addToPartyMenuConfig / PartyUiMode.RELEASE y PartyUiHandler.processReleaseOption.
4. Captura con equipo lleno y held modifiers sigue bloqueada hasta implementar eliminación/transferencia por propietario. Estadísticas, shiny/unlocks y callback completo de captura siguen pendientes. No se declara Classic completo.

## Objetos durante captura y liberación — pendiente de ejecución

1. Extiende la decisión de equipo lleno al frontier de held modifiers soportado. Retiene los propietarios del equipo final, elimina objetos del miembro liberado y conserva los del capturado con PID/stack/raw metadata originales. Rechazar incorporación elimina los objetos del enemigo sin tocar los del equipo.
2. Reutiliza retainPartyHeldInventory. Fuente pinned: AttemptCapturePhase.addToParty/removePokemon y PartyUiHandler.doRelease/removePartyMemberModifiers.
3. Modifiers sin dispatcher y limpieza de otros enemigos siguen rechazados explícitamente; callbacks de captura/profile completos aún pendientes. Tests y compilación aplazados.

## Reemplazo del activo con seis participantes — pendiente de ejecución

1. Retira la identidad liberada después del reparto de EXP y antes del registro del activo al cerrar el turno. Evita exigir siete slots temporales cuando ya participaron los seis miembros.
2. El capturado no recibe EXP por participación anterior a su incorporación. Regresiones 597–598 escritas con historial completo ordenado y reemplazo del activo; sin ejecutar. Tests/compilación permanecen aplazados.

## Checkpoint posterior a reemplazo por captura — pendiente de ejecución

1. Regresiones 599–602 escriben y restauran el checkpoint posterior a una captura con equipo lleno. Comprueban PID, HP, PP, coste único de Master Ball y ausencia del participante liberado.
2. El estado de selección sigue sin ser serializable; se guarda únicamente después de resolver la decisión. Suite sin ejecutar por aplazamiento del usuario; no se declara compatibilidad verificada.

## Consumo RNG de captura crítica — pendiente de ejecución

1. AttemptCapturePhase.start pinned consume randBattleSeedInt(256) antes de las sacudidas, también con probabilidad cero y Master Ball. El runtime ya consume esa tirada; antes la omitía y desplazaba el RNG posterior.
2. Regresión 603 escrita comparando el siguiente draw de captura Master Ball con la secuencia upstream. Sin ejecutar. Fixtures de captura con seeds anteriores pueden cambiar de resultado legítimamente; revisar al ejecutar la suite final, sin ocultar fallos.
3. Probabilidad crítica basada en caught-dex/Catching Charm, status y shiny event siguen pendientes. Esta corrección de secuencia no demuestra paridad completa de captura.

## Fórmula y entradas de captura — pendiente de ejecución

1. Sustituye llamada inexistente intInRange por randSeedInt(65536), API existente correspondiente al draw upstream. Revisión estática; sin compilación.
2. Elimina mínimo artificial de tasa 1 y clamp de probabilidad a 65535. La fórmula upstream permite tasa cero y reporta probabilidad superior al dominio RNG en captura garantizada.
3. Species sin perfil canónico, HP superior al máximo y tipo de ball inválido fallan antes de consumir RNG. Regresiones 604–605 escritas, sin ejecutar. Status/shiny/critical profile continúan pendientes.

## Resolvedor de captura crítica — conexión al perfil pendiente

1. executeCaptureAttempt acepta política explícita resuelta de caught-species, Daily/fresh-start y stacks de Catching Charm. Porta umbrales estrictos, clamp de tasa 255 y multiplicadores 2/2.5/3 del upstream pinned.
2. Captura crítica consume una tirada de sacudida después del draw de selección, incluyendo probabilidad garantizada. Regresiones 606–610 escritas para umbrales, fresh-start, charm y secuencia RNG; sin ejecutar.
3. FirstRunRuntime todavía no suministra esta política: el perfil candy/friendship carece de caughtAttr. Persistir Pokédex real y conectar los modifiers/profile sigue pendiente; no se declara captura crítica integrada en partidas.

## Perfil de especies capturadas — conexión gameplay pendiente

1. Extiende el perfil existente con caught por SpeciesId exacto, separado del progreso de starter/root. P3CANDY2 añade un byte validado por registro y permite especies evolucionadas sin progreso de caramelos inventado.
2. Lee P3CANDY1 conservando candy/friendship y caught=false: no reconstruye capturas desconocidas. Journal acepta ambas versiones; tamaños máximos siguen derivados del catálogo.
3. Regresiones codec 112–114 escritas para caught persistente y lectura legacy, sin ejecutar. Conectar successful capture y policy crítica al perfil sigue pendiente; todavía no se declara Pokédex completo.

## Capturas conectadas al perfil y al resolvedor crítico — pendiente de ejecución

1. Captura exitosa con perfil adjunto marca caught por especie y cadena prevolutionDex, antes de elegir incorporación, como setPokemonCaught/setPokemonSpeciesCaught pinned. La transacción candidata revierte el ledger si falla el comando.
2. executeCaptureAttempt recibe el número de especies caught del perfil real en Classic. Export/journal utilizan P3CANDY2 existente; no cuentan filas de amistad como capturas. Sin perfil adjunto permanece el diagnóstico previo, sin fingir persistencia.
3. Regresiones 590/611–612 cubren captura real, preevoluciones y codificación del perfil actualizado; sin ejecutar. Catching Charm, atributos form/nature/ability/shiny, starters iniciales y callbacks completos siguen pendientes.

## Inicialización de capturas del perfil nuevo — pendiente de ejecución

1. Host llama initializeFreshStarterProfile solo si no existen run ni perfil. Marca starters iniciales desde freshProfileStarter importado, siguiendo GameData.initDexData/defaultStarterSpecies.
2. Perfiles existentes, incluso legacy vacíos, mantienen sus datos; el método rechaza reinicialización o uso sobre run activa. Transacción candidata conserva estado ante fallo.
3. Regresiones 613–615 escritas para catálogo dinámico, conteo y rechazo de reinicialización, sin ejecutar. Flags completos de formas/género/nature/ability/IVs iniciales permanecen pendientes.

## Caramelos tras captura Classic — pendiente de ejecución

1. Captura real con perfil adjunto concede al starter raíz 1 candy ordinario o 2 si el enemigo tiene boss segments, como setPokemonSpeciesCaught/addStarterCandy pinned. Se aplica antes de la decisión de incorporación, junto al registro caught.
2. Helper valida root e inventario y conserva requested/applied; llegar al cap aplica cero sin convertir captura en error. Publicación conserva la transacción candidata existente.
3. Regresiones 616–617 escritas para captura real y cap, sin ejecutar. El actor aún no modela shiny/variant; su bonus, huevos/Daily, log/candyBar y callbacks completos siguen pendientes.

## Starters desbloqueados por captura — pendiente de ejecución

1. Selección y reinicio nativo/QuickJS aceptan starterEligible canónico con caught en perfil, además de starters iniciales. No crean listas paralelas ni permiten especies no elegibles.
2. Codec valida elegibilidad estructural; restore runtime exige perfil adjunto que acredite caught para starters no iniciales. Reconstrucción interna evita aplicar ganancias de perfil durante replay.
3. Regresiones 618–620 escritas para bloqueo sin perfil, desbloqueo, inicio y restore autorizado, sin ejecutar. Selección de equipo múltiple, costes totales, abilities/egg moves/IVs/natures desbloqueados aún pendientes.

## Validación del coste inicial Classic — pendiente de ejecución

1. Extiende PokemonStarterMoveset con validación canónica de selección: 1–6 especies elegibles, sin duplicados y coste total <=10, según getRunValueLimit/selección pinned. Runtime exige además desbloqueo por perfil.
2. Inicio actual de un starter consume este validador; codec rechaza starters cuyo coste base excede el límite. Validación no modifica output al fallar.
3. Regresiones 621–622 escritas con especies del catálogo para duplicados y exceso de presupuesto, sin ejecutar. Equipo inicial múltiple y reducciones de coste por caramelos siguen pendientes; no se declara pantalla de selección completa.

## Cobertura de codec vs permiso del perfil — pendiente de ejecución

1. Regresión codec 12 actualizada por cambio de contrato: especie starterEligible no inicial es estructuralmente válida; permiso caught se prueba en runtime 618–620. Añadida 115 para mantener rechazo explícito de especie no elegible.
2. Cambio basado en integración de desbloqueos, no en resultados de tests: suite no ejecutada y ninguna aserción desactivada. Buffers de journal/bundle siguen derivados de kStarterCandyProfileMaxBytes; perfiles evolucionados conservan candy/friendship cero.

## Reconstrucción transaccional del inicio/reinicio — pendiente de ejecución

1. restoreSetup publica una run nueva solo cuando encuentro, identidad y moveset se resuelven. Selección de otro starter también trabaja en candidato; fallo mantiene estado anterior y comunica error.
2. Conserva el perfil capturas/candy/friendship al reiniciar; reconstrucción interna de checkpoints sigue separada del permiso del perfil. No afirma battleInputSupported para reglas aún no portadas.
3. Regresiones 623–625 escritas para seed inválido sin mutación, reinicio y conservación exacta del perfil; sin ejecutar. Tests y compilación aplazados.

## Memoria de candidatos transaccionales Old 3DS — pendiente de medición

1. Candidatos completos de FirstRunRuntime pasan de objetos automáticos a unique_ptr/new(nothrow). Ledger candidato de EXP también sale de la pila. Propiedad RAII libera la memoria al publicar o fallar.
2. Fallo de asignación devuelve false con motivo y no publica estado parcial. No cambia orden RNG, contenido ni permisos; regresiones existentes de rollback/inicio/captura/EXP siguen pendientes de ejecución.
3. Es corrección por inspección de almacenamiento, no medición de pico real. Heap, frames restantes, fragmentación y rendimiento requieren compilación y Old 3DS en la etapa final.

## Memoria temporal de NativeProgressStore — pendiente de medición

1. Load/commit/export/import pasan sus envelopes NativeRunSave automáticos a propiedad RAII con new(nothrow). Fallo antes de asignación no escribe journals ni publica candidatos.
2. NativeSaveResult.MemoryUnavailable distingue fallo de memoria de contenido inválido y SD I/O. Regresión 116 escrita para nombre distinto; cobertura de rollback existente sin ejecutar. Inyección de fallo de allocator y pico/fragmentación aún pendientes.
3. No se ha compilado ni medido hardware. Otras capas del codec/host conservan envelopes automáticos que deben revisarse; no se declara eliminado todo uso grande de pila.

## Memoria temporal del codec/journal de runs — pendiente de medición

1. NativeRunSaveStore mueve slots, envelopes candidatos y buffers de escritura/exportación a asignaciones comprobadas RAII. Decode y creación de setup también dejan sus envelopes grandes fuera de la pila.
2. Conserva validación de checksum, generación, readback y referencia exacta de perfil. Fallo de asignación devuelve MemoryUnavailable; no se publica output parcial.
3. Regresiones existentes de corrupción, interrupción, ambigüedad y rollback permanecen sin ejecutar. Medición de memoria/fragmentación y fallos de allocator requieren etapa final; no se afirma funcionamiento en consola.

## Checkpoint con resultado explícito y memoria temporal — pendiente de ejecución

1. captureNativeRunSave devuelve NativeSaveResult, distinguiendo fase no soportada, referencias inválidas y memoria insuficiente. SaveNativeProgress consume el resultado antes de tocar journals; llamadas antiguas como statement siguen válidas.
2. Envelopes de captura/load/save y candidato del inventario held pasan a heap comprobado RAII. Fallos mantienen output inválido/estado previo y no publican candidato parcial.
3. Regresión 626 escrita para estado intermedio de captura no guardable, sin ejecutar. Host/bridge que aún ignoran resultado pueden mostrar error genérico y requieren conexión posterior; mediciones ARM11 pendientes.

## Snapshot de presentación sin checkpoint por frame — pendiente de ejecución

1. Host usa presentationStage del estado vivo en lugar de construir NativeRunSave cada frame. Evita copias/asignaciones de almacenamiento en el loop visual; no afirma que la fase mostrada sea serializable.
2. Save QuickJS con progress delega directamente en saveNativeProgress; fallback propaga resultado captureNativeRunSave antes de validar/escribir. Ya no duplica captura de checkpoint ni transforma falta de memoria/fase no soportada en referencia inválida.
3. Regresión nativa 627 y comprobación estática JS del bloque de proyección escritas, sin ejecutar. Rendimiento real y consumo de memoria requieren etapa final en Old 3DS.

## Memoria de comandos storage QuickJS — pendiente de ejecución

1. Save/load/export/import adquieren envelope en heap con nothrow antes de procesar comandos. Fallo retorna MemoryUnavailable sin escribir ni sustituir run viva.
2. Lectura de generación después de save reutiliza el workspace del comando en lugar de un segundo envelope. Preflight del runtime importado y referencias de perfil permanecen intactos.
3. Comprobación estática JS escrita para propiedad y orden del guard, sin ejecutar. No sustituye inyección de OOM ni pruebas del binario/hardware pendientes.

## Cálculo de reducción del coste de starter — conexión al perfil pendiente

1. PokemonStarterMoveset porta GameData.getSpeciesStarterValue: resta un punto por reducción mientras coste>1, después divide por dos. VALUE_REDUCTION_MAX pinned es 2; representación exacta en cuartos evita truncar 0.5/0.25.
2. Validador actual de selección reutiliza el cálculo con reducción cero. Regresiones 628–631 escritas para costes canónicos 1–3 y rechazo de reducción inválida, sin ejecutar.
3. Compra mediante candy prices importados, valueReduction persistente, presupuesto fraccionario de equipo y comandos de selección siguen pendientes. No se declara reducción comprable en partidas.

## Precios canónicos de caramelos de iniciales

1. Importado `allStarterCandyCosts` desde `src/data/balance/starters.ts` en la revisión pinned. Conserva precios de pasiva, dos reducciones de coste, precios de huevos, umbrales, raw y provenance. No ejecuta expresiones TypeScript ni usa precios de fixtures en producción.
2. Generador emite `StarterCandyPrice`/`kStarterCandyPrices` para pasiva y reducción; huevos y umbrales se conservan en el catálogo canónico hasta conectar su sistema. Rechaza tablas ausentes, filas inválidas y costes duplicados.
3. Dos importaciones completas dieron el mismo hash `d2e27cf94a29826157bed69b788e807a9bb30ee05a44de3f4369a51146a9b4ff`. Los pins y conteos de entidades permanecen iguales. Esto es evidencia del pipeline de datos, no de ejecución del juego.
4. El hash anterior `90f3866eb2663f7c79773f01288b2aa9cf56d4fa14bbf611c881464ed4ac2863` ya no coincide: los guardados/perfiles vinculados a él seguirán rechazados hasta implementar una migración explícita. No se reinterpretan silenciosamente.
5. Añadidas regresiones del parser, metadata desconocida, expresiones no soportadas y precios reales/provenance; sin ejecutar por instrucción del usuario. Pendiente conectar compras, reducción persistente y presupuesto con esas reducciones, pasivas y huevos. Tests, compilación, Azahar y consola aplazados.

## Reducciones persistentes de coste de iniciales

1. Perfil `P3CANDY3`: guarda `costReduction` 0–2 en bits 1–2 del noveno byte, junto a caught en bit 0. Mantiene nueve bytes por registro. Lee v1/v2 con reducción cero; valida versión, bits reservados, especies elegibles y máximo dos antes de publicar el decode. La compatibilidad de versión exige también hash de contenido idéntico; no resuelve guardados de hashes anteriores.
2. Compra C++ basada en `kStarterCandyPrices`, precios originales por coste de especie y nivel de reducción. Resultado de compra distingue precio ausente, datos inválidos, máximo y caramelos insuficientes. Conserva amistad/caught y no modifica el registro en un rechazo.
3. `FirstRunRuntime.purchaseStarterCostReduction` solo opera en setup con perfil e inicial desbloqueado; prepara copia en heap y confirma run+perfil mediante `NativeProgressStore` antes de publicar. Error de almacenamiento no publica reducción ni descuento. El resultado de compra describe la preparación; `NativeSaveResult::Ok` es imprescindible para afirmar que se confirmó.
4. `starterSelectionAllowed` suma unidades de cuarto de punto con reducciones del perfil, conserva límite Classic diez puntos y rechaza duplicados. El equipo inicial completo y la interfaz/comando de compra aún están pendientes: una API no demuestra integración visible. Pasivas, huevos y retos distintos de Classic normal permanecen pendientes.
5. Regresiones escritas para dos compras, rechazo por fondos/máximo, v3 roundtrip, lectura v2, presupuesto reducido, persistencia y fallo de escritura de ambos journals. Sin ejecutar; tests, compilación, Azahar y Old 3DS siguen aplazados.

## Compra de reducción conectada al selector

1. Bridge registra `_3ds_purchaseStarterCost`, con un comando 208 exclusivo por tick, solo en setup y con store persistente. Ejecución fuera del tick llama la compra transaccional C++; feedback de éxito exige commit confirmado.
2. Snapshot de presentación expone coste fraccionario, caramelos, reducción, precio y disponibilidad desde catálogo/perfil reales. HUD setup: B abre confirmación, A compra, B cancela; sin comenzar combate durante confirmación. Operaciones de almacenamiento conservan prioridad.
3. Regresión VM del template exacto escrita para confirmación única, coste 0.5, fondos/disponibilidad y prioridad de guardado. Sin ejecutar. Bundle diagnóstico regenerado, sin compilar el programa. Equipo inicial múltiple, presentación final equivalente web y validación de hardware siguen pendientes.

## Equipo inicial: creación C++ de varios actores

1. `restoreStarterTeamSetup` valida 1–6 especies desbloqueadas, duplicados y presupuesto reducido; prepara setup y actores en una copia del runtime. Cada actor llega al array real de party con moveset, estado de batalla, identidad, HP/PP, forma, assets y experiencia propios. Un error no publica un equipo parcial.
2. `resolveFreshStarter` reutiliza la creación anterior. Sigue el perfil inicial con IV 15, naturaleza default, habilidad inicial y sin egg moves desbloqueados. Un único RNG seeded se consume en orden para PID/Tera de cada actor; no reinicia la semilla por miembro. Fuente de orden: `src/phases/select-starter-phase.ts`, `SelectStarterPhase.initBattle`, pinned. No equivale todavía al perfil Pokédex completo.
3. Guardar setup con varios miembros devuelve `UnsupportedStage`: formato actual no representa esa selección. No se degrada a un solo inicial. Checkpoints de combate ya tienen snapshot de party; falta guardar/restaurar selección inicial, interfaz múltiple y verificar el flujo completo.
4. Regresiones escritas con dos especies reales, identidad distinta, reproducibilidad, datos por miembro, rollback y rechazo de guardado incompleto. Sin ejecutar; compilación y validación final aplazadas.

## Guardado de selección inicial múltiple v14

1. Run/save runtime v14 añade `setupStarterCount` y `setupStarterDexes` ordenados al final del payload, con hasta seis especies. Cero mantiene setup legacy de un inicial. Selecciones solo se permiten en RunSetup; validator rechaza duplicados, especies inválidas, primer miembro discordante y campos fuera del conteo.
2. Captura setup de equipo conserva sus especies. Restauración carga el perfil referenciado y reconstruye el equipo mediante `restoreStarterTeamSetup`, que valida desbloqueos y presupuesto efectivo de ese perfil, mantiene semilla/PIDs y no publica un equipo parcial. No representa todavía preferencias avanzadas de formas, naturalezas, IVs, egg moves ni pasivas desbloqueadas.
3. Decoder acepta v13 junto a versiones anteriores y lo normaliza a v14, manteniendo su semántica de un inicial cuando faltan campos de selección. El hash del contenido todavía debe coincidir; no hay migración entre snapshots distintos.
4. Regresiones de setup con dos especies, encode/decode/restore, duplicados y payload legacy v13 escritas; sin ejecutar. Sustituye el rechazo temporal de setup múltiple documentado anteriormente. Selector visible múltiple y validación final pendientes.

## Selector múltiple conectado al runtime

1. Navegación setup usa cursor separado de la primera especie del equipo: no reemplaza ni pierde reservas. `toggleSetupStarter` añade/retira la especie del cursor y reconstruye el equipo real mediante la transacción validada. Conserva orden, presupuesto reducido y máximo seis; no permite retirar el último miembro. Esta restricción de edición difiere del borrador vacío de upstream y queda pendiente de resolver con un modelo de selección vacío.
2. Bridge comando 209 confirma add/remove fuera del tick. Snapshot muestra nombres de equipo, cantidad y coste efectivo; compra se aplica a la especie del cursor. HUD: izquierda/derecha navega, A añade/retira, Start inicia, B confirma reducción mediante su menú.
3. Regresiones nativas escritas de navegación sin reemplazo, incorporación/retiro y protección del último miembro; VM actualizada para A=selección y Start=inicio. Sin ejecutar. Bundle regenerado, sin compilar. Preview gráfico del cursor, selección vacía y presentación final equivalente web pendientes.

## Preview del cursor de iniciales

1. `_3ds_drawStarter` resuelve especie/forma del cursor desde catálogo, verifica desbloqueo y dibuja su atlas frontal existente en la pantalla superior. No construye un actor ni modifica party, estado de combate, PID o RNG.
2. Reutiliza el cache frontal del enemigo mientras setup oculta el combate; no añade otra página de textura retenida. Al entrar en batalla, el presenter resuelve la clave del enemigo como antes. Asset ausente sigue el tratamiento explícito existente del presenter, sin generar sprites falsos.
3. HUD setup superior muestra nombre y pertenencia al equipo; inferior conserva selección y presupuesto. Regresión VM escrita del preview sin comando de gameplay. Bundle regenerado. Tests/compilación/Azahar/hardware siguen aplazados; equivalencia visual final y selección vacía pendientes.

## Ownership de bindings tras transacciones

1. Revisión estática detectó cuatro publicaciones de candidatos (`purchaseStarterCostReduction`, `restoreStarterTeamSetup`, `restoreSetup`, `initializeFreshStarterProfile`) sin reconstruir escena. La copia conserva punteros a nodos/textos del candidato temporal, que se libera al salir del método.
2. Añadido `buildScene` después de publicar cada copia, como ya hacen las restantes transacciones. No cambia datos, reglas ni RNG; vuelve a enlazar presentación con almacenamiento de la instancia definitiva.
3. Regresiones nativas escritas comprueban que nodos de escena pertenecen al runtime tras setup/equipo/compra. Sin ejecutar por instrucción del usuario; sanitizers y validación final pendientes.

## Regresión de equipo inicial en primer turno real

1. Añadida cadena setup de dos especies fresh canónicas → encuentro/movimiento declarado soportado → turno real → checkpoint de party → encode/decode → restauración. Comprueba identidad por miembro, reserva sin gasto de HP/PP, participación exclusiva del activo y ownership de escena restaurada.
2. Busca únicamente un candidato declarado soportado entre semillas acotadas; después de seleccionar ese candidato cualquier error de turno/guardado/restauración falla, sin continuar buscando otro que pase. Si no existe candidato, falla explícitamente.
3. Prueba escrita y pendiente de ejecución; no constituye evidencia de combate exitoso todavía. Revisión detectó también que iniciales capturados no default requieren metadata de Pokédex (naturaleza/IVs/atributos) que el perfil actual no almacena: su resolver fresh los rechaza. Desbloqueo visible no demuestra que esos iniciales sean jugables; esta integración permanece pendiente.

## Metadata de captura: naturalezas e IVs en perfil v4

1. `P3CANDY4` amplía registro de nueve a diecinueve bytes: conserva `natureAttr` con bits upstream n+1 y seis máximos de IV. Captura actualiza especie y preevoluciones a partir de naturaleza/IV reales del actor, mediante OR y máximo por estadística. Fuentes pinned: `src/system/game-data.ts`, `setPokemonCaught`/`updateSpeciesDexIvs`/`getNaturesForAttr`. No cambia IVs del actor ni consume RNG.
2. Lectura v1/v2/v3 conserva sus campos y deja naturaleza/IV desconocidos en cero; no fabrica atributos. Validator rechaza bits de naturaleza fuera de 1–25 e IV fuera de 0–31. Journal acepta v4; capacidades de scratch/bundle derivan del nuevo máximo. El tamaño del ledger C++ y memoria retenida crecen: rendimiento/memoria Old 3DS aún requieren medición final.
3. Regresiones escritas de roundtrip v4, límites, lectura legacy sin atributos ficticios y conservación de reducción. Sin ejecutar.
4. Esto conserva datos reales pero no completa el Pokédex ni habilita por sí solo iniciales capturados no default: habilidades, género, formas/shiny y conexión al resolver siguen pendientes. El perfil inicial todavía usa sus atributos fresh canónicos existentes.

## Metadata de captura: habilidades y género en perfil v5

1. `P3CANDY5` añade `abilityAttr` (bits upstream 1/2/4) y `genderAttr` (MALE/FEMALE 4/8), total veintiún bytes por registro. Captura usa identidad real del actor: habilidad en especies elegibles y regla upstream de hidden cuando índice uno sin segunda habilidad; género masculino/femenino OR, genderless sin esos bits. No inventa shiny, variante o formas.
2. Fuente pinned inspeccionada: `src/system/game-data.ts`, `setPokemonSpeciesCaught`; `src/field/pokemon.ts`, `getDexAttr`; `src/enums/ability-attr.ts`/`dex-attr.ts`. El perfil mantiene ambos grupos por separado, no afirma implementar todo `caughtAttr` ni `getFullUnlocksData`. Propagación completa de formas/battle forms y restricciones de especies requieren su resolver.
3. Lectura v1–v4 conserva campos presentes y deja atributos no existentes en cero; v4 preserva naturaleza/IV. Validator rechaza bits ajenos. Pruebas v5 roundtrip, legacy v4 y bits inválidos escritas, sin ejecutar. Capacidades derivadas aumentan; medición de memoria Old 3DS pendiente.
4. Falta conectar atributos al resolver de iniciales y persistir formas/shiny/variantes. Iniciales capturados no default siguen sin integración jugable completa. Tests/compilación aplazados.

## Perfil nuevo conserva metadata de iniciales default

1. `seedNativeFreshStarterDexMetadata` usa naturaleza canónica reproducible de `PokemonFreshProfile`, IV mínimos 15, ABILITY_1 y ambos bits MALE/FEMALE, tal como `GameData.initDexData`/`initStarterData` pinned. Solo acepta especies default; no infiere atributos de otros iniciales.
2. `initializeFreshStarterProfile` guarda esa metadata en el ledger real. Capturar una especie default combina ese baseline conocido antes de agregar atributos capturados: OR de unlocks y máximos IV, sin perder progreso previo ni bajar IV superiores. No modifica un perfil legacy por el mero hecho de cargarlo.
3. Regresiones de todo el catálogo default, naturaleza exacta, IV/flags y combinación idempotente escritas; sin ejecutar. Formas/shiny/variantes y consumo del perfil por el resolver todavía pendientes; tests y compilación aplazados.

## IV de Pokédex conectados al inicial default

1. `resolveFreshStarter` conserva mejoras del ledger para las especies default: parte del baseline canónico IV 15 y aplica los máximos persistidos por estadística antes de `initializePokemonBattleState`. Stats del actor se calculan con esos IV; no consume draws adicionales ni modifica el perfil. No habilita todavía especies no default ni preferencias de naturaleza/habilidad.
2. Restauración de setup reconstruye también el caso de un solo inicial después de cargar su perfil referenciado; antes se reconstruía solo la selección múltiple en esa etapa. Así conserva los IV mejorados tanto en setup simple como múltiple. Los checkpoints de combate mantienen sus propios snapshots de actor.
3. Regresiones escritas de IV 31/27, baseline de estadísticas restantes y restauración de stats/identidad; pendientes de ejecución. Tests y compilación siguen aplazados.

## Naturaleza default resuelta desde desbloqueos

1. `nativeStarterDefaultNature` sigue `getStarterDefaultNature` de `src/ui/utils/starter-select-ui-utils.ts` pinned: primera naturaleza desbloqueada en orden enum. No consume RNG. Metadata ausente/inválida falla sin modificar output; el caller mantiene el baseline canónico conocido de especies default cuando falta metadata legacy.
2. `resolveFreshStarter` aplica naturaleza del perfil antes de construir identidad/stats. Esto conecta nuevos desbloqueos de naturaleza para iniciales default, además de IV. Preferencias/ciclo de naturaleza y especies no default todavía pendientes.
3. Regresiones de máscara Hardy+Quirky, metadata ausente y naturaleza efectiva del actor escritas; sin ejecutar. Tests/compilación siguen aplazados.

## Habilidad default resuelta desde desbloqueos

1. `nativeStarterDefaultAbility` sigue `getStarterDefaultAbilityIndex` de `src/ui/utils/starter-select-ui-utils.ts` pinned: primera habilidad si está desbloqueada, segunda cuando corresponde, oculta en slot uno sin segunda regular o slot dos con ella. Resuelve IDs canónicos reales y falla sin modificar output cuando falta metadata/definición.
2. Resolver fresh usa habilidad/slot del perfil y atributos de la forma canónica cuando existen; fallback legacy conserva la primera habilidad conocida del baseline default. Sin draws RNG nuevos. No habilita todavía especies no default ni triggers de habilidad pendientes.
3. Regresiones escritas de prioridad de primera habilidad, hidden-only, índice upstream y metadata ausente; sin ejecutar. Tests/compilación siguen aplazados.

## Género default resuelto desde desbloqueos

1. `nativeStarterDefaultGender` sigue selección default de `src/ui/utils/starter-select-ui-utils.ts` pinned: femenino si solo FEMALE está desbloqueado, masculino con MALE; especies canónicas genderless usan Genderless. Rechaza metadata ausente para especies con género, bits ajenos y selección imposible por ratio. Fallo no modifica output.
2. Resolver fresh aplica género del ledger antes de inicializar actor/stats; legacy sin datos mantiene baseline default conocido. No consume draws RNG ni habilita todavía especies no default. Preferencias explícitas siguen pendientes.
3. Regresiones de ambos géneros, female-only, metadata ausente, genderless real y género efectivo del inicial escritas; sin ejecutar. Tests/compilación aplazados.

## Iniciales capturados sin formas alternativas

1. `resolveStarterFromDex` amplía el resolver existente a especies no default cuando tienen caught, naturaleza, habilidad y género/IV resolubles desde perfil. Usa IV reales sin baseline artificial 15, naturaleza/slot/género desbloqueados e IDs canónicos. Mantiene draws de constructor y selección del equipo real.
2. Especies con referencias de formas no default siguen rechazadas hasta persistir/resolver unlocks de forma; no se supone una forma capturada. Metadata legacy incompleta no se sustituye por atributos ficticios. Esto amplía capacidad real, no completa todo el catálogo.
3. Restauración carga perfil antes de reconstruir actores y vuelve a cargarlo después de replay para eliminar ganancias temporales. Setup se reconstruye con ese mismo perfil. No hay escritura durante replay.
4. Regresión de desbloqueo anterior extendida con metadata de Pokédex explícita, naturaleza/IV/habilidad efectivos y roundtrip setup. Sin ejecutar; tests/compilación aplazados. Formas/shiny/variantes, preferencias y todas las reglas de combate pendientes.

## Género sin bits en desbloqueo de preevolución

1. Upstream elige masculino cuando caughtAttr no contiene FEMALE-only, incluso si una captura genderless no aporta bits de género a una preevolución gendered. El resolver ahora admite ese default cuando existen metadata de naturaleza y habilidad de captura; no confunde el cero válido con perfiles legacy incompletos.
2. Mantiene rechazo por ratios imposibles y por metadata ausente. Regresiones distinguen cero de bits con metadata completa frente a legacy sin habilidad; sin ejecutar. Form unlock propagation completa sigue pendiente.

## Índices de forma upstream en catálogo y C++

1. Importer conserva `extensions.upstreamFormIndex` de cada constructor `PokemonForm` en orden fuente, sin asignar IDs paralelos. Generador emite `Form.upstreamFormIndex` y `findFormByUpstreamIndex(speciesDex,index)`; rechaza índices ausentes/invalidos/duplicados por especie. ID/clave de forma, raw, provenance y referencias de assets permanecen.
2. Dos importaciones completas coincidieron en hash `bcc2ae431359522ca9c6c0441bd06e257ce28ccb940e7dbb31d31099cfd38bbe`; pins y conteos siguen iguales: 1084 especies/609 formas. Esta evidencia demuestra generación determinista, no ejecución del juego.
3. El snapshot previo `d2e27cf94a29826157bed69b788e807a9bb30ee05a44de3f4369a51146a9b4ff` deja de coincidir; guardados/perfiles vinculados a ese hash requieren migración explícita todavía pendiente. No se neutraliza la protección de hash.
4. Regresiones de orden de constructor y lookup por especie/índice para todo el catálogo de formas escritas; sin ejecutar. Persistir/desbloquear formas y reglas de battle forms siguen pendientes; tests/compilación aplazados.

## Metadata observada de forma: resolver previo a persistencia

1. Verificada construcción de `PokemonSpecies.forms`: conserva directamente el array upstream; índices importados corresponden a los usados por `Pokemon.getDexAttr`. `pokemonObservedDexFormAttr` resuelve identidad canónica a `DEFAULT_FORM (128) << upstreamFormIndex`, sin RNG y sin modificar output si falla.
2. Distingue especie/forma ausente, especie discordante, forma sin resolver y capacidad de atributo de 64 bits excedida (índice >56). Esa capacidad es del adaptador de metadata, no un límite del catálogo canónico. No trunca bits ni inventa una forma base cuando hay forms.
3. Resolver todavía no conectado al perfil de captura; observa forma, no sustituye `getFullUnlocksData` ni reglas de battle forms. Persistencia y desbloqueos completos pendientes. Regresión del catálogo real escrita, sin ejecutar; tests/compilación aplazados.

## Formas observadas de captura persistidas en v6

1. `P3CANDY6` conserva `observedFormAttr` de 64 bits, total veintinueve bytes por registro. Captura resuelve atributo desde identidad/índice canónicos antes de modificar el ledger; combina por OR solamente en la especie observada. No propaga una forma inventada a preevoluciones.
2. Codec little endian dedicado de 64 bits evita truncar bits superiores a 31. Validator rechaza bits reservados y formas inexistentes para esa especie; admite base implícita únicamente cuando el catálogo no declara formas. Version/record width centralizados; lectura v1–v5 conserva datos presentes y deja observaciones ausentes en cero.
3. Observación se mantiene separada de unlocks: no implementa todavía `getFullUnlocksData`, desbloqueos base de battle forms ni reglas especiales de preevoluciones. No se concede una forma de inicial por observarla. Capacidad 64 bits del adaptador rechaza índices >56 explícitamente; catálogo sigue sin ese límite.
4. Regresiones escritas de todos los forms reales → atributo → encode/decode, legacy v5, referencias inválidas y bits reservados. Sin ejecutar. Capacidades derivadas y ledger crecen; memoria/rendimiento Old 3DS pendientes de medir. Tests/compilación aplazados.

## Permisos de constructor de formas upstream

1. Importer conserva isUnobtainable (default false) e isStarterSelectable (default !formKey), según PokemonForm.constructor pinned. Expresiones desconocidas quedan null y su código fuente completo permanece en raw; no se convierten en permisos.
2. Generador C++ emite kFormPermissions con flags triestado: cero/uno conocidos y menos uno no resuelto. Es metadata de constructor, no la decisión completa del selector: starterSelectableKeys, getFullUnlocksData y reglas especiales de captura todavía deben conectarse.
3. ObservedFormAttr sigue siendo observación, nunca autorización para seleccionar formas de combate. Regresiones de catálogo y Gigantamax añadidas, sin ejecutar. Tests y compilación aplazados.
4. La nueva metadata cambia el hash del contenido. Perfiles y saves anteriores requieren migración explícita todavía pendiente; no se elimina su comprobación de identidad.

5. Dos importaciones pinned coinciden en hash a1d122dbee31f5e6d77fcb847daff51e1879514a333c1f471a82d31c1d0675d3; conteos permanecen 1084 especies y 609 formas. Evidencia de generación determinista, no ejecución en consola.

## Validación C++ de preferencias de forma

1. pokemonValidateStarterForm reproduce el guard de StarterSelectUiHandler: forma existente de la especie, isStarterSelectable verdadero y bit de forma presente en caughtAttr desbloqueado. Distingue metadata desconocida, forma no seleccionable, bloqueada y capacidad de bits excedida. Sin RNG ni mutación.
2. El argumento es unlockedCaughtAttr explícito. El perfil actual contiene observedFormAttr; no se conecta como si fuera el registro upstream de desbloqueos. Persistir y derivar esos desbloqueos sigue pendiente antes de conectar preferencias al selector real.
3. starterSelectableKeys aparece solamente declarado en pokemon-species.ts pinned; no participa en el guard ni se añade una excepción local. isUnobtainable corresponde a getFullUnlocksData, no a este guard de preferencias.
4. Regresiones para todo el catálogo y errores de lookup añadidas al harness nativo, sin ejecutar. git diff --check revisado; tests y compilación siguen aplazados.

## Filtro upstream de formas obtenibles

1. pokemonObtainableFormMask implementa únicamente la componente de formas de PokemonSpecies.getFullUnlocksData: catálogo con cero/una forma usa DEFAULT_FORM; múltiples formas excluyen isUnobtainable. Metadata desconocida y capacidad excedida fallan explícitamente sin modificar output.
2. Máscara de permisos no equivale a desbloqueos ganados. Aún falta combinar capturas, reglas especiales de battle forms/preevoluciones y perfil durable antes de conectar selección de formas. Observaciones no conceden todas las formas obtenibles.
3. Regresión de máscara para las 1084 especies y error de especie ausente añadida al harness; pendiente de ejecución. Tests/compilación siguen aplazados.

## Corrección del lector legacy de perfiles

1. Revisión estática detectó código duplicado dentro del bloque v1 de inspectNativeStarterCandyProfile: referencia a species sin declaración y retornos bool incompatibles con NativeSaveResult. Retirado únicamente ese duplicado.
2. Se conserva la restricción v1 a especies raíz y la validación común StarterCandyProfileCodec::valid para IDs, atributos y formas. No se modifica el formato ni se relajan checksums/hash. Regresiones legacy existentes permanecen; ejecución y compilación aplazadas.

## Perfil v7: formas desbloqueadas separadas

1. P3CANDY7 añade unlockedFormAttr de 64 bits, separado de observedFormAttr. Registro de 37 bytes, capacidades derivadas; SHA-256 y content hash mantienen su protección. Validator revisa bits reservados y existencia de formas para ambos atributos.
2. Lectura v1–v6 conserva campos conocidos y deja unlockedFormAttr en cero; no transforma observaciones legacy en desbloqueos. Nuevos perfiles default reciben DEFAULT_FORM (128) siguiendo GameData.initDexData.
3. Capturas todavía no rellenan este campo: pendientes reglas especiales de battle forms, preevoluciones y registro de form changes. Preferencias/selector aún no conectados. El formato permite persistir la próxima integración sin confundir permiso con observación.
4. Regresiones v7 roundtrip, v6 conserva observación pero no inventa unlock y bits reservados escritas. Tests/compilación aplazados; memoria/rendimiento Old 3DS siguen sin medir.

## Capturas de forma cero y selección base conectadas

1. recordCaughtSpecies combina el bit de forma cero capturado con pokemonObtainableFormMask de cada especie de la cadena de preevoluciones, y lo persiste en unlockedFormAttr. Prevalida máscaras/recorrido antes de modificar el ledger. Sigue GameData.setPokemonSpeciesCaught para formIndex cero, donde no se ejecutan ramas especiales de battle forms.
2. resolveStarterFromDex admite inicial no-default con forma base declarada cuando existe metadata completa y pokemonValidateStarterForm confirma permiso y unlock. Perfiles legacy no reciben ese permiso por inferencia.
3. Capturas de índices no cero mantienen observación; sus unlocks especiales siguen pendientes del registro de form changes y ramas pinned. No se conceden permisos basados solamente en la observación. Selector de otras formas aún pendiente. Tests y compilación aplazados.

## Registro real de cambios de forma y captura

1. SpeciesFormChange.evoFormKey (constructor asigna formKey) importado desde generation-01..09 en extensions.upstreamFormChanges; se conserva constructor completo con triggers/conditions raw y provenance de la especie. C++ kFormChangeReferences registra destinos por especie con sourcePath/symbol/SHA-256.
2. pokemonCaptureFormUnlocks aplica máscara obtenible y ramas de GameData.setPokemonSpeciesCaught: Pikachu/Pichu, Urshifu índices 2/3, Zygarde 4/5 y registro de cambios cuyo destino coincide con la forma capturada. recordCaughtSpecies prevalida toda la cadena y combina unlocks durables; observación sigue separada.
3. Excepciones recursivas que producen bits sin forma concreta para una preevolución quedan UnsupportedReference; captura se rechaza antes de modificar el ledger. Ampliar representación de esos atributos upstream sigue pendiente. Triggers de transformación en combate y selector de formas aún no portados.
4. Importación doble coincide en 400fb84aa16a460c6d3eb6260240e8eae948acb9fab0f5467fd81521b6630e49. Cambia identidad del contenido; migración de perfiles/saves anteriores pendiente. Tests de import y casos reales especiales escritos, sin ejecutar; compilación aplazada.

## Corrección de género por defecto del selector

1. Inspección de getStarterDexAttrPropsFromPreferences confirmó que usa GameData.getSpeciesDefaultDexAttrProps: female es malePercent === 0. No usa getDexAttrProps(caughtAttr) para elegir el género por defecto. La implementación anterior confundía esas dos rutas y daba preferencia a FEMALE-only capturado.
2. nativeStarterDefaultGender y fallback del actor ahora usan la proporción canónica de la especie; genderless se conserva. Bits capturados permanecen intactos para futuras preferencias explícitas. Metadata incompleta legacy y ratios no resueltos continúan rechazados.
3. Regresiones corregidas según la fuente inspeccionada y ampliadas a todo el catálogo; ninguna fue ejecutada. Tests/compilación aplazados. FormIndex upstream por defecto es cero; selección alternativa requiere preferencia explícita todavía pendiente.

## Metadata recursiva Urshifu → Kubfu preservada

1. SetPokemonSpeciesCaught upstream aplica la rama Urshifu formIndex 3 → getFormAttr(1) también durante la recursión de Kubfu. Ahora se conserva ese bit en unlockedFormAttr aunque Kubfu no tenga una forma física en índice uno.
2. Codec permite exclusivamente esta excepción de metadata desbloqueada; observedFormAttr todavía exige forma real. PokemonValidateStarterForm sigue rechazando índice uno de Kubfu, sin inventar sprites, stats ni formas seleccionables. Otras referencias desconocidas permanecen explícitas.
3. Regresión Urshifu → Kubfu, roundtrip durable y rechazo de observación/selección ficticia añadidas; sin ejecución. Formato v7 y hash actual sin cambios. Tests/compilación aplazados.

## Preferencia durable de forma del inicial

1. P3CANDY8 añade preferredFormIndex (uint16, 65535 significa sin preferencia), 39 bytes por registro. Lectura v1–v7 mantiene metadata previa y no inventa preferencia; validator exige forma seleccionable/desbloqueada de una especie inicial para preferencias explícitas.
2. resolveStarterFromDex consume forma preferida: ID, learnset, stats, tipos, habilidad de forma y Tera inicial se resuelven por catálogo. Sin preferencia conserva índice cero upstream. Forma FEMALE explícita exige unlock de género femenino y actualiza identidad antes de inicializar stats.
3. selectSetupStarterForm prepara perfil y reconstrucción del equipo con la misma seed; persiste run+profile antes de publicar y vuelve a enlazar escena. Fallo de validación/almacenamiento conserva runtime original. Conexión del botón/cycling en QuickJS y preview aún pendientes; preferencias de género/naturaleza/habilidad aún no implementadas.
4. Regresiones v8 roundtrip de todas las formas seleccionables de iniciales, rechazo sin unlock y lectura v7 sin preferencia escritas; no ejecutadas. Tests/compilación aplazados; impacto de memoria pendiente.

## Control visible y preview de formas del inicial

1. Up/Down en setup emite _3ds_cycleStarterForm y cola de comando nativa 226/227, fuera del tick. Ciclo conserva índices upstream y ofrece solamente formas seleccionables/desbloqueadas.
2. Transacción valida también el actor del cursor aunque no esté en equipo, reconstruye miembros y persiste antes de publicar. Preview front y etiqueta usan preferencia canónica sin construir actor/PID ni consumir RNG durante dibujo.
3. Prioridad de almacenamiento y modal de compra permanece. Regresión template JS escrita para Up/Down y prioridad sobre A; sin ejecutar. Bundle diagnóstico regenerado; tests/compilación aplazados. Validación nativa de interrupciones de guardado de preferencias y visual/hardware pendientes.

## Regresión nativa de selección y guardado de forma

1. Harness prepara una especie/forma alternativa seleccionable desde el catálogo real y metadata de perfil explícita de test; no busca otro caso después de una falla del runtime.
2. Cubre selección → actor/learnset de forma → commit pareado, interrupción de escritura de perfil, interrupción del run, reload de última generación válida, rollback vivo, rechazo de índice inválido y eliminación de preferencia con retorno a default. Comprueba ownership de escena y conserva PID/HP al fallar.
3. Prueba escrita y registrada, sin ejecutar. No demuestra todavía que el guardado/selección funcionen en Azahar o Old 3DS; tests/compilación siguen aplazados por el usuario.

## Parámetros de estados de movimientos en C++

1. Generador normaliza declaraciones StatusEffectAttr desde raw canónico real a kMoveStatusEffects: ID de move upstream, símbolo de StatusEffect, selfTarget (default false), resolución de parámetros y provenance. Expresiones no constantes quedan explícitamente parametersResolved=false, sin sustituir por un efecto inventado.
2. Fuente inspeccionada src/data/moves/move.ts, StatusEffectAttr.constructor/apply: chance usa MoveEffectAttr.getMoveChance y luego target.trySetStatus; turnos/inmunidades/modificadores pertenecen a otras capas. MultiStatusEffectAttr y callbacks siguen en metadata canónica, no se ejecutan como efecto constante.
3. Esta tabla prepara porting de estados; no habilita esos movimientos en combate. Faltan estado durable, inmunidades, pre-move/residuals y dispatcher antes de declararlos jugables. Regresión de Thunder Wave escrita; tests/compilación aplazados.

## Estado no volátil nativo y captura

1. PokemonStatusEffect conserva IDs 0–7 de src/enums/status-effect.ts. PokemonStatusState en PokemonBattleState separa ausencia de Status, toxicTurnCount y opcionales sleepTurnsRemaining/freezeTurnsRemaining. Validator rechaza IDs no soportados y contadores incoherentes.
2. incrementPokemonStatusTurn reproduce Status.incrementTurn: incrementa toxicTurnCount para todo objeto Status y reduce contadores opcionales positivos. Sin RNG; overflow del adaptador uint32 falla sin modificar estado. isPostTurn clasifica Poison/Toxic/Burn, sin ejecutar todavía residuals.
3. CapturePhase usa getStatusEffectCatchRateMultiplier: 1.5 poison/toxic/paralysis/burn, 2.5 sleep/freeze, uno sin estado. Valida metadata antes de draws de captura. FAINT con HP vivo es inválido. Todavía no existe dispatcher que aplique status desde movimientos.
4. Save actual rechaza status presente en actores/equipos para impedir pérdida silenciosa hasta ampliar codec/restore. Inmunidades, pre-move, residuals, curación y status save pendientes. Regresiones de contadores/overflow/clasificación escritas, sin ejecutar; tests/compilación aplazados.

## Resolver de daño residual de estados

1. applyPokemonStatusResidual porta matemática/orden de PostTurnStatusEffectPhase: activos sin switch-out, contador incrementado antes de BlockNonDirectDamage/BlockStatusDamage, poison floor(maxHP/8), toxic floor(maxHP*turn/16), burn floor(maxHP/16), mínimo uno. ReduceBurnDamageAbAttr vuelve a aplicar mínimo uno tras multiplicador racional.
2. Evento registra daño solicitado/aplicado, HP, bloqueo, contador y faint. Publicación atómica; policy no resuelta, overflow y bosses que requieren dispatcher no mutan actor/output. Residual no ofrece Endure/Sturdy. No realiza RNG ni callbacks inventados.
3. Falta conectar resolución real de habilidades/post-damage/boss y orden de fases al turno; movimientos con status permanecen bloqueados y save status pendiente. Regresiones de tóxico, bloqueo con contador, quemadura reducida y mínimo/KO escritas, sin ejecutar. Tests/compilación aplazados.

## Elegibilidad upstream de estados

1. canPokemonSetStatus reproduce Pokemon.canSetStatus: estado existente/pending, override de Rest, Misty grounded, poison/steel con bypass de fuente por tipo, Electric paralysis, Electric terrain sleep, Ice/sun freeze, Fire burn, bloqueo propio/aliado y Safeguard solo para fuente externa.
2. Política exige tipos efectivos, grounding, campo y callbacks resueltos; no presupone que falta de representación implica ausencia. Sleep conserva detalle pinned de no consultar ignoreField. Predicate no muta actor ni consume RNG.
3. Faltan el proveedor real de policy, trySetStatus/ObtainStatusEffectPhase, durations, reactions y fases del turno antes de habilitar moves con estados. Regresiones de Corrosion parcial, fuente ausente, terrenos, sol y override escritas; sin ejecutar. Tests/compilación aplazados.

## Actor de ObtainStatusEffect

1. obtainPokemonStatus porta componente de actor de Pokemon.doSetStatus: contador toxic cero, sleep sin duración explícita consume randBattleSeedInt(3) y elige 2/3, freeze recibe tres. Optional sleep conserva cero presente para otros estados como el constructor upstream.
2. Elegibilidad, faint y reactionsResolved se comprueban antes del draw; RNG y actor se publican conjuntamente. Tags/queue/callbacks requieren resolución explícita y no se simulan. Dispatcher del turno y save status siguen pendientes.
3. Regresiones de duración/RNG, reacción no resuelta y freeze escritas, sin ejecutar. Tests/compilación aplazados.

## Chequeo de estados previo al movimiento

1. checkPokemonStatusBeforeMove porta actor/RNG de MovePhase.checkSleep/checkFreeze/checkPara: sleep decrementa contador y aplica reducción resuelta, indirect despierta, bypass no cancela; freeze consume randBattleSeedInt(4) antes de comprobar expiración salvo movimiento de curación inmediata; paralysis usa randBattleSeedInt(8), no probabilidad inventada de generaciones anteriores.
2. Política requiere atributos y modo de uso resueltos. Actor, RNG y evento se publican conjuntamente; overflow/metadata inválida no mutan. Overrides de debug no se exponen como regla de producción.
3. Resolver todavía no conectado al dispatcher real; cola/cancelación/PP/mensajes y estado durable siguen pendientes. Tests/compilación aplazados.

## Regresiones de chequeo previo de estados

1. Harness comprueba sleep bloqueado/curado sin draws, bypass sleep con contador real, indirect wake, freeze expirado que aún consume randSeedInt(4), y paralysis con randSeedInt(8) sin incrementar toxicTurnCount.
2. Compara siguiente uint32 del stream esperado y real para detectar draws omitidos/adicionales; policy no resuelta conserva estado. Pruebas escritas/registradas, sin ejecutar. Conexión al turno y save continúa pendiente; tests/compilación aplazados.

## Velocidad por parálisis conectada a resolvers

1. Pokemon.getStat(SPD) pinned aplica ret >>= 1 después de stages y antes de Unburden. pokemonWeatherEffectiveSpeed y pokemonBaselineEffectiveStat(SPD) ahora respetan ese truncado/mitad y mínimo uno. Tipos de status inválidos fallan sin publicar output.
2. Turn order ya consume estos resolvers; no se habilitan todavía moves de status ni se inventan inmunidades. Speed de reservas sin getStat efectivo permanece sin penalización, siguiendo el caller upstream. Regresiones de valor impar, stage positivo y mínimo uno escritas, sin ejecutar. Tests/compilación aplazados.

## Curación nativa de metadata de estados

1. curePokemonStatusState porta clearStatus/resetStatus: revive=false conserva FAINT; curación solicita Nightmare si salía de sleep, Confused si pedido, recarga visual opcional y FPS diez. Actor se limpia solamente tras política de reacciones resuelta.
2. Evento conserva tareas pendientes para dispatcher; no elimina tags inexistentes del modelo ni inventa callbacks. Conexión a items, heal phases, pre-move cure y save aún pendiente. Regresiones de bloqueo/flags/FAINT escritas, sin ejecutar. Tests/compilación aplazados.

## Política de daño por quemadura

1. pokemonBurnDamageMultiplier reproduce condición de Pokemon.getAttackDamage: físico + BURN divide por dos salvo BypassBurnDamageReductionAttr del move o habilidad resuelta que no esté ignorada. No cambia attack stats; multiplicador pertenece a la etapa posterior a STAB/types y anterior a screens.
2. Policy desconocida falla sin publicar output cuando se necesita el callback. Moves especiales/no quemados/bypass declarado conservan factor uno. Dispatcher de daño todavía no consume esta policy; estados siguen sin habilitarse por moves.
3. Regresiones físico/bypass habilidad/ignoreSourceAbility escritas, sin ejecutar. Tests/compilación aplazados.

## Quemadura conectada al cálculo de daño estándar

1. resolveStandardPokemonMoveDamage consume PokemonBurnDamagePolicy opcional y aplica factor después de STAB/types antes de redondear. useStandardPokemonMove transmite policy al resolver conservando API existente por default. No añade draws RNG.
2. Actor quemado físico sin policy resuelta falla antes de precisión/crítico/RNG, en lugar de omitir la habilidad. Actors no quemados y moves bypass no requieren policy adicional. FirstRunRuntime todavía debe resolver callbacks de habilidad antes de habilitar status en combate.
3. Revisión estática/diff; tests/compilación aplazados. Dispatcher de estados y save permanecen pendientes.

## Datos de bypass de quemadura por habilidad

1. Generador extrae atributos constantes BypassBurnDamageReductionAbAttr desde raw real de abilities; catálogo actual declara GUTS. La clase upstream hereda CancelInteractionAbAttr sin condición propia. Otras condiciones de Guts siguen separadas.
2. resolvePokemonBurnDamagePolicy consulta IDs canónicos y tabla generada, exige callbacks/suppression resueltos y distingue habilidad activa de ignoreSourceAbility. No declara resueltas pasivas ni dispatchers desconocidos. Conexión desde FirstRunRuntime pendiente.
3. Datos regenerados sin cambiar snapshot/hash; tests/compilación aplazados.

## Política de quemadura conectada al turno estándar

1. FirstRunRuntime resuelve policy de quemadura para físico BURN antes de damage/recoil/drain y la transmite al comando común. Callbacks constantes se identifican por tabla de todas las habilidades; referencias condicionales/no interpretadas quedan no resueltas y bloquean claramente.
2. Camino soportado usa habilidad primaria activa sin supresión/passive/ignore flags adicionales. No habilita moves con status todavía; aplicación, residual queue y save siguen pendientes. Datos C++ regenerados; tests/compilación aplazados.

## Cobertura transaccional de quemadura

1. Harness de comando estándar comprueba Tackle real con daño por quemadura antes del truncado, bypass resuelto y stream RNG idéntico al cálculo sin quemadura.
2. Política sin resolver debe conservar PP, HP, acumulador de daño, estado, output y RNG. Revisión estática confirma publicación conjunta después del resolver; no fue necesario cambiar el runtime.
3. Pruebas escritas, sin ejecutar. Tests y compilación aplazados; dispatcher completo de estados y persistencia siguen pendientes.

## Persistencia de estados por actor

1. Payload Pokemon v7 conserva efecto, toxicTurnCount y presencia/valor de contadores sleep/freeze con validación y publicación atómica. Actores sin estado mantienen payload v6; lectura v1–v6 conserva ausencia de estado.
2. Capture/restore del actor ya transportan status. El guardado completo sigue rechazando estados hasta extender enemigos/reservas y el envelope de runtime; no se declara save de estados integrado.
3. Regresiones de contador máximo, optional cero, metadata inválida y lectura legacy escritas, sin ejecutar. Tests/compilación aplazados.

## Save de partida v15: estados de combate

1. Envelope/runtime v15 conserva status del jugador, enemigo y seis reservas de entrenador; equipo jugador usa payload actor v7. Lectura de v14 migra ausencia de status, sin inventar contadores.
2. Capture/restore conecta estos campos; active status debe coincidir con el miembro activo. Setup y doubles siguen fuera de esta capacidad. Límites de waves/checkpoints anteriores permanecen.
3. Regresiones de ida/vuelta y discrepancia activo/reserva escritas, sin ejecutar. Revisión estática/diff solamente; tests/compilación aplazados. Conexión del dispatcher de estados y validación en hardware siguen pendientes.

## Orden upstream de descongelación

1. Inspección pinned de MovePhase.checkFreeze/doThawCheck encontró curación demasiado temprana en el resolver. Ahora distingue indirect (cure inmediato sin draws), self-heal cualificado (evento thaw pendiente, sin contador/RNG) y rama de heal posterior al incremento (cure sin draw).
2. Dispatcher deberá aplicar thaw solo tras superar demás failure checks, incluyendo condición de tipo Fire para Burn Up. No se habilitan moves por asumir resueltas estas condiciones.
3. Regresiones de las tres ramas escritas, sin ejecutar. Tests/compilación aplazados.

## Chequeos de estados conectados al turno simple

1. executeActiveBattleMove aplica checkPokemonStatusBeforeMove antes del comando/PP; cancelación sleep/freeze/paralysis conserva PP y permite continuar el turno rival. La transacción externa advanceBattleTurn descarta mutaciones si otra fase falla.
2. Reducción de sueño se obtiene de declaraciones constantes ReduceStatusEffectDurationAbAttr de abilities canónicas; parámetros desconocidos fallan. Source pinned ab-attrs.ts reduce duración en uno. No se habilitan doubles, modos indirectos ni movimientos con bypass/curación de estado en esta ruta.
3. Catálogo regenerado con hash unchanged; regresión de reducción canónica escrita, sin ejecutar. Residuales, aplicación de estados por move, callbacks/tags completos y UI localizada aún pendientes. Tests/compilación aplazados.

## Residuales conectados al cierre del turno

1. finishBattleTurn aplica residual poison/toxic/burn después de WeatherEffect y antes del cierre/held turn healing, conforme phase-manager.ts. Interlude omite estados. HP/contador se publican junto con la transacción del turno y KO pasa por la conclusión existente.
2. Capacidad damageCallbacks existente prueba ausencia de callbacks pendientes para habilidades admitidas. Otras habilidades, bosses, final boss y doubles con residual fallan explícitamente; faltan políticas de bloqueos/reducción/PostDamage y orden compartido de campo. No se asume inmunidad ni se omiten callbacks.
3. Regresión de toxic con capacidad canónica y rollback de policy boss escrita, sin ejecutar. Aplicación de estados por moves/berries y callbacks completos pendientes; tests/compilación aplazados.

## Atributos canónicos para daño residual

1. Generador deriva BlockNonDirectDamageAbAttr, BlockStatusDamageAbAttr y ReduceBurnDamageAbAttr desde raw pinned; guarda máscara por status y ratio de burn, y marca parámetros/builders desconocidos sin resolver. Runtime consulta perfiles por ID canónico, sin ramas por nombre de habilidad.
2. Magic Guard bloquea residual conservando incremento del contador; Heatproof reduce burn a mitad. Poison Heal conserva máscara poison/toxic pero sigue sin resolver hasta implementar PostTurnStatusHealAbAttr. Política distingue habilidad activa y callbacks resueltos; pasivas/suppression completos pendientes.
3. Regresiones de catálogo real, reducción/bloqueo, habilidad inactiva y rechazo de callbacks pendientes escritas, sin ejecutar. Catálogo regenerado, hash unchanged. Tests/compilación aplazados.

## Curación posterior al turno por estado

1. PostTurnStatusHealAbAttr deriva máscara poison/toxic del catálogo y cura toDmgValue(maxHP/8) mediante política explícita de Heal Block/multiplicador. No cura status ni modifica counters; respeta HP máximo/faint.
2. finishBattleTurn aplica este efecto después de held turn healing y antes del reset de turno; interlude lo omite. Poison Heal deja de ser capability pendiente en este dominio porque ahora están ambos atributos de bloqueo/curación. No declara completa su interacción con pasivas/suppression ni doubles.
3. Regresiones de bloqueo toxic + curación, cap, Heal Block, efecto no elegible y policy desconocida escritas sin ejecutar. Catálogo regenerado conservando hash. Tests/compilación aplazados.

## Solicitud de estados por movimiento

1. resolvePokemonMoveStatusApplication porta StatusEffectAttr.apply: probabilidad antes de elegibilidad, draw solo cuando chance no es negativa ni exactamente 100, quiet para daño y solicitud de ObtainStatusEffectPhase. No muta actor ni aplica duración antes de la fase correspondiente.
2. ID de status se deriva del símbolo pinned en tablas generadas; parámetros desconocidos/múltiples atributos no se simulan. Chance/eligibilidad requieren políticas resueltas; RNG/output se conservan ante error de capacidad.
3. Regresiones con Thunder Wave real, inmunidad Electric posterior al draw, chance cero y política desconocida escritas sin ejecutar. Conexión a hit/PP/cola y políticas completas de habilidad/campo siguen pendientes; tests/compilación aplazados.

## Comando PP/hit para estados

1. usePokemonStatusEffectMove valida movimiento canónico de un atributo StatusEffectAttr, resuelve hit antes de chance/eligibilidad y publica PP/RNG/solicitud conjuntamente. Miss/bloqueo consume PP; error de capacidad conserva actor/RNG/output. Estado se aplica en fase posterior.
2. resolvePokemonStatusMoveHit comparte la rutina anterior de accuracy con StatStageChange status, incluyendo USER bypass y bloqueos antes del draw. No duplica fórmulas de precisión.
3. Regresiones Thunder Wave de hit/bypass, miss con draw y política no resuelta escritas, sin ejecutar. Integración de cola y proveedor completo de políticas aún pendiente. Tests/compilación aplazados.

## Fase de aplicación de solicitud aceptada

1. applyPokemonQueuedStatus separa ObtainStatusEffectPhase/doSetStatus de trySetStatus: no repite canSetStatus, conserva ID de destinatario/fuente, RNG de duración y reemplaza status tal como la fase upstream. Caller debe resolver pendingStatus, cancelación de hits, forms y reacciones antes de ejecutar.
2. Solicitud por movimiento y wrapper de elegibilidad rechazan NONE, conforme trySetStatus. Request con destinatario distinto o reacciones sin resolver conserva estado/RNG.
3. Regresiones de solicitud aceptada que no vuelve a consultar elegibilidad, duración seeded, identidad incorrecta y NONE escritas sin ejecutar. Cola activa/pendingStatus y callbacks completos siguen pendientes; tests/compilación aplazados.

## Estado pendiente y frontera de checkpoints

1. PokemonBattleState conserva pendingStatus de PokemonTurnData. enqueuePokemonStatusRequest porta transición normal de trySetStatus y rechaza solicitudes duplicadas; override requiere su resolver separado. applyPokemonQueuedStatus exige coincidencia de destinatario/efecto pendiente y lo limpia al aplicar.
2. Wrapper de obtención usa candidato para publicar solicitud/aplicación atómicamente. Run save/actor snapshot y finishBattleTurn rechazan fases pendientes; no silencian ni serializan una cola incompleta.
3. Regresiones de enqueue sin duración, duplicado, limpieza de pending y rechazo de snapshot escritas sin ejecutar. El runtime todavía debe producir/drenar esta cola desde el comando y resolver callbacks completos; tests/compilación aplazados.

## Comando completo de fases de estado

1. executePokemonStatusEffectCommand integra hit/PP, solicitud, pendingStatus y ObtainStatus sobre candidato; publica actor objetivo/usuario y streams solo al terminar. Stream de duración puede ser compartido o separado del usuario, conservando orden de draws.
2. Reacciones y políticas de campo/atributos deben estar resueltas; override pendiente se rechaza. No se habilitan silenciosamente movimientos en FirstRunRuntime: falta proveedor canónico completo de policy, inmunidades y callbacks/forms.
3. Regresiones con Sleep Powder real de aplicación/PP/pending, streams compartidos/separados y fallo de reacciones escritas sin ejecutar. Tests/compilación aplazados.

## Inmunidades canónicas de estado por habilidad

1. Generador importa StatusEffectImmunityAbAttr y UserFieldStatusEffectImmunityAbAttr en máscaras separadas. Constructor vacío bloquea todos salvo FAINT, conforme PreSetStatusEffectImmunityAbAttr.canApply. Parámetros dinámicos/conditionalAttr/condition y ConditionalUserFieldStatusEffectImmunityAbAttr permanecen sin resolver.
2. resolvePokemonStatusAbilityImmunity distingue ámbito propio/aliado, habilidad activa, callbacks y capacidad desconocida. Fallo no publica output; caller compone selfAbilityBlocks/allyAbilityBlocks en policy de aplicación. Aún falta integración del proveedor completo y reacciones posteriores.
3. Regresiones de Immunity real (poison/toxic), constructor vacío, inmunidad aliada, habilidad inactiva y condiciones desconocidas escritas sin ejecutar. Catálogo regenerado conservando hash; tests/compilación aplazados.

## Bypass de inmunidad de tipo por estado

1. Generador importa IgnoreTypeStatusEffectImmunityAbAttr como conjunto de estados y entradas por tipo; Corrosion pinned declara POISON/TOXIC y STEEL/POISON. Expresiones o condiciones desconocidas quedan sin resolver.
2. resolvePokemonStatusTypeImmunityBypass consulta ID canónico/tipo/estado y distingue capacidad, activación e input inválido. Caller compone bypass de Poison y Steel por separado; fuente ausente sigue inmune conforme canSetStatus.
3. Regresiones de Corrosion real, doble tipo, inactividad, otro estado, fuente ausente e input inválido escritas sin ejecutar. Catálogo regenerado/hash unchanged; integración del proveedor completo y reacciones pendiente. Tests/compilación aplazados.

## Composición de política de aplicación

1. composePokemonStatusApplicationPolicy une listas explícitas de habilidades propias/pasivas, aliados y fuente; deriva bloqueos por ámbito y bypass de ambos tipos desde tablas canónicas. Campo/tipos efectivos/grounding/Safeguard requieren contexto resuelto del caller.
2. No infiere activación/suppression ni omite componentes desconocidos. Error conserva output; fuente ausente limpia bypass. Corrosion nunca sustituye inmunidad por habilidad.
3. Regresiones de Corrosion vs Immunity, fuente ausente, fallo de callback y aliado real escritas sin ejecutar. Falta proveedor de contexto y reacciones/forms en FirstRunRuntime para habilitar moves de estado; tests/compilación aplazados.

## Contexto de tipos/campo para aplicación de estados

1. resolvePokemonStatusApplicationEnvironment combina identidad real de fuente, pendingStatus del actor y campo explícitamente resuelto (grounding, terreno, sol, Safeguard). Valida símbolos de tipos canónicos y publica policy atómicamente.
2. Preserva diferencia pinned de canSetStatus: poison/steel usan getTypes(returnOriginalTypesIfStellar=true), mientras paralysis/ice/fire usan isOfType por defecto. Contexto no infiere tipos vivos a partir de especie si existen overrides de runtime.
3. Regresiones de vistas Stellar, sleep en Electric terrain pese a ignoreField, identidad propia, pendingStatus y contexto inválido escritas sin ejecutar. Falta proveedor desde arena/actor dinámico y reacciones completas para habilitar moves en FirstRunRuntime; tests/compilación aplazados.

## Reacción canónica Synchronize

1. Generador deriva presencia de SynchronizeStatusAbAttr del catálogo; resolver aplica canApply pinned a burn/paralysis/poison/toxic con fuente presente. Produce nueva solicitud con identidades intercambiadas, sin mutar actor ni consumir RNG.
2. Habilidad se activa aunque la solicitud posterior sea rechazada por elegibilidad, siguiendo upstream. No aplica status directamente ni ignora inmunidades del causante. Condiciones/callbacks desconocidos conservan output y exigen dispatcher.
3. Regresiones Synchronize real, fuente Fire inmune, sleep, fuente ausente/inactiva y callbacks pendientes escritas sin ejecutar. Falta conexión de esta reacción a cola y proveedor completo de forms/ConfusionOnStatusEffect; tests/compilación aplazados.

## Confusión provocada por aplicación de estado

1. Generador deriva ConfusionOnStatusEffectAbAttr desde catálogo (Poison Puppeteer). Resolver exige canAddTag/callbacks resueltos, objetivo vivo y estado admitido; solicita CONFUSED con randBattleSeedIntRange(2,5) del causante.
2. Preserva peculiaridad pinned: addTag recibe opponent.id como sourcePokemonId. Simulated, bloqueo de tag y efecto no elegible no consumen RNG. Solicitud no inventa implementación de BattlerTag; fase/tag runtime siguen pendientes.
3. Regresiones de duración/stream, identidades, canAddTag bloqueado, simulated y policy desconocida escritas sin ejecutar. Catálogo regenerado con hash unchanged; tests/compilación aplazados.

## Confusión antes del movimiento

1. checkPokemonConfusionBeforeMove porta ConfusedTag.lapse PRE_MOVE del upstream pinned: decrementa primero, expira sin RNG, probabilidad 1/3 y daño físico de potencia 40 con variación 85–100. No consume PP ni incrementa Rage Fist/turnDamageDealt.
2. Estadísticas efectivas y callbacks de damageAndUpdate requieren política resuelta; falla atómicamente ante capacidad pendiente. Tag explícito todavía requiere lifecycle, checkpoint y conexión al dispatcher de FirstRunRuntime. No se declara confusión integrada al juego.
3. Regresiones de expiración/RNG, secuencia seeded y fallo atómico escritas sin ejecutar. Tests y compilación siguen aplazados.

## Ciclo de vida del tag de confusión

1. canPokemonAddConfusionTag distingue el probe simulado (inmunidades propias/aliadas) de addPokemonConfusionTag (también ConfusedTag.canAdd: Misty si grounded). Overlap precede callbacks y no refresca duración. Eliminación explícita limpia el estado.
2. Precisión adicional de provenance: ConfusionOnStatusEffectAbAttr pasa opponent.id a addTag, pero getBattlerTag(CONFUSED) construye ConfusedTag sin sourceId. La solicitud conserva el argumento upstream; el estado del tag no inventa una fuente que upstream descarta.
3. Regresiones de probe/terreno, overlap, inmunidades y fallo de capacidad escritas sin ejecutar. Dispatcher, almacenamiento y callbacks de daño/inmunidades continúan pendientes; tests/compilación aplazados.

## Inmunidad canónica a confusión

1. Generador deriva BattlerTagImmunityAbAttr/UserFieldBattlerTagImmunityAbAttr (símbolo o array) por habilidad, distinguiendo ámbito propio/aliado. Own Tempo real (20) bloquea CONFUSED en ámbito propio. Condiciones dinámicas/conditionalAttr y variante ConditionalUserField permanecen sin resolver.
2. resolvePokemonConfusionAbilityImmunity conserva output ante capacidad desconocida; activación y callbacks requieren contexto explícito. No declara implementadas suppression/pasivas ni PostSummonRemoveBattlerTagAbAttr.
3. Tabla regenerada con hash canónico conservado. Regresiones de habilidad real, inactividad y callback pendiente escritas sin ejecutar. Tests/compilación aplazados; conexión al dispatcher sigue pendiente.

## Reacción de estado conectada a creación de tag

1. executePokemonStatusConfusionReaction integra canAddTag simulado, duración desde RNG de fuente y addTag real sobre candidato. Misty bloquea creación después de consumir duración; tag existente evita la tirada. La política de aplicación permanece separada del probe para reflejar callbacks simulated vs reales.
2. Capacidad desconocida conserva tag/RNG/output. Solicitud retiene IDs como provenance sin inventar sourceId persistente para ConfusedTag. Dispatcher general y checkpoints aún requieren integración.
3. Regresiones Poison Puppeteer real: Misty con draw, creación por toxic, tag existente sin draw y fallo atómico escritas sin ejecutar. Tests/compilación aplazados.

## Confusión como estado del actor

1. PokemonBattleState conserva el tag de confusión en summon data; resetPokemonSummonState lo elimina al retirar el actor. Cambio de forma conserva status, pendingStatus y confusión en vez de perderlos al reconstruir estadísticas.
2. Snapshots de actor y run rechazan tags de confusión activos/inválidos hasta implementar serialización, evitando pérdida silenciosa. Esta frontera es temporal; no declara resuelta la continuidad de tags.
3. Regresión de recall escrita sin ejecutar. Dispatcher de movimiento/status y serialización siguen pendientes; tests/compilación aplazados.

## Snapshot de actor con confusión

1. Payload pokemon=8 guarda status opcional y contador de confusión presente; formatos 1–7 conservan su lectura y ausencia explícita de tag. Restore valida presencia/contador, y encode sigue usando v6/v7 cuando no hay confusión.
2. Run v15 continúa rechazando confusión porque aún faltan campos activos/enemigos/entrenadores; también rechaza un actor v8 con tag embebido para evitar pérdida al restaurar run. No se declara completo save/continue de confusión.
3. Regresiones de roundtrip v8 y truncamiento escritas sin ejecutar. Tests/compilación aplazados.

## Checkpoint de partida con confusión

1. Run/save runtime v16 conserva contador de confusión de jugador/enemigo activos y equipo entrenador; snapshots v8 conservan equipo jugador. Validación exige coincidencia entre actor activo y miembro, presencia/contador coherentes y slots vacíos sin tag.
2. Captura/restauración FirstRunRuntime copia tags y los repone tras cleanup de reconstrucción. Setup y segundo enemigo siguen fuera de esta frontera. Decoder migra v15 sin inventar tags; lectura anterior permanece.
3. Regresiones de run v16 y discrepancia de actor/miembro escritas sin ejecutar. Dispatcher de confusión todavía pendiente; tests/compilación aplazados.

## Confusión en el comando de combate individual

1. executeActiveBattleMove aplica confusión después de sleep/freeze y antes de paralysis; autogolpe cancela sin consumir PP. Usa tag propiedad del actor y RNG del usuario. Sleep/freeze que cancelan no adelantan contador/RNG de confusión.
2. Proveedor conservador exige combate individual, sin held modifiers/clima/boss del usuario, y habilidades de ambos actores con callbacks de estadísticas/daño cubiertos por la frontera actual. No finge soporte de tags/abilities/campo pendientes; creación por Poison Puppeteer todavía requiere conexión del dispatcher de estados.
3. Regresiones de restore/recapture de tags de ambos activos escritas sin ejecutar; checks de resolver ya cubren contador/autogolpe/RNG. Tests/compilación y validación del flujo completo siguen aplazados.

## Eliminación de confusión por habilidad adquirida/summon

1. Generador deriva PostSummonRemoveBattlerTagAbAttr con parámetros variadic constantes; Own Tempo (20) declara eliminación de CONFUSED. Resolver exige activación/callbacks resueltos, elimina tag y emite activación solamente si existía.
2. Respeta apply upstream sin inventar rama simulated. Invocación tras ganar habilidad/dispatcher post-summon completo permanece pendiente; recall ya limpia summon data independientemente de Own Tempo.
3. Regresiones de Own Tempo, inactividad, callback pendiente y ausencia de tag escritas sin ejecutar. Tests/compilación aplazados.

## Solicitud reflejada Synchronize conectada a aplicación

1. executePokemonSynchronizeReaction integra reacción canónica, enqueue de estado reflejado y ObtainStatus sobre candidato. Conserva identidad de ambos actores y emite activación incluso cuando inmunidad impide aplicar status.
2. Política reflejada/reacciones pendientes falla sin publicar actor/pendingStatus/RNG/output. No crea recursión falsa ni habilita moves de estado sin contexto completo; el dispatcher debe resolver callbacks adicionales.
3. Regresiones Synchronize real (burn aplicado, Fire inmune sin RNG, callback pendiente atómico) escritas sin ejecutar. Tests/compilación aplazados.

## Synchronize sobre fuente debilitada y simulación

1. enqueue distingue Fainted de InvalidState después de canSetStatus, conforme trySetStatus. Synchronize conserva activación y rechazo normal cuando la fuente está debilitada; no aborta el turno por ese rechazo.
2. Ejecución simulated mantiene canApply pero no solicita/aplica fase reflejada ni consulta política de aplicación pendiente, conforme SynchronizeStatusAbAttr.apply. Sin mutación de actor/pending/RNG.
3. Regresiones de fuente debilitada y simulated escritas sin ejecutar. Tests/compilación aplazados.

## Dispatcher de reacciones posteriores a estado

1. executePokemonPostSetStatusReactions une Synchronize del receptor y ConfusionOnStatusEffect de fuente, en el orden de ObtainStatusEffectPhase. Reutiliza perfiles canónicos; actualiza status de fuente/tag de receptor y streams compartidos/separados sobre candidato.
2. Forms y callbacks reflejados exigen resolución explícita. Dos actores distintos con fuente presente son el alcance actual; fases sin fuente/self-target y el proveedor completo del comando activo siguen pendientes. Error tardío revierte también la reflexión previa.
3. Regresiones de pareja real Synchronize/Poison Puppeteer y fallo tardío atómico escritas sin ejecutar. Tests/compilación aplazados.

## Acción completa de movimiento de estado y reacciones

1. executePokemonStatusAction une hit/chance/PP, pendingStatus/ObtainStatus y dispatcher de reacciones. Publica actores/tags/streams juntos; fallo posterior revierte también PP y el estado inicial.
2. Alcance de dos actores distintos y efecto sobre oponente; self-target requiere su dispatcher. Políticas explícitas de hit/campo/forms/callbacks no se sustituyen por defaults. FirstRunRuntime aún necesita proveedor para habilitar estos movimientos.
3. Regresiones de Poison Powder real con Synchronize/Poison Puppeteer y rollback tardío escritas sin ejecutar. Tests/compilación aplazados.

## Frontera FAINT en reacciones de estado

1. Dispatcher posterior omite forms/PostSetStatus/ConfusionOnStatusEffect para FAINT después de validar identidad y estado aplicado, conforme ObtainStatusEffectPhase.start pinned. No exige políticas ni consume RNG de callbacks omitidos.
2. Regresión de FAINT con políticas pendientes escrita sin ejecutar; proveedor del comando activo y Classic completos siguen pendientes. Tests/compilación aplazados.

## Evaluación condicional de reacción de confusión

1. Reacción consulta política de tag solo para habilidad activa, efecto elegible y receptor vivo en ejecución real. Casos no aplicables/simulated no requieren callbacks de tag ni RNG de duración; atributos activos desconocidos siguen fallando explícitamente.
2. Regresiones de burn no elegible, habilidad inactiva, objetivo debilitado y simulated con políticas pendientes escritas sin ejecutar. Proveedor del comando activo y validación final siguen pendientes.

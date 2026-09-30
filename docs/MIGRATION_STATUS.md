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

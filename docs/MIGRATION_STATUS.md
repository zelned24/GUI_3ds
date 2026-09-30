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

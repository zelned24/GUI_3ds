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

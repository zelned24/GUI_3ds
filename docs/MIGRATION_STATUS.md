# Estado actual y evidencia pendiente

## C√≥digo existente, pendiente de validaci√≥n final

1. Pins: Pok√©Rogue 8555c08c823b856cbec4eb99ca84ea52a955836d; assets 056a1f408f26a3be4fef243f7462cb43608c7928; locales 23aea1cb0da5a0b15b836f3c243791591cc42303. Autoridad: tools/js/data/PokerogueSource.js.
2. Conteos, hash y provenance: project/data/pokerogue/import-report.json y canonical-content.json. No son l√≠mites del engine.
3. Runtime limitado: encounters, RNG, da√±o, etapas, clima/cr√≠ticos/precisi√≥n/PP y familias migradas. Self-target stat buffs y Trick Room conectados; inmunidades sonora/polvo con metadata upstream y callbacks desconocidos expl√≠citos.
4. Guardado v8, journal dual y export .p3save. Validator limitado a waves 1‚Äì9/bioma inicial; restaurar clima no neutral exige capacidades de clima soportadas en ambos actores.
5. Conversi√≥n reportada de 2902 atlases a 2903 p√°ginas t3x, incluida espalda de Thundurus Therian en dos p√°ginas. Inventario f√≠sico en build/upstream-assets y build/romfs; animaciones upstream 10 FPS. Hardware no verificado.
6. ContentUpdateStore y lecturas indexadas acotadas existen; cat√°logos siguen compilados. OTA completa no disponible.

## √öltima ampliaci√≥n de combate ‚Äî pendiente de ejecuci√≥n

1. `HealAttr` constante: constructor importado ‚Üí `MoveHealProfile` generado ‚Üí `PokemonHealingEffect.hpp` ‚Üí turno real en `FirstRunRuntime.cpp` y score de IA. Fuente pinned: `src/data/moves/move.ts`, `HealAttr`; PP: `src/phases/move-phase.ts`, `usePP`; curaci√≥n: `src/phases/pokemon-heal-phase.ts`, `getHealAmount`.
2. Curaci√≥n propia con un √∫nico atributo: redondeo base half-up, multiplicador de HealingBooster resuelto por policy y redondeado hacia abajo, l√≠mite de HP, fallo a HP completo tras consumo de PP, Heal Block y cancelaci√≥n previa diferenciados. USER cuesta un PP sin Pressure. Sin draws RNG.
3. El runtime actual conecta actores sin items/passives/tags, multiplicadores neutrales. Rest, VariableHealAttr, curaci√≥n aliada y movimientos con efectos adicionales permanecen no soportados. Regresiones nativas escritas (400‚Äì410), sin ejecutar. Entrenadores contin√∫an bloqueados.

4. `HitHealAttr` basado en da√±o: ratios constantes generados (incluido default 0.5), ataque+drenaje at√≥micos en el turno y beneficio de IA. Fuente pinned `src/data/moves/move.ts`, `HitHealAttr`; da√±o aplicado seg√∫n `src/phases/move-effect-phase.ts`. M√≠nimo uno/floor, cap de HP y multiplicadores posteriores. No se ejecuta al fallar, inmunidad o da√±o cero.
5. Resolver de Liquid Ooze conserva reversi√≥n, redondeo negativo de PokemonHealPhase y bloqueo de da√±o indirecto mediante policy; el flujo real todav√≠a rechaza esta habilidad hasta conectar post-defend/indirect-damage completo. Strength Sap y atributos adicionales no se sustituyen por drain gen√©rico. Regresiones 411‚Äì418 escritas, no ejecutadas.

6. `RecoilAttr` constante: ratios/defaults y `useHp/unblockable` generados; resolver C++ + ataques simples conectados at√≥micamente al turno y score de IA. Rock Head/Magic Guard desde `BlockRecoilDamageAttr`/`BlockNonDirectDamageAbAttr` pinned. Fuente `src/data/moves/move.ts`, `RecoilAttr`, y `src/data/abilities/ab-attrs.ts`. Pruebas 419‚Äì426 escritas; sin ejecuci√≥n. Struggle tiene perfil de retroceso preservado pero su ataque completo (typeless, target, fallback PP) sigue pendiente.
7. Ca√≠da simult√°nea en el equipo actual de un solo Pok√©mon no se marca como victoria; `FaintPhase` upstream lleva a GameOver cuando no quedan Pok√©mon legales. Equipo completo, faint queue y estados de derrota/summary siguen pendientes.

## No completado

1. Equipos completos, todos los efectos/status/abilities/items, dobles y entrenadores jugables. battleInputSupported mantiene bloqueo de entrenadores.
2. Rewards, captura, mapa/biomas, progresi√≥n hasta wave 200, combate final y resumen.
3. Presentaci√≥n web adaptada a ambas pantallas, audio y rendimiento Old 3DS.
4. Save completo, migraciones de contenido y actualizaci√≥n firmada desde consola.
5. Ejecuci√≥n final de pruebas, compilaci√≥n 3dsx, Azahar y hardware.

## Limpieza aprobada por el usuario

1. Retirados servidor, HTML/CSS y editor/shell/preview web. Dependencias del pipeline trasladadas a tools/js, sin reconstruir el Studio.
2. Retiradas suites monol√≠ticas antiguas del Studio; conservadas migraci√≥n/RNG/battle/native. Nuevo ejecutor las registra sin modificar sus asserts; no se ejecut√≥.
3. Documentos hist√≥ricos consolidados en este estado, lista de pendientes y contrato OTA; mapa sem√°ntico obsoleto retirado. Provenance sigue en los datos can√≥nicos y c√≥digo.
4. Fallos hist√≥ricos de validaci√≥n del exporter y del preview pertenecen al Studio retirado, no se declaran arreglados. Suites actuales sin resultado validado todav√≠a.
5. Historial Git, assets, clones upstream y runtime/presentaci√≥n C++ permanecen. Plan y lista exacta de retiros disponibles en el commit de limpieza.

## ContinuaciÛn autorizada: flujo de victoria de entrenadores

1. El usuario autorizÛ ampliar cambios a gameplay, almacenamiento y pruebas. Tests y compilaciÛn siguen aplazados.
2. El plan BattleEnd/rewards/next-wave se crea ˙nicamente al derrotar todo el equipo enemigo, tanto en combate como al restaurar un checkpoint. Fuente pinned: `src/phases/victory-phase.ts`, `VictoryPhase.start`.
3. RegresiÛn aÒadida en `test/native/first_run_restore_harness.cpp`: KO del primer miembro conserva wave, no ofrece recompensas y el avance de EXP envÌa la reserva. Pendiente de ejecuciÛn.
4. Esto corrige el lÌmite entre KO individual y victoria del equipo; no habilita todavÌa la IA, entry effects ni todos los combates de entrenador.

## Clima: conexiÛn del reloj de campo

1. `finishBattleTurn` consulta el resolver existente de duraciÛn de clima, siguiendo `src/phases/turn-end-phase.ts` y `src/data/weather.ts` pinned. Los cambios de ambos relojes de campo se preparan antes de publicar cualquiera.
2. El checkpoint captura weatherType/turnsLeft/maxDuration reales. La restauraciÛn no neutral permanece rechazada; no se neutraliza silenciosamente.
3. La expiraciÛn que solicita cambios de forma permanece explÌcitamente bloqueada hasta conectar ese resolver. Efectos residuales, post-weather abilities, movimientos que cambian clima y selecciÛn de clima del bioma siguen pendientes.
4. RegresiÛn de correspondencia entre campo y save aÒadida; no ejecutada. No se declara P5 completo.

## Movimientos que cambian clima: datos y comando

1. Par·metros de `WeatherChangeAttr` generados desde metadata canÛnica real, con sourcePath/symbol/SHA-256. Snapshot actual: Sandstorm, Rain Dance, Sunny Day, Hail y Snowscape; sin IDs paralelos ni lÌmite de cat·logo.
2. `usePokemonWeatherChangeCommand` en PokemonBattleState prepara PP y clima de manera atÛmica: fallo por clima repetido/inmutable despuÈs de PP, bloqueo temprano sin PP, duraciÛn y coste de PP resueltos por el caller. Sin draws RNG ni asignaciones de heap.
3. Los callbacks de clima deben estar resueltos explÌcitamente antes de ejecutar el comando. El turno de FirstRunRuntime todavÌa no permite estos movimientos: faltan residual phase, post-weather callbacks y cambios de forma. Esta tabla y comando no se presentan como P5 completo.
4. Regresiones 427ñ434 con todos los registros reales importados, pendientes de ejecuciÛn.

## Clima conectado al turno para actores soportados

1. `PokemonWeatherPhase.hpp` conecta WeatherEffectPhase pinned para los cinco climas ordinarios: supresiÛn del campo, inmunidad de tipos/formas, blockers de habilidades y daÒo indirecto; el HP de ambos actores se prepara antes de publicar los cambios. Omite residuals durante el interludio X0, siguiendo `PhaseManager.onInterlude`.
2. `FirstRunRuntime` ejecuta WeatherChangeAttr con duraciÛn inicial cinco (sin FieldEffectModifier en el estado actual), coste PP/Pressure, condiciÛn de clima repetido/inmutable y reloj de TurnEndPhase. La IA conserva el beneficio cero heredado de MoveEffectAttr; no inventa un bonus de utilidad.
3. Guardado/restauraciÛn conserva duraciÛn y tipo no neutral para actores soportados. ExpiraciÛn a NONE se permite cuando no existen callbacks de forma/clima pendientes; en otros casos falla explÌcitamente.
4. Tabla generada de capacidades por cada ability ID canÛnico. Hooks de clima no migrados, como Rain Dish, Forecast e Ice Face, mantienen bloqueo; un ID desconocido nunca se trata como habilidad sin efectos.
5. Regresiones adicionales: daÒo residual real, inmunidad Rock, interludio, atomicidad frente a ID desconocido, restore de clima y expiraciÛn en un encuentro real. Todas pendientes de ejecuciÛn. No se ha compilado ni validado en Azahar.
6. Sigue pendiente extender los callbacks de clima, pasivas/items/tags, efectos de formas y selecciÛn de clima al entrar a nuevos biomas. Esta conexiÛn no demuestra P5 completo ni Classic completo.

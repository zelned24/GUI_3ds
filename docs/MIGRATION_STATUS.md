# Estado actual y evidencia pendiente

## Código existente, pendiente de validación final

1. Pins: PokéRogue 8555c08c823b856cbec4eb99ca84ea52a955836d; assets 056a1f408f26a3be4fef243f7462cb43608c7928; locales 23aea1cb0da5a0b15b836f3c243791591cc42303. Autoridad: tools/js/data/PokerogueSource.js.
2. Conteos, hash y provenance: project/data/pokerogue/import-report.json y canonical-content.json. No son límites del engine.
3. Runtime limitado: encounters, RNG, daño, etapas, clima/críticos/precisión/PP y familias migradas. Self-target stat buffs y Trick Room conectados; inmunidades sonora/polvo con metadata upstream y callbacks desconocidos explícitos.
4. Guardado v8, journal dual y export .p3save. Validator limitado a waves 1–9/bioma inicial; restaurar clima no neutral permanece bloqueado.
5. Conversión reportada de 2902 atlases a 2903 páginas t3x, incluida espalda de Thundurus Therian en dos páginas. Inventario físico en build/upstream-assets y build/romfs; animaciones upstream 10 FPS. Hardware no verificado.
6. ContentUpdateStore y lecturas indexadas acotadas existen; catálogos siguen compilados. OTA completa no disponible.

## Última ampliación de combate — pendiente de ejecución

1. `HealAttr` constante: constructor importado → `MoveHealProfile` generado → `PokemonHealingEffect.hpp` → turno real en `FirstRunRuntime.cpp` y score de IA. Fuente pinned: `src/data/moves/move.ts`, `HealAttr`; PP: `src/phases/move-phase.ts`, `usePP`; curación: `src/phases/pokemon-heal-phase.ts`, `getHealAmount`.
2. Curación propia con un único atributo: redondeo base half-up, multiplicador de HealingBooster resuelto por policy y redondeado hacia abajo, límite de HP, fallo a HP completo tras consumo de PP, Heal Block y cancelación previa diferenciados. USER cuesta un PP sin Pressure. Sin draws RNG.
3. El runtime actual conecta actores sin items/passives/tags, multiplicadores neutrales. Rest, VariableHealAttr, curación aliada y movimientos con efectos adicionales permanecen no soportados. Regresiones nativas escritas (400–410), sin ejecutar. Entrenadores continúan bloqueados.

4. `HitHealAttr` basado en daño: ratios constantes generados (incluido default 0.5), ataque+drenaje atómicos en el turno y beneficio de IA. Fuente pinned `src/data/moves/move.ts`, `HitHealAttr`; daño aplicado según `src/phases/move-effect-phase.ts`. Mínimo uno/floor, cap de HP y multiplicadores posteriores. No se ejecuta al fallar, inmunidad o daño cero.
5. Resolver de Liquid Ooze conserva reversión, redondeo negativo de PokemonHealPhase y bloqueo de daño indirecto mediante policy; el flujo real todavía rechaza esta habilidad hasta conectar post-defend/indirect-damage completo. Strength Sap y atributos adicionales no se sustituyen por drain genérico. Regresiones 411–418 escritas, no ejecutadas.

6. `RecoilAttr` constante: ratios/defaults y `useHp/unblockable` generados; resolver C++ + ataques simples conectados atómicamente al turno y score de IA. Rock Head/Magic Guard desde `BlockRecoilDamageAttr`/`BlockNonDirectDamageAbAttr` pinned. Fuente `src/data/moves/move.ts`, `RecoilAttr`, y `src/data/abilities/ab-attrs.ts`. Pruebas 419–426 escritas; sin ejecución. Struggle tiene perfil de retroceso preservado pero su ataque completo (typeless, target, fallback PP) sigue pendiente.
7. Caída simultánea en el equipo actual de un solo Pokémon no se marca como victoria; `FaintPhase` upstream lleva a GameOver cuando no quedan Pokémon legales. Equipo completo, faint queue y estados de derrota/summary siguen pendientes.

## No completado

1. Equipos completos, todos los efectos/status/abilities/items, dobles y entrenadores jugables. battleInputSupported mantiene bloqueo de entrenadores.
2. Rewards, captura, mapa/biomas, progresión hasta wave 200, combate final y resumen.
3. Presentación web adaptada a ambas pantallas, audio y rendimiento Old 3DS.
4. Save completo, migraciones de contenido y actualización firmada desde consola.
5. Ejecución final de pruebas, compilación 3dsx, Azahar y hardware.

## Limpieza aprobada por el usuario

1. Retirados servidor, HTML/CSS y editor/shell/preview web. Dependencias del pipeline trasladadas a tools/js, sin reconstruir el Studio.
2. Retiradas suites monolíticas antiguas del Studio; conservadas migración/RNG/battle/native. Nuevo ejecutor las registra sin modificar sus asserts; no se ejecutó.
3. Documentos históricos consolidados en este estado, lista de pendientes y contrato OTA; mapa semántico obsoleto retirado. Provenance sigue en los datos canónicos y código.
4. Fallos históricos de validación del exporter y del preview pertenecen al Studio retirado, no se declaran arreglados. Suites actuales sin resultado validado todavía.
5. Historial Git, assets, clones upstream y runtime/presentación C++ permanecen. Plan y lista exacta de retiros disponibles en el commit de limpieza.

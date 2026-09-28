# BETA-UI-8 — PokéRogue Canonical Integration Architecture Audit

**Tipo:** auditoría y plan, sin implementación de features.\
**Repositorio:** `https://github.com/zelned24/GUI_3DS.git`\
**Base inspeccionada:** `175497409a6640f3bde830977f8125f70ada34ec` (`git log`/HEAD al comenzar esta auditoría).\
**Upstream de datos configurado localmente:** `pagefaultgames/pokerogue`, commit `8555c08c823b856cbec4eb99ca84ea52a955836d`; assets `056a1f408f26a3be4fef243f7462cb43608c7928`; locales `23aea1cb0da5a0b15b836f3c243791591cc42303` (`public/js/data/PokerogueSource.js`).
**Alcance:** lectura de modelos, consumidores, imports, UI, runtime, generadores, fixtures y tests. No se borró, movió ni reescribió código de producto.

## 1. Executive Summary

GUI_3DS ya tiene dos conjuntos de infraestructura aprovechables: (1) un Composer/editor de escenas con modelo declarativo, dual screen, componentes, timeline, validación y generación determinista C++; (2) una cadena nativa C++/Citro2D con ScenePlayer, gestión de assets, RomFS y harness de paridad. Además hay adaptadores/importers upstream parciales y un BattleEngine desacoplado de DOM con commands, events y phases.

Sin embargo, la integración jugable no es aún una adaptación de PokéRogue. `AppShell` + `WaveManager` + pantallas de gameplay implementan un vertical slice de diez waves. En `WaveManager` se fijan species, levels, mini/final boss, recompensas y límite; la UI repite `10` y textos/nombres de Golem. No se encontraron modelos/pipelines conectados para `GameMode`, mapa, biome, encounter pools, items de run, transformaciones, save de run o content update. El `BattleEngine` implementa un subconjunto local de reglas, no una paridad demostrada con upstream.

**Recomendación central:** preservar Composer y runtime nativo como infraestructura de presentación/runtime; congelar el gameplay actual como bridge de demostración; portar semántica y contenido desde revisión upstream fijada, primero con modelos/versionado y luego resolvers/flow y bindings dinámicos. No crear un segundo proyecto ni hacer que scenes/timeline sean dueñas de reglas de juego.

## 2. Current Repository Architecture

```text
Studio DOM UI
  → ProjectModel / SceneModel / Components / Timeline
  → PreviewRuntime / CanvasRenderer
  → Validator / SceneCppExporter / CodeGenerator / AssetPackager
  → generated C++ + scene manifest
  → ScenePlayer / RuntimeAssetManager / Renderer2D / input

Separate playable web slice:
  AppShell → ScreenManager + InputManager + WaveManager
           → RunSetup / WaveIntro / Battle / Result / Summary DOM screens
           → BattleState → BattleEngine → Phase resolvers → Events

Partial upstream content path:
  PokerogueSource → PokerogueRepository → PokerogueImporter/Adapter
                  → CanonicalModels / DataManager / PokerogueManifest
  AssetIndex / AssetResolver / PokemonSpriteResolver / AudioResolver
```

Estas rutas existen en el mismo producto pero no convergen en un modelo común de run/presentación. `SceneCppExporter` declara explícitamente que exporta escena sin lógica de gameplay. Las screens jugables son DOM y la salida C++ actual exporta escenas/components, no el loop completo de PokéRogue.

## 3. Target Architecture

```text
Upstream repos + exact revisions
 → source-specific importers / normalizer / unknown-field preservation
 → canonical content + explicit 3DS overrides
 → mode/run/game-flow resolvers (seeded)
 → resolved encounter + runtime Pokemon/item/transformation state
 → immutable presentation view models + typed scene bindings
 → Composer-authored templates / timeline / animation
 → deterministic C++ / Citro2D / ROMFS
 → 3DSX

Content release path:
canonical output → diff → content package → hash/signature/compatibility
                → verify → staged install/rollback
Save path:
run/system state → saveVersion + contentVersion + runtime compatibility
                 → validation and explicit migration
```

Data, presentation, runtime y content-update mantienen ownership separado. El runtime resuelve/ejecuta; el editor autoriza/preview/valida/exporta. Las pantallas envían commands; engine genera events; bindings proyectan el estado. El Composer no gobierna flow/combat.

## 4. CURRENT → TARGET → GAP Matrix

| Sistema | CURRENT | TARGET | GAP | REUSABLE | DUPLICATED | UPSTREAM REQUIRED | 3DS-SPECIFIC | RECOMMENDED NEXT STEP |
|---|---|---|---|---|---|---|---|---|
| Game flow | `AppShell` enum de screens y callbacks; loop lineal setup→wave→battle→result | state machine de run con Continue/New, nodo, biome, wave, battle/event, reward, terminal | No run coordinator persistente; no map/event/continue real; flow derivado del UI | `AppShell`, `ScreenManager`, `InputManager` como borde de UI/input | Game screens DOM frente a scene authoring, propósitos distintos pero integraciones no conectadas | Orden y condiciones de fases | input, lifecycle y recursos del dispositivo | mapear transiciones/owner/eventos; especificar `RunFlow` sin migrar screens aún |
| Game modes | no `GameMode`/policy registry en el core jugable | Classic, Endless, Daily, Spliced Endless, Challenge por policy/config | ClassicOnly implícito en manager, renderer UI y resumen | `DeterministicRNG`, comandos/eventos como seam | flags locales no conforman abstracción reusable | mode definitions y mode-specific rules | restricciones de memoria y controles, no semántica | inventariar modos y reglas del commit upstream fijado |
| Classic waves | `WaveManager.totalWaves=10`, boss wave 10, mini-boss 5, fórmulas para especies/nivel/rewards | data-driven `WaveDefinition`/biome/encounter/reward catalog; scene bindings reutilizables | no implementación de Classic real ni catálogo canonical | `WaveManager` API como bridge temporal; DataManager parcial | wave facts en manager, WaveIntro, BattleScreen, Summary/Options | wave schedule, bosses, biomes, encounter/rewards | presentación compacta/top-bottom | clasificar código de demo KEEP/REFACTOR/ARCHIVE/REMOVE según tabla §7 |
| Endless | no hay política ni estado Endless | progresión/pools/transiciones/end condition del modo, límite de contenido opcional | manager clamp fijo; flow visual muestra `/10`; no mode rules | seed/PRNG y futura definición común de wave | comparte demo Classic por ausencia de policy, no hay módulo Endless duplicado | progresión Endless y contenido mutable | capacidad runtime no depende del límite de waves | definir interfaces mode policy primero; no fijar cifra histórica como límite engine |
| Map | sin definición ni graph resolver | grafo de nodes/edges con tipos extensibles y elección por modo | ninguna ruta/mapa runtime | no system map existente | ninguno demostrado | node availability/transitions | UI de selección con D-pad/touch | levantar semántica upstream; diseñar schema separado de wave |
| Biome | background assets/arena de scene assets; `BattleState.weather` no es biome | entity lógica con localized labels, pools, trainers, music, visual, transitions, provenance | asset/background no equivale a biome; sin registry lógica | asset pipeline, locale importer, asset resolvers | template visual vs biome model son layers a conectar, no duplicados | biome enum/data, encounter/trainer tables, transitions | formato/dimensiones/audio/memoria | localizar archivos upstream y definir importer/canonical mapping |
| Encounters | `WaveManager` elige Pikachu/Golem; `BattleScreen` crea battlers desde esos IDs | seeded resolver contextual → `ResolvedEncounter` | no rules/pools/trainers/forms/modifiers | `PokemonBattleData` construction seam, DataManager lookup | lógica fija está en manager y UI | encounter selection, rarity, trainer/boss/event logic | fallback ante asset ausente; budget de sprites | extraer sólo después de mapear fuente upstream; cubrir seed y provenance |
| Pokémon runtime | `PokemonBattleData`: species/name/level/stats/HP/moves/ability/status/stages; party de uno por lado | estado de runtime alineado a necesidades upstream: form, stats, held/passive items, modifiers, transformation y vista | partial; stats deterministas locales/semillas arbitrarias de prototype; serialization incomplete | modelo canónico partial y states/events como seam | `SpeciesDefinition`, `PokemonBattleData`, `PokemonSpriteNode` son responsabilidades válidas separadas, no fusionar | entity/state semantics, forms, IV/EV/nature, move/ability, transformations | render state, sprite frame/scale | modelar estado por ownership y comparar campo a campo upstream |
| Battle | commands/events/phases/engine/session funcionan en tests locales; synchronous simplified one-on-one | port/adaptation con trazabilidad de fases/efectos a reglas upstream | no paridad completa; sólo subset de movimientos/abilities/status/AI y gen formula afirmada por comentario, no evidencia de parity | command/event/session seams, PRNG test/replay, native harness conceptual | `BattleLabUI` y playable `BattleScreen` son dos frontends del bridge; engine único | turn phases, move/ability/item/weather/status/AI/battle rules | fixed-point/performance, no Phaser/DOM, input latency | congelar ruleset, documentar divergencias, crear port map antes de ampliar |
| Items | `ItemDefinition` mínimo, map vacío/reserva en DataManager; no instances/resolver/shop flow encontrado | `ItemDefinition/Instance/Resolver` para pools/held/passive/consumables/reward/icons | no importer/canonical dataset/lifecycle; no found item-specific `if` gameplay branch | canonical item shell y asset category/index | catálogo de asset “items” ≠ entidades de gameplay | modifier pools/effects/categories/transfers | icons/atlas/memory | analizar modelo upstream Modifier; no añadir UI item hardcodes |
| Transformations | no `TransformationState`; Sprite node tiene form/shiny/facing declarativos; string form no gameplay | state + resolver data-driven para rules, form, sprite, scale, effects/UI | no modeled runtime transformations | `PokemonSpriteNode`, Pokemon sprite/asset resolver (con cautelas §16) | visual properties no sustituyen state de juego | form changes / mega/primal/dynamax/gmax/tera semantics | sprite dimensions/scale/effects budget | port map visual-vs-rule transform capabilities |
| Scene Composer | `SceneModel`, nodes, schema, properties, component registry, history, canvas, timeline/clips/sequencer/composition, validators, exporters | permanecer en presentation/authorship con dynamic slots y scene templates | falta slot/binding schema, resolved data source, test chain C++ native for gameplay data | gran parte del Composer es sólida/reusable | `ProjectModel` (editor project) y `SceneModel` (scene) complementarios; serialization aliases redundan dentro de SceneModel (`nodes/components`) | datos de qué mostrar y anim data/assets; no reglas | dual screen, pixel rounding, controls, C++ backend | preservar estable; extender schema vía compatibility/migrations después del contrato bindings |
| Dynamic bindings | resolver de Pokémon/assets y properties authoring; escenas JSON fijas; no runtime data-binding registry | typed binding registry, view model, template selection, asset/effect resolver | Composer no se alimenta desde Run/Encounter state; C++ exporter exporta literal values | asset IDs, component props, event seams | HTML scene screens y Composer templates dos presentation routes a integrar con un contrato | semantic IDs/fields locales y upstream | resolver assets por RomFS, input focus | contrato binding y validación; no meter data fetching en nodes |
| Import/data pipeline | pinned repository configs; enums/locales/species/move/ability parsers; manifests; asset index y tests offline | completo por dominio, normalization, extension preservation, overrides, generated content | regex/parser específico; sólo vertical slice; `CanonicalModels` usa defaults; offline importer sintetiza declarations; DataManager baseline marcado fallback pero es producción singleton | Source/Repository/Importer/Adapter/Manifest/AssetIndex | AssetResolver/PokemonSpriteResolver especializan distintos índices con solapamiento | totalidad de dominios listados §26 | 3DS override/conversion y provenance de asset | delimitar dataset Fallback de runtime antes de usar como prod; ampliar domain por domain |
| Content update | no package/diff/signature/distribution/install | manifest/package/entry, versions, authenticity/integrity, staged install | todo excepto manifests/import hashes parcial es conceptual | deterministic `PokerogueManifest`, AssetIndex | upstream manifest vs generated scene manifest son distintos | source revision changes/diffs | storage, free space, safe activation | definir threat/key lifecycle + versions antes de package format |
| Save | ProjectDocument/RecentProjectsManager guardan workspace/editor docs; SceneModel migration v2–v5 | save system/run serialization/version validation/migration | no run save/load/continue; no content/runtime compat tags | project load/save and schema migration patterns only | editor persistence is not gameplay save | canonical save fields/migrations, validation/unsupported version | storage backend and failure UX | inventory upstream save domains; keep project persistence separate |

## 5. Game Flow Audit

`AppStates` define BOOT, TITLE, SETUP, OPTIONS, WAVE_INTRO, BATTLE, RESULT, REWARD, RUN_SUMMARY y DEBUG (`public/js/shell/AppShell.js`). Implemented operationally: `init()` pasa BOOT a TITLE; `startNewRun()` reinicia el manager y transiciona a WAVE_INTRO; `advanceWave()` avanza o resume; BattleScreen instancia estado desde manager. `CONTINUE`, MAP y EVENT no aparecen como estados/implementación. `REWARD` está en enum pero no registrado como screen en el `AppShell` leído. `RunSetupScreen` combina setup/starter select y permite species/level; no hay selección compatible con starters/costes/challenges upstream. Result y summary proyectan el slice local.

Datos hardcoded observados: `WaveManager` usa `totalWaves=10`, inicia Pikachu level 15, alterna Pikachu/Golem, fija los levels y boss labels y calcula score/exp por multiplicación. `WaveIntroScreen` muestra `/10`, “Alpha Golem” y texto en ES/EN; `RunSummaryScreen` describe victory sobre wave 10; `OptionsScreen` agrega quick access w1/w10. El título comienza “START RUN”; no se encontró Continue real en el flujo auditado.

Recomendación: conservar ScreenManager/InputManager como adapter de presentación, especificar estados/events/ownership de RunFlow y separar transitions del DOM. GameMode elige qué eventos/node sequence son válidos; no hacer un flujo Classic universal.

## 6. Game Modes Audit

No existe `GameMode`, `GameModeDefinition`, strategy/policy registry ni `mode.rules`. La cadena usa `WaveManager` único y el estado tiene `wave`; tests llaman recorrido Waves 1–10. Eso es `ClassicOnly` por comportamiento implícito, aunque no tenga ese nombre. Endless, Daily, Spliced Endless y Challenge no son extensiones que puedan agregarse sin modificar el manager y screens actuales: máximo, biome/map/pools/rewards/stop conditions están asumidos/fijados en ellos.

Target conceptual: selección de `GameModeDefinition` + políticas composables; `mode.rules.maxWave` representa límite definido por contenido cuando aplica, nunca una capacidad global. Añadir un modo debe registrar diferencias declarativas/resolvers sin copiar todo GameFlow.

## 7. Classic Audit

No hay colección upstream de 200 waves, `WaveDefinition`, `BiomeDefinition`, `MapDefinition`, `EncounterDefinition` ni `RouteDefinition`. `WaveManager.getWaveDefinition` calcula onda y devoluciones a demanda; no son 10 JSONs físicos. `test/fixtures/fallbackVerticalSlice.js` contiene solamente baseline de species/moves/abilities/item-like fixture entries, no catálogo de waves. No vi waves JSON en screens/project. Es una progresión algorítmica de test/demo.

| Elemento actual | Evidencia/uso | Clasificación recomendada (sin actuar ahora) |
|---|---|---|
| `WaveManager` lifecycle/stat aggregation | importado desde AppShell; tests 13.2–13.4 y UI | **KEEP** interfaz de bridge temporal; **REFACTOR** ownership/progression al acordar GameFlow |
| species alternation, level formula, 10 clamp | dentro de `getWaveDefinition/resetRun/advanceWave` | **REPLACE FROM UPSTREAM** semántica/contenido; implementar después, no mantener reglas demo como canonical |
| Golem bosses / `score=num*100`, `exp=num*50` | hardcoded manager/screens | **ARCHIVE** sólo si se necesita conservar demo; caso contrario **REMOVE** tras migración y referencia/uso auditado |
| QA Wave 1/10 | OptionsScreen debug shortcuts | **KEEP** como tooling temporal, después parametrizar por datos de test |
| Playable UI loop tests | cubren slice y contrato API | **KEEP** como bridge regression mientras dure; luego sustituir golden/source-parity con fixtures explícitos |

No hay evidencia para una decisión de borrado hoy; buscar usages y retirar sólo cuando el flujo canonical reemplace el slice.

## 8. Endless Audit

No hay maxWave configurable, biome transition policy, endless progression, encounter policy, periodic boss scheduler ni no-map behavior. El clamp a 10 más `/10` en UI impide que manager + pantalla acepten Endless sin refactor. `mode.rules.maxWave` es necesario como contenido/modalidad si upstream define un fin; si no, engine no debe imponer uno. La estimación `~5850` del documento de entrada no se ha verificado en commit local fijado y no se adopta como regla.

## 9. Map Audit

No aparecen `MapDefinition`, `MapNode`, `MapEdge`, graph/path/route selector. El flow actual incrementa un número. No hay nodes para battle/elite/boss/event/shop/transition/mode-specific; tampoco lista modelada. Mantener modelo como grafo con edges y reglas de disponibilidad, dejando UI de mapa como consumidor. Verificar qué modos de upstream usan map antes de hacerlo obligatorio.

## 10. Biome Audit

No existe entidad/catálogo lógico de biomes ni pool/trainer/music/transition/locale metadata. Hay backgrounds/arenas/assets registrados en `project/data/assets/asset-index.json`, escenas con background y campo climático weather/terrain en `BattleState`; ninguno equivale a `BiomeDefinition`. La definición visual de un scene template debe quedar aparte de biome ID y sus reglas. AssetIndex aporta metadatos de recursos/provenance, no las relaciones bioma→pool o biome progression.

## 11. Encounter Audit

No existen los cuatro modelos/resolver solicitados. `WaveManager` elige directamente enemySpeciesId y level; `BattleScreen.enter` llama `createBattleStateForCurrentWave`; `WaveIntroScreen` resuelve ese ID y sus textos. Así los encounter choices están hardcodeados fuera de escenas JSON pero acoplados al wave manager y UI. Species sprites en `project/screens/PikachuEntrance.json` son escenas authoring demo. Resolver futuro debe consumir `mode/wave/biome/seed/rarity/type/modifiers` y devolver IDs/form/level/trainer/modifiers; UI recibe sólo el resultado resuelto.

## 12. Pokémon Runtime State Audit

`PokemonBattleData` (`public/js/battle/BattleState.js`) guarda referencia Species, IDs/nombre/nickname, level/types/base/actual stats, HP, ability, moves simples, status, stages, fainted y turns. Carece de form index/state, held/passive item instances, varios modificadores, transformación, serialización canónica, persistent-vs-wave-vs-turn sub-states y party breadth. Clonación copia campos explícitos.

`SpeciesDefinition` contiene normalized slice para identity, locale, types, base stats, abilities, learnable/egg moves/forms y sprite metadata; `PokemonSpriteNode` tiene presentación authoring properties. No juntarlos: contenido, runtime state y node visual tienen lifecycle distinto. El cálculo de stats en prototype fija IV=31/EV=0 y ability/moves fallback locales; es baseline determinista, no fórmula parity demostrada.

## 13. Items Audit

`ItemDefinition` sólo modela id/name/category/tier/price/description/source/schema. `DataManager.items` inicializa Map y expone getters implícitos/empty, pero `loadDataset` no carga items. `PokerogueAdapter` importa `ItemDefinition` en el import list pero vertical slice/import workflow observado no devuelve items. Assets categoría `items` e imágenes de item no son gameplay item definitions. No encontré `ItemInstance`, `ItemResolver` ni `if item == X` de gameplay distribuido; no hay tal sistema que consolidar.

Portar semántica real de Modifier/Item y lifecycle, y mapear held/passive/consumable/shop/reward a identidad común con subtipos/behavior data. UI usa bindings/icon resolver, sin branch de item IDs.

## 14. Transformations Audit

No encontré `TransformationState` o resolver. `SpeciesDefinition.forms` es lista nominal que cae por default a BASE; `PokemonSpriteNode` guarda un form string para preview, pero `resolveAsset` pasa National Dex ID solamente. No se resuelve forma, scale/effect/rule o escena según mega/primal/dynamax/gigantamax/tera. Modelo destino es state + rule resolver + visual resolver; nunca scene por transformación.

## 15. Battle Engine Audit

### Lo que está implementado

- `BattleCommand.js`: SelectMove/Forfeit validan commands.
- `BattleEvents.js`: tipos/serialización de eventos.
- `BattlePhases.js`: action order, move execution, damage, effects, faint check, turn end.
- `BattleState.js`: estado 1v1, seed/RNG LCG, snapshots/replay.
- `BattleEngine.js`: dispatch, listeners, queue/step, resolver síncrono y rewind.
- `BattleSession.js`: init, command log y lifecycle headless.
- Tests comprueban invalid command no muta state, orden determinista, repetibilidad y harness de paridad local.

### Subset local y diferencias no probadas

`PokemonBattleData` pone IV 31 y EV 0; la fórmula local es estándar simplificada. El estado inicia una unidad por party. Engine elige AI enemiga por mayor base power, usa un conjunto reducido de phase resolvers/move effect/ability y finaliza turno/simulación en una sola llamada síncrona. El `BattleState.random()` LCG existe, pero no prueba alineación con PRNG/streaming/consumos upstream. `parseAbilities` del importer construye condicionales/efectos simplificados por reconocimiento de `STATIC` y `STURDY`; eso no es traducción general de effects.

No se certifica ninguna regla individual como fiel a PokéRogue. Comentario de `BattleEngine` que afirma “100% Gen 9 damage breakdown” no es evidencia de paridad de PokéRogue. El engine debe quedar **frozen as prototype/bridge/porting target**: conservar seam Command/Event/Phase, harness y modelos de test; no ampliar mecánica local hasta trazar upstream phase → canonical behavior → 3DS resolver/test. Distinguir capacidad matemática portable de reglas que requieren nueva capability/runtime.

## 16. Scene Composer Audit (BETA-UI-7)

### Keep como infraestructura de presentation

`SceneModel` mantiene dual screen, nodes/components, tracks/keyframes, clips/sequence, markers/audio cues, metadata y migraciones schema. `UINode`/`Transform`/`PropertySystem`/`ComponentRegistry` soportan grafo y propiedades declarativas. `TimelineUI`, `AnimationTrack`, `Keyframe`, `Interpolation`, `AnimationClip`, `ClipLibrary`, sequencer e `TimelineEvaluator` implementan autoria/evaluación. `SceneLibrary`, `CompositionNode`, `HistoryManager`, `CanvasRenderer`, `PreviewRuntime`, validadores y generación exportan/describen scenes. En native hay `ScenePlayer`, `RuntimeAssetManager`, `Renderer2D` y input. Test suite incluye determinismo, golden, asset packaging y paridad de interpolación native.

`SceneCppExporter` declara cero gameplay; `CodeGenerator` también exporta componentes/layout hacia APIs UI native. Preservar los contracts/formats mientras se agregan bindings con schema versionada. No mover reglas a ComponentRegistry/Canvas/Timeline.

### Límites

El PreviewRuntime simula foco/input/animation de authoring, no run/battle resolver completo. C++ `ScenePlayer` ejecuta scene definition/timeline/assets; `main.cpp` carga una escena demo. Scenes JSON ejemplo/PikachuEntrance son contenido authoring/demo, no gameplay SceneTemplates. Por tanto el Composer está relativamente sólido en presentation export/animation, pero no está integrado al gameplay.

## 17. Dynamic Scene Bindings Audit

Hay `PokemonSpriteResolver`, `AssetResolver`, `AudioResolver`, asset IDs y component properties; no se encontró registry de semantic bindings Biome/EncounterType/Event/Boss/Transformation/Species/Item. HealthBar/MoveButton exporters serializan valores literales. `PokemonSpriteNode` inicia propiedades Pikachu/dex 25; `project/screens/PikachuEntrance.json` y demos describen assets específicos. `WaveIntroScreen` fija copy Golem y `BattleScreen` consume los species IDs del manager.

Migración conceptual: editor crea template + slots de datos; validar binding against schema, elegir `ResolvedGameData`, resolver IDs a catálogo/assets físicos en preview y C++; renderer dibuja estado. Mantener escenas Pikachu/fixtures bajo demo y no tratarlas como plantilla dinámica ni contenido de producción.

## 18. Asset/Data Pipeline Audit

### Reutilizable/existente

`PokerogueSource` congela repo URLs/revisions; `PokerogueRepository` cachea fuente por revision/path; `PokerogueImporter` analiza slices de TS, enums y locale; `PokerogueAdapter` mapea objetos a `SpeciesDefinition`, `MoveDefinition`, `AbilityDefinition`; `PokerogueManifest` registra repository/revision/path/hash/schema y ordena salida. `AssetIndex` inventaría recursos físicos/statuses, hashes binarios, provenance y salida determinista; `AssetPackager` valida y convierte assets para RomFS. Tests tienen fixtures físicos y parser fixtures.

### Gaps/riesgos encontrados

- `DataManager` singleton llama `loadBaseline()` en constructor y marca `isFallback`; su adapter construye baseline vertical slice. Existe `test/fixtures/fallbackVerticalSlice.js`, y su test exige que fixture esté separado. El mismo patrón de synthetic declarations offline en importer puede producir source-like data que requiere flag explícito; test offline no es una source sync.
- `CanonicalModels.js` usa defaults semánticos para datos ausentes (stats 40, moves/status categories y IDs fallback) y no ofrece raw extension bag genérica; normalizer puede ocultar campos upstream ausentes/desconocidos.
- El regex parser cubre patrones concretos, no compila/ejecuta TS ni necesariamente obtiene la semántica anidada real de upstream. Full imports de moves/abilities requieren pruebas de cobertura/schema en pinned revision.
- Manifest de importer tiene source provenance para files/entities parciales. Asset Index puede incluir local assets e import; asegurar uniformidad de SHA-256 real, source revision/path, source repo, schema version y transform metadata por asset.
- `PokemonSpriteResolver.verifiedRegistry` contiene un catálogo corto con literals de hash que se parecen a sentinels/ficticios y los métodos de registro rellenan rutas `images/pokemon/{id}`/`romfs/...` y hash `verified_local`; no prueban archivo físico dentro de ese class. Este dato contrasta con AssetIndex/packager, que sí buscan archivo físico/hash real. Evitar afirmar que el resolver por sí mismo acredita existencia/provenance hasta reconciliarlo con índice físico. Las pruebas “no fictitious paths” pueden validar una vía distinta y no sustituyen auditoría de cada método.
- Se ven varias fuentes/índices de assets: `AssetIndex` físico central, `AssetResolver` que carga el index y mantiene catalog copy, y `PokemonSpriteResolver` con registry separado; `AudioResolver` especializado. No es prueba de duplicación dañina para Audio o especies, pero sí solapamiento de ownership en imágenes/Pokémon a resolver.
- Escenas generated y manifest son outputs derivados; `project/data/assets/*report.json` son informes generados, no canonical content store.

Todo import productivo debe preservar extension/raw metadata segura y separar overrides. Reusar parsers sólo después de fijar golden samples contra commit exacto y exigir diagnósticos de campos no mapeados.

## 19. Content Update Audit

No se hallaron modelos o runtimes de `runtimeVersion/contentVersion/saveVersion`, `ContentPackage`, `ContentEntry`, firma, diff, release, downloader, verify/install/rollback. `PokerogueManifest` es import manifest y la scene/asset manifest es metadata de build: hash y `schemaVersion` parcial no constituyen un content package. Actualizar solamente records importados/asset bundles hoy requiere tooling de build, no updater 3DS. Especificación completa en roadmap solamente.

## 20. Save Audit

`ProjectDocument` y `RecentProjectsManager` guardan/reabren proyectos/pantallas del editor; `SceneModel.migrateV2ToV3/V3ToV4/V4ToV5` migra schema de scene. No encontré serializer de run/player party, saveVersion/contentVersion/runtimeCompatibility, run restore/continue ni compatibility validator para gameplay. Estas migraciones son patrón reusable pero no save de juego. Upstream `GameData` incluye save data de system/session, validación y migrators separados (`src/system/game-data.ts`, `src/system/version-converter.ts`); estudiar el split antes de definir datos locales.

## 21. Duplication Audit

| Hallazgo | Interpretación por dependencias | Decisión conceptual |
|---|---|---|
| `AssetIndex` vs `AssetResolver` | Index es almacenamiento/physical validation; resolver es lookup y config de node, pero mantiene copia de catálogo. | **KEEP** ambos roles; **REFACTOR** resolver para delegar identity/physical status al index y evitar catálogo divergir. |
| `PokemonSpriteResolver` vs `AssetIndex/AssetResolver` | Resolver especializado con metadata separate; valores/synth paths weakens source-of-truth. | **KEEP** domain API; **MERGE** catalog/provenance lookup sobre AssetIndex después de verificar compatibilidad. |
| `AudioResolver` vs asset system | Resolver de audio/formato especializado; no es duplicado sólo por resolver assets. | **KEEP** como adapter especializado que consulta el índice común. |
| `ProjectModel` vs `SceneModel` | Project agrupa documents/scenes/history; Scene es composition/timeline. Diferente ownership. | **KEEP**; documentar relationship y evitar segunda raíz de escena. |
| `nodes` vs `components` aliases/serialization | SceneModel soporta ambos al cargar y emite ambos en `toJSON`; es redundancia dentro de serializer por compatibilidad. | **KEEP** hacia atrás; futura canonical schema con migrator explícito, luego quitar alias sólo con evidencia de consumers. |
| browser Canvas/Preview vs C++/Citro2D runtime | Web authoring/preview frente a device execution; no son implementaciones idénticas pero requieren parity contract. | **KEEP** dos targets; reforzar contrato/parity. |
| playable DOM game screens vs Composer scenes | DOM screens hacen gameplay presentation; scene system hace authored/exported layout. No comparten binding/runtime. | **KEEP** como prototipo durante transición; **REFACTOR** una sola presentation contract antes de retirar DOM screens. |
| `BattleScreen` vs `BattleLabUI` | Ambos observan/accionan el BattleEngine para dos workflows (run y lab). Engine compartido; frontends separados. | **KEEP** frontends; no duplicar rules en ellos; ambos son prototipo hasta flow integration. |
| `DataManager` vs `PokerogueAdapter/Importer` | Registry/query, mapping y acquisition tienen capas diferentes. | **KEEP**; clarificar fixture vs production initialization. |
| asset reports/manifests | importer provenance, scene manifest y asset integrity report describen artefactos distintos. | **KEEP** separados; formalizar ownership/version fields. |

No se justifica ARCHIVE/REMOVE en esta auditoría; la única futura eliminación posible es duplicación comprobada tras migración/usages review. No hay múltiples save models de gameplay porque no hay uno.

## 22. Reusable Systems

- **Conservar:** Composer model, component/property/transform contracts, timeline/evaluator/clips/sequencer, canvas/history/validators, preview de autoría, project persistence, deterministic C++ export, asset physical indexing/packager, C++ ScenePlayer, runtime asset manager, Renderer2D/input, battle Command/Event/Phase shapes y native parity harness.
- **Reutilizar con boundary changes:** AppShell + screen lifecycle como frontend temporario; DataManager/CanonicalModels/SourceRepository/Importer/Manifest como scaffolding domain imports; WaveManager como adapter de tests/vertical slice; Pokémon state como material de análisis para nuevo state canonical.
- **Fixtures:** mantener en `test/fixtures`; no promover su slice a catálogo runtime.

## 23. Systems to Refactor (later, evidence-based)

1. Delimitar fallback data init de producción y asegurar estado de origen/version.
2. Separar flow/run domain de AppShell/screens y sacar hardcoded wave presentation.
3. Consolidar status/provenance de sprites con AssetIndex físico.
4. Canonicalize importer output con extension preservation/versioned schema/overrides.
5. Convertir SceneModel exporter inputs de literals a slots/bindings mediante compat migrations.
6. Hacer el battle prototype reemplazable/adaptable por interface, preservando APIs temporales.

No iniciar hasta que upstream/domain contracts y tests de compatibilidad estén definidos.

## 24. Systems to Archive

Ningún subsistema debe archivarse inmediatamente. Posibles demos de WaveManager/escenas de Pikachu podrían archivarse como ejemplos/fixtures si se reemplazan; first prove no runtime/test/product references and preserve docs/license/provenance.

## 25. Systems to Remove Later

Ningún código se identifica como eliminable con evidencia suficiente hoy. Después de migración: reglas fake de oleadas/boss/rewards, synthetic importer baselines en production path, resolver de asset literals y alias serializer pueden retirarse sólo con reference audit, migration plan, passing tests/goldens y user-facing compatibility. La auditoría no ejecutó borrados.

## 26. Systems to Port from Upstream

Portar/adaptar, no copiar grandes bloques TypeScript/Phaser:

- startup/title/continue/new-run semantics, mode selection, run state and save/version/migration behavior;
- mode rules, Classic/Endless/Daily/Spliced Endless/Challenge differences and wave progression;
- Pokémon species/forms/stats/nature/IV/EV/runtime state, moves, abilities, held/passive modifiers/items;
- encounter pools/rules, wild/trainer selection, bosses, rarity, events/mystery encounters and seed usage;
- biomes, map/routes/nodes/transitions, trainers, rewards/shop/economy;
- transformation and form-change state/effects;
- visible HUD/message/menu semantics and localization namespaces;
- relevant asset metadata/animations/sprites/audio plus license/provenance and 3DS conversion.

Each capability needs a table mapping upstream source symbol/data → canonical schema → 3DS capability/override → deterministic parity evidence. Semantic behavior comes from upstream; platform, memory/render/input/asset conversion are 3DS adaptations.

## 27. 3DS-Specific Systems

Keep as platform-owned: top 400×240/bottom 320×240 constraints, touch resistive and physical button focus, pixel alignment and layout, Citro2D renderer/export ABI, `.t3x`/RomFS conversion/index/load/cache, VRAM/RAM budgeting, audio format/device handling, build/link/3DSX and device validation. These implement constraints, not alternative PokéRogue mechanics.

## 28. Risks

1. Prototype mechanics may be mistaken for faithful game rules due comments/names and passing tests that only assert local behavior.
2. Synthetic importer fallback and canonical constructor defaults can silently resemble production records.
3. Split asset catalogs may advertise nonexistent paths or weak hashes while packager validates physical sources elsewhere.
4. Wave/biome/mode facts may be duplicated between gameplay UI, manager and future content.
5. Hardcoding `10` blocks multiple modes and grows hidden coupling.
6. Project save migrations could be confused with game save migrations.
7. Scene authoring previews may be treated as gameplay runtime parity despite distinct data sources.
8. Upstream is fast-moving; branch names are not pins. Asset/locales/game repo revisions may not correspond to one compatible release.
9. C++ target memory and supported data schema may constrain content-package design; no devkitARM install was required or run by this audit.
10. Dirty working tree contains unrelated source/test/build-artifact changes; mixing those into docs commit would overwrite/claim unrelated work.

## 29. Recommended Architecture

Establish explicit `SourceSnapshot → CanonicalContent → OverrideSet → RuntimeContent` versions; separate `GameModePolicy`, `RunState/GameFlow`, `Map/Biome/Wave/Encounter resolvers`, `Pokemon/Item/Transformation runtime state`, and `PresentationContext/BindingRegistry`. Engine APIs accept canonical resolved input and emit serializable events; UI/bindings consume read-only presentation state. Scene templates stay editor-authored data and native runtime consumes validated generated representation plus resolved content IDs. AssetIndex is authoritative for existence/provenance; specialization resolvers map domain identity to indexed variants. Saves and content packages carry distinct version/compatibility metadata.

Compatibility requirements: existing `project.json`, `screens/*.json`, and generated C++ contracts migrate explicitly; old projects open unchanged and new bindings fail validation clearly when unresolved. Do not synchronize live upstream at test time; pin snapshots and fixtures.

## 30. Recommended BETA-UI-8 Implementation Order (not performed)

| Phase | Scope | Exit evidence |
|---|---|---|
| A. Canonical data contract | source metadata, schema versions, raw extensions, overrides, explicit fixture/prod boundary | round-trip unknown fields; pinned import provenance; deterministic fixture tests |
| B. GameMode | mode definition/policy interfaces only, no new gameplay implementation | all required mode IDs express different capabilities without manager `if Classic` |
| C. Wave/Map/Biome | schemas and source adapters, graph and transitions | upstream source mapping, validation and compatibility fixtures |
| D. Encounter resolution | pools/rules/seed/context → resolved encounter | deterministic contextual selection + traceable provenance |
| E. Pokémon/item/transformation state | persistent/battle/wave/turn state and definitions/instances | source-to-field coverage and serialization/migration parity design |
| F. Battle port map | freeze local engine behavior, compare phases/effects, define compatible adapter API | reviewed coverage matrix and golden behavior cases per upstream rule |
| G. Presentation bindings | view model, slots, validation, web preview and C++ runtime data | dynamic species/biome/wave test across scene template without editing template |
| H. Classic vertical integration | integrate one bounded canonical mode path through content→flow→battle→reward→scene/runtime | source→import→data→test→result; no UI game data hardcodes |
| I. Endless/Daily/Challenge extension | apply independent mode policies and any no-map flow | mode-specific tests; no global wave cap assumptions |
| J. Content update + save compatibility | package/diff/sign/verify/install and versioned run saves | integrity/compat/migration/rollback gates on device-compatible storage |
| K. Upstream synchronization | scheduled/manual refresh process with human-reviewed diffs and revision update | reproducible import report, licenses/provenance, no unsupported fields lost |

Order may change only with dependency evidence. No feature phase was implemented by this report.

## 31. Upstream References

All project references below use the configured repository commit unless noted. Links are direct file paths at commit `8555c08c823b856cbec4eb99ca84ea52a955836d`, not mutable `beta` paths. This commit was also opened on GitHub for the run, Pokémon and save files. The audit identifies source families to port; detailed semantic implementation still needs symbol-level study per feature.

| Domain | Upstream file/source at pinned revision | Porting relevance |
|---|---|---|
| modes | [`src/enums/game-modes.ts`](https://github.com/pagefaultgames/pokerogue/blob/8555c08c823b856cbec4eb99ca84ea52a955836d/src/enums/game-modes.ts), [`src/system/game-data.ts`](https://github.com/pagefaultgames/pokerogue/blob/8555c08c823b856cbec4eb99ca84ea52a955836d/src/system/game-data.ts) (imports `getGameMode`) | canonical mode IDs and consumer; locate/verify `getGameMode` definition before porting policy |
| Daily | [`src/data/daily-seed/daily-run.ts`](https://github.com/pagefaultgames/pokerogue/blob/8555c08c823b856cbec4eb99ca84ea52a955836d/src/data/daily-seed/daily-run.ts) | configured biomes/forced waves/encounters and seeded daily context |
| Pokémon runtime | [`src/field/pokemon.ts`](https://github.com/pagefaultgames/pokerogue/blob/8555c08c823b856cbec4eb99ca84ea52a955836d/src/field/pokemon.ts), [`src/data/pokemon/pokemon-data.ts`](https://github.com/pagefaultgames/pokerogue/blob/8555c08c823b856cbec4eb99ca84ea52a955836d/src/data/pokemon/pokemon-data.ts) | layered state, form/tera/fusion/wave/turn/battle data; presentation sprite state remains separate |
| species/moves | [`src/data/balance/species/generation-01.ts`](https://github.com/pagefaultgames/pokerogue/blob/8555c08c823b856cbec4eb99ca84ea52a955836d/src/data/balance/species/generation-01.ts), [`src/data/moves/move.ts`](https://github.com/pagefaultgames/pokerogue/blob/8555c08c823b856cbec4eb99ca84ea52a955836d/src/data/moves/move.ts) | canonical balance definitions/source parser coverage |
| abilities | [`src/data/abilities/ab-attrs.ts`](https://github.com/pagefaultgames/pokerogue/blob/8555c08c823b856cbec4eb99ca84ea52a955836d/src/data/abilities/ab-attrs.ts), [`src/data/abilities/init-abilities.ts`](https://github.com/pagefaultgames/pokerogue/blob/8555c08c823b856cbec4eb99ca84ea52a955836d/src/data/abilities/init-abilities.ts) | declarative attributes and initialization; local regex trigger/effect mapping is not enough |
| items/modifiers | [`src/modifier/modifier-type.ts`](https://github.com/pagefaultgames/pokerogue/blob/8555c08c823b856cbec4eb99ca84ea52a955836d/src/modifier/modifier-type.ts), [`src/modifier/modifier.ts`](https://github.com/pagefaultgames/pokerogue/blob/8555c08c823b856cbec4eb99ca84ea52a955836d/src/modifier/modifier.ts) | item/modifier pools, instances/effects and held item behaviors |
| biome/encounters | source tree under `src/data` and `src/phases` (biome IDs, biome generation, encounter/trainer phase families) | locate exact source module paths and dependencies at pinned SHA before importing; avoid relying on guessed paths |
| battle phases | [`src/phases`](https://github.com/pagefaultgames/pokerogue/tree/8555c08c823b856cbec4eb99ca84ea52a955836d/src/phases), [`src/field/pokemon.ts`](https://github.com/pagefaultgames/pokerogue/blob/8555c08c823b856cbec4eb99ca84ea52a955836d/src/field/pokemon.ts) | authoritative phase ordering/state transitions and field semantics |
| save/version | [`src/system/game-data.ts`](https://github.com/pagefaultgames/pokerogue/blob/8555c08c823b856cbec4eb99ca84ea52a955836d/src/system/game-data.ts), [`src/system/version-converter.ts`](https://github.com/pagefaultgames/pokerogue/blob/8555c08c823b856cbec4eb99ca84ea52a955836d/src/system/version-converter.ts), [`src/types/save-data.ts`](https://github.com/pagefaultgames/pokerogue/blob/8555c08c823b856cbec4eb99ca84ea52a955836d/src/types/save-data.ts) | distinct system/session serialization, validation and migration |
| localization | `pagefaultgames/pokerogue-locales` revision `23aea1cb0da5a0b15b836f3c243791591cc42303`; paths loaded by `PokerogueRepository.loadLocaleFile(locale, namespace)` | names/messages for canonical IDs; import per namespace/locale with provenance |
| assets | `pagefaultgames/pokerogue-assets` revision `056a1f408f26a3be4fef243f7462cb43608c7928`; `images/pokemon/{id}.json` and source asset path family | real source metadata/dimensions and license; conversion/atlas binding remains 3DS-specific |
| official repository | [pagefaultgames/pokerogue](https://github.com/pagefaultgames/pokerogue) | canonical source; do not treat unpinned `beta` as reproducible input |

The project config proves pins and code consumers; only the GitHub files explicitly examined in this audit are the commit-pinned Daily, Pokémon, and game-save sources. Other rows identify upstream import/port targets and require path+symbol verification before implementation. No upstream code was copied.

## Verification / Git handoff

- `git diff --check`: run after report creation; the preexisting dirty CRLF-modified files are outside this audit and may produce their own whitespace diagnostics if checked globally.
- `npm test`: executed; 220 passed, 2 failed (details in final response/task record): `BETA-UI-8.19 Export validation` (`test/beta_ui_8_tests.js:568`, missing expected exception) and `BETA-UI-8.34 Preview/runtime parity` (`:822`, `cppNode is not defined`). No test repair was included.
- `npm run native-parity`: executed; 126/126 mathematical checks passed.
- No devkitARM local requirement was introduced.
- Only this report is intended for the BETA-UI-8 audit commit; other worktree modifications are unrelated/preexisting and must remain unstaged.

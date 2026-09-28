# GUI_3DS North Star

## Propósito

GUI_3DS es un Studio de autoría visual y una adaptación ejecutable de PokéRogue para Nintendo 3DS. El Scene/Animation Composer sirve para crear la presentación de esa adaptación; no define las reglas del juego ni convierte el producto en un editor genérico.

## Regla arquitectónica

```text
PokéRogue upstream
  → revisión fijada
  → importador y normalizador
  → modelo canónico + extensiones preservadas
  → overrides 3DS explícitos
  → resolvers de contenido y flujo de juego
  → bindings a SceneTemplates
  → runtime Nintendo 3DS (C++ / Citro2D / RomFS)
```

Mantener independientes:

- **Game data:** qué ocurre; datos de contenido, reglas y estado de run/batalla.
- **Presentation:** cómo se ve; templates, slots, bindings, timeline y assets.
- **Runtime:** cómo carga y ejecuta los datos en hardware 3DS.
- **Content update:** cómo se importa, empaqueta, verifica e instala contenido compatible.
- **Editor:** cómo se autoriza, previsualiza, valida y exporta presentación.

El flujo de dependencias es unidireccional. El modelo de combate no importa DOM ni UI. La UI emite comandos y proyecta eventos/estado mediante bindings; no muta `BattleState` directamente.

## CURRENT / TARGET / GAP

| Área | CURRENT observado | TARGET | GAP principal |
|---|---|---|---|
| GameFlow | `AppShell` y screens independientes implementan título/setup/wave intro/battle/result/summary; `WaveManager` orquesta un slice | Run state machine extensible que modela boot, continue/new, setup, starter, mapa/nodo, biome, wave, encounter/battle/event, reward y cierre | Falta modelo único de flow/run, persistencia y routing por modo; pantallas y manager llevan la progresión |
| Modes | No hay `GameMode` canónico/integrado; comportamiento es esencialmente un demo Classic de 10 waves | Estrategias y `mode.rules` independientes para Classic, Endless, Daily, Spliced Endless y Challenge | Sin configuración, resolvers ni contratos de políticas por modo |
| Classic | `WaveManager` crea encounters alternados y boss/mini-boss codificados | Datos reutilizables de waves, biomes, encounters, routes, rewards y reglas | No existe catálogo upstream de contenido Classic; el slice no representa las 200 waves documentadas por upstream |
| Endless | Sin política Endless | Progresión sin límite de capacidad del engine; reglas biome/encounter/boss propias del modo | Sin progresión ni reglas Endless integradas |
| Map | Sin grafo runtime de mapa | `MapDefinition`, `MapNode`, `MapEdge` y transición según modo | No hay modelos ni resolver de rutas/nodos |
| Biomes | No hay `BiomeDefinition`; backgrounds son assets de escenas/demo | Catálogo de biomas extensible con locale, visual/audio, pools, transiciones, metadata y provenance | Sin modelo ni catálogo enlazado al run/encounter |
| Waves | `WaveManager` fija `totalWaves=10`, clamp y especies/niveles/rewards por fórmula | `WaveDefinition` declarativa que referencia modo, biome, tipo/pool encounter, trainer/boss/event/reward, binding y reglas | Modelo demo no es portable ni data-driven; el número 10 está dentro del manager/UI |
| Encounters | El manager fija IDs Pikachu/Golem; no existe pool/rule resolver | `EncounterPool/Rule/Definition` y `ResolvedEncounter` determinado por modo, wave, biome, seed y modificadores | No hay pipeline de resolución canónico |
| Pokémon State | `PokemonBattleData` tiene species, stats, HP, moves, ability, status y stages | Runtime state completo y separado: forma, moves/abilities, items held/passive, modificadores, transformación y estado visual | Estado parcial; cálculo e inicialización prototipo, un solo Pokémon por lado |
| Items | Hay `ItemDefinition` básico; sin instancia/resolver o integración en run/battle/UI | Definición + instancia + resolver común para held/passive/consumibles/rewards/icons | No existe catálogo/importer de items ni lifecycle integrado |
| Transformations | `PokemonSpriteNode` expone form/shiny/facing básicos; no hay estado de transformación del gameplay | `TransformationState` extensible y resolver de reglas/forma/sprite/escala/efectos/UI | Sin modelo ni binding/transformation resolver |
| Battle | Command/Event/Phase y `BattleSession` DOM-free; `BattleEngine` calcula mecánicas simplificadas | Port/adaptación de semántica upstream a representación canónica y fases 3DS | Engine documentado como prototipo; no se ha probado paridad de reglas completas; no tratarlo como canonical |
| Scenes | `SceneModel`, editor/timeline, preview, validación y generación C++; escenas JSON authoring | `SceneTemplate` de gameplay con slots y bindings a estado resuelto | No hay slot schema/resolver integrado con flow/game data; escenas actuales son layouts fijos |
| Bindings | `PokemonSpriteResolver` y `AssetResolver` resuelven assets; no binding general de datos de run | bindings tipados Biome/Encounter/Event/Boss/Transformation/Species/Item → resolvers/templates | Falta capa runtime de bindings y validación referencial de gameplay |
| Content Pipeline | `PokerogueSource/Repository/Importer/Adapter`, `CanonicalModels`, manifest, locale/enum parsers; species/moves/abilities vertical slice | import → normalize → preservar desconocidos → canonical → overrides → generated data, con provenance completa | No es ingesta completa del dominio; parser parcial de TS, fallbacks sintéticos mezclados con producción, campos desconocidos/schema/upstream override incompletos; provenance de asset y modelos no uniforme |
| Updates | Sin content package/update/install pipeline | Manifest/package/entry versionados, diff, hashes, firma y verificación/instalación compatible; contenido simple no exige rebuild de runtime | No hay paquete, firma, mecanismo de distribución ni compatibilidad instalada |
| Save | `ProjectDocument` persiste documentos del editor; no save de run ni migraciones de juego | saveVersion/contentVersion/runtimeCompatibility explícitos y migraciones de run | Persistencia del editor no equivale a save gameplay |

### Sistemas reutilizables existentes

- Authoring: `ProjectModel`, `ProjectDocument`, `SceneModel`, `UINode`/componentes, `TimelineUI`, `TimelineEvaluator`, `HistoryManager`, `SceneValidator` y `ProjectValidator`.
- Export: `CodeGenerator`, `SceneCppExporter`, `AssetPackager`, `BatchExporter` y manifests generados.
- Native: `ScenePlayer`, `RuntimeAssetManager`, `Renderer2D`, `InputManager` y escenas C++.
- Content/asset: `PokerogueSource`, `PokerogueRepository`, `PokerogueImporter`, `PokerogueAdapter`, modelos canónicos parciales, locale/enum importer, `PokerogueManifest`, `AssetIndex`, resolvers.
- Battle bridge: Commands/Events/Phases, `BattleState`, `BattleEngine`, `BattleSession` y harness de native parity. Conservar como prototipo/porting target hasta contrastar con upstream.

### Duplicación y límites

- Editor scenes y game screens (`public/js/screens/*`) son dos conceptos cercanos con contratos distintos; actualmente las pantallas gameplay están implementadas en DOM y no consumen plantillas del Composer.
- `WaveManager`, `BattleScreen` y `WaveIntroScreen` comparten el conocimiento de wave/encounter y además fijan el total `10`; esta progresión de demo no debe evolucionar como la arquitectura final.
- `PokemonSpriteNode`, `PokemonBattleData` y `SpeciesDefinition` guardan aspectos relacionados, pero pertenecen a presentación de autoría, estado de runtime y contenido respectivamente. No consolidarlos en un sprite node.
- `DataManager` hidrata fallback vertical slice en su constructor y `PokerogueImporter` puede sintetizar source offline; distinguir ese baseline de datos importados de producción.

### Sistemas a portar/estudiar desde upstream

Semántica de modos y progresión, waves/biome transitions, pools y selección de encounters/trainers/bosses/events, reglas de batalla/fases/efectos, entidades y estados de Pokémon, ítems/modificadores, transformaciones, recompensas, game/save flow y textos/assets pertinentes. Portar comportamiento observado mediante adaptadores; no pegar código Phaser/TypeScript al runtime ni reimplementar por intuición.

### Sistemas que seguirán siendo específicos de 3DS

Límites de memoria/VRAM y formatos de assets; resolución y dual screen; input de botones/táctil; renderer Citro2D; gestión/caché de RomFS; layout responsive a 400×240 y 320×240; perfilado, empaquetado y restricciones de build/devkitARM.

## Reglas de integración

1. Investigar upstream fijado y guardar revision/sourcePath/hash/schemaVersion en cada import.
2. Los fixtures de `test/fixtures` nunca son catálogo de producción ni sustituyen silenciosamente al DataManager.
3. Conservar metadatos upstream desconocidos en extensiones seguras; registrar pérdida deliberada con diagnóstico.
4. Aplicar cambios 3DS como overrides rastreables, nunca sobreescribir upstream canónico.
5. Generación determinista y límites de hardware son requisitos del runtime.
6. No borrar/refactorizar sistemas durante la auditoría. Implementaciones futuras deben respetar compatibilidad de `project.json` y `screens/*.json`.

## Documentos normativos

- [Especificación maestra](docs/POKEROGUE_3DS_MASTER_SPEC.md)
- [Contenido y actualizaciones](docs/POKEROGUE_3DS_CONTENT_UPDATE_SPEC.md)
- [Game flow y modos](docs/POKEROGUE_3DS_GAME_FLOW_SPEC.md)
- [Presentación y bindings](docs/POKEROGUE_3DS_PRESENTATION_SPEC.md)

Estos documentos fijan arquitectura y gaps; no autorizan implementar features automáticamente. La auditoría se realizó sobre commit `175497409a6640f3bde830977f8125f70ada34ec`; los cambios locales preexistentes se enumeran en el reporte de entrega y no forman parte de esa base.

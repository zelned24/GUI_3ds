# Repository Semantic Brain Map

The graph at [`semantic-brain-map.json`](semantic-brain-map.json) is the compact index for future repository work. It stores node metadata, source fragments, typed relationships, context profiles and reviewed boundary violations; it never stores source bodies.

## Query a relevant subgraph

Before a cross-file change, query the task area and read only the returned nodes, their source fragments, direct dependencies/consumers, tests, upstream references and linked docs:

```bash
node scripts/semantic_brain_map_query.mjs GameMode --depth=1
node scripts/semantic_brain_map_query.mjs GameFlow --depth=1
node scripts/semantic_brain_map_query.mjs PresentationBindings --depth=1
node scripts/semantic_brain_map_query.mjs "modify biome progression" --depth=0
```

The output starts with a `contextBudget` divided into primary, secondary and reference material. Named profiles are preferred; other queries rank node IDs, symbols, responsibilities and tags. Traversal expands only from primary nodes and returns only edges/violations inside the selected subgraph. Increase depth or limit only when the task requires more context.

For a GameMode task, the current profile selects:

- **Primary:** GameModeDefinition, GameModePolicy, GameModeRegistry and CanonicalContent.
- **Secondary:** importer, repository and BETA-UI-8 tests.
- **Reference:** pinned upstream enum, mode implementation, save consumer and current contract documentation.

This profile intentionally excludes SceneModel, CanvasRenderer, Timeline and native render code.

For the task **“add a new species”**, the profile follows:

```text
SpeciesId / initGenerationOne…Nine
  → SpeciesImporter / FormImporter
  → Normalizer
  → CanonicalContent
  → RuntimeContent
  → asset metadata / AssetIndex boundary
  → LocaleImporter
  → BETA-UI-8C integration test
```

The `add a new species` query selects only the species/form importer, canonical envelope, repository, locale resolver, asset index boundary, pinned species/assets/locales sources and their integration test. It deliberately excludes Timeline, CanvasRenderer, Scene Composer, Citro2D export and BattleEngine because those are not dependencies of adding a declarative species record.

The BETA-UI-8C `modify biome progression` profile returns `BiomeDefinition`, `GameModePolicy`, `WaveDefinition`, `RouteDefinition`, the explicitly planned `BiomeResolver` boundary, canonical registry/import/repository, BETA-UI-8C tests and pinned upstream biome/type/progression references. `--depth=0` keeps the profile boundary: because GameModePolicy is also consumed by unrelated GameFlow and presentation-binding targets, expanding graph neighbors would include context outside this domain task. The query therefore excludes Timeline, CanvasRenderer, audio and native exporter/runtime nodes unless a task explicitly asks to trace into presentation or runtime.

The BETA-UI-9A `change how first wave enemy is resolved` profile returns `EncounterResolver`, `FirstRunFlow`, mode/wave/biome contracts, the production canonical+progression import seam, pinned progression source references and the real-content integration test. It deliberately excludes TimelineUI, CanvasRenderer, AssetPackager and Citro2D exporter; the UI/presentation slot is downstream and does not own encounter selection.

The `build native C++ first-run content screen` profile follows the pinned canonical importer into the compact generated ROM header, then through `FirstRunRuntime` to the existing native `ScenePlayer`. It includes canonical species/locales and presentation contracts without pulling in editor Timeline or battle-rule implementations.

The **“add canonical battle data to native runtime”** profile follows pinned species and move declarations through the normalizers into compact C++ tables. It includes base stats, canonical ability IDs, level-up learnset ranges, move scalar fields, source provenance, the generator's cross-reference validation and its compile-time Bulbasaur/Tackle vectors. Raw upstream effect expressions stay in canonical JSON; this table does not claim move-effect or battle execution support yet.

The **“initialize native Pokémon battle state”** profile follows the generated canonical species/move tables into `PokemonBattleState`. The initializer implements only the pinned base stat formula from explicit instance inputs and validates cross-references without consuming RNG or inventing defaults. Imported form rows retain per-form stats, types, abilities and provenance; an explicit matching form selects those stats, while species without a declared form use canonical species stats. Its result is explicitly `base formula only`; upstream instance generation, EVs/modifiers and battle actions remain unsupported. The profile includes its C++/WASM regression harness and pinned `Pokemon.calculateStats` source.

## Graph model

Layers are PRODUCT → DOMAIN → PRESENTATION → RUNTIME → SOURCE → TEST, with documentation cross-links. A node identifies a semantic fragment (contract/class, method group, test group, fixture, source symbol, or documentation responsibility) by stable ID, type, source location, responsibility, status and tags. An edge is a typed relation such as `IMPORTS`, `CALLS`, `PRODUCES`, `CONSUMES`, `SERIALIZES`, `VALIDATES`, `TESTS`, `MAPS_TO`, `ORIGINATES_FROM`, `DEPENDS_ON` or `RUNTIME_USED_BY`.

The graph is hybrid: code dependencies connect to product/domain docs, tests, fixtures, pinned upstream symbols and runtime/presentation layers. `violations` records only observed problematic/current boundaries with severity, evidence and a recommended owner boundary; gaps that are not violations stay as planned nodes.

## Maintenance and determinism

Keep JSON UTF-8, two-space formatted, and stable: sort nodes by `id`, edges by `from/type/to`, and violations by `id`. Update relevant nodes/edges/profile/violations in the same change that changes architecture. Include symbol or line-fragment metadata for code nodes when practical. Use paths and symbol references only; never embed whole source files, generated assets, timestamps, or machine-specific locations.

Validate graph/query changes through the BETA-UI-8 test group. The query script has no LLM, network, embedding, or repository-crawl dependency; its cost is bounded by the selected semantic subgraph, rather than reading the full repository.

For the **import upstream biome encounter pools** task, the focused profile traces `plainsBiome.pokemonPool` and `plainsBiome.trainerPool` at the pinned upstream source through `PokerogueImporter.parseBiomes`, `BiomeDefinition`, `CanonicalContent`, and the real import integration test. The profile deliberately does not imply that the first-run resolver already applies upstream tier/wave weighting or PRNG behavior.

The first encounter task follows `Arena.randomSpecies` through tier selection, pool member choice, rarity retry, then `getWildSpeciesForLevel` into native `PokerogueEncounterResolver`. Its canonical C++ catalog now contains 488 ordered, provenance-carrying evolution edges. The Old 3DS resolver applies upstream forced-prevolution thresholds and wild level-based evolution selection to the wave-1 candidate using the same encounter RNG stream. A focused context profile retrieves the resolver, generated edge table, first-run consumer, pinned source, and WASM tests without loading presentation or battle code. The double-battle check, second battle-scoped level, and second sequential wild species resolution now follow `Battle` construction and `EncounterPhase.start`; the double vector checks both resolved species and draw ordering. Trainers/bosses and battle creation/execution remain unsupported, so this is not a playable battle yet.

The **“port Pokerogue Phaser RNG to Old 3DS C++”** profile follows `PokerogueRngAdapter`, `PokerogueWaveClock`, the native `PokerogueEncounterResolver`, and the BETA-UI-9C WebAssembly golden-vector test. The first Classic wave's double-check, pool tier/member draw order, and post-pool wild evolution decisions are compared to an independent Phaser reference and canonical content. The profile excludes renderer, Scene Composer, and battle effects until a later task crosses those layers.

The **“import species growth rarity and base total for wild encounter rules”** profile connects the pinned generation importer to typed `SpeciesDefinition` metadata, canonical serialization, compact generated C++ fields and pinned import tests. Rarity is tri-state so an absent source field cannot silently become `false`; evolution edges preserve raw source, thresholds, order and provenance, and feed the native wild-species resolver.

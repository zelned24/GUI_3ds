# BETA-UI-8C — Wave, Biome, Map and Route Contracts

## Ownership and pipeline

`WaveDefinition`, `BiomeDefinition`, `MapDefinition`, `MapNode`, `MapEdge`, and `RouteDefinition` are declarative domain content. They do not execute encounters, battles, rewards, flow transitions, scene rendering, or presentation bindings. `ProgressionContentRegistry` validates their collections inside the existing `CanonicalContent` / `RuntimeContent` pipeline; it is not a second content manager.

```text
Pinned source → PokerogueRepository → PokerogueImporter
  → CanonicalContent.collections.{waves,biomes,maps,routes}
  → RuntimeContent → ProgressionContentRegistry
  → future resolvers / GameModePolicy consumers
```

Game modes and wave/map definitions refer to mode IDs. Biomes are reusable, mode-neutral content. There is no dependency from these contracts to DOM, `SceneModel`, `CanvasRenderer`, `TimelineUI`, sprite nodes, or Citro2D.

## Contracts

- **WaveDefinition** stores a stable content ID, optional positive `waveNumber`, `modeId`, `biomeId`, encounter type/pool references, trainer/boss/event/reward references, scene-binding data, special rules, metadata, provenance, version and extensions. All references are declarative. No one-file-per-wave layout is required.
- **BiomeDefinition** stores identity, a localization key, optional visual-template/background/music identifiers, encounter/trainer pool data, route references, transitions, metadata, provenance, version and extensions. It contains no renderer or scene object and places no fixed limit on biome count.
- **MapDefinition** is a directed graph of `MapNode` and `MapEdge`, optionally associated with a mode. Nodes carry type, optional position/payload/connections, metadata, provenance and extensions. Edges carry `from`, `to`, optional conditions/weight/metadata/extensions. Validation rejects duplicate node IDs and dangling connections/edge endpoints. Cycles are allowed; no DAG assumption is imposed.
- **RouteDefinition** represents an explicit source-to-target relation. For the upstream biome graph, each `Biome.biomeLinks` entry becomes one directed route; an upstream tuple weight is preserved. Route records do not execute selection.

## GameMode and content integration

`WaveDefinition.modeId` and `MapDefinition.modeId` reference existing `GameModeDefinition.id` values; their consumers can obtain the read-only `GameModePolicy` from `GameModeRegistry`. This avoids a circular import or putting policies inside biome records. No inferred map capability is written into a mode. In the pinned source, mode configs do not declare a map capability, and BETA-UI-8B correctly leaves it unknown.

The importer uses existing `CanonicalContent`, `Provenance`, `SourceSnapshot`, `PokerogueManifest`, and `RuntimeContent`. The biome import populates `biomes` and `routes`; `waves` and `maps` are present as empty collections until a genuine source or explicit local authoring pipeline supplies them. The registry loads these four collections from either canonical or runtime content and checks wave-to-biome and biome-to-route references when the referenced collection is present.

Every imported biome and route carries the pinned repository, full revision, `sourcePath`, source symbol, SHA-256 content hash, source type and schema/content version through the existing `Provenance`. The full source file is retained under each biome's `extensions.upstreamRawRecord`; each route also retains the raw `biomeLinks` expression. Pool declarations that this phase does not normalize therefore remain recoverable, and are not silently promoted into executable behavior.

## Upstream audit and semantic mapping

Reviewed against PokéRogue revision `8555c08c823b856cbec4eb99ca84ea52a955836d`:

| Upstream source / symbol | Observed semantics | Canonical target |
|---|---|---|
| `src/@types/biomes.ts` / `Biome`, `BiomeLinks` | A biome has IDs, encounter/trainer pools, trainer chance, weighted weather/terrain, optional BGM and outgoing links. A link is a biome ID or `(biome ID, weight)`. | `BiomeDefinition`, `RouteDefinition`; unnormalized source detail stays in extensions. |
| `src/enums/biome-id.ts` / `BiomeId` | Numeric biome identities; the catalog is not a fixed-size gameplay bound. | `BiomeDefinition.id` plus preserved `upstreamId`. |
| `src/init/init-biomes.ts` / `rawAllBiomes` | Runtime registration references independently defined biome records. | Repository discovers source paths from the pinned registry and imports records. |
| `src/data/balance/biomes/*.ts` / `*Biome`, `biomeLinks` | Per-biome declarative pools, presentation-adjacent BGM identifier and outgoing biome transitions. | `BiomeDefinition`, `RouteDefinition`; pools/raw fields remain preserved. |
| `pokerogue-locales@23aea1cb0da5a0b15b836f3c243791591cc42303` / `en/biomes.json` | Localized display values keyed by biome slug/camelCase ID. | Verified `{ namespace, key }` localization reference plus repository/revision/path/hash metadata. |
| `src/phases/select-biome-phase.ts` / `SelectBiomePhase` | Mode and progression context affect biome changes; ordinary next-biome choices follow the current biome's links, with special mode branches. This is executable phase logic, not a declarative wave/map catalog. | Source reference for a future resolver; no phase logic is copied. |
| `src/game-mode.ts` / `GameMode.isWaveFinal`, `getGameMode` | Per-mode terminal checks and mode flags; no catalog of one `WaveDefinition` per wave or map graph is declared there. | `GameModePolicy` can be referenced by future wave/map consumers; current Wave/Map collections are not falsely imported. |

The audited revision has no separate node/edge map catalog. `BiomeLinks` is the observed directed transition graph, and its meaning is preserved as routes rather than relabeled as a player-selectable map. Classic can represent map/wave/biome content through these contracts; Endless can represent waves/biomes without requiring a map. Daily, Spliced Endless and Challenge remain compatible through `modeId` references. No gameplay policy is inferred from those compatibility properties.

## Fixture boundary and legacy WaveManager

Test-only contract records use `TEST_FIXTURE` provenance under `test/fixtures/`. They are never appended to the production upstream import. `WaveManager` remains the audited 10-wave vertical-slice compatibility layer; it is neither migrated nor deleted, and the canonical importer does not source data from it. The existing Semantic Brain Map records its current hardcoded progression violation.

## Determinism and validation

Definitions reuse `PokerogueManifest.stableStringify` and `computeHash`. Registry order and serialization are stable. Validation checks IDs, schemas, provenance, declared required extensions, wave numbers, map node uniqueness and all graph references. Graph cycles are explicitly permitted. Scene/project JSON schemas and generated C++ formats are unchanged.

The semantic query profile for `modify biome progression` returns the biome contract, `GameModePolicy`, WaveDefinition and RouteDefinition, canonical importer/repository, tests, pinned upstream biome sources and this document. No resolver implementation currently exists; the graph identifies `SelectBiomePhase` as source behavior and marks the future resolver boundary as planned. The profile excludes Timeline, CanvasRenderer, audio resolution and native exporters.

## Deferred

No encounter selection/resolution, battle, reward grant, complete Classic/Endless flow, mode-driven route choice, save migration, concrete map production content, or declarative upstream wave catalog is implemented. `WaveManager`, SceneModel, timelines and screens are untouched.

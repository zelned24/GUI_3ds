# BETA-UI-9A — First Playable PokéRogue Vertical Slice

This slice moves a new run from the title screen through upstream starter selection, canonical mode/run/biome/wave/encounter resolution and a data-bound battle presentation. It is a stepping stone in the full PokéRogue-to-3DS migration, not a claim that the game is fully ported.

## Production pipeline

`PokerogueImporter.importPlayableCanonicalContent()` combines the existing pinned species/forms/moves/abilities/items/locales/modes import with the separate pinned biome/route import. It reads the default starting biome from `GameMode.getStartingBiome()` in `src/game-mode.ts` at the configured upstream revision (Town for Classic), merges that provenance into the snapshot, and emits one `CanonicalContent` and `RuntimeContent`. `BattleScene.launchBattle()` initializes only the provisional Plains background. `scripts/import_pokerogue_content.mjs` packages this deterministic snapshot and report locally. `AppShell.beginNewGame()` loads those packaged artifacts through `DataManager.loadCanonicalProductionSnapshot()`, which verifies the content hash and rejects non-UPSTREAM records before constructing `FirstRunFlow`; gameplay startup does not fetch upstream source files.

The full starter catalog is discovered from pinned species records' upstream `starter: SpeciesId.*` declarations. Fresh-profile choices come from upstream `defaultStarterSpecies` in `src/constants.ts`, with its source revision and hash preserved in the content snapshot. Localized names resolve from canonical locale entries. The form and asset reference travel in `ResolvedPokemon`; asset metadata is not represented as a physically installed 3DS texture.

## Old 3DS C++ runtime

`scripts/generate_3ds_runtime_content.mjs` turns the same pinned canonical snapshot into `project/generated/include/content/PokerogueRuntimeContent.hpp`. The generated ROM data keeps compact catalogs for species, forms, moves, abilities, items, modes, biomes, routes and localized names, with per-record source paths/symbols/hashes and pinned repository revisions. It rejects fixture provenance. Raw TypeScript and the full diagnostic import records stay in the development checkout; the console does not download source files or parse the 17 MB canonical JSON.

`project/src/game/FirstRunRuntime.cpp` consumes this generated C++ data, restricts a fresh native profile to upstream `defaultStarterSpecies`, resolves the Town encounter, and builds a data-bound `SceneDefinition` for the existing `ScenePlayer`. Starter and wild actors carry source-ordered identity/form/nature/tera inputs, canonical base stats, imported movesets and PP. The screen now accepts D-pad/A move commands for a bounded single-opponent path: enemy `SMART_RANDOM` only admits plain single-target damaging moves without attributes/flags, and other moves/doubles fail closed. Standard damage rolls update HP/PP and surface fainting. Save schema v2 can capture and replay this wave-1 single-battle checkpoint between turns, including portable export/import. This remains a partial battle port: abilities, status/field effects, full effective stats, capture/switch/run, rewards and wave progression are not implemented. Sprite decoding/conversion also remains pending. Validation of the newly added command/save paths is deferred until the final migration test/build pass.

## Current slice boundaries

- Mode selection consumes imported `GameModePolicy`; Classic's upstream `maxWave` is the terminal bound. Wave number is run state, not an invented per-wave catalog. Upstream does not declare a `WaveDefinition` catalog at the pinned revision.
- The default initial biome is parsed from upstream `GameMode.getStartingBiome()` (Town for Classic) and must resolve in imported biome content/locales. The `launchBattle` Plains background is only a visual bootstrap. The first node is a `MapNode` projection of the biome and its real imported outgoing routes. There is no fabricated map graph; upstream's pinned content has no map catalog.
- The active `EncounterResolver.resolve()` run path remains a deterministic first-slice adapter over the sorted imported species catalog. Its policy is explicitly marked `MINIMAL_SEEDED_UPSTREAM_CATALOG_SELECTION`; it does **not** claim parity with upstream encounter generation. `EncounterResolver.selectPoolMember()` now ports the pinned Arena tier thresholds, time-of-day pool concatenation, empty-tier downgrade and member-index selection when given upstream random draws, but the run path does not call it yet. Boss determination, RNG state/call order, luck/daily tier overrides, legendary retries and level-driven species replacement still need implementation and golden parity tests before the catalog preview can be replaced.
- The existing `BattleScreen` presents resolved canonical names, levels, initial HP/status, biome, wave, money and command menu. The native C++ command path is separate from `WaveManager` and `BattleEngine`; it is an upstream-bounded partial turn resolver and does not claim full battle parity.
- The same scene-node template in native `FirstRunRuntime` binds different upstream species/run contexts; the JS `SceneTemplate` test independently checks the web preview seam. Scene Composer remains the presentation layer and is not coupled to canonical data.
- Upstream asset metadata is carried to presentation, while physical files and 3DS conversions remain separately verified/pending. No guessed sprite paths are emitted by this flow.

## Determinism and test

`test/beta_ui_9a_tests.js` runs the pinned importer and exercises production content through starter selection, Classic policy, upstream starting biome/routes, seeded encounter, real species/form/locale/asset metadata, `PresentationContext` and one reused immutable `SceneTemplate`. An independent test asserts fixture content is rejected. Importing content is network/cache backed; no test fixture becomes production data.

Context query:

```bash
node scripts/semantic_brain_map_query.mjs "change how first wave enemy is resolved" --depth=0
```

The profile selects the encounter/run resolver, wave/biome/mode contracts, canonical import and relevant upstream references/tests. It excludes Timeline, CanvasRenderer, AssetPackager and Citro2D export.

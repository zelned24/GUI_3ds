# BETA-UI-8B — GameMode Contract

## Ownership

`GameModeDefinition` describes one selectable ruleset. `GameModePolicy` is a read-only domain query view over that definition. `GameModeRegistry` validates and resolves definitions imported as `CanonicalContent.collections.gameModes`. A future `GameFlow` owns run transitions and asks the selected policy for capabilities; a mode definition does not execute lifecycle steps.

```text
Pinned PokéRogue sources
  → PokerogueRepository
  → PokerogueImporter.parseGameModes()
  → CanonicalContent.collections.gameModes
  → GameModeRegistry
  → GameModePolicy
  → future GameFlow / resolvers
```

This phase does not connect the contract to `AppShell`, screens, `WaveManager`, battle, or scene presentation.

## Definition and policies

Each definition records `id`, upstream numeric ID where available, localization key, `schemaVersion`, `contentVersion`, `rules.maxWave`, capabilities, policy groups, context requirements, special rules, provenance, and upstream extensions/raw excerpts. Unknown capability values are represented by `null`, distinct from an explicit `false`.

Capabilities are data, not scattered mode-name checks. Current names cover map support, shop, trainer/wild battles, bosses/events/Mystery Encounters, mode family flags, and biome behavior. The registry returns `true`, `false`, or `null` from `supports()`. This three-state contract prevents absent source information from turning into an invented rule.

Policies are grouped under `map`, `encounters`, `progression`, `shop`, `starter`, `reward`, and `save`. Only fields observed in the imported upstream declarations are populated in this phase. Empty policy groups are extension points, not defaults for future gameplay.

`rules.maxWave` is a content rule, optional per mode. It is imported only when upstream `isWaveFinal()` declares a fixed terminal wave. Endless modes therefore have `maxWave: null`; periodic boss milestones are not misrepresented as a global engine maximum. Runtime capacity is not bounded by this value.

## Canonical content and provenance

The importer reads `src/enums/game-modes.ts` and `src/game-mode.ts` at the pinned revision from `PokerogueSource.js`. It writes the source revision and raw source hashes into the existing `CanonicalContent` envelope and each mode’s `Provenance`. `GameModeRegistry.loadCanonicalContent()` validates the envelope records before registration. Runtime 3DS overrides remain separate through the BETA-UI-8A `RuntimeContent` pipeline.

## Upstream mapping audited

At revision `8555c08c823b856cbec4eb99ca84ea52a955836d`:

| Upstream source / symbol | Observed meaning | Canonical projection |
|---|---|---|
| `src/enums/game-modes.ts` / `GameModes` | Stable enum identities: Classic, Endless, Spliced Endless, Daily, Challenge | Slug `id` plus preserved numeric `upstreamId` |
| `src/game-mode.ts` / `getGameMode` | Per-mode flags: trainers, Mystery Encounters, short biomes, random bosses, daily/challenge/spliced family | Capabilities and grouped encounter/progression policies |
| `src/game-mode.ts` / `isWaveFinal` | Fixed terminal wave for Classic/Challenge and Daily; periodic terminal/boss milestones for Endless families | `maxWave` only for fixed terminal modes; periodic behavior is not an engine maximum |
| `src/game-mode.ts` / `getName`, `trySetCustomDailyConfig` | Localization keys and daily custom-seed configuration | `displayNameKey`; Daily seed context requirement |
| `src/system/game-data.ts` / `getSessionSaveData`, `initSessionFromData`, `offlineNewClear` | Session save stores mode ID and seed, load resolves the mode, Daily clears are tracked by seed | Recorded as source mapping for future save/flow integration; this phase does not change saves |

No upstream map policy was found in these mode configuration symbols. `map` stays `null` for imported modes. The contract supports explicit true/false map capabilities, demonstrated with test-only definitions.

## Validation and determinism

Validation checks ID shape, required localization key, supported schema, positive safe integer `maxWave` when present, known capability names/types, provenance and declared required extensions. Registry rejects invalid and duplicate IDs. Mode and registry serialization/hash reuse `PokerogueManifest` stable serialization; there is no second hashing implementation.

Tests exercise real pinned-source import, all five mode identities, policy differences, unknown map semantics, test fixture map values, canonical registry loading, mode/registry determinism, and separation from renderer/editor modules.

## Boundaries deferred

No Classic/Endless/Daily/Challenge progression, map, wave, biome, encounter, reward, starter, battle, or save behavior is implemented. `WaveManager` remains a 10-wave prototype and `AppShell` remains the current screen shell. BETA-UI-8C will define Wave/Biome/Map contracts that can consume `GameModePolicy` without creating cycles.

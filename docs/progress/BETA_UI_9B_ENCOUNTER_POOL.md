# BETA-UI-9B — Upstream biome pool selection stage

## Upstream findings

Inspected the pinned `src/field/arena.ts` symbols `Arena.randomSpecies`, `generateBossBiomeTier`, `generateNonBossBiomeTier`, `updatePoolsForTimeOfDay` and `getTimeOfDay`; also inspected `src/enums/biome-pool-tier.ts` and `src/battle-scene.ts:getEncounterBossSegments`.

`Arena.updatePoolsForTimeOfDay()` builds each live tier pool by concatenating `TimeOfDay.ALL` members first and current-time members second. Non-boss tier rolls use `[0,512)` with Common at 156–511, Uncommon at 32–155, Rare at 6–31, Super Rare at 1–5, and Ultra Rare at 0. Boss rolls use `[0,64)` with Boss at 20–63, Boss Rare at 6–19, Boss Super Rare at 1–5, and Boss Ultra Rare at 0. Empty tiers downgrade one `BiomePoolTier` ordinal at a time until a pool exists. A selected member is then picked from the pool. If Common is empty, upstream falls back to the global catchable species catalog.

After pool selection, upstream may reroll incompatible legendary-like species (up to ten attempts) and replace the result using `PokemonSpecies.getWildSpeciesForLevel`. Boss eligibility also depends on `BattleScene.getEncounterBossSegments`, mode policy and wave. These are separate rules and are not represented as complete encounter parity here.

## Implemented

- `EncounterResolver.selectPoolMember()` implements the pinned non-boss/boss tier thresholds, `ALL + time` ordered membership, tier downgrade order, member-index bounds and explicit failure for the global-catalog fallback case.
- The method requires actual integer draws from its caller. It deliberately does not manufacture a PRNG or claim seed equivalence.
- The playable importer now pins and hashes the Arena rules source and biome-tier enum in `SourceSnapshot`; canonical extensions record their symbols and the limited coverage.
- The C++ generator emits deterministic pool arrays with source member indices and tier/time-specific source symbols, retaining upstream list order for a later native consumer.
- Tests exercise real Plains data from the pinned importer and every tier boundary. The existing preview path remains clearly labeled as catalog-based and does not call this selector.

## Validation / limits

- Focused BETA-UI-9A integration: 3/3 PASS, including all non-boss/boss threshold boundaries, member bounds, production fixture rejection and generated C++ pool rows.
- Full `npm test`: 228 passed, 6 failed (four native parity/build harness cases cannot find `clang-wasm`; failures `BETA-UI-8.19` missing expected exception and `BETA-UI-8.34` undefined `cppNode`). No failures were hidden or weakened.
- `npm run native-parity`: blocked by missing `clang-wasm`.
- Real Old 3DS `npm run 3ds-build`: PASS; generated 426,164-byte `.3dsx` with devkitARM/Citro2D.
- ARM compiler syntax-check of the 1,749-entry/200-entry pool header and `FirstRunRuntime.cpp`: PASS. C++ does not yet execute this selector.
- Pinned content hash with Arena and tier-enum rule-source provenance: `f2bf26d4b2fcdc385ff69fb4ad979a926041e2c435b98adc45a7408444f4ec37`; deterministic reimport PASS.
- `git diff --check`: PASS.
- Seed-to-draw equivalence is not established. `Arena.randomSpecies()` uses the game RNG, mode/day context, boss check, luck, daily forced tiers and post-selection species rules. The current method is only the exact pool-tier/member-selection stage when supplied those decisions and draws.

## Next

1. Port Phaser's pinned RNG and PokéRogue battle-seed/turn state handling, including seed offsets and call order, with upstream golden vectors.
2. Port Classic boss eligibility, wave/time-of-day calculation, luck, forced tiers, legend-like rerolls and level species substitution.
3. Connect the verified resolver stage to both JS run flow and Old 3DS C++ only after required state and RNG are present; no catalog fallback in production.

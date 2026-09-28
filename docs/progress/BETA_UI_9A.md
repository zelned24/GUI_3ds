# BETA-UI-9A Progress — upstream biome pools into canonical and C++ content

## Current result

This update closes one identified content gap in the first playable slice: the pinned biome source files already carried real `pokemonPool` and `trainerPool` declarations in their preserved raw text, while the canonical `BiomeDefinition` discarded them. `PokerogueImporter.parseBiomes()` now normalizes the declared tiers and time-of-day keys into `encounterPools` and `trainerPools`. The importer fails if a required pool declaration or its tiers cannot be recognized. Pool records retain their parent biome provenance (pinned repository, revision, source path, symbol and hash); unrecognized source syntax remains available in `extensions.upstreamRawRecord`.

The generated Old 3DS C++ content header now includes deterministic, flattened Pokémon and trainer pool tables, with each row carrying biome/tier/time-or-trainer IDs and source provenance. Pool rows are sorted independently of filesystem order. This makes real biome pool data available to the native content package; the current C++ first-run preview does not yet use it to choose an enemy.

## Evidence

- PokéRogue pin: `8555c08c823b856cbec4eb99ca84ea52a955836d`.
- Locales pin: `23aea1cb0da5a0b15b836f3c243791591cc42303`.
- Assets pin: `056a1f408f26a3be4fef243f7462cb43608c7928`.
- Inspected upstream record: `src/data/balance/biomes/plains.ts`, symbols `plainsBiome.pokemonPool` and `plainsBiome.trainerPool`.
- Plains import test checks actual pinned Common Dawn/All species and trainer IDs, plus upstream provenance.
- Content report hash after reimport: `d682fcb47f7a279b02eb2f74f767001940ad596da0568a7ab66c4bae495e4f77`; importer reported deterministic reimport and a complete reference audit.
- Catalog counts: 5 modes, 1,084 species, 609 forms, 708 moves, 320 abilities, 35 items/modifier definitions, 2,704 locales, 35 biomes and 66 routes.
- Generated header contains 1,749 Pokémon biome-pool membership rows and 200 trainer pool rows.

## Deliberate limits / remaining work

- The first-run JS and C++ enemy preview still selects from the full species catalog; it does not yet restrict candidates to the current biome's pool.
- Upstream tier choice, wave-specific boss/rare rules, time-of-day, trainer chance/selection, and exact seeded RNG sequence are not ported or parity-tested. Do not call the current preview an upstream encounter result.
- Native `BattleEngine`/`BattleSession`, turns, move effects, ability triggers, modifier effects, saves and map/gameplay state are not migrated in this slice.
- Complete pinned species snapshots validate all 1,749 biome species-pool references against canonical species IDs and fail clearly on missing IDs. Deliberately filtered generation imports carry `PARTIAL_SPECIES_SNAPSHOT_UNVERIFIED` with a sample/count instead of misclassifying omitted generations as invalid upstream IDs.
- `npm test`: 228 passed, 6 failed. Four failures are native parity/build harness cases that cannot find `clang-wasm` in this checkout; two known failures remain (`BETA-UI-8.19` missing expected exception and `BETA-UI-8.34` undefined `cppNode`). No assertions were changed.
- `npm run native-parity`: blocked by the missing `clang-wasm` executable.
- `npm run 3ds-build`: PASS with real devkitARM/Citro2D; produced a 426,164-byte `.3dsx` for Old 3DS. `FirstRunRuntime.cpp` compiled against the expanded generated content header.
- Focused BETA-UI-9A integration tests: 3/3 PASS after adding partial-snapshot labeling and generated pool-table assertions.
- `git diff --check`: PASS after removing a stray escape from the semantic map note.
- Physical sprite decoding/conversion and broad asset index coverage remain pending; current report verifies source metadata for only 3 of 1,084 species.

## Next implementation order

1. Validate every imported biome species-pool ID against canonical species IDs and classify malformed references as import errors.
2. Add an encounter adapter that consumes the selected biome pools while explicitly labeling unsupported tier/time/RNG semantics.
3. Inspect pinned upstream pool selection, wave finality, trainer choice and PRNG call order; implement and test a parity slice before making fidelity claims.
4. Teach native runtime consumers to resolve canonical pool IDs, forms and locale records without embedding PokéRogue gameplay code.
5. Continue the Classic run through explicit commands/events and replace the prototype bridge incrementally; keep scene composition separate.

## Decision

**Partial progress is publishable; BETA-UI-9A is not complete.** Real pinned biome pools now travel through import, canonical storage, full-snapshot reference validation and deterministic C++ generation. The native first-run preview remains a setup/encounter-preview slice, not a playable Classic run and not a faithful upstream encounter resolver.

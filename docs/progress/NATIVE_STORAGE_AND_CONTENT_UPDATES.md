# Native storage and content update integration

## Implemented state, pending validation

The C++ runtime now connects X (save), Y (save and export), L (load), and R
(import exported save and load) to `NativeRunSaveStore`. Startup attempts to
restore the latest valid journal. The supported schema explicitly records
`RunSetup`: seed, canonical starter, Classic mode, starting biome, wave 1,
runtime/schema versions, and the exact canonical content hash. The encounter is
recomputed from the pinned seed and content. Schema v3 also records the current
single-battle checkpoint: encounter species, battle turn, both HP values,
canonical move IDs, PP, starter level and total experience, and whether the
encounter is active/won/lost. Version-one RunSetup and version-two wave-one
battle saves decode with level-five starting experience and upgrade on the
next write. The validator still rejects waves after wave one until the runtime
can restore full party/reward state; the schema expansion does not claim
multi-wave save support.

The runtime replays the encounter from its seed before restoring the checkpoint,
then verifies the generated encounter, move slots, HP bounds and PP bounds. The
save point is between turns: battle RNG is deterministically re-seeded from the
pinned battle seed and saved turn number, so mid-turn RNG state is not accepted
or serialized. The checkpoint still omits party changes, status/stat stages,
items, rewards and waves after wave 1.

Two SD journal slots preserve the previous complete save when writing the
inactive slot is interrupted. The envelope checksum covers the version and
generation as well as the payload. Loading a newer incompatible valid slot
returns an incompatibility error instead of silently reverting progress.
The backend flushes and synchronizes writes, and the store reads back the file.

Export destination: `sdmc:/3ds/pokerogue/exports/progress.p3save`. This is a native
portable UTF-8 snapshot for another compatible 3DS build. It is not a PokéRogue
web account save. Failed export does not modify the journal. Import validates
the checksum, schema, content hash, stage and canonical references before saving.

## Classic biome route import

The native content generator now emits each pinned upstream `RouteDefinition`
as a compact `PokerogueContent::Route` row with canonical source/destination
IDs, the optional exclusion weight, and file/symbol/hash provenance. Route
conditions the native runtime does not understand fail generation explicitly;
they are not discarded. `routesFrom()` exposes the records to the future native
`SelectBiomePhase` adapter. The inspected upstream source is
`src/phases/select-biome-phase.ts::SelectBiomePhase.start`: it filters weighted
links through `randSeedInt`, chooses among the remaining links, and sends
Classic to the End biome before the final boss segment. The standalone
`resolveClassicNextBiome` C++ adapter ports those deterministic selection rules
and accepts an externally selected destination when a Map Modifier offers a
choice. It returns explicit unsupported/missing-route errors. The resolver is
not yet called by the run/battle flow; Map Modifier ownership, biome switching,
rewards, and multi-wave run transitions remain disconnected.

## Content updates under construction

`ContentUpdateStore` provides a bounded binary manifest parser, signature
verification callback, streamed file integrity verification, content-addressed
release directories and two activation records. A release is activated only
after its assets have been downloaded and checked. Signature verification is
mandatory; no verifier means rejection. SHA-256 alone is not authenticity.

This module is **not an available OTA feature yet**. The HTTPS backend, trusted
publisher key, release packager, UI action, and runtime asset-loader connection
remain unimplemented. Its current manifest represents presentation `.t3x`
assets paired with a catalog hash. Gameplay catalogs are still compiled into
the binary; updating species/moves without reinstalling requires a versioned
runtime data reader. Do not describe the current module as complete content
updating or enable an unsigned fallback.

## Required proof before release

- Run save journal corruption/interruption/export/import regressions.
- Compile and link storage against devkitARM/libctru.
- Exercise real SD save/load/export on Old 3DS and interrupted writes.
- Complete and test signed manifest, compatibility, size, rollback and network
  failure handling before exposing installation.
- Extend the save schema alongside party/reward/unlock/multi-wave progression.
- Validate full Classic victory/defeat and summary independently of these
  infrastructure changes; those gameplay requirements remain unfinished.

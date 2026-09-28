# BETA-UI-9C — Phaser RNG adapter for Old 3DS

## Self-audit

- **Current:** the biome pool selector accepts externally supplied draws, while JS and C++ first-run paths still use local catalog-selection PRNGs. The C++ adapter remains deliberately unconnected pending the exact wave-1 call-order port.
- **Target:** reproduce the pinned Phaser 3.90.0 random stream and PokéRogue's seed-offset mechanics in a small C++ boundary suitable for the 3DS runtime.
- **Gap:** encounter call order, wave-level generation, double-battle decision, biome/time calculation, legendary rerolls and species substitution are not yet integrated. Startup trace confirms Classic begins at Town (unless `STARTING_BIOME_OVERRIDE` is active); Plains in `BattleScene.launchBattle()` is only a provisional background.
- **Sources:** PokéRogue `8555c08c823b856cbec4eb99ca84ea52a955836d`; Phaser `v3.90.0`.
- **Risk:** C++ output could appear deterministic while disagreeing with upstream due to UTF-16 seed shifts, two-draw `frac()`, range boundaries, singleton short-circuiting or stream restoration.
- **Proof:** fixed values derived from Phaser's pinned `sow/hash/rnd/frac/integerInRange/pick/state` source are checked against the actual C++ adapter compiled to WebAssembly; the same header is syntax-checked with devkitARM.

## Implemented

`project/include/game/PokerogueRngAdapter.hpp` implements the Phaser Alea state, seed hashing over UTF-16 code units, two-step `frac()`, range/pick semantics, serializable `c/s0/s1/s2` state, PokéRogue-style `shiftCharCodes`, and a scoped seed-offset save/sow/restore boundary. Singleton picks and ranges of size 1 do not consume random draws; empty pool selection is rejected.

Golden seed `pokerogue-rng-v1` has these checked outputs:

| Case | Expected |
|---|---:|
| First `frac()` | `0.743767629869303` |
| Root seed `randSeedInt(512)` | `380` |
| Wave seed `shiftCharCodes(seed, 1)`, `randSeedInt(512)` | `42` |
| Offset seed `shiftCharCodes(seed, 4)`, `randSeedInt(100)` | `92` |
| Offset 0 `randSeedInt(8)` | `5` |

Additional C++ assertions cover state restoration, singleton/range no-draw behavior, empty-pool rejection and UTF-16 wraparound.

## Validation

- Focused RNG adapter WebAssembly test: **PASS**.
- devkitARM C++ syntax check of the adapter harness: **PASS**.
- `npm test` after correcting the two invalid test setups without weakening their assertions: **235 passed, 0 blocked, 0 failed**. Test 8.19 now mutates the sanitized document into the invalid persisted state it intends to validate; test 8.34 compares against the computed C++ evaluation node.
- `npm run native-parity`: **126/126 PASS**.
- `npm run 3ds-build`: **PASS**, real ELF and 3DSX produced. The latest biome-label runtime tweak is being rebuilt separately.

## Explicit limits

This is an RNG adapter, **not encounter parity**. It is not yet called by `EncounterResolver` or `FirstRunRuntime`; first-run C++ still chooses an enemy from the global species table with xorshift. In particular, first Classic wave also depends on seed-derived time-of-day, the double-battle draw, level draws inside the battle-constructor offset, boss checks, pool/tier/member draws, party luck, and post-selection legend/evolution rules. Those call sites and their ordering must be ported and tested before reporting a real seeded encounter.

`npm ci` restored the exact locked dev dependency and removed the earlier `clang-wasm` environment block. The two existing assertions are reported as baseline failures, not bypassed.

## Next

1. Startup trace completed: `TitlePhase.end()` selects Classic, `GameMode.getStartingBiome()` returns Town, then `SelectStarterPhase` establishes wave 1 and `Arena.init()` prepares the pool before `EncounterPhase`.
2. Port wave-cycle offset, current turn-free encounter stream calls, level seed offset, double check, time-of-day and Classic wave-1 pool resolution in upstream order.
3. Import required species rarity/BST/evolution data or fail the relevant result explicitly; do not treat absent fields as negatives.
4. Replace the JS and Old 3DS catalog preview only when the same fixed-seed golden trace matches upstream at every decision point.

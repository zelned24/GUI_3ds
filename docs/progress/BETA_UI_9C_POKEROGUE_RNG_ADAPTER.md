# BETA-UI-9C — Phaser RNG adapter for Old 3DS

## Self-audit

- **Current:** the native C++ first-run path uses pinned Phaser RNG, Classic wave clock, Town pool selection, non-boss level generation, LegendLike/BST rerolls and wild species evolution/pre-evolution substitution. It presents a resolved species/level candidate; actual battle execution remains incomplete.
- **Target:** reproduce the pinned Phaser 3.90.0 random stream and PokéRogue's seed-offset mechanics in a small C++ boundary suitable for the 3DS runtime.
- **Gap:** trainer/boss selection and battle execution are not yet integrated. Startup trace confirms Classic begins at Town (unless `STARTING_BIOME_OVERRIDE` is active); Plains in `BattleScene.launchBattle()` is only a provisional background.
- **Sources:** PokéRogue `8555c08c823b856cbec4eb99ca84ea52a955836d`; Phaser `v3.90.0`.
- **Risk:** C++ output could appear deterministic while disagreeing with upstream due to UTF-16 seed shifts, two-draw `frac()`, range boundaries, singleton short-circuiting or stream restoration.
- **Proof:** fixed values derived from Phaser's pinned `sow/hash/rnd/frac/integerInRange/pick/state` source are checked against the actual C++ adapter compiled to WebAssembly; the same header is syntax-checked with devkitARM.

## Implemented

`project/include/game/PokerogueRngAdapter.hpp` implements the Phaser Alea state, seed hashing over UTF-16 code units, two-step `frac()`, range/pick semantics, serializable `c/s0/s1/s2` state, PokéRogue-style `shiftCharCodes`, and a scoped seed-offset save/sow/restore boundary. Singleton picks and ranges of size 1 do not consume random draws; empty pool selection is rejected.

`PokerogueWaveClock` also ports the Classic cycle clock: derive `waveCycleOffset = randSeedInt(8) * 5` from a fresh root-seed stream, then map `(waveIndex + offset) % 40` to day `[0,15)`, dusk `[15,20)`, night `[20,35)`, dawn `[35,40)`. Golden coverage checks the pinned vector's offset 25 and every transition/wrap boundary.

Golden seed `pokerogue-rng-v1` has these checked outputs:

| Case | Expected |
|---|---:|
| First `frac()` | `0.743767629869303` |
| Root seed `randSeedInt(512)` | `380` |
| Wave seed `shiftCharCodes(seed, 1)`, `randSeedInt(512)` | `42` |
| Offset seed `shiftCharCodes(seed, 4)`, `randSeedInt(100)` | `92` |
| Offset 0 `randSeedInt(8)` | `5` |

Additional C++ assertions cover state restoration, singleton/range no-draw behavior, empty-pool rejection and UTF-16 wraparound. The same WASM test compares wave-1 time, double check, pool tier/member, actual generated Town candidate, battle-scoped level draws, and a pinned Plains seed whose first LegendLike candidate is rerolled to the next real pool member.

## Validation

- Focused RNG adapter WebAssembly test: **PASS**.
- devkitARM C++ syntax check of the adapter harness: **PASS**.
- `npm test`: **235 passed, 0 blocked, 0 failed**.
- `npm run native-parity`: **126/126 PASS**.
- `npm run 3ds-build`: **PASS**, real ELF and 3DSX produced with the current first-run runtime.

## Explicit limits

This is still **not full encounter parity**. C++ resolves a real biome-pool species, applies the pinned rarity/BST retry and wild evolution/pre-evolution substitution; 488 canonical evolution edges are embedded in the runtime content table. Tests compare forced-prevolution and level-evolution cases to the pinned semantics. Wave-1 doubles now resolve both sequential enemy slots and levels with independent pinned comparison. Trainer/boss selection and executable battle creation remain incomplete.

`npm ci` restored the exact locked dev dependency and removed the earlier `clang-wasm` environment block. The two existing assertions are reported as baseline failures, not bypassed.

## Next

1. Startup trace completed: `TitlePhase.end()` selects Classic, `GameMode.getStartingBiome()` returns Town, then `SelectStarterPhase` establishes wave 1 and `Arena.init()` prepares the pool before `EncounterPhase`.
2. Port real trainer/boss species and trainer pool selection for Classic.
3. Connect the resolved encounter to a battle state/phase implementation. Do not label the current pool candidate as a resolved battle encounter.

## Current fidelity correction — deferred validation

Reviewing the pinned `BattleScene.newBattle()` call order showed that its battle constructor seed is not just `rootSeed + (waveIndex << 3)`: `resetSeed(waveIndex)` first produces `waveSeed = rootSeed + waveIndex`, then `executeWithSeedOffset` applies `waveIndex << 3` to that wave seed. `Battle` creates its seeded 16-character `battleSeed` before generating wild levels, and `Battle.randSeedInt()` re-sows that battle seed with `turn << 6` after `incrementTurn()` resets its saved state.

The native level resolver and BETA-UI-9C reference vectors now model this composed seed. `PokerogueBattleRng` owns the separate battle-seed/per-turn stream, while the encounter RNG remains on the wave-reset seed. These source and expected-vector changes have not been compiled or run; all final validation remains deferred per the active migration instruction.

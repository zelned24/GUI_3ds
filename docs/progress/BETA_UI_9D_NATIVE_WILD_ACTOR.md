# BETA-UI-9D — Native wild actor construction progress

## Current result

`generatePokemonActorForWildEncounter()` composes the existing canonical C++ helpers for a non-fused wild actor: ability index, 32-bit actor ID and derived IVs, gender, selected canonical form, nature, and the initial tera-type selection. The function rejects missing species/forms and missing primary type data instead of fabricating an actor. `FirstRunRuntime` calls the helper for the first wild enemy.

Double encounter selection is explicitly withheld. Upstream calls `EnemyPokemon.generateAndPopulateMoveset()` inside the first enemy's construction, before `EncounterPhase` resolves the next species. GUI_3DS does not yet port that move generator, so calculating a second enemy now would use the wrong RNG state.

The canonical learnset importer and generated C++ table now preserve PokéRogue's signed level markers (`EVOLVE_MOVE = 0`, `RELEARN_MOVE = -1`) instead of coercing them to level 1. The current pinned generation files contain no such literal tuples, so this is schema/runtime readiness rather than evidence that the present snapshot has rows with those markers. `SpeciesLevelMove::level` is signed to retain them if upstream introduces them.

This is an actor-construction slice. It does not create a battle-ready starter or enemy `PokemonBattleState`, does not select moves, and does not implement a turn or advance a Classic run.

## Evidence

- Pinned PokéRogue revision: `8555c08c823b856cbec4eb99ca84ea52a955836d`.
- Upstream order inspected in `src/field/pokemon.ts` (`Pokemon` and `EnemyPokemon` constructors/`generateAndPopulateMoveset`) and `src/phases/encounter-phase.ts` (sequential enemy resolution/construction).
- `test/native/pokemon_battle_state_harness.cpp` compares the composed wild-actor helper to the same pinned stages applied individually and asserts the final Alea state.
- `make -f Makefile.3ds`: devkitARM/Citro2D compile, ELF link, and 3DSX generation pass.
- `npm run native-parity`: 126/126 pass. This gate covers scene math and is separate from the actor harness.
- `npm test`: 237/237 pass after learnset sentinel preservation; pinned content reimport was deterministic at hash `0f7fc83f706dcb97d7ce98c28591d2d85a3821a0ed8b27a99947398bea717420`.

## Remaining gaps

- Player starter actor creation and move generation.
- Enemy move generation from the pinned `src/ai/ai-moveset-gen.ts` generator; required before resolving second slots in doubles.
- The moveset generator consumes generated evolution/relearner move sources and move-effect attributes; carrying signed learnset markers alone does not implement that selection or its RNG draws.
- Shiny and variant state, modifiers/fusion, and complete Pokémon stat initialization.
- Battle commands/phases, battle result, rewards, save/load, map/node progression, and Classic completion/defeat summary.
- Hardware execution and Old 3DS memory/frame-time measurements.

## Next implementation target

Trace pinned `generateMoveset` and first-wave player/enemy call sites. Port the initial move list through imported learnsets and move definitions, initialize both actors into native battle state, and add a fixed-seed C++ golden before wiring a turn command into the 3DS input loop.

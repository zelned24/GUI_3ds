# BETA-UI-9D — Native wild actor construction progress

## Current result

`generatePokemonActorForWildEncounter()` composes the existing canonical C++ helpers for a non-fused wild actor: ability index, 32-bit actor ID and derived IVs, gender, selected canonical form, nature, and the initial tera-type selection. The function rejects missing species/forms and missing primary type data instead of fabricating an actor. `FirstRunRuntime` calls the helper for the first wild enemy.

Double encounter selection is explicitly withheld. Upstream calls `EnemyPokemon.generateAndPopulateMoveset()` inside the first enemy's construction, before `EncounterPhase` resolves the next species. GUI_3DS does not yet port that move generator, so calculating a second enemy now would use the wrong RNG state.

This is an actor-construction slice. It does not create a battle-ready starter or enemy `PokemonBattleState`, does not select moves, and does not implement a turn or advance a Classic run.

## Evidence

- Pinned PokéRogue revision: `8555c08c823b856cbec4eb99ca84ea52a955836d`.
- Upstream order inspected in `src/field/pokemon.ts` (`Pokemon` and `EnemyPokemon` constructors/`generateAndPopulateMoveset`) and `src/phases/encounter-phase.ts` (sequential enemy resolution/construction).
- `test/native/pokemon_battle_state_harness.cpp` compares the composed wild-actor helper to the same pinned stages applied individually and asserts the final Alea state.
- `make -f Makefile.3ds`: devkitARM/Citro2D compile, ELF link, and 3DSX generation pass.
- `npm run native-parity`: 126/126 pass. This gate covers scene math and is separate from the actor harness.

## Remaining gaps

- Player starter actor creation and move generation.
- Enemy move generation from the pinned `src/ai/ai-moveset-gen.ts` generator; required before resolving second slots in doubles.
- Shiny and variant state, modifiers/fusion, and complete Pokémon stat initialization.
- Battle commands/phases, battle result, rewards, save/load, map/node progression, and Classic completion/defeat summary.
- Hardware execution and Old 3DS memory/frame-time measurements.

## Next implementation target

Trace pinned `generateMoveset` and first-wave player/enemy call sites. Port the initial move list through imported learnsets and move definitions, initialize both actors into native battle state, and add a fixed-seed C++ golden before wiring a turn command into the 3DS input loop.

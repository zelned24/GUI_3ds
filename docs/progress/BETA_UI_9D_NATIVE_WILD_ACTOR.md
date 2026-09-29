# BETA-UI-9D — Native wild actor construction progress

## Current result

`generatePokemonActorForWildEncounter()` composes the existing canonical C++ helpers for a non-fused wild actor: ability index, 32-bit actor ID and derived IVs, gender, selected canonical form, nature, and the initial tera-type selection. The function rejects missing species/forms and missing primary type data instead of fabricating an actor. `FirstRunRuntime` calls the helper for the first wild enemy.

Double encounter selection is explicitly withheld. Upstream calls `EnemyPokemon.generateAndPopulateMoveset()` inside the first enemy's construction, before `EncounterPhase` resolves the next species. GUI_3DS does not yet port that move generator, so calculating a second enemy now would use the wrong RNG state.

The canonical learnset importer and generated C++ table now preserve PokéRogue's signed level markers (`EVOLVE_MOVE = 0`, `RELEARN_MOVE = -1`) instead of coercing them to level 1. The pinned form learnsets use the symbolic `RELEARN_MOVE` constant; the importer normalizes it to `-1` while preserving the raw source record. `SpeciesLevelMove::level` is signed so native consumers can distinguish these markers from ordinary level-up moves.

Move definitions now also normalize `isUnimplemented` and attribute class names while retaining the original upstream expression. The generated move rows expose flags for `MoveIsUnimplemented` and `MoveHasSacrificialAttrOnHit`; this pinned snapshot has 86 unimplemented moves and two moves with `SacrificialAttrOnHit` (Memento and Final Gambit). These are declarative inputs for a future move generator and do not implement their effects or move selection.

Form learnsets from upstream `formLevelMoves` now travel through species parsing, canonical form records, and per-form ranges in the native learnset table. For example, the pinned Pikachu Gigantamax record carries Zippy Zap (Lv. 20), Floaty Fall (Lv. 30), Splishy Splash (Lv. 40), Pika Papow (Lv. 50), and Wild Charge (Lv. 55). `levelMovesFor(const Form&)` exposes that range without embedding species-specific rules in the runtime.

This is an actor-construction slice. It does not create a battle-ready starter or enemy `PokemonBattleState`, does not select moves, and does not implement a turn or advance a Classic run.

## Evidence

- Pinned PokéRogue revision: `8555c08c823b856cbec4eb99ca84ea52a955836d`.
- Upstream order inspected in `src/field/pokemon.ts` (`Pokemon` and `EnemyPokemon` constructors/`generateAndPopulateMoveset`) and `src/phases/encounter-phase.ts` (sequential enemy resolution/construction).
- `test/native/pokemon_battle_state_harness.cpp` compares the composed wild-actor helper to the same pinned stages applied individually and asserts the final Alea state.
- `make -f Makefile.3ds`: devkitARM/Citro2D compile, ELF link, and 3DSX generation pass.
- `npm run native-parity`: 126/126 pass. This gate covers scene math and is separate from the actor harness.
- `npm test`: 237/237 pass, including production move metadata and deterministic reimport at content hash `0590caff2b1959e9cf66d3ffd3966b172b2c86eb9b44aa7fd311f2fb96b9f8db`.
- `npm run 3ds-build`: devkitARM/Citro2D compilation, ELF link, and 3DSX generation pass with the expanded move records.
- The generated Pikachu Gigantamax form row points to five canonical form-level move records; form-specific moves remain data selected by `formKey`.

## Remaining gaps

- Player starter actor creation and move generation.
- Enemy move generation from the pinned `src/ai/ai-moveset-gen.ts` generator; required before resolving second slots in doubles.
- The moveset generator consumes generated evolution/relearner move sources and move-effect attributes; carrying signed learnset markers alone does not implement that selection or its RNG draws.
- The move record flags expose two move-generation filters only; other `MoveAttr` semantics remain preserved as raw upstream metadata and are not interpreted by the native runtime.
- Form-specific and prevolution learnset merging is not yet implemented in native code; the current work imports the source records and exposes compact ranges only.
- Shiny and variant state, modifiers/fusion, and complete Pokémon stat initialization.
- Battle commands/phases, battle result, rewards, save/load, map/node progression, and Classic completion/defeat summary.
- Hardware execution and Old 3DS memory/frame-time measurements.

## Next implementation target

Trace pinned `generateMoveset` and first-wave player/enemy call sites. Port the initial move list through imported learnsets and move definitions, initialize both actors into native battle state, and add a fixed-seed C++ golden before wiring a turn command into the 3DS input loop.

# BETA-UI-9D — Native wild actor construction progress

## Current result

`generatePokemonActorForWildEncounter()` composes the existing canonical C++ helpers for a non-fused wild actor: ability index, 32-bit actor ID and derived IVs, gender, selected canonical form, nature, and the initial tera-type selection. The function rejects missing species/forms and missing primary type data instead of fabricating an actor. `FirstRunRuntime` calls the helper for the first wild enemy.

The wave-1 double encounter path now resolves both enemy actors in upstream order. `FirstRunRuntime` finishes the first actor's constructor and regular-wild moveset draws before resolving the second level and species from the shared battle/wave streams, then generates the second actor and moveset. Unsupported metadata aborts the encounter instead of substituting a species or shifted random result. The existing native vector covers the two species/level stream order; a composed first-run harness case and final deferred validation are still needed.

The canonical learnset importer and generated C++ table now preserve PokéRogue's signed level markers (`EVOLVE_MOVE = 0`, `RELEARN_MOVE = -1`) instead of coercing them to level 1. The pinned form learnsets use the symbolic `RELEARN_MOVE` constant; the importer normalizes it to `-1` while preserving the raw source record. `SpeciesLevelMove::level` is signed so native consumers can distinguish these markers from ordinary level-up moves.

Move definitions now also normalize `isUnimplemented` and attribute class names while retaining the original upstream expression. The generated move rows expose flags for `MoveIsUnimplemented` and `MoveHasSacrificialAttrOnHit`; this pinned snapshot has 86 unimplemented moves and two moves with `SacrificialAttrOnHit` (Memento and Final Gambit). These are declarative inputs for a future move generator and do not implement their effects or move selection.

Form learnsets from upstream `formLevelMoves` now travel through species parsing, canonical form records, and per-form ranges in the native learnset table. For example, the pinned Pikachu Gigantamax record carries Zippy Zap (Lv. 20), Floaty Fall (Lv. 30), Splishy Splash (Lv. 40), Pika Papow (Lv. 50), and Wild Charge (Lv. 55). `levelMovesFor(const Form&)` exposes that range without embedding species-specific rules in the runtime.

The native `buildPokemonLevelMovePool()` now combines a species' imported level-up records with its selected form records, applies the current-level gate, removes duplicate/unimplemented/SacrificialAttrOnHit moves, and assigns pinned base weights (`level + 20`, 60 for EVOLVE_MOVE, and 50 for level-1 moves with at least 70 power). The candidate pool merges prevolution/evolution sources according to imported registry initialization, including current-species level-one exclusion and future-level locks. Both regular-wild slots now run the ability-aware move weighting, forced-STAB selection, and weighted draws in sequential constructor order, then retain canonical moves and starting PP.

This remains an actor-construction slice. The starter and up to two regular wild enemies now have canonical actor identities, base-stat battle states, and selected move slots. Enemy AI, turn ordering, complete damage/effects, battle results, rewards, and Classic progression remain unimplemented.

## Evidence

- Pinned PokéRogue revision: `8555c08c823b856cbec4eb99ca84ea52a955836d`.
- Upstream order inspected in `src/field/pokemon.ts` (`Pokemon` and `EnemyPokemon` constructors/`generateAndPopulateMoveset`) and `src/phases/encounter-phase.ts` (sequential enemy resolution/construction).
- `test/native/pokemon_battle_state_harness.cpp` compares the composed wild-actor helper to the same pinned stages applied individually and asserts the final Alea state.
- These are historical results from an earlier checkout; they do not validate the current uncommitted migration changes. Current compilation and tests are intentionally deferred until implementation is complete.
- The generated Pikachu Gigantamax form row points to five canonical form-level move records; form-specific moves remain data selected by `formKey`.

## Remaining gaps

- Starter actor creation and level-1-through-5 starter move selection are now connected for the fresh-profile path; complete profile modifiers, save migration and permanent party state remain open.
- Remaining player and non-regular-wild moveset generation details from pinned `src/ai/ai-moveset-gen.ts`; trainers and bosses are not connected.
- The moveset generator consumes generated evolution/relearner move sources and move-effect attributes; carrying signed learnset markers alone does not implement that selection or its RNG draws.
- The move record flags expose two move-generation filters only; other `MoveAttr` semantics remain preserved as raw upstream metadata and are not interpreted by the native runtime.
- Starter eligibility and the separate 27-entry fresh-profile starter set are now imported from pinned source; registry-derived prevolutions include 544 evolution edges.
- Shiny and variant state, modifiers/fusion, battle stat stages and effective-stat initialization.
- Battle commands/phases, battle result, rewards, full battle-progress save/load, map/node progression, and Classic completion/defeat summary.
- Hardware execution and Old 3DS memory/frame-time measurements.

## Next implementation target

Trace pinned `TurnInitPhase`/move-command ordering and wild AI. Add a command-driven first turn only after its target, battle RNG, supported move semantics and unsupported cases are explicitly modeled; then connect the result and its save state to the 3DS input loop.

## Current continuation (validation in progress)

The corrected snapshot contains 572 upstream starters; evolution-line membership no longer grants starter eligibility. The native selector offers only the pinned fresh-profile starters. Numeric registry initialization means Pikachu retains Pichu as prevolution after Pichu is visited, while Charizard inherits Charmeleon. Current deterministic content hash: `114b52091bf72e22adb933511d3a8354829dac9c3f743e5ac0cd21fb682abc28`, now including 2,284 upstream egg-move entries. Native save/export schema v2 now carries the wave-1 single-battle checkpoint between turns, while validation rejects starter-eligible species outside the fresh-profile set. Current test, compilation and hardware validation remain pending by request.

# Native moveset selection — connected first-wild slice

Source: `pagefaultgames/pokerogue` at
`8555c08c823b856cbec4eb99ca84ea52a955836d`.

`PokemonLevelMovePool.hpp` imports the pinned level-up and prevolution candidate
rules. `PokemonMoveEffectivePower.hpp` adapts the upstream effective-power and
expected-hit formulas. `PokemonMovesetWeights.hpp` implements regular-wild
damage weight adjustment using resolved power and actor stats. The picker keeps
the source's strict weighted-interval comparison, forced-STAB pass, and bounded
four-move selection.

`FirstRunRuntime` now resolves the first regular wild actor's canonical form,
actor identity, battle stats, and level-up pool, then derives each candidate's
power metadata from generated move/ability data and feeds the pinned RNG to the
weighted picker. The resolved move IDs are held on the runtime actor. No move
effect is inferred from the moveset catalog.

The ability adapter supports unconditional accuracy multipliers,
`MaxMultiHitAbAttr`, and `InstantChargeAbAttr`. Ability callbacks with additional
or conditional AI move-generation behavior are marked unsupported by generated
content and make encounter resolution fail explicitly before selecting moves.
Unknown callbacks are not silently treated as neutral. This is a deliberately
partial declarative adapter, not full ability behavior.

The first enemy slot is now connected. Classic double battles still report the
second slot as unsupported; no second actor or substitute moveset is fabricated.
This work does not yet connect the result to a complete player-command/battle
loop, reward progression, or all encounter variants.

The resolved first-slot `PokemonBattleState` now also receives the selected
move IDs through the canonical initializer, so its four `BattleMoveState`
records carry canonical maximum and starting PP. The earlier local state used
for move weighting is no longer the state retained by the run.

Test cases were added for real pinned Compound Eyes, Skill Link, conditional
Hustle and callback-based Drizzle metadata. A native harness path now assembles
canonical move candidates and generated battle stats for Bulbasaur and Pikachu,
generates two movesets from the same seed, and checks deterministic IDs/RNG
replay. Compilation and tests remain deferred until migration implementation is
complete, following the user's instruction. No hardware parity or complete
migration is claimed.

## Ability importer source preservation

The previous regular expression stopped at semicolons in nested `AiMovegenMoveStatsAbAttr` callbacks, truncating canonical raw metadata. `PokerogueImporter.parseAbilities` now reads balanced constructor arguments and their full chained builder expression. The native move-generation adapter recognizes only unconditional accuracy multipliers, max-multihit, and instant-charge metadata; conditional or unknown callback bodies are preserved canonically and marked unsupported for runtime selection.

Reimported the configured pinned revisions without changing them. The import tool ran the same import twice and reported identical content hash `114b52091bf72e22adb933511d3a8354829dac9c3f743e5ac0cd21fb682abc28`; catalog counts are 1,084 species, 609 forms, 2,284 egg-move entries, 920 moves, 320 abilities, 35 items, 2,704 locales, 5 modes, 35 biomes, and 66 routes. The pinned fresh-profile source is `src/constants.ts`; the egg-move source is `src/data/balance/moves/egg-moves.ts`. The importer output records the same hash for two runs. No compiler or test suite was run.

## Fresh-profile starter availability

The fresh native run selector now consumes the pinned `defaultStarterSpecies`
list from upstream `src/constants.ts`. `starterEligible` continues to describe
the full upstream starter catalog, while `freshProfileStarter` separately marks
species offered to a newly initialized profile. This prevents the selector from
mistaking all 572 catalog starters for the 27 starters available to a fresh
upstream profile. The source file hash is part of the canonical snapshot and
each native species row carries the derived fresh-profile flag. Persistent dex
unlock progression is still not implemented, so this restriction represents
the fresh-profile state rather than later unlocks.

Pinned source inspected for starter creation: `src/system/game-data.ts`
initializes default Dex and starter attributes;
`src/ui/utils/starter-select-ui-utils.ts` resolves starter defaults and
level-1-through-5 moves; `src/phases/select-starter-phase.ts` converts the
selected starter record into a runtime Pokémon. The native fresh-profile path
now models only those pinned defaults; persisted unlocks/preferences and the
full starter-selection UI remain future work.

`src/data/balance/moves/egg-moves.ts::speciesEggMoves` is now imported as a
separate pinned source and normalized into canonical species move IDs. The
generated C++ species table carries bounded egg-move ranges, and
`PokemonStarterMoveset.hpp` resolves the level-1-through-5 pool, unlocked egg
moves, compatible preferences, duplicate removal, and the four-slot cap. This
now feeds the selected fresh-profile starter's `ResolvedPokemon.moveIds` and
`moveCount` in `FirstRunRuntime`; the bottom screen resolves those canonical IDs
through English locale entries and displays the actual move names. The generated
species row preserves each starter's ordinal in `defaultStarterSpecies`, so the
native profile code reproduces the pinned isolated neutral-nature stream seeded
with `default`. It initializes profile IVs, first ability, default gender/form,
PID/Tera draws, canonical stats, and initial PP. Fresh profile uses zero
unlocked egg moves and no saved move preferences. Both first actors now expose
canonical HP on screen; this still does not execute a turn or advance Classic
progression.

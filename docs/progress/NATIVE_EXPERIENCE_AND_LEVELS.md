# Native PokéRogue experience data and progression formulas

## Imported source

The pinned production importer now requests `src/data/exp.ts`, parses and
validates the six upstream `expLevels` curves, and stores them with repository,
revision, source path, symbol, and SHA-256 provenance. Species records retain the
upstream `PokemonSpecies.baseExp` scalar from their raw source definitions.
Runtime generation requires all six 100-entry curves and rejects species with
missing or out-of-range `baseExp` instead of substituting a value.

## Native support

The importer and generator source are written to emit compact fixed-width
growth curves and a base experience field on each species row. The checked-in
canonical JSON and generated header have not been refreshed with those fields:
the prior import command was cancelled. `PokemonExperience.hpp` therefore
references generated symbols that are not present in the current header yet.

The defeated reward helper accepts the actual generated form row and applies
the upstream 1.5 `getBaseExp()` multiplier for Mega, Primal, Gigantamax, and
Eternamax form keys before applying `(baseExp * level) / 5 + 1`. The species
base record remains unchanged.

## Still required

`applyPokemonExperience()` now models cumulative XP, level advancement and
upstream cap clipping as a domain operation. It is still not connected to
battle-faint rewards or `PokemonBattleState`.
The native player still has no persisted cumulative experience, party state,
level-up move reminder/evolution flow, multi-wave checkpoint, or reward phase.
Tests and Old 3DS compilation are intentionally deferred to the final validation
pass per the current task instruction.

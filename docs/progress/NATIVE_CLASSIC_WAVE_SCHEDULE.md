# Pinned Classic wave schedule data

The production importer now reads `src/enums/fixed-boss-waves.ts` from the
pinned PokéRogue revision and preserves every enum symbol, wave number, source
path, source symbol, and SHA-256 in `CanonicalContent.extensions`. The C++
generator emits a compact table and lookup. The final wave is read from the
imported Classic `GameModeDefinition.rules.maxWave`, with its `game-mode.ts`
provenance, instead of being a local limit. The `isBoss` multiple-of-ten rule
is also exposed as source-derived Classic schedule data.

`PokerogueClassicWaveSchedule.hpp` distinguishes the final boss, fixed trainer
battles, regular ten-wave bosses, X1 guaranteed wild waves, and waves where an
arena trainer-chance roll is still required. This avoids treating every
unclassified wave as a regular wild encounter.

## Remaining integration

The canonical artifact and generated C++ header have not yet been refreshed.
`FirstRunRuntime` now checks the classification before creating the encounter:
wave 1 remains a regular wild candidate, while trainer-roll, fixed trainer,
major boss and final boss cases fail closed instead of being misresolved as
wild. Trainer chance, trainer pool resolution, boss team resolution, and the
Classic final-boss team are still absent. Compilation and tests remain deferred
until migration work is complete.

Pinned source: `pagefaultgames/pokerogue@8555c08c823b856cbec4eb99ca84ea52a955836d`.

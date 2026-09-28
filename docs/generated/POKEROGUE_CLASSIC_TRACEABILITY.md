# PokéRogue Classic Traceability

Pinned sources currently used by the Classic runtime slice:

| Subsystem | Upstream file | Symbol | Canonical model | GUI_3DS implementation | Test | Native status | 3DS status |
|---|---|---|---|---|---|---|---|
| Starting biome | `src/game-mode.ts` | `GameMode.getStartingBiome` | `GameModeDefinition`, `BiomeDefinition` | Generated runtime content / `FirstRunRuntime` | BETA-UI-9A first-run tests | Verified for Classic start | Built; hardware unverified |
| Wave RNG | `src/battle-scene.ts`, `src/utils/random.ts`, Phaser 3.90.0 `RandomDataGenerator.js` | `BattleScene.resetSeed`, `shiftCharCodes`, Phaser `sow/rnd/frac/integerInRange` | `PokerogueRngState` | `PokerogueRngAdapter`, `PokerogueSeedOffsetScope` | BETA-UI-9C WASM reference comparison | Verified tested vectors | Compiled; hardware unverified |
| Wave clock | `src/field/arena.ts` | `Arena.getTimeOfDay` | `PokerogueTimeOfDay` | `PokerogueWaveClock` | BETA-UI-9C boundary vectors | Verified | Compiled; hardware unverified |
| Non-boss pool candidate | `src/field/arena.ts` | `Arena.randomSpecies`, `generateNonBossBiomeTier`, `updatePoolsForTimeOfDay`, `checkLegendBST` | `BiomeDefinition.encounterPools`, species rarity/BST | `PokerogueEncounterResolver.resolveNonBoss` | BETA-UI-9C real Town pool and draw-order test | Wave-1 candidate and bounded rarity reroll path integrated; Town vector reroll count 0 | Compiled; hardware unverified |
| Wild level | `src/battle.ts`, `src/utils/common.ts` | `Battle.getLevelForWave`, `randSeedGaussForLevel`, `randSeedFloat` | Runtime level scalar | `PokerogueEncounterResolver.nonBossLevelForWave`; `FirstRunRuntime::resolve` supplies battle-scoped RNG after battleSeed initialization | BETA-UI-9C wave-1 independent golden | Wave-1 formula and draw count verified | Compiled; hardware unverified |
| Wild species evolution | `src/ai/ai-species-gen.ts`, `src/data/pokemon-species.ts` | `determineEnemySpecies`, `getRequiredPrevo`, `calcEvoChance`, `getWildSpeciesForLevel` | Ordered `SpeciesEvolutionDefinition` edges with threshold metadata | `PokerogueRuntimeContent::kSpeciesEvolutions`; `PokerogueEncounterResolver.resolveWildSpeciesForLevel` | BETA-UI-9C forced-prevo and level-evolution reference vectors | First-run post-pool substitution integrated; advanced form-specific ancestry remains outside this slice | Compiled; hardware unverified |

Source revision: `pagefaultgames/pokerogue@8555c08c823b856cbec4eb99ca84ea52a955836d`.

This slice applies pinned level-driven species substitution/evolution after the selected pool member. It does not claim complete encounter or battle parity: double-battle second slot, trainers, bosses, Mystery Encounters, battle creation and battle execution remain unsupported. It does not replace missing rules with a synthetic species.

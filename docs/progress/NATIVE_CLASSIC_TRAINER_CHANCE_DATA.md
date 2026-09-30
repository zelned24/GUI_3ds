# Native Classic Trainer Chance Data — Migration Progress

This step extends the existing canonical biome pipeline with PokéRogue's declarative trainer spawn denominator. It does not claim trainer battle generation is complete.

## Pinned upstream evidence

- Repository: `https://github.com/pagefaultgames/pokerogue`
- Revision: `8555c08c823b856cbec4eb99ca84ea52a955836d`
- Biome data: `src/data/balance/biomes/*.ts`, exported biome records, field `trainerChance`
- Runtime consumer inspected: `src/field/arena.ts`, `Arena.trainerChance`
- Schedule consumer inspected: `src/game-mode.ts`, `GameMode.isWaveTrainer`

`Arena.trainerChance` reads the active biome's denominator. `GameMode.isWaveTrainer` additionally evaluates gym/fixed battles around the current wave and uses wave-offset RNG for nearby past waves. Therefore this data-only change deliberately does not switch the runtime's trainer-chance waves into wild or trainer encounters.

The importer now also extracts the wave keys from `src/data/trainers/fixed-battle-configs.ts` (`classicFixedBattles`) and retains each raw declarative config fragment. The runtime generator validates those wave keys against the pinned `ClassicFixedBossWaves` enum and emits a provenance-carrying fixed-battle-wave index. `PokerogueClassicWaveSchedule.hpp` consumes this table and now has a Classic trainer-chance scheduler that mirrors the pinned nearby-gym/fixed-battle suppression and prior-wave seed-offset checks. Its caller must supply the live RNG at the exact upstream call point. This index and scheduler do not yet construct trainer data or start a trainer battle.

## Changes

- `BiomeDefinition` now retains nullable `trainerChance` and validates its canonical numeric domain.
- The pinned biome importer requires the literal upstream field, preserves the raw biome source, and adds an attribute-specific source symbol/hash reference.
- The 3DS runtime-content generator emits a sorted, provenance-checked `BiomeTrainerChance` table. It rejects absent, out-of-range, or mismatched provenance instead of substituting a default.
- The pinned content importer records Classic fixed-battle wave keys and their source fragments; the generator validates and emits a provenance-carrying index.

## Remaining before runtime use

- Connect the trainer scheduler at the verified upstream call point in battle creation, after its preceding random draws are ported.
- Import trainer declarations, teams, scaling, and localization before constructing trainer battles.
- Add parity coverage and run the full requested test/build gates only after the migration implementation is complete.

## Validation status

No import/generator execution, build, compilation, or tests were run in this step, per the requested final-only validation order.

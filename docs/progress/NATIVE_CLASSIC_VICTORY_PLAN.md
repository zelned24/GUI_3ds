# Native Classic victory phase contract

Pinned source: `pagefaultgames/pokerogue@8555c08c823b856cbec4eb99ca84ea52a955836d`.

Inspected symbols:

| Source | Symbol | Native contract |
|---|---|---|
| `src/phases/victory-phase.ts` | `VictoryPhase.start` | `planClassicVictory` preserves the ordered post-victory work and final-wave clear branch. |
| `src/battle-scene.ts` | `BattleScene.isNewBiome` | Classic requests biome selection after waves divisible by ten. |
| `src/battle-scene.ts` | `BattleScene.applyPartyExp` | The participating party and modifiers determine EXP before level-up. |
| `src/field/pokemon.ts` | `Pokemon.addExp` | Level and total EXP must persist across waves and respect the mode cap. |
| `src/phases/select-modifier-phase.ts` | `SelectModifierPhase.start` | Reward options depend on modifier pools and party state; choosing and skipping are gameplay commands. |

`project/include/game/PokerogueClassicVictoryPlan.hpp` now exposes an ordered
Classic phase plan. `FirstRunRuntime` stores this plan after a win and rebuilds
it when restoring a wave-one `BattleWon` checkpoint. The plan distinguishes
ordinary modifier selection, fixed rewards on every tenth wave, the wave-165
Lock Capsule reward, dynamic event rewards, biome selection, next battle and
wave-200 clear. It is a contract: no phase effect is marked executed by
creating the plan.

The current wave-one bridge still stops after victory. It does not yet award
EXP, generate modifier choices, apply rewards, advance the wave or save later
waves. `NativeRunSave` version 2 enforces `wave == 1`; advancing while retaining
that format would silently lose progression. The next implementation must
extend run state and save records together with party EXP/level/HP/PP, reward
selection, biome and RNG continuity, then consume the planned phases in order.
The 3DS program build and Azahar verification remain deferred by user request.

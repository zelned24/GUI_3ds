# BETA-UI-9F — First single-battle command path

## Implemented, validation deferred

`FirstRunRuntime` now exposes move selection and a battle command that is bound
to the existing native first-run screen. The D-pad selects among the starter's
canonical moves, A submits the selected move, and starter selection locks once
the run begins. HP and the selected move remain sourced from canonical runtime
content and the existing battle state.

The enemy move chooser ports the pinned regular-wild `SMART_RANDOM` progression
for the explicitly supported subset: usable, single-target (`NEAR_OTHER`),
damaging moves with no declarative attributes, no upstream behavior flags, and
no Fire-type constructor-added thaw behavior. It scores the current type chart,
base stats, power and STAB, preserves moveset order on tied scores, and applies
the pinned seeded advance loop. The one-opponent target-weight draw with weight
one short-circuits without consuming RNG, matching `randSeedInt(range <= 1)`.
The pinned turn command path enqueues the player field before the enemy field;
this one-on-one command path consequently resolves the player's move first.

Each submitted move passes through the existing standard damaging-move resolver
for accuracy, critical and random damage, then consumes PP and applies capped
HP damage. Fainting is surfaced in the scene. If any currently usable enemy
move or the selected player move falls outside the represented subset, or the
encounter is double, the command reports unsupported and does not mutate battle
state.

## Explicit gaps

This is a command-path slice, not a complete battle implementation. It does
not yet apply abilities, weather, held items, status, stat stages, field effects,
move secondary effects, reward/EXP, wave transition, capture/run/switch, or the
complete upstream end-of-turn phases. The scoring path does not yet model
upstream effective-stat/ability modifiers or the AI KO-only candidate filter.
Native save schema v2 can now persist this single-opponent wave-1 checkpoint
between turns: encounter ID, turn, HP, canonical move IDs and PP. Loading
reconstructs the pinned encounter and validates its derived move/HP/PP state
before restoration. Version-one setup saves are upgraded on load. Party changes,
status/stat stages, rewards and multi-wave progress remain absent, and the
checkpoint schema still needs its deferred regression pass.

## Pinned upstream references

- `src/field/pokemon.ts`: `EnemyPokemon.getNextMove()` and `getNextTargets()`.
- `src/data/moves/move.ts`: `Move.getUserBenefitScore()`,
  `AttackMove.getTargetBenefitScore()` and the `AttackMove` constructor.
- `src/phases/turn-start-phase.ts`: `TurnStartPhase.getCommandOrder()` and
  queued FIGHT phase order.
- Revision: `8555c08c823b856cbec4eb99ca84ea52a955836d`.

No compile, test, parity or hardware validation was run in this implementation
phase, following the instruction to defer all validation until migration work is
finished.

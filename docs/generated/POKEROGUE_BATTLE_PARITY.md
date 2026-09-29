# PokéRogue Battle Parity Status

Pinned source: `pagefaultgames/pokerogue@8555c08c823b856cbec4eb99ca84ea52a955836d`.

| Area | Upstream source | GUI_3DS status | Evidence | Remaining gap |
|---|---|---|---|---|
| Canonical battle tables | `src/data/balance/species/generation-01.ts…generation-09.ts`, `src/data/moves/move.ts` | PORTED as compact generated species base stats, abilities, level learnset references, and move scalar metadata | BETA-UI-9A import/runtime table assertions | Form-specific stats, types, ability IDs and provenance are in the generated form table; effect attributes remain canonical raw metadata |
| Pokémon stat formula | `src/field/pokemon.ts`, `Pokemon.calculateStats` | PARTIAL: base stat + explicit IV/level/nature formula with integer rounding | BETA-UI-9D native C++/WASM checks against pinned Bulbasaur content | IV/nature/ability/form/moveset generation call sequence, EVs and stat modifiers are not integrated |
| Battle actor creation | `src/field/pokemon.ts`, constructor and `src/phases/encounter-phase.ts` | UNSUPPORTED: initializer requires explicit inputs; it does not invent them | Input validation/failure atomicity in BETA-UI-9D | Reproduce upstream constructor RNG order and real starter/wild move generation |
| Move execution | `src/phases/move-effect-phase.ts`, `Pokemon.getAttackDamage` | UNSUPPORTED | No production action/phase consumer exists yet | Accuracy, order, crit/random rolls, type effectiveness, STAB, status, abilities, modifiers, effects and faint/result transitions |
| Battle progression | `src/battle.ts`, phase queue | UNSUPPORTED | Existing Classic resolver only supplies wave-1 encounter candidates | Instantiate player/enemy battlers, process turns, resolve win/loss, reward and next progression |

The stat initializer is deliberately a base-formula building block. It returns a value only when species, level, six IVs, nature stat indices, form-valid ability and up to four canonical move IDs are explicitly provided. `statsAreBaseFormulaOnly` stays true so consumers cannot mistake the result for full upstream stats. No battle parity percentage is claimed.

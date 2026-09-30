# BETA-UI-9E — Move AI source metadata bridge

## Scope completed

The pinned `parseMoves()` importer already records each chained upstream
`.attr(...)` class name in canonical `MoveDefinition.upstreamAttributes`. The
native content generator previously emitted only a small set of derived flags,
so most declared move semantics could not be queried from the Old 3DS runtime.
For the current 920-move snapshot, 750 moves expose 935 attribute instances
across 194 distinct attribute classes. The generated `Move` carries a bounded
offset/count into a compact, flattened `MoveAttribute` table, and
`moveHasAttribute()` can query that table
without allocating memory or embedding move-specific rules in presentation code.

This keeps the behavior metadata data-driven and preserves a path to implement
the pinned enemy AI: `EnemyPokemon.getNextMove()` scores move attributes and
conditions, targets, effectiveness, STAB and seeded tie progression. The C++
metadata bridge does not claim that AI or move effects are implemented.

## Provenance

- Repository: `https://github.com/pagefaultgames/pokerogue`
- Revision: `8555c08c823b856cbec4eb99ca84ea52a955836d`
- Import source: `src/data/moves/move.ts`, `Move` constructor declarations,
  `Move.getUserBenefitScore()`, `Move.getTargetBenefitScore()`.
- AI consumer source: `src/field/pokemon.ts`, `EnemyPokemon.getNextMove()` and
  `EnemyPokemon.getNextTargets()`.
- Canonical bridge: `MoveDefinition.upstreamAttributes` →
  `PokerogueRuntimeContent.hpp::Move.attributeOffset/attributeCount` →
  `PokerogueContent::moveHasAttribute()`.

The generator also emits compile-time sentinels for Karate Chop’s `HighCritAttr`
and Tackle’s empty attribute range. Offset/count arithmetic is bounded before
flattening, and the native query checks ranges without addition overflow.

## Deferred validation

No compile, test, native parity, or hardware run was performed in this
implementation phase, per the active instruction to defer all validation until
the migration implementation is complete. Add importer/runtime attribute
round-trip coverage to the final test pass. Next AI work must translate each
attribute's actual score semantics or report the move explicitly unsupported;
unknown attributes must never be treated as zero-effect gameplay behavior.

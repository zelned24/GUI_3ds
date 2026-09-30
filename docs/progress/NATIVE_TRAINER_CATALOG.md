# Native Trainer Catalog — Migration Progress

This step imports actual trainer identities and declarative trainer configurations from the pinned PokéRogue revision into the canonical catalog. It does not interpret trainer AI, team generation, or battle behavior.

## Pinned upstream inputs

- Game revision: `pagefaultgames/pokerogue@8555c08c823b856cbec4eb99ca84ea52a955836d`
- `src/enums/trainer-type.ts`: numeric `TrainerType` identity.
- `src/data/trainers/trainer-config.ts`: `trainerConfigs[TrainerType.*]` declarative config fragments.
- `src/data/trainers/trainer-party-template.ts`: named static `TrainerPartyTemplate` and `TrainerPartyCompoundTemplate` declarations.
- `src/enums/party-member-strength.ts` and `src/enums/evo-level-threshold-kind.ts`: source numeric values used by those segments.
- Locale revision: `pagefaultgames/pokerogue-locales@23aea1cb0da5a0b15b836f3c243791591cc42303`
- `en/trainer-classes.json` and `en/trainer-names.json`: localized trainer class and person names.

## Pipeline changes

- Added `TrainerDefinition` to the existing canonical models.
- Production importer fetches the enum, config registry, and locale namespaces; it records source hashes and TrainerType IDs, maps trainer-class locale keys, and preserves each config as upstream raw data.
- The canonical trainer record now normalizes the safe declarative subset: money multiplier, static and recognized wave-scaled party-template references, the upstream `TWO_AVG` constructor default, tiered species-pool candidate groups, boss/double/static-party flags, and same-seed behavior. Other party-template callbacks, party-member callbacks, filters, AI, modifiers, dialogue, and presentation settings remain listed as unsupported raw config, never executed.
- Production importer normalizes supported static party-template segments (slot count, strength ID, same-species/balanced flags, evolution-threshold kind), preserves the original source fragment, and rejects unknown constructors. Trainer config references to missing named templates fail import.
- Runtime generation validates each template's compact ranges/slot totals and emits provenance-carrying ordered segment/template tables. Biome trainer pool rows now retain upstream member index so selection does not reorder choices by trainer ID.
- `PokerogueEncounterResolver::resolveTrainerType` ports the pinned rarity roll, boss/non-boss tier thresholds, empty-tier downgrade and ordered pool-member draw. `resolveClassicTrainerPartyLevels` ports the non-Daily `Trainer.getPartyLevels` arithmetic using binary64 operations in upstream order; it currently remains a standalone resolver, not connected to encounter/battle construction.
- Native trainer helpers select static and recognized wave-scaled templates, resolve tiered trainer species pools while preserving source candidate order/re-roll behavior, and apply trainer NORMAL/STRONG evolution thresholds plus Classic wave-20 trainer evolution suppression. Actor creation and upstream-ordered party RNG integration remain pending.
- Import report counts trainer identities/configs and explicitly reports missing upstream config or class-localization records.
- Runtime generator validates trainer identities and biome trainer-pool references, and emits a compact provenance-carrying `TrainerType` table. It does not copy TypeScript configs into native code.

## Remaining before trainer battles

- Normalize the remaining `TrainerConfig` fields used by Classic into compact data: gender/double variants, gym/evil-grunt/rival policies, party-member callbacks and static party overrides, specialty filters, and fixed trainer selection.
- Import and normalize rival slot species pools, dynamic gym/evil-grunt party policies and trainer party member callbacks, then apply trainer level/species/form/moveset generation in the correct shared RNG order.
- Port trainer party generation, battle-side selection, and rewards against pinned source ordering/RNG.
- Connect these results to the native battle command/turn flow and localize the selected trainer/person name.

## Candidate representation follow-up

The importer now preserves `isGroup` independently of group size. The native generator requires this marker (older ambiguous records must be reimported), emits it in `TrainerPoolChoice`, and rejects non-group candidates containing multiple species. The resolver rerolls array candidates even when they contain a single species, retaining the distinction made by the pinned source. Empty tiers downgrade before selecting a member. Candidate counts exceeding the native 16-bit range fail generation explicitly.

Zero money multipliers are accepted by both import and generation. Route and biome trainer-chance declarations now use a stable declaration anchor, so inserting the experience table earlier cannot prevent these declarations from being emitted.

These changes are source edits only. The checked-in generated header is stale relative to the trainer schema; regeneration and end-to-end checks remain pending. Trainer actor construction, full party generation and battle integration are still incomplete.

## Constructor and callback template selection

Pinned `Trainer.constructor` chooses an index from `config.partyTemplates` even if `getPartyTemplate` later calls `partyTemplateFunc`. Canonical `partyTemplateKeys` now represents only the constructor list, including its `TWO_AVG` default. Recognized callback references live separately in `trainerRules.callbackTemplateKeys` and use a separate range in the generated table.

The native selector consumes the constructor draw before resolving the recognized static or wave-scaled callback. A supplied constructor index skips the draw and clamps to the final constructor slot, as upstream does. Status matching is exact; unknown callback statuses remain unsupported. Table ranges are checked before access. This helper still requires integration into the actual constructor/name/party RNG sequence; configuration helpers that indirectly change templates remain pending normalization.

## Initializer and callback parsing boundary

The config parser now reads only top-level method calls, skipping balanced callback bodies, strings and comments. A setter inside a party-member callback no longer changes the outer trainer definition. Argument splitting ignores comment punctuation while preserving source fragments.

Inspection of pinned `TrainerConfig.initForGymLeader`, `initForEliteFour`, `initForChampion`, `initForEvilTeamAdmin`, `initForEvilTeamLeader` and `initForStatTrainer` confirmed that these helpers alter templates, money multipliers, boss/static flags and other fields. Records using such initializers now carry `INITIALIZER_SEMANTICS_PRESERVED` plus ordered initializer calls and arguments. Full trainer construction remains unsupported for that status; the records remain partial configurations until all initializer semantics are supported. Full source records remain available for normalization. This removes an incorrect assumption that named trainers inherit an ordinary `TWO_AVG` party unchanged.

Pending parser coverage: commented commas, nested setter callbacks, single-element species groups, initializer records, constructor/callback RNG ordering, and unknown status rejection. No tests or compilation executed in this continuation.

## Source-derived initializer assignments

The importer now locates the called `TrainerConfig.initFor*` method in the same pinned source file and preserves its method symbol and complete source fragment. Its unconditional top-level `setPartyTemplates`, `setPartyTemplateFunc`, `setMoneyMultiplier`, `setBoss` and `setStaticParty` calls enter the normalizer in call-chain order. Conditional blocks and nested callbacks are excluded from this declarative subset. Later explicit setters replace earlier values, matching the upstream assignment methods instead of concatenating template lists.

This imports, for example, champion/Elite Four/evil leader template references and money multipliers from actual source declarations, without duplicating their values in the normalizer. Other method calls remain recorded alongside the raw definition. The enclosing config provenance identifies repository, revision, file and hash for these fragments. The `INITIALIZER_SEMANTICS_PRESERVED` runtime boundary remains because specialty filters, signature slots, pools assigned through expressions and other callbacks are not yet normalized fully.

Regeneration, execution checks and final tests remain deferred. This is an implementation increment, not evidence of playable trainer battles.

## Classic gym wave policy

`getGymLeaderPartyTemplate` in the pinned party-template source has a separate Classic branch. The importer now parses its ordered inclusive upper bounds and final fallback, validates all referenced template keys, and preserves the function body, source symbol, revision, path and hash. Unexpected branch expressions fail clearly. Daily and default-mode branches are not used for this Classic policy.

The generator emits these bounds on callback template references (`maxWave == 0` denotes the final unbounded range). The C++ template selector consumes the constructor draw, then chooses the first matching range without another RNG draw. `partyTemplateStatus` in native data describes template resolution; `configStatus` separately retains incomplete initializer behavior. Resolving a gym template therefore does not imply that signature slots, filters, modifiers or a full trainer actor are supported.

Canonical import, generated header refresh and runtime verification remain pending under the deferred-validation instruction. The implementation must ultimately be checked at each threshold and neighboring waves, against pinned upstream, alongside constructor RNG state checks.

## Classic evil-grunt wave policy

The importer now reads `getEvilGruntPartyTemplate` from the pinned party-template file. Each inclusive boundary resolves through the already imported `ClassicFixedBossWaves` enum. The canonical range preserves the original enum symbol, numeric value, template key and separate provenance for both source files. The final fallback remains an explicit range, even when it repeats the preceding template.

Recognized direct and zero-argument arrow callbacks use the same `CLASSIC_WAVE_RANGE_TEMPLATES` representation and native selector as gym templates. No grunt-specific numeric wave constants or species lists were added to C++. Unknown conditions, missing enum symbols and missing templates fail normalization. Full grunt species generation, battle construction and rewards remain pending.

No import, generation, build or tests were executed for this increment. Deferred boundary tests must cover each imported grunt threshold and the waves immediately before/after it, plus the final fallback and constructor RNG state.

## Derived trainer identity and member seed offsets

The importer now parses the pinned `TrainerConfig.getDerivedType` switch, including grouped case aliases and identity fallback. It rejects unresolved/duplicate enum references or unrecognized branch syntax. Each trainer carries the resolved enum ID and the method's raw source/provenance. The runtime generator validates that the derived ID resolves to a real imported trainer and emits `derivedTypeId`.

`trainerPartyMemberSeedOffset` in the existing native trainer helper implements the pinned `Trainer.genPartyMember` offset formula: static parties use derived type plus member offset; other parties also include wave and the shifted derived type, with the source same-seed flag controlling member indexing. It is intended for the existing `PokerogueSeedOffsetScope`; actor creation, species resolution and random traits must all run inside that scope. This helper is not yet connected to trainer actor construction.

Deferred checks include variant aliases, different waves, static parties, shared seeds, and restoration of the outer RNG after member generation. No generation, compilation or tests were run.

## Per-member template access

The native trainer helper now resolves each member's strength, evolution threshold, same-species/balanced flags and compound-segment start. Its behavior follows `TrainerPartyTemplate`/`TrainerPartyCompoundTemplate` accessors and the segment-offset loop in `Trainer.genPartyMember`. Simple templates ignore the index; exhausted compound segments return the upstream parent defaults. Table bounds, segment sizes and totals are checked before use.

The existing party-level resolver now uses this accessor for ordinary member strengths. The double-battle level expansion retains the separate upstream parent-strength rule. Species generation must consume the same accessor when implementing same-species reuse and balancing; those behaviors are not yet integrated. No compilation or tests were executed.

## Trainer level representation and arithmetic

`TrainerPartyLevels` now stores 16-bit levels, consistent with `PokemonBattleState`. This removes an unnecessary 8-bit storage restriction; it is not evidence that pinned Classic trainer levels exceed 255.

The resolver now follows `Trainer.getPartyLevels` using binary64 operations, `floor`, `pow` and `ceil` in the source expression order. The previous exact rational rewrite could differ from JavaScript at rounding boundaries. Non-finite or out-of-range levels fail explicitly. Cross-platform numerical parity, including the native math library and compiler behavior, remains unverified until the final tests. No fast-math flags were found in the inspected runtime/build sources; no compilation was performed.

## Same-species evolution ordering

Inspection of pinned `Trainer.genPartyMember` confirms that `genNewPartyMemberSpecies` runs before the same-species branch replaces its result. The future party constructor must preserve those initial draws. Reused segment species then call `getTrainerSpeciesForLevel` with evolution disabled; required prevolutions are still considered before that early return. The final source species-level pass remains separate and must also preserve its draw order.

The native trainer species resolver now accepts `allowEvolving` separately from `tryForcePrevo`, preserving that distinction and propagating it through recursive evolution. Zero-threshold evolution edges are excluded, matching `calcEvoChance`'s positive-threshold pool condition. Same-species party construction itself is still pending; no test, compilation or end-to-end validation was run.

## Validation status

The pinned `src/data/balance/signature-species.ts` registry is now part of the
production source snapshot. The importer records each ordered trainer slot,
individual species or grouped choice, validates every `SpeciesId`, and stores
repository, revision, source path, symbol, hash and raw declaration. The native
generator emits compact choices and species references with checked ranges.
The C++ lookup maps `signatureSpecies[0]` to the last party member, matching
the negative callback slots installed by gym and Elite Four initializers; a
group uses the seeded `randSeedItem` draw. The lookup is not yet connected to
trainer actor construction, movesets or rewards. Generated content
and validation remain deferred under the user's ordering instruction.

`resolveTrainerSignatureMemberSpecies` now applies the imported evolution
threshold after the signature draw, matching `getRandomPartyMemberFunc`.
The caller must keep `PokerogueSeedOffsetScope` active through both that
resolution and `addEnemyPokemon`; actor construction and its RNG consumption
are still pending. The importer flags only initializer calls that actually
install the matching registry entry, so an unused `signatureSpecies` record
does not by itself activate a native party callback.

No pinned content import, generated-data refresh, build, compilation, or tests were run in this step. Full validation remains deferred until migration implementation is complete, per the requested order.

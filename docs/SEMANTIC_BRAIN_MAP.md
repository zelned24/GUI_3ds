# Repository Semantic Brain Map

The graph at [`semantic-brain-map.json`](semantic-brain-map.json) is the compact index for future repository work. It stores node metadata, source fragments, typed relationships, context profiles and reviewed boundary violations; it never stores source bodies.

## Query a relevant subgraph

Before a cross-file change, query the task area and read only the returned nodes, their source fragments, direct dependencies/consumers, tests, upstream references and linked docs:

```bash
node scripts/semantic_brain_map_query.mjs GameMode --depth=1
node scripts/semantic_brain_map_query.mjs GameFlow --depth=1
node scripts/semantic_brain_map_query.mjs PresentationBindings --depth=1
node scripts/semantic_brain_map_query.mjs "modify biome progression" --depth=0
```

The output starts with a `contextBudget` divided into primary, secondary and reference material. Named profiles are preferred; other queries rank node IDs, symbols, responsibilities and tags. Traversal expands only from primary nodes and returns only edges/violations inside the selected subgraph. Increase depth or limit only when the task requires more context.

For a GameMode task, the current profile selects:

- **Primary:** GameModeDefinition, GameModePolicy, GameModeRegistry and CanonicalContent.
- **Secondary:** importer, repository and BETA-UI-8 tests.
- **Reference:** pinned upstream enum, mode implementation, save consumer and current contract documentation.

This profile intentionally excludes SceneModel, CanvasRenderer, Timeline and native render code.

For the task **“add a new species”**, the profile follows:

```text
SpeciesId / initGenerationOne…Nine
  → SpeciesImporter / FormImporter
  → Normalizer
  → CanonicalContent
  → RuntimeContent
  → asset metadata / AssetIndex boundary
  → LocaleImporter
  → BETA-UI-8C integration test
```

The `add a new species` query selects only the species/form importer, canonical envelope, repository, locale resolver, asset index boundary, pinned species/assets/locales sources and their integration test. It deliberately excludes Timeline, CanvasRenderer, Scene Composer, Citro2D export and BattleEngine because those are not dependencies of adding a declarative species record.

The BETA-UI-8C `modify biome progression` profile returns `BiomeDefinition`, `GameModePolicy`, `WaveDefinition`, `RouteDefinition`, the explicitly planned `BiomeResolver` boundary, canonical registry/import/repository, BETA-UI-8C tests and pinned upstream biome/type/progression references. `--depth=0` keeps the profile boundary: because GameModePolicy is also consumed by unrelated GameFlow and presentation-binding targets, expanding graph neighbors would include context outside this domain task. The query therefore excludes Timeline, CanvasRenderer, audio and native exporter/runtime nodes unless a task explicitly asks to trace into presentation or runtime.

The BETA-UI-9A `change how first wave enemy is resolved` profile returns `EncounterResolver`, `FirstRunFlow`, mode/wave/biome contracts, the production canonical+progression import seam, pinned progression source references and the real-content integration test. It deliberately excludes TimelineUI, CanvasRenderer, AssetPackager and Citro2D exporter; the UI/presentation slot is downstream and does not own encounter selection.

The `build native C++ first-run content screen` profile follows the pinned canonical importer into the compact generated ROM header, then through `FirstRunRuntime` to the existing native `ScenePlayer`. It includes canonical species/locales and presentation contracts without pulling in editor Timeline or battle-rule implementations.

The **“add canonical battle data to native runtime”** profile follows pinned species and move declarations through the normalizers into compact C++ tables. It includes base stats, canonical ability IDs, level-up learnset ranges, move scalar fields, source provenance, the generator's cross-reference validation and its compile-time Bulbasaur/Tackle vectors. Raw upstream effect expressions stay in canonical JSON; this table does not claim move-effect or battle execution support yet.

The **“initialize native Pokémon battle state”** profile follows the generated canonical species/form/move tables into `PokemonBattleState`. The initializer implements only the pinned base stat formula from explicit instance inputs and validates cross-references without consuming RNG or inventing defaults. It requires a specified gender, retains that gender in the state, and rejects genderless/ratio metadata inconsistencies. Imported form rows retain per-form stats, types, abilities and provenance; species rows retain upstream malePercent as tenths-of-a-percent with distinct sentinels for genderless null and missing/unknown. The native gender selector matches pinned `PokemonSpecies.generateGender`: a genderless record returns without RNG consumption; a numeric ratio consumes one `randSeedFloat` draw and applies upstream's inclusive comparison. The native battle-state initializer can derive and retain IVs from an explicit 32-bit actor ID. `generatePokemonActorIdentity` mirrors the pinned constructor's initial sequence through ability-index selection, full-width ID/IV generation, and gender. `generatePokemonActorIdentityAndForm` composes that draw sequence with form selection as one ordered API, and only publishes the actor identity after both stages succeed. `selectPokemonActorForm` ports pinned `BattleScene.getSpeciesFormIndex`, preserving canonical upstream form-array order and its random form bounds, gender and nature rules, biome/time choices, Classic Gimmighoul rule, and trainer-specialty selection. It stores the selected imported form ID in the actor identity. `initializePokemonBattleStateForActor` transfers that identity into a validated canonical battle state and resolves its ability slot against the selected form. The native model ports all 25 pinned Nature IDs and their raised/lowered-stat mapping. `generatePokemonActorNature` stores the upstream-sized deterministic choice in the actor identity after the caller has completed preceding form/shiny steps; battle-state initialization transfers the nature and applies its stat modifier. `generatePokemonActorForWildEncounter` composes ability/ID/IV/gender/form/nature and initial tera-type draws for non-fused wild actors. FirstRunRuntime now consumes the fresh-profile starter path and resolves both regular wild actors/movesets in sequential upstream constructor order for a wave-1 double encounter. This is still not a complete battle: shiny state/variant, modifier-driven fusion, stat stages, enemy AI, turn commands and battle progression remain unsupported. `resolveStandardPokemonMoveDamage` follows the pinned baseline order: type immunity check, accuracy roll, stage-0 critical roll, inclusive 85–100 random damage roll, then static-power/base-stat formula, regular STAB, type effectiveness and `toDmgValue` rounding. `useStandardPokemonMove` connects that roll to moveset PP and capped target HP application, reporting fainting and rejecting invalid/no-PP actions without state mutation. Accuracy/evasion stages, crit modifiers, variable move attributes/power, weather/status/ability/item/field modifiers, command ordering and full battle progression remain unsupported. An explicit matching form supplies its stats and types, while species without a declared form use canonical species values. The profile includes its C++/WASM regression harness and pinned `Pokemon.calculateStats`, `PokemonSpecies.generateGender`, `Pokemon.generateAbilityIndex`, `Pokemon.getBaseDamage`, `Pokemon.calculateStabMultiplier`, `Pokemon.getCriticalHitResult`, `MoveEffectPhase.hitCheck`, `getTypeDamageMultiplier` and `toDmgValue` sources.

## Graph model

Layers are PRODUCT → DOMAIN → PRESENTATION → RUNTIME → SOURCE → TEST, with documentation cross-links. A node identifies a semantic fragment (contract/class, method group, test group, fixture, source symbol, or documentation responsibility) by stable ID, type, source location, responsibility, status and tags. An edge is a typed relation such as `IMPORTS`, `CALLS`, `PRODUCES`, `CONSUMES`, `SERIALIZES`, `VALIDATES`, `TESTS`, `MAPS_TO`, `ORIGINATES_FROM`, `DEPENDS_ON` or `RUNTIME_USED_BY`.

The graph is hybrid: code dependencies connect to product/domain docs, tests, fixtures, pinned upstream symbols and runtime/presentation layers. `violations` records only observed problematic/current boundaries with severity, evidence and a recommended owner boundary; gaps that are not violations stay as planned nodes.

## Maintenance and determinism

Keep JSON UTF-8, two-space formatted, and stable: sort nodes by `id`, edges by `from/type/to`, and violations by `id`. Update relevant nodes/edges/profile/violations in the same change that changes architecture. Include symbol or line-fragment metadata for code nodes when practical. Use paths and symbol references only; never embed whole source files, generated assets, timestamps, or machine-specific locations.

Validate graph/query changes through the BETA-UI-8 test group. The query script has no LLM, network, embedding, or repository-crawl dependency; its cost is bounded by the selected semantic subgraph, rather than reading the full repository.

For the **import upstream biome encounter pools** task, the focused profile traces `plainsBiome.pokemonPool` and `plainsBiome.trainerPool` at the pinned upstream source through `PokerogueImporter.parseBiomes`, `BiomeDefinition`, `CanonicalContent`, and the real import integration test. The profile deliberately does not imply that the first-run resolver already applies upstream tier/wave weighting or PRNG behavior.

The first encounter task follows `Arena.randomSpecies` through tier selection, pool member choice, rarity retry, then `getWildSpeciesForLevel` into native `PokerogueEncounterResolver`. Its canonical C++ catalog now contains 488 ordered, provenance-carrying evolution edges. The Old 3DS resolver applies upstream forced-prevolution thresholds and wild level-based evolution selection to the wave-1 candidate using the same encounter RNG stream. A focused context profile retrieves the resolver, generated edge table, first-run consumer, pinned source, and WASM tests without loading presentation or battle code. The double-battle check, second battle-scoped level, second sequential wild species resolution, and second wild actor/moveset construction now follow `Battle` construction and `EncounterPhase.start`; the existing double vector checks both resolved species and draw ordering. The composed native FirstRunRuntime path awaits deferred validation. Trainers/bosses and battle execution remain unsupported, so this is not a playable battle yet.

The native move-generation profile starts at `PokemonLevelMovePool.hpp` and `buildPokemonLevelMovePool()`, then reaches `PokemonWildMovesetGenerator.hpp`, `PokemonMoveEffectivePower.hpp`, and `PokemonMovesetWeights.hpp`. These stages consume imported species/form/prevolution ranges, resolved effect metadata, and the regular-wild STAB-first weighted picker. Attribute interpretation and the first-run battle constructor are still not connected, so they do not establish a playable battle.

The **“port Pokerogue Phaser RNG to Old 3DS C++”** profile follows `PokerogueRngAdapter`, `PokerogueBattleRng`, `PokerogueWaveClock`, the native `PokerogueEncounterResolver`, and BETA-UI-9C reference vectors. It tracks distinct streams for encounter pools, battle construction/level generation, and per-turn battle rolls. The battle seed composes `resetSeed(waveIndex)` with `executeWithSeedOffset(waveIndex << 3)`; each turn re-seeds from the 16-character battle seed shifted by `turn << 6`. The profile excludes renderer and Scene Composer.

The native route-content subgraph now traces `Biome.biomeLinks` and `SelectBiomePhase` through canonical `RouteDefinition` records into generated `PokerogueContent::Route` rows and `resolveClassicNextBiome`. It retains normalized endpoints, optional exclusion weights and source provenance, and follows the pinned seeded exclusion/pick order. The resolver is not called by the native run yet; `SwitchBiomePhase`, rewards and next-wave run state remain separate progression work.

The Classic trainer subgraph now follows pinned `trainerPartyTemplates`, party-strength/evolution-threshold enums, `Arena.randomTrainerType`, `Trainer.getPartyLevels`, and the static branch of `Trainer.genNewPartyMemberSpecies`. Trainer identity/config, static and recognized wave-scaled templates, and tiered species-pool candidate groups flow through canonical models with original raw fragments and provenance. Generated biome trainer-pool rows preserve upstream order. `PokerogueEncounterResolver` ports trainer tier roll/downgrade/member choice and static species-pool candidate selection; `PokerogueTrainerPartyLevels.hpp` ports static/wave-scaled template choice and non-Daily level arithmetic with integer rational rounding. These components are not yet wired into `FirstRunRuntime`: unsupported filters/callback-driven boss/rival policies, actor construction and battle execution remain missing, and generated content is pending final refresh.

The Classic wave-schedule subgraph traces `GameMode.isWaveTrainer/isWaveFinal/isFixedBattle`, `classicFixedBattles`, `ClassicFixedBossWaves`, `Biome.trainerChance`, and `Arena.trainerChance` into canonical wave/trainer metadata and `PokerogueClassicWaveSchedule.hpp`. The importer preserves fixed trainer config fragments and trainer type IDs without executing upstream TypeScript. The native scheduler applies the pinned nearby fixed/gym suppression and wave-offset RNG algorithm when given the live RNG state at its upstream call point; battle construction does not call that scheduler yet. `FirstRunRuntime` still stops unsupported trainer/boss encounters, and the checked-in canonical/generated data awaits the final pinned import and refresh.

The native experience subgraph follows pinned `expLevels`, `getLevelTotalExp`, `Pokemon.getExpValue`, and `BattleScene.getMaxExpLevel` into species `baseExp`, imported growth-rate curves and the compact C++ content header. `PokemonExperience.hpp` currently ports total-experience curves, the defeated-form base-experience multiplier, and the Classic level-cap formula. Faint rewards, player experience state, level-up/stat recalculation, and multi-wave persistence are not connected yet; this graph path is source traceability, not a claim of playable progression.

The **“import species growth rarity and base total for wild encounter rules”** profile connects the pinned generation importer to typed `SpeciesDefinition` metadata, canonical serialization, compact generated C++ fields and pinned import tests. Rarity is tri-state so an absent source field cannot silently become `false`; evolution edges preserve raw source, thresholds, order and provenance, and feed the native wild-species resolver.
# Native save and update continuation

`export native progress` selects `runtime.native-save` and `test.native-save`.
The native journal persists RunSetup or a wave-1 single-battle checkpoint
between turns and exposes portable SD export/import. Schema v2 can replay the
pinned encounter, reapply canonical move IDs/PP and restore HP plus turn index;
v1 setup records upgrade on load. Party, rewards and multi-wave progress remain
pending, and v2 checkpoint regression coverage is deferred to the final test
pass.
`runtime.content-update-store` records the staged manifest/asset verification
core; network transport, trust provisioning and runtime loading are not yet
connected. Neither node implies a complete playable Classic run.
Native wild moveset stages compose learnsets, resolved effective-power data, weighting, STAB-first draw, and move-slot selection. Attribute resolution and battle-constructor integration are pending.

## Current native moveset migration delta

The semantic profile **“generate wild moveset”** now reaches the actual `FirstRunRuntime` consumer: imported learnset candidates, actor stats/types, generated move metadata, supported ability movegen profiles, weighted draws, and the resolved move IDs. Both regular-wild slots are connected in sequential order; unsupported callbacks fail closed. The native harness contains regression cases for Compound Eyes, Skill Link, Hustle, and Drizzle; they have not been compiled or run yet.

The ability importer now reads balanced `AbBuilder` constructors and chained expressions, preserving callback source after nested semicolons. The pinned reimport recorded deterministic content hash `114b52091bf72e22adb933511d3a8354829dac9c3f743e5ac0cd21fb682abc28` for the current source snapshot, including the separate upstream egg-move catalog. This establishes metadata preservation; it does not claim battle behavior for ability callbacks outside the explicitly supported movegen subset.

The fresh-profile starter path now carries upstream `defaultStarterSpecies` order into the generated C++ species rows. `PokemonFreshProfile.hpp` reproduces the isolated neutral-nature stream; `FirstRunRuntime` combines its profile values with canonical starter moves and the run-seeded PID/Tera draws to initialize the player's first `PokemonBattleState`. The native screen connects D-pad move selection and A to a fail-closed single-opponent command resolver with a bounded wild `SMART_RANDOM` subset. The v2 native save captures/restores that checkpoint between turns; full battle effects, double battles, rewards and Classic progression remain open. Command/save tests are deferred to the final validation pass.

For the task **“port wild move selection”**, the profile follows imported `MoveAttr` class names into the generated C++ move catalog, then references the pinned `EnemyPokemon.getNextMove()` AI implementation. `FirstRunRuntime` currently scores and selects only plain `NEAR_OTHER` damaging moves with no source attributes or flags; ability-aware scoring, the KO prefilter and other move semantics still need explicit native implementations. The turn command profile is `runtime.pokemon-command-turn` and intentionally excludes Timeline, CanvasRenderer and Citro2D export.

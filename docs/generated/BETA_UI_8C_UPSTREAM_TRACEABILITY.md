# BETA-UI-8C — Upstream Traceability

Every source below was fetched from the pinned repository revision during the canonical import. Paths and symbols are the inspected upstream declarations; no TypeScript implementation is copied into the 3DS runtime.

| Domain | Upstream file | Upstream symbol | Canonical model | Runtime consumer | Test |
|---|---|---|---|---|---|
| Game mode | `src/enums/game-modes.ts`; `src/game-mode.ts` | `GameModes`; `getGameMode`; `isWaveFinal` | `GameModeDefinition`, `GameModePolicy` | `GameModeRegistry` from canonical/runtime content | `BETA-UI-8B` mode contract; pinned migration test |
| Species | `src/data/balance/species/generation-01.ts` … `generation-09.ts`; `src/enums/species-id.ts` | `initGenerationOne` … generation initializers; `PokemonSpecies`; `SpeciesId` | `SpeciesDefinition` | `DataManager.importCanonicalProduction`; `RuntimeContent.collections.species` | pinned migration test; parser/provenance tests |
| Form | generation species files | `PokemonForm` in `PokemonSpecies.forms` | canonical `forms[]` record with parent species ID | `RuntimeContent.collections.forms`; asset reference remains declarative | pinned migration test checks parent references |
| Move | `src/data/moves/move.ts`; `src/enums/move-id.ts` | `AttackMove`, `Move`, `StatusMove`; `MoveId` | `MoveDefinition` | `DataManager` move catalog / `RuntimeContent.collections.moves` | pinned migration test checks multiple types and categories |
| Ability | `src/data/abilities/init-abilities.ts`; `src/enums/ability-id.ts` | `AbBuilder`; `AbilityId` | `AbilityDefinition` with raw attribute declaration | `RuntimeContent.collections.abilities`; battle triggers remain unimplemented | pinned migration test checks upstream attributes/extensions |
| Item / modifier | `src/modifier/modifier-type.ts` | `ModifierType` subclasses | `ItemDefinition` with source class symbol, raw declaration, and null unknown price/tier | `RuntimeContent.collections.items`; no effect execution | pinned migration test checks provenance and no invented price |
| Locale | `en/{game-mode,pokemon,pokemon-form,move,ability,modifier}.json` | namespace records and keys | locale entries keyed by locale + namespace + canonical ID alias | `PokerogueLocaleImporter.resolveCanonical` | pinned migration test resolves `tackle` to English text |
| Asset metadata | `images/pokemon/{1,6,25}.json` | TexturePacker frame records | provenance-bearing `assetReference` | `RuntimeContent.collections.assetReferences`; resolver integration is a later step | pinned migration test checks 3 verified upstream metadata files |

## Interpretation boundary

Canonical records retain their inspected upstream TypeScript fragments under `extensions.upstreamRawRecord`. Ability attribute semantics, item/modifier effects, and move effect execution are preserved as source metadata and marked `NOT_IMPORTED`; they are not approximated as battle behavior. The report lists MoveId symbols whose declarative constructor shape was not recognized as `NOT_YET_SUPPORTED_BY_GUI_3DS`.

Sprite JSON was fetched and hashed for Bulbasaur (1), Charizard (6), and Pikachu (25). All other species and forms carry only their upstream asset repository/revision and species ID with `pending-source-manifest`; no unverified physical path is asserted. No 3DS asset paths are synthesized.

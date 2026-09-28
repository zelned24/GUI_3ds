# BETA-UI-8A — Canonical Data Contract

This phase establishes a data boundary for imported PokéRogue content. It does not add gameplay rules or couple game data to the editor, renderer, or scene composer.

```text
UPSTREAM
   ↓
SOURCE SNAPSHOT
   ↓
IMPORTER
   ↓
NORMALIZER
   ↓
CANONICAL CONTENT
   ↓
OVERRIDE SET
   ↓
RUNTIME CONTENT
```

## Contracts

- `SourceSnapshot` records repository, pinned revision, optional branch/tag, source type, and the content, asset, and locale revisions. Its identity excludes import timestamps.
- `CanonicalContent` separates `schemaVersion` from `contentVersion`, carries a snapshot, provenance, collections, and extension metadata. Schema validation reports unsupported required extensions and malformed provenance explicitly.
- `Provenance` records repository, revision, source path, source type, content/schema versions, optional symbol, and source hash. `UPSTREAM`, `TEST_FIXTURE`, `LOCAL_OVERRIDE`, `GENERATED`, and `UNVERIFIED` are distinct source classes.
- Import parsers preserve the original matched source record in `extensions.upstreamRawRecord`, allowing later normalizers to understand fields that the current model does not yet interpret.
- `OverrideSet` is separate from canonical collections. Its current allowed scopes are presentation, assets, memory, and hardware; gameplay rule overrides are rejected. `RuntimeContent.resolve()` applies those values to a copy and leaves canonical input untouched.
- `RuntimeContent` carries runtime compatibility bounds independently from content and schema versions. Compatibility failures are returned as actionable errors.

## Fixture / production boundary

Offline importer samples are isolated in `test/fixtures/offlineImporterSources.js`; the adapter baseline remains in `test/fixtures/fallbackVerticalSlice.js`. Offline importer output is labeled `TEST_FIXTURE` and points at its fixture file. `DataManager.importUpstream()` accepts only validated `UPSTREAM` snapshots and entities. A fixture or failed fetch cannot replace the runtime catalog through that production import path. The existing offline parser baseline is retained for tests and tooling, with an explicit fixture label.

## Determinism

`PokerogueManifest.stableStringify()` sorts object keys recursively and excludes `importedAt`, `importTimestamp`, and `timestamp`. The manifest hash and canonical content hash use that representation. Array ordering remains meaningful, so canonical collection producers must order records consistently (normally by stable ID) before publishing a serialized package. Raw source strings retain their exact source hash.

## Compatibility and existing formats

Compatibility checks cover schema support, runtime minimum/maximum, provenance, snapshot pinning, override scope/target, and required extensions. Project and screen JSON formats and `SceneModel` are unchanged; the content contract is an adjacent data layer and does not migrate editor documents. Content downloader, signing/install, save migration, modes, maps, encounters, and battle porting remain future work.

## Version dimensions

This phase models `schemaVersion`, `contentVersion`, and runtime compatibility metadata as distinct fields. It does not claim a complete `runtimeVersion` release process or implement `saveVersion`; save compatibility remains outside this phase.

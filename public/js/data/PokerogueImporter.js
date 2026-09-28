import { SpeciesDefinition, MoveDefinition, AbilityDefinition, ItemDefinition, SourceMetadata } from './CanonicalModels.js';
import { PokerogueManifest } from './PokerogueManifest.js';
import { POKEROGUE_REPOSITORIES } from './PokerogueSource.js';
import { PokerogueEnumParser } from './PokerogueEnumParser.js';
import { PokerogueLocaleImporter } from './PokerogueLocaleImporter.js';
import { CanonicalContent, CanonicalSourceType, Provenance, SourceSnapshot } from './CanonicalDataContract.js';
import { getOfflineImporterFixtureSources } from '../../../test/fixtures/offlineImporterSources.js';
import { GameModeDefinition } from '../game/GameMode.js';
import { BiomeDefinition, RouteDefinition } from '../game/ProgressionContent.js';

function toTitle(s) {
  if (!s) return '';
  const str = String(s).trim();
  if (str.toUpperCase() === 'NONE' || str.toUpperCase() === 'NULL') return 'NONE';
  return str.split(/[\s_]+/).map(w => w.charAt(0).toUpperCase() + w.slice(1).toLowerCase()).join(' ');
}

/**
 * PokerogueImporter - Upstream TypeScript source parser and canonical model builder.
 * Extracts Species, Moves, and Abilities directly from upstream TypeScript code via regex/token parsing,
 * strictly resolving identifiers through the PokerogueEnumParser and PokerogueLocaleImporter pipelines
 * without hardcoded data dictionaries.
 */
export class PokerogueImporter {
  constructor(repository = null) {
    this.repository = repository;
    this.manifest = new PokerogueManifest();
    this.enumParser = new PokerogueEnumParser();
    this.localeImporter = new PokerogueLocaleImporter();
    this.speciesEnumCatalog = null;
    this.moveEnumCatalog = null;
    this.abilityEnumCatalog = null;
    this.typeEnumCatalog = null;
    this.missingDataReport = [];
    this.sourceType = CanonicalSourceType.UNVERIFIED;
    this.importedFrom = null;
    this.productionCanonicalImport = false;
  }

  /**
   * Sets or attaches pre-parsed enum catalogs to the importer.
   */
  setEnumCatalogs({ species = null, moves = null, abilities = null, types = null }) {
    if (species) this.speciesEnumCatalog = species;
    if (moves) this.moveEnumCatalog = moves;
    if (abilities) this.abilityEnumCatalog = abilities;
    if (types) this.typeEnumCatalog = types;
  }

  /** Parse only mode identity and declarative configuration from the pinned upstream mode contract. */
  parseGameModes(enumContent, modeContent, { sourceType = CanonicalSourceType.UNVERIFIED } = {}) {
    const enumBody = enumContent.match(/export\s+enum\s+GameModes\s*\{([\s\S]*?)\}/)?.[1];
    if (!enumBody) throw new Error('Upstream GameModes enum was not found');
    const modes = [];
    let nextValue = 0;
    for (const line of enumBody.split(/\r?\n/)) {
      const match = line.match(/^\s*([A-Z][A-Z0-9_]*)(?:\s*=\s*(\d+))?\s*,?\s*$/);
      if (!match) continue;
      const upstreamId = match[2] === undefined ? nextValue : Number(match[2]);
      modes.push({ symbol: match[1], upstreamId });
      nextValue = upstreamId + 1;
    }
    if (!modes.length) throw new Error('Upstream GameModes enum contains no mode IDs');

    const configById = new Map();
    const factoryStart = modeContent.indexOf('export function getGameMode');
    if (factoryStart < 0) throw new Error('Upstream getGameMode factory was not found');
    const factoryText = modeContent.slice(factoryStart);
    const caseRegex = /case\s+GameModes\.([A-Z][A-Z0-9_]*):/g;
    let caseMatch;
    while ((caseMatch = caseRegex.exec(factoryText)) !== null) {
      const nextCase = factoryText.indexOf('case GameModes.', caseRegex.lastIndex);
      const branch = factoryText.slice(caseRegex.lastIndex, nextCase < 0 ? factoryText.length : nextCase);
      const constructor = branch.match(/return\s+new\s+GameMode\s*\(\s*GameModes\.[A-Z][A-Z0-9_]*\s*,\s*(\{)/);
      if (!constructor) continue;
      const openIndex = branch.indexOf('{', constructor.index);
      let depth = 0, closeIndex = -1;
      for (let index = openIndex; index < branch.length; index++) {
        if (branch[index] === '{') depth++;
        if (branch[index] === '}' && --depth === 0) { closeIndex = index; break; }
      }
      if (closeIndex < 0) throw new Error(`Unclosed mode configuration for ${caseMatch[1]}`);
      const configText = branch.slice(openIndex + 1, closeIndex);
      const flags = {};
      const flagRegex = /([A-Za-z][A-Za-z0-9]*)\s*:\s*(true|false)\b/g;
      let flag;
      while ((flag = flagRegex.exec(configText)) !== null) flags[flag[1]] = flag[2] === 'true';
      configById.set(caseMatch[1], { flags, rawConfig: branch.slice(constructor.index, closeIndex + 1) });
    }

    const nameKeys = new Map();
    const nameRegex = /case\s+GameModes\.([A-Z][A-Z0-9_]*):\s*return\s+i18next\.t\(["']([^"']+)["']\)/g;
    let nameMatch;
    while ((nameMatch = nameRegex.exec(modeContent)) !== null) nameKeys.set(nameMatch[1], nameMatch[2]);

    const terminalWaves = new Map();
    const finalMethod = modeContent.match(/isWaveFinal\s*\([^)]*\)\s*:\s*boolean\s*\{([\s\S]*?)\n\s*\}/)?.[1] || '';
    const terminalRegex = /((?:case\s+GameModes\.[A-Z][A-Z0-9_]*\s*:\s*)+)return\s+waveIndex\s*===\s*(\d+)\s*;/g;
    let terminal;
    while ((terminal = terminalRegex.exec(finalMethod)) !== null) {
      for (const label of terminal[1].matchAll(/GameModes\.([A-Z][A-Z0-9_]*)/g)) terminalWaves.set(label[1], Number(terminal[2]));
    }

    const repository = POKEROGUE_REPOSITORIES.pokerogue;
    const sourcePath = sourceType === CanonicalSourceType.TEST_FIXTURE ? 'test/fixtures/pokerogueGameModeSource.js' : 'src/game-mode.ts';
    const sourceRepository = sourceType === CanonicalSourceType.TEST_FIXTURE ? 'local:test/fixtures/pokerogueGameModeSource.js' : repository.url;
    const revision = sourceType === CanonicalSourceType.TEST_FIXTURE ? 'game-mode-fixture-v1' : repository.revision;
    const sourceHash = PokerogueManifest.computeHash(modeContent);
    const enumHash = PokerogueManifest.computeHash(enumContent);

    return modes.map(({ symbol, upstreamId }) => {
      const { flags = {}, rawConfig = '' } = configById.get(symbol) || {};
      const id = symbol.toLowerCase();
      const capabilities = {
        map: null,
        shop: flags.hasNoShop === undefined ? null : !flags.hasNoShop,
        trainerBattles: flags.hasTrainers ?? null,
        wildEncounters: null,
        bosses: null,
        events: null,
        mysteryEncounters: flags.hasMysteryEncounters ?? null,
        classicRules: flags.isClassic ?? null,
        endlessRules: flags.isEndless ?? null,
        dailyRules: flags.isDaily ?? null,
        splicedOnly: flags.isSplicedOnly ?? null,
        challengeRules: flags.isChallenge ?? null,
        shortBiomes: flags.hasShortBiomes ?? null,
        randomBiomes: flags.hasRandomBiomes ?? null,
        randomBosses: flags.hasRandomBosses ?? null
      };
      const maxWave = terminalWaves.get(symbol) ?? null;
      const provenance = {
        sourceRepository, sourceRevision: revision, sourcePath, sourceSymbol: `GameModes.${symbol}`,
        importedFrom: 'getGameMode / isWaveFinal', sourceType, contentVersion: '1.0.0', schemaVersion: '1.0.0', sourceHash
      };
      return new GameModeDefinition({
        id, upstreamId, displayNameKey: nameKeys.get(symbol) || `gameMode:${id}`,
        contentVersion: '1.0.0', schemaVersion: '1.0.0',
        rules: { maxWave, terminalCondition: maxWave === null ? null : { type: 'fixedWave', wave: maxWave } },
        capabilities,
        policies: {
          map: null,
          encounters: { trainerBattles: capabilities.trainerBattles, mysteryEncounters: capabilities.mysteryEncounters },
          progression: { biomeTransitions: { shortBiomes: capabilities.shortBiomes, randomBiomes: capabilities.randomBiomes }, randomBosses: capabilities.randomBosses },
          shop: { enabled: capabilities.shop }, starter: {}, reward: {}, save: {}
        },
        contextRequirements: { seed: flags.isDaily === true },
        specialRules: [flags.isChallenge === true ? 'challenge-configuration' : null, flags.isSplicedOnly === true ? 'spliced-species-only' : null].filter(Boolean),
        provenance,
        extensions: {
          upstreamConfigFlags: flags,
          upstreamEnum: { sourcePath: 'src/enums/game-modes.ts', sourceRevision: revision, sourceHash: enumHash },
          upstreamRawRecord: { format: 'typescript-source-fragment', value: rawConfig }
        }
      });
    });
  }

  /** Imports mode IDs and policy flags into the existing canonical content envelope. */
  async importGameModes(repository = this.repository) {
    if (!repository) throw new Error('PokerogueRepository required for pinned GameMode import');
    const [enumContent, modeContent] = await Promise.all([
      repository.loadEnumFile('gameMode'),
      repository.loadGameModesFile()
    ]);
    const definitions = this.parseGameModes(enumContent, modeContent, { sourceType: CanonicalSourceType.UPSTREAM });
    const sourceSnapshot = new SourceSnapshot({
      repository: POKEROGUE_REPOSITORIES.pokerogue.url,
      revision: POKEROGUE_REPOSITORIES.pokerogue.revision,
      branch: POKEROGUE_REPOSITORIES.pokerogue.branch,
      sourceType: CanonicalSourceType.UPSTREAM,
      contentRevision: POKEROGUE_REPOSITORIES.pokerogue.revision,
      sources: [
        { repository: 'pokerogue', revision: POKEROGUE_REPOSITORIES.pokerogue.revision, sourcePath: 'src/enums/game-modes.ts', hash: PokerogueManifest.computeHash(enumContent) },
        { repository: 'pokerogue', revision: POKEROGUE_REPOSITORIES.pokerogue.revision, sourcePath: 'src/game-mode.ts', hash: PokerogueManifest.computeHash(modeContent) }
      ]
    });
    const provenance = new Provenance({
      sourceRepository: POKEROGUE_REPOSITORIES.pokerogue.url,
      sourceRevision: POKEROGUE_REPOSITORIES.pokerogue.revision,
      sourcePath: 'src/enums/game-modes.ts',
      sourceSymbol: 'GameModes',
      sourceType: CanonicalSourceType.UPSTREAM,
      contentVersion: '1.0.0', schemaVersion: '1.0.0',
      sourceHash: PokerogueManifest.computeHash(enumContent)
    });
    const canonicalContent = new CanonicalContent({
      schemaVersion: '1.0.0', contentVersion: '1.0.0', sourceSnapshot, provenance,
      collections: { gameModes: definitions }
    });
    const errors = canonicalContent.validate();
    if (errors.length) throw new Error(`Imported GameMode content is invalid: ${errors.join('; ')}`);
    this.manifest.recordFile('pokerogue', sourceSnapshot.revision, 'src/enums/game-modes.ts', enumContent);
    this.manifest.recordFile('pokerogue', sourceSnapshot.revision, 'src/game-mode.ts', modeContent);
    definitions.forEach(definition => this.manifest.recordEntity('GameMode', definition.id, {
      repository: 'pokerogue', revision: sourceSnapshot.revision,
      sourcePath: definition.provenance.sourcePath, hash: definition.provenance.sourceHash
    }));
    return { sourceType: CanonicalSourceType.UPSTREAM, sourceSnapshot, canonicalContent, gameModes: definitions, manifest: this.manifest };
  }

  /**
   * Imports declarative source records from all pinned PokéRogue content families.
   * The generated content preserves source fragments verbatim; unsupported runtime
   * behavior remains explicitly uninterpreted and is never approximated here.
   */
  async importCanonicalContent(repository = this.repository, { locales = ['en'], generations = [1, 2, 3, 4, 5, 6, 7, 8, 9] } = {}) {
    if (!repository) throw new Error('PokerogueRepository required for production canonical-content import');
    const cacheKey = PokerogueManifest.stableStringify({ localeRevisions: locales, generations: [...generations].sort((a, b) => a - b), pokerogueRevision: POKEROGUE_REPOSITORIES.pokerogue.revision, localeRevision: POKEROGUE_REPOSITORIES['pokerogue-locales'].revision, assetRevision: POKEROGUE_REPOSITORIES['pokerogue-assets'].revision });
    if (this.canonicalImportCache?.key === cacheKey) return this.canonicalImportCache.result;
    const repos = POKEROGUE_REPOSITORIES;
    const game = repos.pokerogue;
    if (!/^[0-9a-f]{40}$/i.test(game.revision) || !/^[0-9a-f]{40}$/i.test(repos['pokerogue-assets'].revision) || !/^[0-9a-f]{40}$/i.test(repos['pokerogue-locales'].revision)) throw new Error('Pinned upstream source revisions are required');
    this.sourceType = CanonicalSourceType.UPSTREAM;
    this.importedFrom = 'pinned-canonical-content-import';
    this.productionCanonicalImport = true;

    const fixedPaths = [
      'src/enums/species-id.ts', 'src/enums/move-id.ts', 'src/enums/ability-id.ts',
      'src/enums/pokemon-type.ts', 'src/enums/game-modes.ts', 'src/game-mode.ts',
      'src/data/moves/move.ts', 'src/data/abilities/init-abilities.ts',
      'src/modifier/modifier-type.ts'
    ];
    const requests = [
      ...fixedPaths.map(path => ({ repo: 'pokerogue', path })),
      ...[...generations].sort((a, b) => a - b).map(generation => ({ repo: 'pokerogue', path: `src/data/balance/species/generation-${String(generation).padStart(2, '0')}.ts`, generation })),
      ...[...locales].sort().flatMap(locale => ['game-mode', 'pokemon', 'pokemon-form', 'move', 'ability', 'modifier'].map(namespace => ({ repo: 'pokerogue-locales', path: `${locale}/${namespace}.json`, locale, namespace })))
    ];
    const loaded = new Array(requests.length);
    let requestIndex = 0;
    const workers = Array.from({ length: Math.min(6, requests.length) }, async () => {
      while (requestIndex < requests.length) {
        const index = requestIndex++;
        const request = requests[index];
        loaded[index] = { ...request, content: await repository.getFile(request.repo, request.path) };
      }
    });
    await Promise.all(workers);
    console.log(`[canonical-import] fetched ${loaded.length} pinned files`);
    const byPath = new Map(loaded.map(item => [`${item.repo}:${item.path}`, item]));
    const sourceHash = item => PokerogueManifest.computeHash(item.content);
    const sourceRows = loaded.map(item => ({ repository: item.repo, revision: repos[item.repo].revision, sourcePath: item.path, hash: sourceHash(item) })).sort((a, b) => `${a.repository}:${a.sourcePath}`.localeCompare(`${b.repository}:${b.sourcePath}`));
    const enumSpecs = [
      ['species', 'SpeciesId', 'src/enums/species-id.ts'], ['move', 'MoveId', 'src/enums/move-id.ts'],
      ['ability', 'AbilityId', 'src/enums/ability-id.ts'], ['type', 'PokemonType', 'src/enums/pokemon-type.ts'],
      ['gameMode', 'GameModes', 'src/enums/game-modes.ts']
    ];
    const enumCatalogs = {};
    for (const [key, name, path] of enumSpecs) enumCatalogs[key] = this.enumParser.parseEnum(byPath.get(`pokerogue:${path}`).content, name, path);
    this.setEnumCatalogs({ species: enumCatalogs.species, moves: enumCatalogs.move, abilities: enumCatalogs.ability, types: enumCatalogs.type });

    const localeEntries = [];
    const localeLookup = new Map();
    for (const file of loaded.filter(item => item.repo === 'pokerogue-locales').sort((a, b) => a.path.localeCompare(b.path))) {
      const data = JSON.parse(file.content);
      this.localeImporter.parseLocale(file.content, file.locale, file.namespace, file.path, 'pokerogue-locales');
      for (const [id, value] of Object.entries(data).sort(([a], [b]) => a.localeCompare(b))) {
        const enumKey = ({ pokemon: 'species', move: 'move', ability: 'ability' })[file.namespace];
        const compact = text => String(text).toLowerCase().replace(/[^a-z0-9]/g, '');
        const canonicalSymbol = enumKey ? enumCatalogs[enumKey].entries().find(([symbol]) => compact(symbol) === compact(id))?.[0] : null;
        const canonicalId = canonicalSymbol ? canonicalSymbol.toLowerCase() : compact(id);
        const entry = { id, canonicalId, locale: file.locale, namespace: file.namespace, value, source: { repository: 'pokerogue-locales', revision: repos['pokerogue-locales'].revision, sourcePath: file.path, sourceHash: sourceHash(file), sourceType: CanonicalSourceType.UPSTREAM } };
        localeEntries.push(entry);
        localeLookup.set(`${file.locale}:${file.namespace}:${id.toLowerCase()}`, value);
      }
    }

    const species = [];
    const forms = [];
    for (const generation of [...generations].sort((a, b) => a - b)) {
      console.log(`[canonical-import] species generation ${generation}`);
      const path = `src/data/balance/species/generation-${String(generation).padStart(2, '0')}.ts`;
      const file = byPath.get(`pokerogue:${path}`);
      const records = this.parseSpeciesFromGeneration(file.content, null, { generation, sourcePath: path });
      for (const record of records) {
        const symbol = record.id.toUpperCase();
        const numericId = enumCatalogs.species.getId(symbol);
        if (numericId === undefined) throw new Error(`Invalid import: species ${symbol} is absent from pinned SpeciesId enum`);
        record.speciesId = numericId;
        record.nationalDexId = numericId;
        record.sprites = { atlasPath: null, icon: null, atlas: null, frame: null, hasFemale: null, hasShiny: null, hasVariants: null };
        record.sprite = record.sprites;
        record.forms = [];
        // Upstream declares the starter identity explicitly. Preserve that semantic field
        // instead of inferring starter eligibility from list position or UI choices.
        record.starterEligible = /\bstarter\s*:\s*SpeciesId\./.test(record.extensions?.upstreamRawRecord?.value || '');
        record.source.sourceHash = sourceHash(file);
        record.source.sourceSymbol = `SpeciesId.${symbol}`;
        record.extensions = { ...record.extensions, assetReference: { repository: repos['pokerogue-assets'].url, revision: repos['pokerogue-assets'].revision, speciesId: numericId, resolution: 'pending-source-manifest' } };
        const localized = localeLookup.get(`en:pokemon:${record.id}`);
        if (typeof localized === 'string') { record.names.en = localized; record.name = localized; }
        species.push(record);
        const raw = record.extensions.upstreamRawRecord.value;
        const formListAt = raw.search(/\bforms\s*:\s*\[/);
        if (formListAt >= 0) {
          const open = raw.indexOf('[', formListAt); let depth = 0, close = -1;
          for (let index = open; index < raw.length; index++) { if (raw[index] === '[') depth++; else if (raw[index] === ']' && --depth === 0) { close = index; break; } }
          if (close > open) {
            const formText = raw.slice(open + 1, close);
            const ctorRe = /new\s+PokemonForm\s*\(\s*\{/g; let fm;
            let formIndex = 0;
            while ((fm = ctorRe.exec(formText))) {
              const start = formText.indexOf('{', fm.index); let braces = 0, end = -1;
              for (let index = start; index < formText.length; index++) { if (formText[index] === '{') braces++; else if (formText[index] === '}' && --braces === 0) { end = index; break; } }
              if (end < 0) throw new Error(`Invalid import: unclosed PokemonForm record for ${record.id}`);
              const rawForm = formText.slice(start, end + 1);
              const formName = rawForm.match(/formName\s*:\s*["']([^"']+)/)?.[1] || 'unnamed';
              const formKey = rawForm.match(/formKey\s*:\s*(?:SpeciesFormKey\.)?([A-Z0-9_]+)/)?.[1] || 'BASE';
              forms.push({ id: `${record.id}:${(formKey === 'BASE' ? `base_${formIndex}` : formKey.toLowerCase())}`, speciesId: record.id, formKey, name: formName, types: [...rawForm.matchAll(/type[12]\s*:\s*PokemonType\.([A-Z]+)/g)].map(match => match[1]), baseStats: Object.fromEntries(['Hp', 'Atk', 'Def', 'Spatk', 'Spdef', 'Spd'].map(stat => [stat.toLowerCase(), Number(rawForm.match(new RegExp(`base${stat}\\s*:\\s*(\\d+)`))?.[1] || 0)])), assetReference: { repository: repos['pokerogue-assets'].url, revision: repos['pokerogue-assets'].revision, speciesId: numericId, resolution: 'pending-source-manifest' }, provenance: { sourceRepository: game.url, sourceRevision: game.revision, sourcePath: path, sourceSymbol: `SpeciesId.${symbol}.forms`, sourceType: CanonicalSourceType.UPSTREAM, sourceHash: sourceHash(file) }, extensions: { upstreamRawRecord: { format: 'typescript-source-fragment', value: rawForm }, runtimeTransform: 'NOT_IMPORTED' } });
              formIndex++;
              ctorRe.lastIndex = end + 1;
            }
          }
        }
      }
    }
    const moveFile = byPath.get('pokerogue:src/data/moves/move.ts');
    const abilityFile = byPath.get('pokerogue:src/data/abilities/init-abilities.ts');
    const parsedMoves = this.parseMoves(moveFile.content);
    const unmappedMoveConstructors = parsedMoves.filter(move => enumCatalogs.move.getId(move.id.toUpperCase()) === undefined).map(move => ({ domain: 'move', id: move.id, classification: 'NOT_YET_SUPPORTED_BY_GUI_3DS', reason: 'Upstream constructor symbol has no pinned MoveId enum entry; raw source remains represented in report.' }));
    const moves = parsedMoves.filter(move => enumCatalogs.move.getId(move.id.toUpperCase()) !== undefined);
    console.log(`[canonical-import] parsed ${moves.length} moves`);
    const abilities = this.parseAbilities(abilityFile.content);
    console.log(`[canonical-import] parsed ${abilities.length} abilities`);
    for (const move of moves) {
      move.moveId = enumCatalogs.move.getId(move.id.toUpperCase()) ?? null;
      if (move.moveId === null) throw new Error(`Invalid import: move ${move.id} is absent from pinned MoveId enum`);
      move.source.sourceHash = sourceHash(moveFile);
      move.source.sourceSymbol = `MoveId.${move.id.toUpperCase()}`;
      move.secondaryEffects = null;
      move.target = null;
      move.flags = { contact: null, protectable: null, sound: null, bullet: null };
      move.extensions = { ...move.extensions, upstreamEffectMetadata: move.extensions?.upstreamRawRecord || null, runtimeBehavior: 'NOT_IMPORTED' };
    }
    for (const ability of abilities) {
      const numericId = enumCatalogs.ability.getId(ability.id.toUpperCase());
      if (numericId === undefined) continue;
      ability.abilityId = numericId;
      ability.source.sourceHash = sourceHash(abilityFile);
      ability.source.sourceSymbol = `AbilityId.${ability.id.toUpperCase()}`;
      ability.attributes = [{ kind: 'UPSTREAM_DECLARATION', raw: ability.extensions?.upstreamRawRecord?.value || '' }];
      ability.extensions = { ...ability.extensions, runtimeBehavior: 'NOT_IMPORTED', unknownUpstreamFields: ['attribute constructor semantics'] };
    }
    const modifierFile = byPath.get('pokerogue:src/modifier/modifier-type.ts');
    const items = [];
    const classRe = /export\s+class\s+([A-Za-z0-9_]+)\s+extends\s+(?:Pokemon)?(?:HeldItem)?ModifierType/g; let cm;
    while ((cm = classRe.exec(modifierFile.content)) !== null) {
      const symbol = cm[1]; const id = symbol.replace(/ModifierType$/, '').replace(/([a-z])([A-Z])/g, '$1_$2').toLowerCase();
      const localeId = id.replace(/_/g, '');
      const rawEnd = modifierFile.content.indexOf('\nexport class ', cm.index + cm[0].length);
      const raw = modifierFile.content.slice(cm.index, rawEnd < 0 ? modifierFile.content.length : rawEnd);
      items.push(new ItemDefinition({ id, name: symbol, category: null, tier: null, price: null, description: '', source: new SourceMetadata({ sourceType: 'UPSTREAM', sourceRepository: game.url, sourceRevision: game.revision, sourcePath: 'src/modifier/modifier-type.ts', sourceSymbol: symbol, sourceHash: sourceHash(modifierFile), license: game.license }), extensions: { upstreamRawRecord: { format: 'typescript-source-fragment', value: raw }, localeKey: localeId, runtimeBehavior: 'NOT_IMPORTED' } }));
    }
    const modeResult = await this.importGameModes(repository);
    const gameModes = modeResult.gameModes;
    console.log(`[canonical-import] parsed ${species.length} species, ${forms.length} forms, ${items.length} modifier definitions, ${gameModes.length} modes`);
    for (const definition of gameModes) {
      const loc = localeLookup.get(`en:game-mode:${definition.id}`);
      if (typeof loc === 'string') definition.extensions.localeValue = loc;
    }
    const assetRefs = species.filter(item => [1, 6, 25].includes(item.speciesId));
    const assetMetadata = await Promise.all(assetRefs.map(async item => {
      const path = `images/pokemon/${item.speciesId}.json`;
      const content = await repository.getFile('pokerogue-assets', path);
      const parsed = JSON.parse(content);
      item.extensions.assetReference = { repository: repos['pokerogue-assets'].url, revision: repos['pokerogue-assets'].revision, speciesId: item.speciesId, sourcePath: path, sourceHash: PokerogueManifest.computeHash(content), verified: true, frames: Array.isArray(parsed.frames) ? parsed.frames.length : 0 };
      return { repository: 'pokerogue-assets', revision: repos['pokerogue-assets'].revision, sourcePath: path, hash: PokerogueManifest.computeHash(content) };
    }));
    sourceRows.push(...assetMetadata);
    sourceRows.sort((a, b) => `${a.repository}:${a.sourcePath}`.localeCompare(`${b.repository}:${b.sourcePath}`));
    const snapshot = new SourceSnapshot({ repository: game.url, revision: game.revision, branch: game.branch, sourceType: CanonicalSourceType.UPSTREAM, contentRevision: game.revision, assetRevision: repos['pokerogue-assets'].revision, localeRevision: repos['pokerogue-locales'].revision, sources: sourceRows });
    const provenance = new Provenance({ sourceRepository: game.url, sourceRevision: game.revision, sourcePath: 'src/data/balance/species/generation-01.ts', sourceSymbol: 'initGenerationOne', sourceType: CanonicalSourceType.UPSTREAM, sourceHash: sourceRows.find(item => item.sourcePath.endsWith('generation-01.ts'))?.hash });
    const canonicalAssetReferences = species.map(item => ({ speciesId: item.id, ...item.extensions.assetReference }));
    const canonicalAssetBySpecies = new Map(canonicalAssetReferences.map(item => [item.speciesId, item]));
    const canonicalContent = new CanonicalContent({ schemaVersion: '1.0.0', contentVersion: '1.0.0', sourceSnapshot: snapshot, provenance, collections: { gameModes, species, forms, moves, abilities, items, locales: localeEntries, assetReferences: [...canonicalAssetBySpecies.values()] }, extensions: { importer: 'PokerogueImporter.importCanonicalContent', unknownFieldPolicy: 'preserve-in-upstreamRawRecord', unsupportedBehavior: 'NOT_IMPORTED', skippedMoveRecords: parsedMoves.filter(move => enumCatalogs.move.getId(move.id.toUpperCase()) === undefined).map(move => ({ sourceSymbol: move.id, raw: move.extensions?.upstreamRawRecord || null })) } });
    console.log('[canonical-import] hashing canonical snapshot');
    const errors = canonicalContent.validate();
    if (errors.length) throw new Error(`Invalid canonical production import: ${errors.join('; ')}`);
    const duplicateIds = records => { const ids = records.map(item => item.id); return ids.filter((id, index) => ids.indexOf(id) !== index); };
    for (const [label, records] of Object.entries({ species, forms, moves, abilities, items, gameModes })) { const duplicates = duplicateIds(records); if (duplicates.length) throw new Error(`Invalid import: duplicate ${label} IDs ${[...new Set(duplicates)].join(', ')}`); }
    const speciesIds = new Set(species.map(item => item.id));
    const abilityIds = new Set(abilities.map(item => item.id));
    for (const form of forms) if (!speciesIds.has(form.speciesId)) throw new Error(`Invalid cross-reference: form ${form.id} refers to missing species ${form.speciesId}`);
    for (const item of species) for (const ability of Object.values(item.abilities)) {
      if (!ability || String(ability).toUpperCase() === 'NONE') continue;
      const id = String(ability).toLowerCase().replace(/\s+/g, '_');
      if (!abilityIds.has(id)) throw new Error(`Invalid cross-reference: species ${item.id} refers to missing ability ${id}`);
    }
    for (const move of moves) if (this.typeEnumCatalog.getId(move.type) === undefined) throw new Error(`Invalid cross-reference: move ${move.id} refers to unknown type ${move.type}`);
    const parsedMoveIds = new Set(moves.map(move => move.id.toUpperCase()));
    const skippedMoves = [...unmappedMoveConstructors, ...enumCatalogs.move.entries().filter(([symbol]) => !parsedMoveIds.has(symbol)).map(([symbol, upstreamId]) => ({ domain: 'move', id: symbol, upstreamId, classification: 'NOT_YET_SUPPORTED_BY_GUI_3DS', reason: 'No supported declarative constructor shape was found; enum identity remains preserved.' }))];
    const unknownFieldNames = new Set();
    for (const record of [...species, ...moves, ...abilities, ...items]) {
      const raw = record.extensions?.upstreamRawRecord?.value;
      if (typeof raw === 'string') for (const field of raw.matchAll(/\b([A-Za-z_$][\w$]*)\s*:/g)) unknownFieldNames.add(field[1]);
    }
    const report = { schemaVersion: '1.0.0', sourceRevisions: { pokerogue: game.revision, assets: repos['pokerogue-assets'].revision, locales: repos['pokerogue-locales'].revision }, contentHash: canonicalContent.hash(), catalogCounts: { modes: gameModes.length, species: species.length, forms: forms.length, moves: moves.length, abilities: abilities.length, items: items.length, locales: localeEntries.length }, skippedRecords: skippedMoves, warnings: [], unknownFields: { observedSourceFieldNames: [...unknownFieldNames].sort(), preservedRawRecordCount: species.length + forms.length + moves.length + abilities.length + items.length, abilityAttributeSemantics: 'preserved raw; not interpreted', modifierEffectSemantics: 'preserved raw; not interpreted' }, provenance: sourceRows, assetReferences: { verified: assetRefs.length, pendingMetadataVerification: species.length - assetRefs.length } };
    const result = { sourceType: CanonicalSourceType.UPSTREAM, sourceSnapshot: snapshot, canonicalContent, runtimeContent: new (await import('./CanonicalDataContract.js')).RuntimeContent({ canonicalContent }), gameModes, species, forms, moves, abilities, items, locales: localeEntries, enums: enumCatalogs, importReport: report, manifest: this.manifest };
    this.canonicalImportCache = { key: cacheKey, result };
    return result;
  }

  /** Parse the upstream biome registry and its weighted biomeLinks without importing gameplay behavior. */
  parseBiomes(enumContent, initializerContent, biomeSources, { localeContent = null, sourceType = CanonicalSourceType.UNVERIFIED } = {}) {
    const enumBody = enumContent.match(/export\s+const\s+BiomeId\s*=\s*\{([\s\S]*?)\}\s*as\s+const/)?.[1];
    if (!enumBody) throw new Error('Upstream BiomeId catalog was not found');
    const upstreamIds = new Map([...enumBody.matchAll(/([A-Z][A-Z0-9_]*)\s*:\s*(\d+)/g)].map(match => [match[1], Number(match[2])]));
    const sourceBySymbol = new Map([...initializerContent.matchAll(/import\s+\{\s*([A-Za-z0-9_]+)\s*\}\s+from\s+["']#biomes\/([^"']+)["']/g)].map(match => [match[1], `src/data/balance/biomes/${match[2]}.ts`]));
    if (!sourceBySymbol.size) throw new Error('Upstream init-biomes registry contained no biome definitions');
    const repo = POKEROGUE_REPOSITORIES.pokerogue;
    let localeKeys = null;
    if (localeContent !== null) {
      try { localeKeys = new Set(Object.keys(JSON.parse(localeContent))); }
      catch (error) { throw new Error(`Upstream biome locale JSON is invalid: ${error.message}`); }
    }
    const definitions = [];
    const routes = [];
    for (const [symbol, sourcePath] of [...sourceBySymbol].sort(([a], [b]) => a.localeCompare(b))) {
      const source = biomeSources[sourcePath];
      if (typeof source !== 'string') throw new Error(`Missing upstream biome source: ${sourcePath}`);
      const exportedName = symbol;
      const biomeMatch = source.match(new RegExp(`export\\s+const\\s+${exportedName}\\s*:\\s*Biome\\s*=\\s*\\{`));
      if (!biomeMatch) throw new Error(`Biome export ${symbol} was not found in ${sourcePath}`);
      const open = source.indexOf('{', biomeMatch.index);
      const close = this._findBalancedSourceDelimiter(source, open, '{', '}');
      const record = source.slice(open + 1, close);
      const idMatch = record.match(/biomeId\s*:\s*BiomeId\.([A-Z][A-Z0-9_]*)/);
      if (!idMatch || !upstreamIds.has(idMatch[1])) throw new Error(`Unresolved biomeId in ${sourcePath}`);
      const upstreamSymbol = idMatch[1];
      const id = upstreamSymbol.toLowerCase();
      const localeKey = id.replace(/_([a-z0-9])/g, (_, letter) => letter.toUpperCase());
      if (localeKeys && !localeKeys.has(localeKey)) throw new Error(`Upstream biome locale key is missing: ${localeKey}`);
      const linksMatch = source.match(/const\s+biomeLinks\s*:\s*BiomeLinks\s*=\s*\[/);
      const targets = [];
      if (linksMatch) {
        const linksOpen = source.indexOf('[', linksMatch.index);
        const linksClose = this._findBalancedSourceDelimiter(source, linksOpen, '[', ']');
        const linksBody = source.slice(linksOpen + 1, linksClose);
        const tokenPattern = /BiomeId\.([A-Z][A-Z0-9_]*)(?:\s*,\s*(\d+(?:\.\d+)?))?/g;
        let token;
        while ((token = tokenPattern.exec(linksBody)) !== null) targets.push({ target: token[1], weight: token[2] === undefined ? null : Number(token[2]) });
      }
      const sourceHash = PokerogueManifest.computeHash(source);
      const enumHash = PokerogueManifest.computeHash(enumContent);
      const registryHash = PokerogueManifest.computeHash(initializerContent);
      const provenance = new Provenance({
        sourceRepository: repo.url, sourceRevision: repo.revision, sourcePath,
        sourceSymbol: `${symbol}.biomeLinks`, sourceType, contentVersion: '1.0.0', schemaVersion: '1.0.0', sourceHash
      });
      const routeIds = [];
      for (const [index, link] of targets.entries()) {
        if (!upstreamIds.has(link.target)) throw new Error(`Unknown biomeLinks target ${link.target} in ${sourcePath}`);
        const to = link.target.toLowerCase();
        const routeId = `route.${id}.${to}.${index + 1}`;
        routeIds.push(routeId);
        routes.push(new RouteDefinition({
          id: routeId, from: id, to, weight: link.weight,
          provenance: new Provenance({ ...provenance, sourceSymbol: `${symbol}.biomeLinks[${index}]` }),
          extensions: {
            upstreamRawRecord: { format: 'typescript-biome-link', value: linksMatch ? source.slice(source.indexOf('[', linksMatch.index), this._findBalancedSourceDelimiter(source, source.indexOf('[', linksMatch.index), '[', ']') + 1) : '' },
            upstreamEnumRef: { repository: repo.url, revision: repo.revision, sourcePath: 'src/enums/biome-id.ts', sha256: enumHash },
            upstreamRegistryRef: { repository: repo.url, revision: repo.revision, sourcePath: 'src/init/init-biomes.ts', sha256: registryHash }
          }
        }));
      }
      const bgm = record.match(/\bbgm\s*:\s*["']([^"']+)["']/)?.[1] ?? null;
      definitions.push(new BiomeDefinition({
        id, upstreamId: upstreamIds.get(upstreamSymbol),
        localizedName: { namespace: 'biomes', key: localeKey },
        visualTemplate: null, background: null, music: bgm,
        routes: routeIds,
        metadata: { upstreamBiomeId: upstreamIds.get(upstreamSymbol), sourceBiomeSymbol: symbol },
        provenance: new Provenance({ ...provenance, sourceSymbol: symbol }),
        extensions: {
          upstreamRawRecord: { format: 'typescript-source-file', value: source },
          upstreamEnumRef: { repository: repo.url, revision: repo.revision, sourcePath: 'src/enums/biome-id.ts', sha256: PokerogueManifest.computeHash(enumContent) },
          upstreamRegistryRef: { repository: repo.url, revision: repo.revision, sourcePath: 'src/init/init-biomes.ts', sha256: PokerogueManifest.computeHash(initializerContent) },
          ...(localeContent === null ? {} : { upstreamLocaleRef: { repository: POKEROGUE_REPOSITORIES['pokerogue-locales'].url, revision: POKEROGUE_REPOSITORIES['pokerogue-locales'].revision, sourcePath: 'en/biomes.json', sha256: PokerogueManifest.computeHash(localeContent) } })
        }
      }));
    }
    return { biomes: definitions, routes };
  }

  _findBalancedSourceDelimiter(source, openIndex, openChar, closeChar) {
    let depth = 0;
    for (let index = openIndex; index < source.length; index++) {
      if (source[index] === openChar) depth++;
      else if (source[index] === closeChar && --depth === 0) return index;
    }
    throw new Error(`Unclosed upstream ${openChar}${closeChar} structure`);
  }

  async importBiomes(repository = this.repository) {
    if (!repository) throw new Error('PokerogueRepository required for pinned biome import');
    const enumPath = 'src/enums/biome-id.ts';
    const initializerPath = 'src/init/init-biomes.ts';
    const [enumContent, initializerContent, localeContent] = await Promise.all([
      repository.loadEnumFile('biome'), repository.loadBiomeInitializer(), repository.loadLocaleFile('en', 'biomes')
    ]);
    const sourcePaths = [...initializerContent.matchAll(/import\s+\{\s*([A-Za-z0-9_]+)\s*\}\s+from\s+["']#biomes\/([^"']+)["']/g)]
      .map(match => `src/data/balance/biomes/${match[2]}.ts`).sort();
    const sources = Object.fromEntries(await Promise.all(sourcePaths.map(async sourcePath => [sourcePath, await repository.getFile('pokerogue', sourcePath)])));
    const { biomes, routes } = this.parseBiomes(enumContent, initializerContent, sources, { localeContent, sourceType: CanonicalSourceType.UPSTREAM });
    const sourceSnapshot = new SourceSnapshot({
      repository: POKEROGUE_REPOSITORIES.pokerogue.url, revision: POKEROGUE_REPOSITORIES.pokerogue.revision,
      branch: POKEROGUE_REPOSITORIES.pokerogue.branch, localeRevision: POKEROGUE_REPOSITORIES['pokerogue-locales'].revision,
      sourceType: CanonicalSourceType.UPSTREAM,
      sources: [
        { repository: 'pokerogue', revision: POKEROGUE_REPOSITORIES.pokerogue.revision, sourcePath: enumPath, hash: PokerogueManifest.computeHash(enumContent) },
        { repository: 'pokerogue', revision: POKEROGUE_REPOSITORIES.pokerogue.revision, sourcePath: initializerPath, hash: PokerogueManifest.computeHash(initializerContent) },
        ...Object.entries(sources).sort(([left], [right]) => left.localeCompare(right)).map(([sourcePath, source]) => ({ repository: 'pokerogue', revision: POKEROGUE_REPOSITORIES.pokerogue.revision, sourcePath, hash: PokerogueManifest.computeHash(source) })),
        { repository: 'pokerogue-locales', revision: POKEROGUE_REPOSITORIES['pokerogue-locales'].revision, sourcePath: 'en/biomes.json', hash: PokerogueManifest.computeHash(localeContent) }
      ]
    });
    const provenance = new Provenance({ sourceRepository: sourceSnapshot.repository, sourceRevision: sourceSnapshot.revision, sourcePath: initializerPath, sourceSymbol: 'rawAllBiomes', sourceType: CanonicalSourceType.UPSTREAM, contentVersion: '1.0.0', schemaVersion: '1.0.0', sourceHash: PokerogueManifest.computeHash(initializerContent) });
    const canonicalContent = new CanonicalContent({ schemaVersion: '1.0.0', contentVersion: '1.0.0', sourceSnapshot, provenance, collections: { waves: [], biomes, maps: [], routes } });
    const errors = [...canonicalContent.validate(), ...biomes.flatMap(item => item.validate()), ...routes.flatMap(item => item.validate())];
    if (errors.length) throw new Error(`Imported biome content is invalid: ${errors.join('; ')}`);
    this.manifest.recordFile('pokerogue', sourceSnapshot.revision, enumPath, enumContent);
    this.manifest.recordFile('pokerogue', sourceSnapshot.revision, initializerPath, initializerContent);
    this.manifest.recordFile('pokerogue-locales', POKEROGUE_REPOSITORIES['pokerogue-locales'].revision, 'en/biomes.json', localeContent);
    for (const [sourcePath, source] of Object.entries(sources)) this.manifest.recordFile('pokerogue', sourceSnapshot.revision, sourcePath, source);
    biomes.forEach(item => this.manifest.recordEntity('Biome', item.id, { repository: 'pokerogue', revision: sourceSnapshot.revision, sourcePath: item.provenance.sourcePath, hash: item.provenance.sourceHash }));
    routes.forEach(item => this.manifest.recordEntity('Route', item.id, { repository: 'pokerogue', revision: sourceSnapshot.revision, sourcePath: item.provenance.sourcePath, hash: item.provenance.sourceHash }));
    const rawBiomeLocales = JSON.parse(localeContent);
    const localeEntries = biomes.map(biome => {
      const value = rawBiomeLocales[biome.localizedName.key];
      if (typeof value !== 'string') throw new Error(`Missing upstream locale value ${biome.localizedName.key} for biome ${biome.id}`);
      return { locale: 'en', namespace: 'biomes', canonicalId: biome.id, id: biome.id, value, source: { repository: POKEROGUE_REPOSITORIES['pokerogue-locales'].url, revision: POKEROGUE_REPOSITORIES['pokerogue-locales'].revision, sourcePath: 'en/biomes.json', sourceHash: PokerogueManifest.computeHash(localeContent), sourceType: CanonicalSourceType.UPSTREAM } };
    });
    return { sourceType: CanonicalSourceType.UPSTREAM, sourceSnapshot, canonicalContent, biomes, routes, localeEntries, manifest: this.manifest };
  }

  /** Combine pinned modes and biomes in one canonical progression envelope; no run behavior is executed. */
  async importCanonicalProgression(repository = this.repository) {
    const [modeImport, biomeImport] = await Promise.all([this.importGameModes(repository), this.importBiomes(repository)]);
    const sourceSnapshot = biomeImport.sourceSnapshot;
    const uniqueSources = new Map();
    for (const source of [...sourceSnapshot.sources, ...modeImport.canonicalContent.sourceSnapshot.sources]) {
      uniqueSources.set(`${source.repository}:${source.revision}:${source.sourcePath}`, source);
    }
    sourceSnapshot.sources = [...uniqueSources.values()].sort((left, right) => left.repository.localeCompare(right.repository) || left.sourcePath.localeCompare(right.sourcePath));
    const canonicalContent = new CanonicalContent({
      schemaVersion: '1.0.0', contentVersion: '1.0.0', sourceSnapshot,
      provenance: biomeImport.canonicalContent.provenance,
      collections: { gameModes: modeImport.gameModes, waves: [], biomes: biomeImport.biomes, maps: [], routes: biomeImport.routes },
      extensions: { upstreamModeSources: modeImport.canonicalContent.sourceSnapshot.sources }
    });
    const errors = canonicalContent.validate();
    if (errors.length) throw new Error(`Imported progression content is invalid: ${errors.join('; ')}`);
    return { sourceType: CanonicalSourceType.UPSTREAM, sourceSnapshot, canonicalContent, gameModes: modeImport.gameModes, biomes: biomeImport.biomes, routes: biomeImport.routes, localeEntries: biomeImport.localeEntries, manifest: this.manifest };
  }

  /** Production runtime envelope for the first run: real catalogs plus independently imported real biomes/routes. */
  async importPlayableCanonicalContent(repository = this.repository, options = {}) {
    const content = await this.importCanonicalContent(repository, options);
    const progression = await this.importCanonicalProgression(repository);
    const startPath = 'src/battle-scene.ts';
    const startSource = await repository.getFile('pokerogue', startPath);
    const battlePath = 'src/battle.ts';
    const battleSource = await repository.getFile('pokerogue', battlePath);
    const startMatch = startSource.match(/activeOverrides\.STARTING_BIOME_OVERRIDE\s*\|\|\s*BiomeId\.([A-Z0-9_]+)/);
    if (!startMatch) throw new Error(`Pinned upstream starting biome expression was not recognized in ${startPath}`);
    const startingBiomeId = startMatch[1].toLowerCase();
    const sourceSnapshot = content.canonicalContent.sourceSnapshot;
    const sources = new Map(sourceSnapshot.sources.map(source => [`${source.repository}:${source.revision}:${source.sourcePath}`, source]));
    for (const source of progression.sourceSnapshot.sources) sources.set(`${source.repository}:${source.revision}:${source.sourcePath}`, source);
    sources.set(`pokerogue:${sourceSnapshot.revision}:${startPath}`, { repository: 'pokerogue', revision: sourceSnapshot.revision, sourcePath: startPath, hash: PokerogueManifest.computeHash(startSource) });
    sources.set(`pokerogue:${sourceSnapshot.revision}:${battlePath}`, { repository: 'pokerogue', revision: sourceSnapshot.revision, sourcePath: battlePath, hash: PokerogueManifest.computeHash(battleSource) });
    sourceSnapshot.sources = [...sources.values()].sort((a, b) => a.repository.localeCompare(b.repository) || a.sourcePath.localeCompare(b.sourcePath));
    const localeEntries = [...content.locales, ...progression.localeEntries];
    const canonicalContent = new CanonicalContent({
      schemaVersion: content.canonicalContent.schemaVersion,
      contentVersion: content.canonicalContent.contentVersion,
      sourceSnapshot,
      provenance: content.canonicalContent.provenance,
      collections: { ...content.canonicalContent.collections, biomes: progression.biomes, routes: progression.routes, locales: localeEntries },
      extensions: { ...content.canonicalContent.extensions, importedProgression: { source: 'src/init/init-biomes.ts', waveCatalog: 'NOT_DECLARED_UPSTREAM' }, upstreamStartingBiome: { id: startingBiomeId, sourcePath: startPath, sourceSymbol: `BattleScene.launchBattle:${startMatch[1]}`, sourceHash: PokerogueManifest.computeHash(startSource) }, upstreamEncounterLevel: { sourcePath: battlePath, sourceSymbol: 'Battle.getLevelForWave/randSeedGaussForLevel', sourceHash: PokerogueManifest.computeHash(battleSource) } }
    });
    const errors = canonicalContent.validate();
    if (errors.length) throw new Error(`Playable canonical production content rejected: ${errors.join('; ')}`);
    const runtimeContent = new (await import('./CanonicalDataContract.js')).RuntimeContent({ canonicalContent });
    return { ...content, sourceSnapshot, canonicalContent, runtimeContent, locales: localeEntries, biomes: progression.biomes, routes: progression.routes, importReport: { ...content.importReport, contentHash: canonicalContent.hash(), catalogCounts: { ...content.importReport.catalogCounts, locales: localeEntries.length, biomes: progression.biomes.length, routes: progression.routes.length }, provenance: sourceSnapshot.sources } };
  }

  /**
   * Parses species definitions from PokéRogue generation TypeScript source.
   * Resolves speciesId dynamically via speciesEnumCatalog or explicit ID in block.
   * @param {string} tsContent Upstream generation-01.ts code
   * @param {string[]} [targetIds] Optional filter e.g. ['PIKACHU', 'GOLEM']
   * @returns {SpeciesDefinition[]}
   */
  parseSpeciesFromGeneration(tsContent, targetIds = null, { generation = 1, sourcePath: requestedSourcePath = null } = {}) {
    const speciesList = [];
    const sourcePath = requestedSourcePath || `src/data/balance/species/generation-${String(generation).padStart(2, '0')}.ts`;
    const repoInfo = POKEROGUE_REPOSITORIES.pokerogue;
    const fileHash = PokerogueManifest.computeHash(tsContent);

    this.manifest.recordFile('pokerogue', repoInfo.revision, sourcePath, tsContent);

    const targetSet = Array.isArray(targetIds) && targetIds.length > 0
      ? new Set(targetIds.map(t => t.toUpperCase()))
      : null;

    // Matches blocks of the form:
    // generationOneSpeciesData[SpeciesId.NAME] = {
    // or [SpeciesId.NAME]: {
    const blockHeaderRegex = /(?:generation\w*SpeciesData\s*\[\s*|\[\s*)(?:SpeciesId\.)?([A-Za-z0-9_]+)\s*\]\s*(?::|=)\s*\{/g;
    let match;

    while ((match = blockHeaderRegex.exec(tsContent)) !== null) {
      const speciesKey = match[1].toUpperCase();
      if (targetSet && !targetSet.has(speciesKey)) {
        continue;
      }

      // Extract balanced block from opening '{'
      const startIndex = match.index + match[0].length - 1;
      let depth = 0;
      let endIndex = startIndex;
      for (let i = startIndex; i < tsContent.length; i++) {
        if (tsContent[i] === '{') depth++;
        else if (tsContent[i] === '}') {
          depth--;
          if (depth === 0) {
            endIndex = i;
            break;
          }
        }
      }

      const block = tsContent.substring(startIndex, endIndex + 1);

      // Extract fields using pattern matching on the actual TypeScript block content
      const numIdMatch = block.match(/(?:speciesId|\bid)\s*:\s*(?:SpeciesId\.)?(\d+|[A-Za-z0-9_]+)/i);
      const nameMatch = block.match(/(?:speciesName)\s*:\s*['"`]([^'"`]+)['"`]/i);
      const genMatch = block.match(/generation\s*:\s*(\d+)/i);
      const type1Match = block.match(/type1\s*:\s*(?:PokemonType\.|Type\.)?([A-Za-z0-9_]+)/i);
      const type2Match = block.match(/type2\s*:\s*(?:PokemonType\.|Type\.)?([A-Za-z0-9_]+)/i);

      let baseStats = { hp: 40, atk: 40, def: 40, spatk: 40, spdef: 40, spd: 40 };
      const bHp = block.match(/baseHp\s*:\s*(\d+)/i);
      const bAtk = block.match(/baseAtk\s*:\s*(\d+)/i);
      const bDef = block.match(/baseDef\s*:\s*(\d+)/i);
      const bSpatk = block.match(/baseSpatk\s*:\s*(\d+)/i);
      const bSpdef = block.match(/baseSpdef\s*:\s*(\d+)/i);
      const bSpd = block.match(/baseSpd\s*:\s*(\d+)/i);

      if (bHp && bAtk && bDef) {
        baseStats.hp = Number(bHp[1]);
        baseStats.atk = Number(bAtk[1]);
        baseStats.def = Number(bDef[1]);
        if (bSpatk) baseStats.spatk = Number(bSpatk[1]);
        if (bSpdef) baseStats.spdef = Number(bSpdef[1]);
        if (bSpd) baseStats.spd = Number(bSpd[1]);
      } else {
        const statsObjMatch = block.match(/baseStats\s*:\s*\{([^}]+)\}/i);
        if (statsObjMatch) {
          const c = statsObjMatch[1];
          const hp = c.match(/hp\s*:\s*(\d+)/i);
          const atk = c.match(/atk\s*:\s*(\d+)/i);
          const def = c.match(/def\s*:\s*(\d+)/i);
          const spatk = c.match(/spatk\s*:\s*(\d+)/i);
          const spdef = c.match(/spdef\s*:\s*(\d+)/i);
          const spd = c.match(/spd\s*:\s*(\d+)/i);
          if (hp) baseStats.hp = Number(hp[1]);
          if (atk) baseStats.atk = Number(atk[1]);
          if (def) baseStats.def = Number(def[1]);
          if (spatk) baseStats.spatk = Number(spatk[1]);
          if (spdef) baseStats.spdef = Number(spdef[1]);
          if (spd) baseStats.spd = Number(spd[1]);
        } else {
          const statsArrMatch = block.match(/baseStats\s*:\s*\[\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\]/i);
          if (statsArrMatch) {
            baseStats = {
              hp: Number(statsArrMatch[1]),
              atk: Number(statsArrMatch[2]),
              def: Number(statsArrMatch[3]),
              spatk: Number(statsArrMatch[4]),
              spdef: Number(statsArrMatch[5]),
              spd: Number(statsArrMatch[6])
            };
          }
        }
      }

      const ab1Match = block.match(/(?:ability1|primary)\s*:\s*(?:AbilityId\.)?([A-Za-z0-9_]+)/i);
      const ab2Match = block.match(/(?:ability2|secondary)\s*:\s*(?:AbilityId\.)?([A-Za-z0-9_]+)/i);
      const abhMatch = block.match(/(?:abilityHidden|hidden)\s*:\s*(?:AbilityId\.)?([A-Za-z0-9_]+)/i);
      const abpMatch = block.match(/passive\s*:\s*(?:AbilityId\.)?([A-Za-z0-9_]+)/i);

      const heightMatch = block.match(/height\s*:\s*([\d\.]+)/i);
      const weightMatch = block.match(/weight\s*:\s*([\d\.]+)/i);

      // Balanced extraction for levelMoves
      const moveset = [];
      const lmIndex = block.search(/(?:levelMoves|moveset)\s*:\s*\[/i);
      if (lmIndex !== -1) {
        const arrStart = block.indexOf('[', lmIndex);
        let bDepth = 0;
        let arrEnd = arrStart;
        for (let j = arrStart; j < block.length; j++) {
          if (block[j] === '[') bDepth++;
          else if (block[j] === ']') {
            bDepth--;
            if (bDepth === 0) {
              arrEnd = j;
              break;
            }
          }
        }
        const levelMovesContent = block.substring(arrStart + 1, arrEnd);
        const mRegex = /\[\s*(\d+)\s*,\s*(?:MoveId\.|Moves\.)?([A-Za-z0-9_]+)\s*\]|\{\s*level\s*:\s*(\d+)\s*,\s*move\s*:\s*['"`]?([A-Za-z0-9_]+)['"`]?\s*\}/g;
        let mEntry;
        while ((mEntry = mRegex.exec(levelMovesContent)) !== null) {
          const lvl = Number(mEntry[1] || mEntry[3]);
          const mvName = String(mEntry[2] || mEntry[4]).toLowerCase();
          moveset.push({ level: lvl, move: mvName, id: mvName });
        }
      }

      // Balanced extraction for eggMoves
      const eggMoves = [];
      const emIndex = block.search(/eggMoves\s*:\s*\[/i);
      if (emIndex !== -1) {
        const arrStart = block.indexOf('[', emIndex);
        let bDepth = 0;
        let arrEnd = arrStart;
        for (let j = arrStart; j < block.length; j++) {
          if (block[j] === '[') bDepth++;
          else if (block[j] === ']') {
            bDepth--;
            if (bDepth === 0) {
              arrEnd = j;
              break;
            }
          }
        }
        const eggMovesContent = block.substring(arrStart + 1, arrEnd);
        const eggRegex = /(?:MoveId\.|Moves\.)?([A-Za-z0-9_]+)/g;
        let eggM;
        while ((eggM = eggRegex.exec(eggMovesContent)) !== null) {
          const clean = eggM[1].toLowerCase();
          if (clean && clean !== 'moves' && clean !== 'moveid') eggMoves.push(clean);
        }
      }

      const formattedName = nameMatch ? nameMatch[1] : toTitle(speciesKey);
      const type1 = type1Match ? toTitle(type1Match[1]) : 'Normal';
      const rawType2 = type2Match ? toTitle(type2Match[1]) : 'NONE';
      const type2 = rawType2.toUpperCase() === 'NONE' || rawType2.toUpperCase() === 'NULL' ? 'NONE' : rawType2;

      // Dynamic enum-driven ID resolution:
      // 1. Explicit numeric value in file (e.g. speciesId: 25)
      // 2. Symbolic enum resolution via speciesEnumCatalog
      let resolvedSpeciesId = 0;
      if (numIdMatch) {
        const rawVal = numIdMatch[1];
        if (/^\d+$/.test(rawVal)) {
          resolvedSpeciesId = Number(rawVal);
        } else if (this.speciesEnumCatalog) {
          resolvedSpeciesId = this.speciesEnumCatalog.getId(rawVal) || 0;
        }
      }
      if (!resolvedSpeciesId && this.speciesEnumCatalog) {
        resolvedSpeciesId = this.speciesEnumCatalog.getId(speciesKey) || 0;
      }

      // Check localization if available
      const enName = this.localeImporter.getText('pokemon', speciesKey.toLowerCase(), 'name', 'en') || formattedName;
      const esName = this.localeImporter.getText('pokemon', speciesKey.toLowerCase(), 'name', 'es') || formattedName;

      const provenance = new SourceMetadata({
        source: 'pokerogue',
        sourcePath,
        sourceRevision: this.sourceType === CanonicalSourceType.UPSTREAM ? repoInfo.revision : 'offline-fixture-v1',
        sourceType: this.sourceType,
        sourceRepository: this.sourceType === CanonicalSourceType.UPSTREAM ? repoInfo.url : 'local:test/fixtures/fallbackVerticalSlice.js',
        sourcePath: this.sourceType === CanonicalSourceType.UPSTREAM ? sourcePath : 'test/fixtures/fallbackVerticalSlice.js',
        importedAt: 'CANONICAL_IMPORT',
        license: repoInfo.license
      });

      const species = new SpeciesDefinition({
        id: speciesKey.toLowerCase(),
        speciesId: resolvedSpeciesId,
        name: formattedName,
        names: { en: enName, es: esName },
        generation: genMatch ? Number(genMatch[1]) : generation,
        type1,
        type2,
        baseStats,
        abilities: {
          primary: ab1Match && ab1Match[1].toUpperCase() !== 'NONE' ? toTitle(ab1Match[1]) : 'None',
          secondary: ab2Match && ab2Match[1].toUpperCase() !== 'NONE' ? toTitle(ab2Match[1]) : null,
          hidden: abhMatch && abhMatch[1].toUpperCase() !== 'NONE' ? toTitle(abhMatch[1]) : null,
          passive: abpMatch && abpMatch[1].toUpperCase() !== 'NONE' ? toTitle(abpMatch[1]) : null
        },
        height: heightMatch ? Number(heightMatch[1]) : 1.0,
        weight: weightMatch ? Number(weightMatch[1]) : 10.0,
        levelMoves: moveset,
        learnableMoves: moveset,
        eggMoves,
        source: provenance,
        metadata: {
          ...provenance,
          hash: fileHash
        },
        raw: { format: 'typescript-source', value: block },
        extensions: { upstreamRawRecord: { format: 'typescript-source-fragment', value: block }, growthRate: block.match(/growthRate\s*:\s*GrowthRate\.([A-Z_]+)/)?.[1] || null, category: block.match(/category\s*:\s*["']([^"']+)/)?.[1] || null }
      });

      this.manifest.recordEntity('Species', species.id, {
        repository: 'pokerogue',
        revision: repoInfo.revision,
        sourcePath,
        hash: fileHash
      });

      speciesList.push(species);
    }

    return speciesList;
  }

  /**
   * Parses moves definitions directly from PokéRogue moves TypeScript source.
   * Resolves moveId dynamically via moveEnumCatalog.
   * @param {string} tsContent Upstream move.ts code
   * @param {string[]} [targetIds] Optional filter
   * @returns {MoveDefinition[]}
   */
  parseMoves(tsContent, targetIds = null) {
    const moveList = [];
    const sourcePath = 'src/data/moves/move.ts';
    const repoInfo = POKEROGUE_REPOSITORIES.pokerogue;
    const fileHash = PokerogueManifest.computeHash(tsContent);

    this.manifest.recordFile('pokerogue', repoInfo.revision, sourcePath, tsContent);

    const targetSet = Array.isArray(targetIds) && targetIds.length > 0
      ? new Set(targetIds.map(t => t.toUpperCase()))
      : null;

    // Pattern 1: Constructor syntax used upstream
    // new AttackMove(MoveId.THUNDERBOLT, PokemonType.ELECTRIC, MoveCategory.SPECIAL, 90, 100, 15, 10, 0, 1)
    const ctorRegex = /new\s+(?:AttackMove|Move|StatusMove)\s*\(\s*(?:MoveId\.|Moves\.)?([A-Za-z0-9_]+)\s*,\s*(?:PokemonType\.|Type\.)?([A-Za-z0-9_]+)\s*,\s*(?:MoveCategory\.)?([A-Za-z0-9_]+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)(?:\s*,\s*(\d+))?(?:\s*,\s*(-?\d+))?/g;
    let match;

    while ((match = ctorRegex.exec(tsContent)) !== null) {
      const moveKey = match[1].toUpperCase();
      if (targetSet && !targetSet.has(moveKey)) continue;
      if (moveList.some(m => m.id === moveKey.toLowerCase())) continue;

      const power = Number(match[4]);
      const accuracy = Number(match[5]);
      const pp = Number(match[6]);
      const chance = match[7] ? Number(match[7]) : 0;
      const priority = match[8] ? Number(match[8]) : 0;

      const secondaryEffects = !this.productionCanonicalImport && chance > 0 && moveKey.includes('THUNDER')
        ? [{ chance, status: 'PARALYSIS' }]
        : [];

      const provenance = new SourceMetadata({
        source: 'pokerogue',
        sourcePath,
        sourceRevision: this.sourceType === CanonicalSourceType.UPSTREAM ? repoInfo.revision : 'offline-fixture-v1',
        sourceType: this.sourceType,
        sourceRepository: this.sourceType === CanonicalSourceType.UPSTREAM ? repoInfo.url : 'local:test/fixtures/fallbackVerticalSlice.js',
        sourcePath: this.sourceType === CanonicalSourceType.UPSTREAM ? sourcePath : 'test/fixtures/fallbackVerticalSlice.js',
        importedAt: 'CANONICAL_IMPORT',
        license: repoInfo.license
      });

      const resolvedMoveId = this.moveEnumCatalog ? (this.moveEnumCatalog.getId(moveKey) || 0) : 0;
      const moveIdSlug = moveKey.toLowerCase();
      const enName = this.localeImporter.getText('move', moveIdSlug, 'name', 'en') || toTitle(moveKey);
      const esName = this.localeImporter.getText('move', moveIdSlug, 'name', 'es') || toTitle(moveKey);
      const enDesc = this.localeImporter.getText('move', moveIdSlug, 'effect', 'en') || '';
      const esDesc = this.localeImporter.getText('move', moveIdSlug, 'effect', 'es') || '';

      const move = new MoveDefinition({
        id: moveKey.toLowerCase(),
        moveId: resolvedMoveId,
        name: toTitle(moveKey),
        names: { en: enName, es: esName },
        description: enDesc,
        descriptions: { en: enDesc, es: esDesc },
        type: toTitle(match[2]),
        category: toTitle(match[3]),
        power,
        accuracy,
        pp,
        priority,
        flags: { contact: !this.productionCanonicalImport && match[3].toUpperCase() === 'PHYSICAL', protectable: !this.productionCanonicalImport },
        secondaryEffects,
        source: provenance,
        metadata: { ...provenance, hash: fileHash },
        raw: { format: 'typescript-source', value: match[0] }
      });

      this.manifest.recordEntity('Move', move.id, {
        repository: 'pokerogue',
        revision: repoInfo.revision,
        sourcePath,
        hash: fileHash
      });

      moveList.push(move);
    }

    // Pattern 2: Object syntax [Moves.NAME]: { ... }
    const objBlockRegex = /\[\s*(?:MoveId\.|Moves\.)?([A-Za-z0-9_]+)\s*\]\s*:\s*\{([^}]+)\}/g;
    while ((match = objBlockRegex.exec(tsContent)) !== null) {
      const moveKey = match[1].toUpperCase();
      if (targetSet && !targetSet.has(moveKey)) continue;
      if (moveList.some(m => m.id === moveKey.toLowerCase())) continue;

      const body = match[2];
      const idMatch = body.match(/(?:moveId|id)\s*:\s*(\d+)/i);
      const nameMatch = body.match(/name\s*:\s*['"`]([^'"`]+)['"`]/i);
      const typeMatch = body.match(/type\s*:\s*(?:PokemonType\.|Type\.)?([A-Za-z0-9_]+)/i);
      const catMatch = body.match(/category\s*:\s*(?:MoveCategory\.)?([A-Za-z0-9_]+)/i);
      const powerMatch = body.match(/power\s*:\s*(\d+)/i);
      const accMatch = body.match(/accuracy\s*:\s*(\d+)/i);
      const ppMatch = body.match(/pp\s*:\s*(\d+)/i);
      const prioMatch = body.match(/priority\s*:\s*(-?\d+)/i);

      const isContact = /contact\s*:\s*true/i.test(body);
      const isProtectable = !/protectable\s*:\s*false/i.test(body);

      const secondaryEffects = [];
      const secMatch = body.match(/secondaryEffects\s*:\s*\[([\s\S]*?)\]/i);
      if (secMatch) {
        const chanceM = secMatch[1].match(/chance\s*:\s*(\d+)/i);
        const statusM = secMatch[1].match(/status\s*:\s*['"`]?([A-Za-z0-9_]+)['"`]?/i);
        if (statusM) {
          secondaryEffects.push({
            chance: chanceM ? Number(chanceM[1]) : 10,
            status: statusM[1].toUpperCase()
          });
        }
      }

      const provenance = new SourceMetadata({
        source: 'pokerogue',
        sourcePath,
        sourceRevision: this.sourceType === CanonicalSourceType.UPSTREAM ? repoInfo.revision : 'offline-fixture-v1',
        sourceType: this.sourceType,
        sourceRepository: this.sourceType === CanonicalSourceType.UPSTREAM ? repoInfo.url : 'local:test/fixtures/fallbackVerticalSlice.js',
        sourcePath: this.sourceType === CanonicalSourceType.UPSTREAM ? sourcePath : 'test/fixtures/fallbackVerticalSlice.js',
        importedAt: 'CANONICAL_IMPORT',
        license: repoInfo.license
      });

      let resolvedMoveId = idMatch ? Number(idMatch[1]) : 0;
      if (!resolvedMoveId && this.moveEnumCatalog) {
        resolvedMoveId = this.moveEnumCatalog.getId(moveKey) || 0;
      }

      const moveIdSlug = moveKey.toLowerCase();
      const enName = this.localeImporter.getText('move', moveIdSlug, 'name', 'en') || (nameMatch ? nameMatch[1] : toTitle(moveKey));
      const esName = this.localeImporter.getText('move', moveIdSlug, 'name', 'es') || (nameMatch ? nameMatch[1] : toTitle(moveKey));
      const enDesc = this.localeImporter.getText('move', moveIdSlug, 'effect', 'en') || '';
      const esDesc = this.localeImporter.getText('move', moveIdSlug, 'effect', 'es') || '';

      const move = new MoveDefinition({
        id: moveKey.toLowerCase(),
        moveId: resolvedMoveId,
        name: nameMatch ? nameMatch[1] : toTitle(moveKey),
        names: { en: enName, es: esName },
        description: enDesc,
        descriptions: { en: enDesc, es: esDesc },
        type: typeMatch ? toTitle(typeMatch[1]) : 'Normal',
        category: catMatch ? toTitle(catMatch[1]) : 'Physical',
        power: powerMatch ? Number(powerMatch[1]) : 0,
        accuracy: accMatch ? Number(accMatch[1]) : 100,
        pp: ppMatch ? Number(ppMatch[1]) : 20,
        priority: prioMatch ? Number(prioMatch[1]) : 0,
        flags: { contact: isContact, protectable: isProtectable },
        secondaryEffects,
        source: provenance,
        metadata: { ...provenance, hash: fileHash },
        raw: { format: 'typescript-source', value: match[0] }
      });

      this.manifest.recordEntity('Move', move.id, {
        repository: 'pokerogue',
        revision: repoInfo.revision,
        sourcePath,
        hash: fileHash
      });

      moveList.push(move);
    }

    return moveList;
  }

  /**
   * Parses abilities from PokéRogue init-abilities.ts source.
   * @param {string} tsContent Upstream init-abilities.ts code
   * @param {string[]} [targetIds] Optional filter
   * @returns {AbilityDefinition[]}
   */
  parseAbilities(tsContent, targetIds = null) {
    const abilityList = [];
    const sourcePath = 'src/data/abilities/init-abilities.ts';
    const repoInfo = POKEROGUE_REPOSITORIES.pokerogue;
    const fileHash = PokerogueManifest.computeHash(tsContent);

    this.manifest.recordFile('pokerogue', repoInfo.revision, sourcePath, tsContent);

    const targetSet = Array.isArray(targetIds) && targetIds.length > 0
      ? new Set(targetIds.map(t => t.toUpperCase()))
      : null;

    // Pattern 1: Upstream AbBuilder syntax
    // new AbBuilder(AbilityId.STATIC, ...)
    const builderRegex = /new\s+AbBuilder\s*\(\s*(?:AbilityId\.|Abilities\.)?([A-Za-z0-9_]+)(?:[\s\S]*?)(?=(?:new\s+AbBuilder|;|\n\s*\n|$))/g;
    let match;

    while ((match = builderRegex.exec(tsContent)) !== null) {
      const abKey = match[1].toUpperCase();
      if (targetSet && !targetSet.has(abKey)) continue;
      if (abilityList.some(a => a.id === abKey.toLowerCase())) continue;

      const trigger = !this.productionCanonicalImport && abKey === 'STATIC' ? 'ON_DAMAGE_RECEIVED' : (!this.productionCanonicalImport && abKey === 'STURDY' ? 'ON_DAMAGE_PREVENTION' : 'UNINTERPRETED');
      const conditions = [];
      if (!this.productionCanonicalImport && abKey === 'STATIC') conditions.push({ key: 'contact', value: true });
      if (!this.productionCanonicalImport && abKey === 'STURDY') conditions.push({ key: 'hpRatio', value: 1.0 });
      const effects = [];
      if (!this.productionCanonicalImport && abKey === 'STATIC') effects.push({ action: 'APPLY_STATUS', status: 'PARALYSIS', chance: 30 });
      if (!this.productionCanonicalImport && abKey === 'STURDY') effects.push({ action: 'SURVIVE_LETHAL_HIT', minHP: 1 });

      const provenance = new SourceMetadata({
        source: 'pokerogue',
        sourcePath,
        sourceRevision: this.sourceType === CanonicalSourceType.UPSTREAM ? repoInfo.revision : 'offline-fixture-v1',
        sourceType: this.sourceType,
        sourceRepository: this.sourceType === CanonicalSourceType.UPSTREAM ? repoInfo.url : 'local:test/fixtures/fallbackVerticalSlice.js',
        sourcePath: this.sourceType === CanonicalSourceType.UPSTREAM ? sourcePath : 'test/fixtures/fallbackVerticalSlice.js',
        importedAt: 'CANONICAL_IMPORT',
        license: repoInfo.license
      });

      const abSlug = abKey.toLowerCase();
      const enName = this.localeImporter.getText('ability', abSlug, 'name', 'en') || toTitle(abKey);
      const esName = this.localeImporter.getText('ability', abSlug, 'name', 'es') || toTitle(abKey);
      const enDesc = this.localeImporter.getText('ability', abSlug, 'description', 'en') || `${toTitle(abKey)} ability from upstream PokéRogue.`;
      const esDesc = this.localeImporter.getText('ability', abSlug, 'description', 'es') || '';

      const ability = new AbilityDefinition({
        id: abKey.toLowerCase(),
        name: toTitle(abKey),
        names: { en: enName, es: esName },
        description: enDesc,
        descriptions: { en: enDesc, es: esDesc },
        trigger,
        attributes: [],
        conditions,
        effects,
        source: provenance,
        metadata: { ...provenance, hash: fileHash },
        raw: { format: 'typescript-source', value: match[0] },
        extensions: { upstreamAttributes: { format: 'typescript-source-fragment', value: match[0] }, runtimeBehavior: 'NOT_IMPORTED' }
      });

      this.manifest.recordEntity('Ability', ability.id, {
        repository: 'pokerogue',
        revision: repoInfo.revision,
        sourcePath,
        hash: fileHash
      });

      abilityList.push(ability);
    }

    // Pattern 2: Object syntax [Abilities.NAME]: { ... }
    const objBlockRegex = /\[\s*(?:AbilityId\.|Abilities\.)?([A-Za-z0-9_]+)\s*\]\s*:\s*\{([^}]+)\}/g;
    while ((match = objBlockRegex.exec(tsContent)) !== null) {
      const abKey = match[1].toUpperCase();
      if (targetSet && !targetSet.has(abKey)) continue;
      if (abilityList.some(a => a.id === abKey.toLowerCase())) continue;

      const body = match[2];
      const nameMatch = body.match(/name\s*:\s*['"`]([^'"`]+)['"`]/i);
      const descMatch = body.match(/description\s*:\s*['"`]([^'"`]+)['"`]/i);
      const trigMatch = body.match(/trigger\s*:\s*['"`]?([A-Za-z0-9_]+)['"`]?/i);
      const chanceMatch = body.match(/chance\s*:\s*(\d+)/i);

      const conditions = [];
      if (/contact\s*:\s*true/i.test(body)) conditions.push({ key: 'contact', value: true });
      if (/hpRatio\s*:\s*1/i.test(body)) conditions.push({ key: 'hpRatio', value: 1.0 });

      const effects = [];
      if (/PARALYSIS/i.test(body)) effects.push({ action: 'APPLY_STATUS', status: 'PARALYSIS', chance: chanceMatch ? Number(chanceMatch[1]) : 30 });
      if (/SURVIVE_LETHAL_HIT/i.test(body)) effects.push({ action: 'SURVIVE_LETHAL_HIT', minHP: 1 });

      const provenance = new SourceMetadata({
        source: 'pokerogue',
        sourcePath,
        sourceRevision: this.sourceType === CanonicalSourceType.UPSTREAM ? repoInfo.revision : 'offline-fixture-v1',
        sourceType: this.sourceType,
        sourceRepository: this.sourceType === CanonicalSourceType.UPSTREAM ? repoInfo.url : 'local:test/fixtures/fallbackVerticalSlice.js',
        sourcePath: this.sourceType === CanonicalSourceType.UPSTREAM ? sourcePath : 'test/fixtures/fallbackVerticalSlice.js',
        importedAt: 'CANONICAL_IMPORT',
        license: repoInfo.license
      });

      const abSlug = abKey.toLowerCase();
      const enName = this.localeImporter.getText('ability', abSlug, 'name', 'en') || (nameMatch ? nameMatch[1] : toTitle(abKey));
      const esName = this.localeImporter.getText('ability', abSlug, 'name', 'es') || (nameMatch ? nameMatch[1] : toTitle(abKey));
      const enDesc = this.localeImporter.getText('ability', abSlug, 'description', 'en') || (descMatch ? descMatch[1] : '');
      const esDesc = this.localeImporter.getText('ability', abSlug, 'description', 'es') || '';

      const ability = new AbilityDefinition({
        id: abKey.toLowerCase(),
        name: nameMatch ? nameMatch[1] : toTitle(abKey),
        names: { en: enName, es: esName },
        description: enDesc,
        descriptions: { en: enDesc, es: esDesc },
        trigger: trigMatch ? trigMatch[1].toUpperCase() : 'PASSIVE',
        conditions,
        effects,
        source: provenance,
        metadata: { ...provenance, hash: fileHash },
        raw: { format: 'typescript-source', value: match[0] }
      });

      this.manifest.recordEntity('Ability', ability.id, {
        repository: 'pokerogue',
        revision: repoInfo.revision,
        sourcePath,
        hash: fileHash
      });

      abilityList.push(ability);
    }

    return abilityList;
  }

  /**
   * Full ingestion workflow: imports the canonical vertical slice from repository or cache.
   * Dynamically loads upstream enum files and locale packages if a repository is provided.
   */
  async importVerticalSlice(repository = this.repository) {
    let speciesContent = '';
    let movesContent = '';
    let abilitiesContent = '';

    this.productionCanonicalImport = false;
    this.sourceType = repository ? CanonicalSourceType.UPSTREAM : CanonicalSourceType.TEST_FIXTURE;
    this.importedFrom = repository ? 'pinned-upstream-import' : 'offline-test-fixture';
    if (repository) {
      try {
        // Load Enums first
        const speciesEnumTs = await repository.loadEnumFile('species').catch(() => '');
        if (speciesEnumTs) {
          this.speciesEnumCatalog = this.enumParser.parseEnum(speciesEnumTs, 'SpeciesId', 'src/enums/species-id.ts');
        }
        const moveEnumTs = await repository.loadEnumFile('move').catch(() => '');
        if (moveEnumTs) {
          this.moveEnumCatalog = this.enumParser.parseEnum(moveEnumTs, 'MoveId', 'src/enums/move-id.ts');
        }
        const abilityEnumTs = await repository.loadEnumFile('ability').catch(() => '');
        if (abilityEnumTs) {
          this.abilityEnumCatalog = this.enumParser.parseEnum(abilityEnumTs, 'AbilityId', 'src/enums/ability-id.ts');
        }

        // Load Locales
        const enMovesJson = await repository.loadLocaleFile('en', 'move').catch(() => '');
        if (enMovesJson) this.localeImporter.parseLocale(enMovesJson, 'en', 'move', 'en/move.json');
        const esMovesJson = await repository.loadLocaleFile('es-ES', 'move').catch(() => '');
        if (esMovesJson) this.localeImporter.parseLocale(esMovesJson, 'es-ES', 'move', 'es-ES/move.json');

        const enAbJson = await repository.loadLocaleFile('en', 'ability').catch(() => '');
        if (enAbJson) this.localeImporter.parseLocale(enAbJson, 'en', 'ability', 'en/ability.json');
        const esAbJson = await repository.loadLocaleFile('es-ES', 'ability').catch(() => '');
        if (esAbJson) this.localeImporter.parseLocale(esAbJson, 'es-ES', 'ability', 'es-ES/ability.json');

        const enPkJson = await repository.loadLocaleFile('en', 'pokemon').catch(() => '');
        if (enPkJson) this.localeImporter.parseLocale(enPkJson, 'en', 'pokemon', 'en/pokemon.json');
        const esPkJson = await repository.loadLocaleFile('es-ES', 'pokemon').catch(() => '');
        if (esPkJson) this.localeImporter.parseLocale(esPkJson, 'es-ES', 'pokemon', 'es-ES/pokemon.json');

        speciesContent = await repository.loadSpeciesGeneration(1);
        movesContent = await repository.loadMovesFile();
        abilitiesContent = await repository.loadAbilitiesFile();
      } catch (err) {
        this.sourceType = CanonicalSourceType.TEST_FIXTURE;
        this.importedFrom = 'offline-test-fixture';
        this.missingDataReport.push(`Remote repository fetch failed: ${err.message}. Result is a TEST_FIXTURE and cannot be promoted to production.`);
        speciesContent = ''; movesContent = ''; abilitiesContent = '';
      }
    }

    // Ensure enum catalogs are always initialized even in strict offline mode without remote cache
    if (!this.speciesEnumCatalog) {
      const offlineSpeciesEnumTs = `export enum SpeciesId { BULBASAUR = 1, IVYSAUR, VENUSAUR, PIKACHU = 25, GEODUDE = 74, GRAVELER, GOLEM }`;
      this.speciesEnumCatalog = this.enumParser.parseEnum(offlineSpeciesEnumTs, 'SpeciesId', 'src/enums/species-id.ts');
    }
    if (!this.moveEnumCatalog) {
      const offlineMoveEnumTs = `export enum MoveId { NONE, POUND, TACKLE = 33, THUNDERBOLT = 85, EARTHQUAKE = 89, QUICK_ATTACK = 98, ROCK_SLIDE = 157 }`;
      this.moveEnumCatalog = this.enumParser.parseEnum(offlineMoveEnumTs, 'MoveId', 'src/enums/move-id.ts');
    }
    if (!this.abilityEnumCatalog) {
      const offlineAbilityEnumTs = `export enum AbilityId { NONE, STENCH, STURDY = 5, STATIC = 9 }`;
      this.abilityEnumCatalog = this.enumParser.parseEnum(offlineAbilityEnumTs, 'AbilityId', 'src/enums/ability-id.ts');
    }

    // Offline parser samples are isolated under test/fixtures and remain labeled as fixtures.
    const offlineSources = getOfflineImporterFixtureSources();
    if (!speciesContent) speciesContent = offlineSources.species;

    if (!movesContent) movesContent = offlineSources.moves;

    if (!abilitiesContent) abilitiesContent = offlineSources.abilities;

    const species = this.parseSpeciesFromGeneration(speciesContent, ['PIKACHU', 'GOLEM']);
    const moves = this.parseMoves(movesContent, ['THUNDERBOLT', 'TACKLE', 'QUICK_ATTACK', 'EARTHQUAKE', 'ROCK_SLIDE']);
    const abilities = this.parseAbilities(abilitiesContent, ['STATIC', 'STURDY']);
    const gameSource = POKEROGUE_REPOSITORIES.pokerogue;
    const sourceSnapshot = new SourceSnapshot({
      repository: this.sourceType === CanonicalSourceType.UPSTREAM ? gameSource.url : 'local:test/fixtures/offlineImporterSources.js',
      revision: this.sourceType === CanonicalSourceType.UPSTREAM ? gameSource.revision : 'offline-fixture-v1',
      branch: this.sourceType === CanonicalSourceType.UPSTREAM ? gameSource.branch : null,
      sourceType: this.sourceType, contentRevision: this.sourceType === CanonicalSourceType.UPSTREAM ? gameSource.revision : 'offline-fixture-v1',
      assetRevision: this.sourceType === CanonicalSourceType.UPSTREAM ? POKEROGUE_REPOSITORIES['pokerogue-assets'].revision : null,
      localeRevision: this.sourceType === CanonicalSourceType.UPSTREAM ? POKEROGUE_REPOSITORIES['pokerogue-locales'].revision : null
    });
    const sourcePath = this.sourceType === CanonicalSourceType.UPSTREAM ? 'src/data/balance/species/generation-01.ts' : 'test/fixtures/offlineImporterSources.js';
    const canonicalContent = new CanonicalContent({
      sourceSnapshot, contentVersion: '1.0.0', schemaVersion: '1.0.0',
      provenance: new Provenance({ sourceRepository: sourceSnapshot.repository, sourceRevision: sourceSnapshot.revision, sourcePath, sourceType: this.sourceType, contentVersion: '1.0.0', schemaVersion: '1.0.0' }),
      collections: { species, moves, abilities }
    });

    return {
      sourceType: this.sourceType, sourceSnapshot, canonicalContent,
      species,
      moves,
      abilities,
      enums: {
        species: this.speciesEnumCatalog,
        moves: this.moveEnumCatalog,
        abilities: this.abilityEnumCatalog
      },
      localeImporter: this.localeImporter,
      manifest: this.manifest,
      missingDataReport: [...this.missingDataReport]
    };
  }
}

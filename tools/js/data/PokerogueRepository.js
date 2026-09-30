import { PokerogueSource, POKEROGUE_REPOSITORIES } from './PokerogueSource.js';

/**
 * PokerogueRepository - Data access layer for upstream PokéRogue source code.
 * Implements canonical in-memory caching and path resolution for generation files,
 * moves, abilities, enums, asset atlases, and localization JSON files.
 */
export class PokerogueRepository {
  constructor() {
    /** @type {Map<string, string>} */
    this.cache = new Map();
  }

  /**
   * Generates a canonical cache key: ${repo}:${revision}:${path}
   */
  getCacheKey(repoKey, filePath) {
    const config = POKEROGUE_REPOSITORIES[repoKey];
    const revision = config ? config.revision : 'unknown';
    const cleanPath = String(filePath).replace(/^\/+/, '');
    return `${repoKey}:${revision}:${cleanPath}`;
  }

  /**
   * Directly seeds the cache with content (useful for offline tests or pre-cached bundles).
   */
  setCache(repoKey, filePath, content) {
    const key = this.getCacheKey(repoKey, filePath);
    this.cache.set(key, content);
  }

  /**
   * Checks if a file is present in the local cache.
   */
  hasCache(repoKey, filePath) {
    return this.cache.has(this.getCacheKey(repoKey, filePath));
  }

  /**
   * Retrieves content from cache or fetches from upstream.
   */
  async getFile(repoKey, filePath) {
    const key = this.getCacheKey(repoKey, filePath);
    if (this.cache.has(key)) {
      return this.cache.get(key);
    }
    const content = await PokerogueSource.fetchSourceFile(repoKey, filePath);
    this.cache.set(key, content);
    return content;
  }

  /**
   * Loads the TypeScript species file for a generation (e.g., generation-01.ts).
   * @param {number} gen 1-9
   */
  async loadSpeciesGeneration(gen = 1) {
    const padGen = String(gen).padStart(2, '0');
    const path = `src/data/balance/species/generation-${padGen}.ts`;
    return this.getFile('pokerogue', path);
  }

  async loadAllSpeciesGenerations() {
    return Promise.all(Array.from({ length: 9 }, (_, index) => this.loadSpeciesGeneration(index + 1)));
  }

  /**
   * Loads the upstream moves TypeScript file.
   */
  async loadMovesFile() {
    return this.getFile('pokerogue', 'src/data/moves/move.ts');
  }

  /**
   * Loads the upstream abilities initialization TypeScript file.
   */
  async loadAbilitiesFile() {
    return this.getFile('pokerogue', 'src/data/abilities/init-abilities.ts');
  }

  async loadModifierTypesFile() {
    return this.getFile('pokerogue', 'src/modifier/modifier-type.ts');
  }

  async loadModifierDataFile() {
    return this.getFile('pokerogue', 'src/system/modifier-data.ts');
  }

  async loadModifierPoolFile() {
    return this.getFile('pokerogue', 'src/modifier/init-modifier-pools.ts');
  }

  async loadGameModesFile() {
    return this.getFile('pokerogue', 'src/game-mode.ts');
  }

  async loadBiomeInitializer() {
    return this.getFile('pokerogue', 'src/init/init-biomes.ts');
  }

  /**
   * Loads upstream enum source files.
   * @param {'species'|'move'|'ability'|'type'|'gameMode'|'form'} enumType
   */
  async loadEnumFile(enumType) {
    const enumMap = {
      species: 'src/enums/species-id.ts',
      move: 'src/enums/move-id.ts',
      ability: 'src/enums/ability-id.ts',
      type: 'src/enums/pokemon-type.ts',
      gameMode: 'src/enums/game-modes.ts',
      biome: 'src/enums/biome-id.ts',
      form: 'src/enums/species-form-key.ts'
    };
    const path = enumMap[enumType];
    if (!path) {
      throw new Error(`Unknown enum type: ${enumType}`);
    }
    return this.getFile('pokerogue', path);
  }

  /**
   * Loads upstream localization JSON files from pokerogue-locales.
   * @param {string} localeCode 'en', 'es-ES', 'es-419'
   * @param {'move'|'ability'|'pokemon'|'battle'} namespace 
   */
  async loadLocaleFile(localeCode, namespace) {
    const cleanLocale = localeCode === 'es' ? 'es-ES' : (localeCode === 'en' ? 'en' : localeCode);
    const path = `${cleanLocale}/${namespace}.json`;
    return this.getFile('pokerogue-locales', path);
  }

  async loadLocales(localeCodes = ['en'], namespaces = ['game-mode', 'pokemon', 'pokemon-form', 'move', 'ability', 'modifier']) {
    const requests = [];
    for (const locale of [...localeCodes].sort()) {
      for (const namespace of [...namespaces].sort()) requests.push({ locale, namespace });
    }
    const values = await Promise.all(requests.map(async request => ({ ...request, content: await this.loadLocaleFile(request.locale, request.namespace) })));
    return values;
  }

  /**
   * Loads the TexturePacker JSON asset atlas for a Pokemon species.
   * @param {number|string} speciesId 
   */
  async loadPokemonAssetMetadata(speciesId) {
    const path = `images/pokemon/${speciesId}.json`;
    return this.getFile('pokerogue-assets', path);
  }
}

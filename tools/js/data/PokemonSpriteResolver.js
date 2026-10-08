import { POKEROGUE_REPOSITORIES, PokerogueSource } from './PokerogueSource.js';

/**
 * PokemonSpriteResolver - Resolves real sprite and atlas assets from pokerogue-assets.
 * Answers the 5 core asset provenance questions without inventing fictitious paths:
 * 1. Which asset corresponds?
 * 2. Where does it originate?
 * 3. What revision produced it?
 * 4. Does it physically exist?
 * 5. What format and dimensions does it have?
 */
export class PokemonSpriteResolver {
  constructor(repository = null) {
    this.repository = repository;
    this.repoConfig = POKEROGUE_REPOSITORIES['pokerogue-assets'];

    // Physical metadata must be supplied by the importer/verifier. No built-in
    // example can stand in for a production sprite or checksum.
    this.registry = new Map();
  }

  /**
   * Resolves the asset record for a given Pokémon national dex ID.
   * Answers the 5 core questions and reports failures without inventing fictitious paths.
   * @param {number|string} speciesId 
   * @returns {Object}
   */
  resolvePokemonSprite(speciesId) {
    const numId = Number(speciesId);
    const entry = this.registry.get(numId);

    if (!entry) {
      return {
        exists: false,
        speciesId: numId,
        error: `Asset for species #${numId} is not indexed or does not exist in pokerogue-assets`,
        sourceRepository: this.repoConfig.url,
        sourceRevision: this.repoConfig.revision,
        assetPaths: null,
        target3DS: null
      };
    }

    return {
      exists: entry.physicalVerified === true,
      status: entry.physicalVerified === true ? 'AVAILABLE' : 'UNVERIFIED',
      speciesId: numId,
      name: entry.name,
      sourceRepository: this.repoConfig.url,
      sourceRevision: this.repoConfig.revision,
      assetPaths: {
        json: entry.jsonPath,
        image: entry.imagePath,
        rawJsonUrl: PokerogueSource.buildRawUrl('pokerogue-assets', entry.jsonPath),
        rawImageUrl: PokerogueSource.buildRawUrl('pokerogue-assets', entry.imagePath)
      },
      jsonHash: entry.jsonHash,
      format: entry.format,
      colorDepth: entry.colorDepth,
      dimensions: { ...entry.dimensions },
      target3DS: { ...entry.target3DS }
    };
  }

  /**
   * Returns imported species IDs; membership alone does not prove physical availability.
   */
  getIndexedSpeciesIds() {
    return Array.from(this.registry.keys());
  }

  /**
   * Registers explicit imported metadata as UNVERIFIED until a physical verifier is connected.
   * @param {Object} entry 
   */
  registerPokemonSprite(entry) {
    if (!entry || entry.speciesId === undefined || entry.speciesId === null) {
      throw new Error('Invalid pokemon sprite registration: missing speciesId');
    }
    const numId = Number(entry.speciesId);
    if (!Number.isInteger(numId) || numId <= 0) {
      throw new Error(`Invalid pokemon sprite registration: speciesId must be positive integer, got ${entry.speciesId}`);
    }
    if(entry.physicalVerified===true) throw new Error('Caller assertion is not physical verification; connect the asset verifier');
    if(entry.sourceType==='TEST_FIXTURE') throw new Error('Production sprite resolver rejects TEST_FIXTURE');
    const safePath=value=>typeof value==='string' && value.length>0 && !value.startsWith('/') && !value.includes('\\') && !value.split('/').some(part=>!part || part==='.' || part==='..');
    if(!safePath(entry.jsonPath) || !safePath(entry.imagePath) || !safePath(entry.target3DS?.t3xPath) ||
       !/^sha256:[0-9a-f]{64}$/.test(entry.jsonHash || '') ||
       !Number.isInteger(entry.dimensions?.width) || entry.dimensions.width<=0 ||
       !Number.isInteger(entry.dimensions?.height) || entry.dimensions.height<=0 ||
       typeof entry.format!=='string' || !entry.format || typeof entry.colorDepth!=='string' || !entry.colorDepth ||
       typeof entry.target3DS?.format!=='string' || !entry.target3DS.format)
      throw new Error('Sprite registration requires imported paths, SHA-256, format and dimensions');
    this.registry.set(numId, {
      ...entry, speciesId:numId, dimensions:{...entry.dimensions}, target3DS:{...entry.target3DS},
      physicalVerified:false
    });
    return true;
  }
}

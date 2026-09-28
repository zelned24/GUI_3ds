import { GameModeRegistry } from './GameMode.js';
import { MapNode, ProgressionContentRegistry } from './ProgressionContent.js';
import { SpeciesDefinition } from '../data/CanonicalModels.js';
import { PokemonBattleData } from '../battle/BattleState.js';

function hashSeed(value) {
  let hash = 2166136261;
  for (const char of String(value)) hash = Math.imul(hash ^ char.charCodeAt(0), 16777619);
  return hash >>> 0;
}

function randomAt(seed, salt) {
  let value = (hashSeed(seed) ^ hashSeed(salt)) >>> 0;
  value = (Math.imul(value, 1664525) + 1013904223) >>> 0;
  return value / 0x100000000;
}

function requireUpstream(record, domain) {
  const provenance = record?.provenance || record?.source || record?.metadata;
  if (provenance?.sourceType !== 'UPSTREAM') throw new Error(`${domain} must come from pinned UPSTREAM content`);
}

export class RunState {
  constructor({ modeId, seed, starter, biome, node, wave = 1, money = 0 }) {
    if (!modeId || !Number.isSafeInteger(seed)) throw new Error('RunState requires modeId and an integer seed');
    this.id = `run-${hashSeed(`${modeId}:${seed}`).toString(16).padStart(8, '0')}`;
    this.modeId = modeId;
    this.seed = seed >>> 0;
    this.currentWave = wave;
    this.currentBiomeId = biome.id;
    this.currentMapNodeId = node.id;
    this.party = [{ speciesId: starter.id, formId: null, level: 5 }];
    this.money = money;
    this.progression = { completedWaves: wave - 1, nodeType: node.type };
    Object.freeze(this.party[0]);
    Object.freeze(this.party);
    Object.freeze(this.progression);
  }
}

export class ResolvedPokemon {
  constructor({ species, form = null, locale, localeEntries, assetReference }) {
    requireUpstream(species, 'Species');
    const localizedName = localeEntries.find(entry => entry.locale === locale && entry.namespace === 'pokemon' && entry.canonicalId === species.id)?.value;
    if (typeof localizedName !== 'string' || !localizedName) throw new Error(`Missing ${locale} locale for species ${species.id}`);
    if (form) requireUpstream(form, 'Form');
    this.speciesId = species.id;
    this.formId = form?.id || null;
    this.name = localizedName;
    this.level = null;
    this.types = form?.types || species.types;
    this.assetReference = assetReference || form?.assetReference || species.extensions?.assetReference || null;
    this.assetStatus = this.assetReference?.verified === true ? 'SOURCE_METADATA_VERIFIED_PHYSICAL_PENDING' : 'PENDING_SOURCE_METADATA_AND_PHYSICAL_ASSET';
    this.visualState = { formId: this.formId, shiny: false, facing: 'front' };
    this.provenance = species.provenance || species.source || species.metadata;
    this.hp = null;
    this.status = null;
  }
}

/** Small deterministic, production-content-only first encounter resolver. It is not PokéRogue's complete pool/rarity algorithm. */
export class EncounterResolver {
  /** Select only the upstream biome-pool member. Random draws must come from the caller's real RNG source. */
  selectPoolMember({ biome, timeOfDay, boss = false, tierRoll = null, memberIndex = null, forcedTier = null, species, sourceRef }) {
    requireUpstream(biome, 'Biome');
    const tierOrder = ['common', 'uncommon', 'rare', 'super_rare', 'ultra_rare', 'boss', 'boss_rare', 'boss_super_rare', 'boss_ultra_rare'];
    if (!['dawn', 'day', 'dusk', 'night'].includes(timeOfDay)) throw new Error(`Unsupported upstream encounter time-of-day: ${timeOfDay}`);
    if (!sourceRef || sourceRef.sourcePath !== 'src/field/arena.ts' || sourceRef.sourceType !== 'UPSTREAM') throw new Error('Pinned upstream Arena.randomSpecies provenance is required');
    const rollMax = boss ? 64 : 512;
    let tier;
    if (forcedTier !== null) {
      tier = String(forcedTier).toLowerCase();
      if (!tierOrder.includes(tier)) throw new Error(`Invalid forced upstream biome pool tier: ${forcedTier}`);
    } else {
      if (!Number.isSafeInteger(tierRoll) || tierRoll < 0 || tierRoll >= rollMax) throw new Error(`Upstream ${boss ? 'boss' : 'non-boss'} tier roll must be an integer in [0, ${rollMax})`);
      if (boss) tier = tierRoll >= 20 ? 'boss' : tierRoll >= 6 ? 'boss_rare' : tierRoll >= 1 ? 'boss_super_rare' : 'boss_ultra_rare';
      else tier = tierRoll >= 156 ? 'common' : tierRoll >= 32 ? 'uncommon' : tierRoll >= 6 ? 'rare' : tierRoll >= 1 ? 'super_rare' : 'ultra_rare';
    }
    const requestedTier = tier;
    const poolFor = selectedTier => [...(biome.encounterPools?.[selectedTier]?.all || []), ...(biome.encounterPools?.[selectedTier]?.[timeOfDay] || [])];
    let pool = poolFor(tier);
    while (!pool.length && tierOrder.indexOf(tier) > 0) {
      tier = tierOrder[tierOrder.indexOf(tier) - 1];
      pool = poolFor(tier);
    }
    if (!pool.length) throw new Error(`UNSUPPORTED_UPSTREAM_FALLBACK: biome ${biome.id} has no pool members at ${timeOfDay}; Arena.randomSpecies falls back to the global catchable species catalog`);
    let index = null;
    if (pool.length > 1) {
      if (!Number.isSafeInteger(memberIndex) || memberIndex < 0 || memberIndex >= pool.length) throw new Error(`Upstream member draw must be an integer in [0, ${pool.length})`);
      index = memberIndex;
    }
    const speciesId = pool[index ?? 0];
    if (!(species || []).some(item => item.id === speciesId)) throw new Error(`Biome pool references ${speciesId}, which is missing from the supplied canonical species snapshot`);
    return Object.freeze({ speciesId, biomeId: biome.id, timeOfDay, isBossPool: Boolean(boss), requestedTier, selectedTier: tier, poolIndex: index, poolSize: pool.length, selectionStage: 'UPSTREAM_POOL_MEMBER_ONLY', source: { ...sourceRef } });
  }

  resolve({ mode, wave, biome, mapNode, seed, species }) {
    if (!mode || !wave || !biome || !mapNode || !Number.isSafeInteger(seed)) throw new Error('Encounter resolution requires mode, wave, biome, map node, and integer seed');
    if (mode.supports('wildEncounters') === false) throw new Error(`GameMode ${mode.id} does not support wild encounters`);
    if (!biome.provenance?.sourceType || biome.provenance.sourceType !== 'UPSTREAM') throw new Error(`Biome ${biome.id} is not pinned UPSTREAM content`);
    const candidates = species.filter(item => item?.source?.sourceType === 'UPSTREAM' || item?.provenance?.sourceType === 'UPSTREAM').sort((a, b) => a.id.localeCompare(b.id));
    if (!candidates.length) throw new Error('No UPSTREAM species are available to resolve an encounter');
    const selected = candidates[Math.floor(randomAt(seed, `${mode.id}:${wave.waveNumber}:${biome.id}:${mapNode.id}`) * candidates.length)];
    if (mode.id !== 'classic') throw new Error(`Encounter level adaptation is not yet supported for GameMode ${mode.id}`);
    const difficultyWave = wave.waveNumber; // pinned GameMode.getWaveForDifficulty default branch for Classic
    const baseLevel = 1 + difficultyWave / 2 + Math.pow(difficultyWave / 25, 2);
    const deviation = 10 / difficultyWave;
    let randomSum = 0;
    for (let index = 0; index < Math.ceil(deviation); index++) randomSum += randomAt(seed, `level:${wave.waveNumber}:${index}`);
    const level = Math.max(Math.round(baseLevel + Math.abs(randomSum / deviation)), 1);
    return Object.freeze({ speciesId: selected.id, formId: null, level, trainerId: null, modifiers: [], encounterType: 'wild', sourceSpecies: selected, resolutionPolicy: 'MINIMAL_SEEDED_UPSTREAM_CATALOG_SELECTION', levelPolicy: 'UPSTREAM_GET_LEVEL_FOR_WAVE_SEEDED_ADAPTATION', levelSource: wave.levelSource });
  }
}

export class PresentationContext {
  constructor({ run, mode, wave, biome, node, playerPokemon, enemyPokemon }) {
    this.runId = run.id;
    this.modeId = mode.id;
    this.wave = wave.waveNumber;
    this.biome = { id: biome.id, name: biome.localizedName?.resolved || null, background: biome.background, music: biome.music };
    this.mapNode = { id: node.id, type: node.type };
    this.playerPokemon = playerPokemon;
    this.enemyPokemon = enemyPokemon;
    this.money = run.money;
    this.commandMenu = ['fight', 'pokemon', 'bag', 'run'];
  }
}

/** Data-only template binding. The template is immutable and independent of species/run state. */
export class SceneTemplate {
  constructor(id = 'battle.default') {
    this.id = id;
    this.slots = Object.freeze(['playerPokemon', 'enemyPokemon', 'biome', 'wave', 'money', 'commandMenu']);
    Object.freeze(this);
  }
  evaluate(context) {
    const bindings = {};
    for (const slot of this.slots) {
      if (!(slot in context)) throw new Error(`SceneTemplate ${this.id} missing binding ${slot}`);
      bindings[slot] = context[slot];
    }
    return Object.freeze({ templateId: this.id, bindings: Object.freeze(bindings) });
  }
}

export class FirstRunFlow {
  constructor({ runtimeContent, localeEntries, locale = 'en', assetReferences = [] }) {
    if (!runtimeContent?.resolve) throw new Error('FirstRunFlow requires production RuntimeContent');
    this.resolved = runtimeContent.resolve();
    if (this.resolved.sourceSnapshot?.sourceType !== 'UPSTREAM') throw new Error('FirstRunFlow rejects fixture/non-UPSTREAM production content');
    this.extensions = runtimeContent.canonicalContent?.extensions || {};
    this.localeEntries = localeEntries || this.resolved.collections.locales || [];
    this.locale = locale;
    this.assetReferences = assetReferences.length ? assetReferences : (this.resolved.collections.assetReferences || []);
    this.modes = new GameModeRegistry(this.resolved.collections.gameModes || []);
    this.progression = new ProgressionContentRegistry();
    this.progression.loadCanonicalContent({ collections: {
      gameModes: this.resolved.collections.gameModes || [],
      waves: this.resolved.collections.waves || [],
      biomes: this.resolved.collections.biomes || [],
      maps: this.resolved.collections.maps || [],
      routes: this.resolved.collections.routes || []
    } });
    this.encounters = new EncounterResolver();
    this.sceneTemplate = new SceneTemplate();
  }

  getStarters() {
    const starters = (this.resolved.collections.species || []).filter(item => {
      const source = item.extensions?.upstreamRawRecord?.value || '';
      return item.source?.sourceType === 'UPSTREAM' && item.starterEligible === true && /\bstarter\s*:\s*SpeciesId\./.test(source);
    }).sort((a, b) => a.id.localeCompare(b.id));
    if (!starters.length) throw new Error('Pinned source contains no normalized UPSTREAM starter species');
    return starters;
  }

  start({ modeId, starterId, seed }) {
    const mode = this.modes.require(modeId);
    const starter = this.getStarters().find(item => item.id === starterId);
    if (!starter) throw new Error(`Species ${starterId} is not a canonical upstream starter`);
    const biomes = this.progression.getCollection('biomes');
    if (!biomes.length) throw new Error('Canonical RuntimeContent has no pinned biome catalog');
    const startingBiomeId = this.extensions.upstreamStartingBiome?.id;
    if (!startingBiomeId) throw new Error('Canonical content is missing the pinned upstream starting-biome reference');
    const biome = biomes.find(item => item.id === startingBiomeId);
    if (!biome) throw new Error(`Pinned starting biome ${startingBiomeId} is missing from the canonical biome catalog`);
    const localeValue = this.localeEntries.find(entry => entry.locale === this.locale && entry.namespace === 'biomes' && entry.canonicalId === biome.id)?.value;
    if (typeof localeValue !== 'string' || !localeValue) throw new Error(`Missing ${this.locale} locale for biome ${biome.id}`);
    biome.localizedName = { ...biome.localizedName, resolved: localeValue };
    const outgoing = this.progression.getCollection('routes').filter(route => route.from === biome.id);
    const node = new MapNode({ id: `biome-node:${biome.id}`, type: 'biome', connections: outgoing.map(route => `biome-node:${route.to}`), payload: { biomeId: biome.id, routeIds: outgoing.map(route => route.id) }, provenance: biome.provenance, extensions: { upstreamRoutes: outgoing } });
    const run = new RunState({ modeId, seed, starter, biome, node });
    const wave = Object.freeze({ waveNumber: run.currentWave, modeId, biomeId: biome.id, terminalWave: mode.getMaxWave(), provenance: mode.definition.provenance, sourceSymbol: 'GameMode.isWaveFinal', levelSource: this.extensions.upstreamEncounterLevel });
    const encounter = this.encounters.resolve({ mode, wave, biome, mapNode: node, seed, species: this.resolved.collections.species || [] });
    const enemy = (this.resolved.collections.species || []).find(item => item.id === encounter.speciesId);
    const form = (this.resolved.collections.forms || []).find(item => item.speciesId === starter.id) || null;
    const getAsset = record => this.assetReferences.find(item => String(item.speciesId) === String(record.speciesId || record.nationalDexId)) || record.extensions?.assetReference || null;
    const playerPokemon = new ResolvedPokemon({ species: starter, form, locale: this.locale, localeEntries: this.localeEntries, assetReference: getAsset(starter) });
    playerPokemon.level = 5;
    const enemyPokemon = new ResolvedPokemon({ species: enemy, locale: this.locale, localeEntries: this.localeEntries, assetReference: getAsset(enemy) });
    enemyPokemon.level = encounter.level;
    const resolveBridgeVitals = (record, level) => {
      const battleSpecies = new SpeciesDefinition({ ...record, learnableMoves: [], levelMoves: [] });
      const prototypeState = new PokemonBattleData(battleSpecies, level);
      return { current: prototypeState.currentHp, maximum: prototypeState.maxHp, status: prototypeState.status, source: 'PokemonBattleData prototype presentation adapter' };
    };
    playerPokemon.hp = resolveBridgeVitals(starter, playerPokemon.level);
    enemyPokemon.hp = resolveBridgeVitals(enemy, enemyPokemon.level);
    playerPokemon.status = playerPokemon.hp.status;
    enemyPokemon.status = enemyPokemon.hp.status;
    const context = new PresentationContext({ run, mode, wave, biome, node, playerPokemon, enemyPokemon });
    return Object.freeze({ run, mode, node, biome, wave, encounter, playerPokemon, enemyPokemon, presentation: context, scene: this.sceneTemplate.evaluate(context) });
  }
}

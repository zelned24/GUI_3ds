import { PokerogueManifest } from '../data/PokerogueManifest.js';
import { CanonicalContent, RuntimeContent, Provenance } from '../data/CanonicalDataContract.js';

const ID_PATTERN = /^[a-z][a-z0-9_.:-]*$/;
const clone = value => value === undefined ? {} : JSON.parse(PokerogueManifest.stableStringify(value));

class ProgressionDefinition {
  constructor(data = {}) {
    this.schemaVersion = data.schemaVersion || '1.0.0';
    this.contentVersion = data.contentVersion || '1.0.0';
    this.id = data.id || '';
    this.metadata = clone(data.metadata);
    this.provenance = data.provenance instanceof Provenance ? data.provenance : new Provenance(data.provenance);
    this.extensions = clone(data.extensions);
  }
  validate({ supportedSchemaVersions = ['1.0.0'], supportedExtensions = [] } = {}) {
    const errors = [];
    if (!ID_PATTERN.test(this.id)) errors.push(`Invalid ${this.constructor.name} id: "${this.id}"`);
    if (!supportedSchemaVersions.includes(this.schemaVersion)) errors.push(`Unsupported ${this.constructor.name} schemaVersion: ${this.schemaVersion}`);
    errors.push(...this.provenance.validate());
    for (const name of this.extensions.required || []) if (!supportedExtensions.includes(name)) errors.push(`Unknown required ${this.constructor.name} extension: ${name}`);
    return errors;
  }
  serialize() { return PokerogueManifest.stableStringify(this, 2); }
  hash() { return PokerogueManifest.computeHash(this.serialize()); }
}

/** Declarative per-wave content reference. It contains no executable gameplay or renderer behavior. */
export class WaveDefinition extends ProgressionDefinition {
  constructor(data = {}) {
    super(data);
    this.waveNumber = data.waveNumber ?? null;
    this.modeId = data.modeId ?? null;
    this.biomeId = data.biomeId ?? null;
    this.encounterType = data.encounterType ?? null;
    this.encounterPool = data.encounterPool == null ? null : clone(data.encounterPool);
    this.trainer = data.trainer == null ? null : clone(data.trainer);
    this.boss = data.boss == null ? null : clone(data.boss);
    this.event = data.event == null ? null : clone(data.event);
    this.reward = data.reward == null ? null : clone(data.reward);
    this.sceneBinding = data.sceneBinding == null ? null : clone(data.sceneBinding);
    this.specialRules = Array.isArray(data.specialRules) ? clone(data.specialRules) : [];
  }
  validate(options = {}) {
    const errors = super.validate(options);
    if (this.waveNumber !== null && (!Number.isSafeInteger(this.waveNumber) || this.waveNumber < 1)) errors.push(`WaveDefinition ${this.id} waveNumber must be a positive safe integer or null`);
    for (const field of ['modeId', 'biomeId']) if (this[field] !== null && !ID_PATTERN.test(this[field])) errors.push(`WaveDefinition ${this.id} ${field} is invalid`);
    return errors;
  }
}

/** Domain biome data; visual fields are identifiers/metadata, never renderer or scene instances. */
export class BiomeDefinition extends ProgressionDefinition {
  constructor(data = {}) {
    super(data);
    this.localizedName = data.localizedName == null ? null : clone(data.localizedName);
    this.visualTemplate = data.visualTemplate ?? null;
    this.background = data.background ?? null;
    this.music = data.music ?? null;
    this.encounterPools = data.encounterPools == null ? null : clone(data.encounterPools);
    this.trainerPools = data.trainerPools == null ? null : clone(data.trainerPools);
    this.routes = Array.isArray(data.routes) ? data.routes.map(route => clone(route)) : [];
    this.transitions = data.transitions == null ? null : clone(data.transitions);
  }
  validate(options = {}) {
    const errors = super.validate(options);
    if (!Array.isArray(this.routes)) errors.push(`BiomeDefinition ${this.id} routes must be an array`);
    return errors;
  }
}

export class MapNode {
  constructor(data = {}) {
    this.id = data.id || '';
    this.type = data.type || 'other';
    this.position = data.position === undefined ? null : clone(data.position);
    this.payload = data.payload == null ? null : clone(data.payload);
    this.connections = Array.isArray(data.connections) ? [...data.connections] : [];
    this.metadata = clone(data.metadata);
    this.provenance = data.provenance instanceof Provenance ? data.provenance : new Provenance(data.provenance);
    this.extensions = clone(data.extensions);
  }
}

export class MapEdge {
  constructor(data = {}) {
    this.from = data.from || '';
    this.to = data.to || '';
    this.conditions = data.conditions == null ? null : clone(data.conditions);
    this.weight = data.weight ?? null;
    this.metadata = clone(data.metadata);
    this.extensions = clone(data.extensions);
  }
}

/** Directed graph. Cycles are allowed: the upstream biome transition graph is not assumed acyclic. */
export class MapDefinition extends ProgressionDefinition {
  constructor(data = {}) {
    super(data);
    this.modeId = data.modeId ?? null;
    this.nodes = Array.isArray(data.nodes) ? data.nodes.map(node => node instanceof MapNode ? node : new MapNode(node)) : [];
    this.edges = Array.isArray(data.edges) ? data.edges.map(edge => edge instanceof MapEdge ? edge : new MapEdge(edge)) : [];
  }
  validate(options = {}) {
    const errors = super.validate(options);
    const nodeIds = new Set();
    for (const node of this.nodes) {
      if (!ID_PATTERN.test(node.id)) errors.push(`MapDefinition ${this.id} has invalid MapNode id: "${node.id}"`);
      if (nodeIds.has(node.id)) errors.push(`MapDefinition ${this.id} has duplicate MapNode id: ${node.id}`);
      nodeIds.add(node.id);
      if (!node.type) errors.push(`MapDefinition ${this.id} MapNode ${node.id} requires type`);
    }
    for (const node of this.nodes) for (const target of node.connections) if (!nodeIds.has(target)) errors.push(`MapDefinition ${this.id} MapNode ${node.id} connects to unknown node ${target}`);
    for (const edge of this.edges) {
      if (!nodeIds.has(edge.from)) errors.push(`MapDefinition ${this.id} MapEdge references unknown source node ${edge.from}`);
      if (!nodeIds.has(edge.to)) errors.push(`MapDefinition ${this.id} MapEdge references unknown target node ${edge.to}`);
      if (edge.weight !== null && (!Number.isFinite(edge.weight) || edge.weight < 0)) errors.push(`MapDefinition ${this.id} MapEdge weight must be a non-negative finite number or null`);
    }
    if (this.modeId !== null && !ID_PATTERN.test(this.modeId)) errors.push(`MapDefinition ${this.id} modeId is invalid`);
    return errors;
  }
}

/** Semantic source-to-target transition; used for upstream Biome.biomeLinks. */
export class RouteDefinition extends ProgressionDefinition {
  constructor(data = {}) {
    super(data);
    this.from = data.from || '';
    this.to = data.to || '';
    this.weight = data.weight ?? null;
    this.conditions = data.conditions == null ? null : clone(data.conditions);
  }
  validate(options = {}) {
    const errors = super.validate(options);
    if (!ID_PATTERN.test(this.from) || !ID_PATTERN.test(this.to)) errors.push(`RouteDefinition ${this.id} requires valid from/to IDs`);
    if (this.weight !== null && (!Number.isFinite(this.weight) || this.weight < 0)) errors.push(`RouteDefinition ${this.id} weight must be a non-negative finite number or null`);
    return errors;
  }
}

/** Validates and loads progression collections from the existing canonical/runtime envelopes. */
export class ProgressionContentRegistry {
  constructor() { this.collections = { waves: [], biomes: [], maps: [], routes: [] }; }
  loadCanonicalContent(content, options = {}) {
    const collections = content instanceof CanonicalContent ? content.collections : content?.collections;
    if (!collections || typeof collections !== 'object') throw new Error('CanonicalContent collections are required');
    const envelopeErrors = content instanceof CanonicalContent ? content.validate(options) : [];
    const staged = {
      waves: (collections.waves || []).map(item => item instanceof WaveDefinition ? item : new WaveDefinition(item)),
      biomes: (collections.biomes || []).map(item => item instanceof BiomeDefinition ? item : new BiomeDefinition(item)),
      maps: (collections.maps || []).map(item => item instanceof MapDefinition ? item : new MapDefinition(item)),
      routes: (collections.routes || []).map(item => item instanceof RouteDefinition ? item : new RouteDefinition(item))
    };
    const errors = [...envelopeErrors];
    for (const list of Object.values(staged)) {
      const ids = new Set();
      for (const item of list) {
        errors.push(...item.validate(options));
        if (ids.has(item.id)) errors.push(`Duplicate ${item.constructor.name} id: ${item.id}`);
        ids.add(item.id);
      }
    }
    const biomeIds = new Set(staged.biomes.map(item => item.id));
    const modeIds = new Set((collections.gameModes || []).map(item => String(item.id)));
    for (const wave of staged.waves) if (wave.biomeId && biomeIds.size && !biomeIds.has(wave.biomeId)) errors.push(`WaveDefinition ${wave.id} references unknown biome ${wave.biomeId}`);
    for (const wave of staged.waves) if (wave.modeId && modeIds.size && !modeIds.has(wave.modeId)) errors.push(`WaveDefinition ${wave.id} references unknown GameMode ${wave.modeId}`);
    for (const map of staged.maps) if (map.modeId && modeIds.size && !modeIds.has(map.modeId)) errors.push(`MapDefinition ${map.id} references unknown GameMode ${map.modeId}`);
    const routeIds = new Set(staged.routes.map(item => item.id));
    for (const biome of staged.biomes) for (const routeId of biome.routes) if (typeof routeId === 'string' && routeIds.size && !routeIds.has(routeId)) errors.push(`BiomeDefinition ${biome.id} references unknown route ${routeId}`);
    for (const route of staged.routes) if (biomeIds.size && (!biomeIds.has(route.from) || !biomeIds.has(route.to))) errors.push(`RouteDefinition ${route.id} references unknown biome`);
    if (errors.length) throw new Error(`Invalid progression content: ${errors.join('; ')}`);
    this.collections = staged;
    return Object.values(staged).reduce((sum, list) => sum + list.length, 0);
  }
  loadRuntimeContent(runtimeContent, options = {}) {
    if (!(runtimeContent instanceof RuntimeContent)) throw new Error('RuntimeContent instance required');
    const errors = runtimeContent.validateCompatibility(options);
    if (errors.length) throw new Error(`Runtime progression content is incompatible: ${errors.join('; ')}`);
    return this.loadCanonicalContent(runtimeContent.resolve(), options);
  }
  getCollection(name) { return (this.collections[name] || []).slice(); }
  serialize() { return PokerogueManifest.stableStringify(this.collections, 2); }
  hash() { return PokerogueManifest.computeHash(this.serialize()); }
}

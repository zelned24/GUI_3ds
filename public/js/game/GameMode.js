import { CanonicalContent, RuntimeContent } from '../data/CanonicalDataContract.js';
import { PokerogueManifest } from '../data/PokerogueManifest.js';
import { Provenance } from '../data/CanonicalDataContract.js';

const MODE_ID_PATTERN = /^[a-z][a-z0-9_]*$/;
const CAPABILITY_KEYS = new Set([
  'map', 'shop', 'trainerBattles', 'wildEncounters', 'bosses', 'events', 'mysteryEncounters',
  'classicRules', 'endlessRules', 'dailyRules', 'splicedOnly', 'challengeRules',
  'shortBiomes', 'randomBiomes', 'randomBosses'
]);

function clone(value) {
  return JSON.parse(PokerogueManifest.stableStringify(value === undefined ? {} : value));
}

export class GameModeDefinition {
  constructor(data = {}) {
    this.schemaVersion = data.schemaVersion || '1.0.0';
    this.contentVersion = data.contentVersion || '1.0.0';
    this.id = data.id || '';
    this.upstreamId = data.upstreamId ?? null;
    this.displayNameKey = data.displayNameKey || '';
    this.descriptionKey = data.descriptionKey || null;
    this.rules = clone(data.rules);
    this.rules.maxWave = data.rules?.maxWave ?? null;
    this.capabilities = clone(data.capabilities);
    this.policies = {
      map: data.policies?.map ?? null,
      encounters: clone(data.policies?.encounters),
      progression: clone(data.policies?.progression),
      shop: clone(data.policies?.shop),
      starter: clone(data.policies?.starter),
      reward: clone(data.policies?.reward),
      save: clone(data.policies?.save)
    };
    this.contextRequirements = clone(data.contextRequirements);
    this.specialRules = Array.isArray(data.specialRules) ? [...data.specialRules] : [];
    this.provenance = data.provenance instanceof Provenance ? data.provenance : new Provenance(data.provenance);
    this.extensions = clone(data.extensions);
  }

  validate({ supportedSchemaVersions = ['1.0.0'], supportedExtensions = [] } = {}) {
    const errors = [];
    if (!MODE_ID_PATTERN.test(this.id)) errors.push(`Invalid GameMode id: "${this.id}"`);
    if (!this.displayNameKey) errors.push(`GameMode ${this.id || '(unknown)'} requires displayNameKey`);
    if (!supportedSchemaVersions.includes(this.schemaVersion)) errors.push(`Unsupported GameMode schemaVersion: ${this.schemaVersion}`);
    if (this.rules.maxWave !== null && (!Number.isSafeInteger(this.rules.maxWave) || this.rules.maxWave < 1)) errors.push(`GameMode ${this.id} rules.maxWave must be a positive safe integer or null`);
    for (const [capability, value] of Object.entries(this.capabilities)) {
      if (!CAPABILITY_KEYS.has(capability)) errors.push(`Unknown GameMode capability: ${capability}`);
      if (value !== null && typeof value !== 'boolean') errors.push(`GameMode capability ${capability} must be boolean or null`);
    }
    errors.push(...this.provenance.validate());
    for (const extension of this.extensions.required || []) if (!supportedExtensions.includes(extension)) errors.push(`Unknown required GameMode extension: ${extension}`);
    return errors;
  }

  toJSON() {
    return {
      schemaVersion: this.schemaVersion,
      contentVersion: this.contentVersion,
      id: this.id,
      upstreamId: this.upstreamId,
      displayNameKey: this.displayNameKey,
      descriptionKey: this.descriptionKey,
      rules: this.rules,
      capabilities: this.capabilities,
      policies: this.policies,
      contextRequirements: this.contextRequirements,
      specialRules: this.specialRules,
      provenance: this.provenance,
      extensions: this.extensions
    };
  }

  serialize() { return PokerogueManifest.stableStringify(this, 2); }
  hash() { return PokerogueManifest.computeHash(this.serialize()); }
}

/** Read-only policy view used by domain consumers; it has no UI or GameFlow dependency. */
export class GameModePolicy {
  constructor(definition) {
    this.definition = definition instanceof GameModeDefinition ? definition : new GameModeDefinition(definition);
    Object.freeze(this);
  }
  get id() { return this.definition.id; }
  supports(capability) { return Object.hasOwn(this.definition.capabilities, capability) ? this.definition.capabilities[capability] : null; }
  getMaxWave() { return this.definition.rules.maxWave; }
  getBiomeTransitionPolicy() { return clone(this.definition.policies.progression?.biomeTransitions ?? null); }
  getEncounterPolicy() { return clone(this.definition.policies.encounters); }
  getProgressionPolicy() { return clone(this.definition.policies.progression); }
  getRule(name) { return Object.hasOwn(this.definition.rules, name) ? this.definition.rules[name] : null; }
}

export class GameModeRegistry {
  constructor(definitions = []) {
    this._definitions = new Map();
    for (const definition of definitions) this.register(definition);
  }

  register(definition, options = {}) {
    const normalized = definition instanceof GameModeDefinition ? definition : new GameModeDefinition(definition);
    const errors = normalized.validate(options);
    if (errors.length) throw new Error(`Invalid GameMode ${normalized.id}: ${errors.join('; ')}`);
    if (this._definitions.has(normalized.id)) throw new Error(`Duplicate GameMode id: ${normalized.id}`);
    this._definitions.set(normalized.id, normalized);
    return new GameModePolicy(normalized);
  }

  loadCanonicalContent(content, options = {}) {
    const gameModes = content instanceof CanonicalContent
      ? content.collections.gameModes
      : content?.collections?.gameModes;
    if (!Array.isArray(gameModes)) throw new Error('CanonicalContent.collections.gameModes must be an array');
    const staged = gameModes.map(definition => definition instanceof GameModeDefinition ? definition : new GameModeDefinition(definition));
    const errors = staged.flatMap(definition => definition.validate(options));
    if (errors.length) throw new Error(`Canonical GameMode content rejected: ${errors.join('; ')}`);
    const ids = new Set();
    for (const definition of staged) {
      if (ids.has(definition.id) || this._definitions.has(definition.id)) throw new Error(`Duplicate GameMode id: ${definition.id}`);
      ids.add(definition.id);
    }
    staged.forEach(definition => this._definitions.set(definition.id, definition));
    return staged.length;
  }

  loadRuntimeContent(runtimeContent, { runtimeVersion, ...options } = {}) {
    if (!(runtimeContent instanceof RuntimeContent)) throw new Error('RuntimeContent instance required to resolve runtime GameModes');
    const errors = runtimeContent.validateCompatibility({ runtimeVersion, ...options });
    if (errors.length) throw new Error(`Runtime GameMode content is incompatible: ${errors.join('; ')}`);
    return this.loadCanonicalContent(runtimeContent.resolve(), options);
  }

  get(id) {
    const definition = this._definitions.get(String(id));
    return definition ? new GameModePolicy(definition) : null;
  }
  require(id) {
    const result = this.get(id);
    if (!result) throw new Error(`Unknown GameMode: ${id}`);
    return result;
  }
  list() { return [...this._definitions.values()].sort((a, b) => a.id.localeCompare(b.id)); }
  serialize() { return PokerogueManifest.stableStringify(this.list(), 2); }
  hash() { return PokerogueManifest.computeHash(this.serialize()); }
}

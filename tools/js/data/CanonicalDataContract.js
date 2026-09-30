import { PokerogueManifest } from './PokerogueManifest.js';

export const CanonicalSourceType = Object.freeze({
  UPSTREAM: 'UPSTREAM', TEST_FIXTURE: 'TEST_FIXTURE', LOCAL_OVERRIDE: 'LOCAL_OVERRIDE', GENERATED: 'GENERATED', UNVERIFIED: 'UNVERIFIED'
});

const MUTABLE_REVISIONS = new Set(['main', 'master', 'beta', 'latest', 'head']);

export function stableCanonicalStringify(value, space = 0) {
  return PokerogueManifest.stableStringify(value, space);
}

export class SourceSnapshot {
  constructor(data = {}) {
    this.repository = data.repository || '';
    this.revision = data.revision || '';
    this.branch = data.branch || null;
    this.tag = data.tag || null;
    this.sourceType = data.sourceType || CanonicalSourceType.UNVERIFIED;
    this.contentRevision = data.contentRevision || this.revision;
    this.assetRevision = data.assetRevision || null;
    this.localeRevision = data.localeRevision || null;
    this.sources = Array.isArray(data.sources) ? data.sources.map(source => ({ ...source })) : [];
    this.importTimestamp = data.importTimestamp || null;
  }

  identity() {
    const { importTimestamp, ...stable } = this;
    return PokerogueManifest.computeHash(stableCanonicalStringify(stable));
  }

  validate() {
    const errors = [];
    if (!this.repository) errors.push('SourceSnapshot.repository is required');
    if (!this.revision) errors.push('SourceSnapshot.revision is required');
    if (this.sourceType === CanonicalSourceType.UPSTREAM && MUTABLE_REVISIONS.has(String(this.revision).toLowerCase())) {
      errors.push(`UPSTREAM revision must be pinned; received mutable revision "${this.revision}"`);
    }
    return errors;
  }
}

export class Provenance {
  constructor(data = {}) {
    this.sourceRepository = data.sourceRepository || data.repository || '';
    this.sourceRevision = data.sourceRevision || data.revision || '';
    this.sourcePath = data.sourcePath || '';
    this.sourceSymbol = data.sourceSymbol || null;
    this.importedFrom = data.importedFrom || null;
    this.sourceType = data.sourceType || CanonicalSourceType.UNVERIFIED;
    this.contentVersion = data.contentVersion || '1.0.0';
    this.schemaVersion = data.schemaVersion || '1.0.0';
    this.sourceHash = data.sourceHash || data.hash || null;
  }

  validate() {
    const errors = [];
    if (!Object.values(CanonicalSourceType).includes(this.sourceType)) errors.push('Provenance.sourceType is unsupported');
    if (!this.sourceRepository || !this.sourceRevision || !this.sourcePath) errors.push('Provenance requires sourceRepository, sourceRevision, and sourcePath');
    if (this.sourceType === CanonicalSourceType.UPSTREAM && MUTABLE_REVISIONS.has(String(this.sourceRevision).toLowerCase())) errors.push('UPSTREAM provenance revision must be pinned');
    if (this.sourceType === CanonicalSourceType.TEST_FIXTURE && !this.sourcePath.replace(/\\/g, '/').startsWith('test/fixtures/')) errors.push('TEST_FIXTURE provenance must point under test/fixtures/');
    return errors;
  }
}

export class CanonicalContent {
  constructor(data = {}) {
    this.schemaVersion = data.schemaVersion || '1.0.0';
    this.contentVersion = data.contentVersion || '1.0.0';
    this.sourceSnapshot = data.sourceSnapshot instanceof SourceSnapshot ? data.sourceSnapshot : new SourceSnapshot(data.sourceSnapshot);
    this.provenance = data.provenance instanceof Provenance ? data.provenance : new Provenance(data.provenance);
    this.collections = data.collections || {};
    this.extensions = data.extensions || {};
    this.requiredExtensions = Array.isArray(data.requiredExtensions) ? [...data.requiredExtensions] : [];
  }

  validate({ supportedSchemaVersions = ['1.0.0'], supportedExtensions = [] } = {}) {
    const errors = [];
    if (!supportedSchemaVersions.includes(this.schemaVersion)) errors.push(`Unsupported canonical schemaVersion: ${this.schemaVersion}`);
    errors.push(...this.sourceSnapshot.validate(), ...this.provenance.validate());
    for (const name of this.requiredExtensions) if (!supportedExtensions.includes(name)) errors.push(`Unknown required extension: ${name}`);
    if (!this.collections || typeof this.collections !== 'object') errors.push('CanonicalContent.collections must be an object');
    return errors;
  }

  serialize() { return stableCanonicalStringify(this, 2); }
  hash() { return PokerogueManifest.computeHash(this.serialize()); }
}

const ALLOWED_OVERRIDE_SCOPES = new Set(['presentation', 'assets', 'memory', 'hardware']);
export class OverrideSet {
  constructor(data = {}) {
    this.id = data.id || '3ds-default';
    this.sourceType = CanonicalSourceType.LOCAL_OVERRIDE;
    this.overrides = Array.isArray(data.overrides) ? data.overrides.map(item => ({ ...item })) : [];
  }
  validate() {
    return this.overrides.flatMap((item, index) => {
      const errors = [];
      if (!item.target || !item.path) errors.push(`Override ${index} requires target and path`);
      if (!ALLOWED_OVERRIDE_SCOPES.has(item.scope)) errors.push(`Override ${index} has invalid scope "${item.scope}"`);
      if (item.path && !/^(presentation|sprites|assets|memory|hardware)(\.|$)/.test(item.path)) errors.push(`Override ${index} cannot change gameplay or canonical content fields`);
      return errors;
    });
  }
}

export class RuntimeContent {
  constructor({ canonicalContent, overrideSet = new OverrideSet(), runtimeCompatibility = {} } = {}) {
    if (!(canonicalContent instanceof CanonicalContent)) throw new Error('RuntimeContent requires CanonicalContent');
    this.canonicalContent = canonicalContent;
    this.overrideSet = overrideSet instanceof OverrideSet ? overrideSet : new OverrideSet(overrideSet);
    this.schemaVersion = canonicalContent.schemaVersion;
    this.contentVersion = canonicalContent.contentVersion;
    this.runtimeCompatibility = { minimumRuntimeVersion: runtimeCompatibility.minimumRuntimeVersion || '0.0.0', maximumRuntimeVersion: runtimeCompatibility.maximumRuntimeVersion || null };
  }

  resolve() {
    const errors = [...this.canonicalContent.validate(), ...this.overrideSet.validate()];
    if (errors.length) throw new Error(`Invalid runtime content: ${errors.join('; ')}`);
    const collections = JSON.parse(stableCanonicalStringify(this.canonicalContent.collections));
    for (const override of this.overrideSet.overrides) {
      const [collection, entityId] = override.target.split(':');
      const entity = collections[collection]?.find?.(item => String(item.id) === entityId);
      if (!entity) throw new Error(`Override target not found: ${override.target}`);
      const parts = override.path.split('.');
      let target = entity;
      for (const part of parts.slice(0, -1)) target = target[part] ||= {};
      target[parts.at(-1)] = override.value;
    }
    return { schemaVersion: this.schemaVersion, contentVersion: this.contentVersion, sourceSnapshot: this.canonicalContent.sourceSnapshot, collections };
  }

  validateCompatibility({ runtimeVersion, supportedSchemaVersions = ['1.0.0'], supportedExtensions = [] } = {}) {
    const errors = this.canonicalContent.validate({ supportedSchemaVersions, supportedExtensions });
    errors.push(...this.overrideSet.validate());
    if (runtimeVersion && compareVersion(runtimeVersion, this.runtimeCompatibility.minimumRuntimeVersion) < 0) errors.push(`Runtime ${runtimeVersion} is below minimum ${this.runtimeCompatibility.minimumRuntimeVersion}`);
    if (runtimeVersion && this.runtimeCompatibility.maximumRuntimeVersion && compareVersion(runtimeVersion, this.runtimeCompatibility.maximumRuntimeVersion) > 0) errors.push(`Runtime ${runtimeVersion} exceeds maximum ${this.runtimeCompatibility.maximumRuntimeVersion}`);
    return errors;
  }
}

function compareVersion(left, right) {
  const parse = value => String(value).split('.').map(part => Number.parseInt(part, 10));
  const a = parse(left), b = parse(right);
  if (a.length !== 3 || b.length !== 3 || [...a, ...b].some(Number.isNaN)) throw new Error('Runtime versions must use numeric major.minor.patch format');
  for (let index = 0; index < 3; index++) if (a[index] !== b[index]) return a[index] < b[index] ? -1 : 1;
  return 0;
}

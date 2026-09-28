import { globalRNG } from './DeterministicRNG.js';

export const PRESET_TYPES = ['Animation', 'Effect', 'Composition'];

/**
 * Preset - Versioned authoring asset for animations, effects, or compositions.
 */
export class Preset {
  constructor(data = {}) {
    this.id = data.id || globalRNG.nextId('preset');
    this.name = data.name || 'Unnamed Preset';
    this.version = Math.max(1, Math.round(data.version || 1));
    this.schemaVersion = Math.max(1, Math.round(data.schemaVersion || 1));
    this.type = PRESET_TYPES.includes(data.type) ? data.type : 'Effect';
    this.data = { ...(data.data || {}) };
  }

  toJSON() {
    return {
      id: this.id,
      name: this.name,
      version: this.version,
      schemaVersion: this.schemaVersion,
      type: this.type,
      data: { ...this.data }
    };
  }

  static fromJSON(json) {
    if (!json) return null;
    return new Preset(json);
  }
}

/**
 * PresetManager - Central registry for versioned authoring presets.
 */
export class PresetManager {
  static _presets = new Map();

  static register(presetOrData) {
    const inst = presetOrData instanceof Preset ? presetOrData : new Preset(presetOrData);
    this._presets.set(inst.id, inst);
    return inst;
  }

  static get(id) {
    return this._presets.get(id) || null;
  }

  static getAll() {
    return Array.from(this._presets.values()).sort((a, b) => a.id.localeCompare(b.id));
  }

  static getByType(type) {
    return this.getAll().filter(p => p.type === type);
  }

  static remove(id) {
    return this._presets.delete(id);
  }

  static clear() {
    this._presets.clear();
  }

  static toJSON() {
    return this.getAll().map(p => p.toJSON());
  }

  static load(list = []) {
    if (Array.isArray(list)) {
      for (const item of list) {
        this.register(item);
      }
    }
  }
}

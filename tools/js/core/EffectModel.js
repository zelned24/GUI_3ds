import { globalRNG } from './DeterministicRNG.js';

/**
 * Valid Citro2D blend modes.
 * Mapped to Citro2D / OpenGL ES 1.1 blend functions.
 */
export const VALID_BLEND_MODES = ['normal', 'add', 'multiply', 'screen'];

export const EFFECT_TYPES = ['Opacity', 'Tint', 'Brightness', 'ColorOverlay', 'Fade'];

/**
 * BlendModes - Citro2D blend mode definitions and validation.
 */
export class BlendModes {
  static Normal = 'normal';
  static Add = 'add';
  static Multiply = 'multiply';
  static Screen = 'screen';

  static isValid(mode) {
    if (!mode || typeof mode !== 'string') return false;
    return VALID_BLEND_MODES.includes(mode.toLowerCase());
  }

  static assertSupported(mode) {
    if (!this.isValid(mode)) {
      throw new Error(`Unsupported blend mode: "${mode}". Citro2D supports only: ${VALID_BLEND_MODES.join(', ')}`);
    }
    return mode.toLowerCase();
  }
}

/**
 * Effect - Declarative visual effect applied to a UI node.
 */
export class Effect {
  constructor(data = {}) {
    this.id = data.id || globalRNG.nextId('eff');
    if (data.type && !EFFECT_TYPES.includes(data.type)) {
      throw new Error(`Unknown effect type: "${data.type}". Valid types are: ${EFFECT_TYPES.join(', ')}`);
    }
    this.type = data.type || 'Opacity';
    this.enabled = data.enabled !== false;
    this.parameters = this._normalizeParameters(this.type, data.parameters || {});
  }

  _normalizeParameters(type, raw = {}) {
    switch (type) {
      case 'Opacity':
        return {
          opacity: Math.max(0, Math.min(1, parseFloat(raw.opacity ?? 1.0)))
        };
      case 'Tint':
        return {
          color: String(raw.color || '#ffffff'),
          intensity: Math.max(0, Math.min(1, parseFloat(raw.intensity ?? 0.5)))
        };
      case 'Brightness':
        return {
          factor: Math.max(0, Math.min(2.0, parseFloat(raw.factor ?? 1.0)))
        };
      case 'ColorOverlay':
        return {
          color: String(raw.color || '#ff0000'),
          blendMode: VALID_BLEND_MODES.includes(raw.blendMode?.toLowerCase()) ? raw.blendMode.toLowerCase() : 'normal',
          opacity: Math.max(0, Math.min(1, parseFloat(raw.opacity ?? 0.5)))
        };
      case 'Fade':
        return {
          startAlpha: Math.max(0, Math.min(1, parseFloat(raw.startAlpha ?? 0.0))),
          endAlpha: Math.max(0, Math.min(1, parseFloat(raw.endAlpha ?? 1.0))),
          progress: Math.max(0, Math.min(1, parseFloat(raw.progress ?? 0.5)))
        };
      default:
        return { ...raw };
    }
  }

  static isSupportedBlendMode(mode) {
    if (!mode || typeof mode !== 'string') return false;
    return VALID_BLEND_MODES.includes(mode.toLowerCase());
  }

  clone(overrides = {}) {
    return new Effect({
      id: overrides.id || globalRNG.nextId('eff'),
      type: overrides.type || this.type,
      enabled: overrides.enabled !== undefined ? overrides.enabled : this.enabled,
      parameters: {
        ...this.parameters,
        ...(overrides.parameters || {})
      }
    });
  }

  toJSON() {
    return {
      id: this.id,
      type: this.type,
      enabled: this.enabled,
      parameters: { ...this.parameters }
    };
  }

  static fromJSON(json) {
    if (!json) return null;
    return new Effect(json);
  }
}

/**
 * EffectStack - Deterministic ordered list of effects applied to a node.
 */
export class EffectStack {
  constructor(effects = []) {
    this.effects = [];
    if (Array.isArray(effects)) {
      for (const e of effects) {
        this.effects.push(e instanceof Effect ? e : Effect.fromJSON(e));
      }
    }
  }

  add(effect) {
    const inst = effect instanceof Effect ? effect : new Effect(effect);
    this.effects.push(inst);
    return inst;
  }

  remove(idOrIndex) {
    const idx = typeof idOrIndex === 'number'
      ? idOrIndex
      : this.effects.findIndex(e => e.id === idOrIndex);
    if (idx >= 0 && idx < this.effects.length) {
      return this.effects.splice(idx, 1)[0];
    }
    return null;
  }

  get(idOrIndex) {
    if (typeof idOrIndex === 'number') {
      return this.effects[idOrIndex] || null;
    }
    return this.effects.find(e => e.id === idOrIndex) || null;
  }

  reorder(fromIndex, toIndex) {
    if (fromIndex < 0 || fromIndex >= this.effects.length ||
        toIndex < 0 || toIndex >= this.effects.length) {
      return false;
    }
    const [moved] = this.effects.splice(fromIndex, 1);
    this.effects.splice(toIndex, 0, moved);
    return true;
  }

  getAll() {
    return [...this.effects];
  }

  getEffects() {
    return this.getAll();
  }

  get length() {
    return this.effects.length;
  }

  static fromJSON(rawList) {
    return new EffectStack(Array.isArray(rawList) ? rawList : []);
  }

  toJSON() {
    return this.effects.map(e => e.toJSON());
  }

  clone() {
    return new EffectStack(this.effects.map(e => e.clone()));
  }
}

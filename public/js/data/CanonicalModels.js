/**
 * CanonicalModels - Neutral, declarative domain models for the 3DS Game Creation Studio.
 * Decouples PokéRogue's internal TypeScript implementation from the Studio and 3DS runtime.
 */

export class SourceMetadata {
  constructor(data = {}) {
    this.sourceType = data?.sourceType || (String(data?.source || '').startsWith('TEST_FIXTURE') ? 'TEST_FIXTURE' : 'UNVERIFIED');
    this.source = data?.source || (this.sourceType === 'UPSTREAM' ? 'pokerogue' : this.sourceType);
    this.sourceRepository = data?.sourceRepository || data?.repository || (this.sourceType === 'UPSTREAM' ? 'https://github.com/pagefaultgames/pokerogue' : '');
    this.sourcePath = data?.sourcePath || '';
    this.sourceRevision = data?.sourceRevision || data?.revision || '';
    this.importedAt = data?.importedAt || 'CANONICAL_IMPORT';
    this.license = data?.license || 'AGPL-v3.0-only';
    this.contentVersion = data?.contentVersion || '1.0.0';
    this.schemaVersion = data?.schemaVersion || '1.0.0';
    this.sourceHash = data?.sourceHash || data?.hash || null;
    this.sourceSymbol = data?.sourceSymbol || null;
  }
}

function toTitleCase(str) {
  if (!str) return '';
  const s = String(str).trim();
  if (s.toUpperCase() === 'NONE') return 'NONE';
  return s.split(/[\s_]+/).map(w => w.charAt(0).toUpperCase() + w.slice(1).toLowerCase()).join(' ');
}


export class SpeciesDefinition {
  constructor(data = {}) {
    this.id = data.id || 'unknown';
    this.speciesId = Number(data.speciesId || data.nationalDexId || 0);
    this.nationalDexId = this.speciesId;
    this.name = data.name || this.id;
    this.names = {
      en: data.names?.en || data.name || this.id,
      es: data.names?.es || data.names?.['es-ES'] || data.name || this.id,
      ...data.names
    };
    this.generation = Number(data.generation || 1);
    this.type1 = toTitleCase(data.type1 || (data.types && data.types[0]) || 'Normal');
    this.type2 = data.type2 && data.type2.toUpperCase() !== 'NONE' ? toTitleCase(data.type2) : ((data.types && data.types[1]) ? toTitleCase(data.types[1]) : 'NONE');
    this.types = [this.type1];
    if (this.type2 && this.type2 !== 'NONE') {
      this.types.push(this.type2);
    }
    this.baseStats = {
      hp: Number(data.baseStats?.hp ?? 40),
      atk: Number(data.baseStats?.atk ?? 40),
      def: Number(data.baseStats?.def ?? 40),
      spatk: Number(data.baseStats?.spatk ?? 40),
      spdef: Number(data.baseStats?.spdef ?? 40),
      spd: Number(data.baseStats?.spd ?? 40)
    };
    this.baseTotal = data.baseTotal == null ? null : Number(data.baseTotal);
    this.rarity = {
      legendary: data.rarity?.legendary ?? null,
      subLegendary: data.rarity?.subLegendary ?? null,
      mythical: data.rarity?.mythical ?? null
    };
    this.growthRate = data.growthRate ?? null;
    const ab1 = data.abilities?.primary || data.abilities?.ability1 || data.ability1 || 'NONE';
    const ab2 = data.abilities?.secondary || data.abilities?.ability2 || data.ability2 || null;
    const abh = data.abilities?.hidden || data.abilityHidden || null;
    const abp = data.abilities?.passive || data.passive || null;
    this.abilities = {
      primary: ab1,
      secondary: ab2,
      hidden: abh,
      passive: abp,
      ability1: ab1,
      ability2: ab2 || 'NONE'
    };
    this.starterCost = Number(data.starterCost ?? 3);
    this.eggTier = data.eggTier || 'COMMON';
    const rawMoves = Array.isArray(data.learnableMoves)
      ? data.learnableMoves
      : (Array.isArray(data.levelMoves) ? data.levelMoves : []);
    this.learnableMoves = rawMoves.map(m => {
      if (typeof m === 'string') {
        return { id: m, move: m, level: 1, name: toTitleCase(m.replace(/_/g, ' ')) };
      }
      const moveKey = m.id || m.move || 'unknown';
      return {
        ...m,
        id: moveKey,
        move: moveKey,
        level: m.level || 1,
        name: m.name || toTitleCase(moveKey.replace(/_/g, ' '))
      };
    });
    this.levelMoves = this.learnableMoves;

    this.eggMoves = Array.isArray(data.eggMoves) ? [...data.eggMoves] : [];
    this.forms = Array.isArray(data.forms) ? [...data.forms] : ['BASE'];
    this.sprites = {
      atlasPath: data.sprites?.atlasPath || `romfs/sprites/pokemon/${this.speciesId}.t3x`,
      icon: data.sprites?.icon || `romfs/sprites/icons/${this.speciesId}.png`,
      atlas: data.sprite?.atlas || 'pokemon_front',
      frame: data.sprite?.frame || `${this.speciesId}`,
      hasFemale: Boolean(data.sprite?.hasFemale),
      hasShiny: Boolean(data.sprite?.hasShiny ?? true),
      hasVariants: Boolean(data.sprite?.hasVariants)
    };
    this.sprite = this.sprites;
    this.source = new SourceMetadata(data.metadata || data.source);
    this.metadata = this.source;
    this.extensions = data.extensions || {};
    if (data.raw) this.extensions.upstreamRawRecord = data.raw;
    this.schemaVersion = 1;
  }

  getName(locale = 'en') {
    const loc = String(locale).toLowerCase().startsWith('es') ? 'es' : 'en';
    return this.names?.[loc] || this.name;
  }
}

export class MoveDefinition {
  constructor(data = {}) {
    this.id = data.id || 'unknown';
    this.moveId = Number(data.moveId || 0);
    this.name = data.name || this.id;
    this.names = {
      en: data.names?.en || data.name || this.id,
      es: data.names?.es || data.names?.['es-ES'] || data.name || this.id,
      ...data.names
    };
    this.description = data.description || '';
    this.descriptions = {
      en: data.descriptions?.en || data.description || '',
      es: data.descriptions?.es || data.descriptions?.['es-ES'] || data.description || '',
      ...data.descriptions
    };
    this.type = (data.type || 'NORMAL').toUpperCase();
    this.category = data.category || 'Physical';
    this.power = Number(data.power ?? 0);
    this.accuracy = Number(data.accuracy ?? 100);
    this.pp = Number(data.pp ?? 20);
    this.maxPp = Number(data.maxPp ?? Math.floor(this.pp * 1.6));
    this.priority = Number(data.priority ?? 0);
    this.target = data.target || 'Selected';
    this.flags = {
      contact: Boolean(data.flags?.contact),
      protectable: Boolean(data.flags?.protectable ?? true),
      sound: Boolean(data.flags?.sound),
      bullet: Boolean(data.flags?.bullet)
    };
    this.secondaryEffects = data.secondaryEffects === null ? null : (Array.isArray(data.secondaryEffects) ? [...data.secondaryEffects] : []);
    this.source = new SourceMetadata(data.metadata || data.source);
    this.metadata = this.source;
    this.extensions = data.extensions || {};
    if (data.raw) this.extensions.upstreamRawRecord = data.raw;
    this.schemaVersion = 1;
  }

  getName(locale = 'en') {
    const loc = String(locale).toLowerCase().startsWith('es') ? 'es' : 'en';
    return this.names?.[loc] || this.name;
  }

  getDescription(locale = 'en') {
    const loc = String(locale).toLowerCase().startsWith('es') ? 'es' : 'en';
    return this.descriptions?.[loc] || this.description;
  }
}

export class AbilityDefinition {
  constructor(data = {}) {
    this.id = data.id || 'unknown';
    this.name = data.name || this.id;
    this.names = {
      en: data.names?.en || data.name || this.id,
      es: data.names?.es || data.names?.['es-ES'] || data.name || this.id,
      ...data.names
    };
    this.description = data.description || '';
    this.descriptions = {
      en: data.descriptions?.en || data.description || '',
      es: data.descriptions?.es || data.descriptions?.['es-ES'] || data.description || '',
      ...data.descriptions
    };
    this.trigger = data.trigger || 'PASSIVE';
    this.attributes = Array.isArray(data.attributes) ? [...data.attributes] : [];
    this.conditions = Array.isArray(data.conditions) ? [...data.conditions] : [];
    this.effects = Array.isArray(data.effects) ? [...data.effects] : [];
    this.source = new SourceMetadata(data.metadata || data.source);
    this.metadata = this.source;
    this.extensions = data.extensions || {};
    if (data.raw) this.extensions.upstreamRawRecord = data.raw;
    this.schemaVersion = 1;
  }

  getName(locale = 'en') {
    const loc = String(locale).toLowerCase().startsWith('es') ? 'es' : 'en';
    return this.names?.[loc] || this.name;
  }

  getDescription(locale = 'en') {
    const loc = String(locale).toLowerCase().startsWith('es') ? 'es' : 'en';
    return this.descriptions?.[loc] || this.description;
  }
}

export class ItemDefinition {
  constructor(data = {}) {
    this.id = data.id || 'unknown';
    this.name = data.name || this.id;
    this.names = {
      en: data.names?.en || data.name || this.id,
      es: data.names?.es || data.names?.['es-ES'] || data.name || this.id,
      ...data.names
    };
    this.category = data.category ?? null;
    this.tier = data.tier ?? null;
    this.price = data.price === undefined || data.price === null ? null : Number(data.price);
    this.description = data.description || '';
    this.descriptions = {
      en: data.descriptions?.en || data.description || '',
      es: data.descriptions?.es || data.descriptions?.['es-ES'] || data.description || '',
      ...data.descriptions
    };
    this.source = new SourceMetadata(data.metadata || data.source);
    this.metadata = this.source;
    this.extensions = data.extensions || {};
    if (data.raw) this.extensions.upstreamRawRecord = data.raw;
    this.schemaVersion = 1;
  }

  getName(locale = 'en') {
    const loc = String(locale).toLowerCase().startsWith('es') ? 'es' : 'en';
    return this.names?.[loc] || this.name;
  }
}

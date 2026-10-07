import { SpeciesDefinition, SpeciesEvolutionDefinition, MoveDefinition, AbilityDefinition, ItemDefinition, TrainerDefinition, TrainerPartyTemplateDefinition, SourceMetadata } from './CanonicalModels.js';
import { PokerogueManifest } from './PokerogueManifest.js';
import { POKEROGUE_REPOSITORIES } from './PokerogueSource.js';
import { PokerogueEnumParser } from './PokerogueEnumParser.js';
import { PokerogueLocaleImporter } from './PokerogueLocaleImporter.js';
import { CanonicalContent, CanonicalSourceType, Provenance, SourceSnapshot } from './CanonicalDataContract.js';
import { getOfflineImporterFixtureSources } from '../../../test/fixtures/offlineImporterSources.js';
import { GameModeDefinition } from '../game/GameMode.js';
import { BiomeDefinition, RouteDefinition } from '../game/ProgressionContent.js';
import { POKEROGUE_BASE_ATLAS_IDS, POKEROGUE_BASE_ATLAS_REVISION, POKEROGUE_FORM_ATLAS_KEYS } from './PokerogueBaseAtlasIndex.js';

function extractBalancedLiteral(source, openingIndex) {
  const opening = source[openingIndex];
  const closing = opening === '[' ? ']' : opening === '{' ? '}' : opening === '(' ? ')' : null;
  if (!closing) return null;
  let depth = 0;
  let quote = null;
  let lineComment = false;
  let blockComment = false;
  let escaped = false;
  for (let index = openingIndex; index < source.length; index++) {
    const char = source[index], next = source[index + 1];
    if (lineComment) { if (char === '\n') lineComment = false; continue; }
    if (blockComment) { if (char === '*' && next === '/') { blockComment = false; index++; } continue; }
    if (quote) {
      if (escaped) escaped = false;
      else if (char === '\\') escaped = true;
      else if (char === quote) quote = null;
      continue;
    }
    if (char === '/' && next === '/') { lineComment = true; index++; continue; }
    if (char === '/' && next === '*') { blockComment = true; index++; continue; }
    if (char === '"' || char === "'" || char === '`') { quote = char; continue; }
    if (char === opening) depth++;
    else if (char === closing && --depth === 0) return source.slice(openingIndex, index + 1);
  }
  return null;
}

function splitTopLevelArguments(source) {
  const parts = [];
  let start = 0, parens = 0, braces = 0, brackets = 0, quote = null, escaped = false, lineComment = false, blockComment = false;
  for (let index = 0; index < source.length; index++) {
    const char = source[index], next = source[index + 1];
    if (lineComment) { if (char === '\n') lineComment = false; continue; }
    if (blockComment) { if (char === '*' && next === '/') { blockComment = false; index++; } continue; }
    if (quote) {
      if (escaped) escaped = false;
      else if (char === '\\') escaped = true;
      else if (char === quote) quote = null;
      continue;
    }
    if (char === '/' && next === '/') { lineComment = true; index++; continue; }
    if (char === '/' && next === '*') { blockComment = true; index++; continue; }
    if (char === '"' || char === "'" || char === '`') { quote = char; continue; }
    if (char === '(') parens++;
    else if (char === ')') parens--;
    else if (char === '{') braces++;
    else if (char === '}') braces--;
    else if (char === '[') brackets++;
    else if (char === ']') brackets--;
    else if (char === ',' && parens === 0 && braces === 0 && brackets === 0) {
      parts.push(source.slice(start, index).trim()); start = index + 1;
    }
  }
  if (source.slice(start).trim()) parts.push(source.slice(start).trim());
  return parts;
}

export function parseStarterCandyPriceTable(source) {
  const declaration = /\bconst\s+allStarterCandyCosts\b[^=]*=\s*(\[)/.exec(source);
  if (!declaration) throw new Error('Missing pinned allStarterCandyCosts');
  const opening = declaration.index + declaration[0].lastIndexOf('[');
  const raw = extractBalancedLiteral(source, opening);
  if (!raw) throw new Error('Unclosed starter candy price table');
  const json = raw.replace(/\/\/[^\n]*|\/\*[\s\S]*?\*\//g, '')
    .replace(/([{,]\s*)([A-Za-z_$][\w$]*)\s*:/g, '$1"$2":')
    .replace(/,\s*([}\]])/g, '$1');
  let rows;
  try { rows = JSON.parse(json); } catch { throw new Error('Unsupported starter candy table expression'); }
  const positive = value => Number.isInteger(value) && value > 0 && value <= 65535;
  if (!Array.isArray(rows) || !rows.length) throw new Error('Empty starter candy price table');
  const entries = rows.map((row, index) => {
    if (!row || !positive(row.passive) || !Array.isArray(row.costReduction) || row.costReduction.length !== 2 ||
        !row.costReduction.every(positive) || !Array.isArray(row.eggCosts) || !row.eggCosts.length ||
        !row.eggCosts.every(positive) || !Array.isArray(row.eggCostReductionThresholds) ||
        row.eggCostReductionThresholds.length !== row.eggCosts.length - 1 ||
        !row.eggCostReductionThresholds.every(positive)) throw new Error('Invalid starter candy price row');
    return { cost: index + 1, passive: row.passive, costReduction: row.costReduction,
      eggCosts: row.eggCosts, eggCostReductionThresholds: row.eggCostReductionThresholds, raw: row };
  });
  return { entries, raw };
}

function parseLevelMoveArray(arrayLiteral, context) {
  if (!arrayLiteral?.startsWith('[') || !arrayLiteral.endsWith(']')) throw new Error(`Invalid import: ${context} must be an array`);
  const moves = [];
  for (const rawEntry of splitTopLevelArguments(arrayLiteral.slice(1, -1))) {
    const entry = rawEntry.replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*/g, '').trim();
    if (!entry) continue;
    const tuple = entry.match(/^\[\s*(-?\d+|EVOLVE_MOVE|RELEARN_MOVE)\s*,\s*(?:MoveId\.|Moves\.)?([A-Za-z0-9_]+)\s*\]$/);
    if (!tuple) throw new Error(`Invalid import: unsupported ${context} tuple ${entry}`);
    const level = tuple[1] === 'EVOLVE_MOVE' ? 0 : tuple[1] === 'RELEARN_MOVE' ? -1 : Number(tuple[1]);
    moves.push({ level, move: tuple[2].toLowerCase(), id: tuple[2].toLowerCase() });
  }
  return moves;
}

function parseExperienceGrowthRates(source) {
  const marker = /\bconst\s+expLevels\s*=\s*/.exec(source);
  if (!marker) throw new Error('Invalid import: pinned src/data/exp.ts has no expLevels table');
  const open = source.indexOf('[', marker.index + marker[0].length);
  const literal = extractBalancedLiteral(source, open);
  if (!literal) throw new Error('Invalid import: unclosed pinned expLevels table');
  const rows = [...literal.matchAll(/\[\s*(\d+(?:\s*,\s*\d+)*)\s*,?\s*\]/g)]
    .map(match => match[1].split(',').map(value => Number(value.trim())));
  const growthRates = ['ERRATIC', 'FAST', 'MEDIUM_FAST', 'MEDIUM_SLOW', 'SLOW', 'FLUCTUATING'];
  if (rows.length !== growthRates.length || rows.some(row => row.length !== 100 || row.some(value => !Number.isSafeInteger(value) || value < 0))) {
    throw new Error(`Invalid import: pinned expLevels table shape is ${rows.length} x ${rows.map(row => row.length).join(',')}; expected 6 x 100 non-negative integers`);
  }
  return Object.fromEntries(growthRates.map((rate, index) => [rate, rows[index]]));
}

function parseClassicFixedBossWaves(source) {
  const body = source.match(/export\s+enum\s+ClassicFixedBossWaves\s*\{([\s\S]*?)\}/)?.[1];
  if (!body) throw new Error('Pinned ClassicFixedBossWaves enum was not found');
  const entries = [];
  let nextValue = 0;
  for (const line of body.split(/\r?\n/)) {
    const match = line.match(/^\s*([A-Z][A-Z0-9_]*)\s*(?:=\s*(\d+))?\s*,?\s*$/);
    if (!match) continue;
    const wave = match[2] === undefined ? nextValue : Number(match[2]);
    if (!Number.isInteger(wave) || wave < 1 || wave > 0xFFFF) {
      throw new Error(`Pinned Classic fixed-boss wave is outside the native range: ${match[1]}=${wave}`);
    }
    entries.push({ id: match[1].toLowerCase(), symbol: match[1], wave });
    nextValue = wave + 1;
  }
  if (!entries.length) throw new Error('Pinned ClassicFixedBossWaves enum contains no entries');
  const seen = new Set();
  for (const entry of entries) {
    if (seen.has(entry.wave)) throw new Error(`Duplicate pinned Classic fixed-boss wave ${entry.wave}`);
    seen.add(entry.wave);
  }
  return entries.sort((left, right) => left.wave - right.wave);
}

function parseClassicFixedBattleWaves(source, fixedBossWaves, trainerTypes) {
  const marker = /export\s+const\s+classicFixedBattles\s*:\s*FixedBattleConfigs\s*=\s*\{/;
  const match = marker.exec(source);
  if (!match) throw new Error('Pinned classicFixedBattles configuration was not found');
  const open = source.indexOf('{', match.index);
  const literal = extractBalancedLiteral(source, open);
  const body = literal?.slice(1, -1);
  if (!body) throw new Error('Pinned classicFixedBattles configuration is unclosed or empty');
  const bySymbol = new Map(fixedBossWaves.map(entry => [entry.symbol, entry.wave]));
  const entries = [];
  for (const raw of splitTopLevelArguments(body)) {
    const key = raw.replace(/^\s*(?:(?:\/\*[\s\S]*?\*\/)|(?:\/\/[^\n]*(?:\n|$))\s*)*/, '').match(/^\s*\[\s*ClassicFixedBossWaves\.([A-Z][A-Z0-9_]*)\s*\]\s*:/);
    if (!key) throw new Error(`Pinned classicFixedBattles contains an unsupported declaration: ${raw.slice(0, 96)}`);
    const wave = bySymbol.get(key[1]);
    if (!Number.isSafeInteger(wave)) throw new Error(`Pinned classicFixedBattles references unresolved ClassicFixedBossWaves.${key[1]}`);
    // Only a direct constructor has a fixed trainer identity. Dynamic callbacks
    // remain in raw until their selection semantics are ported.
    const staticTrainer = raw.match(/\.setGetTrainerFunc\(\s*\(\)\s*=>\s*new Trainer\(\s*TrainerType\.([A-Z][A-Z0-9_]*)\s*,/);
    const trainerTypeSymbol = staticTrainer?.[1] ?? null;
    const trainerTypeId = trainerTypeSymbol ? trainerTypes.getId(trainerTypeSymbol) : null;
    if (trainerTypeSymbol && !Number.isSafeInteger(trainerTypeId))
      throw new Error(`Pinned fixed battle ${key[1]} references unknown TrainerType.${trainerTypeSymbol}`);
    const seededBinaryGenderVariant = /new Trainer\(\s*TrainerType\.[A-Z][A-Z0-9_]*\s*,\s*randSeedInt\(2\)\s*\?\s*TrainerVariant\.FEMALE\s*:\s*TrainerVariant\.DEFAULT\s*\)/.test(raw);
    entries.push({ wave, symbol: key[1], trainerTypeSymbol, trainerTypeId, seededBinaryGenderVariant, raw });
  }
  if (!entries.length) throw new Error('Pinned classicFixedBattles configuration produced no wave keys');
  if (new Set(entries.map(entry => entry.wave)).size !== entries.length) throw new Error('Pinned classicFixedBattles contains duplicate wave keys');
  return entries.sort((left, right) => left.wave - right.wave);
}

function parseTrainerConfigRecords(source) {
  const marker = /export\s+const\s+trainerConfigs\s*:\s*TrainerConfigs\s*=\s*\{/;
  const match = marker.exec(source);
  if (!match) throw new Error('Pinned trainerConfigs registry was not found');
  const open = source.indexOf('{', match.index);
  const literal = extractBalancedLiteral(source, open);
  if (!literal) throw new Error('Pinned trainerConfigs registry is unclosed');
  const records = new Map();
  for (const raw of splitTopLevelArguments(literal.slice(1, -1))) {
    const parsed = raw.replace(/^\s*(?:(?:\/\*[\s\S]*?\*\/)|(?:\/\/[^\n]*(?:\n|$))\s*)*/, '')
      .match(/^\s*\[\s*TrainerType\.([A-Z][A-Z0-9_]*)\s*\]\s*:/);
    if (!parsed) throw new Error(`Pinned trainerConfigs contains an unsupported declaration: ${raw.slice(0, 96)}`);
    if (records.has(parsed[1])) throw new Error(`Duplicate pinned trainer config TrainerType.${parsed[1]}`);
    records.set(parsed[1], raw);
  }
  if (!records.size) throw new Error('Pinned trainerConfigs registry produced no trainer records');
  return records;
}

function parseEvoLevelThresholdKinds(source) {
  const body = source.match(/export\s+const\s+EvoLevelThresholdKind\s*=\s*Object\.freeze\s*\(\s*\{([\s\S]*?)\}\s*\)/)?.[1];
  if (!body) throw new Error('Pinned EvoLevelThresholdKind object was not found');
  const values = new Map([...body.matchAll(/\b([A-Z][A-Z0-9_]*)\s*:\s*(\d+)\b/g)].map(match => [match[1], Number(match[2])]));
  if (!values.has('NORMAL') || !values.has('STRONG')) throw new Error('Pinned trainer evolution-threshold symbols are incomplete');
  return values;
}

function parseTrainerPartyTemplates(source, partyStrengthCatalog, evolutionThresholds) {
  const marker = /export\s+const\s+trainerPartyTemplates\s*=\s*\{/;
  const match = marker.exec(source);
  if (!match) throw new Error('Pinned trainerPartyTemplates declaration was not found');
  const open = source.indexOf('{', match.index);
  const literal = extractBalancedLiteral(source, open);
  if (!literal) throw new Error('Pinned trainerPartyTemplates declaration is unclosed');
  const templates = [];
  const parseSegments = (expression, templateId) => {
    const clean = expression.replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*/g, '');
    const calls = /new\s+TrainerPartyTemplate\s*\(/g;
    const segments = [];
    let call;
    let lastEnd = 0;
    while ((call = calls.exec(clean)) !== null) {
      if (!/^[\s,]*$/.test(clean.slice(lastEnd, call.index))) throw new Error(`Unsupported trainer party segment expression in ${templateId}`);
      const argsOpen = clean.indexOf('(', call.index);
      const argsLiteral = extractBalancedLiteral(clean, argsOpen);
      if (!argsLiteral) throw new Error(`Unclosed TrainerPartyTemplate in ${templateId}`);
      const args = splitTopLevelArguments(argsLiteral.slice(1, -1));
      const size = Number(args[0]);
      const strengthSymbol = args[1]?.match(/^\s*PartyMemberStrength\.([A-Z][A-Z0-9_]*)\s*$/)?.[1];
      const strengthId = strengthSymbol ? partyStrengthCatalog.getId(strengthSymbol) : undefined;
      const sameSpecies = args[2] === undefined || args[2].trim() === 'undefined' ? false : args[2].trim() === 'true' ? true : args[2].trim() === 'false' ? false : null;
      const balanced = args[3] === undefined || args[3].trim() === 'undefined' ? false : args[3].trim() === 'true' ? true : args[3].trim() === 'false' ? false : null;
      const thresholdSymbol = args[4] === undefined || args[4].trim() === 'undefined'
        ? 'NORMAL' : args[4].match(/^\s*EvoLevelThresholdKind\.([A-Z][A-Z0-9_]*)\s*$/)?.[1];
      const thresholdId = thresholdSymbol ? evolutionThresholds.get(thresholdSymbol) : undefined;
      if (!Number.isInteger(size) || size < 1 || size > 6 || !strengthSymbol || !Number.isInteger(strengthId) ||
          sameSpecies === null || balanced === null || !['NORMAL', 'STRONG'].includes(thresholdSymbol) || !Number.isInteger(thresholdId)) {
        throw new Error(`Unsupported or invalid pinned TrainerPartyTemplate constructor in ${templateId}: ${argsLiteral}`);
      }
      segments.push({ size, strength: strengthSymbol, strengthId, sameSpecies, balanced, evolutionThresholdKind: thresholdSymbol, evolutionThresholdKindId: thresholdId });
      lastEnd = argsOpen + argsLiteral.length;
      calls.lastIndex = lastEnd;
    }
    if (!segments.length || !/^[\s,]*$/.test(clean.slice(lastEnd))) throw new Error(`Unsupported trainer party template expression: ${templateId}`);
    return segments;
  };
  for (const raw of splitTopLevelArguments(literal.slice(1, -1))) {
    const cleanRaw = raw.replace(/^\s*(?:(?:\/\*[\s\S]*?\*\/)|(?:\/\/[^\n]*(?:\n|$))\s*)*/, '');
    const keyMatch = cleanRaw.match(/^\s*([A-Z][A-Z0-9_]*)\s*:/);
    if (!keyMatch) throw new Error(`Unsupported pinned trainer party template declaration: ${raw.slice(0, 96)}`);
    const id = keyMatch[1].toLowerCase();
    const expression = cleanRaw.slice(keyMatch[0].length).trim();
    const compound = /^new\s+TrainerPartyCompoundTemplate\s*\(/.test(expression);
    if (!compound && !/^new\s+TrainerPartyTemplate\s*\(/.test(expression)) throw new Error(`Unsupported pinned trainer party template form ${keyMatch[1]}`);
    let segmentExpression = expression;
    if (compound) {
      const argsOpen = expression.indexOf('(');
      const compoundArgs = extractBalancedLiteral(expression, argsOpen);
      if (!compoundArgs || !/^\s*,?\s*$/.test(expression.slice(argsOpen + compoundArgs.length))) throw new Error(`Invalid compound trainer party template ${keyMatch[1]}`);
      segmentExpression = compoundArgs.slice(1, -1);
    }
    const segments = parseSegments(segmentExpression, id);
    const totalSize = segments.reduce((total, segment) => total + segment.size, 0);
    if (totalSize > 6) throw new Error(`Pinned trainer party template ${id} exceeds upstream party size range`);
    templates.push({ id, templateKey: keyMatch[1], totalSize, isCompound: compound, segments, raw });
  }
  if (!templates.length || new Set(templates.map(item => item.id)).size !== templates.length) throw new Error('Pinned trainer party template catalog is empty or duplicated');
  return templates;
}

// Read only top-level calls. Callback bodies and nested expressions cannot
// mutate the declarative configuration merely by containing a setter name.
function trainerTopLevelCalls(source) {
  const calls = [];
  for (let index = 0; index < source.length; index++) {
    const char = source[index], next = source[index + 1];
    if (char === '/' && next === '/') { const end = source.indexOf('\n', index + 2); index = end < 0 ? source.length : end; continue; }
    if (char === '/' && next === '*') { const end = source.indexOf('*/', index + 2); if (end < 0) throw new Error('Unclosed trainer config comment'); index = end + 1; continue; }
    if (char === '"' || char === "'" || char === '`') {
      const quote = char;
      for (++index; index < source.length; index++) {
        if (source[index] === '\\') { index++; continue; }
        if (source[index] === quote) break;
      }
      continue;
    }
    if (char === '.') {
      const match = source.slice(index).match(/^\.([A-Za-z_$][\w$]*)\s*\(/);
      if (match) {
        const open = index + match[0].lastIndexOf('(');
        const literal = extractBalancedLiteral(source, open);
        if (!literal) throw new Error(`Unclosed trainer config call ${match[1]}`);
        calls.push({ method: match[1], args: splitTopLevelArguments(literal.slice(1, -1)), raw: source.slice(index, open + literal.length) });
        index = open + literal.length - 1;
        continue;
      }
    }
    if (char === '(' || char === '[' || char === '{') {
      const literal = extractBalancedLiteral(source, index);
      if (!literal) throw new Error('Unclosed trainer config expression');
      index += literal.length - 1;
    }
  }
  return calls;
}

function parseClassicGymTemplatePolicy(source, templateIds) {
  const marker = /export\s+function\s+getGymLeaderPartyTemplate\s*\(\s*\)\s*\{/.exec(source);
  if (!marker) throw new Error('Missing pinned getGymLeaderPartyTemplate');
  const body = extractBalancedLiteral(source, source.indexOf('{', marker.index));
  const classic = body?.match(/case\s+GameModes\.CLASSIC\s*:([\s\S]*?)default\s*:/)?.[1];
  if (!classic) throw new Error('Unsupported pinned Classic gym template policy');
  let remaining = classic.replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*/g, '').trim();
  const ranges = [];
  while (remaining.startsWith('if')) {
    const match = remaining.match(/^if\s*\(\s*currentBattle\?\.waveIndex\s*<=\s*(\d+)\s*\)\s*\{\s*return\s+trainerPartyTemplates\.([A-Z0-9_]+)\s*;\s*\}/);
    if (!match) throw new Error('Unsupported pinned Classic gym condition');
    const maxWave = Number(match[1]);
    if (!Number.isSafeInteger(maxWave) || maxWave < 1 || maxWave > 65535 || (ranges.length && maxWave <= ranges.at(-1).maxWave)) throw new Error('Invalid Classic gym wave boundary');
    ranges.push({ maxWave, templateKey: match[2] });
    remaining = remaining.slice(match[0].length).trim();
  }
  const fallback = remaining.match(/^return\s+trainerPartyTemplates\.([A-Z0-9_]+)\s*;$/);
  if (!fallback || !ranges.length) throw new Error('Unsupported Classic gym template fallback');
  ranges.push({ maxWave: null, templateKey: fallback[1] });
  for (const range of ranges) if (!templateIds.has(range.templateKey)) throw new Error(`Missing gym template ${range.templateKey}`);
  return { modeId: 'classic', ranges, sourceSymbol: 'getGymLeaderPartyTemplate', raw: body };
}

function parseClassicGruntTemplatePolicy(source, templateIds, fixedWaves) {
  const marker = /export\s+function\s+getEvilGruntPartyTemplate\s*\(\s*\)\s*:\s*TrainerPartyTemplate\s*\{/.exec(source);
  if (!marker) throw new Error('Missing pinned getEvilGruntPartyTemplate');
  const body = extractBalancedLiteral(source, source.indexOf('{', marker.index));
  if (!body) throw new Error('Unclosed pinned grunt template policy');
  let remaining = body.slice(1, -1).replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*/g, '').trim();
  const waveBinding = remaining.match(/^const\s+waveIndex\s*=\s*globalScene\.currentBattle\?\.waveIndex\s*;/);
  if (!waveBinding) throw new Error('Unsupported grunt template wave source');
  remaining = remaining.slice(waveBinding[0].length).trim();
  const bySymbol = new Map(fixedWaves.map(entry => [entry.symbol, entry.wave]));
  const ranges = [];
  while (remaining.startsWith('if')) {
    const match = remaining.match(/^if\s*\(\s*waveIndex\s*<=\s*ClassicFixedBossWaves\.([A-Z0-9_]+)\s*\)\s*\{\s*return\s+trainerPartyTemplates\.([A-Z0-9_]+)\s*;\s*\}/);
    if (!match) throw new Error('Unsupported pinned grunt template condition');
    const maxWave = bySymbol.get(match[1]);
    if (!Number.isSafeInteger(maxWave) || maxWave < 1 || maxWave > 65535 || (ranges.length && maxWave <= ranges.at(-1).maxWave)) throw new Error(`Invalid grunt template boundary ${match[1]}`);
    ranges.push({ maxWave, waveSymbol: `ClassicFixedBossWaves.${match[1]}`, templateKey: match[2] });
    remaining = remaining.slice(match[0].length).trim();
  }
  const fallback = remaining.match(/^return\s+trainerPartyTemplates\.([A-Z0-9_]+)\s*;$/);
  if (!fallback || !ranges.length) throw new Error('Unsupported grunt template fallback');
  ranges.push({ maxWave: null, templateKey: fallback[1] });
  for (const range of ranges) if (!templateIds.has(range.templateKey)) throw new Error(`Missing grunt template ${range.templateKey}`);
  return { modeId: 'classic', ranges, sourceSymbol: 'getEvilGruntPartyTemplate', raw: body };
}

function parseTrainerSignatureSpecies(source, speciesCatalog) {
  const marker = /export\s+const\s+signatureSpecies\s*:\s*SignatureSpecies\s*=\s*new\s+Proxy\s*\(/.exec(source);
  if (!marker) throw new Error('Missing pinned signatureSpecies registry');
  const open = source.indexOf('{', marker.index + marker[0].length);
  const literal = extractBalancedLiteral(source, open);
  if (!literal) throw new Error('Unclosed pinned signatureSpecies registry');
  const result = new Map();
  for (const raw of splitTopLevelArguments(literal.slice(1, -1))) {
    const clean = raw.replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*/g, '').trim();
    if (!clean) continue;
    const match = clean.match(/^([A-Z][A-Z0-9_]*)\s*:\s*/);
    if (!match || result.has(match[1])) throw new Error(`Invalid signature species record: ${clean.slice(0, 80)}`);
    const expression = clean.slice(match[0].length);
    const list = extractBalancedLiteral(expression, 0);
    if (!list || expression.slice(list.length).trim()) throw new Error(`Unsupported signature species list: ${match[1]}`);
    const choices = splitTopLevelArguments(list.slice(1, -1)).map((candidate, slot) => {
      const group = candidate.trim().startsWith('[');
      const value = group ? extractBalancedLiteral(candidate.trim(), 0) : candidate.trim();
      if (!value || (group && candidate.trim().slice(value.length).trim())) throw new Error(`Invalid signature group: ${match[1]}[${slot}]`);
      const elements = group ? splitTopLevelArguments(value.slice(1, -1)) : [value];
      const speciesIds = elements.map(element => {
        const symbol = element.trim().match(/^SpeciesId\.([A-Z][A-Z0-9_]*)$/)?.[1];
        if (!symbol || !speciesCatalog.hasSymbol(symbol)) throw new Error(`Unknown signature species: ${match[1]}[${slot}] ${element}`);
        return symbol.toLowerCase();
      });
      if (!speciesIds.length) throw new Error(`Empty signature group: ${match[1]}[${slot}]`);
      return { slot, isGroup: group, speciesIds };
    });
    result.set(match[1], { choices, raw });
  }
  if (!result.size) throw new Error('Empty pinned signatureSpecies registry');
  return result;
}

function parsePokemonSpriteAtlas(source, sourcePath) {
  const parsed = JSON.parse(source);
  const textures = parsed?.textures;
  const texture = Array.isArray(textures) && textures.length === 1
    ? textures[0]
    : parsed?.frames && typeof parsed.frames === 'object'
      ? { image: parsed.meta?.image, size: parsed.meta?.size,
          frames: Array.isArray(parsed.frames) ? parsed.frames
            : Object.entries(parsed.frames).map(([filename, entry]) => ({ filename, ...entry })) }
      : null;
  if (!texture) throw new Error(`Unsupported pinned sprite atlas: ${sourcePath}`);
  const width = texture?.size?.w;
  const height = texture?.size?.h;
  const imageName = texture?.image;
  if (!Number.isInteger(width) || !Number.isInteger(height) || width < 1 || height < 1
      || typeof imageName !== 'string' || !/^[A-Za-z0-9_-]+\.png$/.test(imageName)
      || !Array.isArray(texture.frames) || !texture.frames.length)
    throw new Error(`Invalid pinned sprite atlas: ${sourcePath}`);
  const frames = texture.frames.map((entry, index) => {
    const frame = entry?.frame;
    const sourceSize = entry?.sourceSize;
    const spriteSourceSize = entry?.spriteSourceSize;
    if (typeof entry.filename !== 'string' || !/^[A-Za-z0-9_-]+\.png$/.test(entry.filename)
        || entry.rotated !== false || typeof entry.trimmed !== 'boolean'
        || ![frame?.x, frame?.y, frame?.w, frame?.h,
          sourceSize?.w, sourceSize?.h, spriteSourceSize?.x,
          spriteSourceSize?.y, spriteSourceSize?.w, spriteSourceSize?.h]
          .every(value => Number.isInteger(value) && value >= 0)
        || frame.w < 1 || frame.h < 1 || sourceSize.w < 1 || sourceSize.h < 1
        || frame.x + frame.w > width || frame.y + frame.h > height
        || spriteSourceSize.x + spriteSourceSize.w > sourceSize.w
        || spriteSourceSize.y + spriteSourceSize.h > sourceSize.h
        || spriteSourceSize.w !== frame.w || spriteSourceSize.h !== frame.h)
      throw new Error(`Invalid pinned sprite frame ${sourcePath}[${index}]`);
    const { filename, frame: _, sourceSize: __, spriteSourceSize: ___, ...other } = entry;
    return { filename, frame: { ...frame }, sourceSize: { ...sourceSize },
      spriteSourceSize: { ...spriteSourceSize }, extensions: other };
  });
  return { imagePath: sourcePath.slice(0, sourcePath.lastIndexOf('/') + 1) + imageName,
    width, height, frames, format: Array.isArray(textures) ? 'TEXTUREPACKER_TEXTURES' : 'ASEPRITE_FRAMES',
    extensions: Array.isArray(textures) ? { rawTextureMetadata: Object.fromEntries(Object.entries(texture).filter(([key]) => !['frames', 'size', 'image'].includes(key))) }
      : { rawMeta: parsed.meta } };
}

function parseSupercededMovePairs(source, moveEnum) {
  const aliases = new Map();
  for (const match of source.matchAll(/\bconst\s+([A-Z][A-Z0-9_]+)\s*:\s*readonly\s+MoveId\[\]\s*=\s*/g)) {
    const open = source.indexOf('[', match.index + match[0].length);
    const literal = extractBalancedLiteral(source, open);
    if (!literal) throw new Error(`Invalid import: unclosed superceded-move alias ${match[1]}`);
    aliases.set(match[1], literal);
  }
  const marker = /\bexport\s+const\s+SUPERCEDED_MOVES\s*:[^=]+?=\s*/.exec(source);
  if (!marker) throw new Error('Invalid import: SUPERCEDED_MOVES declaration missing');
  const open = source.indexOf('{', marker.index + marker[0].length);
  const literal = extractBalancedLiteral(source, open);
  if (!literal) throw new Error('Invalid import: unclosed SUPERCEDED_MOVES map');
  const moveId = symbol => {
    const id = moveEnum.getId(symbol);
    if (!Number.isInteger(id) || id < 1 || id > 0xFFFF)
      throw new Error(`Invalid import: unknown superceded MoveId.${symbol}`);
    return id;
  };
  const parseArray = (array, context) => {
    const result = [];
    for (const raw of splitTopLevelArguments(array.slice(1, -1))) {
      const entry = raw.replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*/g, '').trim();
      if (!entry) continue;
      const match = /^MoveId\.([A-Z0-9_]+)$/.exec(entry);
      if (!match) throw new Error(`Invalid import: unsupported ${context} entry ${entry}`);
      result.push(moveId(match[1]));
    }
    if (!result.length || new Set(result).size !== result.length)
      throw new Error(`Invalid import: empty or duplicate ${context} replacements`);
    return result;
  };
  const pairs = [];
  const seen = new Set();
  for (const raw of splitTopLevelArguments(literal.slice(1, -1))) {
    const entry = raw.replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*/g, '').trim();
    if (!entry) continue;
    const match = /^\[MoveId\.([A-Z0-9_]+)\]\s*:\s*([\s\S]+)$/.exec(entry);
    if (!match) throw new Error(`Invalid import: unsupported SUPERCEDED_MOVES entry ${entry}`);
    const sourceMoveId = moveId(match[1]);
    if (seen.has(sourceMoveId)) throw new Error(`Invalid import: duplicate superceded MoveId.${match[1]}`);
    seen.add(sourceMoveId);
    const replacementLiteral = match[2].startsWith('[') ? match[2] : aliases.get(match[2]);
    if (!replacementLiteral?.startsWith('[') || !replacementLiteral.endsWith(']'))
      throw new Error(`Invalid import: unknown superceded alias ${match[2]}`);
    for (const replacementMoveId of parseArray(replacementLiteral, `MoveId.${match[1]}`))
      pairs.push({ moveId: sourceMoveId, replacementMoveId });
  }
  if (!pairs.length) throw new Error('Invalid import: SUPERCEDED_MOVES map is empty');
  return pairs;
}

function parseMoveGenerationBlocklist(source, declaration, moveEnum) {
  const marker = new RegExp(`\\bexport\\s+const\\s+${declaration}\\s*:[^=]+?=\\s*new\\s+Set\\s*\\(`).exec(source);
  if (!marker) throw new Error(`Invalid import: ${declaration} declaration missing`);
  const open = source.indexOf('[', marker.index + marker[0].length);
  const literal = extractBalancedLiteral(source, open);
  if (!literal) throw new Error(`Invalid import: unclosed ${declaration} set`);
  const ids = [];
  for (const raw of splitTopLevelArguments(literal.slice(1, -1))) {
    const entry = raw.replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*/g, '').trim();
    if (!entry) continue;
    const match = /^MoveId\.([A-Z0-9_]+)$/.exec(entry);
    if (!match) throw new Error(`Invalid import: unsupported ${declaration} entry ${entry}`);
    const id = moveEnum.getId(match[1]);
    if (!Number.isInteger(id) || id < 1 || id > 0xFFFF)
      throw new Error(`Invalid import: unknown ${declaration} MoveId.${match[1]}`);
    ids.push(id);
  }
  if (!ids.length || new Set(ids).size !== ids.length)
    throw new Error(`Invalid import: empty or duplicate ${declaration} set`);
  return ids;
}

function parseForcedSignatureMoves(source, declaration, speciesEnum, moveEnum) {
  const marker = new RegExp(`\\bexport\\s+const\\s+${declaration}\\s*:[^=]+?=\\s*`).exec(source);
  if (!marker) throw new Error(`Invalid import: ${declaration} declaration missing`);
  const open = source.indexOf('{', marker.index + marker[0].length);
  const literal = extractBalancedLiteral(source, open);
  if (!literal) throw new Error(`Invalid import: unclosed ${declaration} map`);
  const entries = [];
  const seen = new Set();
  for (const raw of splitTopLevelArguments(literal.slice(1, -1))) {
    const entry = raw.replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*/g, '').trim();
    if (!entry) continue;
    const match = /^\[SpeciesId\.([A-Z0-9_]+)\]\s*:\s*([\s\S]+)$/.exec(entry);
    if (!match) throw new Error(`Invalid import: unsupported ${declaration} entry ${entry}`);
    const speciesDex = speciesEnum.getId(match[1]);
    if (!Number.isInteger(speciesDex) || speciesDex < 1 || speciesDex > 0xFFFF || seen.has(speciesDex))
      throw new Error(`Invalid import: unknown or duplicate ${declaration} SpeciesId.${match[1]}`);
    seen.add(speciesDex);
    const isArray = match[2].startsWith('[');
    const rawMoves = isArray ? splitTopLevelArguments(match[2].slice(1, -1)) : [match[2]];
    if (isArray && !match[2].endsWith(']')) throw new Error(`Invalid import: unclosed ${declaration} move array`);
    const moveIds = rawMoves.map(rawMove => {
      const moveMatch = /^MoveId\.([A-Z0-9_]+)$/.exec(rawMove.trim());
      const moveId = moveMatch ? moveEnum.getId(moveMatch[1]) : null;
      if (!Number.isInteger(moveId) || moveId < 1 || moveId > 0xFFFF)
        throw new Error(`Invalid import: unknown ${declaration} move ${rawMove}`);
      return moveId;
    });
    if (!moveIds.length) throw new Error(`Invalid import: empty ${declaration} move array`);
    entries.push({ speciesDex, isArray, moveIds });
  }
  if (!entries.length) throw new Error(`Invalid import: empty ${declaration} map`);
  return entries;
}

function parseDerivedTrainerTypes(source, catalog) {
  const marker = /getDerivedType\s*\(/.exec(source);
  // Locate the declaration, excluding constructor calls to this method.
  const declaration = /^\s*getDerivedType\s*\([^\n]*\)\s*:\s*TrainerType\s*\{/m.exec(source);
  if (!marker || !declaration) throw new Error('Missing TrainerConfig.getDerivedType declaration');
  const body = extractBalancedLiteral(source, source.indexOf('{', declaration.index));
  const switchMatch = body?.match(/^\{\s*switch\s*\(\s*trainerType\s*\)\s*\{([\s\S]*)\}\s*\}$/);
  if (!switchMatch) throw new Error('Unsupported derived trainer type implementation');
  let remaining = switchMatch[1].trim();
  const aliases = new Map();
  while (remaining.startsWith('case')) {
    const keys = [];
    let match;
    while ((match = remaining.match(/^case\s+TrainerType\.([A-Z0-9_]+)\s*:/))) {
      keys.push(match[1]); remaining = remaining.slice(match[0].length).trim();
    }
    const target = remaining.match(/^return\s+TrainerType\.([A-Z0-9_]+)\s*;/);
    if (!keys.length || !target || !catalog.hasSymbol(target[1])) throw new Error('Unsupported derived trainer type branch');
    for (const key of keys) {
      if (!catalog.hasSymbol(key) || aliases.has(key)) throw new Error(`Invalid derived trainer alias ${key}`);
      aliases.set(key, catalog.getId(target[1]));
    }
    remaining = remaining.slice(target[0].length).trim();
  }
  if (!/^default\s*:\s*return\s+trainerType\s*;$/.test(remaining)) throw new Error('Unsupported derived trainer type fallback');
  return { aliases, raw: body };
}

function trainerInitializerDefinition(source, method) {
  const declaration = new RegExp(`^\\s*(?:public\\s+)?${method}\\s*\\(`, 'm').exec(source);
  if (!declaration) throw new Error(`Missing upstream TrainerConfig.${method}`);
  const open = source.indexOf('(', declaration.index);
  const parameters = extractBalancedLiteral(source, open);
  if (!parameters) throw new Error(`Unclosed TrainerConfig.${method} parameters`);
  const suffix = source.slice(open + parameters.length).match(/^\s*:\s*TrainerConfig\s*\{/);
  if (!suffix) throw new Error(`Unsupported TrainerConfig.${method} declaration`);
  const bodyOpen = open + parameters.length + suffix[0].lastIndexOf('{');
  const body = extractBalancedLiteral(source, bodyOpen);
  if (!body) throw new Error(`Unclosed TrainerConfig.${method} body`);
  const supported = new Set(['setPartyTemplates', 'setPartyTemplateFunc', 'setMoneyMultiplier', 'setBoss', 'setStaticParty']);
  const calls = trainerTopLevelCalls(body.slice(1, -1));
  return {
    sourceSymbol: `TrainerConfig.${method}`,
    raw: source.slice(declaration.index, bodyOpen + body.length),
    declarativeCalls: calls.filter(call => supported.has(call.method)),
    remainingCalls: calls.filter(call => !supported.has(call.method))
  };
}

// Source-derived rival pools stay structured; post-process callbacks remain raw.
export function parseRivalPartyConfiguration(source, speciesCatalog) {
  const pools = new Map();
  const parseSpecies = expression => {
    const value = expression.replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*/g, '').trim();
    if (value.startsWith('[')) {
      const literal = extractBalancedLiteral(value, 0);
      if (!literal || literal.length !== value.length) throw new Error('Invalid rival species array');
      return splitTopLevelArguments(literal.slice(1, -1)).filter(entry => entry.trim()).map(parseSpecies);
    }
    const symbol = value.match(/^SpeciesId\.([A-Z0-9_]+)$/)?.[1];
    if (!symbol || speciesCatalog.getId(symbol) == null) throw new Error(`Invalid rival species: ${value}`);
    return symbol.toLowerCase();
  };
  for (const declaration of source.matchAll(/\bconst\s+(SLOT_\d+_(?:FIGHT_\d+|FINAL))\s*=\s*\[/g)) {
    const start = declaration.index + declaration[0].length - 1;
    const literal = extractBalancedLiteral(source, start);
    if (!literal) throw new Error(`Unclosed rival pool ${declaration[1]}`);
    pools.set(declaration[1], { species: parseSpecies(literal), raw: literal });
  }
  const birdFunction = source.match(/function\s+forceRivalBirdAbility\s*\([^)]*\)\s*:\s*void\s*\{/);
  if (!birdFunction) throw new Error('Missing pinned rival bird post-process');
  const birdRaw = extractBalancedLiteral(source, birdFunction.index + birdFunction[0].length - 1);
  const birdAbilitySlots = new Map();
  for (const group of birdRaw.matchAll(/((?:\s*(?:\/\/[^\n]*\n)?\s*case\s+SpeciesId\.[A-Z0-9_]+\s*:\s*)+)\{\s*pokemon\.abilityIndex\s*=\s*([012])\s*;/g))
    for (const entry of group[1].matchAll(/SpeciesId\.([A-Z0-9_]+)/g)) birdAbilitySlots.set(entry[1].toLowerCase(), Number(group[2]));
  if (!birdAbilitySlots.size) throw new Error('Rival bird ability assignments could not normalize');
  const configurations = new Map();
  for (const declaration of source.matchAll(/export\s+const\s+(RIVAL_\d+_POOL)\s*:\s*RivalPoolConfig\s*=\s*\[/g)) {
    const literal = extractBalancedLiteral(source, declaration.index + declaration[0].length - 1);
    if (!literal) throw new Error(`Unclosed rival configuration ${declaration[1]}`);
    const slots = splitTopLevelArguments(literal.slice(1, -1)).filter(entry => entry.trim()).map((raw, slot) => {
      const poolKey = raw.match(/\bpool\s*:\s*(SLOT_\d+_(?:FIGHT_\d+|FINAL))\b/)?.[1];
      const pool = pools.get(poolKey);
      if (!pool) throw new Error(`Missing rival slot pool ${declaration[1]}:${slot}`);
      const postProcess = raw.match(/\bpostProcess\s*:\s*([\s\S]*?)(?:,\s*(?:balanceTypes|balanceWeaknesses)\s*:|\s*\}\s*$)/)?.[1]?.trim();
      if (!postProcess) throw new Error(`Missing rival slot post-process ${declaration[1]}:${slot}`);
      const forcedAbilities = [...birdAbilitySlots].map(([speciesId, abilityIndex]) => ({ speciesId, abilityIndex }));
      return { slot, poolKey, species: pool.species, poolRaw: pool.raw, postProcess,
        balanceTypes: /\bbalanceTypes\s*:\s*true\b/.test(raw),
        balanceWeaknesses: /\bbalanceWeaknesses\s*:\s*true\b/.test(raw),
        postProcessPolicy: postProcess === 'forceRivalStarterTraits' ? { kind: 'STARTER', abilityIndex: 0, teraPrimary: true }
          : postProcess === 'forceRivalBirdAbility' ? { kind: 'BIRD', forcedAbilities } : { kind: 'UNSUPPORTED_CALLBACK' }, raw };
    });
    configurations.set(declaration[1], slots);
  }
  if (!configurations.size) throw new Error('No pinned rival configurations found');
  return configurations;
}

function parseTrainerConfigDetails(raw, trainerType, speciesCatalog, trainerPoolTierCatalog, templateIds, partyStrengthCatalog, evolutionThresholds, configSource, gymTemplatePolicy, gruntTemplatePolicy) {
  const directCalls = trainerTopLevelCalls(raw);
  const helperCalls = [];
  const calls = [];
  for (const call of directCalls) {
    if (!call.method.startsWith('initFor')) { calls.push(call); continue; }
    const definition = trainerInitializerDefinition(configSource, call.method);
    helperCalls.push({ ...call, definition });
    calls.push(...definition.declarativeCalls);
  }
  // Upstream setters assign, rather than append. Preserve chain order and
  // allow a later explicit setter to override an initializer's assignment.
  const readMethod = method => calls.filter(call => call.method === `set${method}`).slice(-1).map(call => call.args);
  const partyCalls = readMethod('PartyTemplates');
  const partyTemplateKeys = [];
  const inlineTemplates = [];
  for (const args of partyCalls) for (const arg of args) {
    const normalizedArg = arg.replace(/^\s*(?:(?:\/\*[\s\S]*?\*\/)|(?:\/\/[^\n]*(?:\n|$))\s*)*/, '').trim();
    let key = normalizedArg.match(/^trainerPartyTemplates\.([A-Z][A-Z0-9_]*)$/)?.[1];
    if (!key && /^new\s+TrainerParty(?:Compound)?Template\s*\(/.test(normalizedArg)) {
      key = `TRAINER_${trainerType}_PARTY_${inlineTemplates.length}`;
      const wrappedSource = `export const trainerPartyTemplates = { ${key}: ${normalizedArg} };`;
      const [template] = parseTrainerPartyTemplates(wrappedSource, partyStrengthCatalog, evolutionThresholds);
      template.id = key.toLowerCase();
      template.templateKey = key;
      inlineTemplates.push({ ...template, sourceFragment: normalizedArg });
    }
    if (!key || (!templateIds.has(key) && !inlineTemplates.some(template => template.templateKey === key))) throw new Error(`Unsupported or missing static party template in TrainerConfig.${trainerType}: ${arg}`);
    partyTemplateKeys.push(key);
  }
  const poolCalls = readMethod('SpeciesPools');
  if (poolCalls.length > 1) throw new Error(`Duplicate TrainerConfig.setSpeciesPools call for ${trainerType}`);
  const speciesPools = [];
  if (poolCalls.length) {
    const expression = poolCalls[0][0]?.trim();
    if (!expression) throw new Error(`Empty TrainerConfig.setSpeciesPools for ${trainerType}`);
    const pools = [];
    if (expression.startsWith('{')) {
      const literal = extractBalancedLiteral(expression, 0);
      if (!literal || expression.slice(literal.length).trim()) throw new Error(`Unsupported TrainerConfig species pool object for ${trainerType}`);
      for (const rawEntry of splitTopLevelArguments(literal.slice(1, -1))) {
        const entry = rawEntry.replace(/^\s*(?:(?:\/\*[\s\S]*?\*\/)|(?:\/\/[^\n]*(?:\n|$))\s*)*/, '');
        const key = entry.match(/^\s*\[\s*TrainerPoolTier\.([A-Z][A-Z0-9_]*)\s*\]\s*:/)?.[1];
        if (!key || !trainerPoolTierCatalog.hasSymbol(key)) throw new Error(`Unknown TrainerPoolTier in ${trainerType}: ${entry.slice(0, 80)}`);
        const colon = entry.indexOf(':');
        const listExpression = entry.slice(colon + 1).trim();
        const list = extractBalancedLiteral(listExpression, 0);
        if (!list || !listExpression.startsWith('[') || listExpression.slice(list.length).trim()) throw new Error(`Unsupported species pool list for ${trainerType}.${key}`);
        pools.push({ tier: key.toLowerCase(), tierId: trainerPoolTierCatalog.getId(key), list });
      }
    } else if (expression.startsWith('[')) {
      const list = extractBalancedLiteral(expression, 0);
      if (!list || expression.slice(list.length).trim()) throw new Error(`Unsupported common species pool list for ${trainerType}`);
      pools.push({ tier: 'common', tierId: trainerPoolTierCatalog.getId('COMMON'), list });
    } else throw new Error(`Unsupported TrainerConfig species pool expression for ${trainerType}: ${expression.slice(0, 80)}`);
    for (const pool of pools) {
      const candidates = [];
      for (const rawCandidate of splitTopLevelArguments(pool.list.slice(1, -1))) {
        const candidate = rawCandidate.replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*/g, '').trim();
        if (!candidate) continue;
        const speciesExpression = candidate.startsWith('[') ? extractBalancedLiteral(candidate, 0) : null;
        if (candidate.startsWith('[') && (!speciesExpression || candidate.slice(speciesExpression.length).trim())) throw new Error(`Unsupported nested species choice for ${trainerType}.${pool.tier}`);
        const symbols = [...(speciesExpression ?? candidate).matchAll(/SpeciesId\.([A-Z][A-Z0-9_]*)/g)].map(item => item[1]);
        const expected = (speciesExpression ? speciesExpression.slice(1, -1) : candidate).replace(/SpeciesId\.[A-Z][A-Z0-9_]*/g, '').replace(/[\s,]/g, '');
        if (!symbols.length || expected) throw new Error(`Unsupported trainer species candidate ${trainerType}.${pool.tier}: ${candidate}`);
        const speciesIds = symbols.map(symbol => {
          if (!speciesCatalog.hasSymbol(symbol)) throw new Error(`Unknown SpeciesId.${symbol} in TrainerConfig.${trainerType}`);
          return symbol.toLowerCase();
        });
        candidates.push({ speciesIds, isGroup: speciesExpression !== null });
      }
      if (!candidates.length) throw new Error(`Empty trainer species pool ${trainerType}.${pool.tier}`);
      speciesPools.push({ tier: pool.tier, tierId: pool.tierId, candidates });
    }
  }
  const moneyCalls = readMethod('MoneyMultiplier');
  if (moneyCalls.length > 1 || (moneyCalls.length && (!/^\d+(?:\.\d+)?$/.test(moneyCalls[0][0]?.trim() ?? '')))) throw new Error(`Unsupported TrainerConfig money multiplier for ${trainerType}`);
  const moneyMultiplier = moneyCalls.length ? Number(moneyCalls[0][0].trim()) : 1;
  if (!Number.isFinite(moneyMultiplier) || moneyMultiplier < 0) throw new Error(`Invalid TrainerConfig money multiplier for ${trainerType}`);
  const partyTemplateFuncCalls = readMethod('PartyTemplateFunc');
  let partyTemplateStatus = partyTemplateFuncCalls.length ? 'DYNAMIC_FUNCTION_PRESERVED' : partyCalls.length ? 'STATIC_TEMPLATES' : 'MISSING_IN_UPSTREAM';
  const callbackTemplateKeys = [];
  let callbackWavePolicy = null;
  if (!partyCalls.length) {
    if (!templateIds.has('TWO_AVG')) throw new Error(`Pinned TrainerConfig default party template is unavailable for ${trainerType}`);
    partyTemplateKeys.push('TWO_AVG');
    if (!partyTemplateFuncCalls.length) partyTemplateStatus = 'UPSTREAM_DEFAULT_TEMPLATE';
  }
  if (partyTemplateFuncCalls.length > 1) throw new Error(`Duplicate TrainerConfig.setPartyTemplateFunc call for ${trainerType}`);
  if (partyTemplateFuncCalls.length) {
    const callback = partyTemplateFuncCalls[0][0]?.trim() ?? '';
    const waveCall = callback.match(/getWavePartyTemplate\s*\(/);
    if (/^(?:getEvilGruntPartyTemplate|\(\s*\)\s*=>\s*getEvilGruntPartyTemplate\(\s*\))$/.test(callback)) {
      callbackWavePolicy = gruntTemplatePolicy;
      callbackTemplateKeys.push(...gruntTemplatePolicy.ranges.map(range => range.templateKey));
      partyTemplateStatus = 'CLASSIC_WAVE_RANGE_TEMPLATES';
    }
    if (callback === 'getGymLeaderPartyTemplate') {
      callbackWavePolicy = gymTemplatePolicy;
      callbackTemplateKeys.push(...gymTemplatePolicy.ranges.map(range => range.templateKey));
      partyTemplateStatus = 'CLASSIC_WAVE_RANGE_TEMPLATES';
    }
    if (waveCall && /^\(\s*\)\s*=>\s*getWavePartyTemplate\s*\(/.test(callback)) {
      const open = callback.indexOf('(', waveCall.index);
      const literal = extractBalancedLiteral(callback, open);
      if (!literal || callback.slice(open + literal.length).trim()) throw new Error(`Unsupported wave-scaled party-template callback for ${trainerType}`);
      for (const arg of splitTopLevelArguments(literal.slice(1, -1))) {
        const key = arg.trim().match(/^trainerPartyTemplates\.([A-Z][A-Z0-9_]*)$/)?.[1];
        if (!key || !templateIds.has(key)) throw new Error(`Invalid wave-scaled party template for ${trainerType}: ${arg}`);
        callbackTemplateKeys.push(key);
      }
      if (!callbackTemplateKeys.length) throw new Error(`Empty wave-scaled party-template callback for ${trainerType}`);
      partyTemplateStatus = 'WAVE_SCALED_TEMPLATES';
    } else if (/^\(\s*\)\s*=>\s*trainerPartyTemplates\.([A-Z][A-Z0-9_]*)\s*$/.test(callback)) {
      const key = callback.match(/trainerPartyTemplates\.([A-Z][A-Z0-9_]*)/)?.[1];
      if (!templateIds.has(key)) throw new Error(`Missing callback party template for ${trainerType}: ${key}`);
      callbackTemplateKeys.push(key);
      partyTemplateStatus = 'STATIC_CALLBACK_TEMPLATE';
    }
  }
  let specialtyType = null;
  let specialtyTypeStatus = 'RESOLVED';
  for (const call of directCalls) {
    let expression;
    if (call.method === 'setSpecialtyType') expression = call.args[0];
    else if (call.method.startsWith('initFor')) {
      const definition = helperCalls.find(helper => helper.method === call.method)?.definition;
      const raw = definition?.raw ?? '';
      const parameters = extractBalancedLiteral(raw, raw.indexOf('('));
      const index = parameters ? splitTopLevelArguments(parameters.slice(1, -1)).findIndex(parameter => /^specialtyType\??\s*:/.test(parameter.trim())) : -1;
      if (index >= 0) expression = call.args[index];
    }
    if (expression === undefined) continue;
    const symbol = expression.trim().match(/^PokemonType\.([A-Z][A-Z0-9_]*)$/)?.[1];
    if (symbol) { specialtyType = symbol; specialtyTypeStatus = 'RESOLVED'; }
    else if (/^(null|undefined)$/.test(expression.trim())) { specialtyType = null; specialtyTypeStatus = 'RESOLVED'; }
    else specialtyTypeStatus = 'UNSUPPORTED_EXPRESSION';
  }
  const trainerRules = {
    specialtyType,
    specialtyTypeStatus,
    isBoss: readMethod('Boss').length > 0,
    hasDouble: readMethod('HasDouble').length > 0,
    doubleOnly: readMethod('DoubleOnly').length > 0,
    hasStaticParty: readMethod('StaticParty').length > 0,
    useSameSeedForAllMembers: readMethod('UseSameSeedForAllMembers').length > 0,
    // Direct initializer assignments are normalized from source. Conditional
    // filters, signature species and callbacks still require runtime support.
    partyTemplateStatus: helperCalls.length ? 'INITIALIZER_SEMANTICS_PRESERVED' : partyTemplateStatus,
    initializerCalls: helperCalls.map(call => ({ method: call.method, arguments: call.args, raw: call.raw, definition: call.definition })),
    callbackTemplateKeys,
    callbackWavePolicy,
    templateResolutionStatus: partyTemplateStatus
  };
  return { moneyMultiplier, partyTemplateKeys, speciesPools, trainerRules, inlineTemplates };
}

function parseFormLevelMoves(source, context) {
  const marker = /\bformLevelMoves\s*:\s*\{/g.exec(source);
  if (!marker) return null;
  const open = source.indexOf('{', marker.index);
  const literal = extractBalancedLiteral(source, open);
  if (!literal) throw new Error(`Invalid import: unclosed formLevelMoves object for ${context}`);
  const result = {};
  for (const rawEntry of splitTopLevelArguments(literal.slice(1, -1))) {
    const entry = rawEntry.replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*/g, '').trim();
    if (!entry) continue;
    const colon = entry.indexOf(':');
    if (colon < 0) throw new Error(`Invalid import: malformed formLevelMoves entry for ${context}: ${entry}`);
    const rawKey = entry.slice(0, colon).trim();
    const enumKey = rawKey.match(/^\[\s*SpeciesFormKey\.([A-Z0-9_]+)\s*\]$/);
    const stringKey = rawKey.match(/^(?:"([^"]*)"|'([^']*)'|([A-Za-z0-9_-]+))$/);
    if (!enumKey && !stringKey) throw new Error(`Invalid import: unsupported formLevelMoves key for ${context}: ${rawKey}`);
    const key = enumKey?.[1] ?? (stringKey[1] ?? stringKey[2] ?? stringKey[3]).toUpperCase();
    const arrayStart = entry.indexOf('[', colon + 1);
    const arrayLiteral = extractBalancedLiteral(entry, arrayStart);
    if (!arrayLiteral) throw new Error(`Invalid import: unsupported formLevelMoves value for ${context}.${key}`);
    result[key] = parseLevelMoveArray(arrayLiteral, `${context}.formLevelMoves.${key}`);
  }
  return result;
}

function readConstructorCall(source, openingIndex) {
  let depth = 0, quote = null, escaped = false;
  for (let index = openingIndex; index < source.length; index++) {
    const char = source[index];
    if (quote) {
      if (escaped) escaped = false;
      else if (char === '\\') escaped = true;
      else if (char === quote) quote = null;
      continue;
    }
    if (char === '"' || char === "'" || char === '`') { quote = char; continue; }
    if (char === '(') depth++;
    else if (char === ')' && --depth === 0) {
      return { argsText: source.slice(openingIndex + 1, index), closeIndex: index };
    }
  }
  return null;
}

function readChainedExpression(source, closeIndex) {
  let parens = 0, braces = 0, brackets = 0, quote = null, escaped = false, lineComment = false, blockComment = false;
  for (let index = closeIndex + 1; index < source.length; index++) {
    const char = source[index], next = source[index + 1];
    if (lineComment) { if (char === '\n') lineComment = false; continue; }
    if (blockComment) { if (char === '*' && next === '/') { blockComment = false; index++; } continue; }
    if (quote) {
      if (escaped) escaped = false;
      else if (char === '\\') escaped = true;
      else if (char === quote) quote = null;
      continue;
    }
    if (char === '/' && next === '/') { lineComment = true; index++; continue; }
    if (char === '/' && next === '*') { blockComment = true; index++; continue; }
    if (char === '"' || char === "'" || char === '`') { quote = char; continue; }
    if (char === '(') parens++;
    else if (char === ')') parens--;
    else if (char === '{') braces++;
    else if (char === '}') braces--;
    else if (char === '[') brackets++;
    else if (char === ']') brackets--;
    else if (char === ',' && parens === 0 && braces === 0 && brackets === 0) return source.slice(closeIndex + 1, index);
    else if (char === ';' && parens === 0 && braces === 0 && brackets === 0) return source.slice(closeIndex + 1, index);
  }
  return source.slice(closeIndex + 1);
}

function enumArgument(value, prefixes = []) {
  let result = String(value || '').trim();
  for (const prefix of prefixes) result = result.replace(new RegExp(`^${prefix}\\.`), '');
  return result;
}

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
    if (POKEROGUE_BASE_ATLAS_REVISION !== repos['pokerogue-assets'].revision) throw new Error('Pinned sprite atlas index revision mismatch');
    const indexedBaseFront = new Set(POKEROGUE_BASE_ATLAS_IDS.front);
    const indexedBaseBack = new Set(POKEROGUE_BASE_ATLAS_IDS.back);
    const indexedFormFront = new Set(POKEROGUE_FORM_ATLAS_KEYS.front);
    const indexedFormBack = new Set(POKEROGUE_FORM_ATLAS_KEYS.back);
    this.sourceType = CanonicalSourceType.UPSTREAM;
    this.importedFrom = 'pinned-canonical-content-import';
    this.productionCanonicalImport = true;

    const fixedPaths = [
      'src/enums/species-id.ts', 'src/enums/species-form-key.ts', 'src/enums/move-id.ts', 'src/enums/ability-id.ts', 'src/enums/trainer-type.ts',
      'src/enums/pokemon-type.ts', 'src/enums/berry-type.ts', 'src/enums/stat.ts', 'src/data/berry.ts', 'src/data/battler-tags.ts', 'src/phases/berry-phase.ts', 'src/enums/game-modes.ts', 'src/game-mode.ts',
      'src/constants.ts', 'src/data/exp.ts', 'src/enums/fixed-boss-waves.ts', 'src/data/trainers/fixed-battle-configs.ts',
      'src/data/moves/move.ts', 'src/data/abilities/init-abilities.ts', 'src/data/abilities/ab-attrs.ts', 'src/utils/common.ts',
      'src/modifier/modifier-type.ts', 'src/modifier/init-modifier-pools.ts', 'src/modifier/modifier.ts', 'src/phases/victory-phase.ts', 'src/phases/exp-phase.ts', 'src/data/trainers/rival-party-config.ts', 'src/ai/rival-team-gen.ts', 'src/data/trainers/trainer-config.ts', 'src/data/trainers/trainer-party-template.ts', 'src/data/balance/signature-species.ts',
      'src/enums/party-member-strength.ts', 'src/enums/evo-level-threshold-kind.ts', 'src/enums/trainer-pool-tier.ts', 'src/data/species-data-registry.ts',
      'src/data/balance/moves/moveset-generation.ts', 'src/data/balance/moves/egg-moves.ts',
      'src/data/balance/moves/superceded-moves.ts', 'src/data/balance/moves/forbidden-moves.ts',
      'src/data/balance/moves/signature-moves.ts', 'src/data/balance/starters.ts', 'src/constants/game-constants.ts'
    ];
    const requests = [
      ...fixedPaths.map(path => ({ repo: 'pokerogue', path })),
      ...[...generations].sort((a, b) => a - b).map(generation => ({ repo: 'pokerogue', path: `src/data/balance/species/generation-${String(generation).padStart(2, '0')}.ts`, generation })),
      ...[...locales].sort().flatMap(locale => ['game-mode', 'pokemon', 'pokemon-form', 'move', 'ability', 'modifier', 'modifier-type', 'berry', 'trainer-classes', 'trainer-names'].map(namespace => ({ repo: 'pokerogue-locales', path: `${locale}/${namespace}.json`, locale, namespace })))
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
    const formSpriteKeyEnum = new Map([...byPath.get('pokerogue:src/enums/species-form-key.ts').content
      .matchAll(/^\s*([A-Z][A-Z0-9_]*)\s*=\s*"([a-z0-9-]+)"/gm)]
      .map(match => [match[1], match[2]]));
    const sourceHash = item => PokerogueManifest.computeHash(item.content);
    const sourceRows = loaded.map(item => ({ repository: item.repo, revision: repos[item.repo].revision, sourcePath: item.path, hash: sourceHash(item) })).sort((a, b) => `${a.repository}:${a.sourcePath}`.localeCompare(`${b.repository}:${b.sourcePath}`));
    const experienceSource = byPath.get('pokerogue:src/data/exp.ts');
    const experienceGrowthRates = parseExperienceGrowthRates(experienceSource.content);
    const fixedBossWaveSource = byPath.get('pokerogue:src/enums/fixed-boss-waves.ts');
    const classicFixedBossWaves = parseClassicFixedBossWaves(fixedBossWaveSource.content);
    const fixedBattleSource = byPath.get('pokerogue:src/data/trainers/fixed-battle-configs.ts');
    const trainerConfigSource = byPath.get('pokerogue:src/data/trainers/trainer-config.ts');
    const trainerConfigRecords = parseTrainerConfigRecords(trainerConfigSource.content);
    const trainerPartyTemplateSource = byPath.get('pokerogue:src/data/trainers/trainer-party-template.ts');
    const partyStrengthCatalog = this.enumParser.parseEnum(byPath.get('pokerogue:src/enums/party-member-strength.ts').content, 'PartyMemberStrength', 'src/enums/party-member-strength.ts');
    const evolutionThresholdSource = byPath.get('pokerogue:src/enums/evo-level-threshold-kind.ts');
    const evolutionThresholds = parseEvoLevelThresholdKinds(evolutionThresholdSource.content);
    const parsedTrainerPartyTemplates = parseTrainerPartyTemplates(trainerPartyTemplateSource.content, partyStrengthCatalog, evolutionThresholds);
    const enumSpecs = [
      ['species', 'SpeciesId', 'src/enums/species-id.ts'], ['move', 'MoveId', 'src/enums/move-id.ts'],
      ['ability', 'AbilityId', 'src/enums/ability-id.ts'], ['trainerType', 'TrainerType', 'src/enums/trainer-type.ts'], ['type', 'PokemonType', 'src/enums/pokemon-type.ts'],
      ['trainerPoolTier', 'TrainerPoolTier', 'src/enums/trainer-pool-tier.ts'],
      ['gameMode', 'GameModes', 'src/enums/game-modes.ts']
    ];
    const enumCatalogs = {};
    for (const [key, name, path] of enumSpecs) enumCatalogs[key] = this.enumParser.parseEnum(byPath.get(`pokerogue:${path}`).content, name, path);
    const classicFixedBattleWaves = parseClassicFixedBattleWaves(fixedBattleSource.content, classicFixedBossWaves, enumCatalogs.trainerType);
    this.setEnumCatalogs({ species: enumCatalogs.species, moves: enumCatalogs.move, abilities: enumCatalogs.ability, types: enumCatalogs.type });

    const localeEntries = [];
    const localeLookup = new Map();
    for (const file of loaded.filter(item => item.repo === 'pokerogue-locales').sort((a, b) => a.path.localeCompare(b.path))) {
      const data = JSON.parse(file.content);
      this.localeImporter.parseLocale(file.content, file.locale, file.namespace, file.path, 'pokerogue-locales');
      const entries = file.namespace === 'modifier-type'
        ? Object.entries(data.ModifierType || {}).map(([id, value]) => [`ModifierType.${id}`, value])
        : Object.entries(data);
      for (const [id, value] of entries.sort(([a], [b]) => a.localeCompare(b))) {
        const enumKey = ({ pokemon: 'species', move: 'move', ability: 'ability' })[file.namespace];
        const compact = text => String(text).toLowerCase().replace(/[^a-z0-9]/g, '');
        const canonicalSymbol = enumKey ? enumCatalogs[enumKey].entries().find(([symbol]) => compact(symbol) === compact(id))?.[0] : null;
        const canonicalId = canonicalSymbol ? canonicalSymbol.toLowerCase() : compact(id);
        const entry = { id, canonicalId, locale: file.locale, namespace: file.namespace, value, source: { repository: 'pokerogue-locales', revision: repos['pokerogue-locales'].revision, sourcePath: file.path, sourceHash: sourceHash(file), sourceType: CanonicalSourceType.UPSTREAM } };
        localeEntries.push(entry);
        localeLookup.set(`${file.locale}:${file.namespace}:${id.toLowerCase()}`, value);
      }
    }

    const trainerTypeCatalog = enumCatalogs.trainerType;
    const derivedTrainerTypes = parseDerivedTrainerTypes(trainerConfigSource.content, trainerTypeCatalog);
    const signatureSource = byPath.get('pokerogue:src/data/balance/signature-species.ts');
    const signatureSpecies = parseTrainerSignatureSpecies(signatureSource.content, enumCatalogs.species);
    const trainerPartyTemplates = parsedTrainerPartyTemplates.map(template => new TrainerPartyTemplateDefinition({
      ...template,
      source: new SourceMetadata({ sourceType: CanonicalSourceType.UPSTREAM, sourceRepository: game.url, sourceRevision: game.revision,
        sourcePath: 'src/data/trainers/trainer-party-template.ts', sourceSymbol: `trainerPartyTemplates.${template.templateKey}`,
        sourceHash: sourceHash(trainerPartyTemplateSource) }),
      extensions: { upstreamRawRecord: { format: 'typescript-source-fragment', value: template.raw }, semantics: 'DECLARATIVE_PARTY_STRENGTH_ONLY' }
    }));
    const trainerPartyTemplateIds = new Set(trainerPartyTemplates.map(template => template.templateKey));
    const gymTemplatePolicy = {
      ...parseClassicGymTemplatePolicy(trainerPartyTemplateSource.content, trainerPartyTemplateIds),
      repository: game.url, revision: game.revision,
      sourcePath: 'src/data/trainers/trainer-party-template.ts', sourceHash: sourceHash(trainerPartyTemplateSource)
    };
    const gruntTemplatePolicy = {
      ...parseClassicGruntTemplatePolicy(trainerPartyTemplateSource.content, trainerPartyTemplateIds, classicFixedBossWaves),
      repository: game.url, revision: game.revision,
      sourcePath: 'src/data/trainers/trainer-party-template.ts', sourceHash: sourceHash(trainerPartyTemplateSource),
      waveProvenance: { repository: game.url, revision: game.revision, sourcePath: 'src/enums/fixed-boss-waves.ts', sourceSymbol: 'ClassicFixedBossWaves', sourceHash: sourceHash(fixedBossWaveSource) }
    };
    for (const [symbol, raw] of trainerConfigRecords) {
      for (const ref of raw.matchAll(/trainerPartyTemplates\.([A-Z][A-Z0-9_]*)/g)) {
        if (!trainerPartyTemplateIds.has(ref[1])) throw new Error(`Pinned trainer config TrainerType.${symbol} references missing party template ${ref[1]}`);
      }
    }
    const rivalSource = byPath.get('pokerogue:src/data/trainers/rival-party-config.ts');
    const rivalAlgorithmSource = byPath.get('pokerogue:src/ai/rival-team-gen.ts');
    const rivalConfigurations = parseRivalPartyConfiguration(rivalSource.content, enumCatalogs.species);
    const trainerTypeLocale = localeEntries.filter(entry => entry.namespace === 'trainer-classes' && entry.locale === 'en');
    const trainerLocaleById = new Map(trainerTypeLocale.map(entry => [String(entry.canonicalId).toLowerCase(), entry.value]));
    const trainers = trainerTypeCatalog.entries().map(([symbol, trainerTypeId]) => {
      const id = symbol.toLowerCase();
      const localeKey = symbol.replace(/[^a-z0-9]/gi, '').toLowerCase();
      const locale = trainerLocaleById.get(localeKey);
      const sourceRaw = trainerConfigRecords.get(symbol) ?? null;
      const config = sourceRaw === null ? { moneyMultiplier: null, partyTemplateKeys: [], speciesPools: [], trainerRules: { partyTemplateStatus: 'MISSING_IN_UPSTREAM' } }
        : parseTrainerConfigDetails(sourceRaw, symbol, enumCatalogs.species, enumCatalogs.trainerPoolTier, trainerPartyTemplateIds, partyStrengthCatalog, evolutionThresholds, trainerConfigSource.content, gymTemplatePolicy, gruntTemplatePolicy);
      const rivalCallbacks = sourceRaw ? trainerTopLevelCalls(sourceRaw).filter(call => call.method === 'setPartyMemberFunc') : [];
      config.trainerRules.rivalPartySlots = rivalCallbacks.flatMap(call => {
        const match = call.args[1]?.trim().match(/^getRandomRivalPartyMemberFunc\(\s*(RIVAL_\d+_POOL)\s*,\s*(\d+)\s*\)$/);
        if (!match) return [];
        const slot = Number(match[2]), slots = rivalConfigurations.get(match[1]);
        if (!slots?.[slot] || Number(call.args[0]) !== slot) throw new Error(`Invalid rival callback ${symbol}:${slot}`);
        return [{ ...slots[slot], configuration: match[1], referenceSpecies: rivalConfigurations.get('RIVAL_6_POOL')?.[slot]?.species,
          provenance: { repository: game.url, revision: game.revision, sourcePath: rivalSource.path,
            sourceSymbol: `${match[1]}[${slot}]`, sourceHash: sourceHash(rivalSource) },
          algorithmProvenance: { repository: game.url, revision: game.revision, sourcePath: rivalAlgorithmSource.path,
            sourceSymbol: 'getRandomRivalPartyMemberFunc', sourceHash: sourceHash(rivalAlgorithmSource), raw: rivalAlgorithmSource.content } }];
      });
      config.trainerRules.derivedTrainerTypeId = derivedTrainerTypes.aliases.get(symbol) ?? trainerTypeId;
      config.trainerRules.derivedTypeProvenance = { repository: game.url, revision: game.revision, sourcePath: 'src/data/trainers/trainer-config.ts', sourceSymbol: 'TrainerConfig.getDerivedType', sourceHash: sourceHash(trainerConfigSource), raw: derivedTrainerTypes.raw };
      const signature = signatureSpecies.get(symbol);
      config.trainerRules.signatureSpecies = signature?.choices ?? [];
      config.trainerRules.signatureSpeciesProvenance = signature ? { repository: game.url, revision: game.revision, sourcePath: 'src/data/balance/signature-species.ts', sourceSymbol: `signatureSpecies.${symbol}`, sourceHash: sourceHash(signatureSource), raw: signature.raw } : null;
      config.trainerRules.signatureCallbackInstalled = (config.trainerRules.initializerCalls ?? []).some(call =>
        ['initForGymLeader', 'initForEliteFour', 'initForEvilTeamLeader'].includes(call.method)
        && call.arguments[0]?.trim() === `signatureSpecies["${symbol}"]`);
      for (const inline of config.inlineTemplates ?? []) {
        if (trainerPartyTemplateIds.has(inline.templateKey)) throw new Error(`Duplicate derived inline party-template key: ${inline.templateKey}`);
        trainerPartyTemplateIds.add(inline.templateKey);
        trainerPartyTemplates.push(new TrainerPartyTemplateDefinition({
          ...inline,
          source: new SourceMetadata({ sourceType: CanonicalSourceType.UPSTREAM, sourceRepository: game.url, sourceRevision: game.revision,
            sourcePath: 'src/data/trainers/trainer-config.ts', sourceSymbol: `trainerConfigs[TrainerType.${symbol}].partyTemplates[${inline.templateKey}]`, sourceHash: sourceHash(trainerConfigSource) }),
          extensions: { upstreamRawRecord: { format: 'typescript-source-fragment', value: inline.sourceFragment }, semantics: 'DECLARATIVE_PARTY_STRENGTH_ONLY', derivedKey: 'STABLE_FROM_UPSTREAM_TRAINER_AND_SLOT' }
        }));
      }
      const { inlineTemplates, ...configData } = config;
      return new TrainerDefinition({
        id, trainerTypeId, name: typeof locale === 'string' ? locale : id,
        names: { en: typeof locale === 'string' ? locale : id },
        ...configData,
        source: new SourceMetadata({ sourceType: CanonicalSourceType.UPSTREAM, sourceRepository: game.url, sourceRevision: game.revision, sourcePath: 'src/enums/trainer-type.ts', sourceSymbol: `TrainerType.${symbol}`, sourceHash: sourceHash(byPath.get('pokerogue:src/enums/trainer-type.ts')) }),
        extensions: {
          configStatus: sourceRaw === null ? 'MISSING_IN_UPSTREAM' : 'NORMALIZED_SUBSET_WITH_RAW_PRESERVED',
          normalizedConfigFields: sourceRaw === null ? [] : ['moneyMultiplier', 'partyTemplateKeys', 'waveScaledPartyTemplatePolicy', 'speciesPools', 'isBoss', 'hasDouble', 'doubleOnly', 'hasStaticParty', 'useSameSeedForAllMembers', ...(config.trainerRules.rivalPartySlots.length ? ['rivalPartySlots'] : [])],
          unsupportedConfigFields: sourceRaw === null ? [] : ['partyTemplateFunc (except recognized wave-scaled/static callback forms)', 'partyMemberFuncs (except normalized rival pools; runtime support varies)', 'speciesFilter', 'AI', 'modifiers', 'dialogue', 'BGM', 'variant names'],
          ...(config.trainerRules.rivalPartySlots.length ? { upstreamRivalConfig: {
            repository: game.url, revision: game.revision, sourcePath: rivalSource.path,
            sourceSymbol: config.trainerRules.rivalPartySlots[0].configuration, sourceHash: sourceHash(rivalSource), raw: rivalSource.content
          } } : {}),
          partyTemplateStatus: config.trainerRules.partyTemplateStatus,
          ...(sourceRaw === null ? {} : { upstreamConfig: { repository: game.url, revision: game.revision, sourcePath: 'src/data/trainers/trainer-config.ts', sourceSymbol: `trainerConfigs[TrainerType.${symbol}]`, sourceHash: sourceHash(trainerConfigSource), raw: sourceRaw } }),
          localizationStatus: typeof locale === 'string' ? 'RESOLVED' : 'MISSING_IN_UPSTREAM',
          localizationNamespace: 'trainer-classes'
        }
      });
    }).sort((left, right) => left.trainerTypeId - right.trainerTypeId);
    this.manifest.recordFile('pokerogue', game.revision, 'src/enums/trainer-type.ts', byPath.get('pokerogue:src/enums/trainer-type.ts').content);
    this.manifest.recordFile('pokerogue', game.revision, 'src/data/trainers/trainer-config.ts', trainerConfigSource.content);
    this.manifest.recordFile('pokerogue', game.revision, rivalSource.path, rivalSource.content);
    this.manifest.recordFile('pokerogue', game.revision, rivalAlgorithmSource.path, rivalAlgorithmSource.content);
    this.manifest.recordFile('pokerogue', game.revision, 'src/data/trainers/trainer-party-template.ts', trainerPartyTemplateSource.content);
    this.manifest.recordFile('pokerogue', game.revision, 'src/enums/party-member-strength.ts', byPath.get('pokerogue:src/enums/party-member-strength.ts').content);
    this.manifest.recordFile('pokerogue', game.revision, 'src/enums/evo-level-threshold-kind.ts', evolutionThresholdSource.content);
    for (const trainer of trainers) this.manifest.recordEntity('TrainerType', trainer.id, {
      repository: 'pokerogue', revision: game.revision, sourcePath: trainer.source.sourcePath,
      sourceSymbol: trainer.source.sourceSymbol, hash: trainer.source.sourceHash
    });
    const referencedTrainerTypes = new Set([...trainerConfigRecords.keys()]);
    for (const symbol of referencedTrainerTypes) if (!trainerTypeCatalog.hasSymbol(symbol)) throw new Error(`Pinned trainer config references missing TrainerType.${symbol}`);

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
        // SpeciesDataRegistry.isStarter checks the cost declared on this record;
        // `starter` alone identifies an evolution line and does not grant eligibility.
        const sourceRecord = record.extensions?.upstreamRawRecord?.value || '';
        const changeDeclaration = /\bformChanges\s*:\s*\[/.exec(sourceRecord);
        const changeText = changeDeclaration
          ? extractBalancedLiteral(sourceRecord, changeDeclaration.index + changeDeclaration[0].lastIndexOf('[')) : '[]';
        if (!changeText) throw new Error(`Invalid import: unclosed form changes for ${record.id}`);
        const changes = [];
        const changeConstructor = /new\s+SpeciesFormChange\s*\(\s*\{/g;
        let changeMatch;
        while ((changeMatch = changeConstructor.exec(changeText))) {
          const opening = changeMatch.index + changeMatch[0].lastIndexOf('{');
          const raw = extractBalancedLiteral(changeText, opening);
          if (!raw) throw new Error(`Invalid import: unclosed SpeciesFormChange for ${record.id}`);
          const key = raw.match(/\bevoFormKey\s*:\s*(?:SpeciesFormKey\.([A-Z0-9_]+)|"([^"]*)"|'([^']*)')/);
          const owner = raw.match(/\bspeciesId\s*:\s*SpeciesId\.([A-Z0-9_]+)/)?.[1];
          if (!key || owner !== symbol)
            throw new Error(`Unsupported form change identity for ${record.id}: ${raw}`);
          changes.push({ formKey: key[1] || (key[2] ?? key[3]).toUpperCase() || 'BASE',
            sourceSymbol: `SpeciesId.${symbol}.formChanges`, raw });
          changeConstructor.lastIndex = opening + raw.length;
        }
        record.extensions.upstreamFormChanges = changes;
        const starterCost = sourceRecord.match(/\bstarterCost\s*:\s*(\d+)/);
        record.starterCost = starterCost ? Number(starterCost[1]) : null;
        record.starterEligible = Boolean(record.starterCost);
        record.starterSpeciesId = sourceRecord.match(/\bstarter\s*:\s*SpeciesId\.([A-Z0-9_]+)/)?.[1]?.toLowerCase() ?? null;
        record.source.sourceHash = sourceHash(file);
        record.source.sourceSymbol = `SpeciesId.${symbol}`;
        const namedFront = POKEROGUE_FORM_ATLAS_KEYS.front.filter(key => key.startsWith(`${numericId}-`));
        const namedBack = POKEROGUE_FORM_ATLAS_KEYS.back.filter(key => key.startsWith(`${numericId}-`));
        const assetReference = {
          repository: repos['pokerogue-assets'].url, revision: repos['pokerogue-assets'].revision,
          speciesId: numericId, resolution: indexedBaseFront.has(numericId) ? 'INDEXED_BASE_ATLAS'
            : namedFront.length ? 'INDEXED_NAMED_FORM_ATLASES' : 'MISSING_IN_PINNED_ASSET_TREE',
          sourcePath: indexedBaseFront.has(numericId) ? `images/pokemon/${numericId}.json` : null,
          backSourcePath: indexedBaseBack.has(numericId) ? `images/pokemon/back/${numericId}.json` : null,
          namedFrontPaths: namedFront.map(key => `images/pokemon/${key}.json`),
          namedBackPaths: namedBack.map(key => `images/pokemon/back/${key}.json`),
          manifestVerified: false, imageVerified: false
        };
        record.extensions = { ...record.extensions, assetReference };
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
              const formKeyMatch = rawForm.match(/formKey\s*:\s*(?:SpeciesFormKey\.([A-Z0-9_]+)|["']([^"']*)["']|([A-Za-z0-9_-]+))/);
              const formKey = formKeyMatch?.[1] || formKeyMatch?.[2]?.toUpperCase() || formKeyMatch?.[3]?.toUpperCase() || 'BASE';
              const upstreamFormKey = formKeyMatch?.[1] ? formSpriteKeyEnum.get(formKeyMatch[1])
                : formKeyMatch?.[2] ?? formKeyMatch?.[3] ?? '';
              const spriteMatch = rawForm.match(/\bformSpriteKey\s*:\s*(?:SpeciesFormKey\.([A-Z0-9_]+)|"([^"]*)"|'([^']*)'|(null))/);
              const spriteDeclared = /\bformSpriteKey\s*:/.test(rawForm);
              const formSpriteKey = !spriteDeclared || spriteMatch?.[4] === 'null' ? upstreamFormKey
                : spriteMatch?.[1] ? formSpriteKeyEnum.get(spriteMatch[1])
                : spriteMatch?.[2] ?? spriteMatch?.[3];
              const spriteAtlasKey = typeof formSpriteKey === 'string'
                ? `${numericId}${formSpriteKey ? `-${formSpriteKey}` : ''}` : null;
              const frontIndexed = spriteAtlasKey && (spriteAtlasKey === String(numericId)
                ? indexedBaseFront.has(numericId) : indexedFormFront.has(spriteAtlasKey));
              const backIndexed = spriteAtlasKey && (spriteAtlasKey === String(numericId)
                ? indexedBaseBack.has(numericId) : indexedFormBack.has(spriteAtlasKey));
              const levelMoves = record.extensions?.upstreamFormLevelMoves?.[formKey] ?? [];
              // PokemonForm constructor defaults from the pinned source. Unknown expressions
              // remain explicit; they must never be treated as an unlock permission.
              const formBoolean = (key, fallback) => {
                const declared = rawForm.match(new RegExp(`\\b${key}\\s*:\\s*([^,}]+)`));
                if (!declared) return fallback;
                const value = declared[1].trim();
                return value === 'true' ? true : value === 'false' ? false : null;
              };
              const isUnobtainable = formBoolean('isUnobtainable', false);
              const isStarterSelectable = formBoolean('isStarterSelectable',
                typeof upstreamFormKey === 'string' ? !upstreamFormKey : null);
              forms.push({
                id: `${record.id}:${(formKey === 'BASE' ? `base_${formIndex}` : formKey.toLowerCase())}`,
                speciesId: record.id,
                formKey,
                formSpriteKey: formSpriteKey ?? null,
                spriteAtlasKey,
                name: formName,
                types: [...rawForm.matchAll(/type[12]\s*:\s*PokemonType\.([A-Z]+)/g)].map(match => match[1]),
                baseStats: Object.fromEntries(['Hp', 'Atk', 'Def', 'Spatk', 'Spdef', 'Spd'].map(stat => [stat.toLowerCase(), Number(rawForm.match(new RegExp(`base${stat}\\s*:\\s*(\\d+)`))?.[1] || 0)])),
                levelMoves,
                assetReference: { repository: repos['pokerogue-assets'].url,
                  revision: repos['pokerogue-assets'].revision, speciesId: numericId,
                  resolution: !spriteAtlasKey ? 'NOT_YET_SUPPORTED_BY_GUI_3DS'
                    : frontIndexed ? 'INDEXED_FORM_ATLAS' : 'MISSING_IN_PINNED_ASSET_TREE',
                  sourcePath: frontIndexed ? `images/pokemon/${spriteAtlasKey}.json` : null,
                  backSourcePath: backIndexed ? `images/pokemon/back/${spriteAtlasKey}.json` : null,
                  manifestVerified: false, imageVerified: false },
                provenance: { sourceRepository: game.url, sourceRevision: game.revision, sourcePath: path, sourceSymbol: `SpeciesId.${symbol}.forms`, sourceType: CanonicalSourceType.UPSTREAM, sourceHash: sourceHash(file) },
                extensions: { upstreamFormIndex: formIndex, isUnobtainable, isStarterSelectable, upstreamRawRecord: { format: 'typescript-source-fragment', value: rawForm }, runtimeTransform: 'NOT_IMPORTED' }
              });
              formIndex++;
              ctorRe.lastIndex = end + 1;
            }
          }
        }
      }
    }
    const constantsSource = byPath.get('pokerogue:src/constants.ts').content;
    const defaultStarterBlock = constantsSource.match(/export\s+const\s+defaultStarterSpecies\s*:[^=]+?=\s*\[([\s\S]*?)\]/);
    if (!defaultStarterBlock) throw new Error('Pinned defaultStarterSpecies declaration was not found in src/constants.ts');
    const defaultStarterSymbols = [...defaultStarterBlock[1].matchAll(/SpeciesId\.([A-Z0-9_]+)/g)].map(match => match[1].toLowerCase());
    if (!defaultStarterSymbols.length || new Set(defaultStarterSymbols).size !== defaultStarterSymbols.length) throw new Error('Pinned defaultStarterSpecies declaration is empty or contains duplicates');
    const canonicalSpeciesById = new Map(species.map(item => [item.id, item]));
    const unresolvedDefaultStarters = defaultStarterSymbols.filter(id => !canonicalSpeciesById.has(id));
    if (unresolvedDefaultStarters.length) throw new Error(`Pinned default starters are absent from imported species: ${unresolvedDefaultStarters.join(', ')}`);
    const moveFile = byPath.get('pokerogue:src/data/moves/move.ts');
    const supercededMoveFile = byPath.get('pokerogue:src/data/balance/moves/superceded-moves.ts');
    const supercededMovePairs = parseSupercededMovePairs(supercededMoveFile.content, enumCatalogs.move);
    const forbiddenMoveFile = byPath.get('pokerogue:src/data/balance/moves/forbidden-moves.ts');
    const forbiddenSinglesMoveIds = parseMoveGenerationBlocklist(forbiddenMoveFile.content,
      'FORBIDDEN_SINGLES_MOVES', enumCatalogs.move);
    const levelBasedDenylistMoveIds = parseMoveGenerationBlocklist(forbiddenMoveFile.content,
      'LEVEL_BASED_DENYLIST', enumCatalogs.move);
    const forbiddenTmMoveIds = parseMoveGenerationBlocklist(forbiddenMoveFile.content,
      'FORBIDDEN_TM_MOVES', enumCatalogs.move);
    const signatureMoveFile = byPath.get('pokerogue:src/data/balance/moves/signature-moves.ts');
    const forcedSignatureMoves = parseForcedSignatureMoves(signatureMoveFile.content,
      'FORCED_SIGNATURE_MOVES', enumCatalogs.species, enumCatalogs.move);
    const forcedRivalSignatureMoves = parseForcedSignatureMoves(signatureMoveFile.content,
      'FORCED_RIVAL_SIGNATURE_MOVES', enumCatalogs.species, enumCatalogs.move);
    const signatureChanceSource = byPath.get('pokerogue:src/data/balance/moves/moveset-generation.ts');
    const signatureChanceMatch = /\bexport\s+const\s+FORCED_SIGNATURE_MOVE_CHANCE\s*=\s*(\d+)\b/.exec(signatureChanceSource.content);
    const forcedSignatureChance = signatureChanceMatch ? Number(signatureChanceMatch[1]) : null;
    if (!Number.isInteger(forcedSignatureChance) || forcedSignatureChance < 1 || forcedSignatureChance > 100)
      throw new Error('Invalid import: pinned forced signature move chance missing');
    const abilityFile = byPath.get('pokerogue:src/data/abilities/init-abilities.ts');
    const parsedMoves = this.parseMoves(moveFile.content, null,
      byPath.get('pokerogue:src/data/balance/moves/moveset-generation.ts')?.content ?? null);
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
      move.flags = { contact: null, protectable: null, sound: null, bullet: null };
      move.extensions = { ...move.extensions, runtimeBehavior: 'NOT_IMPORTED' };
    }
    const eggMoveFile = byPath.get('pokerogue:src/data/balance/moves/egg-moves.ts');
    const speciesByEnumId = new Map(species.map(item => [enumCatalogs.species.getId(item.id.toUpperCase()), item]));
    const moveByEnumId = new Map(moves.map(item => [item.moveId, item]));
    const eggMoveRecord = /\[SpeciesId\.([A-Z0-9_]+)\]\s*:\s*\[([^\]]*)\]/g;
    let eggMoveMatch;
    const importedEggSpecies = new Set();
    while ((eggMoveMatch = eggMoveRecord.exec(eggMoveFile.content)) !== null) {
      const speciesEnumId = enumCatalogs.species.getId(eggMoveMatch[1]);
      const speciesRecord = speciesByEnumId.get(speciesEnumId);
      if (!speciesRecord) throw new Error(`Pinned egg move record references missing species: SpeciesId.${eggMoveMatch[1]}`);
      if (importedEggSpecies.has(speciesEnumId)) throw new Error(`Duplicate pinned egg move record: SpeciesId.${eggMoveMatch[1]}`);
      const symbols = [...eggMoveMatch[2].matchAll(/MoveId\.([A-Z0-9_]+)/g)].map(match => match[1]);
      if (symbols.length !== 4) throw new Error(`Pinned egg move array must contain four entries: SpeciesId.${eggMoveMatch[1]}`);
      const canonicalMoves = symbols.map(symbol => {
        const moveId = enumCatalogs.move.getId(symbol);
        const record = moveByEnumId.get(moveId);
        if (moveId === undefined || !record) throw new Error(`Pinned egg move references missing canonical MoveId.${symbol}`);
        return record.id;
      });
      speciesRecord.eggMoves = canonicalMoves;
      speciesRecord.extensions = { ...speciesRecord.extensions,
        upstreamEggMoves: { sourcePath: 'src/data/balance/moves/egg-moves.ts', sourceSymbol: `speciesEggMoves[SpeciesId.${eggMoveMatch[1]}]`, sourceHash: sourceHash(eggMoveFile), raw: eggMoveMatch[0] } };
      importedEggSpecies.add(speciesEnumId);
    }
    if (!importedEggSpecies.size) throw new Error('Pinned speciesEggMoves declaration produced no egg move records');
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
    const modifierMarker = /const\s+modifierTypeInitObj\s*=\s*Object\.freeze\s*\(/.exec(modifierFile.content);
    if (!modifierMarker) throw new Error('Invalid import: pinned modifierTypeInitObj missing');
    const modifierOpening = modifierFile.content.indexOf('{', modifierMarker.index + modifierMarker[0].length);
    const modifierLiteral = extractBalancedLiteral(modifierFile.content, modifierOpening);
    if (!modifierLiteral) throw new Error('Invalid import: pinned modifierTypeInitObj is unclosed');
    const modifierLocales = new Map(loaded.filter(file => file.repo === 'pokerogue-locales' && file.namespace === 'modifier-type')
      .map(file => [file.locale, { file, values: JSON.parse(file.content) }]));
    const items = [];
    for (const entry of splitTopLevelArguments(modifierLiteral.slice(1, -1))) {
      const raw = entry.replace(/^\s*(?:(?:\/\/[^\n]*\n)|(?:\/\*[\s\S]*?\*\/))*\s*/, '');
      if (!raw) continue;
      const match = /^([A-Z][A-Z0-9_]*)\s*:\s*([\s\S]+)$/.exec(raw);
      if (!match) throw new Error(`Invalid import: unsupported modifierTypeInitObj entry ${raw.slice(0, 80)}`);
      const id = match[1];
      const english = modifierLocales.get('en')?.values?.ModifierType?.[id];
      const spanish = modifierLocales.get('es')?.values?.ModifierType?.[id]
        || modifierLocales.get('es-ES')?.values?.ModifierType?.[id];
      items.push(new ItemDefinition({ id, name: english?.name || id,
        names: { en: english?.name || id, es: spanish?.name || english?.name || id },
        category: null, tier: null, price: null, description: english?.description || '',
        descriptions: { en: english?.description || '', es: spanish?.description || '' },
        source: new SourceMetadata({ sourceType: 'UPSTREAM', sourceRepository: game.url, sourceRevision: game.revision,
          sourcePath: 'src/modifier/modifier-type.ts', sourceSymbol: `modifierTypeInitObj.${id}`,
          sourceHash: sourceHash(modifierFile), license: game.license }),
        extensions: { upstreamRawRecord: { format: 'typescript-source-fragment', value: raw },
          localeKey: `modifier-type:${id}`, runtimeBehavior: 'NOT_IMPORTED' } }));
    }
    if (!items.length) throw new Error('Invalid import: pinned modifierTypeInitObj has no entries');
    const modifierPoolFile = byPath.get('pokerogue:src/modifier/init-modifier-pools.ts');
    const modifierIds = new Set(items.map(item => item.id));
    const modifierPools = [];
    const poolAssignment = /\b(modifierPool|wildModifierPool|trainerModifierPool|enemyBuffModifierPool|dailyStarterModifierPool)\[ModifierTier\.([A-Z]+)\]\s*=\s*/g;
    let poolMatch;
    while ((poolMatch = poolAssignment.exec(modifierPoolFile.content)) !== null) {
      const open = modifierPoolFile.content.indexOf('[', poolMatch.index + poolMatch[0].length);
      const literal = extractBalancedLiteral(modifierPoolFile.content, open);
      if (!literal) throw new Error(`Invalid import: unclosed ${poolMatch[1]} ${poolMatch[2]} pool`);
      let slot = 0;
      for (const entry of splitTopLevelArguments(literal.slice(1, -1))) {
        const trimmed = entry.trim();
        if (!trimmed) continue;
        const constructor = /^new\s+WeightedModifierType\s*\(/.exec(trimmed);
        if (!constructor) throw new Error(`Invalid import: unsupported ${poolMatch[1]} ${poolMatch[2]} entry ${trimmed.slice(0, 80)}`);
        const argsOpen = trimmed.indexOf('(', constructor.index);
        const argsLiteral = extractBalancedLiteral(trimmed, argsOpen);
        if (!argsLiteral || argsOpen + argsLiteral.length !== trimmed.length)
          throw new Error(`Invalid import: malformed ${poolMatch[1]} ${poolMatch[2]} entry`);
        const args = splitTopLevelArguments(argsLiteral.slice(1, -1));
        const id = /^modifierTypes\.([A-Z][A-Z0-9_]*)$/.exec(args[0] || '')?.[1];
        if (!id || !modifierIds.has(id) || args.length < 2 || args.length > 3)
          throw new Error(`Invalid import: unresolved modifier pool reference ${trimmed.slice(0, 80)}`);
        const literalWeight = /^\d+(?:\.\d+)?$/.test(args[1]) ? Number(args[1]) : null;
        const maxWeight = args[2] === undefined ? null : /^\d+(?:\.\d+)?$/.test(args[2]) ? Number(args[2]) : null;
        modifierPools.push({ pool: poolMatch[1], tier: poolMatch[2], slot: slot++, itemId: id,
          weight: literalWeight, maxWeight, weightExpression: args[1], raw: trimmed,
          provenance: { repository: game.url, revision: game.revision,
            sourcePath: 'src/modifier/init-modifier-pools.ts',
            sourceSymbol: `${poolMatch[1]}[ModifierTier.${poolMatch[2]}]`, sourceHash: sourceHash(modifierPoolFile) } });
      }
      poolAssignment.lastIndex = open + literal.length;
    }
    if (!modifierPools.length) throw new Error('Invalid import: pinned modifier pools have no entries');
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
      const atlas = parsePokemonSpriteAtlas(content, path);
      item.extensions.assetReference = { repository: repos['pokerogue-assets'].url, revision: repos['pokerogue-assets'].revision,
        speciesId: item.speciesId, sourcePath: path, sourceHash: PokerogueManifest.computeHash(content),
        imagePath: atlas.imagePath, imageHash: null, verified: true, manifestVerified: true, imageVerified: false,
        backSourcePath: item.extensions.assetReference.backSourcePath,
        namedFrontPaths: item.extensions.assetReference.namedFrontPaths,
        namedBackPaths: item.extensions.assetReference.namedBackPaths,
        atlasWidth: atlas.width, atlasHeight: atlas.height, frames: atlas.frames,
        atlasFormat: atlas.format, extensions: atlas.extensions };
      return { repository: 'pokerogue-assets', revision: repos['pokerogue-assets'].revision, sourcePath: path, hash: PokerogueManifest.computeHash(content) };
    }));
    sourceRows.push(...assetMetadata);
    sourceRows.sort((a, b) => `${a.repository}:${a.sourcePath}`.localeCompare(`${b.repository}:${b.sourcePath}`));
    const snapshot = new SourceSnapshot({ repository: game.url, revision: game.revision, branch: game.branch, sourceType: CanonicalSourceType.UPSTREAM, contentRevision: game.revision, assetRevision: repos['pokerogue-assets'].revision, localeRevision: repos['pokerogue-locales'].revision, sources: sourceRows });
    const provenance = new Provenance({ sourceRepository: game.url, sourceRevision: game.revision, sourcePath: 'src/data/balance/species/generation-01.ts', sourceSymbol: 'initGenerationOne', sourceType: CanonicalSourceType.UPSTREAM, sourceHash: sourceRows.find(item => item.sourcePath.endsWith('generation-01.ts'))?.hash });
    const canonicalAssetReferences = species.map(item => ({ speciesId: item.id, ...item.extensions.assetReference }));
    const canonicalAssetBySpecies = new Map(canonicalAssetReferences.map(item => [item.speciesId, item]));
    const classicModeDefinition = gameModes.find(mode => mode.id === 'classic');
    if (!Number.isSafeInteger(classicModeDefinition?.rules?.maxWave) || !classicModeDefinition.provenance?.sourceHash) {
      throw new Error('Pinned Classic final-wave rule is missing or unsupported');
    }
    if (classicFixedBossWaves.some(entry => entry.wave > classicModeDefinition.rules.maxWave)) {
      throw new Error('Pinned Classic fixed-boss wave exceeds the imported Classic final wave');
    }
    const friendshipSource = byPath.get('pokerogue:src/data/balance/starters.ts');
    const friendshipConstants = byPath.get('pokerogue:src/constants.ts');
    const friendshipValue = (file, symbol, maximum = 255) => {
      const match = [...file.content.matchAll(/export\s+const\s+([A-Z0-9_]+)\s*=\s*(\d+)\s*;/g)]
        .find(entry => entry[1] === symbol);
      if (!match) throw new Error(`Missing pinned friendship rule: ${symbol}`);
      const value = Number(match[2]);
      if (!Number.isSafeInteger(value) || value > maximum) throw new Error(`Invalid pinned friendship rule: ${symbol}`);
      return { value, provenance: { repository: game.url, revision: game.revision, sourcePath: file.path,
        sourceSymbol: symbol, sourceHash: sourceHash(file) } };
    };
    const pokemonFriendshipRules = {
      battleGain: friendshipValue(friendshipSource, 'FRIENDSHIP_GAIN_FROM_BATTLE'),
      rareCandyGain: friendshipValue(friendshipSource, 'FRIENDSHIP_GAIN_FROM_RARE_CANDY'),
      faintLoss: friendshipValue(friendshipSource, 'FRIENDSHIP_LOSS_FROM_FAINT'),
      rareCandyCap: friendshipValue(friendshipConstants, 'RARE_CANDY_FRIENDSHIP_CAP')
    };
    const capSymbol = 'getStarterValueFriendshipCap';
    const capStart = friendshipSource.content.indexOf(`export function ${capSymbol}(`);
    if (capStart < 0) throw new Error('Missing pinned starter friendship cap function');
    const capOpen = friendshipSource.content.indexOf('{', capStart);
    const capRaw = extractBalancedLiteral(friendshipSource.content, capOpen);
    if (!capRaw) throw new Error('Unclosed pinned starter friendship cap function');
    const capClean = capRaw.slice(1, -1).replace(/\/\/[^\n]*|\/\*[\s\S]*?\*\//g, '');
    const capSwitch = /^\s*switch\s*\(\s*starterCost\s*\)\s*\{([\s\S]*)\}\s*$/.exec(capClean);
    if (!capSwitch) throw new Error('Unsupported pinned starter friendship cap body');
    const capEntries = [];
    let pendingCosts = [], pendingDefault = false, capFallback = null, capCursor = 0;
    for (const token of capSwitch[1].matchAll(/case\s+(\d+)\s*:|default\s*:|return\s+(\d+)\s*;/g)) {
      if (capSwitch[1].slice(capCursor, token.index).trim()) throw new Error('Unsupported starter cap branch syntax');
      capCursor = token.index + token[0].length;
      if (token[1] !== undefined) pendingCosts.push(Number(token[1]));
      else if (token[2] !== undefined) {
        const value = Number(token[2]);
        if (!Number.isSafeInteger(value) || value <= 0 || value > 0xffffffff) throw new Error('Invalid starter friendship threshold');
        for (const cost of pendingCosts) {
          if (!Number.isInteger(cost) || cost < 1 || cost > 255 || capEntries.some(entry => entry.cost === cost))
            throw new Error('Invalid/duplicate starter friendship cost');
          capEntries.push({ cost, value });
        }
        if (pendingDefault) {
          if (capFallback !== null) throw new Error('Duplicate starter friendship fallback');
          capFallback = value;
        }
        pendingCosts = []; pendingDefault = false;
      } else pendingDefault = true;
    }
    if (pendingCosts.length || pendingDefault || capFallback === null || !capEntries.length ||
        capSwitch[1].slice(capCursor).trim()) throw new Error('Incomplete starter friendship cap normalization');
    const starterCandyPrices = parseStarterCandyPriceTable(friendshipSource.content);
    starterCandyPrices.provenance = { repository: game.url, revision: game.revision,
      sourcePath: friendshipSource.path, sourceSymbol: 'allStarterCandyCosts', sourceHash: sourceHash(friendshipSource) };
    const starterCandyRules = {
      prices: starterCandyPrices,
      maxCandyCount: friendshipValue(byPath.get('pokerogue:src/constants/game-constants.ts'), 'MAX_STARTER_CANDY_COUNT', 65535),
      classicMultiplier: friendshipValue(friendshipSource, 'CLASSIC_CANDY_FRIENDSHIP_MULTIPLIER'),
      friendshipCaps: { entries: capEntries.sort((a, b) => a.cost - b.cost), fallback: capFallback,
        provenance: { repository: game.url, revision: game.revision, sourcePath: friendshipSource.path,
          sourceSymbol: capSymbol, sourceHash: sourceHash(friendshipSource) }, raw: capRaw }
    };
    const modifierBehaviorSource = byPath.get('pokerogue:src/modifier/modifier.ts');
    const boosterClass = modifierBehaviorSource.content.match(/export\s+class\s+ExpBoosterModifier\s+extends\s+PersistentModifier\s*\{/);
    if (!boosterClass) throw new Error('Missing pinned ExpBoosterModifier');
    const boosterRaw = extractBalancedLiteral(modifierBehaviorSource.content, boosterClass.index + boosterClass[0].length - 1);
    const limits = boosterRaw.match(/return\s+this\.boostMultiplier\s*<\s*([\d.]+)\s*\?\s*\(this\.boostMultiplier\s*<\s*([\d.]+)\s*\?\s*(\d+)\s*:\s*(\d+)\)\s*:\s*(\d+)\s*;/);
    if (!limits || !/boost\.value\s*=\s*Math\.floor\(boost\.value\s*\*\s*\(1\s*\+\s*this\.getStackCount\(\)\s*\*\s*this\.boostMultiplier\)\)/.test(boosterRaw))
      throw new Error('Unsupported pinned EXP booster stack/apply semantics');
    const victorySource = byPath.get('pokerogue:src/phases/victory-phase.ts');
    const victoryRaw = victorySource.content;
    const fixedRule = victoryRaw.match(/currentWaveIndex\s*<=\s*(\d+)\s*&&\s*\(currentWaveIndex\s*<=\s*(\d+)\s*\|\|\s*currentWaveIndex\s*%\s*(\d+)\s*===\s*superExpWave\)/);
    const offsetRule = victoryRaw.match(/const\s+superExpWave\s*=\s*gameMode\.isEndless\s*\?\s*\d+\s*:\s*globalScene\.offsetGym\s*\?\s*(\d+)\s*:\s*(\d+)\s*;/);
    const rewardRule = victoryRaw.match(/currentWaveIndex\s*%\s*(\d+)\s*!==\s*superExpWave\s*\|\|\s*currentWaveIndex\s*>\s*(\d+)[\s\S]*?modifierTypes\.([A-Z_]+)\s*:\s*modifierTypes\.([A-Z_]+)/);
    const firstRule = victoryRaw.match(/gameMode\.isClassic\s*&&\s*currentWaveIndex\s*===\s*(\d+)\)[\s\S]*?modifierTypes\.([A-Z_]+)/);
    const goldenRule = victoryRaw.match(/currentWaveIndex\s*<=\s*(\d+)\s*&&\s*!\(currentWaveIndex\s*%\s*(\d+)\)[\s\S]*?modifierTypes\.([A-Z_]+)/);
    const interval = victoryRaw.match(/if\s*\(currentWaveIndex\s*%\s*(\d+)\)/);
    if (!fixedRule || !offsetRule || !rewardRule || !firstRule || !goldenRule || !interval || fixedRule[3] !== rewardRule[1])
      throw new Error('Unsupported pinned Classic fixed reward policy');
    const metadata = (source, sourceSymbol) => ({ repository: game.url, revision: game.revision,
      sourcePath: source.path, sourceSymbol, sourceHash: sourceHash(source) });
    const persistentExperienceRules = {
      lowThresholdPercent: Number(limits[2]) * 100, highThresholdPercent: Number(limits[1]) * 100,
      lowMaxStacks: Number(limits[3]), middleMaxStacks: Number(limits[4]), highMaxStacks: Number(limits[5]),
      raw: boosterRaw, provenance: metadata(modifierBehaviorSource, 'ExpBoosterModifier'),
      phase: { raw: byPath.get('pokerogue:src/phases/exp-phase.ts').content,
        provenance: metadata(byPath.get('pokerogue:src/phases/exp-phase.ts'), 'ExpPhase.start') }
    };
    const classicFixedModifierRewards = {
      interval: Number(interval[1]), limitWave: Number(fixedRule[1]), regularUntilWave: Number(fixedRule[2]),
      superPeriod: Number(fixedRule[3]), offsetGymRemainder: Number(offsetRule[1]), ordinaryRemainder: Number(offsetRule[2]),
      maxSuperWave: Number(rewardRule[2]), experienceItemId: rewardRule[3], superExperienceItemId: rewardRule[4],
      extraFirstWave: Number(firstRule[1]), extraFirstItemId: firstRule[2],
      maxGoldenWave: Number(goldenRule[1]), goldenInterval: Number(goldenRule[2]), goldenItemId: goldenRule[3],
      raw: victoryRaw, provenance: metadata(victorySource, 'VictoryPhase.start')
    };
    const berryEnumSource = byPath.get('pokerogue:src/enums/berry-type.ts');
    const berryEnum = this.enumParser.parseEnum(berryEnumSource.content, 'BerryType', berryEnumSource.path);
    const berryItem = items.find(item => item.id === 'BERRY');
    if (!berryItem) throw new Error('Missing pinned BERRY generator');
    const berryGeneration = parseBerryGeneration(berryItem.extensions.upstreamRawRecord.value, berryEnum.entries());
    const berryClass = modifierBehaviorSource.content.match(/export\s+class\s+BerryModifier\s+extends\s+PokemonHeldItemModifier\s*\{/);
    if (!berryClass) throw new Error('Missing pinned BerryModifier');
    const berryBehaviorRaw = extractBalancedLiteral(modifierBehaviorSource.content, berryClass.index + berryClass[0].length - 1);
    berryGeneration.heldLimits = parseBerryHeldLimits(berryBehaviorRaw, berryEnum.entries());
    berryGeneration.behavior = { raw: berryBehaviorRaw, provenance: metadata(modifierBehaviorSource, 'BerryModifier') };
    const preserveClass = modifierBehaviorSource.content.match(/export\s+class\s+PreserveBerryModifier\s+extends\s+PersistentModifier\s*\{/);
    if (!preserveClass) throw new Error('Missing pinned PreserveBerryModifier');
    const preserveRaw = extractBalancedLiteral(modifierBehaviorSource.content, preserveClass.index + preserveClass[0].length - 1);
    berryGeneration.preservation = parseBerryPreservation(preserveRaw);
    berryGeneration.preservation.provenance = metadata(modifierBehaviorSource, 'PreserveBerryModifier.apply');

    berryGeneration.provenance = berryItem.source;
    berryGeneration.enumProvenance = metadata(berryEnumSource, 'BerryType');
    const berryEffectSource = byPath.get('pokerogue:src/data/berry.ts');
    const berryPhaseSource = byPath.get('pokerogue:src/phases/berry-phase.ts');
    const berryStatSource = byPath.get('pokerogue:src/enums/stat.ts');
    const berryStats = this.enumParser.parseEnum(berryStatSource.content, 'Stat', berryStatSource.path);
    berryGeneration.effects = parseBerryEffects(berryEffectSource.content, berryEnum.entries(), berryStats.entries());
    berryGeneration.effects.provenance = metadata(berryEffectSource, 'getBerryPredicate/getBerryEffectFunc');
    berryGeneration.effects.statProvenance = metadata(berryStatSource, 'Stat');
    berryGeneration.phase = { raw: berryPhaseSource.content, provenance: metadata(berryPhaseSource, 'BerryPhase.eatBerries') };
    const berryAbilitySource = byPath.get('pokerogue:src/data/abilities/ab-attrs.ts');
    berryGeneration.abilityRules = parseBerryAbilityRules(berryAbilitySource.content, abilities);
    berryGeneration.abilityRules.provenance = metadata(berryAbilitySource, 'AbAttr/Berry-related subclasses');
    const berryRoundingSource = byPath.get('pokerogue:src/utils/common.ts');
    berryGeneration.healingRounding = parseBerryHealingRounding(berryRoundingSource.content);
    berryGeneration.healingRounding.provenance = metadata(berryRoundingSource, 'toDmgValue');
    const berryTagSource = byPath.get('pokerogue:src/data/battler-tags.ts');
    const criticalClass = berryTagSource.content.match(/export\s+class\s+CritBoostTag\s+extends\s+SerializableBattlerTag\s*\{/);
    if (!criticalClass) throw new Error('Missing pinned CritBoostTag');
    const criticalRaw = extractBalancedLiteral(berryTagSource.content, criticalClass.index + criticalClass[0].length - 1);
    berryGeneration.criticalTag = parseBerryCriticalTag(criticalRaw);
    berryGeneration.criticalTag.provenance = metadata(berryTagSource, 'CritBoostTag.onAdd/lapse/loadTag');


    const canonicalContent = new CanonicalContent({ schemaVersion: '1.0.0', contentVersion: '1.0.0', sourceSnapshot: snapshot, provenance, collections: { gameModes, species, forms, moves, abilities, items, trainers, trainerPartyTemplates, locales: localeEntries, assetReferences: [...canonicalAssetBySpecies.values()] }, extensions: { berryGeneration, pokemonFriendshipRules, starterCandyRules, persistentExperienceRules, classicFixedModifierRewards, importer: 'PokerogueImporter.importCanonicalContent', unknownFieldPolicy: 'preserve-in-upstreamRawRecord', unsupportedBehavior: 'NOT_IMPORTED', modifierPools: { entries: modifierPools, dynamicWeights: modifierPools.filter(entry => entry.weight === null).length }, trainerMoveSupercedence: { pairs: supercededMovePairs, provenance: { repository: game.url, revision: game.revision, sourcePath: 'src/data/balance/moves/superceded-moves.ts', sourceSymbol: 'SUPERCEDED_MOVES', sourceHash: sourceHash(supercededMoveFile) } }, freshProfile: { defaultStarterSpecies: defaultStarterSymbols, provenance: { repository: game.url, revision: game.revision, sourcePath: 'src/constants.ts', sourceSymbol: 'defaultStarterSpecies', sourceHash: sourceHash(byPath.get('pokerogue:src/constants.ts')) } }, pokemonExperience: { growthRates: experienceGrowthRates, provenance: { repository: game.url, revision: game.revision, sourcePath: 'src/data/exp.ts', sourceSymbol: 'GrowthRate/expLevels/getLevelTotalExp', sourceHash: sourceHash(experienceSource) } }, classicFixedBossWaves: { entries: classicFixedBossWaves, provenance: { repository: game.url, revision: game.revision, sourcePath: 'src/enums/fixed-boss-waves.ts', sourceSymbol: 'ClassicFixedBossWaves', sourceHash: sourceHash(fixedBossWaveSource) }, finalWave: { wave: classicModeDefinition.rules.maxWave, provenance: { repository: game.url, revision: game.revision, sourcePath: classicModeDefinition.provenance.sourcePath, sourceSymbol: 'GameMode.isWaveFinal:classic', sourceHash: classicModeDefinition.provenance.sourceHash } } }, classicFixedBattleWaves: { entries: classicFixedBattleWaves, provenance: { repository: game.url, revision: game.revision, sourcePath: 'src/data/trainers/fixed-battle-configs.ts', sourceSymbol: 'classicFixedBattles', sourceHash: sourceHash(fixedBattleSource) } }, trainerPartyTemplateCatalog: { provenance: { repository: game.url, revision: game.revision, sourcePath: 'src/data/trainers/trainer-party-template.ts', sourceSymbol: 'trainerPartyTemplates', sourceHash: sourceHash(byPath.get('pokerogue:src/data/trainers/trainer-party-template.ts')) }, partyStrengthSource: { sourcePath: 'src/enums/party-member-strength.ts', sourceHash: sourceHash(byPath.get('pokerogue:src/enums/party-member-strength.ts')) }, evolutionThresholdSource: { sourcePath: 'src/enums/evo-level-threshold-kind.ts', sourceHash: sourceHash(byPath.get('pokerogue:src/enums/evo-level-threshold-kind.ts')) }, trainerPoolTierSource: { sourcePath: 'src/enums/trainer-pool-tier.ts', sourceHash: sourceHash(byPath.get('pokerogue:src/enums/trainer-pool-tier.ts')) } }, skippedMoveRecords: parsedMoves.filter(move => enumCatalogs.move.getId(move.id.toUpperCase()) === undefined).map(move => ({ sourceSymbol: move.id, raw: move.extensions?.upstreamRawRecord || null })) } });
    canonicalContent.extensions.trainerMoveBlocklists = {
      singles: forbiddenSinglesMoveIds,
      levelBased: levelBasedDenylistMoveIds,
      tm: forbiddenTmMoveIds,
      provenance: { repository: game.url, revision: game.revision,
        sourcePath: 'src/data/balance/moves/forbidden-moves.ts',
        sourceSymbol: 'FORBIDDEN_SINGLES_MOVES/LEVEL_BASED_DENYLIST/FORBIDDEN_TM_MOVES',
        sourceHash: sourceHash(forbiddenMoveFile) }
    };
    canonicalContent.extensions.forcedSignatureMoves = {
      regular: forcedSignatureMoves,
      rival: forcedRivalSignatureMoves,
      chancePercent: forcedSignatureChance,
      provenance: { repository: game.url, revision: game.revision,
        sourcePath: 'src/data/balance/moves/signature-moves.ts',
        sourceSymbol: 'FORCED_SIGNATURE_MOVES/FORCED_RIVAL_SIGNATURE_MOVES',
        sourceHash: sourceHash(signatureMoveFile) },
      chanceProvenance: { repository: game.url, revision: game.revision,
        sourcePath: 'src/data/balance/moves/moveset-generation.ts',
        sourceSymbol: 'FORCED_SIGNATURE_MOVE_CHANCE',
        sourceHash: sourceHash(signatureChanceSource) }
    };
    console.log('[canonical-import] hashing canonical snapshot');
    const errors = canonicalContent.validate();
    if (errors.length) throw new Error(`Invalid canonical production import: ${errors.join('; ')}`);
    const duplicateIds = records => { const ids = records.map(item => item.id); return ids.filter((id, index) => ids.indexOf(id) !== index); };
    for (const [label, records] of Object.entries({ species, forms, moves, abilities, items, trainers, trainerPartyTemplates, gameModes })) { const duplicates = duplicateIds(records); if (duplicates.length) throw new Error(`Invalid import: duplicate ${label} IDs ${[...new Set(duplicates)].join(', ')}`); }
    const speciesIds = new Set(species.map(item => item.id));
    const speciesById = new Map(species.map(item => [item.id, item]));
    const abilityIds = new Set(abilities.map(item => item.id));
    for (const form of forms) if (!speciesIds.has(form.speciesId)) throw new Error(`Invalid cross-reference: form ${form.id} refers to missing species ${form.speciesId}`);
    for (const item of species) for (const evolution of item.evolutions) {
      if (!evolution.targetSpeciesId) {
        throw new Error(`Invalid cross-reference: species ${item.id} evolution target ${evolution.targetSpeciesId || '(unparsed)'} is absent from the pinned catalog`);
      }
      if (evolution.level == null) throw new Error(`Invalid import: species ${item.id} evolution to ${evolution.targetSpeciesId} has no supported numeric level`);
      const targetSpecies = speciesById.get(evolution.targetSpeciesId);
      if (!targetSpecies && generations.length === 9) {
        throw new Error(`Invalid cross-reference: species ${item.id} evolution target ${evolution.targetSpeciesId} is absent from the full pinned catalog`);
      }
      evolution.extensions = {
        ...evolution.extensions,
        targetReferenceStatus: targetSpecies ? 'RESOLVED' : 'NOT_INCLUDED_IN_FILTERED_SNAPSHOT'
      };
      if (targetSpecies && evolution.targetSpeciesEnumId != null && evolution.targetSpeciesEnumId !== targetSpecies.speciesId) {
        throw new Error(`Invalid cross-reference: species ${item.id} evolution enum ID disagrees with canonical target ${evolution.targetSpeciesId}`);
      }
    }
    // Port the registry's initialization rather than guessing a predecessor
    // from one incoming edge: form evolutions may repeat the same target and
    // starter resets happen in numeric SpeciesId order.
    const registryFile = byPath.get('pokerogue:src/data/species-data-registry.ts');
    const megaKeysLiteral = registryFile.content.match(/const\s+megaFormKeys\s*=\s*\[([\s\S]*?)\]/)?.[1];
    if (!megaKeysLiteral) throw new Error('Invalid import: SpeciesDataRegistry.initPreEvolutions mega-form exclusions were not found');
    const excludedPrevolutionForms = new Set([...megaKeysLiteral.matchAll(/SpeciesFormKey\.([A-Z0-9_]+)/g)].map(match => match[1]));
    const prevolutionSource = { sourceRepository: game.url, sourceRevision: game.revision, sourcePath: registryFile.path,
      sourceSymbol: 'SpeciesDataRegistry.initPreEvolutions', sourceHash: sourceHash(registryFile), sourceType: CanonicalSourceType.UPSTREAM };
    for (const item of species) {
      if (item.extensions.upstreamEvolutionDeclarations?.unsupported?.length) throw new Error(`Invalid import: unsupported evolution declaration for ${item.id}`);
      item.prevolutionSpeciesId = null;
      item.extensions.prevolution = { status: generations.length === 9 ? 'RESOLVED' : 'FILTERED_SNAPSHOT', source: prevolutionSource };
    }
    const assignPrevolutions = (item, visiting) => {
      if (visiting.has(item.id)) throw new Error(`Invalid import: cyclic evolution chain at ${item.id}`);
      const nextVisiting = new Set(visiting).add(item.id);
      for (const evolution of item.evolutions) {
        if (excludedPrevolutionForms.has(evolution.evoFormKey)) continue;
        const target = speciesById.get(evolution.targetSpeciesId);
        if (!target) continue; // A filtered snapshot records unresolved targets above.
        target.prevolutionSpeciesId = item.id;
        assignPrevolutions(target, nextVisiting);
      }
    };
    for (const starter of [...species].filter(item => item.starterEligible).sort((a, b) => a.speciesId - b.speciesId)) {
      starter.prevolutionSpeciesId = null;
      assignPrevolutions(starter, new Set());
    }
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
    const report = { schemaVersion: '1.0.0', sourceRevisions: { pokerogue: game.revision, assets: repos['pokerogue-assets'].revision, locales: repos['pokerogue-locales'].revision }, contentHash: canonicalContent.hash(), catalogCounts: { modes: gameModes.length, species: species.length, speciesWithBaseExperience: species.filter(item => Number.isInteger(item.baseExp)).length, experienceGrowthRates: Object.keys(experienceGrowthRates).length, fixedBossWaves: classicFixedBossWaves.length, fixedBattleWaves: classicFixedBattleWaves.length, trainers: trainers.length, trainerConfigs: trainers.filter(item => item.extensions.configStatus === 'NORMALIZED_SUBSET_WITH_RAW_PRESERVED').length, trainerPartyTemplates: trainerPartyTemplates.length, trainersWithStaticPartyTemplates: trainers.filter(item => item.trainerRules.partyTemplateStatus === 'STATIC_TEMPLATES').length, trainersWithDynamicPartyTemplateFunctions: trainers.filter(item => item.trainerRules.partyTemplateStatus === 'DYNAMIC_FUNCTION_PRESERVED').length, trainersWithSpeciesPools: trainers.filter(item => item.speciesPools.length > 0).length, rivalPartySlots: trainers.reduce((count, trainer) => count + trainer.trainerRules.rivalPartySlots.length, 0), rivalPartySpeciesChoices: trainers.reduce((count, trainer) => count + trainer.trainerRules.rivalPartySlots.reduce((sum, slot) => sum + slot.species.flat(Infinity).length, 0), 0), trainerSpeciesPoolCandidates: trainers.reduce((total, trainer) => total + trainer.speciesPools.reduce((sum, pool) => sum + pool.candidates.length, 0), 0), forms: forms.length, freshProfileStarters: defaultStarterSymbols.length, eggMoveEntries: species.reduce((count, item) => count + (item.eggMoves?.length ?? 0), 0), moves: moves.length, trainerMoveSupercedencePairs: supercededMovePairs.length, abilities: abilities.length, items: items.length, modifierPoolEntries: modifierPools.length, dynamicModifierWeights: modifierPools.filter(entry => entry.weight === null).length, locales: localeEntries.length }, skippedRecords: skippedMoves, warnings: [...species.filter(item => !Number.isInteger(item.baseExp)).map(item => ({ domain: 'species', id: item.id, classification: 'MISSING_IN_UPSTREAM', field: 'baseExp' })), ...trainers.filter(item => item.extensions.configStatus === 'MISSING_IN_UPSTREAM').map(item => ({ domain: 'trainer', id: item.id, classification: 'MISSING_IN_UPSTREAM', field: 'trainerConfig' })), ...trainers.filter(item => item.extensions.localizationStatus !== 'RESOLVED').map(item => ({ domain: 'trainer', id: item.id, classification: 'MISSING_IN_UPSTREAM', field: 'trainerClassLocale' }))], unknownFields: { observedSourceFieldNames: [...unknownFieldNames].sort(), preservedRawRecordCount: species.length + forms.length + moves.length + abilities.length + items.length + trainers.filter(item => item.extensions.upstreamConfig).length + trainerPartyTemplates.length, trainerConfigSemantics: 'supported declarative subset normalized; all raw records preserved; rival pool callbacks normalized; other callbacks remain raw', trainerPartyTemplateSemantics: 'unknown constructor behavior retained as raw source; supported declarative fields normalized', abilityAttributeSemantics: 'Berry callback ancestry and constant multipliers/fractions normalized; other trigger semantics preserved raw', modifierEffectSemantics: 'preserved raw; not interpreted' }, provenance: sourceRows, assetReferences: { verifiedManifests: assetRefs.length, verifiedImages: 0, pendingMetadataVerification: species.length - assetRefs.length, pendingImageVerification: species.length } };
    report.catalogCounts.forbiddenSinglesMoves = forbiddenSinglesMoveIds.length;
    report.catalogCounts.levelBasedDeniedMoves = levelBasedDenylistMoveIds.length;
    report.catalogCounts.forbiddenTmMoves = forbiddenTmMoveIds.length;
    report.catalogCounts.forcedSignatureSpecies = forcedSignatureMoves.length;
    report.catalogCounts.forcedRivalSignatureSpecies = forcedRivalSignatureMoves.length;
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
      const pokemonPools = this._parseBiomeSymbolPools(source, 'pokemonPool', 'TimeOfDay');
      const trainerPools = this._parseBiomeSymbolPools(source, 'trainerPool', null);
      const trainerChanceMatch = record.match(/\btrainerChance\s*:\s*(\d+)\b/);
      if (!trainerChanceMatch) throw new Error(`Upstream trainerChance was not found in ${sourcePath}`);
      const trainerChance = Number(trainerChanceMatch[1]);
      if (!Number.isSafeInteger(trainerChance) || trainerChance > 0xFFFF) throw new Error(`Upstream trainerChance is outside the 3DS canonical range in ${sourcePath}`);
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
        visualTemplate: null, background: null, music: bgm, trainerChance,
        encounterPools: pokemonPools,
        trainerPools,
        routes: routeIds,
        metadata: { upstreamBiomeId: upstreamIds.get(upstreamSymbol), sourceBiomeSymbol: symbol },
        provenance: new Provenance({ ...provenance, sourceSymbol: symbol }),
        extensions: {
          upstreamTrainerChance: { value: trainerChance, sourcePath, sourceSymbol: `${symbol}.trainerChance`, sourceHash },
          upstreamRawRecord: { format: 'typescript-source-file', value: source },
          upstreamEnumRef: { repository: repo.url, revision: repo.revision, sourcePath: 'src/enums/biome-id.ts', sha256: PokerogueManifest.computeHash(enumContent) },
          upstreamRegistryRef: { repository: repo.url, revision: repo.revision, sourcePath: 'src/init/init-biomes.ts', sha256: PokerogueManifest.computeHash(initializerContent) },
          ...(localeContent === null ? {} : { upstreamLocaleRef: { repository: POKEROGUE_REPOSITORIES['pokerogue-locales'].url, revision: POKEROGUE_REPOSITORIES['pokerogue-locales'].revision, sourcePath: 'en/biomes.json', sha256: PokerogueManifest.computeHash(localeContent) } })
        }
      }));
    }
    return { biomes: definitions, routes };
  }

  /** Preserve upstream biome pool membership by its declared tier/time keys; this does not execute selection rules. */
  _parseBiomeSymbolPools(source, declaration, nestedEnum) {
    const match = source.match(new RegExp(`const\\s+${declaration}\\s*:[^=]+?=\\s*\\{`));
    if (!match) throw new Error(`Upstream biome ${declaration} declaration was not found`);
    const open = source.indexOf('{', match.index);
    const close = this._findBalancedSourceDelimiter(source, open, '{', '}');
    const body = source.slice(open + 1, close);
    const tiers = new Map();
    const tierPattern = nestedEnum
      ? /\[BiomePoolTier\.([A-Z][A-Z0-9_]*)\]\s*:\s*\{/g
      : /\[BiomePoolTier\.([A-Z][A-Z0-9_]*)\]\s*:\s*\[/g;
    let tierMatch;
    while ((tierMatch = tierPattern.exec(body)) !== null) {
      const openChar = nestedEnum ? '{' : '[';
      const closeChar = nestedEnum ? '}' : ']';
      const tierOpen = nestedEnum
        ? body.indexOf(openChar, tierMatch.index)
        : body.indexOf(openChar, body.indexOf(':', tierMatch.index) + 1);
      const tierClose = this._findBalancedSourceDelimiter(body, tierOpen, openChar, closeChar);
      const tierBody = body.slice(tierOpen + 1, tierClose);
      if (nestedEnum) {
        const times = {};
        const timePattern = /\[TimeOfDay\.([A-Z][A-Z0-9_]*)\]\s*:\s*\[/g;
        let timeMatch;
        while ((timeMatch = timePattern.exec(tierBody)) !== null) {
          const arrayOpen = tierBody.indexOf('[', tierBody.indexOf(':', timeMatch.index) + 1);
          const arrayClose = this._findBalancedSourceDelimiter(tierBody, arrayOpen, '[', ']');
          const values = [...tierBody.slice(arrayOpen + 1, arrayClose).matchAll(/SpeciesId\.([A-Z][A-Z0-9_]*)/g)].map(item => item[1].toLowerCase());
          times[timeMatch[1].toLowerCase()] = values;
          timePattern.lastIndex = arrayClose + 1;
        }
        tiers.set(tierMatch[1].toLowerCase(), times);
      } else {
        const values = [...tierBody.matchAll(/TrainerType\.([A-Z][A-Z0-9_]*)/g)].map(item => item[1].toLowerCase());
        tiers.set(tierMatch[1].toLowerCase(), values);
      }
      tierPattern.lastIndex = tierClose + 1;
    }
    if (!tiers.size) throw new Error(`Upstream biome ${declaration} has no recognized tiers`);
    return Object.fromEntries([...tiers.entries()].sort(([left], [right]) => left.localeCompare(right)));
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
    const startPath = 'src/game-mode.ts';
    const battlePath = 'src/battle.ts';
    const arenaPath = 'src/field/arena.ts';
    const poolTierEnumPath = 'src/enums/biome-pool-tier.ts';
    const [startSource, battleSource, arenaSource, poolTierEnumSource] = await Promise.all([
      repository.getFile('pokerogue', startPath), repository.getFile('pokerogue', battlePath),
      repository.getFile('pokerogue', arenaPath), repository.getFile('pokerogue', poolTierEnumPath)
    ]);
    const startMatch = startSource.match(/getStartingBiome\(\)[\s\S]*?switch\s*\(this\.modeId\)[\s\S]*?default:\s*return\s+BiomeId\.([A-Z0-9_]+)/);
    if (!startMatch) throw new Error(`Pinned upstream GameMode.getStartingBiome default was not recognized in ${startPath}`);
    const startingBiomeId = startMatch[1].toLowerCase();
    const pokemonPath = 'src/field/pokemon.ts';
    const pokemonSource = await repository.getFile('pokerogue', pokemonPath);
    const fixedCase = /case this\.species\.speciesId === SpeciesId\.ETERNATUS:([\s\S]*?)\n\s*break;/.exec(pokemonSource);
    if (!fixedCase) throw new Error('Pinned Eternatus moveset case was not recognized');
    if (!/this\.moveset\s*=\s*\(formIndex === undefined \? this\.formIndex : formIndex\)\s*\?/.test(fixedCase[1]))
      throw new Error('Pinned Eternatus form-index phase selection changed');
    const arrays = [...fixedCase[1].matchAll(/\[([\s\S]*?)\]/g)].filter(match => /new PokemonMove/.test(match[1]));
    if (arrays.length !== 2) throw new Error('Pinned Eternatus phase moveset arrays changed');
    const fixedProfiles = arrays.map((match, index) => {
      const moves = [...match[1].matchAll(/new PokemonMove\(MoveId\.([A-Z_]+)(?:,\s*(\d+),\s*(-?\d+))?\)/g)].map(token => {
        const move = content.moves.find(entry => entry.source?.sourceSymbol === `MoveId.${token[1]}`);
        if (!move) throw new Error(`Missing canonical fixed boss move ${token[1]}`);
        return { moveId: move.moveId, ppUsed: Number(token[2] ?? 0), ppUp: Number(token[3] ?? 0) };
      });
      if (moves.length !== 4) throw new Error('Invalid pinned Eternatus fixed moveset');
      return { speciesId: 'eternatus', formIndex: index === 0 ? 1 : 0, moves };
    });
    const fixedEnemyMovesets = { entries: fixedProfiles, raw: fixedCase[0],
      inverseBattleOverride: { status: 'NOT_YET_SUPPORTED', raw: fixedCase[1].slice(fixedCase[1].indexOf('if (globalScene')) },
      provenance: { repository: POKEROGUE_REPOSITORIES.pokerogue.url, revision: POKEROGUE_REPOSITORIES.pokerogue.revision,
        sourcePath: pokemonPath, sourceSymbol: 'EnemyPokemon.generateAndPopulateMoveset:ETERNATUS', sourceHash: PokerogueManifest.computeHash(pokemonSource) } };

    const sourceSnapshot = content.canonicalContent.sourceSnapshot;
    const speciesIds = new Set(content.species.map(species => species.id));
    const missingPoolSpecies = [];
    for (const biome of progression.biomes) {
      for (const [tier, times] of Object.entries(biome.encounterPools || {})) {
        for (const [time, ids] of Object.entries(times || {})) {
          for (const speciesId of ids) if (!speciesIds.has(speciesId)) missingPoolSpecies.push(`${biome.id}.${tier}.${time}:${speciesId}`);
        }
      }
    }
    const importedGenerations = new Set(options.generations ?? [1, 2, 3, 4, 5, 6, 7, 8, 9]);
    const completeSpeciesSnapshot = [1, 2, 3, 4, 5, 6, 7, 8, 9].every(generation => importedGenerations.has(generation));
    if (missingPoolSpecies.length && completeSpeciesSnapshot) throw new Error(`Biome encounter pools reference species absent from complete canonical content: ${missingPoolSpecies.slice(0, 12).join(', ')}${missingPoolSpecies.length > 12 ? ` (+${missingPoolSpecies.length - 12} more)` : ''}`);
    const sources = new Map(sourceSnapshot.sources.map(source => [`${source.repository}:${source.revision}:${source.sourcePath}`, source]));
    for (const source of progression.sourceSnapshot.sources) sources.set(`${source.repository}:${source.revision}:${source.sourcePath}`, source);
    sources.set(`pokerogue:${sourceSnapshot.revision}:${startPath}`, { repository: 'pokerogue', revision: sourceSnapshot.revision, sourcePath: startPath, hash: PokerogueManifest.computeHash(startSource) });
    sources.set(`pokerogue:${sourceSnapshot.revision}:${battlePath}`, { repository: 'pokerogue', revision: sourceSnapshot.revision, sourcePath: battlePath, hash: PokerogueManifest.computeHash(battleSource) });
    sources.set(`pokerogue:${sourceSnapshot.revision}:${arenaPath}`, { repository: 'pokerogue', revision: sourceSnapshot.revision, sourcePath: arenaPath, hash: PokerogueManifest.computeHash(arenaSource) });
    sources.set(`pokerogue:${sourceSnapshot.revision}:${poolTierEnumPath}`, { repository: 'pokerogue', revision: sourceSnapshot.revision, sourcePath: poolTierEnumPath, hash: PokerogueManifest.computeHash(poolTierEnumSource) });
    sources.set(`pokerogue:${sourceSnapshot.revision}:${pokemonPath}`, { repository: 'pokerogue', revision: sourceSnapshot.revision, sourcePath: pokemonPath, hash: PokerogueManifest.computeHash(pokemonSource) });
    sourceSnapshot.sources = [...sources.values()].sort((a, b) => a.repository.localeCompare(b.repository) || a.sourcePath.localeCompare(b.sourcePath));
    const localeEntries = [...content.locales, ...progression.localeEntries];
    const canonicalContent = new CanonicalContent({
      schemaVersion: content.canonicalContent.schemaVersion,
      contentVersion: content.canonicalContent.contentVersion,
      sourceSnapshot,
      provenance: content.canonicalContent.provenance,
      collections: { ...content.canonicalContent.collections, biomes: progression.biomes, routes: progression.routes, locales: localeEntries },
      extensions: { ...content.canonicalContent.extensions, fixedEnemyMovesets, importedProgression: { source: 'src/init/init-biomes.ts', waveCatalog: 'NOT_DECLARED_UPSTREAM' }, biomePoolReferenceAudit: missingPoolSpecies.length ? { status: 'PARTIAL_SPECIES_SNAPSHOT_UNVERIFIED', unverifiedReferenceCount: missingPoolSpecies.length, sample: missingPoolSpecies.slice(0, 12) } : { status: 'COMPLETE_AND_VALIDATED', referenceCount: progression.biomes.reduce((count, biome) => count + Object.values(biome.encounterPools || {}).reduce((tierCount, times) => tierCount + Object.values(times || {}).reduce((timeCount, ids) => timeCount + ids.length, 0), 0), 0) }, upstreamStartingBiome: { id: startingBiomeId, sourcePath: startPath, sourceSymbol: `GameMode.getStartingBiome:default:${startMatch[1]}`, sourceHash: PokerogueManifest.computeHash(startSource) }, upstreamEncounterLevel: { sourcePath: battlePath, sourceSymbol: 'Battle.getLevelForWave/randSeedGaussForLevel', sourceHash: PokerogueManifest.computeHash(battleSource) }, upstreamEncounterSelection: { repository: POKEROGUE_REPOSITORIES.pokerogue.url, revision: POKEROGUE_REPOSITORIES.pokerogue.revision, sourcePath: arenaPath, sourceSymbol: 'Arena.randomSpecies/generateBossBiomeTier/generateNonBossBiomeTier/updatePoolsForTimeOfDay/getTimeOfDay', sourceHash: PokerogueManifest.computeHash(arenaSource), poolTierEnum: { sourcePath: poolTierEnumPath, sourceSymbol: 'BiomePoolTier', sourceHash: PokerogueManifest.computeHash(poolTierEnumSource) }, coverage: 'POOL_TIER_AND_MEMBER_SELECTION_RULES_ONLY; downstream legendary reroll/species substitution/RNG seeding are not ported' } }
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
      const malePercentDeclared = /\bmalePercent\s*:/i.test(block);
      const malePercentMatch = block.match(/\bmalePercent\s*:\s*(null|\d+(?:\.\d+)?)/i);
      if (malePercentDeclared && !malePercentMatch) throw new Error(`Invalid import: unsupported malePercent value for ${speciesKey}`);
      const malePercent = malePercentMatch ? (malePercentMatch[1].toLowerCase() === 'null' ? null : Number(malePercentMatch[1])) : undefined;
      if (typeof malePercent === 'number' && (malePercent < 0 || malePercent > 100)) throw new Error(`Invalid import: malePercent out of range for ${speciesKey}`);
      const type1Match = block.match(/type1\s*:\s*(?:PokemonType\.|Type\.)?([A-Za-z0-9_]+)/i);
      const type2Match = block.match(/type2\s*:\s*(?:PokemonType\.|Type\.)?([A-Za-z0-9_]+)/i);

      let baseStats = { hp: 40, atk: 40, def: 40, spatk: 40, spdef: 40, spd: 40 };
      const bHp = block.match(/baseHp\s*:\s*(\d+)/i);
      const bAtk = block.match(/baseAtk\s*:\s*(\d+)/i);
      const bDef = block.match(/baseDef\s*:\s*(\d+)/i);
      const bSpatk = block.match(/baseSpatk\s*:\s*(\d+)/i);
      const bSpdef = block.match(/baseSpdef\s*:\s*(\d+)/i);
      const bSpd = block.match(/baseSpd\s*:\s*(\d+)/i);
      const baseTotalMatch = block.match(/baseTotal\s*:\s*(\d+)/i);
      const baseExpMatch = block.match(/\bbaseExp\s*:\s*(\d+)/i);
      if (/\bbaseExp\s*:/.test(block) && !baseExpMatch) throw new Error(`Invalid import: unsupported baseExp value for ${speciesKey}`);
      const rarityField = field => {
        const match = block.match(new RegExp(`\\b${field}\\s*:\\s*(true|false)`, 'i'));
        if (match) return match[1].toLowerCase() === 'true';
        // Pinned PokemonSpecies constructor uses data.<rarity> ?? false.
        // Explicit expressions remain unknown instead of silently becoming false.
        return new RegExp(`\\b${field}\\s*:`).test(block) ? null : false;
      };
      const growthRateMatch = block.match(/growthRate\s*:\s*GrowthRate\.([A-Z_]+)/);
      const evolutions = [];
      const evolutionArrayMatch = /\bevolutions\s*:\s*\[/.exec(block);
      const unsupportedEvolutionDeclarations = [];
      if (evolutionArrayMatch) {
        const arrayOpen = block.indexOf('[', evolutionArrayMatch.index);
        const evolutionArray = extractBalancedLiteral(block, arrayOpen);
        if (!evolutionArray) throw new Error(`Invalid import: unclosed evolutions array for ${speciesKey}`);
        const evolutionCtor = /new\s+(SpeciesEvolution|SpeciesFormEvolution)\s*\(\s*\{/g;
        let evolutionMatch;
        while ((evolutionMatch = evolutionCtor.exec(evolutionArray))) {
          const objectOpen = evolutionArray.indexOf('{', evolutionMatch.index);
          const rawEvolution = extractBalancedLiteral(evolutionArray, objectOpen);
          if (!rawEvolution) throw new Error(`Invalid import: unclosed SpeciesEvolution record for ${speciesKey}`);
          const targetMatch = rawEvolution.match(/\bspeciesId\s*:\s*(?:SpeciesId\.)?([A-Za-z0-9_]+)/);
          const levelMatch = rawEvolution.match(/\blevel\s*:\s*(\d+)/);
          const itemMatch = rawEvolution.match(/\bitem\s*:\s*(?:EvolutionItem\.)?([A-Za-z0-9_]+)/);
          const delayMatch = rawEvolution.match(/\bevoDelay\s*:\s*\[\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\]/);
          const conditionMatch = rawEvolution.match(/\bcondition\s*:\s*([\s\S]*?)(?=,\s*(?:evoDelay|item)\s*:|\s*}$)/);
          const formKey = key => {
            const match = rawEvolution.match(new RegExp(`\\b${key}\\s*:\\s*(?:SpeciesFormKey\\.([A-Z0-9_]+)|["']([^"']*)["'])`));
            if (!match && new RegExp(`\\b${key}\\s*:`).test(rawEvolution)) throw new Error(`Invalid import: unsupported ${key} in ${speciesKey} evolution`);
            return match ? (match[1] ?? match[2].toUpperCase()) : null;
          };
          const targetSymbol = targetMatch?.[1]?.toUpperCase() ?? null;
          const targetSpeciesEnumId = targetSymbol ? this.speciesEnumCatalog?.getId(targetSymbol) : undefined;
          const edgeIndex = evolutions.length;
          evolutions.push(new SpeciesEvolutionDefinition({
            targetSpeciesId: targetSymbol?.toLowerCase() ?? null,
            targetSpeciesEnumId,
            level: levelMatch ? Number(levelMatch[1]) : null,
            evoLevelThreshold: delayMatch ? { strong: Number(delayMatch[1]), normal: Number(delayMatch[2]), wild: Number(delayMatch[3]) } : null,
            item: itemMatch?.[1] ?? null,
            condition: conditionMatch?.[1]?.trim() ?? null,
            declarationKind: evolutionMatch[1],
            preFormKey: formKey('preFormKey'),
            evoFormKey: formKey('evoFormKey'),
            source: {
              source: 'pokerogue', sourceType: this.sourceType,
              sourceRepository: this.sourceType === CanonicalSourceType.UPSTREAM ? repoInfo.url : 'local:test/fixtures/fallbackVerticalSlice.js',
              sourceRevision: this.sourceType === CanonicalSourceType.UPSTREAM ? repoInfo.revision : 'offline-fixture-v1',
              sourcePath: this.sourceType === CanonicalSourceType.UPSTREAM ? sourcePath : 'test/fixtures/fallbackVerticalSlice.js',
              sourceHash: fileHash,
              sourceSymbol: `generationSpeciesData[SpeciesId.${speciesKey}].evolutions[${edgeIndex}]`
            },
            extensions: { upstreamRawRecord: { format: 'typescript-source-fragment', value: rawEvolution } }
          }));
          evolutionCtor.lastIndex = objectOpen + rawEvolution.length;
        }
        const declaredEvolutionCount = [...evolutionArray.matchAll(/new\s+Species(?:Form)?Evolution\s*\(/g)].length;
        if (declaredEvolutionCount !== evolutions.length) {
          unsupportedEvolutionDeclarations.push({ declaredCount: declaredEvolutionCount, parsedCount: evolutions.length, raw: evolutionArray });
        }
      }

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
        const mRegex = /\[\s*(-?\d+|EVOLVE_MOVE|RELEARN_MOVE)\s*,\s*(?:MoveId\.|Moves\.)?([A-Za-z0-9_]+)\s*\]|\{\s*level\s*:\s*(-?\d+|EVOLVE_MOVE|RELEARN_MOVE)\s*,\s*move\s*:\s*['"`]?([A-Za-z0-9_]+)['"`]?\s*\}/g;
        let mEntry;
        while ((mEntry = mRegex.exec(levelMovesContent)) !== null) {
          const rawLevel = mEntry[1] || mEntry[3];
          const lvl = rawLevel === 'EVOLVE_MOVE' ? 0 : rawLevel === 'RELEARN_MOVE' ? -1 : Number(rawLevel);
          const mvName = String(mEntry[2] || mEntry[4]).toLowerCase();
          moveset.push({ level: lvl, move: mvName, id: mvName });
        }
      }
      const formLevelMoves = parseFormLevelMoves(block, speciesKey);

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
        baseTotal: baseTotalMatch ? Number(baseTotalMatch[1]) : null,
        baseExp: baseExpMatch ? Number(baseExpMatch[1]) : null,
        rarity: { legendary: rarityField('legendary'), subLegendary: rarityField('subLegendary'), mythical: rarityField('mythical') },
        growthRate: growthRateMatch?.[1] ?? null,
        ...(malePercentMatch ? { malePercent } : {}),
        evolutions,
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
        extensions: {
          upstreamRawRecord: { format: 'typescript-source-fragment', value: block },
          ...(formLevelMoves ? { upstreamFormLevelMoves: formLevelMoves } : {}),
          category: block.match(/category\s*:\s*["']([^"']+)/)?.[1] || null,
          ...(evolutionArrayMatch ? { upstreamEvolutionDeclarations: { status: unsupportedEvolutionDeclarations.length ? 'PARTIAL_PARSE' : 'PARSED', unsupported: unsupportedEvolutionDeclarations } } : { upstreamEvolutionDeclarations: { status: 'NOT_DECLARED' } })
        }
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
  parseMoves(tsContent, targetIds = null, generationSource = null) {
    const moveList = [];
    const sourcePath = 'src/data/moves/move.ts';
    const repoInfo = POKEROGUE_REPOSITORIES.pokerogue;
    const fileHash = PokerogueManifest.computeHash(tsContent);

    this.manifest.recordFile('pokerogue', repoInfo.revision, sourcePath, tsContent);

    const targetSet = Array.isArray(targetIds) && targetIds.length > 0
      ? new Set(targetIds.map(t => t.toUpperCase()))
      : null;
    let stabBlacklist = null;
    let stabBlacklistSource = null;
    if (generationSource !== null) {
      const declaration = generationSource.match(/export\s+const\s+STAB_BLACKLIST\s*:[^=]+?=\s*new\s+Set\s*\(\s*\[([\s\S]*?)\]\s*\)/);
      if (!declaration) throw new Error('Invalid import: pinned STAB_BLACKLIST declaration was not found');
      stabBlacklist = new Set([...declaration[1].matchAll(/MoveId\.([A-Z0-9_]+)/g)].map(entry => entry[1]));
      const upstream = POKEROGUE_REPOSITORIES.pokerogue;
      stabBlacklistSource = { sourceRepository: upstream.url, sourceRevision: upstream.revision,
        sourcePath: 'src/data/balance/moves/moveset-generation.ts', sourceSymbol: 'STAB_BLACKLIST',
        sourceHash: PokerogueManifest.computeHash(generationSource), sourceType: this.sourceType };
    } else if (this.productionCanonicalImport) {
      throw new Error('Invalid import: production move catalog requires pinned STAB_BLACKLIST source');
    }

    // Constructor signatures differ upstream: AttackMove, StatusMove and
    // SelfStatusMove do not share positional fields. Parse the call before mapping.
    const ctorRegex = /new\s+(AttackMove|StatusMove|SelfStatusMove|ChargingAttackMove|ChargingSelfStatusMove|Move)\s*\(/g;
    let match;

    while ((match = ctorRegex.exec(tsContent)) !== null) {
      const call = readConstructorCall(tsContent, ctorRegex.lastIndex - 1);
      if (!call) throw new Error(`Invalid import: unclosed move constructor at offset ${match.index}`);
      ctorRegex.lastIndex = call.closeIndex + 1;
      const args = splitTopLevelArguments(call.argsText);
      const constructor = match[1];
      const moveKey = enumArgument(args[0], ['MoveId', 'Moves']).toUpperCase();
      if (!/^[A-Z0-9_]+$/.test(moveKey)) throw new Error(`Invalid import: unsupported move ID expression in ${constructor}: ${args[0] || '<missing>'}`);
      // Upstream's NONE entry is a non-playable sentinel and uses a constructor
      // shape outside the move-definition contract; it is not canonical content.
      if (moveKey === 'NONE') continue;
      if (targetSet && !targetSet.has(moveKey)) continue;
      if (moveList.some(m => m.id === moveKey.toLowerCase())) continue;
      const numberAt = index => {
        const token = args[index];
        if (!/^-?\d+$/.test(token || '')) throw new Error(`Invalid import: ${constructor} ${moveKey} argument ${index + 1} must be an integer, received ${token || '<missing>'}`);
        return Number(token);
      };
      const type = enumArgument(args[1], ['PokemonType', 'Type']).toUpperCase();
      let category, power, accuracy, pp, chance, priority, generation, target;
      if (constructor.includes('AttackMove')) {
        category = enumArgument(args[2], ['MoveCategory']).toUpperCase();
        power = numberAt(3); accuracy = numberAt(4); pp = numberAt(5);
        chance = numberAt(6); priority = numberAt(7); generation = numberAt(8);
        target = 'NEAR_OTHER';
      } else if (constructor.includes('SelfStatusMove')) {
        category = 'STATUS'; power = -1;
        accuracy = numberAt(2); pp = numberAt(3); chance = numberAt(4);
        priority = numberAt(5); generation = numberAt(6); target = 'USER';
      } else if (constructor === 'StatusMove') {
        category = 'STATUS'; power = -1;
        accuracy = numberAt(2); pp = numberAt(3); chance = numberAt(4);
        priority = numberAt(5); generation = numberAt(6); target = 'NEAR_OTHER';
      } else {
        category = enumArgument(args[2], ['MoveCategory']).toUpperCase();
        target = enumArgument(args[3], ['MoveTarget']).toUpperCase();
        power = numberAt(4); accuracy = numberAt(5); pp = numberAt(6);
        chance = numberAt(7); priority = numberAt(8); generation = numberAt(9);
      }
      if (!['PHYSICAL', 'SPECIAL', 'STATUS'].includes(category)) throw new Error(`Invalid import: ${moveKey} has unsupported category ${category}`);
      const chained = readChainedExpression(tsContent, call.closeIndex);
      const explicitTarget = chained.match(/\.target\s*\(\s*MoveTarget\.([A-Z0-9_]+)\s*\)/);
      if (explicitTarget) target = explicitTarget[1];
      const rawExpression = tsContent.slice(match.index, call.closeIndex + 1) + chained;
      const isUnimplemented = /\.unimplemented\s*\(/.test(chained);
      const upstreamAttributes = [...chained.matchAll(/\.attr\s*\(\s*(?:new\s+)?([A-Za-z_$][\w$]*)/g)]
        .map(attribute => attribute[1]);

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
        type,
        category: toTitle(category),
        power,
        accuracy,
        pp,
        priority,
        target,
        isUnimplemented,
        upstreamAttributes,
        flags: { contact: !this.productionCanonicalImport && category === 'PHYSICAL', protectable: !this.productionCanonicalImport },
        secondaryEffects,
        source: provenance,
        metadata: { ...provenance, hash: fileHash },
        extensions: { upstreamMoveClass: constructor, upstreamChance: chance, upstreamGeneration: generation,
          upstreamMoveGeneration: stabBlacklist ? { stabBlacklisted: stabBlacklist.has(moveKey), source: stabBlacklistSource } : undefined,
          upstreamEffectMetadata: { format: 'typescript-source', value: rawExpression } },
        raw: { format: 'typescript-source', value: rawExpression }
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

    // Pattern 1: Upstream AbBuilder syntax. Scan balanced constructor and
    // chained expression so semicolons inside callback bodies do not truncate
    // preserved source metadata.
    const builderRegex = /new\s+AbBuilder\s*\(/g;
    let match;

    while ((match = builderRegex.exec(tsContent)) !== null) {
      const openingIndex = tsContent.indexOf('(', match.index);
      const constructor = readConstructorCall(tsContent, openingIndex);
      if (!constructor) throw new Error(`Invalid import: unclosed AbBuilder constructor at offset ${match.index}`);
      const argumentsList = splitTopLevelArguments(constructor.argsText);
      const idMatch = argumentsList[0]?.match(/^(?:AbilityId\.|Abilities\.)?([A-Za-z0-9_]+)$/);
      if (!idMatch) throw new Error(`Invalid import: unsupported AbBuilder ability ID at offset ${match.index}`);
      const chain = readChainedExpression(tsContent, constructor.closeIndex);
      const rawDeclaration = tsContent.slice(match.index, constructor.closeIndex + 1) + chain;
      if (!/\.build\s*\(\s*\)\s*$/.test(rawDeclaration.trim())) {
        throw new Error(`Invalid import: AbBuilder declaration missing terminal build() at offset ${match.index}`);
      }
      const abKey = idMatch[1].toUpperCase();
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
        raw: { format: 'typescript-source', value: rawDeclaration },
        extensions: { upstreamAttributes: { format: 'typescript-source-fragment', value: rawDeclaration }, runtimeBehavior: 'NOT_IMPORTED' }
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

export function parseBerryGeneration(raw, enumEntries) {
  const draw = raw.match(/const\s+rand\s*=\s*randSeedInt\(\s*(\d+)\s*\)/);
  const branches = [...raw.matchAll(/(?:if|else\s+if)\s*\(rand\s*<\s*(\d+)\)\s*\{\s*randBerryType\s*=\s*BerryType\.([A-Z_]+)\s*;/g)];
  const fallback = raw.match(/berryTypes\[randSeedInt\(berryTypes\.length\s*-\s*(\d+)\)\s*\+\s*(\d+)\]/);
  if (!draw || branches.length !== 3 || !fallback) throw new Error('Unsupported pinned Berry generator');
  const entries = enumEntries.map(([symbol, id]) => ({ symbol, id }));
  if (!entries.length || entries.some(row => !Number.isInteger(row.id) || row.id < 0 || row.id > 32767)) throw new Error('Invalid Berry enum');
  const thresholds = branches.map(match => {
    const entry = entries.find(row => row.symbol === match[2]);
    if (!entry) throw new Error(`Broken Berry generator reference: ${match[2]}`);
    return { upperExclusive: Number(match[1]), berryId: entry.id };
  });
  const rollRange = Number(draw[1]), fallbackExcludedCount = Number(fallback[1]), fallbackOffset = Number(fallback[2]);
  if (rollRange < 1 || rollRange > 32767 || thresholds.some((row, i) => row.upperExclusive < 1 || row.upperExclusive >= rollRange ||
      (i && row.upperExclusive <= thresholds[i - 1].upperExclusive)) ||
      entries.length <= fallbackExcludedCount || fallbackOffset + entries.length - fallbackExcludedCount > entries.length)
    throw new Error('Invalid Berry generator ranges');
  return { entries, rollRange, thresholds, fallbackExcludedCount, fallbackOffset, raw };
}

export function parseBerryHeldLimits(raw, enumEntries) {
  const match = raw.match(/if\s*\(\s*\[([^\]]+)\]\.includes\(this\.berryType\)\s*\)\s*\{\s*return\s+(\d+)\s*;\s*\}\s*return\s+(\d+)\s*;/);
  if (!match || !/modifier\s+instanceof\s+BerryModifier[\s\S]*?berryType\s*===\s*this\.berryType/.test(raw))
    throw new Error('Unsupported pinned Berry held match/limit policy');
  const symbols = match[1].split(',').map(value => value.trim().match(/^BerryType\.([A-Z_]+)$/)?.[1]);
  const reducedMaxStacks = Number(match[2]), defaultMaxStacks = Number(match[3]);
  if (symbols.some(symbol => !symbol || !enumEntries.some(([known]) => known === symbol)) ||
      !reducedMaxStacks || reducedMaxStacks > 65535 || !defaultMaxStacks || defaultMaxStacks > 65535)
    throw new Error('Invalid Berry held policy');
  return { reducedIds: symbols.map(symbol => enumEntries.find(([known]) => known === symbol)[1]), reducedMaxStacks, defaultMaxStacks };
}


// Normalize the pinned declarative Berry predicates/effects. Unknown cases keep
// their bodies but never acquire an executable profile from their name alone.
export function parseBerryEffects(raw, berryEnumEntries, statEnumEntries) {
  const functionBody = name => {
    const match = raw.match(new RegExp(`export\\s+function\\s+${name}\\b[^\\{]*\\{`));
    if (!match) throw new Error(`Missing Berry function ${name}`);
    return extractBalancedLiteral(raw, match.index + match[0].length - 1);
  };
  const predicateRaw = functionBody('getBerryPredicate');
  const effectRaw = functionBody('getBerryEffectFunc');
  const cases = body => {
    const labels = [...body.matchAll(/case\s+BerryType\.([A-Z_]+)\s*:/g)];
    const result = new Map();
    let pending = [];
    labels.forEach((label, i) => {
      pending.push(label[1]);
      let value = body.slice(label.index + label[0].length, labels[i + 1]?.index ?? body.length);
      if (!value.trim()) return;
      value = value.split(/\bdefault\s*:/)[0];
      for (const symbol of pending) result.set(symbol, value);
      pending = [];
    });
    return result;
  };
  const predicates = cases(predicateRaw), effects = cases(effectRaw);
  const stats = new Map(statEnumEntries), ids = new Map(berryEnumEntries);
  if (![...['ATK','DEF','SPATK','SPDEF','SPD']].every(symbol => Number.isInteger(stats.get(symbol))))
    throw new Error('Missing Berry battle stat references');
  const entries = berryEnumEntries.map(([symbol, id]) => {
    const predicate = predicates.get(symbol), effect = effects.get(symbol);
    if (!predicate || !effect) throw new Error(`Missing required Berry predicate/effect: ${symbol}`);
    let predicateKind = 'UNSUPPORTED', effectKind = 'UNSUPPORTED', hpThreshold = 0, amount = 0, stat = 0;
    const ratio = predicate.match(/getHpRatio\(\)\s*<\s*(0\.\d+)/);
    const reducedThreshold = predicate.match(/new NumberHolder\((0\.\d+)\)/);
    if (ratio) { predicateKind = /CRIT_BOOST/.test(predicate) ? 'LOW_HP_NO_CRIT' : 'LOW_HP'; hpThreshold = Number(ratio[1]); }
    else if (/getHpRatio\(\)\s*<\s*hpRatioReq.value\s*&&\s*pokemon.getStatStage\(stat\)\s*<\s*6/.test(predicate)) {
      predicateKind = 'LOW_HP_STAT'; hpThreshold = Number(reducedThreshold?.[1]);
      stat = id - ids.get('ENIGMA');
      if (![...stats.values()].includes(stat) || stat < stats.get('ATK') || stat > stats.get('SPD')) throw new Error('Invalid Berry stat offset');
    } else if (/!!pokemon.status\s*\|\|\s*!!pokemon.getTag\(BattlerTagType.CONFUSED\)/.test(predicate)) predicateKind = 'STATUS_OR_CONFUSION';
    else if (/attacksReceived.some/.test(predicate) && /HitResult.SUPER_EFFECTIVE/.test(predicate) && /HitResult.EXTREMELY_EFFECTIVE/.test(predicate)) predicateKind = 'SUPER_EFFECTIVE_RECEIVED';
    else if (/getMoveset\(\).find\(m => !m.getPpRatio\(\)\)/.test(predicate)) predicateKind = 'EMPTY_PP';
    const heal = effect.match(/toDmgValue\(consumer.getMaxHp\(\)\s*\/\s*(\d+)\)/);
    const stages = effect.match(/(?:statStages|stages)\s*=\s*new NumberHolder\((\d+)\)/);
    const pp = effect.match(/ppUsed\s*-\s*(\d+)/);
    if (heal) { effectKind = 'HEAL'; amount = Number(heal[1]); }
    else if (/consumer.resetStatus\(true, true\)/.test(effect)) effectKind = 'CURE_STATUS';
    else if (/consumer.addTag\(BattlerTagType.CRIT_BOOST\)/.test(effect)) effectKind = 'CRIT_BOOST';
    else if (stages && /randSeedInt\(Stat.SPD, Stat.ATK\)/.test(effect)) { effectKind = 'RANDOM_STAT'; amount = Number(stages[1]); }
    else if (stages && /berryType - BerryType.ENIGMA/.test(effect)) { effectKind = 'STAT'; amount = Number(stages[1]); }
    else if (pp && /ppUsed === m.getMovePp\(\)/.test(effect)) { effectKind = 'RESTORE_PP'; amount = Number(pp[1]); }
    const calls = [...(predicate + effect).matchAll(/\b([A-Za-z_$][\w$]*)\s*\(/g)].map(match => match[1]);
    const knownCalls = new Set(['pokemon','consumer','getHpRatio','getTag','some','getStatStage','applyAbAttrs','NumberHolder',
      'getMoveset','find','getPpRatio','toDmgValue','getMaxHp','unshiftNew','queueMessage','t','getPokemonNameWithAffix',
      'getBerryName','getStatusEffectHealText','resetStatus','updateInfo','addTag','randSeedInt','getMovePp','max','getName',
      'switch','error','if','push','return','getBattlerIndex']);
    const callbacks = [...(predicate + effect).matchAll(/applyAbAttrs\("([^"]+)"/g)].map(match => match[1]);
    const resolved = calls.every(call => knownCalls.has(call)) &&
      callbacks.every(callback => ['ReduceBerryUseThresholdAbAttr','DoubleBerryEffectAbAttr'].includes(callback)) &&
      predicateKind !== 'UNSUPPORTED' && effectKind !== 'UNSUPPORTED' &&
      Number.isFinite(hpThreshold) && hpThreshold >= 0 && hpThreshold < 1 && Number.isInteger(amount) && amount >= 0 && amount <= 65535;
    return { id, symbol, resolved, predicateKind, effectKind, hpThreshold, amount, stat,
      thresholdCallback: /ReduceBerryUseThresholdAbAttr/.test(predicate),
      doubledEffectCallback: /DoubleBerryEffectAbAttr/.test(effect),
      raw: { predicate, effect }, nameKey: `berry:${symbol.toLowerCase()}.name`, effectKey: `berry:${symbol.toLowerCase()}.effect` };
  });
  return { entries, randomStatRange: stats.get('SPD'), randomStatMinimum: stats.get('ATK'), raw };
}

export function parseBerryPreservation(raw) {
  const chance = raw.match(/doPreserve.value\s*\|\|=\s*pokemon.randBattleSeedInt\((\d+)\)\s*<\s*this.getStackCount\(\)\s*\*\s*(\d+)\s*;/);
  const cap = raw.match(/getMaxStackCount\(\)\s*:\s*number\s*\{\s*return\s+(\d+)\s*;/);
  if (!chance || !cap) throw new Error('Unsupported pinned Berry preservation policy');
  const rollRange = Number(chance[1]), chancePerStack = Number(chance[2]), maxStacks = Number(cap[1]);
  if (![rollRange,chancePerStack,maxStacks].every(value => Number.isInteger(value) && value > 0 && value <= 32767) ||
      chancePerStack * maxStacks > rollRange) throw new Error('Invalid Berry preservation ranges');
  return { rollRange, chancePerStack, maxStacks, shortCircuitWhenPreserved: true, raw };
}

export function parseBerryCriticalTag(raw) {
  const stages = raw.match(/else\s*\{\s*\(this as Writable<CritBoostTag>\).critStages\s*=\s*(\d+)\s*;/);
  const legacy = raw.match(/critStages\s*=\s*source.critStages\s*\?\?\s*(\d+)\s*;/);
  if (!stages || !legacy || !/return lapseType !== BattlerTagLapseType.CUSTOM \|\| super.lapse\(pokemon, lapseType\)/.test(raw) ||
      !/this.tagType === BattlerTagType.DRAGON_CHEER/.test(raw)) throw new Error('Unsupported pinned Berry critical tag');
  const boostStages = Number(stages[1]), legacyStages = Number(legacy[1]);
  if (boostStages < 1 || boostStages > 2 || legacyStages < 1 || legacyStages > boostStages)
    throw new Error('Invalid Berry critical stages');
  return { boostStages, legacyStages, lapseOnCustomOnly: true, raw };
}

// Class ancestry is data: it proves callback absence without a hand-maintained
// ability whitelist. Unknown builders/subclasses stay explicit and retain raw.
export function parseBerryAbilityRules(source, abilities) {
  const classes = new Map();
  const declarations = /^\s*(?:export\s+)?(?:abstract\s+)?class\s+([A-Za-z_$][\w$]*)(?:\s+extends\s+([A-Za-z_$][\w$]*))?\s*\{/gm;
  for (const match of source.matchAll(declarations)) {
    if (classes.has(match[1])) throw new Error(`Duplicate ability attribute class: ${match[1]}`);
    classes.set(match[1], { name: match[1], parent: match[2] ?? null,
      raw: extractBalancedLiteral(source, match.index + match[0].length - 1) });
  }
  if (!classes.has('AbAttr')) throw new Error('Missing upstream AbAttr');
  const required = ['DoubleBerryEffectAbAttr', 'ReduceBerryUseThresholdAbAttr', 'PreventBerryUseAbAttr',
    'HealFromBerryUseAbAttr', 'CudChewConsumeBerryAbAttr', 'CudChewRecordBerryAbAttr', 'PostTurnRestoreBerryAbAttr'];
  for (const name of required) if (!classes.has(name)) throw new Error(`Missing upstream ${name}`);
  const effect = classes.get('DoubleBerryEffectAbAttr').raw.match(/effectValue\.value\s*\*=\s*(\d+(?:\.\d+)?)\s*;/);
  const threshold = classes.get('ReduceBerryUseThresholdAbAttr').raw.match(/hpRatioReq\.value\s*\*=\s*(\d+(?:\.\d+)?)\s*;/);
  if (!effect || !threshold || !/return hpRatioReq\.value < hpRatio\s*;/.test(classes.get('ReduceBerryUseThresholdAbAttr').raw))
    throw new Error('Unsupported upstream Berry multiplier semantics');
  if (!/toDmgValue\(pokemon\.getMaxHp\(\) \* this\.healPercent\)/.test(classes.get('HealFromBerryUseAbAttr').raw))
    throw new Error('Unsupported upstream Berry healing semantics');
  const parents = name => {
    const chain = [], visited = new Set();
    while (name) {
      if (visited.has(name) || !classes.has(name)) return null;
      visited.add(name); chain.push(name);
      name = classes.get(name).parent;
    }
    return chain.includes('AbAttr') ? chain : null;
  };
  const entries = abilities.map(ability => {
    const raw = ability.extensions?.upstreamRawRecord?.value ?? '';
    const declarations = [...raw.matchAll(/\.(?:attr|conditionalAttr)\s*\(\s*([A-Za-z_$][\w$]*)/g)];
    const calls = [...raw.matchAll(/\.(?:attr|conditionalAttr)\s*\(/g)];
    const callbacks = [];
    let resolved = calls.length === declarations.length;
    for (const declaration of declarations) {
      const chain = parents(declaration[1]);
      if (!chain) { resolved = false; continue; }
      for (const name of required) if (chain.includes(name)) {
        callbacks.push(name);
        // A new subclass can override apply/canApply; do not inherit behavior blindly.
        if (declaration[1] !== name) resolved = false;
      }
    }
    if (callbacks.length && /\.(?:condition|conditionalAttr|unimplemented|partial)\s*\(/.test(raw)) resolved = false;
    let healFraction = 0;
    if (callbacks.includes('HealFromBerryUseAbAttr')) {
      const literal = raw.match(/\.attr\s*\(\s*HealFromBerryUseAbAttr\s*,\s*(\d+(?:\.\d+)?)\s*(?:\/\s*(\d+(?:\.\d+)?))?\s*\)/);
      if (!literal) resolved = false;
      else {
        const value = Number(literal[1]) / Number(literal[2] ?? 1);
        if (!Number.isFinite(value)) resolved = false;
        else healFraction = Math.max(0, Math.min(1, value));
      }
    }
    return { abilityId: ability.abilityId, id: ability.id, resolved, callbacks,
      effectMultiplier: callbacks.includes('DoubleBerryEffectAbAttr') ? Number(effect[1]) : 1,
      thresholdMultiplier: callbacks.includes('ReduceBerryUseThresholdAbAttr') ? Number(threshold[1]) : 1,
      preventsUse: callbacks.includes('PreventBerryUseAbAttr'), healFraction,
      cudChewConsume: callbacks.includes('CudChewConsumeBerryAbAttr'),
      cudChewRecord: callbacks.includes('CudChewRecordBerryAbAttr'),
      harvest: callbacks.includes('PostTurnRestoreBerryAbAttr'), raw,
      provenance: ability.source ?? ability.provenance };
  });
  return { entries, ancestry: [...classes.values()].map(({name,parent}) => ({name,parent})),
    raw: source, thresholdConditional: 'threshold < hpRatio', healingRounding: 'toDmgValue' };
}

export function parseBerryHealingRounding(source) {
  const declaration = /export\s+function\s+toDmgValue\s*\(\s*value\s*:\s*number\s*\)\s*:\s*number\s*\{/.exec(source);
  if (!declaration) throw new Error('Missing pinned toDmgValue');
  const raw = extractBalancedLiteral(source, declaration.index + declaration[0].length - 1);
  const match = /^\{\s*return\s+Math\.max\(Math\.floor\(value\),\s*(\d+)\)\s*;\s*\}$/.exec(raw);
  if (!match || Number(match[1]) < 1 || Number(match[1]) > 65535) throw new Error('Unsupported pinned toDmgValue semantics');
  return { kind: 'FLOOR_WITH_MINIMUM', minimum: Number(match[1]), raw };
}

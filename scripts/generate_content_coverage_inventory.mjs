import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const input = await fs.readFile(path.join(root, 'project/data/pokerogue/canonical-content.json'));
const content = JSON.parse(input);
if (content.sourceSnapshot?.sourceType !== 'UPSTREAM' || !content.sourceSnapshot?.revision) {
  throw new Error('Coverage inventory requires a pinned UPSTREAM snapshot');
}
// Inspect actual pinned class definitions; ancestry is metadata, never execution evidence.
const revision = content.sourceSnapshot.revision;
const inspectedSources = {};
const parents = new Map();
for (const sourcePath of ['src/data/moves/move.ts', 'src/data/abilities/ab-attrs.ts']) {
  const bytes = execFileSync('git', ['-C', path.join(root, 'build/upstream/pokerogue'),
    'show', `${revision}:${sourcePath}`], { maxBuffer: 16 * 1024 * 1024 });
  const text = bytes.toString('utf8');
  inspectedSources[sourcePath] = { revision, sha256: createHash('sha256').update(bytes).digest('hex') };
  for (const match of text.matchAll(/(?:export\s+)?(?:abstract\s+)?class\s+(\w+)\s+extends\s+(\w+)/g))
    parents.set(match[1], { parent: match[2], sourcePath });
  if (sourcePath.endsWith('/move.ts') &&
      !text.includes('this.addAttr(new HealStatusEffectAttr(false, StatusEffect.FREEZE))'))
    throw new Error('Pinned FIRE constructor cure requires a fresh semantic audit');
}
function ancestry(name) {
  const result = [], seen = new Set([name]);
  while (parents.has(name)) {
    const entry = parents.get(name);
    if (seen.has(entry.parent)) throw new Error(`Attribute inheritance cycle: ${name}`);
    seen.add(entry.parent);
    result.push({ name: entry.parent, sourcePath: entry.sourcePath });
    name = entry.parent;
  }
  return result;
}
const compare = (a, b) => a < b ? -1 : a > b ? 1 : 0;
function catalog(domain, numericId) {
  if (!Array.isArray(content.collections?.[domain])) throw new Error(`Missing catalog: ${domain}`);
  const seen = new Set();
  return content.collections[domain].map(record => {
    const id = record[numericId];
    if (!Number.isInteger(id) || seen.has(id)) throw new Error(`Invalid/duplicate ${domain} ID: ${id}`);
    seen.add(id);
    const raw = record.extensions?.upstreamRawRecord?.value;
    if (typeof raw !== 'string' || !raw) throw new Error(`Missing raw declaration: ${domain}/${id}`);
    const source = record.source ?? record.metadata;
    if (!source?.sourcePath || !source?.sourceSymbol || !source?.sourceHash || !source?.sourceRevision) {
      throw new Error(`Missing provenance: ${domain}/${id}`);
    }
    const attributes = [...raw.matchAll(/\.attr\s*\(\s*([A-Za-z_$][\w$]*)/g)].map(match => match[1]);
    const builders = [...new Set([...raw.matchAll(/\.([A-Za-z_$][\w$]*)\s*\(/g)]
      .map(match => match[1]).filter(name => name !== 'attr'))].sort(compare);
    const implicitAttributes = domain === 'moves' && record.type === 'FIRE' && record.category !== 'Status'
      ? [{ name: 'HealStatusEffectAttr', selfTarget: false, status: 'FREEZE',
          sourcePath: 'src/data/moves/move.ts', sourceSymbol: 'AttackMove.constructor' }] : [];
    return { id, canonicalId: record.id, attributes, builders, implicitAttributes,
      runtimeAssessment: 'NOT_AUDITED', executionEvidence: null,
      provenance: { repository: source.sourceRepository, revision: source.sourceRevision,
        sourcePath: source.sourcePath, sourceSymbol: source.sourceSymbol, sha256: source.sourceHash } };
  }).sort((a, b) => a.id - b.id);
}
const catalogs = { moves: catalog('moves', 'moveId'), abilities: catalog('abilities', 'abilityId') };
function families(records) {
  const groups = new Map();
  for (const record of records) for (const attribute of new Set(record.attributes)) {
    if (!groups.has(attribute)) groups.set(attribute, []);
    groups.get(attribute).push(record.id);
  }
  return [...groups].sort(([a], [b]) => compare(a, b)).map(([attribute, ids]) => ({ attribute, count: ids.length, ids }));
}
const attributeInheritance = Object.fromEntries([...new Set(Object.values(catalogs)
  .flatMap(records => records.flatMap(record => record.attributes)))].sort(compare)
  .map(name => [name, ancestry(name)]));
const inventory = { schemaVersion: 2, inspectedSources, attributeInheritance, inputSha256: createHash('sha256').update(input).digest('hex'),
  upstreamRevision: content.sourceSnapshot.revision,
  assessment: 'DECLARATION_INVENTORY_ONLY_NOT_RUNTIME_COVERAGE',
  counts: Object.fromEntries(Object.entries(catalogs).map(([domain, records]) => [domain, records.length])),
  families: Object.fromEntries(Object.entries(catalogs).map(([domain, records]) => [domain, families(records)])),
  catalogs };
const output = path.join(root, 'docs/generated');
await fs.mkdir(output, { recursive: true });
await fs.writeFile(path.join(output, 'CONTENT_COVERAGE_INVENTORY.json'), JSON.stringify(inventory, null, 2) + '\n');
const lines = ['# Inventario canónico de movimientos y habilidades', '',
  `Revisión upstream: ${inventory.upstreamRevision}.`, '',
  'Inventario de declaraciones, no porcentaje de soporte runtime. Cada registro queda NOT_AUDITED hasta inspeccionar ejecución, consumidor y prueba.', '',
  'El JSON conserva herencia inspeccionada y cura FREEZE implícita del constructor Fuego. Herencia no equivale a efectos ejecutados; otros efectos implícitos aún requieren auditoría.', '',
  'El JSON asociado contiene IDs, builders y provenance. Las familias cuentan registros distintos; un registro puede pertenecer a varias familias.', ''];
for (const [domain, rows] of Object.entries(inventory.families)) {
  lines.push(`## ${domain}: ${inventory.counts[domain]} registros`, '', '| Atributo | Registros |', '|---|---:|');
  for (const row of rows) lines.push(`| ${row.attribute} | ${row.count} |`);
  lines.push('');
}
await fs.writeFile(path.join(output, 'CONTENT_COVERAGE_INVENTORY.md'), lines.join('\n') + '\n');
console.log(JSON.stringify({ counts: inventory.counts, families: Object.fromEntries(
  Object.entries(inventory.families).map(([key, rows]) => [key, rows.length])), inputSha256: inventory.inputSha256 }));

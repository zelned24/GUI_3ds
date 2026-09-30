import fs from 'node:fs/promises';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { POKEROGUE_REPOSITORIES } from '../tools/js/data/PokerogueSource.js';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const repo = path.join(root, 'build/upstream/pokerogue-assets');
const revision = POKEROGUE_REPOSITORIES['pokerogue-assets'].revision;
const head = execFileSync('git', ['rev-parse', 'HEAD'], { cwd: repo, encoding: 'utf8' }).trim();
if (head !== revision) throw new Error(`Asset tree revision ${head} does not match ${revision}`);
const paths = execFileSync('git', ['ls-tree', '-r', '--name-only', 'HEAD', 'images/pokemon'], {
  cwd: repo, encoding: 'utf8', maxBuffer: 16 * 1024 * 1024
}).trim().split(/\r?\n/);
const files = { front: new Map(), back: new Map() };
for (const sourcePath of paths) {
  const match = /^images\/pokemon\/(back\/)?([1-9][0-9]*(?:-[a-z0-9-]+)?)\.(json|png)$/.exec(sourcePath);
  if (!match) continue;
  const facing = match[1] ? 'back' : 'front';
  const key = match[2];
  const id = Number(key.split('-')[0]);
  if (!Number.isSafeInteger(id)) throw new Error(`Invalid asset ID in ${sourcePath}`);
  const record = files[facing].get(key) ?? new Set();
  record.add(match[3]);
  files[facing].set(key, record);
}
const ids = {};
const forms = {};
for (const facing of ['front', 'back']) {
  const complete = [...files[facing]].filter(([, extensions]) => extensions.has('json') && extensions.has('png'))
    .map(([key]) => key);
  ids[facing] = complete.filter(key => !key.includes('-')).map(Number).sort((a, b) => a - b);
  forms[facing] = complete.filter(key => key.includes('-')).sort();
}
const source = `// Generated from ${POKEROGUE_REPOSITORIES['pokerogue-assets'].url}@${revision}\n`
  + `// Complete base and named-form PNG/JSON pairs. Shiny and variant directories are separate.\n`
  + `export const POKEROGUE_BASE_ATLAS_REVISION = '${revision}';\n`
  + `export const POKEROGUE_BASE_ATLAS_IDS = Object.freeze(${JSON.stringify(ids)});\n`
  + `export const POKEROGUE_FORM_ATLAS_KEYS = Object.freeze(${JSON.stringify(forms)});\n`
  + `export function hasPokerogueBaseAtlas(speciesId, facing = 'front') {\n`
  + `  return Number.isSafeInteger(speciesId) && POKEROGUE_BASE_ATLAS_IDS[facing]?.includes(speciesId) === true;\n`
  + `}\n`
  + `export function hasPokerogueFormAtlas(key, facing = 'front') {\n`
  + `  return typeof key === 'string' && POKEROGUE_FORM_ATLAS_KEYS[facing]?.includes(key) === true;\n`
  + `}\n`;
const output = path.join(root, 'tools/js/data/PokerogueBaseAtlasIndex.js');
await fs.writeFile(output, source);
console.log(`Indexed ${ids.front.length}/${ids.back.length} base and ${forms.front.length}/${forms.back.length} named-form front/back atlas pairs`);

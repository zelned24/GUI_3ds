import fs from 'node:fs/promises';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

// Asset conversion only. This script never compiles the 3DS program.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const python = process.env.POKEROGUE_PYTHON || (process.platform === 'win32' ? 'python' : 'python3');
let tex3ds = process.env.TEX3DS || 'tex3ds';
if (!process.env.TEX3DS && process.platform === 'win32') {
  // Some Windows shells inherit a Unix DEVKITPRO value; also check the local
  // devkitPro installation before falling back to PATH.
  for (const candidate of [process.env.DEVKITPRO, 'C:\\devkitPro']) {
    if (!candidate || !path.isAbsolute(candidate)) continue;
    const installed = path.join(candidate, 'tools/bin/tex3ds.exe');
    try { await fs.access(installed); tex3ds = installed; break; }
    catch { /* Try the next installation root. */ }
  }
}
const run = (file, args) => execFileSync(file, args, { cwd: root, stdio: 'inherit', env: { ...process.env, TEX3DS: tex3ds } });
const report = name => fs.readFile(path.join(root, 'build/upstream-assets', name), 'utf8').then(JSON.parse);

// --staged resumes conversion of the existing verified inventory. It does not
// claim to import appearances that have not yet been staged.
if(!process.argv.includes('--staged')) {
  run(python, ['scripts/materialize_pokemon_appearance_catalog.py']);
  run(process.execPath, ['scripts/stage_pokerogue_sprite_assets.mjs', '--all', '--appearances']);
}
let staged = await report('staged-sprite-assets.json');
for (const entry of [...staged.unsupported]) {
  if (entry.classification !== 'INVALID_UPSTREAM_FRAME_BOUNDS')
    throw new Error(`Unresolved pinned atlas ${entry.key}:${entry.facing}: ${entry.classification}`);
  run(python, ['scripts/pad_pokerogue_sprite_atlas.py', entry.key, entry.facing]);
}
staged = await report('staged-sprite-assets.json');
if (staged.unsupported.length) throw new Error('Some staged atlases remain unsupported');

run(python, ['scripts/native_sprite_pixels.py', '--tex3ds', tex3ds]);
run(process.execPath, ['scripts/convert_pokerogue_sprite_atlases.mjs', '--convert', '--native-pixels']);
const plan = await report('atlas-conversion-plan.json');
for (const entry of plan.unsupported) {
  if (entry.classification !== 'NOT_YET_SUPPORTED_BY_3DS_TEXTURE_EDGE')
    throw new Error(`Atlas conversion failed ${entry.atlasKey}:${entry.facing}: ${entry.classification}`);
  run(python, ['scripts/split_pokerogue_sprite_atlas.py', entry.atlasKey,
    entry.facing, '--tex3ds', tex3ds]);
}
run(process.execPath, ['scripts/finalize_pokerogue_sprite_conversion.mjs']);

run(process.execPath, ['scripts/generate_pokemon_appearance_index.mjs']);

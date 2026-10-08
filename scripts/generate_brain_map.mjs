import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const compare = (a, b) => a < b ? -1 : a > b ? 1 : 0;
const files = [];
async function walk(dir) {
  for (const entry of await fs.readdir(path.join(root, dir), { withFileTypes: true })) {
    const relative = `${dir}/${entry.name}`;
    if (entry.isDirectory()) await walk(relative);
    else if (/\.(?:js|mjs|py|cpp|hpp|h)$/.test(relative)) files.push(relative);
  }
}
for (const dir of ['project/src', 'project/include', 'project/generated/include', 'scripts', 'tools/js', 'test']) await walk(dir);
files.sort(compare);
const known = new Set(files);
function layer(p) {
  if (p.startsWith('test/')) return 'tests';
  if (p.startsWith('project/generated/')) return 'generated';
  if (p.includes('/game/')) return 'gameplay';
  if (p.includes('/storage/')) return 'storage';
  if (p.startsWith('project/') && p.includes('/content/')) return 'content-update';
  if (p.startsWith('project/') && /\/(runtime|gfx)\//.test(p)) return 'presentation';
  if (p.startsWith('project/')) return 'entry';
  if (p.startsWith('scripts/') || p.includes('/data/')) return 'content-tools';
  return 'presentation-tools';
}
const edges = [];
for (const from of files) {
  const source = await fs.readFile(path.join(root, from), 'utf8');
  const js = /(?:from\s*|import\s*\(|import\s*)['"]([^'"]+)['"]/g;
  const cpp = /#\s*include\s*"([^"]+)"/g;
  const regex = /\.(js|mjs)$/.test(from) ? js : /\.(cpp|hpp|h)$/.test(from) ? cpp : null;
  if (!regex) continue;
  for (const match of source.matchAll(regex)) {
    const target = match[1];
    const candidates = regex === js ? (target.startsWith('.') ? [path.posix.normalize(path.posix.join(path.posix.dirname(from), target))] : []) : [path.posix.normalize(path.posix.join(path.posix.dirname(from), target)), `project/include/${target}`, `project/generated/include/${target}`];
    const to = candidates.find(p => known.has(p));
    if (to) edges.push({ from, to, kind: regex === js ? 'import' : 'include' });
  }
}
const unique = [...new Map(edges.map(e => [JSON.stringify(e), e])).values()].sort((a,b) => compare(JSON.stringify(a), JSON.stringify(b)));
const tasks = {
  species: ['tools/js/data/PokerogueImporter.js', 'scripts/import_pokerogue_content.mjs', 'scripts/generate_3ds_runtime_content.mjs', 'test/migration_content_tests.js'],
  battle: ['project/include/game/PokemonBattleState.hpp', 'project/src/game/PokemonBattleState.cpp', 'test/battle_tests.js'],
  presentation: ['project/src/main.cpp', 'project/src/runtime/PokemonAtlasPresenter.cpp', 'project/src/gfx/renderer2d.cpp'],
  nativeFonts: ['scripts/pixel_font.py', 'project/src/gfx/renderer2d.cpp', 'project/include/runtime/NativeTextRaster.hpp', 'test/pixel_font_tests.py', 'test/ui_font_coverage_tests.py'],
  pokemonAppearances: ['scripts/starter_variant_icons.py', 'test/starter_variant_icon_tests.py', 'scripts/materialize_pokemon_appearance_catalog.py', 'scripts/pokemon_variant_palette.py',
    'scripts/stage_pokerogue_sprite_assets.mjs', 'scripts/prepare_pokerogue_sprite_catalog.mjs',
    'scripts/generate_pokemon_appearance_index.mjs', 'project/generated/include/content/PokemonAppearanceAssets.hpp',
    'project/src/runtime/PokemonAtlasPresenter.cpp', 'test/pokemon_variant_palette_tests.py',
    'test/pokemon_appearance_index_tests.mjs', 'test/native/pokemon_icon_index_harness.cpp'],
  frontendMenus: ['project/include/runtime/FrontendMenuPresenter.hpp', 'project/src/main.cpp',
    'project/include/storage/NativeProgressStore.hpp', 'test/frontend_menu_source_tests.mjs',
    'test/native/frontend_menu_harness.cpp'],
  save: ['project/src/storage/NativeRunSave.cpp', 'project/src/storage/SdNativeSaveStorage.cpp'],
  update: ['project/src/content/ContentUpdateStore.cpp', 'docs/progress/NATIVE_RUNTIME_CONTENT_PACK_CONTRACT.md']
};
for (const entries of Object.values(tasks)) for (const p of entries) await fs.access(path.join(root,p));
const output = '{\n"schemaVersion":1,\n"scope":"Static local imports/includes; no execution or completion proof",\n"nodes":[\n' + files.map(p => JSON.stringify({path:p,layer:layer(p)})).join(',\n') + '\n],\n"edges":[\n' + unique.map(e => JSON.stringify(e)).join(',\n') + '\n],\n"tasks":' + JSON.stringify(tasks) + '\n}\n';
await fs.writeFile(path.join(root, 'docs/brain-map.json'), output);
console.log(`Brain map: ${files.length} files, ${unique.length} dependencies`);

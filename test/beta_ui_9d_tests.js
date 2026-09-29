import assert from 'assert';
import fs from 'fs';
import path from 'path';
import { execFileSync } from 'child_process';
import { fileURLToPath } from 'url';

const testDir = path.dirname(fileURLToPath(import.meta.url));
const rootDir = path.resolve(testDir, '..');

export function registerBetaUI9DTests(test) {
  test('BETA-UI-9D: canonical species and moves initialize validated native battle state', () => {
    const clangCandidates = process.platform === 'win32'
      ? [path.join(rootDir, 'node_modules', 'clang-wasm-win64', 'clang.exe'), path.join(rootDir, 'node_modules', '.bin', 'clang.exe')]
      : [path.join(rootDir, 'node_modules', 'clang-wasm-linux-x64', 'clang'), path.join(rootDir, 'node_modules', 'clang-wasm-linux-arm64', 'clang'), path.join(rootDir, 'node_modules', '.bin', 'clang')];
    const clang = clangCandidates.find(candidate => fs.existsSync(candidate));
    if (!clang) {
      const error = new Error('BLOCKED — missing toolchain/dependency: bundled clang-wasm');
      error.isToolchainBlocked = true;
      throw error;
    }

    const out = path.join(rootDir, 'test', 'native', 'pokemon_battle_state_test.wasm');
    const generatedInclude = path.join(rootDir, 'project', 'generated', 'include');
    const projectInclude = path.join(rootDir, 'project', 'include');
    const hostCompat = path.join(rootDir, 'test', 'native', 'host_compat');
    const compile = [
      '--target=wasm32', '-O2', '-nostdlib', '-fno-rtti', '-fno-exceptions',
      '-Wl,--no-entry', '-Wl,--export-all', `-I${generatedInclude}`, `-I${projectInclude}`, `-I${hostCompat}`,
      '-o', out,
      path.join(rootDir, 'project', 'src', 'game', 'PokemonBattleState.cpp'),
      path.join(testDir, 'native', 'pokemon_battle_state_harness.cpp'),
    ];
    try {
      execFileSync(clang, compile, { stdio: 'pipe' });
      const module = new WebAssembly.Module(fs.readFileSync(out));
      const instance = new WebAssembly.Instance(module);
      assert.strictEqual(instance.exports.runPokemonBattleStateChecks(), 0,
        'pinned canonical battle state and real Pikachu Gigantamax level-move candidates match the native contract');
    } finally {
      if (fs.existsSync(out)) fs.rmSync(out);
    }
  });
}

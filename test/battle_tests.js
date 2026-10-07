import assert from 'assert';
import fs from 'fs';
import path from 'path';
import { execFileSync } from 'child_process';
import { fileURLToPath } from 'url';

const testDir = path.dirname(fileURLToPath(import.meta.url));
const rootDir = path.resolve(testDir, '..');

export function registerBattleTests(test) {
  test('Native battle state: canonical species and moves initialize validated native battle state', () => {
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
      '-Wl,--no-entry', '-Wl,--export-all', '-Wl,-z,stack-size=4194304', `-I${generatedInclude}`, `-I${projectInclude}`, `-I${hostCompat}`,
      '-o', out,
      path.join(rootDir, 'project', 'src', 'game', 'PokemonBattleState.cpp'),
      path.join(rootDir, 'project', 'src', 'storage', 'NativeRunSave.cpp'),
      path.join(testDir, 'native', 'native_save_harness.cpp'),
      path.join(testDir, 'native', 'pokemon_battle_state_harness.cpp'),
    ];
    try {
      execFileSync(clang, compile, { stdio: 'pipe' });
      const module = new WebAssembly.Module(fs.readFileSync(out));
      let instance;
      let heap = 0;
      const live = new Map(), free = [];
      const imports = { env: {
        pokerogueTestPow: Math.pow,
        testAllocate(size) {
          size = Math.max(8, Math.ceil((size >>> 0) / 8) * 8);
          if (!heap) heap = Number(instance.exports.__heap_base.value);
          const index = free.findIndex(block => block.size >= size);
          let ptr;
          if (index >= 0) {
            const block = free.splice(index, 1)[0]; ptr = block.ptr;
            if (block.size > size) free.push({ ptr: ptr + size, size: block.size - size });
          } else {
            ptr = heap;
            const end = heap + size;
            const memory = instance.exports.memory;
            if (end > memory.buffer.byteLength) memory.grow(Math.ceil((end - memory.buffer.byteLength) / 65536));
            heap = end;
          }
          live.set(ptr, size); return ptr;
        },
        testRelease(ptr) {
          if (!ptr) return;
          assert.ok(live.has(ptr), 'allocator rejects invalid or double free');
          free.push({ ptr, size: live.get(ptr) }); live.delete(ptr);
          free.sort((a, b) => a.ptr - b.ptr);
          for (let i = 1; i < free.length;) {
            if (free[i-1].ptr + free[i-1].size === free[i].ptr) {
              free[i-1].size += free[i].size; free.splice(i, 1);
            } else ++i;
          }
        },
      } };
      instance = new WebAssembly.Instance(module, imports);
      const saveResult = instance.exports.runNativeSaveChecks();
      const battleResult = instance.exports.runPokemonBattleStateChecks();
      assert.deepStrictEqual({ save: saveResult, battle: battleResult }, { save: 0, battle: 0 },
        'native save journal and pinned canonical battle contract both pass');
    } finally {
      if (fs.existsSync(out)) fs.rmSync(out);
    }
  });
  test('Native first run: real encounters, turn commands and save/restore execute in C++', () => {
    const compiler = process.env.POKEROGUE_HOST_CXX || (process.platform === 'win32'
      ? 'C:/devkitPro/msys2/usr/bin/g++.exe' : 'g++');
    const env = { ...process.env, PATH: `${path.dirname(compiler)}${path.delimiter}${process.env.PATH || ''}` };
    const out = path.join(rootDir, 'build', `first_run_restore_test${process.platform === 'win32' ? '.exe' : ''}`);
    fs.mkdirSync(path.dirname(out), { recursive: true });
    try {
      execFileSync(compiler, ['-std=c++17', '-O2',
        ...(process.platform === 'win32' ? ['-Wl,--stack,33554432'] : []),
        `-I${path.join(rootDir, 'project', 'generated', 'include')}`,
        `-I${path.join(rootDir, 'project', 'include')}`, '-o', out,
        path.join(rootDir, 'project', 'src', 'game', 'FirstRunRuntime.cpp'),
        path.join(rootDir, 'project', 'src', 'game', 'PokemonBattleState.cpp'),
        path.join(rootDir, 'project', 'src', 'storage', 'NativeRunSave.cpp'),
        path.join(testDir, 'native', 'first_run_restore_harness.cpp'),
      ], { env, stdio: 'pipe' });
      const result = execFileSync(out, [], { env, encoding: 'utf8' });
      console.log(result.trim());
      assert.match(result, /FirstRunRuntime checks: 0(?:\r?\n|$)/);
    } catch (error) {
      error.message += `\n${error.stdout?.toString() || ''}\n${error.stderr?.toString() || ''}`;
      throw error;
    } finally {
      if (fs.existsSync(out)) fs.rmSync(out);
    }
  });

}

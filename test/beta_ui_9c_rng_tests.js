import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const testDir = path.dirname(fileURLToPath(import.meta.url));
const rootDir = path.resolve(testDir, '..');

function resolveClang() {
  const candidates = process.platform === 'win32'
    ? [path.join(rootDir, 'node_modules', 'clang-wasm-win64', 'clang.exe')]
    : [
        path.join(rootDir, 'node_modules', 'clang-linux-x64', 'bin', 'clang'),
        path.join(rootDir, 'node_modules', 'clang-wasm-linux-x64', 'clang'),
        path.join(rootDir, 'node_modules', '.bin', 'clang'),
      ];
  return candidates.find(candidate => fs.existsSync(candidate)) || null;
}

export function registerBetaUI9CRngTests(test) {
  test('BETA-UI-9C: Old 3DS Phaser RNG adapter matches pinned Phaser 3.90.0 golden vectors', async () => {
    const clang = resolveClang();
    if (!clang) {
      const error = new Error('BLOCKED — missing toolchain/dependency: clang-wasm; install from package-lock.json with npm ci');
      error.isToolchainBlocked = true;
      throw error;
    }
    const buildDir = path.join(testDir, 'native', 'build');
    fs.mkdirSync(buildDir, { recursive: true });
    const wasmPath = path.join(buildDir, 'pokerogue_rng_harness.wasm');
    execFileSync(clang, [
      '--target=wasm32', '-O2', '-nostdlib', '-fno-rtti', '-fno-exceptions',
      '-Wl,--no-entry', '-Wl,--export-all',
      `-I${path.join(rootDir, 'project', 'include')}`,
      `-I${path.join(testDir, 'native', 'host_compat')}`,
      '-o', wasmPath,
      path.join(testDir, 'native', 'pokerogue_rng_harness.cpp'),
    ], { stdio: 'pipe' });
    const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath));
    const api = instance.exports;
    const close = (actual, expected, label) => assert.ok(Math.abs(actual - expected) <= 1e-15, `${label}: ${actual} != ${expected}`);

    // Golden values were generated from Phaser v3.90.0 RandomDataGenerator.js
    // sow/rnd/frac/integerInRange/pick semantics, using seeds that exercise
    // root, shifted wave and standalone executeWithSeedOffset derivation.
    close(api.harness_pokerogue_rng_fraction(), 0.743767629869303, 'first Phaser frac()');
    assert.equal(api.harness_pokerogue_rng_range_511(), 380, 'root seed range [0, 511]');
    assert.equal(api.harness_pokerogue_rng_wave1_range_511(), 42, 'wave seed shiftCharCodes(seed, 1)');
    assert.equal(api.harness_pokerogue_rng_offset4_range_99(), 92, 'executeWithSeedOffset(seed, 4)');
    assert.equal(api.harness_pokerogue_rng_seed_offset0_range_7(), 5, 'seed offset zero');
    assert.equal(api.harness_pokerogue_rng_empty_pick_is_rejected(), 1, 'empty pool is explicitly rejected without fallback');
    assert.equal(api.harness_pokerogue_rng_singleton_is_no_draw(), 1, 'singleton randSeedItem consumes no RNG');
    assert.equal(api.harness_pokerogue_rng_range_one_is_no_draw(), 1, 'randSeedInt(range <= 1) consumes no RNG');
    assert.equal(api.harness_pokerogue_rng_state_roundtrip(), 1, 'serializable Phaser RNG state restores the next draw');
    assert.equal(api.harness_pokerogue_rng_offset_restores_state(), 1, 'seed-offset scope restores caller RNG state');
    assert.equal(api.harness_pokerogue_shift_wraps_utf16(), 1, 'shiftCharCodes follows UTF-16 modulo semantics');
    assert.equal(api.harness_pokerogue_wave_cycle_offset(), 1, 'Classic wave cycle uses root randSeedInt(8) multiplied by five');
    assert.equal(api.harness_pokerogue_time_of_day_boundaries(), 1, 'Arena.getTimeOfDay boundaries and 40-wave wrap match upstream');
  });
}

// Diagnostic host bundle only: no PokéRogue gameplay modules are imported here.
// Canonical import produces data, not an executable browser-free upstream engine.
import { mkdirSync, writeFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const outDir = resolve(root, 'build/romfs/js');
const browserStubs = `
// Compatibility placeholders for host diagnostics, not a Phaser implementation.
const window = globalThis;
const document = {
  createElement: () => ({style: {}, addEventListener: () => {}}),
  getElementById: () => null,
  querySelector: () => null,
  addEventListener: () => {},
};
const navigator = {userAgent: '3DS/QuickJS'};
const localStorage = {getItem: () => null, setItem: () => {}, removeItem: () => {}};
const sessionStorage = localStorage;
const performance = {now: () => 0};
// No recursive scheduling: main owns frames, and this bridge has no async job pump.
const requestAnimationFrame = () => {throw new Error('Use _3ds_tick; RAF unavailable');};
const cancelAnimationFrame = () => {};
const fetch = () => {throw new Error('Networking unavailable in diagnostic bundle');};
const XMLHttpRequest = undefined;
const WebSocket = undefined;
`;
const entry = `
// Native engine owns combat. This HUD only projects its read-only snapshot.
const WHITE = 0xFFFFFFFF, GREEN = 0xFF00FF88, RED = 0xFF4444FF;
function hpBar(current, max) {
  if (!(max > 0)) return '[------------]';
  const filled = Math.max(0, Math.min(12, Math.round(current / max * 12)));
  return '[' + '#'.repeat(filled) + '-'.repeat(12 - filled) + ']';
}
let previousJson = '', state = {};
globalThis._3ds_tick = function(input) {
  const json = _3ds_getBattleState();
  if (json !== previousJson) { state = JSON.parse(json); previousJson = json; }
  _3ds_beginTop();
  _3ds_clear(0xFF1A1A2E);
  _3ds_drawText('PokeRogue - Old 3DS', 90, 16, 0.7, WHITE);
  _3ds_drawText('Native battle / QuickJS HUD', 70, 42, 0.5, GREEN);
  _3ds_drawText('Gameplay coverage still incomplete', 55, 64, 0.45, WHITE);
  _3ds_beginBottom();
  _3ds_clear(0xFF16213E);
  _3ds_drawText('Wave: ' + (state.wave || 0), 10, 10, 0.65, 0xFF00FFFF);
  _3ds_drawText('Player #' + (state.playerDex || 0), 10, 36, 0.55, WHITE);
  _3ds_drawText('HP: ' + (state.playerHp || 0) + '/' + (state.playerMaxHp || 0) + ' ' + hpBar(state.playerHp, state.playerMaxHp), 10, 54, 0.48, GREEN);
  _3ds_drawText('Enemy #' + (state.enemyDex || 0), 10, 80, 0.55, WHITE);
  _3ds_drawText('HP: ' + (state.enemyHp || 0) + '/' + (state.enemyMaxHp || 0) + ' ' + hpBar(state.enemyHp, state.enemyMaxHp), 10, 98, 0.48, RED);
  _3ds_drawText('Move #' + (state.moveId || 0) + ' PP: ' + (state.pp || 0), 10, 125, 0.5, WHITE);
  _3ds_drawText(state.finished ? 'Battle finished' : state.supported ? 'Battle input available' : 'Pending rules block this battle', 10, 147, 0.45, state.supported ? GREEN : RED);
  _3ds_drawText('Up/Down: move  Left/Right: starter', 8, 185, 0.43, WHITE);
  _3ds_drawText('A:Turn/EXP  B:Skip reward  X:Save', 8, 203, 0.43, WHITE);
  _3ds_drawText('Y:Export  L:Load  R:Import', 8, 219, 0.43, WHITE);
};
`;

mkdirSync(outDir, {recursive: true});
const bundle = browserStubs + entry;
const output = resolve(outDir, 'bundle.js');
writeFileSync(output, bundle, 'utf8');
console.log(`[bundle_quickjs] Diagnostic bundle: ${output} (${Buffer.byteLength(bundle)} bytes)`);

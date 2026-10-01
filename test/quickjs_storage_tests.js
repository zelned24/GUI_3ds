import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

// Exercise the exact diagnostic script template without generating ROMFS or
// starting QuickJS/GPU. Native journal/replay regressions remain separate.
export function registerQuickJsStorageTests(register) {
  const source = readFileSync(new URL('../scripts/bundle_quickjs.mjs', import.meta.url), 'utf8');
  const match = source.match(/const entry = `([\s\S]*?)`;/);
  assert.ok(match, 'diagnostic entry template exists');
  for (const [key, binding] of [['X', '_3ds_importNative'], ['Y', '_3ds_exportNative'],
    ['L', '_3ds_saveNative'], ['R', '_3ds_loadNative']]) {
    register(`QuickJS storage: ${key} queues only ${binding}`, () => {
      const commands = [];
      const state = { runStarted: true, finished: false, supported: true,
        playerMoves: [1], playerPP: [10] };
      const context = { JSON, Math };
      for (const name of new Set(match[1].match(/_3ds_[A-Za-z]+/g))) {
        context[name] = () => undefined;
      }
      context._3ds_getBattleState = () => JSON.stringify(state);
      context._3ds_getPresentationInfo = () => ({});
      context._3ds_getCombatLog = () => '';
      context._3ds_getMoveName = () => 'Move';
      for (const name of ['_3ds_importNative', '_3ds_exportNative', '_3ds_saveNative',
        '_3ds_loadNative', '_3ds_submitAction', '_3ds_skipReward', '_3ds_resetRun']) {
        context[name] = (...args) => { commands.push([name, ...args]); return true; };
      }
      vm.createContext(context);
      vm.runInContext(match[1], context);
      context._3ds_tick({ [key]: true, A: true, start: true });
      assert.deepEqual(commands, [[binding]], 'storage takes priority over combat commands');
      commands.length = 0;
      context._3ds_tick({});
      assert.deepEqual(commands, [], 'no repeated storage command without another key pulse');
    });
  }
}

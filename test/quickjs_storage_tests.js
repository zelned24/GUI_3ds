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
  register('QuickJS rewards: confirmation opens pool and recovery targets a reserve', () => {
    const commands = [];
    const state = { runStarted: true, finished: true, playerWon: true,
      experienceGranted: true, rewardPending: false, selectedMove: 0 };
    const presentation = { playerPartyCount: 2, activePlayerPartyIndex: 0,
      playerParty: [1, 16], rewardRecovery: true, rewardChoices: ['POTION'], selectedRewardChoice: 0 };
    const context = { JSON, Math };
    for (const name of new Set(match[1].match(/_3ds_[A-Za-z]+/g))) context[name] = () => undefined;
    context._3ds_getBattleState = () => JSON.stringify(state);
    context._3ds_getPresentationInfo = () => presentation;
    context._3ds_getCombatLog = () => '';
    const drawn = [];
    context._3ds_drawText = text => drawn.push(text);
    presentation.playerPartyDetails = [
      { name: 'Bulbasaur', hp: 20, maxHp: 20, moves: [{id: 33, pp: 10, maxPp: 35}] },
      { name: 'Pidgey', hp: 3, maxHp: 19, moves: [{id: 33, pp: 4, maxPp: 35}, {id: 16, pp: 2, maxPp: 35}] }
    ];
    context._3ds_getMoveName = () => 'Move';
    context._3ds_submitAction = (...args) => { commands.push(['action', ...args]); return true; };
    context._3ds_skipReward = () => { commands.push(['skip']); return true; };
    vm.createContext(context);
    vm.runInContext(match[1], context);
    context._3ds_tick({ A: true });
    assert.deepEqual(commands, [['action', 0]], 'generate rewards instead of automatically skipping');
    commands.length = 0;
    state.rewardPending = true;
    context._3ds_tick({ A: true });
    assert.deepEqual(commands, [], 'recipient choice does not apply the reward yet');
    context._3ds_tick({ down: true });
    context._3ds_tick({ right: true });
    context._3ds_tick({ A: true });
    assert.deepEqual(commands, [['action', 305]], 'reserve index one, move slot one');
    assert.ok(drawn.some(text => text.includes('Pidgey HP:3/19')));
    assert.ok(drawn.some(text => text.includes('Move PP:2/35')));
    commands.length = 0;
    context._3ds_tick({ B: true });
    assert.deepEqual(commands, [], 'cancel recipient selection keeps the pending reward');
    context._3ds_tick({ B: true });
    assert.deepEqual(commands, [['skip']], 'only explicit skip advances without an item');
  });
  register('QuickJS rewards: held reward targets a reserve without a move slot', () => {
    const commands = [];
    const state = { runStarted: true, finished: true, playerWon: true,
      experienceGranted: true, rewardPending: false, selectedMove: 0 };
    const presentation = { playerPartyCount: 2, activePlayerPartyIndex: 0,
      playerParty: [1, 16], rewardRecovery: false, rewardHeld: true, rewardChoices: ['LEFTOVERS'], selectedRewardChoice: 0 };
    const context = { JSON, Math };
    for (const name of new Set(match[1].match(/_3ds_[A-Za-z]+/g))) context[name] = () => undefined;
    context._3ds_getBattleState = () => JSON.stringify(state);
    context._3ds_getPresentationInfo = () => presentation;
    context._3ds_getCombatLog = () => '';
    context._3ds_getMoveName = () => 'Move';
    context._3ds_submitAction = (...args) => { commands.push(['action', ...args]); return true; };
    context._3ds_skipReward = () => { commands.push(['skip']); return true; };
    vm.createContext(context);
    vm.runInContext(match[1], context);
    context._3ds_tick({ A: true });
    assert.deepEqual(commands, [['action', 0]], 'generate rewards instead of automatically skipping');
    commands.length = 0;
    state.rewardPending = true;
    context._3ds_tick({ A: true });
    assert.deepEqual(commands, [], 'recipient choice does not apply the reward yet');
    context._3ds_tick({ down: true });
    context._3ds_tick({ right: true });
    context._3ds_tick({ A: true });
    assert.deepEqual(commands, [['action', 331]], 'reserve index one is the held item owner');
    commands.length = 0;
    context._3ds_tick({ B: true });
    assert.deepEqual(commands, [], 'cancel recipient selection keeps the pending reward');
    context._3ds_tick({ B: true });
    assert.deepEqual(commands, [['skip']], 'only explicit skip advances without an item');
  });

  for (const phase of ['moveLearningPending', 'evolutionPending']) {
    register(`QuickJS progression: ${phase} precedes capture and party commands`, () => {
      const commands = [];
      const context = { JSON, Math };
      for (const name of new Set(match[1].match(/_3ds_[A-Za-z]+/g))) context[name] = () => undefined;
      context._3ds_getBattleState = () => JSON.stringify({runStarted: true, finished: false, selectedMove: 3});
      context._3ds_getPresentationInfo = () => ({[phase]: true, progressionName: 'Pidgey', pendingLearnMoveId: 16});
      context._3ds_getCombatLog = () => '';
      context._3ds_getMoveName = () => 'Move';
      context._3ds_submitAction = id => commands.push(['action', id]);
      context._3ds_skipReward = () => commands.push(['decline']);
      vm.createContext(context);
      vm.runInContext(match[1], context);
      context._3ds_tick({ B: true, select: true });
      assert.deepEqual(commands, [['decline']], 'declines progression instead of throwing a ball');
      commands.length = 0;
      context._3ds_tick({ A: true });
      assert.deepEqual(commands, [['action', 3]], 'selected learning slot reaches the native progression handler');
    });
  }

  register('QuickJS party: Select opens menu without toggling evolution pause', () => {
    const commands = [];
    const context = { JSON, Math };
    for (const name of new Set(match[1].match(/_3ds_[A-Za-z]+/g))) context[name] = () => undefined;
    context._3ds_getBattleState = () => JSON.stringify({runStarted: true, finished: false});
    context._3ds_getPresentationInfo = () => ({playerPartyCount: 2, activePlayerPartyIndex: 0, playerParty: [1, 16]});
    context._3ds_getCombatLog = () => '';
    context._3ds_getMoveName = () => 'Move';
    context._3ds_submitAction = id => commands.push(id);
    vm.createContext(context);
    vm.runInContext(match[1], context);
    context._3ds_tick({select: true});
    assert.deepEqual(commands, [], 'opening party menu emits no gameplay mutation');
    context._3ds_tick({down: true});
    context._3ds_tick({A: true});
    assert.deepEqual(commands, [212], 'only confirmed switch to reserve is queued');
    // Static host ownership assertion supplements the script regression until
    // the native executable can be built and exercised at the final stage.
    const host = readFileSync(new URL('../project/src/main.cpp', import.meta.url), 'utf8');
    const start = host.indexOf('if (!jsCommands) {');
    const end = host.indexOf('Healthy JS owns menu/game controls', start);
    const ownershipBlock = host.slice(start, end);
    assert.ok(ownershipBlock.includes('togglePlayerEvolutionPause'));
    assert.ok(ownershipBlock.indexOf('togglePlayerEvolutionPause') < ownershipBlock.indexOf('\n        }\n'));
  });

  register('QuickJS capture: full party choice owns buttons before combat', () => {
    const commands = [];
    const context = { JSON, Math };
    for (const name of new Set(match[1].match(/_3ds_[A-Za-z]+/g))) context[name] = () => undefined;
    context._3ds_getBattleState = () => JSON.stringify({runStarted: true, finished: false});
    context._3ds_getPresentationInfo = () => ({capturePartyChoicePending: true,
      selectedCapturePartyChoice: 2, capturedName: 'Captured', playerPartyDetails: []});
    context._3ds_getCombatLog = () => '';
    context._3ds_submitAction = id => commands.push(id);
    vm.createContext(context);
    vm.runInContext(match[1], context);
    context._3ds_tick({up: true});
    context._3ds_tick({down: true});
    context._3ds_tick({A: true});
    context._3ds_tick({B: true});
    context._3ds_tick({select: true});
    assert.deepEqual(commands, [-1, 100, 342, 346]);
  });

  register('QuickJS snapshot: render loop does not construct save checkpoints', () => {
    const host = readFileSync(new URL('../project/src/main.cpp', import.meta.url), 'utf8');
    const start = host.indexOf('const auto& state = game.presentation();');
    const end = host.indexOf('bridge.setBattleStateJson', start);
    assert.ok(start >= 0 && end > start);
    const projection = host.slice(start, end);
    assert.ok(projection.includes('game.presentationStage()'));
    assert.ok(!projection.includes('captureNativeRunSave'));
    assert.ok(!projection.includes('NativeRunSave snapshot'));
  });

  register('QuickJS storage: command envelopes have checked ownership', () => {
    const bridge = readFileSync(new URL('../project/src/runtime/QuickJSBridge.cpp', import.meta.url), 'utf8');
    const start = bridge.indexOf('} else if (action == 206 || action == 207)');
    const end = bridge.indexOf('} else if (action == -1 || action == 100)', start);
    assert.ok(start >= 0 && end > start);
    const storage = bridge.slice(start, end);
    assert.equal((storage.match(/if \(!saveStorage\)/g) || []).length, 2);
    assert.ok(!storage.includes('NativeRunSave save{}'));
    assert.ok(!storage.includes('NativeRunSave stored{}'));
    assert.ok(storage.indexOf('if (!saveStorage)') < storage.indexOf('m_game->saveNativeProgress'));
  });

}

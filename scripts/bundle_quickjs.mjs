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
// Presentation palette only; canonical biome IDs drive selection, never encounter rules.
function biomeColor(id) {
  const palette = [0xFF2D1B4E, 0xFF30502A, 0xFF60432C, 0xFF4B5962];
  let hash = 0;
  for (let i = 0; i < id.length; ++i) hash = ((hash * 31) + id.charCodeAt(i)) >>> 0;
  return palette[hash % palette.length];
}
let previousJson = '', state = {}, partyMenu = false, partyCursor = 0, recipientMenu = false, recoveryMove = 0, starterCostMenu = false;
globalThis._3ds_tick = function(input) {
  const json = _3ds_getBattleState();
  if (json !== previousJson) { state = JSON.parse(json); previousJson = json; }
  // input contains hidKeysDown pulses already; do not edge-detect it as held keys.
  // Queue one command. Host processes it before the next snapshot/render frame.
  const presentation = _3ds_getPresentationInfo() || {};
  const gameOverScreen = state.finished && !state.playerWon;
  if (!state.rewardPending) recipientMenu = false;
  if (state.runStarted) starterCostMenu = false;
  if (input.X) _3ds_importNative();
  else if (input.Y) _3ds_exportNative();
  else if (input.L) _3ds_saveNative();
  else if (input.R) _3ds_loadNative();
  else if (!state.runStarted && starterCostMenu) {
    if (input.B) starterCostMenu = false;
    else if (input.A && presentation.starterCanReduce) {
      _3ds_purchaseStarterCost(); starterCostMenu = false;
    }
  }
  else if (!state.runStarted && input.B) { starterCostMenu = true; partyMenu = false; }
  else if (presentation.capturePartyChoicePending) {
    partyMenu = false;
    if (input.up) _3ds_submitAction(-1);
    else if (input.down) _3ds_submitAction(100);
    else if (input.A || input.start) _3ds_submitAction(340 + (presentation.selectedCapturePartyChoice || 0));
    else if (input.B) _3ds_submitAction(346);
  }
  else if (presentation.moveLearningPending || presentation.evolutionPending) {
    if (input.B) _3ds_skipReward();
    else if (input.A || input.start) _3ds_submitAction(state.selectedMove || 0);
    else if (presentation.moveLearningPending && input.up) _3ds_submitAction(-1);
    else if (presentation.moveLearningPending && input.down) _3ds_submitAction(100);
  }
  else if (gameOverScreen) {
    if (input.left) _3ds_cycleStarter(-1);
    else if (input.right) _3ds_cycleStarter(1);
    else if (input.start || input.A) _3ds_resetRun();
  } else if (state.rewardPending && recipientMenu) {
    const count = presentation.playerPartyCount || 1;
    if (input.up) partyCursor = (partyCursor + count - 1) % count;
    else if (input.down) partyCursor = (partyCursor + 1) % count;
    else if (input.left && presentation.rewardRecovery) recoveryMove = (recoveryMove + 3) % 4;
    else if (input.right && presentation.rewardRecovery) recoveryMove = (recoveryMove + 1) % 4;
    else if (input.B) recipientMenu = false;
    else if (input.A || input.start) _3ds_submitAction(presentation.rewardHeld
      ? 330 + partyCursor : 300 + partyCursor * 4 + recoveryMove);
  } else if (state.rewardPending && (input.A || input.start) && (presentation.rewardRecovery || presentation.rewardHeld)) {
    recipientMenu = true;
    partyCursor = presentation.activePlayerPartyIndex || 0;
    recoveryMove = 0;
  } else if (state.runStarted && !state.finished && input.select) {
    partyMenu = !partyMenu;
    partyCursor = presentation.activePlayerPartyIndex || 0;
  } else if (partyMenu && !state.finished) {
    const count = presentation.playerPartyCount || 1;
    if (input.up) partyCursor = (partyCursor + count - 1) % count;
    else if (input.down) partyCursor = (partyCursor + 1) % count;
    else if (input.B) partyMenu = false;
    else if (input.A) { _3ds_submitAction(211 + partyCursor); partyMenu = false; }
  } else if (input.B && !state.finished && state.runStarted) _3ds_submitAction(210);
  else if (state.runStarted && !state.finished && presentation.doubleBattle && input.left) _3ds_cycleStarter(-1);
  else if (state.runStarted && !state.finished && presentation.doubleBattle && input.right) _3ds_cycleStarter(1);
  else if (!state.runStarted && input.left) _3ds_cycleStarter(-1);
  else if (!state.runStarted && input.right) _3ds_cycleStarter(1);
  else if (!state.runStarted && input.up) _3ds_cycleStarterForm(-1);
  else if (!state.runStarted && input.down) _3ds_cycleStarterForm(1);
  else if (!state.runStarted && input.A) _3ds_toggleStarterTeam();
  else if (input.start || input.A) {
    if (!state.finished || state.playerWon) _3ds_submitAction(state.selectedMove || 0);
  } else if (input.up) _3ds_submitAction(-1);
  else if (input.down) _3ds_submitAction(100);
  else if (input.B && state.finished && state.playerWon && state.experienceGranted) _3ds_skipReward();
  const combatLog = _3ds_getCombatLog();
  if (presentation.classicClearPending) {
    _3ds_beginTop(); _3ds_clear(0xFF2D1B4E);
    _3ds_drawText('Final battle won', 80, 70, 0.8, GREEN);
    _3ds_drawText('GameClear plan detected', 70, 110, 0.55, WHITE);
    _3ds_drawText('Clear/save/profile phases still pending', 35, 155, 0.45, WHITE);
    _3ds_beginBottom(); _3ds_clear(0xFF16213E);
    _3ds_drawText('Classic completion is not verified', 20, 80, 0.5, WHITE);
    _3ds_drawText('R: load a compatible save', 25, 130, 0.5, WHITE);
    return;
  }
  if (gameOverScreen) {
    _3ds_beginTop(); _3ds_clear(0xFF0A0A0A);
    _3ds_drawText('DERROTA', 135, 72, 1.0, RED);
    _3ds_drawText('Wave ' + (state.wave || 0) + ' alcanzada', 115, 116, 0.55, WHITE);
    if (combatLog) _3ds_drawText(combatLog.slice(0, 65), 10, 220, 0.38, WHITE);
    _3ds_beginBottom(); _3ds_clear(0xFF0D0D1A);
    _3ds_drawText('Nueva run', 95, 24, 0.75, WHITE);
    _3ds_drawText('Starter: ' + _3ds_getStarterName(), 30, 83, 0.6, GREEN);
    _3ds_drawText('Left/Right: cambiar starter', 25, 160, 0.48, WHITE);
    _3ds_drawText('A/Start: reiniciar  R: cargar save', 15, 194, 0.45, WHITE);
    return;
  }
  _3ds_beginTop();
  _3ds_clear(biomeColor(presentation.biomeId || ''));
  if (!state.runStarted) {
    _3ds_drawText('Selecciona tu equipo', 100, 15, 0.65, GREEN);
    _3ds_drawStarter();
    _3ds_drawText(_3ds_getStarterName(), 145, 195, 0.55, WHITE);
    _3ds_drawText(presentation.starterInTeam ? 'En tu equipo' : 'A: incorporar', 145, 215, 0.45, GREEN);
  } else {
  if (state.enemyDex) _3ds_drawPokemon(state.enemyDex, false, presentation.doubleBattle ? 185 : 230, 42, presentation.doubleBattle ? 0.9 : 1.2);
  if (presentation.doubleBattle && presentation.secondEnemyDex)
    _3ds_drawPokemon(presentation.secondEnemyDex, false, 245, 88, 0.9, 2);
  if (state.playerDex) _3ds_drawPokemon(state.playerDex, true, 30, 100, 1.2);
  _3ds_drawText('Wave ' + (state.wave || 0) + ' - ' + (presentation.biomeName || presentation.biomeId || ''), 10, 8, 0.48, WHITE);
  _3ds_drawText('Classic ' + hpBar(state.wave, state.finalWave) + ' ' + (state.wave || 0) + '/' + (state.finalWave || 0), 10, 28, 0.38, WHITE);
  _3ds_drawText((presentation.enemyName || 'Enemy #' + (state.enemyDex || 0)).slice(0, 23), 222, 64, 0.4, WHITE);
  _3ds_drawText(hpBar(state.enemyHp, state.enemyMaxHp), 222, 79, 0.4, GREEN);
  if (presentation.doubleBattle && presentation.secondEnemyDex) {
    _3ds_drawText((presentation.secondEnemyName || 'Enemy #' + presentation.secondEnemyDex).slice(0, 23), 222, 174, 0.4, WHITE);
    _3ds_drawText(hpBar(presentation.secondEnemyHp, presentation.secondEnemyMaxHp), 222, 189, 0.4, GREEN);
  }
  _3ds_drawText('Weather: ' + (presentation.weatherName || 'NONE'), 10, 52, 0.4, WHITE);
  _3ds_drawText((presentation.playerName || 'Player #' + (state.playerDex || 0)).slice(0, 23), 10, 193, 0.48, WHITE);
  _3ds_drawText(hpBar(state.playerHp, state.playerMaxHp), 10, 208, 0.48, GREEN);
  if (combatLog) _3ds_drawText(combatLog.slice(0, 65), 10, 225, 0.38, 0xFFFFDD44);
  }
  _3ds_beginBottom();
  _3ds_clear(0xFF16213E);
  _3ds_drawText('Wave: ' + (state.wave || 0) + (presentation.doubleBattle ? ' - Doble batalla' : ''), 10, 10, 0.55, 0xFF00FFFF);
  if (presentation.capturePartyChoicePending) {
    _3ds_drawText("Caught: " + (presentation.capturedName || ""), 10, 34, 0.5, GREEN);
    const details = presentation.playerPartyDetails || [];
    for (let i = 0; i < details.length; ++i) {
      const actor = details[i];
      _3ds_drawText((i === presentation.selectedCapturePartyChoice ? "> " : "  ") +
        (actor.name || "").slice(0, 18) + " HP:" + actor.hp + "/" + actor.maxHp,
        10,  60 + i * 21, 0.44, WHITE);
    }
    _3ds_drawText("Up/Down: member A: replace B: decline", 8, 199, 0.4, WHITE);
    return;
  }
  if (presentation.moveLearningPending || presentation.evolutionPending) {
    _3ds_drawText((presentation.progressionName || '').slice(0, 26), 10, 34, 0.55, WHITE);
    if (presentation.moveLearningPending) {
      _3ds_drawText('Learn: ' + _3ds_getMoveName(presentation.pendingLearnMoveId), 10, 57, 0.48, GREEN);
      const moves = state.playerMoves || [];
      for (let i = 0; i < 4; ++i)
        _3ds_drawText((i === state.selectedMove ? '> ' : '  ') + (i + 1) + ': ' + _3ds_getMoveName(moves[i] || 0),
          10, 85 + i * 24, 0.48, WHITE);
      _3ds_drawText('Up/Down: slot A: learn B: reject', 8, 199, 0.43, WHITE);
    } else {
      _3ds_drawText(presentation.evolutionPauseConfirmation ? 'Pause future evolutions?' : 'Evolution ready',
        10, 90, 0.6, GREEN);
      _3ds_drawText(presentation.evolutionPauseConfirmation ? 'A: pause  B: keep enabled' : 'A: evolve  B: cancel',
        10, 142, 0.48, WHITE);
    }
    return;
  }
  if (state.rewardPending) {
    if (recipientMenu) {
      const party = presentation.playerParty || [];
      const details = presentation.playerPartyDetails || [];
      for (let i = 0; i < party.length; ++i) {
        const actor = details[i] || {};
        _3ds_drawText((i === partyCursor ? '> ' : '  ') +
          (actor.name || 'Pokemon #' + party[i]).slice(0, 16) + ' HP:' + (actor.hp || 0) + '/' + (actor.maxHp || 0),
          10, 38 + i * 23, 0.44, WHITE);
      }
      if (presentation.rewardRecovery) {
        const actor = details[partyCursor] || {};
        const move = (actor.moves || [])[recoveryMove];
        _3ds_drawText(move ? _3ds_getMoveName(move.id) + ' PP:' + move.pp + '/' + move.maxPp : 'No move in this slot',
          8, 178, 0.42, WHITE);
        _3ds_drawText('Slot ' + (recoveryMove + 1) + ' Left/Right', 8, 194, 0.4, WHITE);
      }
      else _3ds_drawText('Choose the held item owner', 8, 188, 0.43, WHITE);
      _3ds_drawText('A: apply  B: return to rewards', 8, 207, 0.43, WHITE);
    } else {
      const choices = presentation.rewardChoices || [];
      for (let i = 0; i < choices.length; ++i)
        _3ds_drawText((i === presentation.selectedRewardChoice ? '> ' : '  ') + choices[i], 10, 42 + i * 32, 0.5, WHITE);
      _3ds_drawText('Up/Down: reward A: claim B: skip', 8, 185, 0.43, WHITE);
    }
    return;
  }
  if (partyMenu && !state.finished) {
    const party = presentation.playerParty || [];
    for (let i = 0; i < party.length; ++i)
      _3ds_drawText((i === partyCursor ? '> ' : '  ') + 'Pokemon #' + party[i], 10, 40 + i * 24, 0.5, WHITE);
    _3ds_drawText('A: switch  B: cancel', 10, 205, 0.45, WHITE);
    return;
  }
  if (!state.runStarted) {
    _3ds_drawText('Starter: ' + _3ds_getStarterName(), 10, 42, 0.55, GREEN);
    _3ds_drawText('Cost: ' + (presentation.starterCost || 0) + ' / 10   Candy: ' + (presentation.starterCandy || 0), 10, 70, 0.48, WHITE);
    _3ds_drawText('Form: ' + (presentation.starterFormName || 'Base'), 10, 90, 0.42, WHITE);
    if (starterCostMenu) {
      _3ds_drawText('Reduce cost: ' + (presentation.starterReductionPrice || 0) + ' candy', 10, 105, 0.5, WHITE);
      _3ds_drawText(presentation.starterCanReduce ? 'A: buy and save   B: cancel' :
        (presentation.starterReduction >= 2 ? 'Maximum reduction reached' : 'Purchase unavailable'), 10, 135, 0.45, presentation.starterCanReduce ? GREEN : RED);
    } else {
      _3ds_drawText('Left/Right: browse  A: add/remove', 10, 110, 0.45, WHITE);
      _3ds_drawText('Up/Down: unlocked form', 10, 125, 0.4, WHITE);
      _3ds_drawText('Start: begin   B: reduce cost', 10, 140, 0.5, GREEN);
    }
    _3ds_drawText('Team ' + (presentation.playerPartyCount || 1) + '/6  Cost: ' + (presentation.starterTeamCost || 0) + '/10', 10, 170, 0.45, WHITE);
    _3ds_drawText((presentation.starterTeamNames || '').slice(0, 55), 10, 192, 0.4, WHITE);
    _3ds_drawText('L:Save R:Load Y:Export X:Import', 8, 219, 0.43, WHITE);
    return;
  }
  const moves = state.playerMoves || [], pp = state.playerPP || [];
  for (let i = 0; i < 4; ++i) {
    _3ds_drawText((i === state.selectedMove ? '> ' : '  ') + (i + 1) + ': ' + _3ds_getMoveName(moves[i] || 0) + ' PP:' + (pp[i] || 0),
      10, 42 + i * 25, 0.5, i === state.selectedMove ? GREEN : WHITE);
  }
  const phaseText = state.finished
    ? state.playerWon
      ? state.experienceGranted ? state.rewardPending ? 'Reward pending - A: choose B: skip' : 'Victory transition needs remaining phases' : 'Victory - Start: collect EXP'
      : 'Defeat - Start: restart'
    : state.supported ? 'Start/A: execute selected move' : 'Pending rules block this battle';
  _3ds_drawText(phaseText, 10, 147, 0.43, state.playerWon || state.supported ? GREEN : RED);
  if (presentation.trainerTypeId) {
    _3ds_drawText((presentation.trainerName || 'Trainer') + ' (' + (presentation.trainerPartyCount || 0) + ' Pokemon)', 8, 166, 0.4, WHITE);
    _3ds_drawText('Party: ' + (presentation.trainerParty || []).join(' / '), 8, 183, 0.4, WHITE);
  } else _3ds_drawText('Up/Down: move  Left/Right: starter', 8, 185, 0.43, WHITE);
  _3ds_drawText('A:Confirm B:Catch/Skip Select:Party', 8, 203, 0.43, WHITE);
  _3ds_drawText('L:Save R:Load Y:Export X:Import', 8, 219, 0.43, WHITE);
};
`;

mkdirSync(outDir, {recursive: true});
const bundle = browserStubs + entry;
const output = resolve(outDir, 'bundle.js');
writeFileSync(output, bundle, 'utf8');
console.log(`[bundle_quickjs] Diagnostic bundle: ${output} (${Buffer.byteLength(bundle)} bytes)`);

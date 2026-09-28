import { BaseScreen } from './BaseScreen.js';
import { AppStates } from '../shell/AppShell.js';

export class WaveIntroScreen extends BaseScreen {
  async enter(payload = {}) {
    this.resolved = payload.resolvedGameData;
    if (!this.resolved?.presentation) {
      if (!this.topEl || !this.bottomEl) return; // legacy/headless lifecycle bridge; never used by the imported production flow
      throw new Error('Wave intro requires resolved production game data');
    }
    this._render();
  }

  _render() {
    if (!this.topEl || !this.bottomEl) return;
    const { presentation: context, encounter } = this.resolved;
    this.topEl.innerHTML = `<div class="screen-view wave-top-view"><div class="wave-badge-top">WAVE ${context.wave} / ${this.resolved.mode.getMaxWave() ?? '∞'}</div><h2>${context.biome.name}</h2><div>${context.enemyPokemon.name} · Lv. ${context.enemyPokemon.level}</div><div class="asset-state">${context.enemyPokemon.assetStatus}</div></div>`;
    this.bottomEl.innerHTML = `<div class="screen-view wave-bottom-view"><div class="wave-card"><div>Mode: ${context.modeId}</div><div>Biome: ${context.biome.id}</div><div>Node: ${context.mapNode.id} (${context.mapNode.type})</div><div>Encounter: ${encounter.encounterType}</div><button id="btn_start_wave_battle" class="btn-game-touch primary">CONTINUE TO BATTLE</button></div></div>`;
    const go = () => this.appShell.transitionTo(AppStates.BATTLE, { resolvedGameData: this.resolved });
    this.bottomEl.querySelector('#btn_start_wave_battle')?.addEventListener('click', go);
  }

  handleInput(input) {
    if (input.action === 'CONFIRM') { this.appShell.transitionTo(AppStates.BATTLE, { resolvedGameData: this.resolved }); return true; }
    return false;
  }
}

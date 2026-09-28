import { BaseScreen } from './BaseScreen.js';
import { AppStates } from '../shell/AppShell.js';

export class RunSetupScreen extends BaseScreen {
  constructor(appShell) {
    super(appShell);
    this.starters = [];
    this.selectedSpeciesId = null;
    this.locale = 'en';
  }

  async enter() {
    if (!this.appShell.firstRunFlow) {
      if (!this.topEl || !this.bottomEl) return; // headless lifecycle tests do not enter production starter selection
      throw new Error('Starter selection requires imported pinned production content');
    }
    this.locale = this.appShell.firstRunFlow.locale;
    this.starters = this.appShell.firstRunFlow.getStarters();
    this.selectedSpeciesId = this.starters[0]?.id || null;
    if (!this.selectedSpeciesId) throw new Error('Pinned content has no eligible upstream starters');
    this._render();
  }

  _name(species) {
    const entry = this.appShell.firstRunFlow.localeEntries.find(item => item.locale === this.locale && item.namespace === 'pokemon' && item.canonicalId === species.id);
    if (!entry || typeof entry.value !== 'string') throw new Error(`Missing ${this.locale} locale for starter ${species.id}`);
    return entry.value;
  }

  _render() {
    if (!this.topEl || !this.bottomEl) return;
    const species = this.starters.find(item => item.id === this.selectedSpeciesId);
    if (!species) throw new Error(`Selected canonical starter ${this.selectedSpeciesId} is missing`);
    const name = this._name(species);
    const modes = this.appShell.firstRunFlow.modes.list().filter(mode => mode.capabilities.classicRules === true);
    if (!modes.length) throw new Error('Pinned content contains no mode with upstream Classic rules');
    this.topEl.innerHTML = `<div class="screen-view setup-top-view"><h2>STARTER</h2><div class="setup-name">${name}</div><div class="setup-types">${species.types.join(' / ')}</div><div class="asset-state">${species.extensions?.assetReference?.verified ? 'Upstream metadata verified; physical 3DS conversion pending' : 'Upstream asset metadata pending verification'}</div></div>`;
    this.bottomEl.innerHTML = `<div class="screen-view setup-bottom-view"><div class="setup-card"><label for="select_starter_species">Choose a canonical PokéRogue starter</label><select id="select_starter_species" class="game-select">${this.starters.map(item => `<option value="${item.id}" ${item.id === species.id ? 'selected' : ''}>${this._name(item)} (#${item.speciesId})</option>`).join('')}</select><label for="select_game_mode">Game mode</label><select id="select_game_mode" class="game-select">${modes.map(mode => `<option value="${mode.id}">${mode.extensions?.localeValue || mode.id}</option>`).join('')}</select><button id="btn_setup_back" class="btn-game-touch secondary">BACK</button><button id="btn_setup_start" class="btn-game-touch primary">START RUN</button></div></div>`;
    this.bottomEl.querySelector('#select_starter_species')?.addEventListener('change', event => { this.selectedSpeciesId = event.target.value; this._render(); });
    this.bottomEl.querySelector('#btn_setup_back')?.addEventListener('click', () => this.appShell.transitionTo(AppStates.TITLE));
    this.bottomEl.querySelector('#btn_setup_start')?.addEventListener('click', () => this._start());
  }

  _start() {
    const modeId = this.bottomEl?.querySelector('#select_game_mode')?.value;
    this.appShell.startNewRun({ modeId, speciesId: this.selectedSpeciesId, seed: 1 });
  }

  handleInput(input) {
    if (input.action === 'CONFIRM') { this._start(); return true; }
    if (input.action === 'CANCEL') { this.appShell.transitionTo(AppStates.TITLE); return true; }
    return false;
  }
}

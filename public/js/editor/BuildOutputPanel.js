/**
 * BuildOutputPanel - Data model and coordinator for the production build & export diagnostics panel.
 */
export class BuildOutputPanel {
  constructor() {
    this.status = 'idle'; // 'idle' | 'building' | 'success' | 'failed'
    this.logs = [];
    this.warnings = [];
    this.errors = [];
    this.artifacts = {
      elfSize: 0,
      '3dsxSize': 0,
      romfsBytes: 0,
      assetCount: 0
    };
    this.currentReport = null;
    this.listeners = [];
  }

  startBuild() {
    this.status = 'building';
    this.logs = [];
    this.warnings = [];
    this.errors = [];
    this.currentReport = null;
    this.appendLog('[BUILD START] Initializing Citro2D export & hardware build pipeline...');
    this._emit();
  }

  appendLog(line) {
    this.logs.push(String(line));
    this._emit();
  }

  addWarning(warning) {
    this.warnings.push(String(warning));
    this.appendLog(`[WARNING] ${warning}`);
    this._emit();
  }

  addError(error) {
    this.errors.push(String(error));
    this.appendLog(`[ERROR] ${error}`);
    this._emit();
  }

  setArtifacts(info = {}) {
    this.artifacts = {
      elfSize: Math.round(info.elfSize || 0),
      '3dsxSize': Math.round(info['3dsxSize'] || info.size3dsx || 0),
      romfsBytes: Math.round(info.romfsBytes || 0),
      assetCount: Math.round(info.assetCount || 0)
    };
    this._emit();
  }

  setSuccess(report = null) {
    this.status = 'success';
    this.currentReport = report;
    this.appendLog('[BUILD SUCCESS] Hardware build and RomFS generation completed successfully.');
    this._emit();
  }

  setFailed(errorMessage) {
    this.status = 'failed';
    this.addError(errorMessage);
    this.appendLog('[BUILD FAILED] Pipeline aborted with fatal errors.');
    this._emit();
  }

  clear() {
    this.status = 'idle';
    this.logs = [];
    this.warnings = [];
    this.errors = [];
    this.currentReport = null;
    this._emit();
  }

  subscribe(listener) {
    this.listeners.push(listener);
    return () => {
      this.listeners = this.listeners.filter(l => l !== listener);
    };
  }

  _emit() {
    for (const l of this.listeners) {
      try { l(this); } catch (err) { console.error('BuildOutputPanel listener error:', err); }
    }
  }
}

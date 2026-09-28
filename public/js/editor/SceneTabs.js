/**
 * SceneTabs - Multi-scene editor tabs management with dirty indicators.
 */
export class SceneTabs {
  constructor(initialSceneId = null, initialName = null) {
    this._tabs = new Map();
    this._activeTabId = initialSceneId;
    this.listeners = [];

    if (initialSceneId) {
      this.openTab(initialSceneId, initialName);
    }
  }

  openTab(sceneId, name = null) {
    if (!sceneId) return;
    if (!this._tabs.has(sceneId)) {
      this._tabs.set(sceneId, {
        id: sceneId,
        name: name || sceneId,
        dirty: false
      });
    }
    this._activeTabId = sceneId;
    this._emit();
  }

  closeTab(sceneId) {
    if (!this._tabs.has(sceneId)) return false;
    this._tabs.delete(sceneId);

    if (this._activeTabId === sceneId) {
      const remaining = Array.from(this._tabs.keys());
      this._activeTabId = remaining.length > 0 ? remaining[remaining.length - 1] : null;
    }
    this._emit();
    return true;
  }

  setActiveTab(sceneId) {
    if (this._tabs.has(sceneId)) {
      this._activeTabId = sceneId;
      this._emit();
    }
  }

  getActiveTab() {
    return this._activeTabId ? (this._tabs.get(this._activeTabId) || null) : null;
  }

  getActiveTabId() {
    return this._activeTabId;
  }

  getTabs() {
    return Array.from(this._tabs.values());
  }

  getOpenTabs() {
    return this.getTabs();
  }

  markTabDirty(sceneId, isDirty = true) {
    const tab = this._tabs.get(sceneId);
    if (tab) {
      tab.dirty = Boolean(isDirty);
      this._emit();
    }
  }

  setTabDirty(sceneId, isDirty = true) {
    this.markTabDirty(sceneId, isDirty);
  }

  isTabDirty(sceneId) {
    return Boolean(this._tabs.get(sceneId)?.dirty);
  }

  subscribe(listener) {
    this.listeners.push(listener);
    return () => {
      this.listeners = this.listeners.filter(l => l !== listener);
    };
  }

  _emit() {
    const active = this.getActiveTab();
    const tabs = this.getTabs();
    for (const l of this.listeners) {
      try { l(active, tabs); } catch (err) { console.error('SceneTabs listener error:', err); }
    }
  }
}

import { SceneModel } from './SceneModel.js';
import { SceneLibrary } from './SceneLibrary.js';
import { globalRNG } from './DeterministicRNG.js';

/**
 * RecentProjectsManager - Editor-only local registry of recently opened projects.
 */
export class RecentProjectsManager {
  static _recentList = [];

  static getRecentProjects() {
    return [...this._recentList];
  }

  static addRecentProject(nameOrPath) {
    if (!nameOrPath || typeof nameOrPath !== 'string') return;
    const entry = nameOrPath.trim();
    this._recentList = this._recentList.filter(p => p !== entry);
    this._recentList.unshift(entry);
    if (this._recentList.length > 10) {
      this._recentList.pop();
    }
  }

  static clearRecentProjects() {
    this._recentList = [];
  }
}

/**
 * ProjectDocument - Central production authoring project model for Nintendo 3DS.
 * Consolidates scenes, compositions, authoring templates, asset manifests, and export settings.
 */
export class ProjectDocument {
  constructor(data = {}) {
    this.schemaVersion = Math.round(data.schemaVersion || 1);
    this.name = data.name || 'UntitledProject';
    this.settings = {
      defaultScreen: data.settings?.defaultScreen || 'top',
      targetFps: Math.round(data.settings?.targetFps || 60),
      exportProfile: ['debug', 'development', 'release'].includes(data.settings?.exportProfile?.toLowerCase())
        ? data.settings.exportProfile.toLowerCase()
        : 'development',
      snapToPixel: data.settings?.snapToPixel !== false
    };

    // Scenes Map
    this._scenes = new Map();
    const rawScenes = Array.isArray(data.scenes) ? data.scenes : [];
    for (const s of rawScenes) {
      const sceneModel = s instanceof SceneModel ? s : new SceneModel(s);
      this._scenes.set(sceneModel.id, sceneModel);
      SceneLibrary.registerScene(sceneModel);
    }

    // Default Scene if empty
    if (this._scenes.size === 0) {
      const defaultScene = new SceneModel({ id: 'MainScene', name: 'Main Scene', durationFrames: 60 });
      this._scenes.set(defaultScene.id, defaultScene);
      SceneLibrary.registerScene(defaultScene);
    }

    // Active Scene ID
    this.activeSceneId = data.activeSceneId && this._scenes.has(data.activeSceneId)
      ? data.activeSceneId
      : Array.from(this._scenes.keys())[0];

    // Templates and Assets list
    this.templates = Array.isArray(data.templates) ? [...data.templates] : [];
    this.assets = Array.isArray(data.assets) ? [...data.assets] : [];

    // Deterministic dirty state tracking (no Date.now())
    this._isDirty = false;
  }

  // --- Dirty State Tracking (BETA-UI-8) ---
  isDirty() {
    return this._isDirty;
  }

  markDirty() {
    this._isDirty = true;
  }

  markClean() {
    this._isDirty = false;
  }

  // --- Scene Operations ---
  getActiveScene() {
    return this._scenes.get(this.activeSceneId) || null;
  }

  setActiveScene(sceneId) {
    if (!this._scenes.has(sceneId)) {
      throw new Error(`Scene "${sceneId}" not found in project`);
    }
    this.activeSceneId = sceneId;
  }

  addScene(scene) {
    const model = scene instanceof SceneModel ? scene : new SceneModel(scene);
    if (this._scenes.has(model.id)) {
      throw new Error(`Scene with ID "${model.id}" already exists in project`);
    }
    this._scenes.set(model.id, model);
    SceneLibrary.registerScene(model);
    this.markDirty();
    return model;
  }

  removeScene(sceneId) {
    if (!this._scenes.has(sceneId)) return false;
    if (this._scenes.size <= 1) {
      throw new Error('Cannot remove the only remaining scene in project');
    }
    this._scenes.delete(sceneId);
    SceneLibrary.removeScene(sceneId);
    if (this.activeSceneId === sceneId) {
      this.activeSceneId = Array.from(this._scenes.keys())[0];
    }
    this.markDirty();
    return true;
  }

  getScene(sceneId) {
    return this._scenes.get(sceneId) || null;
  }

  getScenes() {
    return Array.from(this._scenes.values());
  }

  // --- Project Lifecycle ---
  static newProject(name = 'NewProject', settings = {}) {
    const proj = new ProjectDocument({ name, settings });
    RecentProjectsManager.addRecentProject(proj.name);
    return proj;
  }

  saveProject() {
    this.markClean();
    return this.toJSON();
  }

  saveProjectAs(newName) {
    if (newName && typeof newName === 'string') {
      this.name = newName.trim();
    }
    RecentProjectsManager.addRecentProject(this.name);
    return this.saveProject();
  }

  static loadProject(jsonOrData) {
    if (!jsonOrData) {
      throw new Error('Project load failed: input data is null or undefined');
    }

    let parsed = jsonOrData;
    if (typeof jsonOrData === 'string') {
      try {
        parsed = JSON.parse(jsonOrData);
      } catch (err) {
        throw new Error(`Project load failed: Corrupted JSON - ${err.message}`);
      }
    }

    if (typeof parsed !== 'object' || parsed === null) {
      throw new Error('Project load failed: root project structure must be an object');
    }

    if (parsed.schemaVersion !== undefined && typeof parsed.schemaVersion !== 'number') {
      throw new Error('Project load failed: schemaVersion must be an integer');
    }

    const doc = new ProjectDocument(parsed);
    RecentProjectsManager.addRecentProject(doc.name);
    return doc;
  }

  toJSON() {
    return {
      schemaVersion: this.schemaVersion,
      name: this.name,
      activeSceneId: this.activeSceneId,
      settings: { ...this.settings },
      templates: [...this.templates],
      assets: [...this.assets],
      scenes: Array.from(this._scenes.values()).map(s => (typeof s.toJSON === 'function' ? s.toJSON() : s))
    };
  }
}

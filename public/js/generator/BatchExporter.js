import { SceneCppExporter } from './SceneCppExporter.js';
import { SceneLibrary } from '../core/SceneLibrary.js';
import { ProjectValidator } from './ProjectValidator.js';

/**
 * BatchExporter - Deterministic multi-scene export pipeline without cross-contamination.
 */
export class BatchExporter {
  /**
   * Resolves list of scenes from Project, array, or SceneLibrary.
   */
  static _resolveScenesInput(input) {
    if (input && typeof input.getScenes === 'function') {
      return input.getScenes();
    }
    if (input && Array.isArray(input.scenes)) {
      return input.scenes;
    }
    if (Array.isArray(input)) {
      return input;
    }
    return [];
  }

  /**
   * Exports multiple scenes in a single atomic pipeline.
   * @param {Array<SceneModel|Object|string>|ProjectDocument} scenesOrProject 
   * @param {Object} [options]
   */
  static exportScenes(scenesOrProject, options = {}) {
    const rawList = this._resolveScenesInput(scenesOrProject);
    const resultsMap = new Map();
    const errors = [];

    if (rawList.length === 0) {
      const emptyArr = [];
      emptyArr.success = false;
      emptyArr.results = resultsMap;
      emptyArr.exports = [];
      emptyArr.errors = [{ message: 'No scenes provided for batch export' }];
      emptyArr.totalScenes = 0;
      return emptyArr;
    }

    // Resolve each scene instance cleanly
    const resolvedScenes = [];
    for (const item of rawList) {
      let scene = null;
      if (typeof item === 'string') {
        scene = SceneLibrary.getScene(item);
      } else if (item && typeof item === 'object') {
        scene = item;
      }
      if (!scene || !scene.id) {
        errors.push({ message: `Could not resolve scene for item "${item}"` });
        continue;
      }
      resolvedScenes.push(scene);
    }

    if (errors.length > 0) {
      const errArr = [];
      errArr.success = false;
      errArr.results = resultsMap;
      errArr.exports = [];
      errArr.errors = errors;
      errArr.totalScenes = resolvedScenes.length;
      return errArr;
    }

    const exportsList = [];

    // Export each scene independently to prevent cross-contamination
    for (const scene of resolvedScenes) {
      try {
        const sceneJson = typeof scene.toJSON === 'function' ? scene.toJSON() : scene;
        const exported = SceneCppExporter.export(sceneJson, options);
        const entry = {
          sceneId: scene.id,
          sceneName: scene.name || scene.id,
          exported,
          cppExport: exported,
          success: true
        };
        resultsMap.set(scene.id, entry);
        exportsList.push(entry);
      } catch (err) {
        errors.push({ sceneId: scene.id, message: err.message });
        const entry = {
          sceneId: scene.id,
          success: false,
          error: err.message
        };
        resultsMap.set(scene.id, entry);
        exportsList.push(entry);
      }
    }

    exportsList.success = errors.length === 0;
    exportsList.results = resultsMap;
    exportsList.exports = exportsList;
    exportsList.errors = errors;
    exportsList.totalScenes = resolvedScenes.length;

    return exportsList;
  }

  /**
   * Validates and executes batch build across multiple scenes.
   * @param {Array<SceneModel|Object|string>|ProjectDocument} scenesOrProject 
   * @param {Object} [options]
   */
  static buildAll(scenesOrProject, options = {}) {
    const exportResult = this.exportScenes(scenesOrProject, options);
    if (!exportResult.success) {
      const errMsgs = exportResult.errors.map(e => e.message).join('; ');
      throw new Error(`BATCH BUILD FAILED: ${errMsgs}`);
    }
    return exportResult;
  }
}

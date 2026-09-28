import { SceneValidator } from './SceneValidator.js';
import { VALID_BLEND_MODES } from '../core/EffectModel.js';

/**
 * ProjectValidator - Holistic validator for production projects, scenes, compositions, assets, and audio.
 * Categorizes diagnostics into Errors, Warnings, and Info with precise resource coordinates.
 */
export class ProjectValidator {
  /**
   * Validates an entire project document and its referenced scenes.
   * @param {ProjectDocument|Object} project 
   * @param {Object} [options]
   * @param {Array<string>} [options.indexedAssetIds]
   * @returns {{ valid: boolean, errors: Array<Object>, warnings: Array<Object>, info: Array<Object> }}
   */
  static validateProject(project, options = {}) {
    const errors = [];
    const warnings = [];
    const info = [];

    if (!project) {
      errors.push({ type: 'error', message: 'Project document is null or undefined' });
      return { valid: false, errors, warnings, info };
    }

    // 1. Validate Project Settings
    if (!project.name) {
      warnings.push({ type: 'warning', message: 'Project name is not defined; defaulting to UntitledProject' });
    }

    const targetFps = project.settings?.targetFps ?? 60;
    if (targetFps !== 60 && targetFps !== 30) {
      warnings.push({ type: 'warning', message: `Target FPS ${targetFps} is non-standard for Nintendo 3DS (recommended 60 or 30)` });
    }

    // 2. Validate Scenes
    const scenes = typeof project.getScenes === 'function'
      ? project.getScenes()
      : (Array.isArray(project.scenes) ? project.scenes : []);

    if (scenes.length === 0) {
      errors.push({ type: 'error', message: 'Project contains no scenes. At least one scene is required.' });
    }

    const sceneMap = new Map();
    for (const scene of scenes) {
      if (!scene || !scene.id) {
        errors.push({ type: 'error', message: 'Scene missing required ID' });
        continue;
      }
      if (sceneMap.has(scene.id)) {
        errors.push({ type: 'error', sceneId: scene.id, message: `Duplicate scene ID "${scene.id}" detected in project` });
      }
      sceneMap.set(scene.id, scene);
    }

    // Validate active scene reference
    if (project.activeSceneId && !sceneMap.has(project.activeSceneId)) {
      errors.push({
        type: 'error',
        message: `Active scene "${project.activeSceneId}" does not exist in project scenes list`
      });
    }

    // 3. Validate Each Scene in Depth
    const availableAssetIds = new Set();
    const hasAssetPool = Array.isArray(options.indexedAssetIds) || Array.isArray(project.assets);
    if (Array.isArray(options.indexedAssetIds)) {
      for (const a of options.indexedAssetIds) availableAssetIds.add(typeof a === 'string' ? a : (a.id || a.assetId));
    } else if (Array.isArray(project.assets)) {
      for (const a of project.assets) availableAssetIds.add(typeof a === 'string' ? a : (a.id || a.assetId));
    }

    for (const [sceneId, scene] of sceneMap.entries()) {
      // Scene basic bounds & dimensions
      if (scene.durationFrames <= 0) {
        errors.push({ type: 'error', sceneId, scene: sceneId, message: `Scene "${sceneId}" durationFrames must be >= 1` });
      }
      if (scene.durationFrames > 65535) {
        errors.push({ type: 'error', sceneId, scene: sceneId, message: `Scene "${sceneId}" durationFrames (${scene.durationFrames}) exceeds uint16 limit` });
      }

      // Validate cycle references
      try {
        SceneValidator.assertNoCompositionCycles(sceneId, (id) => sceneMap.get(id));
      } catch (cycleErr) {
        errors.push({ type: 'error', sceneId, scene: sceneId, message: cycleErr.message });
      }

      // Validate Nodes
      const nodes = scene.nodes || scene.components || [];
      const nodeIds = new Set();

      for (const node of nodes) {
        if (!node.id) {
          errors.push({ type: 'error', sceneId, scene: sceneId, message: 'Node is missing required ID' });
          continue;
        }

        if (nodeIds.has(node.id)) {
          errors.push({ type: 'error', sceneId, scene: sceneId, nodeId: node.id, node: node.id, message: `Duplicate node ID "${node.id}" within scene "${sceneId}"` });
        }
        nodeIds.add(node.id);

        // Integer dimensions contract & overflow
        const w = Math.round(node.width ?? node.transform?.width ?? 0);
        const h = Math.round(node.height ?? node.transform?.height ?? 0);
        if (w <= 0 || h <= 0) {
          errors.push({ type: 'error', sceneId, scene: sceneId, nodeId: node.id, node: node.id, message: `Node "${node.id}" dimensions must be positive integers (> 0)` });
        }
        if (w > 65535 || h > 65535) {
          errors.push({ type: 'error', sceneId, scene: sceneId, nodeId: node.id, node: node.id, message: `Node "${node.id}" dimensions exceed uint16 limit (65535)` });
        }

        // Parent reference validation
        if (node.parent && !nodes.some(n => n.id === node.parent)) {
          errors.push({ type: 'error', sceneId, scene: sceneId, nodeId: node.id, node: node.id, message: `Node "${node.id}" references non-existent parent "${node.parent}"` });
        }

        // Composition node child scene reference
        if (node.type === 'Composition' || node.type === 'CompositionNode') {
          const targetSceneId = node.sceneId || node.properties?.sceneId || node.properties?.compositionId;
          const localComps = scene.compositions || [];
          const existsInLocal = localComps.some(c => c.id === targetSceneId);
          if (!targetSceneId) {
            errors.push({ type: 'error', sceneId, scene: sceneId, nodeId: node.id, node: node.id, message: `Composition node "${node.id}" is missing sceneId` });
          } else if (!sceneMap.has(targetSceneId) && !existsInLocal) {
            errors.push({ type: 'error', sceneId, scene: sceneId, nodeId: node.id, node: node.id, message: `Composition node "${node.id}" references non-existent scene or composition "${targetSceneId}"` });
          }
        }

        // Asset existence check (if asset pool provided)
        const assetRef = node.asset || node.properties?.asset || node.properties?.assetId;
        if (assetRef && hasAssetPool && !availableAssetIds.has(assetRef)) {
          errors.push({
            type: 'error',
            code: 'MISSING_ASSET',
            sceneId,
            scene: sceneId,
            nodeId: node.id,
            node: node.id,
            assetId: assetRef,
            message: `Node "${node.id}" references missing asset "${assetRef}"`
          });
        }

        // Effects validation (BETA-UI-8)
        const effects = node.effects ? (typeof node.effects.getAll === 'function' ? node.effects.getAll() : node.effects) : [];
        for (const eff of effects) {
          if (eff.type === 'ColorOverlay' && eff.parameters?.blendMode) {
            if (!VALID_BLEND_MODES.includes(eff.parameters.blendMode.toLowerCase())) {
              errors.push({
                type: 'error',
                sceneId,
                scene: sceneId,
                nodeId: node.id,
                node: node.id,
                message: `Effect "${eff.id}" uses unsupported blend mode "${eff.parameters.blendMode}"`
              });
            }
          }
        }
      }

      // Audio Cues Validation
      const cues = scene.audioCues || [];
      for (const cue of cues) {
        const cueAsset = cue.asset || cue.assetId;
        if (!cueAsset) {
          errors.push({ type: 'error', sceneId, scene: sceneId, message: `Audio cue at frame ${cue.frame} is missing audio asset` });
        } else if (hasAssetPool && !availableAssetIds.has(cueAsset)) {
          errors.push({ type: 'error', sceneId, scene: sceneId, assetId: cueAsset, message: `Audio cue references missing audio asset "${cueAsset}"` });
        }
        if (cue.volume < 0 || cue.volume > 1) {
          warnings.push({ type: 'warning', sceneId, scene: sceneId, message: `Audio cue "${cueAsset}" volume ${cue.volume} should be clamped between 0 and 1` });
        }
      }

      // Tracks Validation
      const tracks = scene.tracks || [];
      for (const track of tracks) {
        if (!track.targetNodeId || !nodeIds.has(track.targetNodeId)) {
          errors.push({
            type: 'error',
            sceneId,
            trackId: track.id,
            nodeId: track.targetNodeId,
            message: `Track targets non-existent node "${track.targetNodeId}" in scene "${sceneId}"`
          });
        }
      }
    }

    info.push({
      type: 'info',
      message: `Project validation analyzed ${scenes.length} scene(s) with ${errors.length} error(s) and ${warnings.length} warning(s)`
    });

    return {
      valid: errors.length === 0,
      errors,
      warnings,
      info
    };
  }

  static validate(project, options = {}) {
    return this.validateProject(project, options);
  }

  /**
   * Asserts that project is clean for export; blocks pipeline with descriptive diagnostics if errors exist.
   * @param {ProjectDocument|Object} project 
   * @param {Object} [options]
   */
  static assertCanExport(project, options = {}) {
    const report = this.validateProject(project, options);
    if (!report.valid) {
      const errMsgs = report.errors.map(e => `[${e.sceneId || 'Project'}] ${e.message}`).join('; ');
      throw new Error(`EXPORT BLOCKED: Project validation failed with ${report.errors.length} error(s): ${errMsgs}`);
    }
    return report;
  }
}

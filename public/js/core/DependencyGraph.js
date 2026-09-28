import { SceneLibrary } from './SceneLibrary.js';

/**
 * DependencyGraph - Analyzes relationships between Projects, Scenes, Compositions, and Assets.
 * Provides reference lookup, usage tracking, and safe deletion detection.
 */
export class DependencyGraph {
  /**
   * Resolves list of scenes from Project, array, or SceneLibrary.
   */
  static _resolveScenes(source) {
    if (source && typeof source.getScenes === 'function') {
      return source.getScenes();
    }
    if (Array.isArray(source)) {
      return source;
    }
    if (source && Array.isArray(source.scenes)) {
      return source.scenes;
    }
    return SceneLibrary.getAllScenes();
  }

  /**
   * Finds all references to an asset (texture, audio, sprite) across scenes.
   * Can be called as (assetId, source) or (source, assetId).
   */
  static findAssetReferences(arg1, arg2 = null) {
    let assetId = typeof arg1 === 'string' ? arg1 : (typeof arg2 === 'string' ? arg2 : null);
    let source = typeof arg1 === 'object' && arg1 !== null ? arg1 : (typeof arg2 === 'object' && arg2 !== null ? arg2 : null);

    if (!assetId) return [];
    const scenes = this._resolveScenes(source);
    const references = [];

    for (const scene of scenes) {
      const nodes = scene.nodes || scene.components || [];
      for (const node of nodes) {
        // Image / Sprite asset
        const nodeAsset = node.asset || node.properties?.asset || node.properties?.assetId;
        if (nodeAsset === assetId) {
          references.push({
            sceneId: scene.id,
            sceneName: scene.name || scene.id,
            nodeId: node.id,
            nodeName: node.name || node.id,
            usage: node.type === 'PokemonSprite' ? 'sprite' : 'image'
          });
        }
      }

      // Audio cues
      const cues = scene.audioCues || [];
      for (const cue of cues) {
        const cueAsset = cue.asset || cue.assetId;
        if (cueAsset === assetId) {
          references.push({
            sceneId: scene.id,
            sceneName: scene.name || scene.id,
            nodeId: cue.id,
            nodeName: `AudioCue: ${cueAsset}`,
            usage: 'audio'
          });
        }
      }
    }

    return references;
  }

  /**
   * Finds all references to a scene by nested composition nodes.
   * Can be called as (targetSceneId, source) or (source, targetSceneId).
   */
  static findSceneReferences(arg1, arg2 = null) {
    let targetSceneId = typeof arg1 === 'string' ? arg1 : (typeof arg2 === 'string' ? arg2 : null);
    let source = typeof arg1 === 'object' && arg1 !== null ? arg1 : (typeof arg2 === 'object' && arg2 !== null ? arg2 : null);

    if (!targetSceneId) return [];
    const scenes = this._resolveScenes(source);
    const references = [];

    for (const scene of scenes) {
      const nodes = scene.nodes || scene.components || [];
      for (const node of nodes) {
        const isComp = node.type === 'Composition' || node.type === 'CompositionNode';
        const childSceneId = node.sceneId || node.properties?.sceneId || node.properties?.compositionId;
        if (isComp && childSceneId === targetSceneId) {
          references.push({
            sceneId: scene.id,
            sceneName: scene.name || scene.id,
            nodeId: node.id,
            nodeName: node.name || node.id,
            type: 'Composition'
          });
        }
      }
    }

    return references;
  }

  /**
   * Builds the complete dependency tree for a project or scene set.
   * @param {Object} [source] 
   */
  static buildGraph(source = null) {
    const scenes = this._resolveScenes(source);
    const nodes = [];
    const edges = [];

    for (const scene of scenes) {
      nodes.push({ id: `scene:${scene.id}`, type: 'scene', label: scene.name || scene.id });

      const sceneNodes = scene.nodes || scene.components || [];
      for (const n of sceneNodes) {
        if (n.type === 'Composition' || n.type === 'CompositionNode') {
          const targetId = n.sceneId || n.properties?.sceneId || n.properties?.compositionId;
          if (targetId) {
            edges.push({
              from: `scene:${scene.id}`,
              to: `scene:${targetId}`,
              relationship: 'composes'
            });
          }
        }

        const asset = n.asset || n.properties?.asset || n.properties?.assetId;
        if (asset) {
          nodes.push({ id: `asset:${asset}`, type: 'asset', label: asset });
          edges.push({
            from: `scene:${scene.id}`,
            to: `asset:${asset}`,
            relationship: 'uses_asset'
          });
        }
      }

      for (const cue of (scene.audioCues || [])) {
        const cueAsset = cue.asset || cue.assetId;
        if (cueAsset) {
          nodes.push({ id: `asset:${cueAsset}`, type: 'asset', label: cueAsset });
          edges.push({
            from: `scene:${scene.id}`,
            to: `asset:${cueAsset}`,
            relationship: 'plays_audio'
          });
        }
      }
    }

    // Deduplicate nodes
    const uniqueNodes = Array.from(new Map(nodes.map(n => [n.id, n])).values());
    return { nodes: uniqueNodes, edges };
  }

  /**
   * Evaluates if a resource can be safely deleted or if active references exist.
   * Can be called as (type, id, source) or (source, type, id).
   */
  static safeDelete(arg1, arg2, arg3 = null) {
    let source = null;
    let type = null;
    let id = null;

    if (typeof arg1 === 'object' && arg1 !== null) {
      source = arg1;
      type = arg2;
      id = arg3;
    } else {
      type = arg1;
      id = arg2;
      source = arg3;
    }

    if (type === 'asset') {
      const refs = this.findAssetReferences(id, source);
      return {
        safe: refs.length === 0,
        canDelete: refs.length === 0,
        references: refs,
        message: refs.length === 0
          ? `Asset "${id}" has no references and can be safely deleted.`
          : `Asset "${id}" is referenced in ${refs.length} place(s). Deletion will cause missing asset errors.`
      };
    }

    if (type === 'scene' || type === 'composition') {
      const refs = this.findSceneReferences(id, source);
      return {
        safe: refs.length === 0,
        canDelete: refs.length === 0,
        references: refs,
        message: refs.length === 0
          ? `Scene "${id}" is not referenced by any compositions and can be safely deleted.`
          : `Scene "${id}" is composed in ${refs.length} node(s). Deletion will break nested compositions.`
      };
    }

    return { safe: true, canDelete: true, references: [], message: 'No reference checks required.' };
  }
}

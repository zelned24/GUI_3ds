import { SceneLibrary } from '../core/SceneLibrary.js';

/**
 * GlobalSearch - Fast indexing search across all scenes, nodes, assets, clips, compositions, and markers.
 * Bound to Ctrl/Cmd + P in the editor.
 */
export class GlobalSearch {
  /**
   * Performs universal search across all project resources.
   * Accepts (query, context) or (context, query).
   */
  static search(arg1, arg2 = {}) {
    let query = typeof arg1 === 'string' ? arg1 : (typeof arg2 === 'string' ? arg2 : '');
    let context = typeof arg1 === 'object' && arg1 !== null ? arg1 : (typeof arg2 === 'object' && arg2 !== null ? arg2 : {});

    if (!query || typeof query !== 'string') return [];
    const q = query.trim().toLowerCase();
    if (q.length === 0) return [];

    const results = [];

    // Resolve scenes
    let scenes = [];
    if (context.scenes && Array.isArray(context.scenes)) {
      scenes = context.scenes;
    } else if (typeof context.getScenes === 'function') {
      scenes = context.getScenes();
    } else if (context.project && typeof context.project.getScenes === 'function') {
      scenes = context.project.getScenes();
    } else {
      scenes = SceneLibrary.getAllScenes();
    }

    // 1. Search Scenes
    for (const scene of scenes) {
      const matchId = scene.id?.toLowerCase().includes(q);
      const matchName = scene.name?.toLowerCase().includes(q);
      if (matchId || matchName) {
        results.push({
          type: 'scene',
          id: scene.id,
          name: scene.name || scene.id,
          location: `Scene: ${scene.name || scene.id}`,
          data: scene
        });
      }

      // 2. Search Nodes & Compositions in Scene
      const nodes = scene.nodes || scene.components || [];
      for (const node of nodes) {
        const nMatchId = node.id?.toLowerCase().includes(q);
        const nMatchName = node.name?.toLowerCase().includes(q);
        const nMatchType = node.type?.toLowerCase().includes(q);
        const nMatchText = node.properties?.text?.toLowerCase().includes(q);
        const isComp = node.type === 'Composition' || node.type === 'CompositionNode';

        if (nMatchId || nMatchName || nMatchType || nMatchText) {
          results.push({
            type: isComp ? 'composition' : 'node',
            id: node.id,
            name: node.name || node.id,
            location: `${scene.name || scene.id} > ${node.name || node.id} (${node.type})`,
            data: { sceneId: scene.id, node }
          });
        }
      }

      // 3. Search Clips
      const clips = scene.clips || [];
      for (const clip of clips) {
        if (clip.id?.toLowerCase().includes(q) || clip.name?.toLowerCase().includes(q)) {
          results.push({
            type: 'clip',
            id: clip.id,
            name: clip.name || clip.id,
            location: `${scene.name || scene.id} > Clip: ${clip.name || clip.id}`,
            data: { sceneId: scene.id, clip }
          });
        }
      }

      // 4. Search Markers
      const markers = scene.markers || [];
      for (const marker of markers) {
        const mLabel = marker.label || marker.name || '';
        const mId = marker.id || '';
        if (mLabel.toLowerCase().includes(q) || mId.toLowerCase().includes(q) || marker.type?.toLowerCase().includes(q)) {
          results.push({
            type: 'marker',
            id: marker.id,
            name: mLabel || marker.id,
            location: `${scene.name || scene.id} > Marker @ Frame ${marker.frame}`,
            data: { sceneId: scene.id, marker }
          });
        }
      }
    }

    // 5. Search Assets
    const assets = context.assets && Array.isArray(context.assets)
      ? context.assets
      : (context.project && Array.isArray(context.project.assets) ? context.project.assets : []);

    for (const asset of assets) {
      const assetId = typeof asset === 'string' ? asset : (asset.id || asset.assetId || '');
      const assetName = typeof asset === 'string' ? asset : (asset.name || asset.id || '');
      if (assetId.toLowerCase().includes(q) || assetName.toLowerCase().includes(q)) {
        results.push({
          type: 'asset',
          id: assetId,
          name: assetName,
          location: `Asset Library: ${assetId}`,
          data: asset
        });
      }
    }

    // Deterministic sort: type then name
    return results.sort((a, b) => {
      const typeCmp = a.type.localeCompare(b.type);
      if (typeCmp !== 0) return typeCmp;
      return (a.name || a.id).localeCompare(b.name || b.id);
    });
  }
}

/**
 * ExportReport - Generates structured and human-readable reports of production builds.
 */
export class ExportReport {
  /**
   * Generates a comprehensive report for an exported scene and build artifact.
   * @param {SceneModel|Object} scene 
   * @param {Object} [buildMetadata] 
   * @returns {Object}
   */
  static generateReport(scene, buildMetadata = {}) {
    const rawNodes = scene.nodes || scene.components || [];
    const tracks = scene.tracks || [];
    const clips = scene.clips || [];
    const audio = scene.audioCues || [];
    const markers = scene.markers || [];

    // Distinct referenced assets
    const assetSet = new Set();
    for (const n of rawNodes) {
      const a = n.asset || n.properties?.asset;
      if (a) assetSet.add(a);
    }
    for (const c of audio) {
      if (c.asset) assetSet.add(c.asset);
    }

    return {
      scene: scene.name || scene.id || 'Scene',
      sceneId: scene.id,
      duration: scene.durationFrames || 60,
      fps: scene.fps || 60,
      nodeCount: rawNodes.length,
      trackCount: tracks.length,
      clipCount: clips.length,
      markerCount: markers.length,
      audioCueCount: audio.length,
      assetCount: assetSet.size,
      referencedAssets: Array.from(assetSet).sort(),
      romfsBytes: Math.round(buildMetadata.romfsBytes || 0),
      elfSize: Math.round(buildMetadata.elfSize || 0),
      '3dsxSize': Math.round(buildMetadata['3dsxSize'] || buildMetadata.size3dsx || 0),
      profile: buildMetadata.profile || 'development',
      warnings: [...(buildMetadata.warnings || [])],
      errors: [...(buildMetadata.errors || [])]
    };
  }

  /**
   * Formats report into a clean, deterministic summary string.
   * @param {Object} report 
   * @returns {string}
   */
  static formatReportText(report) {
    return [
      '====================================================',
      `  3DS PRODUCTION EXPORT REPORT: ${report.scene}`,
      '====================================================',
      `Scene ID:         ${report.sceneId}`,
      `Duration:         ${report.duration} frames @ ${report.fps} FPS`,
      `Nodes:            ${report.nodeCount}`,
      `Tracks:           ${report.trackCount}`,
      `Clips:            ${report.clipCount}`,
      `Markers:          ${report.markerCount}`,
      `Audio Cues:       ${report.audioCueCount}`,
      `Referenced Assets:${report.assetCount}`,
      `Export Profile:   ${report.profile.toUpperCase()}`,
      `RomFS Payload:    ${report.romfsBytes} bytes`,
      `ELF Binary Size:  ${report.elfSize} bytes`,
      `3DSX Binary Size: ${report['3dsxSize']} bytes`,
      `Warnings:         ${report.warnings.length}`,
      `Errors:           ${report.errors.length}`,
      '===================================================='
    ].join('\n');
  }
}

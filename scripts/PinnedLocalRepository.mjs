import fs from 'node:fs/promises';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { PokerogueRepository } from '../tools/js/data/PokerogueRepository.js';
import { POKEROGUE_REPOSITORIES } from '../tools/js/data/PokerogueSource.js';

/** Reads the exact configured revisions from ignored local Git snapshots when available. */
export class PinnedLocalRepository extends PokerogueRepository {
  constructor(root) {
    super();
    this.root = root;
    this.verified = new Map();
  }

  async getFile(repoKey, filePath) {
    const source = POKEROGUE_REPOSITORIES[repoKey];
    if (!source) throw new Error(`Unknown pinned repository: ${repoKey}`);
    const normalized = String(filePath).replaceAll('\\', '/');
    if (normalized.startsWith('/') || normalized.split('/').some(part => !part || part === '.' || part === '..')) {
      throw new Error(`Invalid pinned source path: ${filePath}`);
    }
    const cacheKey = this.getCacheKey(repoKey, normalized);
    if (this.cache.has(cacheKey)) return this.cache.get(cacheKey);
    const repoDir = path.join(this.root, 'build', 'upstream', repoKey);
    if (!this.verified.has(repoKey)) {
      try {
        const actual = execFileSync('git', ['rev-parse', 'HEAD'], { cwd: repoDir, encoding: 'utf8' }).trim();
        if (actual !== source.revision) throw new Error(`Local ${repoKey} is at ${actual}, expected ${source.revision}`);
        this.verified.set(repoKey, repoDir);
      } catch (error) {
        if (error.code !== 'ENOENT' && error.code !== 'ENOTDIR' && !/not a git repository/.test(error.message)) throw error;
        this.verified.set(repoKey, null);
      }
    }
    const verifiedDir = this.verified.get(repoKey);
    if (!verifiedDir) return super.getFile(repoKey, normalized);
    let content;
    try {
      content = await fs.readFile(path.join(verifiedDir, ...normalized.split('/')), 'utf8');
    } catch (error) {
      if (error.code !== 'ENOENT') throw error;
      // Sparse asset checkouts retain the pinned tree even when a blob is not materialized.
      content = execFileSync('git', ['show', `HEAD:${normalized}`], {
        cwd: verifiedDir, encoding: 'utf8', maxBuffer: 32 * 1024 * 1024
      });
    }
    this.cache.set(cacheKey, content);
    return content;
  }
}

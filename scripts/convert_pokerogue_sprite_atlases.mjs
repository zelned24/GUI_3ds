import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { POKEROGUE_REPOSITORIES } from '../public/js/data/PokerogueSource.js';

// The default invocation only writes a deterministic conversion plan. Actual
// texture conversion is explicit so the source migration can be inspected
// before any 3DS build or asset compilation.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const stagedPath = path.join(root, 'build/upstream-assets/staged-sprite-assets.json');
const planPath = path.join(root, 'build/upstream-assets/atlas-conversion-plan.json');
const convert = process.argv.includes('--convert');
const tex3ds = process.env.TEX3DS || 'tex3ds';
const staged = JSON.parse(await fs.readFile(stagedPath, 'utf8'));
let previous = null;
try { previous = JSON.parse(await fs.readFile(planPath, 'utf8')); }
catch (error) { if (error.code !== 'ENOENT') throw error; }
const pinned = POKEROGUE_REPOSITORIES['pokerogue-assets'];
if (staged.repository !== pinned.url || staged.revision !== pinned.revision
    || !Array.isArray(staged.assets) || staged.staged !== staged.assets.length) {
  throw new Error('Staged sprite inventory does not match pinned assets revision');
}

const inside = relative => {
  if (typeof relative !== 'string' || path.isAbsolute(relative)
      || relative.split(/[\\/]/).some(part => part === '..' || part === '.')) {
    throw new Error(`Unsafe asset path: ${relative}`);
  }
  const absolute = path.resolve(root, relative);
  if (!absolute.startsWith(root + path.sep)) throw new Error(`Unsafe asset path: ${relative}`);
  return absolute;
};
const hash = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
const plan = { schemaVersion: 1, repository: pinned.url, revision: pinned.revision,
  textureFormat: 'rgba4', maximumTextureEdge: 1024, conversionTool: 'tex3ds',
  total: staged.assets.length, eligible: 0, converted: 0, unsupported: [], assets: [] };
const seen = new Set();
const previousAssets = convert && previous?.revision === pinned.revision
  ? new Map(previous.assets.filter(entry => entry.outputSha256)
      .map(entry => [`${entry.atlasKey}:${entry.facing}`, entry]))
  : new Map();
const checkpoint = async () => {
  await fs.mkdir(path.dirname(planPath), { recursive: true });
  await fs.writeFile(planPath, JSON.stringify(plan, null, 2) + '\n');
};

for (const asset of staged.assets) {
  if (!/^[1-9][0-9]*(?:-[a-z0-9-]+)?$/.test(asset.atlasKey)
      || !['front', 'back'].includes(asset.facing)
      || seen.has(`${asset.atlasKey}:${asset.facing}`)) {
    throw new Error(`Invalid or duplicate staged atlas ${asset.atlasKey}:${asset.facing}`);
  }
  seen.add(`${asset.atlasKey}:${asset.facing}`);
  const expectedRomfsPath = `romfs/sprites/pokemon/${asset.facing === 'back' ? 'back/' : ''}${asset.atlasKey}.t3x`;
  if (asset.romfsPath !== expectedRomfsPath
      || !Number.isInteger(asset.width) || !Number.isInteger(asset.height)
      || asset.width < 1 || asset.height < 1
      || !/^sha256:[0-9a-f]{64}$/.test(asset.expectedSha256)) {
    throw new Error(`Invalid conversion record ${asset.atlasKey}:${asset.facing}`);
  }
  const source = inside(asset.sourcePath);
  const sourceBytes = await fs.readFile(source);
  const sourceHash = hash(sourceBytes);
  if (sourceHash !== asset.expectedSha256.slice(7)) {
    throw new Error(`Pinned PNG hash mismatch ${asset.atlasKey}:${asset.facing}`);
  }
  if (sourceBytes.length < 24 || !sourceBytes.subarray(0, 8).equals(Buffer.from('89504e470d0a1a0a', 'hex'))
      || sourceBytes.readUInt32BE(16) !== asset.width || sourceBytes.readUInt32BE(20) !== asset.height) {
    throw new Error(`Pinned PNG dimensions changed ${asset.atlasKey}:${asset.facing}`);
  }
  if (asset.width > plan.maximumTextureEdge || asset.height > plan.maximumTextureEdge) {
    plan.unsupported.push({ atlasKey: asset.atlasKey, facing: asset.facing,
      width: asset.width, height: asset.height,
      classification: 'NOT_YET_SUPPORTED_BY_3DS_TEXTURE_EDGE' });
    continue;
  }
  ++plan.eligible;
  const record = { atlasKey: asset.atlasKey, facing: asset.facing,
    sourcePath: asset.sourcePath, sourceSha256: sourceHash,
    metadataPath: asset.romfsMetadataPath, outputPath: asset.romfsPath,
    width: asset.width, height: asset.height };
  if (convert) {
    const output = inside(path.join('build', asset.romfsPath));
    await fs.mkdir(path.dirname(output), { recursive: true });
    const prior = previousAssets.get(`${asset.atlasKey}:${asset.facing}`);
    let converted = null;
    if (prior?.sourceSha256 === sourceHash && prior.outputPath === asset.romfsPath
        && prior.width === asset.width && prior.height === asset.height) {
      try {
        const existing = await fs.readFile(output);
        if (hash(existing) === prior.outputSha256) converted = existing;
      } catch (error) { if (error.code !== 'ENOENT') throw error; }
    }
    if (!converted) {
      // One pinned PNG is one Citro2D sheet image. No trimming or repacking:
      // .p3a frame coordinates remain in the original PNG coordinate system.
      try {
        execFileSync(tex3ds, ['--atlas', '-f', 'rgba4', '-z', 'auto',
          '-o', output, source], { stdio: 'pipe' });
      } catch (error) {
        plan.unsupported.push({ atlasKey: asset.atlasKey, facing: asset.facing,
          classification: 'TEX3DS_CONVERSION_FAILURE',
          detail: String(error.stderr || error.message).slice(0, 512) });
        await checkpoint();
        throw error;
      }
      converted = await fs.readFile(output);
    }
    if (!converted.length) throw new Error(`tex3ds wrote an empty sheet: ${record.outputPath}`);
    record.outputBytes = converted.length;
    record.outputSha256 = hash(converted);
    ++plan.converted;
  }
  plan.assets.push(record);
  if (convert && plan.converted % 100 === 0) {
    await checkpoint();
    console.log(`Converted ${plan.converted}/${staged.assets.length} pinned atlases`);
  }
}

await checkpoint();
console.log(`${plan.eligible}/${plan.total} pinned atlases eligible; ${plan.unsupported.length} require a split. ${plan.converted} converted. ${planPath}`);

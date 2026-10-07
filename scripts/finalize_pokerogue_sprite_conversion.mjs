import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { POKEROGUE_REPOSITORIES } from '../tools/js/data/PokerogueSource.js';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const build = path.join(root, 'build/upstream-assets');
const staged = JSON.parse(await fs.readFile(path.join(build, 'staged-sprite-assets.json'), 'utf8'));
const normal = JSON.parse(await fs.readFile(path.join(build, 'atlas-conversion-plan.json'), 'utf8'));
const pinned = POKEROGUE_REPOSITORIES['pokerogue-assets'];
if (staged.repository !== pinned.url || staged.revision !== pinned.revision
    || normal.repository !== pinned.url || normal.revision !== pinned.revision) {
  throw new Error('Sprite conversion uses a different pinned source revision');
}
const sha256 = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
const physical = relative => {
  if (typeof relative !== 'string' || path.isAbsolute(relative)
      || relative.split(/[\\/]/).some(part => part === '.' || part === '..'))
    throw new Error(`Unsafe generated path: ${relative}`);
  const result = path.resolve(root, relative);
  if (!result.startsWith(root + path.sep)) throw new Error(`Unsafe generated path: ${relative}`);
  return result;
};
const checkedFile = async (relative, expected) => {
  const bytes = await fs.readFile(physical(relative));
  const found = sha256(bytes);
  if (found !== expected) throw new Error(`Converted file hash mismatch: ${relative}`);
  return bytes.length;
};
const normalById = new Map(normal.assets.map(asset => [`${asset.atlasKey}:${asset.facing}`, asset]));
const output = { schemaVersion: 1, repository: pinned.url, revision: pinned.revision,
  nativePixelPolicy: normal.nativePixelPolicy ?? null,
  atlasCount: 0, textureCount: 0, textureBytes: 0, assets: [] };
if(normal.assets.some(asset=>asset.nativeOverride) && (!output.nativePixelPolicy
    || output.nativePixelPolicy.policySHA256!==sha256(await fs.readFile(path.join(root,'project/data/assets/presentation-overrides.json')))
    || output.nativePixelPolicy.converterSHA256!==sha256(await fs.readFile(path.join(root,'scripts/native_sprite_pixels.py')))))
  throw new Error('Native sprite plan uses an obsolete policy/converter');
for (const source of staged.assets) {
  const id = `${source.atlasKey}:${source.facing}`;
  const regular = normalById.get(id);
  let metadataPath, metadataSha256, textures;
  if(regular?.nativeOverride) {
    const native=regular.nativeOverride;
    if(native.sourceSHA256!==source.expectedSha256.slice(7) || native.sourceMetadataSHA256!==source.metadataSha256
      || native.manifestSha256!==source.manifestSha256) throw new Error(`Native source mismatch: ${id}`);
    metadataPath=native.metadataPath;metadataSha256=native.metadataSha256;textures=native.textures;
  } else if (regular && regular.outputSha256) {
    if (regular.sourceSha256 !== source.expectedSha256.slice(7)
        || regular.outputPath !== source.romfsPath)
      throw new Error(`Converted atlas/source mismatch: ${id}`);
    metadataPath = `build/${source.romfsMetadataPath}`;
    metadataSha256 = source.metadataSha256;
    textures = [{ path: `build/${regular.outputPath}`, sha256: regular.outputSha256 }];
  } else {
    const splitPath = path.join(build, `split-${source.atlasKey}-${source.facing}.json`);
    const split = JSON.parse(await fs.readFile(splitPath, 'utf8'));
    if (split.repository !== pinned.url || split.revision !== pinned.revision
        || split.atlasKey !== source.atlasKey || split.facing !== source.facing
        || split.sourceSha256 !== source.expectedSha256.slice(7)
        || split.manifestSha256 !== source.manifestSha256
        || split.frameCount !== source.frameCount || !Array.isArray(split.pages)
        || split.pages.length < 2) throw new Error(`Invalid split atlas: ${id}`);
    metadataPath = split.metadataPath;
    metadataSha256 = split.metadataSha256;
    textures = split.pages.map((page, index) => {
      if (page.index !== index || !page.texturePath.endsWith(`-p${index}.t3x`))
        throw new Error(`Invalid split page sequence: ${id}`);
      return { path: page.texturePath, sha256: page.textureSha256 };
    });
  }
  await checkedFile(metadataPath, metadataSha256);
  for (const texture of textures) {
    texture.bytes = await checkedFile(texture.path, texture.sha256);
    output.textureBytes += texture.bytes;
    ++output.textureCount;
  }
  output.assets.push({ atlasKey: source.atlasKey, facing: source.facing,
    sourcePath: source.upstreamImagePath,
    sourceSha256: source.upstreamImageSha256 ?? source.expectedSha256.slice(7),
    conversionInputSha256: source.expectedSha256.slice(7),
    sourceAdjustment: source.sourceAdjustment ?? null,
    nativeAdjustment: regular?.nativeOverride?.adjustment ?? null,
    sourceMetadataSha256: source.metadataSha256,
    manifestPath: source.manifestPath, manifestSha256: source.manifestSha256,
    referencedImage: source.referencedImage ?? null,
    imageReferenceOverridden: source.imageReferenceOverridden === true,
    frameCount: source.frameCount, metadataPath, metadataSha256, textures });
  ++output.atlasCount;
}
if (output.atlasCount !== staged.staged || output.atlasCount !== normal.total)
  throw new Error('Not every staged atlas has a converted texture and metadata');
const manifestBytes = Buffer.from(JSON.stringify(output, null, 2) + '\n');
const inventoryPath = path.join(build, 'converted-sprite-assets.json');
await fs.writeFile(inventoryPath, manifestBytes);
console.log(`${output.atlasCount} pinned atlases, ${output.textureCount} .t3x pages, ${output.textureBytes} texture bytes; inventory SHA-256 ${sha256(manifestBytes)}`);

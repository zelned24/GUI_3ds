import fs from 'node:fs/promises';
import {validateMaterializedAppearanceCatalog} from './generate_pokemon_appearance_index.mjs';
import path from 'node:path';
import crypto from 'node:crypto';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { POKEROGUE_REPOSITORIES, PokerogueSource } from '../tools/js/data/PokerogueSource.js';
import { POKEROGUE_BASE_ATLAS_REVISION, POKEROGUE_BASE_ATLAS_IDS,
  POKEROGUE_FORM_ATLAS_KEYS } from '../tools/js/data/PokerogueBaseAtlasIndex.js';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const lock = JSON.parse(await fs.readFile(path.join(root, 'project/data/assets/pokerogue-sprite-lock.json'), 'utf8'));
const pinned = POKEROGUE_REPOSITORIES['pokerogue-assets'];
if (lock.repository !== pinned.url || lock.revision !== pinned.revision
    || POKEROGUE_BASE_ATLAS_REVISION !== pinned.revision || !/^[0-9a-f]{40}$/.test(pinned.revision)) {
  throw new Error('Sprite index or lock revision differs from pinned upstream assets');
}

const all = process.argv.includes('--all');
const destination = path.join(root, 'build/upstream-assets');
const localRepo = path.join(root, 'build/upstream/pokerogue-assets');
let localPinned = false;
try {
  const head = execFileSync('git', ['rev-parse', 'HEAD'], { cwd: localRepo, encoding: 'utf8' }).trim();
  if (head !== pinned.revision) throw new Error(`Local assets are at ${head}, expected ${pinned.revision}`);
  localPinned = true;
} catch (error) {
  if (error.code !== 'ENOENT' && error.code !== 'ENOTDIR' && !/not a git repository/.test(error.message)) throw error;
}

const pngSignature = Buffer.from('89504e470d0a1a0a', 'hex');
const sha256 = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
// Repeated palettes reference identical pinned sources. Retain hashes, not image buffers.
const verifiedAppearanceSourceHashes = new Map();
const locked = new Map(lock.sprites.map(entry => [`${entry.speciesId}:${entry.facing}`, entry]));
const selected = all
  ? ['front', 'back'].flatMap(facing => [
      ...POKEROGUE_BASE_ATLAS_IDS[facing].map(id => ({ key: String(id), facing })),
      ...POKEROGUE_FORM_ATLAS_KEYS[facing].map(key => ({ key, facing }))
    ])
  : lock.sprites.map(entry => ({ key: String(entry.speciesId), facing: entry.facing }));
// Normal female atlases are enumerated from the pinned tree, never guessed.
if(all) {
  if(!localPinned) throw new Error('Full female atlas enumeration requires the pinned local repository');
  const paths=execFileSync('git',['ls-tree','-r','--name-only',pinned.revision,
    'images/pokemon/female','images/pokemon/back/female'],{cwd:localRepo,encoding:'utf8'}).split(/\r?\n/);
  for(const source of paths.sort()) {
    const match=/^images\/pokemon\/(back\/)?female\/([1-9][0-9]*(?:-[a-z0-9-]+)?)\.json$/.exec(source);
    if(match) selected.push({key:match[2]+'-female',facing:match[1]?'back':'front',
      normalFemaleBase:match[2],manifestPath:source,imagePath:source.replace(/\.json$/,'.png')});
  }
}
// Explicit derived appearances keep their pinned source lineage. They use the
// same atlas staging/conversion path as base sprites, with distinct runtime IDs.
if (process.argv.includes('--appearances')) {
  const appearanceRoot = path.join(destination, 'appearances');
  const catalogIdentities=validateMaterializedAppearanceCatalog(JSON.parse(await fs.readFile(path.join(appearanceRoot,'catalog-report.json'),'utf8')));
  const stagedAppearanceIdentities=new Set();
  const visit = async directory => {
    let entries;
    try { entries = await fs.readdir(directory, { withFileTypes: true }); }
    catch (error) { if (error.code === 'ENOENT') return; throw error; }
    for (const entry of entries.sort((a,b)=>a.name.localeCompare(b.name,'en'))) {
      const physical = path.join(directory, entry.name);
      if (entry.isDirectory()) { await visit(physical); continue; }
      if (!entry.name.endsWith('-provenance.json')) continue;
      const appearance = JSON.parse(await fs.readFile(physical, 'utf8'));
      if (appearance.schemaVersion !== 1 || appearance.runtimeStatus !== 'SOURCE_MATERIALIZED_NOT_YET_T3X'
          || !/^[1-9][0-9]*(?:-[a-z0-9-]+)?$/.test(appearance.atlasKey)
          || !['front','back'].includes(appearance.facing) || appearance.shiny !== true
          || typeof appearance.female !== 'boolean' || ![0,1,2].includes(appearance.variant)
          || appearance.converterSHA256 !== sha256(await fs.readFile(path.join(root,'scripts/pokemon_variant_palette.py')))
          || !Array.isArray(appearance.sources) || appearance.sources.length < 6) throw new Error('Invalid derived appearance provenance');
      for (const source of appearance.sources) {
        const config = Object.values(POKEROGUE_REPOSITORIES).find(row=>row.url===source.repository);
        if (!config || config.revision !== source.revision || typeof source.sourcePath !== 'string'
            || source.sourcePath.startsWith('/') || source.sourcePath.split('/').some(part=>part==='..' || part==='.')
            || !/^[0-9a-f]{64}$/.test(source.sha256)) throw new Error('Invalid appearance source pin/path');
        const identity=config.url+'@'+config.revision+':'+source.sourcePath;
        let actualHash=verifiedAppearanceSourceHashes.get(identity);
        if(actualHash===undefined) {
          const raw=execFileSync('git',['show',config.revision+':'+source.sourcePath],
            {cwd:path.join(root,'build/upstream',config.name),maxBuffer:64*1024*1024});
          actualHash=sha256(raw);
          verifiedAppearanceSourceHashes.set(identity,actualHash);
        }
        if(actualHash!==source.sha256) throw new Error('Appearance source hash mismatch');
      }
      const catalogIdentity=[appearance.atlasKey,appearance.facing,appearance.female,appearance.variant].join(':');
      if(catalogIdentities.get(catalogIdentity)!==appearance.pngSHA256 || stagedAppearanceIdentities.has(catalogIdentity))
        throw new Error('Appearance files differ from completed pinned catalog');
      stagedAppearanceIdentities.add(catalogIdentity);
      const basename = entry.name.slice(0,-'-provenance.json'.length);
      const expectedName = appearance.atlasKey+'-shiny-v'+appearance.variant;
      if (basename!==expectedName) throw new Error('Appearance filename differs from identity');
      const imagePhysical = path.join(directory,basename+'.png');
      const manifestPhysical = path.join(directory,basename+'.json');
      const imageBytes = await fs.readFile(imagePhysical), manifestBytes = await fs.readFile(manifestPhysical);
      const upstreamManifest = appearance.sources.findLast(row=>row.repository===pinned.url && row.sourcePath.endsWith('.json') && !row.sourcePath.includes('/variant/'))
        ?? appearance.sources.find(row=>row.repository===pinned.url && /_[1-3]\.json$/.test(row.sourcePath));
      const upstreamImage = appearance.sources.find(row=>row.repository===pinned.url && row.sourcePath.endsWith('.png'));
      if (!upstreamManifest || !upstreamImage || sha256(imageBytes)!==appearance.pngSHA256
          || sha256(manifestBytes)!==upstreamManifest.sha256) throw new Error('Materialized appearance file hash mismatch');
      selected.push({key:appearance.atlasKey+(appearance.female?'-female':'')+'-shiny-v'+appearance.variant,
        facing:appearance.facing, appearance, manifestFile:{bytes:manifestBytes,physical:manifestPhysical},
        imageFile:{bytes:imageBytes,physical:imagePhysical},manifestPath:upstreamManifest.sourcePath,imagePath:upstreamImage.sourcePath});
    }
  };
  await visit(appearanceRoot);
  if(stagedAppearanceIdentities.size!==catalogIdentities.size) throw new Error('Completed appearance catalog has missing physical records');
}
selected.sort((a, b) => a.key.localeCompare(b.key, 'en', { numeric: true }) || a.facing.localeCompare(b.facing));

async function readPinned(sourcePath) {
  const candidates = [
    ...(localPinned ? [path.join(localRepo, sourcePath)] : []),
    path.join(destination, sourcePath)
  ];
  for (const physical of candidates) {
    try { return { bytes: await fs.readFile(physical), physical }; }
    catch (error) { if (error.code !== 'ENOENT') throw error; }
  }
  const response = await fetch(PokerogueSource.buildRawUrl('pokerogue-assets', sourcePath));
  if (!response.ok) throw new Error(`Pinned sprite fetch failed: ${sourcePath} HTTP ${response.status}`);
  const bytes = Buffer.from(await response.arrayBuffer());
  const physical = path.join(destination, sourcePath);
  await fs.mkdir(path.dirname(physical), { recursive: true });
  await fs.writeFile(physical, bytes);
  return { bytes, physical };
}

const staged = [];
const unsupported = [];
const warnings = [];
const seen = new Set();
for (const { key, facing, appearance: suppliedAppearance, normalFemaleBase, manifestFile: derivedManifest, imageFile: derivedImage, manifestPath: derivedManifestPath, imagePath: derivedImagePath } of selected) {
  const identity = `${key}:${facing}`;
  if (!/^[1-9][0-9]*(?:-[a-z0-9-]+)?$/.test(key) || !['front', 'back'].includes(facing) || seen.has(identity))
    throw new Error(`Invalid or duplicate sprite key ${identity}`);
  seen.add(identity);
  const speciesId = Number(key.split('-')[0]);
  const prefix = facing === 'back' ? 'images/pokemon/back' : 'images/pokemon';
  const manifestPath = derivedManifestPath ?? `${prefix}/${key}.json`;
  const imagePath = derivedImagePath ?? `${prefix}/${key}.png`;
  const manifestFile = derivedManifest ?? await readPinned(manifestPath);
  const imageFile = derivedImage ?? await readPinned(imagePath);
  const appearance=normalFemaleBase ? {schemaVersion:1,atlasKey:normalFemaleBase,facing,
    female:true,shiny:false,variant:0,sourceMode:0,upstreamGameRevision:POKEROGUE_REPOSITORIES.pokerogue.revision,
    converterSHA256:sha256(await fs.readFile(fileURLToPath(import.meta.url))),
    sources:[{repository:pinned.url,revision:pinned.revision,sourcePath:manifestPath,sha256:sha256(manifestFile.bytes)},
      {repository:pinned.url,revision:pinned.revision,sourcePath:imagePath,sha256:sha256(imageFile.bytes)}]} : suppliedAppearance;
  const manifestSha256 = sha256(manifestFile.bytes);
  const imageSha256 = sha256(imageFile.bytes);
  const expected = locked.get(`${speciesId}:${facing}`);
  if (!appearance && expected && expected.manifestPath === manifestPath) {
    if (manifestSha256 !== expected.manifestSha256 || imageSha256 !== expected.imageSha256)
      throw new Error(`Pinned sprite lock hash mismatch: ${identity}`);
  }
  const manifest = JSON.parse(manifestFile.bytes.toString('utf8'));
  const png = imageFile.bytes;
  const width = png.length >= 24 ? png.readUInt32BE(16) : 0;
  const height = png.length >= 24 ? png.readUInt32BE(20) : 0;
  if (!png.subarray(0, 8).equals(pngSignature) || width < 1 || height < 1)
    throw new Error(`Invalid pinned PNG: ${imagePath}`);
  const textures = manifest.textures;
  const atlasFormat = Array.isArray(textures) && textures.length === 1 && Array.isArray(textures[0].frames)
    ? 'TEXTUREPACKER_TEXTURES' : Array.isArray(manifest.frames) ? 'ASEPRITE_FRAME_ARRAY'
      : manifest.frames && typeof manifest.frames === 'object' ? 'ASEPRITE_FRAME_OBJECT' : null;
  const texture = atlasFormat === 'TEXTUREPACKER_TEXTURES' ? textures[0]
    : atlasFormat?.startsWith('ASEPRITE_')
      ? { image: manifest.meta?.image, size: manifest.meta?.size,
          frames: Array.isArray(manifest.frames) ? manifest.frames
            : Object.entries(manifest.frames).map(([filename, entry]) => ({ filename, ...entry })) }
      : null;
  if (!texture || !texture.frames.length) {
    unsupported.push({ key, facing, manifestPath, manifestSha256,
      classification: 'NOT_YET_SUPPORTED_BY_GUI_3DS', reason: 'atlas layout' });
    continue;
  }
  const imageReferenceOverridden = texture.image !== `${key}.png`;
  if (imageReferenceOverridden) {
    // Several pinned manifests name their base image (or omit the name), but
    // the keyed PNG is physically present. Keep that discrepancy explicit and
    // only use the keyed PNG after the complete frame geometry check below.
    warnings.push({ key, facing, classification: 'UPSTREAM_IMAGE_REFERENCE_PAIRED_WITH_KEYED_PNG',
      referencedImage: texture.image ?? null, imagePath });
  }
  if (texture.frames.some(frame => !frame?.frame || !Number.isInteger(frame.frame.x)
      || !Number.isInteger(frame.frame.y) || !Number.isInteger(frame.frame.w) || !Number.isInteger(frame.frame.h)
      || frame.frame.x < 0 || frame.frame.y < 0 || frame.frame.w < 1 || frame.frame.h < 1
      || frame.frame.x + frame.frame.w > width || frame.frame.y + frame.frame.h > height)) {
    unsupported.push({ key, facing, manifestPath, manifestSha256, imagePath, imageSha256,
      classification: 'INVALID_UPSTREAM_FRAME_BOUNDS',
      declaredWidth: texture.size?.w, declaredHeight: texture.size?.h,
      physicalWidth: width, physicalHeight: height });
    continue;
  }
  if (texture.size?.w !== width || texture.size?.h !== height) {
    warnings.push({ key, facing, classification: 'UPSTREAM_DECLARED_SIZE_DIFFERS_FROM_PNG',
      declaredWidth: texture.size?.w, declaredHeight: texture.size?.h,
      physicalWidth: width, physicalHeight: height });
  }
  if (!appearance && expected && expected.manifestPath === manifestPath
      && (expected.width !== width || expected.height !== height))
    throw new Error(`Pinned sprite lock dimensions disagree: ${identity}`);
  const metadata = Buffer.alloc(84 + texture.frames.length * 32);
  metadata.write('P3ATLAS1', 0, 'ascii');
  metadata.writeUInt32LE(1, 8);
  metadata.writeUInt16LE(width, 12);
  metadata.writeUInt16LE(height, 14);
  metadata.writeUInt32LE(texture.frames.length, 16);
  Buffer.from(imageSha256, 'hex').copy(metadata, 20);
  Buffer.from(manifestSha256, 'hex').copy(metadata, 52);
  const frameNames = new Set();
  let invalidFrame = null;
  for (let index = 0; index < texture.frames.length; ++index) {
    const entry = texture.frames[index];
    const rect = entry.frame, source = entry.sourceSize, trim = entry.spriteSourceSize;
    const bordered = rect.w === trim?.w + 2 && rect.h === trim?.h + 2;
    const cropX = rect.x + (bordered ? 1 : 0), cropY = rect.y + (bordered ? 1 : 0);
    const cropW = bordered ? trim.w : rect.w, cropH = bordered ? trim.h : rect.h;
    const values = [cropX, cropY, cropW, cropH, source?.w, source?.h,
      trim?.x, trim?.y, entry.duration ?? 0];
    if (typeof entry.filename !== 'string' || !/^[A-Za-z0-9_.-]{1,11}$/.test(entry.filename)
        || frameNames.has(entry.filename) || entry.rotated !== false
        || !values.every(value => Number.isInteger(value) && value >= 0 && value <= 65535)
        || !source.w || !source.h || !cropW || !cropH
        || (!bordered && (trim.w !== rect.w || trim.h !== rect.h))
        || trim.x + cropW > source.w + 1 || trim.y + cropH > source.h + 1) {
      invalidFrame = index;
      break;
    }
    frameNames.add(entry.filename);
    if (bordered) warnings.push({ key, facing, classification: 'ATLAS_EXTRUDED_FRAME_BORDER', frameIndex: index });
    if (trim.x + cropW > source.w || trim.y + cropH > source.h) {
      warnings.push({ key, facing, classification: 'UPSTREAM_SOURCE_CANVAS_ONE_PIXEL_OVERFLOW', frameIndex: index,
        sourceWidth: source.w, sourceHeight: source.h,
        actualRight: trim.x + cropW, actualBottom: trim.y + cropH });
    }
    const offset = 84 + index * 32;
    metadata.write(entry.filename, offset, 'ascii');
    for (let field = 0; field < 9; ++field) metadata.writeUInt16LE(values[field], offset + 12 + field * 2);
    metadata.writeUInt16LE(entry.trimmed ? 1 : 0, offset + 30);
  }
  if (invalidFrame !== null) {
    unsupported.push({ key, facing, manifestPath, manifestSha256, imagePath, imageSha256,
      classification: 'UNSUPPORTED_FRAME_METADATA', frameIndex: invalidFrame });
    continue;
  }
  const metadataRelative = `build/upstream-assets/atlas-metadata/${facing}/${key}.p3a`;
  const metadataPhysical = path.join(root, metadataRelative);
  await fs.mkdir(path.dirname(metadataPhysical), { recursive: true });
  await fs.writeFile(metadataPhysical, metadata);
  const romfsMetadataPath = `romfs/sprites/pokemon/atlas/${facing}/${key}.p3a`;
  const romfsMetadataPhysical = path.join(root, 'build', romfsMetadataPath);
  await fs.mkdir(path.dirname(romfsMetadataPhysical), { recursive: true });
  await fs.writeFile(romfsMetadataPhysical, metadata);
  const relativeSource = path.relative(root, imageFile.physical).replaceAll('\\', '/');
  staged.push({ assetId: `pokemon_sprite_${key}_${facing}`, speciesId, atlasKey: key, facing,
    repository: pinned.url, revision: pinned.revision, manifestPath, manifestSha256,
    sourcePath: relativeSource, upstreamImagePath: imagePath,
    expectedSha256: `sha256:${imageSha256}`, width, height, atlasFormat,
    referencedImage: texture.image ?? null, imageReferenceOverridden,
    metadataPath: metadataRelative, metadataSha256: sha256(metadata),
    romfsMetadataPath,
    romfsPath: `romfs/sprites/pokemon/${facing === 'back' ? 'back/' : ''}${key}.t3x`,
    frameCount: texture.frames.length, ...(appearance ? {appearance,
      upstreamImageSha256: appearance.sources.find(row=>row.repository===pinned.url && row.sourcePath===imagePath).sha256,
      sourceAdjustment: {kind:normalFemaleBase?"PINNED_NORMAL_FEMALE_ATLAS":"PINNED_APPEARANCE_MATERIALIZATION",sourceMode:appearance.sourceMode,converterSHA256:appearance.converterSHA256}} : {}) });
}

await fs.mkdir(destination, { recursive: true });
const output = path.join(destination, 'staged-sprite-assets.json');
await fs.writeFile(output, JSON.stringify({ schemaVersion: 2, repository: pinned.url,
  revision: pinned.revision, indexed: selected.length, staged: staged.length,
  warnings, unsupported, assets: staged }, null, 2) + '\n');
console.log(`Indexed ${selected.length}, staged ${staged.length}, warnings ${warnings.length}, unsupported ${unsupported.length} pinned sprite PNGs: ${output}`);

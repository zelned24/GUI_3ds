import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { PokerogueImporter } from '../public/js/data/PokerogueImporter.js';
import { PokerogueRepository } from '../public/js/data/PokerogueRepository.js';
import { stableCanonicalStringify } from '../public/js/data/CanonicalDataContract.js';
import { execFileSync } from 'node:child_process';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const repository = new PokerogueRepository();
const first = await new PokerogueImporter(repository).importPlayableCanonicalContent(repository);
const second = await new PokerogueImporter(repository).importPlayableCanonicalContent(repository);
if (first.importReport.contentHash !== second.importReport.contentHash) throw new Error('Pinned content import is not deterministic');

const outputDir = path.join(root, 'project', 'data', 'pokerogue');
const docsDir = path.join(root, 'docs', 'generated');
await fs.mkdir(outputDir, { recursive: true });
await fs.mkdir(docsDir, { recursive: true });
await fs.writeFile(path.join(outputDir, 'canonical-content.json'), first.canonicalContent.serialize() + '\n', 'utf8');
const report = { ...first.importReport, deterministicReimport: true };
await fs.writeFile(path.join(outputDir, 'import-report.json'), stableCanonicalStringify(report, 2) + '\n', 'utf8');
await fs.writeFile(path.join(docsDir, 'BETA_UI_8C_IMPORT_REPORT.json'), stableCanonicalStringify(report, 2) + '\n', 'utf8');
execFileSync(process.execPath, [path.join(root, 'scripts', 'generate_3ds_runtime_content.mjs')], { cwd: root, stdio: 'inherit' });
console.log(JSON.stringify({ contentHash: first.importReport.contentHash, counts: first.importReport.catalogCounts }, null, 2));

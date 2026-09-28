import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(scriptDir, '..');
const defaultMapPath = path.join(repoRoot, 'docs', 'semantic-brain-map.json');

function compareStrings(left, right) { return left.localeCompare(right); }

export function serializeSemanticBrainMap(graph) {
  const arraySections = new Set(['contextProfiles', 'edges', 'nodes', 'violations']);
  const keys = Object.keys(graph).sort(compareStrings);
  const lines = ['{'];
  keys.forEach((key, index) => {
    const suffix = index === keys.length - 1 ? '' : ',';
    if (arraySections.has(key) && Array.isArray(graph[key])) {
      lines.push(`  ${JSON.stringify(key)}: [`);
      graph[key].forEach((value, valueIndex) => {
        const itemSuffix = valueIndex === graph[key].length - 1 ? '' : ',';
        lines.push(`    ${JSON.stringify(value)}${itemSuffix}`);
      });
      lines.push(`  ]${suffix}`);
    } else {
      lines.push(`  ${JSON.stringify(key)}: ${JSON.stringify(graph[key])}${suffix}`);
    }
  });
  lines.push('}');
  return `${lines.join('\n')}\n`;
}

export function selectRelevantSubgraph(graph, { query, depth = 1, limit = 8 } = {}) {
  if (!query || !String(query).trim()) throw new Error('A semantic task query is required');
  const normalizedQuery = String(query).trim().toLowerCase();
  const profile = graph.contextProfiles.find(item => item.task.toLowerCase() === normalizedQuery)
    || graph.contextProfiles.find(item => item.task.toLowerCase().includes(normalizedQuery));
  const nodesById = new Map(graph.nodes.map(node => [node.id, node]));
  const selected = new Set();
  const budget = profile ? {
    primary: [...profile.primary], secondary: [...profile.secondary], reference: [...profile.reference]
  } : { primary: [], secondary: [], reference: [] };

  if (profile) {
    [...budget.primary, ...budget.secondary, ...budget.reference].forEach(id => { if (nodesById.has(id)) selected.add(id); });
  } else {
    const tokens = normalizedQuery.split(/[^a-z0-9]+/).filter(Boolean);
    const scored = graph.nodes.map(node => {
      const haystack = `${node.id} ${node.symbol} ${node.responsibility} ${node.tags.join(' ')}`.toLowerCase();
      let score = node.id.toLowerCase() === normalizedQuery ? 100 : 0;
      if (node.id.toLowerCase().includes(normalizedQuery)) score += 40;
      for (const token of tokens) if (haystack.includes(token)) score += token.length > 4 ? 4 : 2;
      return { id: node.id, score };
    }).filter(item => item.score > 0).sort((a, b) => b.score - a.score || compareStrings(a.id, b.id));
    budget.primary = scored.slice(0, Math.max(1, limit)).map(item => item.id);
    budget.primary.forEach(id => selected.add(id));
  }

  let frontier = new Set(budget.primary);
  for (let level = 0; level < Math.max(0, Number(depth)); level++) {
    const next = new Set();
    for (const edge of graph.edges) {
      if (frontier.has(edge.from) && nodesById.has(edge.to) && !selected.has(edge.to)) next.add(edge.to);
      if (frontier.has(edge.to) && nodesById.has(edge.from) && !selected.has(edge.from)) next.add(edge.from);
    }
    next.forEach(id => selected.add(id));
    frontier = next;
    if (!frontier.size) break;
  }

  const ids = [...selected].sort(compareStrings);
  const idSet = new Set(ids);
  const nodes = ids.map(id => nodesById.get(id));
  const edges = graph.edges.filter(edge => idSet.has(edge.from) && idSet.has(edge.to))
    .sort((a, b) => compareStrings(a.from, b.from) || compareStrings(a.type, b.type) || compareStrings(a.to, b.to));
  const violations = graph.violations.filter(item => idSet.has(item.edge.from) || idSet.has(item.edge.to))
    .sort((a, b) => compareStrings(a.id, b.id));

  return { query: String(query), contextBudget: budget, nodes, edges, violations };
}

function runCli() {
  const [, , ...args] = process.argv;
  const query = args.find(arg => !arg.startsWith('--'));
  const depthArg = args.find(arg => arg.startsWith('--depth='));
  const limitArg = args.find(arg => arg.startsWith('--limit='));
  const mapArg = args.find(arg => arg.startsWith('--map='));
  const mapPath = mapArg ? path.resolve(repoRoot, mapArg.slice('--map='.length)) : defaultMapPath;
  const graph = JSON.parse(fs.readFileSync(mapPath, 'utf8'));
  const result = selectRelevantSubgraph(graph, {
    query,
    depth: depthArg ? Number(depthArg.split('=')[1]) : 1,
    limit: limitArg ? Number(limitArg.split('=')[1]) : 8
  });
  process.stdout.write(`${JSON.stringify(result, null, 2)}\n`);
}

if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) runCli();

import { registerQuickJsStorageTests } from './quickjs_storage_tests.js';
import { registerMigrationContentTests } from './migration_content_tests.js';
import { registerRngTests } from './rng_tests.js';
import { registerBattleTests } from './battle_tests.js';
const cases = [];
const register = (name, run) => cases.push({ name, run });
registerMigrationContentTests(register);
registerRngTests(register);
registerBattleTests(register);
registerQuickJsStorageTests(register);
let failed = 0;
for (const { name, run } of cases) {
  try { await run(); console.log(`PASS ${name}`); }
  catch (error) { ++failed; console.error(`FAIL ${name}`, error.stack ?? error); }
}
console.log(`${cases.length - failed} passed, ${failed} failed`);
if (failed) process.exitCode = 1;

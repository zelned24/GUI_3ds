import { registerBetaUI9ATests } from './migration_content_tests.js';
import { registerBetaUI9CRngTests } from './rng_tests.js';
import { registerBetaUI9DTests } from './battle_tests.js';
const cases = [];
const register = (name, run) => cases.push({ name, run });
registerBetaUI9ATests(register);
registerBetaUI9CRngTests(register);
registerBetaUI9DTests(register);
let failed = 0;
for (const { name, run } of cases) {
  try { await run(); console.log(`PASS ${name}`); }
  catch (error) { ++failed; console.error(`FAIL ${name}`, error.stack ?? error); }
}
console.log(`${cases.length - failed} passed, ${failed} failed`);
if (failed) process.exitCode = 1;

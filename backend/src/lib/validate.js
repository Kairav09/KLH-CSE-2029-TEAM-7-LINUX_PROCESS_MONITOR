// Validation for POST /api/runs. Shared by Phase 1 (contract) and Phase 2 (engine).
// Caps are safety limits — see docs/ARCHITECTURE.md §4.6.
import os from "node:os";

export function validateRunConfig(cfg) {
  const { cpuWorkers, cpuIntensity, memoryMB, memoryPattern, durationSec, loadPattern } = cfg;

  if (!Number.isInteger(cpuWorkers) || cpuWorkers < 1 || cpuWorkers > os.cpus().length) {
    return `cpuWorkers must be an integer between 1 and ${os.cpus().length}`;
  }
  if (typeof cpuIntensity !== "number" || cpuIntensity < 1 || cpuIntensity > 100) {
    return "cpuIntensity must be a number between 1 and 100";
  }
  const maxMemMB = Math.floor(os.totalmem() / 1024 / 1024 / 2); // ≤ 50% of RAM
  if (!Number.isInteger(memoryMB) || memoryMB < 0 || memoryMB > maxMemMB) {
    return `memoryMB must be an integer between 0 and ${maxMemMB} (50% of RAM)`;
  }
  if (!["hold", "churn"].includes(memoryPattern)) {
    return "memoryPattern must be 'hold' or 'churn'";
  }
  if (!Number.isInteger(durationSec) || durationSec < 5 || durationSec > 600) {
    return "durationSec must be an integer between 5 and 600";
  }
  if (!["sustained", "ramp", "spike", "step"].includes(loadPattern)) {
    return "loadPattern must be sustained, ramp, spike, or step";
  }
  return null;
}

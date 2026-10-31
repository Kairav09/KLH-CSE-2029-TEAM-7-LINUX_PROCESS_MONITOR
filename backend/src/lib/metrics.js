// Live system sampling using Node's os module + /proc/meminfo for swap (Linux).
// CPU % is computed from tick deltas between successive calls, so the
// first call returns null and the engine warms it up before a run starts.
// Phase 3 adds per-core % (same delta method, per CPU).
import fs from "node:fs";
import os from "node:os";

let prevCpus = null;

function cpuFromDelta() {
  const cpus = os.cpus();
  if (!prevCpus) {
    prevCpus = cpus;
    return { total: null, perCore: null };
  }
  let idleDelta = 0;
  let totalDelta = 0;
  const perCore = [];
  for (let i = 0; i < cpus.length; i++) {
    const prev = prevCpus[i]?.times;
    const cur = cpus[i].times;
    if (!prev) {
      perCore.push(null);
      continue;
    }
    const prevTotal = prev.user + prev.nice + prev.sys + prev.idle + prev.irq;
    const curTotal = cur.user + cur.nice + cur.sys + cur.idle + cur.irq;
    const dIdle = cur.idle - prev.idle;
    const dTotal = curTotal - prevTotal;
    idleDelta += dIdle;
    totalDelta += dTotal;
    perCore.push(dTotal === 0 ? 0 : Math.max(0, Math.min(100, ((dTotal - dIdle) / dTotal) * 100)));
  }
  prevCpus = cpus;
  const total = totalDelta === 0 ? 0 : Math.max(0, Math.min(100, ((totalDelta - idleDelta) / totalDelta) * 100));
  return { total, perCore };
}

function swapMB() {
  // Only on Linux; returns 0 elsewhere instead of crashing.
  try {
    if (os.platform() !== "linux") return 0;
    const text = fs.readFileSync("/proc/meminfo", "utf8");
    const total = /SwapTotal:\s+(\d+)/.exec(text);
    const free = /SwapFree:\s+(\d+)/.exec(text);
    if (!total || !free) return 0;
    return Math.max(0, Math.round((Number(total[1]) - Number(free[1])) / 1024));
  } catch {
    return 0;
  }
}

export function sampleSystem() {
  const totalMB = os.totalmem() / 1024 / 1024;
  const freeMB = os.freemem() / 1024 / 1024;
  const usedMB = totalMB - freeMB;
  const cpu = cpuFromDelta();
  return {
    cpuPct: cpu.total,
    perCore: cpu.perCore,
    memUsedMB: Math.round(usedMB),
    memPct: (usedMB / totalMB) * 100,
    memTotalMB: Math.round(totalMB),
    memFreeMB: Math.round(freeMB),
    swapUsedMB: swapMB(),
    load1: os.loadavg()[0],
  };
}

// Prime the delta so the first real sample in a run is not null.
export function warmupMetrics() {
  sampleSystem();
}

// Stress engine — owns the single active run.
// Spawns CPU workers + one memory worker, samples the OS every 1s,
// enforces load patterns + watchdog, persists samples to SQLite,
// and broadcasts samples over an EventEmitter (SSE in routes).
import { EventEmitter } from "node:events";
import { randomUUID } from "node:crypto";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { Worker } from "node:worker_threads";
import { sampleSystem, warmupMetrics } from "./metrics.js";
import { addSample, createRun, finishRun, getRun } from "./store.js";

const workersDir = path.join(path.dirname(fileURLToPath(import.meta.url)), "..", "workers");

export function intensityAt(t, duration, target, pattern) {
  if (pattern === "ramp") return Math.max(5, Math.round((target * t) / duration));
  if (pattern === "spike") {
    const mid = duration / 2;
    const halfWindow = Math.min(5, duration / 4);
    return t >= mid - halfWindow && t <= mid + halfWindow ? target : Math.round(target * 0.1);
  }
  if (pattern === "step") {
    if (t < duration / 3) return Math.round(target * 0.33);
    if (t < (2 * duration) / 3) return Math.round(target * 0.66);
    return target;
  }
  return target; // sustained
}

class StressEngine extends EventEmitter {
  constructor() {
    super();
    this.active = null; // { id, config, startedAt, workers, memWorker, timer, samples, opsPending, finished }
  }

  getActive() {
    return this.active;
  }

  async start(config) {
    if (this.active) {
      const e = new Error("A run is already active");
      e.code = "RUN_ACTIVE";
      throw e;
    }
    warmupMetrics();
    // Prime one sample so CPU % delta is valid on the first tick.
    await new Promise((r) => setTimeout(r, 120));

    const id = randomUUID();
    const run = { id, createdAt: Date.now(), status: "running", config };
    createRun(run);

    const startedAt = Date.now();
    const samples = [];
    let opsPending = 0;
    let finished = false;

    const cpuWorkers = [];
    for (let i = 0; i < config.cpuWorkers; i++) {
      const w = new Worker(path.join(workersDir, "cpuWorker.js"), {
        workerData: { intensity: intensityAt(0, config.durationSec, config.cpuIntensity, config.loadPattern) },
      });
      w.on("message", (msg) => {
        if (msg?.type === "ops") opsPending += msg.ops;
      });
      w.on("error", () => { /* worker crash: watchdog + sampling keep running; run ends on duration */ });
      cpuWorkers.push(w);
    }

    let memWorker = null;
    if (config.memoryMB > 0) {
      memWorker = new Worker(path.join(workersDir, "memoryWorker.js"), {
        workerData: { memoryMB: config.memoryMB, pattern: config.memoryPattern },
      });
      memWorker.on("error", () => {});
    }

    const finish = async (status, endReason) => {
      if (finished) return;
      finished = true;
      clearInterval(timer);
      // Ask memory worker to free first, then terminate everything.
      try { memWorker?.postMessage({ type: "stop" }); } catch {}
      await Promise.allSettled([
        ...cpuWorkers.map((w) => w.terminate()),
        memWorker ? memWorker.terminate() : Promise.resolve(),
      ]);
      const cpuVals = samples.map((s) => s.cpuPct).filter((v) => v != null);
      const memVals = samples.map((s) => s.memPct).filter((v) => v != null);
      const summary = {
        avgCpu: cpuVals.length ? cpuVals.reduce((a, b) => a + b, 0) / cpuVals.length : null,
        peakCpu: cpuVals.length ? Math.max(...cpuVals) : null,
        avgMemPct: memVals.length ? memVals.reduce((a, b) => a + b, 0) / memVals.length : null,
        peakMemPct: memVals.length ? Math.max(...memVals) : null,
        opsTotal: samples.reduce((a, s) => a + (s.opsPerSec || 0), 0),
      };
      finishRun(id, status, endReason, summary);
      this.active = null;
      this.emit("end", id, { ...getRun(id), samples });
    };

    const timer = setInterval(async () => {
      if (finished) return;
      const t = (Date.now() - startedAt) / 1000;
      // Update live intensity for the pattern.
      const cur = intensityAt(t, config.durationSec, config.cpuIntensity, config.loadPattern);
      for (const w of cpuWorkers) {
        try { w.postMessage({ type: "setIntensity", intensity: cur }); } catch {}
      }
      const sys = sampleSystem();
      const sample = {
        t: Math.round(t * 10) / 10,
        cpuPct: sys.cpuPct,
        perCore: sys.perCore,
        memUsedMB: sys.memUsedMB,
        memPct: sys.memPct,
        swapUsedMB: sys.swapUsedMB,
        load1: sys.load1,
        opsPerSec: opsPending,
      };
      opsPending = 0;
      // Only persist/emit once CPU % is valid (first tick after warmup is real).
      if (sys.cpuPct != null) {
        samples.push(sample);
        addSample(id, sample);
        this.emit("sample", id, sample);
      }
      // Watchdog — memory guardrail only (CPU 100% can be intentional).
      // Absolute thresholds, not relative: the host may already be at 85-90%
      // used (e.g. a small VM), so we only auto-stop when it is truly close
      // to OOM — under 120 MB free, or over 97% used.
      if (sys.memFreeMB < 120 || sys.memPct > 97) {
        await finish("auto-stopped", "Memory guardrail: free memory below 120 MB — auto-stopped to protect the host");
        return;
      }
      if (t >= config.durationSec) {
        await finish("completed", "Duration reached");
      }
    }, 1000);

    this.active = {
      id, config, startedAt, cpuWorkers, memWorker, timer, samples,
      finish,
    };
    return { ...getRun(id) };
  }

  async stop(id, reason = "Stopped by user") {
    if (!this.active || this.active.id !== id) {
      const existing = getRun(id);
      if (!existing) {
        const e = new Error("run_not_found");
        e.code = "NOT_FOUND";
        throw e;
      }
      if (existing.status !== "running") return existing;
      // Run is marked running in DB but no active engine (e.g. server restarted) — mark stopped.
      finishRun(id, "stopped", reason, { avgCpu: existing.avgCpu, peakCpu: existing.peakCpu, avgMemPct: existing.avgMemPct, peakMemPct: existing.peakMemPct, opsTotal: existing.opsTotal ?? 0 });
      return getRun(id);
    }
    await this.active.finish("stopped", reason);
    return getRun(id);
  }

  async shutdown() {
    if (this.active) await this.active.finish("stopped", "Server shutting down");
  }
}

export const engine = new StressEngine();

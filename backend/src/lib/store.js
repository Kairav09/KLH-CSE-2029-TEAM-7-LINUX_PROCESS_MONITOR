// SQLite persistence via Node's built-in node:sqlite (no native dependency).
// DB file lives at backend/data/stress.db — gitignored, created on first run.
import { DatabaseSync } from "node:sqlite";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const dir = path.join(path.dirname(fileURLToPath(import.meta.url)), "..", "..", "data");
fs.mkdirSync(dir, { recursive: true });
const db = new DatabaseSync(path.join(dir, "stress.db"));

db.exec(`
  CREATE TABLE IF NOT EXISTS runs (
    id TEXT PRIMARY KEY,
    created_at INTEGER NOT NULL,
    status TEXT NOT NULL,
    config_json TEXT NOT NULL,
    cpu_workers INTEGER NOT NULL,
    cpu_intensity REAL NOT NULL,
    memory_mb INTEGER NOT NULL,
    memory_pattern TEXT NOT NULL,
    duration_sec INTEGER NOT NULL,
    load_pattern TEXT NOT NULL,
    avg_cpu REAL,
    peak_cpu REAL,
    avg_mem_pct REAL,
    peak_mem_pct REAL,
    ops_total INTEGER DEFAULT 0,
    end_reason TEXT
  );
  CREATE TABLE IF NOT EXISTS samples (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id TEXT NOT NULL,
    t_sec REAL NOT NULL,
    cpu_pct REAL,
    mem_used_mb REAL,
    mem_pct REAL,
    swap_used_mb REAL,
    load1 REAL,
    ops_per_sec REAL,
    per_core_json TEXT,
    FOREIGN KEY (run_id) REFERENCES runs(id)
  );
  CREATE INDEX IF NOT EXISTS idx_samples_run ON samples(run_id);
`);

// Phase 3 migration: per-core samples. Existing DBs (Phase 2) lack this column.
try {
  const cols = db.prepare(`PRAGMA table_info(samples)`).all();
  if (!cols.some((c) => c.name === "per_core_json")) {
    db.exec(`ALTER TABLE samples ADD COLUMN per_core_json TEXT`);
  }
} catch { /* fresh DB already has it via CREATE TABLE below on next restart */ }

const insertRun = db.prepare(
  `INSERT INTO runs (id, created_at, status, config_json, cpu_workers, cpu_intensity, memory_mb, memory_pattern, duration_sec, load_pattern)
   VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`
);
const insertSample = db.prepare(
  `INSERT INTO samples (run_id, t_sec, cpu_pct, mem_used_mb, mem_pct, swap_used_mb, load1, ops_per_sec, per_core_json)
   VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)`
);
const updateRunStatus = db.prepare(`UPDATE runs SET status = ?, end_reason = ? WHERE id = ?`);
const updateRunSummary = db.prepare(
  `UPDATE runs SET status = ?, end_reason = ?, avg_cpu = ?, peak_cpu = ?, avg_mem_pct = ?, peak_mem_pct = ?, ops_total = ? WHERE id = ?`
);

export function createRun(run) {
  insertRun.run(
    run.id, run.createdAt, run.status, JSON.stringify(run.config),
    run.config.cpuWorkers, run.config.cpuIntensity, run.config.memoryMB,
    run.config.memoryPattern, run.config.durationSec, run.config.loadPattern
  );
  return run;
}

export function addSample(runId, s) {
  insertSample.run(runId, s.t, s.cpuPct, s.memUsedMB, s.memPct, s.swapUsedMB, s.load1, s.opsPerSec, s.perCore ? JSON.stringify(s.perCore) : null);
}

export function setRunStatus(id, status, endReason = null) {
  updateRunStatus.run(status, endReason, id);
}

export function finishRun(id, status, endReason, summary) {
  updateRunSummary.run(
    status, endReason,
    summary.avgCpu ?? null, summary.peakCpu ?? null,
    summary.avgMemPct ?? null, summary.peakMemPct ?? null,
    summary.opsTotal ?? 0, id
  );
}

export function listRuns() {
  return db.prepare(`SELECT * FROM runs ORDER BY created_at DESC`).all().map(rowToRun);
}

export function getRun(id) {
  const row = db.prepare(`SELECT * FROM runs WHERE id = ?`).get(id);
  return row ? rowToRun(row) : null;
}

export function getSamples(runId) {
  return db.prepare(`SELECT t_sec AS t, cpu_pct AS cpuPct, mem_used_mb AS memUsedMB, mem_pct AS memPct, swap_used_mb AS swapUsedMB, load1, ops_per_sec AS opsPerSec, per_core_json AS perCoreJson FROM samples WHERE run_id = ? ORDER BY t_sec ASC`).all(runId).map((s) => ({
    ...s,
    perCore: s.perCoreJson ? JSON.parse(s.perCoreJson) : null,
    perCoreJson: undefined,
  }));
}

function rowToRun(r) {
  return {
    id: r.id,
    createdAt: r.created_at,
    status: r.status,
    config: JSON.parse(r.config_json),
    avgCpu: r.avg_cpu,
    peakCpu: r.peak_cpu,
    avgMemPct: r.avg_mem_pct,
    peakMemPct: r.peak_mem_pct,
    opsTotal: r.ops_total,
    endReason: r.end_reason,
  };
}

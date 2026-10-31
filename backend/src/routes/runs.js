import { Router } from "express";
import { engine } from "../lib/stressEngine.js";
import { getRun, getSamples, listRuns } from "../lib/store.js";
import { validateRunConfig } from "../lib/validate.js";

const router = Router();

// POST /api/runs — start a stress run (single active run only).
router.post("/", async (req, res) => {
  const err = validateRunConfig(req.body ?? {});
  if (err) return res.status(400).json({ error: err });
  try {
    const run = await engine.start(req.body);
    return res.status(201).json(run);
  } catch (e) {
    if (e.code === "RUN_ACTIVE") return res.status(409).json({ error: "run_active", message: "A run is already active — stop it before starting another" });
    return res.status(500).json({ error: "start_failed", message: e.message });
  }
});

// GET /api/runs — history, newest first.
router.get("/", (_req, res) => res.json(listRuns()));

// GET /api/runs/:id — run + its samples.
router.get("/:id", (req, res) => {
  const run = getRun(req.params.id);
  if (!run) return res.status(404).json({ error: "run_not_found" });
  return res.json({ ...run, samples: getSamples(req.params.id) });
});

// POST /api/runs/:id/stop — emergency / manual stop.
router.post("/:id/stop", async (req, res) => {
  try {
    const run = await engine.stop(req.params.id);
    return res.json(run);
  } catch (e) {
    if (e.code === "NOT_FOUND") return res.status(404).json({ error: "run_not_found" });
    return res.status(500).json({ error: "stop_failed", message: e.message });
  }
});

// GET /api/runs/:id/stream — SSE, one sample per second + end event.
router.get("/:id/stream", (req, res) => {
  const run = getRun(req.params.id);
  if (!run) return res.status(404).json({ error: "run_not_found" });

  res.writeHead(200, {
    "Content-Type": "text/event-stream",
    "Cache-Control": "no-cache",
    Connection: "keep-alive",
  });
  res.write(`event: start\ndata: ${JSON.stringify({ runId: run.id, status: run.status })}\n\n`);

  // Replay past samples so a late joiner still sees the curve.
  for (const s of getSamples(run.id)) {
    res.write(`event: sample\ndata: ${JSON.stringify(s)}\n\n`);
  }

  if (run.status !== "running") {
    res.write(`event: end\ndata: ${JSON.stringify(run)}\n\n`);
    return res.end();
  }

  const onSample = (runId, sample) => {
    if (runId !== run.id) return;
    res.write(`event: sample\ndata: ${JSON.stringify(sample)}\n\n`);
  };
  const onEnd = (runId, finalRun) => {
    if (runId !== run.id) return;
    res.write(`event: end\ndata: ${JSON.stringify(finalRun)}\n\n`);
    cleanup();
    res.end();
  };
  const cleanup = () => {
    engine.off("sample", onSample);
    engine.off("end", onEnd);
  };
  engine.on("sample", onSample);
  engine.on("end", onEnd);
  req.on("close", cleanup);
});

// GET /api/runs/:id/export — CSV of samples.
router.get("/:id/export", (req, res) => {
  const run = getRun(req.params.id);
  if (!run) return res.status(404).json({ error: "run_not_found" });
  const samples = getSamples(req.params.id);
  const lines = ["t_sec,cpu_pct,mem_used_mb,mem_pct,swap_used_mb,load1,ops_per_sec"];
  for (const s of samples) {
    lines.push([s.t, s.cpuPct ?? "", s.memUsedMB ?? "", s.memPct ?? "", s.swapUsedMB ?? "", s.load1 ?? "", s.opsPerSec ?? ""].join(","));
  }
  res.setHeader("Content-Type", "text/csv");
  res.setHeader("Content-Disposition", `attachment; filename="run-${run.id}.csv"`);
  res.send(lines.join("\n"));
});

export default router;

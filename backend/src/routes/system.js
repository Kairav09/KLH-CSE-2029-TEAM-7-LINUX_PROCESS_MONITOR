import os from "node:os";
import { Router } from "express";

const router = Router();

// GET /api/system — real host info from Node's os module.
// This is live in Phase 1; stress endpoints arrive in Phase 2.
router.get("/", (_req, res) => {
  const cpus = os.cpus();
  res.json({
    cpu: {
      model: cpus[0]?.model ?? "unknown",
      cores: cpus.length,
      speedMHz: cpus[0]?.speed ?? 0,
    },
    memory: {
      totalMB: Math.round(os.totalmem() / 1024 / 1024),
      freeMB: Math.round(os.freemem() / 1024 / 1024),
    },
    os: {
      platform: os.platform(),
      release: os.release(),
      arch: os.arch(),
    },
    // Node returns [0, 0, 0] on Windows — that is "not available", not a real
    // measurement. Return null so the frontend shows N/A instead of fake zeros.
    loadAvg: os.platform() === "win32" ? null : os.loadavg(),
  });
});

export default router;

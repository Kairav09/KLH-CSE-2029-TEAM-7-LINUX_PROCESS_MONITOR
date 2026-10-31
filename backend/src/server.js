import cors from "cors";
import express from "express";
import { engine } from "./lib/stressEngine.js";
import processesRouter, { cleanupTestProcesses } from "./routes/processes.js";
import runsRouter from "./routes/runs.js";
import systemRouter from "./routes/system.js";

const app = express();
const PORT = process.env.PORT || 4000;

app.use(cors());
app.use(express.json());

app.get("/api/health", (_req, res) => {
  res.json({ status: "ok", uptimeSec: Math.round(process.uptime()) });
});

app.use("/api/system", systemRouter);
app.use("/api/runs", runsRouter);
app.use("/api/processes", processesRouter);

// 404 for unknown API routes
app.use("/api", (_req, res) => res.status(404).json({ error: "not_found" }));

const server = app.listen(PORT, () => {
  console.log(`Backend listening on http://localhost:${PORT}`);
});

// Graceful shutdown: stop any active stress run before exiting,
// and kill any test processes (cleanup_test_processes in the C original).
async function shutdown(signal) {
  console.log(`\n${signal} — stopping active run (if any)…`);
  try { cleanupTestProcesses(); } catch {}
  try { await engine.shutdown(); } catch {}
  server.close(() => process.exit(0));
  setTimeout(() => process.exit(0), 3000).unref();
}
process.on("SIGINT", () => shutdown("SIGINT"));
process.on("SIGTERM", () => shutdown("SIGTERM"));

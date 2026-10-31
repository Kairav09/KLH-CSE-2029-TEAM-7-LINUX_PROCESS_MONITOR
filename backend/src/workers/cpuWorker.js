// CPU worker (worker_threads).
// Busy-loop in 100ms windows: busy for intensity% of the window, idle the rest.
// Intensity can be updated live via parentPort messages (load patterns).
// Reports ops every ~1s so the engine can compute throughput.
import { parentPort, workerData } from "node:worker_threads";

let intensity = workerData.intensity ?? 50;
let ops = 0;
let lastReport = Date.now();
let stopped = false;

parentPort.on("message", (msg) => {
  if (msg?.type === "setIntensity" && typeof msg.intensity === "number") {
    intensity = Math.max(0, Math.min(100, msg.intensity));
  }
  if (msg?.type === "stop") stopped = true;
});

function chunk() {
  if (stopped) return;
  const windowMs = 100;
  const busyMs = windowMs * (intensity / 100);
  const start = Date.now();
  // Real work: sqrt loop so the CPU actually computes (not a no-op spin).
  let x = 0;
  while (Date.now() - start < busyMs) {
    for (let i = 0; i < 2000; i++) {
      x += Math.sqrt(Math.random() * 1_000_000 + 1);
    }
    ops += 2000;
  }
  // Prevent the loop from being optimised away.
  if (x === -1) console.log("unreachable");
  if (Date.now() - lastReport >= 1000) {
    parentPort.postMessage({ type: "ops", ops });
    ops = 0;
    lastReport = Date.now();
  }
  const idleMs = windowMs - busyMs;
  setTimeout(chunk, Math.max(0, idleMs));
}

chunk();

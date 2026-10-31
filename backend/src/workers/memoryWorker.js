// Memory worker (worker_threads).
// Allocates Buffers and touches every 4KB page so the OS actually commits RAM
// (plain allocation alone uses lazy pages and would not pressure memory).
// Patterns: hold = allocate once, re-touch periodically; churn = free 25% and
// re-allocate every 2s to create allocation churn.
import { parentPort, workerData } from "node:worker_threads";

const { memoryMB = 0, pattern = "hold" } = workerData;
let buffers = [];
let stopped = false;

const CHUNK_MB = 4;

function touch(buf) {
  for (let off = 0; off < buf.length; off += 4096) buf[off] = (buf[off] + 1) & 0xff;
}

function allocateAll() {
  const n = Math.ceil(memoryMB / CHUNK_MB);
  for (let i = 0; i < n; i++) {
    const size = Math.min(CHUNK_MB, memoryMB - i * CHUNK_MB) * 1024 * 1024;
    if (size <= 0) break;
    const buf = Buffer.alloc(size, 0xab);
    touch(buf);
    buffers.push(buf);
  }
  parentPort.postMessage({ type: "allocated", mb: memoryMB });
}

function churnOnce() {
  // Free ~25% then re-allocate it, to simulate repeated alloc/free pressure.
  const drop = Math.ceil(buffers.length / 4);
  buffers.splice(0, drop);
  const needMB = memoryMB - buffers.reduce((s, b) => s + b.length / 1024 / 1024, 0);
  const n = Math.ceil(Math.max(0, needMB) / CHUNK_MB);
  for (let i = 0; i < n; i++) {
    const size = Math.min(CHUNK_MB, needMB - i * CHUNK_MB) * 1024 * 1024;
    if (size <= 0) break;
    const buf = Buffer.alloc(size, 0xab);
    touch(buf);
    buffers.push(buf);
  }
}

if (memoryMB > 0) allocateAll();

const retouch = setInterval(() => {
  if (stopped) return;
  for (const b of buffers) touch(b);
  if (pattern === "churn") churnOnce();
}, 2000);

parentPort.on("message", (msg) => {
  if (msg?.type === "stop") {
    stopped = true;
    clearInterval(retouch);
    buffers = [];
    parentPort.postMessage({ type: "freed" });
  }
});

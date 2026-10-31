// API client for the process-monitor backend — the web port of the
// C program's scan / control / log cycle (see backend/src/routes/processes.js).
const BASE = "https://cio-unavailable-regulatory-believe.trycloudflare.com";

export async function fetchProcesses() {
  const res = await fetch(`${BASE}/api/processes`);
  const data = await res.json().catch(() => ({}));
  if (!res.ok) {
    const err = new Error(data.error || "scan failed");
    err.status = res.status;
    throw err;
  }
  return data;
}

export async function processAction(pid, action) {
  const res = await fetch(`${BASE}/api/processes/${pid}/${action}`, { method: "POST" });
  const data = await res.json().catch(() => ({}));
  if (!res.ok) {
    const err = new Error(data.error || `${action} failed`);
    err.blocked = !!data.blocked;
    throw err;
  }
  return data;
}

export async function createTestProcess() {
  const res = await fetch(`${BASE}/api/processes/test-process`, { method: "POST" });
  const data = await res.json().catch(() => ({}));
  if (!res.ok) throw new Error(data.error || "could not create test process");
  return data;
}

export async function fetchActivity() {
  const res = await fetch(`${BASE}/api/processes/activity`);
  if (!res.ok) throw new Error("log fetch failed");
  return res.json();
}

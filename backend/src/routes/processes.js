import { spawn } from "node:child_process";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import express from "express";

/* Processes API — a faithful port of the C project
   (Project #18: Linux Process Monitoring and Control System).
   Same data source (/proc), same two-snapshot CPU% method, same
   control rules, same threshold defaults, same log line format.
   The C original is in ../../proc-monitor/ (process.h + member*.c).

   Endpoints:
     GET  /api/processes                 two-snapshot scan, sorted by CPU desc
     POST /api/processes/:pid/pause      SIGSTOP  (control-checked first)
     POST /api/processes/:pid/resume     SIGCONT  (control-checked first)
     POST /api/processes/:pid/kill       SIGKILL  (control-checked first)
     POST /api/test-process              spawn `yes` — the C demo's CPU burner
     GET  /api/activity                  activity.log lines (newest last)
*/

const router = express.Router();

export const CPU_LIMIT_PERCENT = 50.0;   // member3_main.c: CPU_LIMIT_PERCENT
export const MEM_LIMIT_KB = 500000;      // member3_main.c: MEM_LIMIT_KB (~500 MB)
const PF_KTHREAD_FLAG = 0x00200000;      // process.h
const MAX_PROCESSES = 1024;              // process.h
const MAX_TEST_PROCS = 32;               // member3_control.c

/* ---------------- username lookup (getpwuid equivalent) ---------------- */
let passwdByUid = null;
function usernameFor(uid) {
  if (!passwdByUid) {
    passwdByUid = new Map();
    try {
      for (const line of fs.readFileSync("/etc/passwd", "utf8").split("\n")) {
        const parts = line.split(":");
        if (parts.length >= 3) passwdByUid.set(Number(parts[2]), parts[0]);
      }
    } catch { /* no /etc/passwd — fall back to the number, like the C code */ }
  }
  return passwdByUid.get(uid) ?? String(uid);
}
const myUid = typeof process.geteuid === "function" ? process.geteuid() : 0;
const myUsername = usernameFor(myUid);

/* ---------------- Member 1: scanning (member1_scanner.c) ---------------- */
function parseStat(pid) {
  let line;
  try { line = fs.readFileSync(`/proc/${pid}/stat`, "utf8"); } catch { return null; }
  const open = line.indexOf("(");
  const close = line.lastIndexOf(")");
  if (open < 0 || close < open) return null;
  const name = line.slice(open + 1, close);
  const fields = line.slice(close + 2).trim().split(/\s+/);
  // After the name: fields[0]=state, [1]=ppid, [6]=flags, [11]=utime, [12]=stime
  return {
    pid,
    name,
    state: fields[0]?.[0] ?? "?",
    ppid: Number(fields[1] ?? 0),
    isKernelThread: (Number(fields[6] ?? 0) & PF_KTHREAD_FLAG) !== 0,
    utime: Number(fields[11] ?? 0),
    stime: Number(fields[12] ?? 0),
  };
}

function parseStatus(pid, p) {
  let text;
  try { text = fs.readFileSync(`/proc/${pid}/status`, "utf8"); } catch { return false; }
  p.memKb = 0; // kernel threads have no VmRSS line — same default as the C code
  p.uid = 0;
  for (const line of text.split("\n")) {
    if (line.startsWith("VmRSS:")) p.memKb = Number(line.slice(6).trim().split(/\s+/)[0]) || 0;
    else if (line.startsWith("Uid:")) p.uid = Number(line.slice(4).trim().split(/\s+/)[0]) || 0;
  }
  p.owner = usernameFor(p.uid);
  return true;
}

function scanProcesses() {
  let names;
  try { names = fs.readdirSync("/proc"); } catch { return []; }
  const out = [];
  for (const name of names) {
    if (!/^\d+$/.test(name) || out.length >= MAX_PROCESSES) continue;
    const p = parseStat(Number(name));
    if (!p) continue;                    // process vanished — not a bug (C comment)
    if (!parseStatus(Number(name), p)) continue;
    out.push(p);
  }
  return out;
}

/* ---------------- Member 1: CPU% (member1_cpu_calc.c) ---------------- */
const CLK_TCK = 100; // sysconf(_SC_CLK_TCK) is 100 on standard Linux/x86-64
function computeCpuPercent(prev, curr, elapsedSec) {
  const byPid = new Map(prev.map((p) => [p.pid, p]));
  for (const p of curr) {
    const old = byPid.get(p.pid);
    if (!old) { p.cpuPercent = 0; continue; }  // new process — no baseline yet
    const deltaTicks = (p.utime + p.stime) - (old.utime + old.stime);
    p.cpuPercent = ((deltaTicks / CLK_TCK) / elapsedSec) * 100;
  }
}

/* Two snapshots ~1s apart — refresh_snapshot() in member3_main.c */
const wait = (ms) => new Promise((r) => setTimeout(r, ms));
async function refreshSnapshot() {
  const t1 = process.hrtime.bigint();
  const first = scanProcesses();
  await wait(1000);
  const curr = scanProcesses();
  const t2 = process.hrtime.bigint();
  const elapsed = Number(t2 - t1) / 1e9;
  computeCpuPercent(first, curr, elapsed);
  curr.sort((a, b) => b.cpuPercent - a.cpuPercent); // sort_by_cpu_desc (member2)
  return curr;
}

/* ---------------- Member 3: control status (member3_control.c) ---------------- */
function controlStatus(p) {
  if (p.pid === 1) return "INIT";
  if (p.isKernelThread) return "KERN";
  if (myUid === 0 || p.uid === myUid) return "YES";
  return "NO";
}
function explainControl(p) {
  switch (controlStatus(p)) {
    case "INIT":
      return `PID ${p.pid} (${p.name}) is the init process. The kernel protects it: SIGSTOP/SIGKILL sent to it are ignored, even by root.`;
    case "KERN":
      return `PID ${p.pid} (${p.name}) is a kernel thread. It has no user-space code, and the kernel ignores SIGSTOP/SIGKILL for it, so it can't be paused or killed (even by root).`;
    case "NO":
      return `PID ${p.pid} (${p.name}) belongs to '${p.owner}', but you are '${myUsername}'. Linux only lets a user signal their own processes (root can signal anyone). Re-run with sudo to control it.`;
    default:
      return `PID ${p.pid} (${p.name}) can be controlled.`;
  }
}

/* ---------------- Member 3: logger (member3_logger.c) ---------------- */
const logFile = path.join(path.dirname(fileURLToPath(import.meta.url)), "..", "..", "activity.log");
let logLines = [];
try { logLines = fs.readFileSync(logFile, "utf8").split("\n").filter(Boolean).slice(-200); } catch {}
function logAction(action, pid, result) {
  const now = new Date();
  const p2 = (n) => String(n).padStart(2, "0");
  const ts = `${now.getFullYear()}-${p2(now.getMonth() + 1)}-${p2(now.getDate())} ${p2(now.getHours())}:${p2(now.getMinutes())}:${p2(now.getSeconds())}`;
  const line = `[${ts}] ${action} PID ${pid} -- ${result === 0 ? "SUCCESS" : "FAILED"}`;
  logLines.push(line);
  if (logLines.length > 200) logLines = logLines.slice(-200);
  try { fs.appendFileSync(logFile, line + "\n"); } catch {}
}

/* ---------------- Routes ---------------- */
let lastScan = [];
router.get("/", async (_req, res) => {
  if (process.platform !== "linux") {
    return res.status(501).json({ error: "The C project reads /proc, which only exists on Linux. Run this backend on Linux/Ubuntu (or WSL)." });
  }
  try {
    const procs = await refreshSnapshot();
    lastScan = procs;
    res.json({
      scannedAt: new Date().toISOString(),
      me: { uid: myUid, username: myUsername },
      thresholds: { cpuPercent: CPU_LIMIT_PERCENT, memKb: MEM_LIMIT_KB },
      total: procs.length,
      processes: procs.map((p) => ({ ...p, control: controlStatus(p) })),
    });
  } catch (e) {
    res.status(500).json({ error: `scan failed: ${e.message}` });
  }
});

const SIGNALS = { pause: "SIGSTOP", resume: "SIGCONT", kill: "SIGKILL" };
const ACTION_NAME = { pause: "PAUSE", resume: "RESUME", kill: "KILL" };
const DONE_TEXT = { pause: "paused (SIGSTOP)", resume: "resumed (SIGCONT)", kill: "killed (SIGKILL)" };

for (const action of Object.keys(SIGNALS)) {
  router.post(`/:pid/${action}`, (req, res) => {
    const pid = Number(req.params.pid);
    if (!Number.isInteger(pid) || pid <= 0) return res.status(400).json({ error: "That's not a valid PID." });

    /* control_with_check(): look the process up first; if it can't be
       controlled, explain why and log FAILED instead of signalling. */
    const p = lastScan.find((x) => x.pid === pid) ?? (() => { const s = parseStat(pid); return s && parseStatus(pid, s) ? s : null; })();
    if (p && controlStatus(p) !== "YES") {
      logAction(ACTION_NAME[action], pid, -1);
      return res.status(403).json({ blocked: true, error: explainControl(p) });
    }
    if (pid === process.pid) {
      logAction(ACTION_NAME[action], pid, -1);
      return res.status(403).json({ blocked: true, error: `PID ${pid} is this backend server itself — the web port of the C program won't signal its own server. (The C program lists itself too, but killing the server would take the dashboard down with it.)` });
    }
    try {
      process.kill(pid, SIGNALS[action]);
      logAction(ACTION_NAME[action], pid, 0);
      res.json({ ok: true, message: `PID ${pid} ${DONE_TEXT[action]}.` });
    } catch (e) {
      logAction(ACTION_NAME[action], pid, -1);
      res.status(400).json({ error: `Could not ${action} PID ${pid}: ${e.message}` });
    }
  });
}

/* create_test_process(): fork + exec `yes > /dev/null` in C;
   spawn() is the Node equivalent. Children are tracked and killed
   on shutdown — cleanup_test_processes(). */
const testChildren = new Set();
router.post("/test-process", (_req, res) => {
  if (testChildren.size >= MAX_TEST_PROCS) return res.status(400).json({ error: "Too many test processes created." });
  try {
    const child = spawn("yes", [], { stdio: "ignore" });
    testChildren.add(child);
    child.on("exit", () => testChildren.delete(child));
    logAction("CREATE", child.pid, 0);
    res.json({ ok: true, pid: child.pid, message: `Created test process PID ${child.pid} (burning CPU). Refresh to see it.` });
  } catch (e) {
    logAction("CREATE", 0, -1);
    res.status(500).json({ error: `Could not create test process: ${e.message}` });
  }
});
export function cleanupTestProcesses() {
  for (const child of testChildren) { try { child.kill("SIGKILL"); } catch {} }
  testChildren.clear();
}

router.get("/activity", (_req, res) => res.json({ entries: logLines }));

export default router;

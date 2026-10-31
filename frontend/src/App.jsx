import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { createTestProcess, fetchActivity, fetchProcesses, processAction } from "./lib/procApi.js";

/* ProcMonitor — web frontend for Project #18, the C program
   "Linux Process Monitoring and Control System" (proc-monitor/).
   Every panel mirrors something the C program actually does:
   the /proc table (Member 1+2), threshold warnings (Member 2),
   signal controls with blocked explanations (Member 3), and the
   activity.log (Member 3). Design is an original light "system
   ledger" system — Archivo + IBM Plex Mono, deep pine on sage. */

const STATE_META = {
  R: { label: "Running", hint: "Running or runnable" },
  S: { label: "Sleeping", hint: "Interruptible sleep — waiting for an event" },
  D: { label: "Disk sleep", hint: "Uninterruptible sleep — usually waiting on I/O" },
  T: { label: "Stopped", hint: "Stopped by a signal (SIGSTOP) or debugger" },
  t: { label: "Traced", hint: "Stopped by a debugger (tracing)" },
  Z: { label: "Zombie", hint: "Exited, waiting for its parent to reap it" },
  X: { label: "Dead", hint: "Dead — almost never seen" },
  I: { label: "Idle", hint: "Idle kernel thread" },
};

const CTRL_HINT = {
  YES: "You can control it — you own it (or the monitor runs as root)",
  NO: "Owned by another user — Linux will refuse the signal (needs sudo)",
  KERN: "Kernel thread — the kernel ignores SIGSTOP/SIGKILL for it",
  INIT: "PID 1 — kernel-protected init process",
};

function explainLocal(p, me) {
  if (!p) return "";
  switch (p.control) {
    case "INIT":
      return `PID ${p.pid} (${p.name}) is the init process. The kernel protects it: SIGSTOP/SIGKILL sent to it are ignored, even by root.`;
    case "KERN":
      return `PID ${p.pid} (${p.name}) is a kernel thread. It has no user-space code, and the kernel ignores SIGSTOP/SIGKILL for it, so it can't be paused or killed (even by root).`;
    case "NO":
      return `PID ${p.pid} (${p.name}) belongs to '${p.owner}', but you are '${me?.username ?? "?"}'. Linux only lets a user signal their own processes (root can signal anyone). Re-run with sudo to control it.`;
    default:
      return `PID ${p.pid} (${p.name}) can be controlled.`;
  }
}

function LogoMark() {
  return (
    <svg viewBox="0 0 64 64" aria-hidden="true">
      <rect width="64" height="64" rx="12" fill="#1e7a46" />
      <polyline points="10,40 20,40 25,26 32,48 38,32 44,40 54,40" fill="none" stroke="#f4f7f1" strokeWidth="4" strokeLinecap="round" strokeLinejoin="round" />
      <circle cx="50" cy="15" r="3.5" fill="#f2b33d" />
    </svg>
  );
}

export default function App() {
  const [data, setData] = useState(null);
  const [error, setError] = useState("");
  const [platformUnsupported, setPlatformUnsupported] = useState(false);
  const [loading, setLoading] = useState(false);
  const [onlyMine, setOnlyMine] = useState(false);
  const [query, setQuery] = useState("");
  const [selectedPid, setSelectedPid] = useState(null);
  const [notice, setNotice] = useState(null); // {kind: ok|blocked|error, text}
  const [activity, setActivity] = useState([]);
  const [cpuLimit, setCpuLimit] = useState(50);
  const [memLimit, setMemLimit] = useState(500000);
  const [armKillPid, setArmKillPid] = useState(null);
  const [autoRefresh, setAutoRefresh] = useState(false);
  const busyRef = useRef(false);

  const refreshLog = useCallback(async () => {
    try { const a = await fetchActivity(); setActivity(a.entries || []); } catch {}
  }, []);

  const refresh = useCallback(async () => {
    if (busyRef.current) return;
    busyRef.current = true;
    setLoading(true);
    try {
      const d = await fetchProcesses();
      setData(d);
      setError("");
      setPlatformUnsupported(false);
      await refreshLog();
    } catch (e) {
      setError(e.message);
      setPlatformUnsupported(e.status === 501);
    } finally {
      setLoading(false);
      busyRef.current = false;
    }
  }, [refreshLog]);

  useEffect(() => { refresh(); }, [refresh]);

  useEffect(() => {
    if (!autoRefresh) return;
    const id = setInterval(refresh, 5000);
    return () => clearInterval(id);
  }, [autoRefresh, refresh]);

  const processes = data?.processes ?? [];
  const me = data?.me;

  const visible = useMemo(() => {
    let list = processes;
    if (onlyMine) list = list.filter((p) => p.control === "YES");
    const q = query.trim().toLowerCase();
    if (q) list = list.filter((p) => p.name.toLowerCase().includes(q) || String(p.pid) === q || p.owner.toLowerCase().includes(q));
    return list;
  }, [processes, onlyMine, query]);

  /* check_thresholds() — same rule and same wording as member2_threshold.c */
  const warnings = useMemo(() => processes.filter((p) => p.cpuPercent > cpuLimit || p.memKb > memLimit), [processes, cpuLimit, memLimit]);
  const warnText = (p) =>
    `[WARNING] PID ${p.pid} (${p.name}) exceeds limit -- CPU: ${p.cpuPercent.toFixed(2)}% (limit ${Number(cpuLimit).toFixed(2)}%), MEM: ${p.memKb} kB (limit ${memLimit} kB)`;

  const selected = processes.find((p) => p.pid === selectedPid) ?? null;
  const counts = useMemo(() => ({
    total: processes.length,
    controllable: processes.filter((p) => p.control === "YES").length,
    kernel: processes.filter((p) => p.control === "KERN").length,
    running: processes.filter((p) => p.state === "R").length,
    stopped: processes.filter((p) => p.state === "T" || p.state === "t").length,
  }), [processes]);

  async function act(pid, action) {
    setNotice(null);
    setArmKillPid(null);
    try {
      const r = await processAction(pid, action);
      setNotice({ kind: "ok", text: r.message });
      await refresh();
    } catch (e) {
      setNotice({ kind: e.blocked ? "blocked" : "error", text: e.blocked ? `[BLOCKED] ${e.message}` : e.message });
      await refreshLog();
    }
  }

  async function newTestProcess() {
    setNotice(null);
    try {
      const r = await createTestProcess();
      setNotice({ kind: "ok", text: r.message });
      await refresh();
      setSelectedPid(r.pid);
    } catch (e) {
      setNotice({ kind: "error", text: e.message });
    }
  }

  const fmtTime = data?.scannedAt ? new Date(data.scannedAt).toLocaleTimeString() : "—";

  return (
    <div className="page">
      <header className="topbar">
        <div className="brand-mark"><LogoMark /></div>
        <div>
          <div className="brand">ProcMonitor</div>
          <div className="brand-sub">PROJECT #18 · LINUX PROCESS MONITORING &amp; CONTROL</div>
        </div>
        <div className="topbar-meta">
          {me && <span className="chip">signed in as <strong>{me.username}</strong> (uid {me.uid})</span>}
          <span className="chip">thresholds: CPU &gt; {cpuLimit}% · MEM &gt; {memLimit.toLocaleString()} kB</span>
          <span className={`chip status-chip ${error ? (platformUnsupported ? "warn" : "down") : "ok"}`}><span className="dot-live" />{error ? (platformUnsupported ? "LINUX REQUIRED" : "BACKEND OFFLINE") : "MONITOR LIVE"}</span>
        </div>
      </header>

      <main className="layout">
        {/* ---------- Compact hero ---------- */}
        <section className="hero">
          <div className="hero-copy">
            <div className="eyebrow">Operating Systems Project · Based on the C program in <span className="mono">proc-monitor/</span></div>
            <h1 className="hero-title">Watch every process.<br />Control the ones you own.</h1>
            <p className="hero-sub">
              A web front-end for our C process monitor. It scans <span className="mono">/proc</span> twice, one second apart,
              to compute real CPU% per process — then sorts, flags threshold breaches, and sends
              SIGSTOP / SIGCONT / SIGKILL only where Linux actually allows it.
            </p>
            <div className="hero-actions">
              <button className="btn primary" onClick={refresh} disabled={loading}>{loading ? "Scanning…" : "↻ Refresh snapshot"}</button>
              <button className="btn accent" onClick={newTestProcess}>+ New test process</button>
              <label className="auto-toggle">
                <input type="checkbox" checked={autoRefresh} onChange={(e) => setAutoRefresh(e.target.checked)} />
                Auto-refresh 5s
              </label>
            </div>
            <p className="hero-note">New test process = the C demo's trick: it forks <span className="mono">yes &gt; /dev/null</span>, a process that burns ~100% of one core so you can watch it top the table, then pause or kill it.</p>
          </div>
          <div className="hero-stats" aria-label="Snapshot summary">
            <div className="stat"><span className="stat-num">{counts.total || "—"}</span><span className="stat-label">Processes scanned</span></div>
            <div className="stat"><span className="stat-num">{counts.controllable || "—"}</span><span className="stat-label">You can control (CTRL = YES)</span></div>
            <div className="stat warn"><span className="stat-num">{warnings.length}</span><span className="stat-label">Over threshold</span></div>
            <div className="stat"><span className="stat-num">{counts.running || "—"}</span><span className="stat-label">Running (state R)</span></div>
            <div className="stat-foot">Last snapshot: {fmtTime} · two scans, 1s apart · sorted by CPU ↓</div>
          </div>
        </section>

        {error && <div className="banner error-banner">{error}{!platformUnsupported && <> — start the backend with <span className="mono">cd backend &amp;&amp; npm run dev</span>.</>}</div>}
        {notice && <div className={`banner notice-${notice.kind}`}>{notice.text}</div>}

        {/* ---------- 01 Process table ---------- */}
        <section className="panel">
          <div className="panel-head">
            <div>
              <div className="section-tag">01 · Process table</div>
              <h2>Live processes — sorted by CPU</h2>
            </div>
            <div className="table-tools">
              <input className="search" type="text" placeholder="Filter by name, PID or user…" value={query} onChange={(e) => setQuery(e.target.value)} aria-label="Filter processes" />
              <button className={`btn toggle ${onlyMine ? "on" : ""}`} onClick={() => setOnlyMine((v) => !v)} aria-pressed={onlyMine}>
                Filter: only mine {onlyMine ? "ON" : "OFF"}
              </button>
            </div>
          </div>

          <div className="table-scroll">
            <table className="ptable">
              <thead>
                <tr><th>PID</th><th>PPID</th><th>USER</th><th>STATE</th><th>MEM (kB)</th><th>CPU%</th><th>CTRL</th><th>NAME</th><th>ACTIONS</th></tr>
              </thead>
              <tbody>
                {visible.map((p) => (
                  <tr key={p.pid} className={selectedPid === p.pid ? "selected" : ""} onClick={() => setSelectedPid(p.pid)}>
                    <td className="mono">{p.pid}</td>
                    <td className="mono">{p.ppid}</td>
                    <td>{p.owner}</td>
                    <td><span className={`state state-${p.state}`} title={STATE_META[p.state]?.hint || p.state}>{p.state}</span></td>
                    <td className="mono">{p.memKb.toLocaleString()}</td>
                    <td className="mono cpu-cell">
                      <span>{p.cpuPercent.toFixed(2)}</span>
                      <span className="cpu-bar"><span style={{ width: `${Math.min(100, p.cpuPercent)}%` }} /></span>
                    </td>
                    <td><span className={`ctrl ctrl-${p.control}`} title={CTRL_HINT[p.control]}>{p.control}</span></td>
                    <td className="pname">{p.name}{p.isKernelThread ? <span className="kern-tag">kernel</span> : null}</td>
                    <td className="row-actions" onClick={(e) => e.stopPropagation()}>
                      {p.control === "YES" ? (
                        <>
                          {p.state === "T" || p.state === "t"
                            ? <button className="mini" onClick={() => act(p.pid, "resume")}>Resume</button>
                            : <button className="mini" onClick={() => act(p.pid, "pause")}>Pause</button>}
                          {armKillPid === p.pid
                            ? <button className="mini kill armed" onClick={() => act(p.pid, "kill")}>Confirm kill?</button>
                            : <button className="mini kill" onClick={() => setArmKillPid(p.pid)}>Kill</button>}
                        </>
                      ) : (
                        <button className="mini why" onClick={() => { setSelectedPid(p.pid); setNotice({ kind: "blocked", text: `[BLOCKED] ${explainLocal(p, me)}` }); }}>Why not?</button>
                      )}
                    </td>
                  </tr>
                ))}
                {visible.length === 0 && (
                  <tr><td colSpan={9} className="empty-row">{loading ? "Scanning /proc…" : processes.length ? "No processes match this filter." : "No scan yet."}</td></tr>
                )}
              </tbody>
            </table>
          </div>
          <div className="table-foot">
            <span>{onlyMine ? `Showing ${visible.length} of ${processes.length} processes (filter ON: only ones you can control)` : `Total processes: ${processes.length}`}</span>
            <span className="ctrl-legend">CTRL: <strong>YES</strong> = you can control it · <strong>NO</strong> = another user's (needs sudo) · <strong>KERN</strong> = kernel thread · <strong>INIT</strong> = PID 1 (kernel-protected)</span>
          </div>
        </section>

        {/* ---------- Detail + thresholds + log ---------- */}
        <section className="lower-grid">
          <div className="panel">
            <div className="section-tag">02 · Inspect &amp; control</div>
            <h2>Process detail</h2>
            {selected ? (
              <>
                <div className="detail-title"><span className="mono">PID {selected.pid}</span> · {selected.name}</div>
                <dl className="detail-list">
                  <div><dt>Parent (PPID)</dt><dd className="mono">{selected.ppid}</dd></div>
                  <div><dt>Owner</dt><dd>{selected.owner} (uid {selected.uid})</dd></div>
                  <div><dt>State</dt><dd>{selected.state} — {STATE_META[selected.state]?.label || "Unknown"} · {STATE_META[selected.state]?.hint || ""}</dd></div>
                  <div><dt>Memory (VmRSS)</dt><dd className="mono">{selected.memKb.toLocaleString()} kB</dd></div>
                  <div><dt>CPU%</dt><dd className="mono">{selected.cpuPercent.toFixed(2)}%</dd></div>
                  <div><dt>CPU ticks (utime + stime)</dt><dd className="mono">{selected.utime} + {selected.stime}</dd></div>
                  <div><dt>Kernel thread</dt><dd>{selected.isKernelThread ? "Yes (PF_KTHREAD set in /proc stat flags)" : "No"}</dd></div>
                  <div><dt>Control</dt><dd><span className={`ctrl ctrl-${selected.control}`}>{selected.control}</span> {CTRL_HINT[selected.control]}</dd></div>
                </dl>
                <p className="explain">{explainLocal(selected, me)}</p>
                {selected.control === "YES" && (
                  <div className="detail-actions">
                    {selected.state === "T" || selected.state === "t"
                      ? <button className="btn primary" onClick={() => act(selected.pid, "resume")}>Resume (SIGCONT)</button>
                      : <button className="btn primary" onClick={() => act(selected.pid, "pause")}>Pause (SIGSTOP)</button>}
                    {armKillPid === selected.pid
                      ? <button className="btn danger" onClick={() => act(selected.pid, "kill")}>Confirm kill (SIGKILL)?</button>
                      : <button className="btn danger-ghost" onClick={() => setArmKillPid(selected.pid)}>Kill (SIGKILL)</button>}
                  </div>
                )}
              </>
            ) : (
              <p className="muted">Click any row in the table to inspect it here — owner, state, ticks, and exactly why it can or can't be controlled.</p>
            )}

            <div className="threshold-box">
              <div className="section-tag">03 · Thresholds</div>
              <h3>Limits — from <span className="mono">member3_main.c</span></h3>
              <div className="thresh-fields">
                <label>CPU limit (%)<input type="number" min={1} max={1000} value={cpuLimit} onChange={(e) => setCpuLimit(Number(e.target.value) || 50)} /></label>
                <label>MEM limit (kB)<input type="number" min={0} step={10000} value={memLimit} onChange={(e) => setMemLimit(Number(e.target.value) || 500000)} /></label>
              </div>
              {warnings.length === 0
                ? <p className="muted">No process is over the limits in the latest snapshot.</p>
                : <div className="warnings">{warnings.map((p) => <div key={p.pid} className="warn-line mono">{warnText(p)}</div>)}</div>}
            </div>
          </div>

          <div className="panel log-panel">
            <div className="section-tag">04 · Activity log</div>
            <h2>activity.log</h2>
            <p className="muted">Every control action is logged with a timestamp, exactly like <span className="mono">member3_logger.c</span> — blocked attempts are logged as FAILED, just as the C program logs them.</p>
            {activity.length === 0
              ? <div className="log-empty mono">No actions logged yet this session.<br />Pause, resume, kill, or create a test process and it will appear here.</div>
              : <ol className="log-list mono">{[...activity].reverse().map((line, i) => <li key={i} className={line.includes("FAILED") ? "failed" : ""}>{line}</li>)}</ol>}
          </div>
        </section>

        {/* ---------- How it works ---------- */}
        <section className="panel">
          <div className="section-tag">05 · How it works</div>
          <h2>From C program to web page — the same cycle</h2>
          <div className="cycle">
            <div className="cycle-step"><span>1</span><h3>Scan</h3><p>Read every numeric entry in <span className="mono">/proc</span>. Parse <span className="mono">stat</span> for PID, PPID, state, flags and CPU ticks (splitting at the last <span className="mono">)</span> because names can contain spaces), and <span className="mono">status</span> for VmRSS memory and owner UID.</p></div>
            <div className="cycle-step"><span>2</span><h3>Wait, scan again</h3><p><span className="mono">/proc</span> only gives cumulative ticks, never a live %. So take two snapshots ~1s apart: CPU% = (Δticks ÷ CLK_TCK) ÷ elapsed × 100. New processes report 0% — they have no baseline yet.</p></div>
            <div className="cycle-step"><span>3</span><h3>Sort &amp; flag</h3><p>Sort by CPU% descending, like <span className="mono">top</span>. Flag anything over the limits (defaults from the C code: CPU &gt; 50%, MEM &gt; 500,000 kB) with a [WARNING] line.</p></div>
            <div className="cycle-step"><span>4</span><h3>Check, signal, log</h3><p>Before any signal, check controllability: PID 1 and kernel threads are protected, other users' processes need sudo. Allowed actions send SIGSTOP / SIGCONT / SIGKILL; every attempt is written to <span className="mono">activity.log</span>.</p></div>
          </div>
          <div className="modules">
            <div className="module-card">
              <div className="module-who">Member 1 · Scanning</div>
              <div className="module-files mono">member1_scanner.c · member1_cpu_calc.c</div>
              <p>/proc parsing and the two-snapshot CPU% calculation.</p>
            </div>
            <div className="module-card">
              <div className="module-who">Member 2 · Display</div>
              <div className="module-files mono">member2_display.c · member2_threshold.c</div>
              <p>CPU-descending sort, the process table, and threshold warnings.</p>
            </div>
            <div className="module-card">
              <div className="module-who">Member 3 · Control</div>
              <div className="module-files mono">member3_control.c · member3_logger.c · member3_main.c</div>
              <p>Signals with control checks and explanations, the test-process helper, the activity logger, and the main loop.</p>
            </div>
          </div>
          <div className="build-box mono">
            <div className="build-title">Run the original C program (Ubuntu):</div>
            gcc -Wall member1_scanner.c member1_cpu_calc.c member2_display.c member2_threshold.c member3_control.c member3_logger.c member3_main.c -o proc_monitor &amp;&amp; ./proc_monitor
            <div className="build-alt">…or the all-in-one copy: gcc -Wall final.c -o final &amp;&amp; ./final</div>
          </div>
        </section>
      </main>

      <footer className="foot">ProcMonitor · Project #18 — Linux Process Monitoring &amp; Control System · Web front-end for the C program in <span className="mono">proc-monitor/</span> · Menu in C: [r] refresh · [p] pause · [c] resume · [k] kill · [n] new test process · [f] filter · [q] quit</footer>
    </div>
  );
}

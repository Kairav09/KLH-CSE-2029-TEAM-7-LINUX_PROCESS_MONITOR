# Live Demo Script — CPU and Memory Stress Testing Tool (StressLab)

**Total time: ~6 minutes. Rehearse once on the demo laptop before evaluation.**
**Golden rule: start from a clean state — no run active, history visible.**

## 0. Before the demo (2 min before, not on stage)
1. `cd backend && npm run dev` — check `http://localhost:4000/api/health` returns ok.
2. `cd frontend && npm run dev` — open `http://localhost:5173`.
3. Close other heavy apps. The demo machine should be as idle as possible so numbers are clean.
4. Have this script open on a second screen / phone — do not read it aloud.

## 1. Opening — the problem (45 sec)
> "Existing stress tools are terminal commands — powerful but hard to control, hard to watch, and risky for beginners. A wrong memory value can freeze the machine. We built StressLab: a web app where you pick a controlled workload, watch the system live, and compare runs — with safety caps so you can't freeze the host."

Point at the dashboard: System panel (real CPU model, cores, RAM, OS).

## 2. First run — Medium preset (90 sec)
1. Click **Medium** preset. Read the form aloud: "2 workers, 60%, 256 MB, 30 seconds, sustained."
2. Click **Start run**. Point out:
   - Progress bar + elapsed / remaining.
   - Live stat tiles: CPU %, Memory %, Swap, Load + Throughput.
   - Charts moving every second (CPU %, Memory %, Throughput).
   - Per-core bars — "each core is measured separately, so you can see scheduling."
3. While it runs, say: "The backend spawns worker threads with a duty cycle — 60% means busy 60ms, idle 40ms per 100ms window. Memory is allocated and every 4KB page is touched, so the OS actually commits the RAM."
4. Let it finish (or STOP early once — see step 3). Point at history: "Every run is saved to SQLite with its config and averages."

## 3. STOP + safety (45 sec)
1. Start a **Heavy** preset run (or a 60s run), wait 3 seconds, click **STOP**.
2. Say: "STOP terminates all workers immediately — measured 0.08–0.10s in our tests — and memory is freed."
3. Mention caps: "Memory is capped at 50% of RAM, workers at core count, duration at 600s. A watchdog auto-stops if free memory drops below 120 MB. We deliberately use absolute thresholds — an early version used 92% and false-triggered on a busy VM."

## 4. Patterns — Spike vs Sustained (60 sec)
1. Open the detail of the Medium run you just did (click its history row) — show its charts + CSV + Copy summary.
2. Say: "Now the part that makes this more than on/off." Start a **Spike test** preset (or show a previous Spike run's detail).
3. Explain the four patterns: Sustained (constant), Ramp (rises linearly), Spike (quiet, burst, quiet), Step (three rising steps).
4. Quote real results: "In our test matrix, Spike showed middle 79.7% vs edges 34.5%, and Step showed 35.3 / 48.1 / 66.1 across its three thirds — the shapes come out exactly as configured."

## 5. Compare — the payoff (60 sec)
1. Tick two history rows (e.g. a Light run and a Medium/Heavy run) → **Compare selected**.
2. Show: overlay charts (A indigo, B red), difference table in percentage points.
3. Quote: "Light 1×30% vs Medium 2×60% gave avg CPU 34.0% vs 64.1% — a +30.2pp difference. That's the kind of A/B answer this tool is for: does this config actually use more CPU?"
4. Click **Download compare CSV** — "Every run and every comparison exports to CSV for the report."

## 6. Close — who is this for (30 sec)
> "Students can see scheduling, paging, and saturation live. Developers and QA can check how an app behaves under load. DevOps can validate a small server before deploying. Everything is original, runs locally in a controlled environment, and never touches other users' processes."

## If something goes wrong (backup lines)
- **Backend not reachable:** "Let me restart the backend — `npm run dev` — the frontend shows 'Backend offline' until it reconnects." (Restart, refresh.)
- **Run already active (409):** "Only one run at a time by design — I'll STOP the current one first." (Click STOP, retry.)
- **Numbers look noisy:** "CPU % is total system CPU, so background processes add noise — that's why we compare runs on the same machine rather than quoting absolute numbers."
- **No history:** "History is SQLite at `backend/data/stress.db` — if the file was deleted, history starts empty, which is expected."

## Questions the evaluators may ask (and short answers)
- **Why Node, not Python/psutil?** "Node's `worker_threads` start fast and terminate cleanly, and the whole stack is one language. We read the same OS counters (`os.cpus()`, `os.freemem()`, `os.loadavg()`) and compute CPU % from tick deltas, plus swap from `/proc/meminfo` on Linux."
- **Why SSE, not WebSockets?** "Live data is one-way, server to browser. SSE is simpler, auto-reconnects, and needs no extra protocol."
- **Why single active run?** "So measurements stay interpretable — two overlapping runs would make every graph meaningless, and it prevents accidental double-load."
- **How do you know memory is really used?** "We touch every 4KB page. Plain allocation uses lazy pages and wouldn't pressure RAM. We verified free memory drops during a run and returns after STOP."
- **What did each member do?** (Fill in before demo — see README Teamwork note. Each member must be able to explain the engine, the metrics, and the frontend.)

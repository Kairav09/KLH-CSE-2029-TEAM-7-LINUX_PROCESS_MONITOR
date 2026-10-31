# StressLab — CPU and Memory Stress Testing Tool

A controlled workload generator that stresses CPU and memory and measures system performance under different loads. Built as a complete web application for the OS Hackathon.

## ProcMonitor web frontend (v4, 2026-10-31) — based on the C project

The current web frontend (`frontend/`) is **ProcMonitor**, a web version of the team's C program **Project #18: Linux Process Monitoring and Control System** (sources in `proc-monitor/`). The backend route `backend/src/routes/processes.js` is a faithful port of the C logic, and the page mirrors the C program's interface:

| C program | Web frontend |
|---|---|
| `member1_scanner.c` — parse `/proc/<pid>/stat` + `status` | `GET /api/processes` — same parsing (name split at last `)`, flags → kernel-thread, VmRSS, UID → username) |
| `member1_cpu_calc.c` — two snapshots ~1s apart, CPU% = (Δticks/CLK_TCK)/elapsed × 100 | Same two-snapshot scan per request; new processes report 0% |
| `member2_display.c` — table sorted by CPU desc, CTRL column (YES/NO/KERN/INIT) | Process table with the same columns, order, badges and CTRL legend; "Filter: only mine" = the C `[f]` toggle |
| `member2_threshold.c` — `[WARNING] PID … exceeds limit` (defaults CPU > 50%, MEM > 500000 kB from `member3_main.c`) | Threshold panel with the same defaults (editable) and identical warning wording |
| `member3_control.c` — control check + explanation before SIGSTOP/SIGCONT/SIGKILL; test process = fork + `yes` | Pause/Resume/Kill buttons (kill is two-click), blocked actions return the C explanation text; "New test process" spawns `yes` (~100% CPU) |
| `member3_logger.c` — `[YYYY-MM-DD HH:MM:SS] ACTION PID n -- SUCCESS/FAILED` in `activity.log` | Log panel fed from the same `activity.log` (backend root); blocked attempts logged FAILED, as in C |

Run it: backend `npm run dev` (:4000) + frontend `npm run dev` (:5173), on Linux/Ubuntu (it reads `/proc`). Run the original: `gcc -Wall final.c -o final && ./final` inside `proc-monitor/` (or the modular build in `member3_main.c`'s header). Design: an original light "System Ledger" theme (Archivo + IBM Plex Mono, deep pine on sage) made for this project; hero title capped at 38px. The stress-run API (`/api/runs`, `/api/system`) still exists in the backend; the earlier stress dashboard frontend was replaced by ProcMonitor and is preserved in `os-project-improved.zip` history. One deliberate safety deviation: the web backend refuses to signal its own server PID.

## What it does
- Configure CPU workers, CPU intensity, memory size, memory pattern, duration, and load pattern (Sustained / Ramp / Spike / Step)
- Watch live CPU, per-core bars, memory, swap, load-average, and throughput while the run is happening — small 160px charts with hover tooltips, not big graphs
- Click any past run to open its detail view with full charts, CSV, and copyable summary
- Tick two runs to compare them side-by-side with overlay charts and a difference table
- One-click presets: Baseline, Light, Medium, Heavy, Spike, Ramp, Memory-pressure
- Safety first: hard caps, auto-stop on dangerous thresholds, one-click emergency STOP
- Save every run, compare two runs, export CSV

## Stack
- **Frontend:** React + Vite (dashboard, `frontend/`, port 5173)
- **Backend:** Node + Express (API + stress engine, `backend/`, port 4000)
- **Store:** SQLite — run history + per-second samples (Phase 2+)
- **Live updates:** Server-Sent Events (SSE)

## Quick start (Phase 1)

```bash
# Backend
cd backend
npm install
npm run dev        # http://localhost:4000/api/health

# Frontend (new terminal)
cd frontend
npm install
npm run dev        # http://localhost:5173
```

## Project structure
```
os-project/
├── docs/
│   ├── PROBLEM_DEFINITION.md
│   ├── ARCHITECTURE.md
│   ├── TESTING.md
│   ├── PROJECT_REPORT.md
│   ├── PRESENTATION.pptx
│   └── DEMO_SCRIPT.md
├── proc-monitor/
│   └── The C project (Project #18): process.h, member1_*.c, member2_*.c, member3_*.c, final.c
├── backend/
│   ├── package.json
│   ├── activity.log (created at runtime by the processes API, gitignored)
│   ├── data/ (stress.db — created on first run, gitignored)
│   └── src/
│       ├── server.js
│       ├── routes/ (system.js, runs.js, processes.js)
│       ├── lib/ (metrics.js, store.js, stressEngine.js, validate.js)
│       └── workers/ (cpuWorker.js, memoryWorker.js)
├── frontend/
│   ├── package.json, vite.config.js, index.html
│   └── src/
│       ├── App.jsx, App.css, main.jsx
│       └── lib/ (procApi.js)
└── README.md
```

## Docs
- Problem definition: `docs/PROBLEM_DEFINITION.md`
- Architecture + API contract: `docs/ARCHITECTURE.md`
- Testing matrix: `docs/TESTING.md`
- Project report: `docs/PROJECT_REPORT.md` (also as PDF: `docs/PROJECT_REPORT.pdf`)
- Presentation: `docs/PRESENTATION.pptx` (source: `docs/build_pptx.py`)
- Live demo script: `docs/DEMO_SCRIPT.md`
- Evaluation mapping: see below

## Evaluation mapping (100 pts)
| Criterion | Points | Where it is covered |
|---|---|---|
| Problem Definition | 10 | `docs/PROBLEM_DEFINITION.md` |
| Novelty and Innovation | 15 | Load patterns, safety guardrails, compare + export |
| Architecture Design | 10 | `docs/ARCHITECTURE.md` |
| Implementation | 20 | Working engine + dashboard (Phases 2–4) |
| Testing | 10 | Testing matrix (Phase 5) |
| Documentation | 10 | README + docs + inline comments |
| GitHub Repository link | 5 | Repo link at submission |
| Presentation | 10 | Demo script + slides (Phase 5) |
| Teamwork | 5 | Team of 3 — contributions decided at the end |
| Business Impact | 5 | QA / DevOps / capacity-planning use case |

## Phases
- **Phase 1 (done):** Problem definition + architecture + skeleton + `/api/system` + dashboard shell
- **Phase 2 (done, verified 2026-10-16):** Real stress engine — CPU workers with duty-cycle intensity, memory allocation + page-touching (hold/churn), per-second metrics, SSE live stream, STOP (0.1s), watchdog (auto-stop when free memory &lt; 120 MB), SQLite persistence, CSV export, frontend wired (controls + live sparklines + history). See `docs/TESTING.md` for verified results.
- **Phase 3 (done, verified 2026-10-16):** Polished dashboard — 160px SVG charts with axes + hover crosshair/tooltip (CPU %, Memory %, Throughput), per-core CPU bars, run detail view (click any history row: config + avg/peak + full charts + CSV), live stat tiles, progress bar with elapsed/remaining, SQLite per-core migration for old DBs. See `docs/TESTING.md`.
- **Phase 4 (done, verified 2026-10-16):** Presets (Baseline, Light, Medium, Heavy, Spike, Ramp, Memory-pressure — auto-clamped to host cores/RAM), run compare (tick 2 history rows → overlay CPU/Memory charts + diff table in pp + compare CSV + copy compare summary), report helpers (Copy summary on run detail, for PPT/report). Compare logic in `frontend/src/lib/compare.js` (pure, unit-tested). See `docs/TESTING.md`.
- **Phase 5 (done, verified 2026-10-16):** Full test matrix (4 patterns + edges + validation + SSE/STOP — see `docs/TESTING.md`), project report (`docs/PROJECT_REPORT.md`), presentation (`docs/PRESENTATION.pptx`), live demo script (`docs/DEMO_SCRIPT.md`).
- **Frontend redesign (done, verified 2026-10-16):** "Thermal Console" visual system — dark instrument theme built for this project (Space Grotesk + JetBrains Mono; CPU = amber/heat, Memory = cyan/coolant), hero with live status + big CPU readout, Lab console with preset cards, sliders, Hold/Churn segmented control and load-pattern previews (Sustained/Ramp/Spike/Step mini-graphs), telemetry panel with KPI tiles + per-core bars, machine strip with free-memory warning. Engine untouched. Also fixed: Load avg now shows **N/A** on Windows (Node returns fake `0.00` there) — backend returns `loadAvg: null` on `win32`.
- **Frontend redesign v3 (done, verified 2026-10-31):** "Editorial Lab" visual system inspired by the Team 7 project site — paper-on-ink palette (signal orange `#FF4D00` = CPU, acid lime `#D9FF3D` = Memory), Fraunces serif display type, numbered sections (01 Stress Vector / 02 Control Deck / 03 Live Telemetry / 04 Benchmark Records / 05 How It Works / 06 Team 07), corner-tick lab panels, chamfered primary CTAs, square-thumb sliders, graph-paper grid background, live CHRONO clock, and a Team 07 attribution section. The site's oversized hero type was deliberately **not** copied: the hero tagline is capped at 48px (`clamp(32px, 4.2vw, 48px)`). No Tailwind/GSAP/custom-cursor from the site — this stays a clean React + Vite build. Engine and API logic untouched; production build passes and a live 5s run was verified end-to-end against the backend.

## Hackathon deliverables (per prof instructions)
- Working prototype: this repo (run it with Quick start above)
- GitHub repo: complete source + README + docs + project report — see `docs/PROJECT_REPORT.md`
- Presentation (PPT): problem, design, implementation, tech, testing, results, impact — see `docs/PRESENTATION.pptx`
- Live demo: start a run from the dashboard and show live metrics + STOP + history — see `docs/DEMO_SCRIPT.md`
- Team participation: team of 3 — all members contribute and can explain roles/concepts (contributions decided at the end)
- Testing & validation: see `docs/TESTING.md`
- Originality: original implementation, controlled environment, capped loads, no disruption to other users/processes

## Safety
Local lab tool — bind to `localhost` by default. All memory allocations are capped at 50% of total RAM. Never run unbounded stress on a machine with unsaved work.

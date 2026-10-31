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


pped at 50% of total RAM. Never run unbounded stress on a machine with unsaved work.

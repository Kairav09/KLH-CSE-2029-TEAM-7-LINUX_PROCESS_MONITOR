# ProcMonitor

## Linux Process Monitoring and Control System

ProcMonitor is an Operating Systems project that scans and manages running
Linux processes. It includes the original C implementation and a web
interface that exposes the same process-monitoring and control workflow.

The project demonstrates:

- Reading and parsing `/proc/<pid>/stat` and `/proc/<pid>/status`
- CPU usage calculation from two process snapshots
- Resident memory and process-owner detection
- Process sorting, filtering, and threshold warnings
- Safe process control using `SIGSTOP`, `SIGCONT`, and `SIGKILL`
- Permission and kernel-process checks before sending signals
- Timestamped success/failure activity logging

> The process-monitoring backend must run on Linux because it reads `/proc`.
> Windows users can run it inside WSL2/Ubuntu.

## Project layout

```text
os-project/
├── proc-monitor/       # Original modular C implementation
├── backend/            # Node.js + Express API
│   └── src/
│       ├── routes/
│       │   └── processes.js
│       └── server.js
├── frontend/           # React + Vite web interface
├── docs/               # Architecture, testing, report, and presentation
└── README.md
```

## Features

### Process scanner

The backend performs a two-snapshot scan approximately one second apart.
CPU usage is calculated from the change in process CPU ticks:

```text
CPU% = (delta process ticks / CLK_TCK) / elapsed seconds × 100
```

Each process includes its PID, parent PID, name, state, CPU percentage,
resident memory, owner, and control status.

### Process controls

The web interface supports:

- **Pause** — sends `SIGSTOP`
- **Resume** — sends `SIGCONT`
- **Kill** — sends `SIGKILL` after confirmation
- **New test process** — starts a controlled `yes > /dev/null` workload

The backend refuses unsafe operations for PID 1, kernel threads, processes
owned by another user, and its own server process.

### Web interface

The React frontend provides:

- Live process table sorted by CPU usage
- Search by process name, PID, or owner
- “Only mine” filtering
- CPU and memory threshold warnings
- Process detail and control actions
- Activity log display
- Automatic refresh every five seconds

## Requirements

- Linux, Ubuntu, or WSL2
- Node.js 18 or newer
- npm
- GCC (only required for the original C program)

## Run with WSL2 on Windows

Open an Ubuntu/WSL terminal, not Windows PowerShell:

```bash
cd /mnt/d/College/OS_Project/os-project/backend
npm install
npm run dev
```

Keep the backend terminal open. In a second Ubuntu/WSL terminal, start the
frontend:

```bash
cd /mnt/d/College/OS_Project/os-project/frontend
npm install
npm run dev
```

Open the displayed frontend URL, normally:

```text
http://localhost:5173
```

The backend health endpoint is:

```text
http://localhost:4000/api/health
```

If port 4000 is already in use, stop the other backend process before
starting the Linux/WSL backend.

## Run the original C program

From a Linux/WSL terminal:

```bash
cd /mnt/d/College/OS_Project/os-project/proc-monitor
gcc -Wall -Wextra final.c -o final
./final
```

The modular source files are also included:

```text
member1_scanner.c
member1_cpu_calc.c
member2_display.c
member2_threshold.c
member3_control.c
member3_logger.c
member3_main.c
process.h
```

## API

| Method | Endpoint | Purpose |
|---|---|---|
| `GET` | `/api/health` | Check whether the backend is running |
| `GET` | `/api/processes` | Scan and return Linux processes |
| `GET` | `/api/processes/activity` | Read the activity log |
| `POST` | `/api/processes/test-process` | Create a test workload |
| `POST` | `/api/processes/:pid/pause` | Send `SIGSTOP` |
| `POST` | `/api/processes/:pid/resume` | Send `SIGCONT` |
| `POST` | `/api/processes/:pid/kill` | Send `SIGKILL` |

## Logging

Process actions are written to `backend/activity.log` in this format:

```text
[YYYY-MM-DD HH:MM:SS] ACTION PID n -- SUCCESS
```

Runtime logs, databases, environment files, dependencies, and build output
are excluded by `.gitignore`.

## Documentation

- [Problem definition](docs/PROBLEM_DEFINITION.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Testing matrix](docs/TESTING.md)
- [Project report](docs/PROJECT_REPORT.md)
- [Demo script](docs/DEMO_SCRIPT.md)
- [Presentation](docs/PRESENTATION.pptx)

## Safety note

Do not run process-control actions with elevated privileges unless necessary.
Only control test processes or processes you own. Killing a system process can
make Linux unstable or terminate important services.

## Team

**KLH CSE 2029 — Team 7**

Project: **Linux Process Monitoring and Control System**

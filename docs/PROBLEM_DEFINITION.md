# Problem Definition — CPU and Memory Stress Testing Tool

## 1. Title
**CPU and Memory Stress Testing Tool** — A controlled workload generator that stresses CPU and memory and measures system performance under different loads.

## 2. Background
Every application — from a college project to a production API — eventually runs on finite hardware. Developers and system administrators need to answer questions like:

- How does my machine behave at 50% CPU vs 95% CPU?
- At what memory usage does the system start to slow down (thrashing / swapping)?
- How long can the system sustain a spike before latency degrades?
- What is the safe operating limit before we need more resources?

Today, answering these questions usually means running ad-hoc scripts, `stress-ng`, or prime-number loops in a terminal. Those tools work, but they are:
- Hard to control precisely (no intensity %, no pattern, no duration cap)
- Hard to observe (numbers scroll in a terminal, no live graphs, no history)
- Hard to compare (no saved runs, no A vs B)
- Risky for beginners (a runaway memory allocation can freeze the machine)

## 3. Problem Statement
There is no simple, safe, web-based tool that lets a user **generate a controlled CPU + memory workload, watch live system performance, and compare runs** — without risking a system freeze and without needing terminal expertise.

## 4. Objectives
1. **Controlled generation:** Let the user configure CPU workers, CPU intensity, memory size, memory pattern, duration, and load pattern (Sustained / Ramp / Spike / Step).
2. **Live measurement:** Sample CPU (total + per-core), memory (used / free / %), swap, load average, and workload throughput every second and stream it to the browser.
3. **Safety by design:** Hard caps, auto-stop on dangerous thresholds, one-click emergency STOP, and workers that can be killed cleanly.
4. **Comparison & reporting:** Save every run, show history, compare two runs side-by-side, export CSV.
5. **OS learning value:** Make OS concepts visible — saturation, queuing, paging/swapping, thrashing, and the difference between CPU-bound and memory-bound load.

## 5. Scope
**In scope (this project):**
- Web dashboard (React) + backend API (Node + Express)
- CPU stress via worker threads, memory stress via controlled allocations
- Real-time metrics via Server-Sent Events (SSE)
- Run history (SQLite), compare view, CSV export
- Safety limits + emergency stop

**Out of scope (deliberately):**
- Disk / network / GPU stress (future work)
- Distributed / multi-machine stress
- Kernel-level tracing (eBPF, perf)
- Authentication / multi-user accounts (single-user lab tool)

## 6. Target Users
- **Students / OS learners** — see scheduling, memory pressure, and thrashing in real time.
- **Developers / QA** — check how an app behaves when the machine is busy.
- **DevOps / hobbyists** — validate a laptop / VM / small server before deploying.

## 7. Success Criteria (how we know it works)
- A 30-second run at 50% CPU shows ~50% CPU on the dashboard, not 0% or 100%.
- A 512 MB memory run shows ~512 MB extra memory used, and frees it on STOP.
- STOP halts all workers within 2 seconds and memory returns to baseline.
- The system never freezes during a capped run; auto-stop triggers before swap thrashing.
- Two runs with different settings produce visibly different graphs and can be compared.

## 8. Why This Is Not Just Another Stress Tool
Existing CLI tools optimise for raw power. This tool optimises for **control + visibility + safety**:
- Patterns (Ramp / Spike) instead of just on/off
- Live per-core + memory graphs instead of terminal numbers
- Guardrails (caps, watchdog, STOP) instead of "hope you don't OOM"
- History + compare + export instead of one-off runs

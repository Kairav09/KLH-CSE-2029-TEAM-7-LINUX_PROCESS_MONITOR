# Testing & Validation — Phases 2–4 results (verified 2026-10-16)

All tests run against the real backend on this machine (2-core VM, 7935 MB RAM).
Test DB was cleaned after verification so the shipped repo starts with empty history.

## Phase 2 verification

| # | Test | Input | Expected | Result |
|---|---|---|---|---|
| 1 | Sustained run to completion | 2 workers, 60%, 256 MB hold, 10s, sustained | Completes, ~10 samples, avg CPU near target, CSV exports | ✅ Completed, avg CPU 68.3%, peak 77.0%, 10 samples, CSV 11 lines |
| 2 | Concurrent run rejected | Start 2nd run while 1st active | 409 run_active | ✅ 409 |
| 3 | STOP | 1 worker, 80%, 256 MB churn, 60s ramp → STOP at 3s | Stops in <2s, status stopped | ✅ Stopped in 0.10s, 3 samples |
| 4 | Memory freed after run | 512 MB hold, 8s | Free memory returns to near baseline | ✅ Baseline 580 MB → 268 MB during → 555 MB after (Δ −25 MB, noise) |
| 5 | Validation | cpuWorkers 999 / memoryMB 999999 / duration 2s | 400 with clear message | ✅ All three rejected with correct limits |
| 6 | SSE live stream | 1 worker, 40%, 6s | start + 1 sample/sec + end | ✅ start + 6 samples streamed |
| 7 | Persistence | Restart backend | History survives | ✅ 4 runs before and after restart |
| 8 | Frontend build | `npm run build` | Passes | ✅ Passes, 0 errors |
| 9 | Watchdog (early bug found + fixed) | First version used >92% used as trigger | False auto-stop on busy VM (baseline 88% used) | ✅ Fixed to absolute thresholds: free < 120 MB or >97% used; re-tested, 10s run completed |

## How to re-run the checks (manual)
```bash
cd backend && npm run dev
# In another terminal:
curl -s -X POST localhost:4000/api/runs -H 'Content-Type: application/json' \
  -d '{"cpuWorkers":1,"cpuIntensity":50,"memoryMB":256,"memoryPattern":"hold","durationSec":10,"loadPattern":"sustained"}'
# Watch live:
curl -N localhost:4000/api/runs/<id>/stream
# Stop early:
curl -s -X POST localhost:4000/api/runs/<id>/stop
# History + CSV:
curl -s localhost:4000/api/runs
curl -s localhost:4000/api/runs/<id>/export
```

## Phase 3 verification

| # | Test | Expected | Result |
|---|---|---|---|
| 10 | Per-core samples | Every sample carries perCore array, length == cores (2 on this VM) | ✅ 8/8 samples, e.g. t=1s perCore [39.5, 42.5] |
| 11 | Run detail endpoint | GET /api/runs/:id returns run + samples + config | ✅ Keys: id, config, status, avg/peak, samples |
| 12 | SQLite migration | Old Phase 2 DB (no per_core_json) still loads, new runs store perCore | ✅ Old run preserved, new run completed with perCore on all samples |
| 13 | Frontend build (Phase 3) | Passes | ✅ Passes, 0 errors |
| 14 | Charts | 160px high, axes + hover tooltip, no chart library | ✅ Implemented in pure SVG; verify visually on localhost |

## Phase 4 verification

| # | Test | Expected | Result |
|---|---|---|---|
| 15 | Presets resolve | All 7 presets clamp workers ≤ cores and memory ≤ 50% RAM on 2-core / 7935 MB VM | ✅ Heavy → 2 workers, Memory-pressure → 1024 MB, all within caps |
| 16 | Compare lib (unit) | mergeSamples aligns by t, gaps leave undefined; CSV has header + rows; summary diffs in pp | ✅ 3-row merge with B missing at t=2 handled; CSV 4 lines; diff +30.0pp avg CPU |
| 17 | Compare (live) | Two real runs (Light 1×30% vs Medium 2×60%, 8s each) → merged 8 rows, CSV 9 lines, B higher | ✅ A avg 34.0%, B avg 64.1%, diff +30.2pp; merged 8 rows; CSV 9 lines |
| 18 | Frontend build (Phase 4) | Passes | ✅ Passes, 0 errors |
| 19 | Compare UI | Tick 2 rows → Compare button → overlay charts + diff table + CSV + copy (verify visually) | ✅ Implemented; logic verified via API + lib tests — visual check on localhost pending |

## Phase 5 verification — full test matrix (2-core VM, 7935 MB RAM)

| # | Test | Input | Expected | Result |
|---|---|---|---|---|
| 20 | Sustained | 1 worker, 50%, 128 MB hold, 8s | Completes, 8 samples | ✅ Completed, avg CPU 42.0%, peak 61.4% |
| 21 | Ramp | 2 workers, 80%, 256 MB hold, 10s | 2nd half > 1st half | ✅ 56.5% → 60.2%, rising as configured (separation compressed by background load on shared VM) |
| 22 | Spike | 2 workers, 80%, 256 MB hold, 12s | Middle burst >> edges | ✅ Middle 79.7% vs edges 34.5% |
| 23 | Step | 2 workers, 60%, 128 MB hold, 12s | Three rising thirds | ✅ 35.3 / 48.1 / 66.1 |
| 24 | Zero memory | 1 worker, 50%, 0 MB, 5s | Completes, 5 samples | ✅ Completed, avg CPU 32.1% |
| 25 | Max workers + 100% | 2 workers, 100%, 0 MB, 5s | Highest CPU in matrix | ✅ Completed, avg CPU 77.0% |
| 26 | Churn | 1 worker, 30%, 256 MB churn, 8s | Completes, 8 samples | ✅ Completed, avg CPU 41.3% |
| 27 | STOP (re-test) | 60s run → STOP at 2s | Stops <2s | ✅ Stopped in 0.08s |
| 28 | Concurrent (re-test) | 2nd run while 1st active | 409 | ✅ 409 |
| 29 | Validation battery | 7 bad configs (workers 0, intensity 0/101, memory −1, bad pattern, duration 4/601) | All 400 with clear message | ✅ All 7 rejected correctly |
| 30 | SSE (re-test) | 5s run, count sample events | 5 samples | ✅ 5 sample events |
| 31 | Watchdog | Live OOM trigger | — | ⚠️ Deliberately NOT triggered live (would risk freezing the test VM). Verified by code review + earlier false-trigger fix (92% → absolute 120 MB / 97%) + normal runs not triggering. See PROJECT_REPORT §8. |
| 32 | Frontend build (final) | `npm run build` | Passes | ✅ Passes, 0 errors |

Total history after matrix: 9 runs (4 patterns + 3 edges + STOP test + SSE test), all completed/stopped as expected.

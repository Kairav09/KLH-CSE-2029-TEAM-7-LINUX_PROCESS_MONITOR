# Linux Process Monitoring and Control System

A C systems programming project (Project #18) that scans `/proc`, computes
live CPU% and memory usage per process, displays a `top`-style table, flags
processes over configurable thresholds, and lets the user pause, resume, or
kill a process — all logged with timestamps.

## Features

- **Process scanning** — walks `/proc/<pid>` for every running process,
  parsing PID, PPID, state, and memory (`VmRSS`) from `/proc/<pid>/stat` and
  `/proc/<pid>/status`.
- **Live CPU% calculation** — takes two time-spaced snapshots of cumulative
  CPU ticks and derives a percentage, the same approach tools like `top` use.
- **Sorted table display** — processes ranked by CPU% descending.
- **Threshold alerts** — flags any process exceeding a configurable CPU% or
  memory limit.
- **Process control** — pause (`SIGSTOP`), resume (`SIGCONT`), or kill
  (`SIGKILL`) any process by PID from an interactive menu.
- **Activity logging** — every control action is appended to `activity.log`
  with a timestamp and success/failure result.

## Project Structure

```
proc_monitor/
├── process.h              # shared struct Process + all function declarations
├── member1_scanner.c       # /proc scanning: PID, PPID, state, memory
├── member1_cpu_calc.c       # CPU% calculation from two snapshots
├── member1_scanner_test.c    # standalone test main() for Member 1's module
├── member2_display.c        # sorts + prints the process table
├── member2_threshold.c       # flags processes over CPU/memory limits
├── member3_control.c        # pause / resume / kill via signals
├── member3_logger.c          # timestamped activity logging
├── member3_main.c            # integrated main() tying everything together
└── final.c                  # all of the above combined into one file,
                              # for quick single-file build/testing in an IDE
```

## Requirements

- Linux (relies on `/proc`; will not run on macOS or Windows)
- `gcc` (or any C99-compatible compiler)

## Build & Run

**Full modular build (recommended / the real submission):**

```bash
gcc -Wall member1_scanner.c member1_cpu_calc.c \
    member2_display.c member2_threshold.c \
    member3_control.c member3_logger.c member3_main.c \
    -o proc_monitor
./proc_monitor
```

**Single-file build (quick test / IDE run):**

```bash
gcc -Wall final.c -o final
./final
```

**Member 1 module only (standalone test):**

```bash
gcc -Wall member1_scanner.c member1_cpu_calc.c member1_scanner_test.c -o test1
./test1
```

## Usage

On launch, the program takes an initial snapshot and prints the process
table. From the menu you can:

| Key | Action |
|-----|--------|
| `r` | Refresh — take a new snapshot and redisplay the table |
| `p` | Pause a process by PID (`SIGSTOP`) |
| `c` | Resume a paused process by PID (`SIGCONT`) |
| `k` | Kill a process by PID (`SIGKILL`) |
| `q` | Quit |

Thresholds default to **CPU > 50%** and **memory > 500,000 kB**, set as
`CPU_LIMIT_PERCENT` and `MEM_LIMIT_KB` near the top of `member3_main.c` (or
`final.c`) — edit and rebuild to change them.

Note: pausing, resuming, or killing a process you don't own will fail with a
permission error unless the program is run with elevated privileges (`sudo`).

## How CPU% Is Calculated

`/proc/<pid>/stat` only exposes **cumulative** CPU ticks used since the
process started — not a live percentage. To get a percentage:

1. Snapshot ticks at time T1
2. Wait ~1 second
3. Snapshot ticks at time T2
4. `delta_ticks = ticks(T2) - ticks(T1)`
5. `cpu_percent = (delta_ticks / CLK_TCK) / elapsed_seconds * 100`

`CLK_TCK` (ticks per second, typically 100 on Linux) is read via
`sysconf(_SC_CLK_TCK)`.

## Team / Module Breakdown

| Member | Responsibility | Files |
|--------|----------------|-------|
| Member 1 | Process scanning + CPU% calculation | `member1_scanner.c`, `member1_cpu_calc.c` |
| Member 2 | Display + threshold checking | `member2_display.c`, `member2_threshold.c` |
| Member 3 | Process control + logging + integration | `member3_control.c`, `member3_logger.c`, `member3_main.c` |

## License

Educational project — no license restrictions.

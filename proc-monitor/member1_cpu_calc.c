/*
 * member1_cpu_calc.c
 * -------------------------------------------------------
 * MEMBER 1's PART B -- CPU% calculation from two time-spaced
 *                       /proc snapshots.
 * -------------------------------------------------------
 */

#include <unistd.h>   // sysconf()
#include "process.h"

/*
 * Find a process by PID within an already-scanned array.
 * Returns a pointer to it, or NULL if not found.
 */
struct Process *find_by_pid(struct Process procs[], int count, int pid) {
    for (int i = 0; i < count; i++) {
        if (procs[i].pid == pid) return &procs[i];
    }
    return NULL;
}

/*
 * /proc gives CUMULATIVE CPU ticks, not a live percentage. So:
 *   1. snapshot ticks at T1
 *   2. snapshot ticks at T2
 *   3. delta_ticks = ticks(T2) - ticks(T1)
 *   4. cpu_percent = (delta_ticks / CLK_TCK) / elapsed_seconds * 100
 */
void compute_cpu_percent(struct Process prev[], int prev_count,
                          struct Process curr[], int curr_count,
                          double elapsed_seconds) {
    long clk_tck = sysconf(_SC_CLK_TCK); // ticks per second, e.g. 100

    for (int i = 0; i < curr_count; i++) {
        struct Process *old = find_by_pid(prev, prev_count, curr[i].pid);

        if (old == NULL) {
            // New process, no baseline yet -- report 0% this cycle.
            curr[i].cpu_percent = 0.0;
            continue;
        }

        unsigned long delta_ticks =
            (curr[i].utime + curr[i].stime) - (old->utime + old->stime);

        double delta_cpu_seconds = (double)delta_ticks / (double)clk_tck;

        curr[i].cpu_percent = (delta_cpu_seconds / elapsed_seconds) * 100.0;
    }
}

/*
 * member2_threshold.c
 * -------------------------------------------------------
 * MEMBER 2's PART B -- flagging processes that exceed CPU or
 *                       memory limits.
 * -------------------------------------------------------
 */

#include <stdio.h>
#include "process.h"

/*
 * Walk the process list and print a warning for any process
 * whose CPU% or memory exceeds the given limits.
 *
 * Returns the number of processes flagged, so the caller (Member
 * 3's main loop) can decide whether to act on any of them.
 */
int check_thresholds(struct Process procs[], int count,
                      double cpu_limit, long mem_limit_kb) {
    int flagged = 0;

    for (int i = 0; i < count; i++) {
        int over_cpu = procs[i].cpu_percent > cpu_limit;
        int over_mem = procs[i].mem_kb > mem_limit_kb;

        if (over_cpu || over_mem) {
            printf("[WARNING] PID %d exceeds limit -- CPU: %.2f%% "
                   "(limit %.2f%%), MEM: %ld kB (limit %ld kB)\n",
                   procs[i].pid, procs[i].cpu_percent, cpu_limit,
                   procs[i].mem_kb, mem_limit_kb);
            flagged++;
        }
    }

    return flagged;
}

/*
 * member1_scanner_test.c
 * -------------------------------------------------------
 * Standalone demo/test main() so Member 1's module can be run
 * and checked on its own. NOT part of the final integrated
 * program -- member3_main.c has the real main().
 *
 * Build:  gcc -Wall member1_scanner.c member1_cpu_calc.c member1_scanner_test.c -o test1
 * Run:    ./test1
 * -------------------------------------------------------
 */

#include <stdio.h>
#include <unistd.h>   // sleep()
#include "process.h"

int main(void) {
    struct Process prev_scan[MAX_PROCESSES];
    struct Process curr_scan[MAX_PROCESSES];

    printf("Taking first snapshot...\n");
    int prev_count = scan_processes(prev_scan, MAX_PROCESSES);

    sleep(1); // wait 1 second before the second snapshot

    printf("Taking second snapshot...\n");
    int curr_count = scan_processes(curr_scan, MAX_PROCESSES);

    compute_cpu_percent(prev_scan, prev_count, curr_scan, curr_count, 1.0);

    printf("\n%-8s %-6s %-6s %-10s %-8s\n",
           "PID", "PPID", "STATE", "MEM(kB)", "CPU%");
    printf("--------------------------------------------\n");

    for (int i = 0; i < curr_count; i++) {
        printf("%-8d %-6d %-6c %-10ld %-8.2f\n",
               curr_scan[i].pid, curr_scan[i].ppid, curr_scan[i].state,
               curr_scan[i].mem_kb, curr_scan[i].cpu_percent);
    }

    return 0;
}

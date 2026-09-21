/*
 * member3_main.c
 * -------------------------------------------------------
 * MEMBER 3's PART C -- the real integrated main() for
 *                       Project #18: Linux Process Monitoring
 *                       and Control System.
 *
 * Flow each cycle:
 *   1. Take two /proc snapshots ~1 second apart (Member 1)
 *   2. Compute CPU% from the delta (Member 1)
 *   3. Sort + display the table (Member 2)
 *   4. Flag any process over the CPU/MEM thresholds (Member 2)
 *   5. Let the user pause/resume/kill a PID, or refresh, or quit
 *      (Member 3 control + logger)
 *
 * Build (from the proc_monitor folder):
 *   gcc -Wall member1_scanner.c member1_cpu_calc.c \
 *       member2_display.c member2_threshold.c \
 *       member3_control.c member3_logger.c member3_main.c \
 *       -o proc_monitor
 * Run:
 *   ./proc_monitor
 * -------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "process.h"

#define CPU_LIMIT_PERCENT 50.0
#define MEM_LIMIT_KB      500000  // ~500 MB, tweak as needed

/* Take two snapshots ~1s apart and fill curr[] with CPU% computed. */
static int refresh_snapshot(struct Process curr[]) {
    static struct Process prev[MAX_PROCESSES];
    static int prev_count = 0;
    static int have_prev = 0;

    struct Process first[MAX_PROCESSES];
    int first_count = scan_processes(first, MAX_PROCESSES);
    sleep(1);
    int curr_count = scan_processes(curr, MAX_PROCESSES);

    // Prefer the snapshot taken 1 second ago (either the very first
    // "first" scan on cycle 1, or the previous cycle's "curr" after
    // that) as the baseline for computing CPU%.
    if (have_prev) {
        compute_cpu_percent(prev, prev_count, curr, curr_count, 1.0);
    } else {
        compute_cpu_percent(first, first_count, curr, curr_count, 1.0);
    }

    // Save this cycle's snapshot as next cycle's baseline.
    for (int i = 0; i < curr_count; i++) prev[i] = curr[i];
    prev_count = curr_count;
    have_prev = 1;

    return curr_count;
}

static void print_menu(void) {
    printf("\nOptions: [r] refresh  [p] pause PID  [c] resume(cont) PID  "
           "[k] kill PID  [q] quit\n> ");
}

int main(void) {
    log_init("activity.log");

    struct Process curr[MAX_PROCESSES];
    char choice;
    int pid;
    int running = 1;

    printf("Linux Process Monitoring and Control System\n");
    printf("(thresholds: CPU > %.1f%%, MEM > %ld kB)\n",
           CPU_LIMIT_PERCENT, (long)MEM_LIMIT_KB);

    int count = refresh_snapshot(curr);
    sort_by_cpu_desc(curr, count);
    display_table(curr, count);
    check_thresholds(curr, count, CPU_LIMIT_PERCENT, MEM_LIMIT_KB);

    while (running) {
        print_menu();

        if (scanf(" %c", &choice) != 1) break;

        int result;
        switch (choice) {
            case 'r':
                count = refresh_snapshot(curr);
                sort_by_cpu_desc(curr, count);
                display_table(curr, count);
                check_thresholds(curr, count, CPU_LIMIT_PERCENT, MEM_LIMIT_KB);
                break;

            case 'p':
                printf("PID to pause: ");
                scanf("%d", &pid);
                result = pause_process(pid);
                log_action("PAUSE", pid, result);
                break;

            case 'c':
                printf("PID to resume: ");
                scanf("%d", &pid);
                result = resume_process(pid);
                log_action("RESUME", pid, result);
                break;

            case 'k':
                printf("PID to kill: ");
                scanf("%d", &pid);
                result = kill_process(pid);
                log_action("KILL", pid, result);
                break;

            case 'q':
                running = 0;
                break;

            default:
                printf("Unknown option '%c'.\n", choice);
        }
    }

    printf("Exiting. See activity.log for the action history.\n");
    return 0;
}

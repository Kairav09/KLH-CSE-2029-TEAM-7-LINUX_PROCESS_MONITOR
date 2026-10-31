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
 *   5. Let the user pause/resume/kill a PID, create a test process,
 *      toggle the "only mine" filter, refresh, or quit (Member 3)
 *
 * Before sending any signal we check whether the process can be
 * controlled at all, and explain why not instead of just failing.
 *
 * Build (from the proc_monitor folder):
 *   gcc -Wall member1_scanner.c member1_cpu_calc.c \
 *       member2_display.c member2_threshold.c \
 *       member3_control.c member3_logger.c member3_main.c \
 *       -o proc_monitor
 * -------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "process.h"

#define CPU_LIMIT_PERCENT 50.0
#define MEM_LIMIT_KB      500000  // ~500 MB, tweak as needed

/* Take two snapshots ~1s apart and fill curr[] with CPU% computed. */
static int refresh_snapshot(struct Process curr[]) {
    reap_children();   // clear zombies from killed test processes

    // Baseline snapshot, then a short wait, then the "current" snapshot.
    // CPU% = ticks used BETWEEN these two scans / real elapsed time.
    struct Process first[MAX_PROCESSES];
    struct timespec t1, t2;

    clock_gettime(CLOCK_MONOTONIC, &t1);
    int first_count = scan_processes(first, MAX_PROCESSES);
    sleep(1);
    int curr_count = scan_processes(curr, MAX_PROCESSES);
    clock_gettime(CLOCK_MONOTONIC, &t2);

    double elapsed = (t2.tv_sec - t1.tv_sec) + (t2.tv_nsec - t1.tv_nsec) / 1e9;
    compute_cpu_percent(first, first_count, curr, curr_count, elapsed);

    return curr_count;
}

static void print_menu(void) {
    printf("\nOptions: [r] refresh  [p] pause PID  [c] resume(cont) PID  "
           "[k] kill PID  [n] new test process  [f] filter: only mine  [q] quit\n> ");
    fflush(stdout);
}

/* Read a PID from the user. Returns 1 on success, 0 on bad input. */
static int read_pid(const char *prompt, int *pid) {
    printf("%s", prompt);
    fflush(stdout);
    if (scanf("%d", pid) == 1) return 1;

    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) { }  // throw away the bad line
    printf("That's not a valid PID.\n");
    return 0;
}

/*
 * Run a control action (pause/resume/kill) on a PID -- but first look the
 * process up in the latest scan. If we already know we aren't allowed to
 * control it, explain why and skip the signal instead of letting the kernel
 * reject it with a bare "Operation not permitted".
 */
static void control_with_check(const char *action_name, int (*action)(int),
                                int pid, struct Process list[], int count) {
    struct Process *p = find_by_pid(list, count, pid);

    if (p != NULL && control_status(p) != CTRL_YES) {
        char why[512];
        explain_control(p, why, sizeof(why));
        printf("[BLOCKED] %s\n", why);
        log_action(action_name, pid, -1);
        return;
    }

    int result = action(pid);   // if p == NULL, let the kernel decide
    log_action(action_name, pid, result);
}

int main(void) {
    log_init("activity.log");

    struct Process curr[MAX_PROCESSES];
    char choice;
    int pid;
    int running = 1;
    int only_controllable = 0;

    printf("Linux Process Monitoring and Control System\n");
    printf("(thresholds: CPU > %.1f%%, MEM > %ld kB)\n",
           CPU_LIMIT_PERCENT, (long)MEM_LIMIT_KB);

    int count = refresh_snapshot(curr);
    sort_by_cpu_desc(curr, count);
    display_table(curr, count, only_controllable);
    check_thresholds(curr, count, CPU_LIMIT_PERCENT, MEM_LIMIT_KB);

    while (running) {
        print_menu();

        if (scanf(" %c", &choice) != 1) break;

        switch (choice) {
            case 'r':
                count = refresh_snapshot(curr);
                sort_by_cpu_desc(curr, count);
                display_table(curr, count, only_controllable);
                check_thresholds(curr, count, CPU_LIMIT_PERCENT, MEM_LIMIT_KB);
                break;

            case 'p':
                if (read_pid("PID to pause: ", &pid))
                    control_with_check("PAUSE", pause_process, pid, curr, count);
                break;

            case 'c':
                if (read_pid("PID to resume: ", &pid))
                    control_with_check("RESUME", resume_process, pid, curr, count);
                break;

            case 'k':
                if (read_pid("PID to kill: ", &pid))
                    control_with_check("KILL", kill_process, pid, curr, count);
                break;

            case 'n': {
                pid_t child = create_test_process();
                if (child > 0) {
                    printf("Created test process PID %d (burning CPU). "
                           "Press 'r' to see it.\n", (int)child);
                    log_action("CREATE", (int)child, 0);
                } else {
                    log_action("CREATE", 0, -1);
                }
                break;
            }

            case 'f':
                only_controllable = !only_controllable;
                printf("Filter %s. Press 'r' to refresh the table.\n",
                       only_controllable ? "ON (only processes you can control)"
                                         : "OFF (all processes)");
                break;

            case 'q':
                running = 0;
                break;

            default:
                printf("Unknown option '%c'.\n", choice);
        }
    }

    cleanup_test_processes();
    printf("Exiting. See activity.log for the action history.\n");
    return 0;
}

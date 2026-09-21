/*
 * final.c
 * -------------------------------------------------------
 * Linux Process Monitoring and Control System (Project #18)
 * ALL-IN-ONE FILE -- everything from process.h + all three
 * members' .c files combined, so this single file can be
 * opened and run directly in an IDE (no multi-file build
 * config needed).
 *
 * The team's real submission should still be the separate
 * files (process.h, member1_*.c, member2_*.c, member3_*.c)
 * since that's what the rubric wants to see for "modular
 * design" -- this file is just a convenience copy for
 * quick testing/demo in one click.
 *
 * Build:  gcc -Wall final.c -o final
 * Run:    ./final
 * -------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

#define MAX_PROCESSES 1024
#define CPU_LIMIT_PERCENT 50.0
#define MEM_LIMIT_KB      500000  // ~500 MB, tweak as needed

/* ===================================================================
 * SHARED DATA MODEL  (was process.h)
 * =================================================================== */
struct Process {
    int pid;
    int ppid;
    char state;
    long mem_kb;
    unsigned long utime;
    unsigned long stime;
    double cpu_percent;
};

/* ===================================================================
 * MEMBER 1 -- scanning /proc  (was member1_scanner.c)
 * =================================================================== */
int is_all_digits(const char *s) {
    if (*s == '\0') return 0;
    while (*s) {
        if (!isdigit((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

int parse_stat(int pid, struct Process *p) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    FILE *fp = fopen(path, "r");
    if (fp == NULL) return -1;

    char line[1024];
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    char *close_paren = strrchr(line, ')');
    if (!close_paren) return -1;

    p->pid = pid;

    char *rest = close_paren + 2;
    char *token;
    int field = 0;

    token = strtok(rest, " ");
    while (token != NULL) {
        field++;
        switch (field) {
            case 1: p->state = token[0]; break;
            case 2: p->ppid = atoi(token); break;
            case 12: p->utime = strtoul(token, NULL, 10); break;
            case 13: p->stime = strtoul(token, NULL, 10); break;
        }
        if (field >= 13) break;
        token = strtok(NULL, " ");
    }

    return 0;
}

int parse_status_mem(int pid, struct Process *p) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);

    FILE *fp = fopen(path, "r");
    if (fp == NULL) return -1;

    char line[256];
    p->mem_kb = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%ld", &p->mem_kb);
            break;
        }
    }

    fclose(fp);
    return 0;
}

int scan_processes(struct Process procs[], int max_count) {
    DIR *dir = opendir("/proc");
    if (dir == NULL) {
        perror("opendir");
        return 0;
    }

    int count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL && count < max_count) {
        if (!is_all_digits(entry->d_name)) continue;

        int pid = atoi(entry->d_name);
        struct Process p;
        memset(&p, 0, sizeof(p));

        if (parse_stat(pid, &p) != 0) continue;
        parse_status_mem(pid, &p);

        procs[count] = p;
        count++;
    }

    closedir(dir);
    return count;
}

/* ===================================================================
 * MEMBER 1 -- CPU% calculation  (was member1_cpu_calc.c)
 * =================================================================== */
struct Process *find_by_pid(struct Process procs[], int count, int pid) {
    for (int i = 0; i < count; i++) {
        if (procs[i].pid == pid) return &procs[i];
    }
    return NULL;
}

void compute_cpu_percent(struct Process prev[], int prev_count,
                          struct Process curr[], int curr_count,
                          double elapsed_seconds) {
    long clk_tck = sysconf(_SC_CLK_TCK);

    for (int i = 0; i < curr_count; i++) {
        struct Process *old = find_by_pid(prev, prev_count, curr[i].pid);

        if (old == NULL) {
            curr[i].cpu_percent = 0.0;
            continue;
        }

        unsigned long delta_ticks =
            (curr[i].utime + curr[i].stime) - (old->utime + old->stime);

        double delta_cpu_seconds = (double)delta_ticks / (double)clk_tck;

        curr[i].cpu_percent = (delta_cpu_seconds / elapsed_seconds) * 100.0;
    }
}

/* ===================================================================
 * MEMBER 2 -- sorting + display  (was member2_display.c)
 * =================================================================== */
static int compare_cpu_desc(const void *a, const void *b) {
    const struct Process *pa = (const struct Process *)a;
    const struct Process *pb = (const struct Process *)b;

    if (pa->cpu_percent < pb->cpu_percent) return 1;
    if (pa->cpu_percent > pb->cpu_percent) return -1;
    return 0;
}

void sort_by_cpu_desc(struct Process procs[], int count) {
    qsort(procs, count, sizeof(struct Process), compare_cpu_desc);
}

void display_table(struct Process procs[], int count) {
    printf("\n%-8s %-6s %-6s %-10s %-8s\n",
           "PID", "PPID", "STATE", "MEM(kB)", "CPU%");
    printf("--------------------------------------------\n");

    for (int i = 0; i < count; i++) {
        printf("%-8d %-6d %-6c %-10ld %-8.2f\n",
               procs[i].pid, procs[i].ppid, procs[i].state,
               procs[i].mem_kb, procs[i].cpu_percent);
    }
    printf("--------------------------------------------\n");
    printf("Total processes: %d\n", count);
}

/* ===================================================================
 * MEMBER 2 -- thresholds  (was member2_threshold.c)
 * =================================================================== */
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

/* ===================================================================
 * MEMBER 3 -- control  (was member3_control.c)
 * =================================================================== */
int pause_process(int pid) {
    if (kill(pid, SIGSTOP) == -1) {
        fprintf(stderr, "Could not pause PID %d: %s\n", pid, strerror(errno));
        return -1;
    }
    printf("PID %d paused (SIGSTOP).\n", pid);
    return 0;
}

int resume_process(int pid) {
    if (kill(pid, SIGCONT) == -1) {
        fprintf(stderr, "Could not resume PID %d: %s\n", pid, strerror(errno));
        return -1;
    }
    printf("PID %d resumed (SIGCONT).\n", pid);
    return 0;
}

int kill_process(int pid) {
    if (kill(pid, SIGKILL) == -1) {
        fprintf(stderr, "Could not kill PID %d: %s\n", pid, strerror(errno));
        return -1;
    }
    printf("PID %d killed (SIGKILL).\n", pid);
    return 0;
}

/* ===================================================================
 * MEMBER 3 -- logger  (was member3_logger.c)
 * =================================================================== */
static char log_path[256] = "activity.log";

void log_init(const char *path) {
    strncpy(log_path, path, sizeof(log_path) - 1);
    log_path[sizeof(log_path) - 1] = '\0';
}

void log_action(const char *action, int pid, int result) {
    FILE *fp = fopen(log_path, "a");
    if (fp == NULL) {
        perror("log_action: fopen");
        return;
    }

    time_t now = time(NULL);
    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", localtime(&now));

    fprintf(fp, "[%s] %s PID %d -- %s\n",
            timebuf, action, pid, result == 0 ? "SUCCESS" : "FAILED");

    fclose(fp);
}

/* ===================================================================
 * MEMBER 3 -- integrated main()  (was member3_main.c)
 * =================================================================== */
static int refresh_snapshot(struct Process curr[]) {
    static struct Process prev[MAX_PROCESSES];
    static int prev_count = 0;
    static int have_prev = 0;

    struct Process first[MAX_PROCESSES];
    int first_count = scan_processes(first, MAX_PROCESSES);
    sleep(1);
    int curr_count = scan_processes(curr, MAX_PROCESSES);

    if (have_prev) {
        compute_cpu_percent(prev, prev_count, curr, curr_count, 1.0);
    } else {
        compute_cpu_percent(first, first_count, curr, curr_count, 1.0);
    }

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

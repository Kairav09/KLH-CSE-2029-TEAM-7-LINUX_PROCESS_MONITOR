/*
 * final.c
 * -------------------------------------------------------
 * Linux Process Monitoring and Control System (Project #18)
 * ALL-IN-ONE FILE -- process.h + all three members' .c files
 * combined, so this single file can be built and run directly:
 *
 *     gcc -Wall final.c -o final
 *     ./final
 *
 * The team's real submission is still the separate files
 * (process.h, member1_*.c, member2_*.c, member3_*.c) for the
 * "modular design" criterion; this is a convenience copy.
 * -------------------------------------------------------
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* ===================================================================
 * SHARED HEADER  (was process.h)
 * =================================================================== */
/*
 * process.h
 * -------------------------------------------------------
 * SHARED HEADER -- Project #18: Linux Process Monitoring
 *                  and Control System
 *
 * Every .c file in the project #includes this one. It's the
 * "contract" between all three members: the struct fields and
 * function signatures here are what everyone codes against.
 *
 * JAVA NOTE: A .h file is like a Java interface + field list
 * combined -- it declares WHAT exists (struct shape, function
 * signatures) but not HOW functions work. The "how" (the body)
 * lives in the matching .c file.
 * -------------------------------------------------------
 */

#ifndef PROCESS_H
#define PROCESS_H


#define MAX_PROCESSES 1024
#define NAME_LEN 32
#define USER_LEN 32

// Bit set in field 9 of /proc/<pid>/stat for kernel threads (PF_KTHREAD).
#define PF_KTHREAD_FLAG 0x00200000UL

/* ---------------- shared data model ---------------- */
struct Process {
    int pid;
    int ppid;
    char state;              // 'R' running, 'S' sleeping, 'T' stopped, 'Z' zombie...
    long mem_kb;              // VmRSS -- resident memory in kilobytes
    unsigned long utime;       // CPU ticks, user mode (raw, cumulative)
    unsigned long stime;       // CPU ticks, kernel mode (raw, cumulative)
    double cpu_percent;        // computed -- % of one CPU core since last scan
    char name[NAME_LEN];       // command name, e.g. "bash" (kernel truncates to 15 chars)
    uid_t uid;                 // user ID of the process's owner
    char owner[USER_LEN];      // owner's username (or the number if unknown)
    int is_kernel_thread;      // 1 if the kernel marks it as a kernel thread
};

/* Can the person running this program send signals to a given process? */
enum ControlStatus {
    CTRL_YES,          // you own it (or you are root)
    CTRL_NO_OWNER,     // owned by another user -- Linux denies the signal
    CTRL_KERNEL,       // kernel thread -- kernel ignores SIGSTOP/SIGKILL
    CTRL_INIT          // PID 1 -- kernel protects it
};

/* ---------------- Member 1: scanning ---------------- */
int is_all_digits(const char *s);
int parse_stat(int pid, struct Process *p);
int parse_status(int pid, struct Process *p);
int scan_processes(struct Process procs[], int max_count);

/* ---------------- Member 1: cpu calc ---------------- */
struct Process *find_by_pid(struct Process procs[], int count, int pid);
void compute_cpu_percent(struct Process prev[], int prev_count,
                          struct Process curr[], int curr_count,
                          double elapsed_seconds);

/* ---------------- Member 2: display ---------------- */
void sort_by_cpu_desc(struct Process procs[], int count);
void display_table(struct Process procs[], int count, int only_controllable);

/* ---------------- Member 2: thresholds ---------------- */
int check_thresholds(struct Process procs[], int count,
                      double cpu_limit, long mem_limit_kb);

/* ---------------- Member 3: control ---------------- */
enum ControlStatus control_status(const struct Process *p);
const char *control_label(enum ControlStatus s);
void explain_control(const struct Process *p, char *buf, size_t buf_size);
int pause_process(int pid);
int resume_process(int pid);
int kill_process(int pid);
pid_t create_test_process(void);
void reap_children(void);
void cleanup_test_processes(void);

/* ---------------- Member 3: logger ---------------- */
void log_init(const char *path);
void log_action(const char *action, int pid, int result);

#endif // PROCESS_H

/* ===================================================================
 * was member1_scanner.c
 * =================================================================== */
/*
 * member1_scanner.c
 * -------------------------------------------------------
 * MEMBER 1's PART A -- scanning /proc for PID, PPID, STATE,
 *                       name, owner, and memory (VmRSS).
 * -------------------------------------------------------
 */


int is_all_digits(const char *s) {
    if (*s == '\0') return 0;
    while (*s) {
        if (!isdigit((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

/*
 * Parse /proc/<pid>/stat for NAME, STATE, PPID, kernel-thread flag,
 * UTIME and STIME.
 *
 * The line looks like:  1234 (my prog) S 1 ... flags ... utime stime ...
 * The name sits inside parentheses and can itself contain spaces or
 * ')' characters, so we split at the LAST ')'. Fields after it, counted
 * from STATE=1:  PPID=2, FLAGS=7, UTIME=12, STIME=13.
 */
int parse_stat(int pid, struct Process *p) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    FILE *fp = fopen(path, "r");
    if (fp == NULL) return -1; // process may have exited -- not a bug

    char line[1024];
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    char *open_paren = strchr(line, '(');
    char *close_paren = strrchr(line, ')');
    if (!open_paren || !close_paren || close_paren < open_paren) return -1;

    p->pid = pid;

    // command name = text between the first '(' and the last ')'
    size_t name_len = (size_t)(close_paren - open_paren - 1);
    if (name_len >= NAME_LEN) name_len = NAME_LEN - 1;
    memcpy(p->name, open_paren + 1, name_len);
    p->name[name_len] = '\0';

    char *rest = close_paren + 2; // skip ") "
    char *token;
    int field = 0;

    token = strtok(rest, " ");
    while (token != NULL) {
        field++;
        switch (field) {
            case 1: p->state = token[0]; break;
            case 2: p->ppid = atoi(token); break;
            case 7: p->is_kernel_thread =
                        (strtoul(token, NULL, 10) & PF_KTHREAD_FLAG) != 0;
                    break;
            case 12: p->utime = strtoul(token, NULL, 10); break;
            case 13: p->stime = strtoul(token, NULL, 10); break;
        }
        if (field >= 13) break;
        token = strtok(NULL, " ");
    }

    return 0;
}

/*
 * Parse /proc/<pid>/status for VmRSS (resident memory in kB) and the
 * owner's user ID ("Uid:" line, first number = real UID). This file is
 * "Label:\tvalue" per line, much easier to parse than /stat.
 */
int parse_status(int pid, struct Process *p) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);

    FILE *fp = fopen(path, "r");
    if (fp == NULL) return -1;

    char line[256];
    p->mem_kb = 0; // default if VmRSS line is missing (e.g. kernel threads)

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%ld", &p->mem_kb);
        } else if (strncmp(line, "Uid:", 4) == 0) {
            unsigned int uid = 0;
            sscanf(line + 4, "%u", &uid);
            p->uid = (uid_t)uid;
        }
    }
    fclose(fp);

    // Turn the numeric UID into a username (e.g. 0 -> "root").
    struct passwd *pw = getpwuid(p->uid);
    if (pw != NULL) {
        snprintf(p->owner, sizeof(p->owner), "%s", pw->pw_name);
    } else {
        snprintf(p->owner, sizeof(p->owner), "%u", (unsigned)p->uid);
    }
    return 0;
}

/*
 * Scan all of /proc once, filling the given array.
 * Returns the number of processes found.
 */
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
        memset(&p, 0, sizeof(p)); // C does not zero-init structs like Java does

        if (parse_stat(pid, &p) != 0) continue;   // process vanished, skip
        if (parse_status(pid, &p) != 0) continue; // vanished between reads
        procs[count] = p;
        count++;
    }

    closedir(dir);
    return count;
}

/* ===================================================================
 * was member1_cpu_calc.c
 * =================================================================== */
/*
 * member1_cpu_calc.c
 * -------------------------------------------------------
 * MEMBER 1's PART B -- CPU% calculation from two time-spaced
 *                       /proc snapshots.
 * -------------------------------------------------------
 */


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

/* ===================================================================
 * was member3_control.c
 * =================================================================== */
/*
 * member3_control.c
 * -------------------------------------------------------
 * MEMBER 3's PART A -- controlling processes via signals, and
 *                       explaining WHY a process can't be controlled.
 *
 * JAVA NOTE: there's no equivalent of Unix signals in plain Java.
 * kill() is like calling a method on another running program from
 * outside it, except the "method" is a numbered signal (SIGSTOP,
 * SIGCONT, SIGKILL) and the OS decides whether it's allowed.
 * -------------------------------------------------------
 */


/* ===================================================================
 * Can we control this process? (checked BEFORE sending any signal)
 * =================================================================== */

/*
 * Linux rules for sending a signal:
 *   - You may signal a process only if you own it (same user ID), or
 *     you are root. Otherwise kill() fails with EPERM.
 *   - Kernel threads have no user-space code; the kernel ignores
 *     SIGSTOP/SIGKILL for them, even from root.
 *   - PID 1 (init) is protected: SIGSTOP/SIGKILL sent to it from inside
 *     the system are ignored, even from root.
 */
enum ControlStatus control_status(const struct Process *p) {
    if (p->pid == 1) return CTRL_INIT;
    if (p->is_kernel_thread) return CTRL_KERNEL;

    uid_t me = geteuid();
    if (me == 0 || p->uid == me) return CTRL_YES;
    return CTRL_NO_OWNER;
}

const char *control_label(enum ControlStatus s) {
    switch (s) {
        case CTRL_YES:      return "YES";
        case CTRL_NO_OWNER: return "NO";
        case CTRL_KERNEL:   return "KERN";
        case CTRL_INIT:     return "INIT";
    }
    return "?";
}

static const char *my_username(void) {
    static char name[USER_LEN];
    struct passwd *pw = getpwuid(geteuid());
    if (pw != NULL) {
        snprintf(name, sizeof(name), "%s", pw->pw_name);
    } else {
        snprintf(name, sizeof(name), "uid %u", (unsigned)geteuid());
    }
    return name;
}

/* Write a human-readable reason into buf for why p can't be controlled. */
void explain_control(const struct Process *p, char *buf, size_t buf_size) {
    switch (control_status(p)) {
        case CTRL_INIT:
            snprintf(buf, buf_size,
                "PID %d (%s) is the init process. The kernel protects it: "
                "SIGSTOP/SIGKILL sent to it are ignored, even by root.",
                p->pid, p->name);
            break;
        case CTRL_KERNEL:
            snprintf(buf, buf_size,
                "PID %d (%s) is a kernel thread. It has no user-space code, "
                "and the kernel ignores SIGSTOP/SIGKILL for it, so it can't "
                "be paused or killed (even by root).",
                p->pid, p->name);
            break;
        case CTRL_NO_OWNER:
            snprintf(buf, buf_size,
                "PID %d (%s) belongs to '%s', but you are '%s'. Linux only "
                "lets a user signal their own processes (root can signal "
                "anyone). Re-run with sudo to control it.",
                p->pid, p->name, p->owner, my_username());
            break;
        case CTRL_YES:
            snprintf(buf, buf_size, "PID %d (%s) can be controlled.",
                     p->pid, p->name);
            break;
    }
}

/* ===================================================================
 * Sending signals
 * Each returns 0 on success, -1 on failure (e.g. no such PID).
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
 * Test-process helpers: lets the demo create its own pausable process
 * (a child of this program, so we own it and signals are permitted).
 * =================================================================== */
#define MAX_TEST_PROCS 32
static pid_t test_pids[MAX_TEST_PROCS];
static int test_count = 0;

/*
 * fork() a child that burns CPU (`yes > /dev/null`), so it shows up near
 * the top of the table at ~100% CPU. Returns the child's PID, or -1.
 *
 * JAVA NOTE: fork() has no Java equivalent. It clones the running program;
 * it returns TWICE -- 0 in the new child, the child's PID in the parent.
 */
pid_t create_test_process(void) {
    if (test_count >= MAX_TEST_PROCS) {
        fprintf(stderr, "Too many test processes created.\n");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        // ---- child: silence output, then become `yes` ----
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
        }
        execlp("yes", "yes", (char *)NULL);
        for (;;) { }   // fallback busy loop if `yes` isn't installed
    }

    // ---- parent ----
    test_pids[test_count++] = pid;
    return pid;
}

/*
 * A killed child lingers as a zombie (state 'Z') until its parent calls
 * wait(). WNOHANG = don't block if nothing has exited.
 */
void reap_children(void) {
    while (waitpid(-1, NULL, WNOHANG) > 0) { }
}

/* Kill every test process we created, so none keep burning CPU after exit. */
void cleanup_test_processes(void) {
    for (int i = 0; i < test_count; i++) {
        kill(test_pids[i], SIGKILL);
        waitpid(test_pids[i], NULL, 0);   // SIGKILL can't be ignored, so this won't hang
    }
}

/* ===================================================================
 * was member2_display.c
 * =================================================================== */
/*
 * member2_display.c
 * -------------------------------------------------------
 * MEMBER 2's PART A -- sorting and displaying the process table.
 *
 * JAVA NOTE: qsort() is C's version of Collections.sort() /
 * Arrays.sort() with a Comparator. Instead of a Comparator object,
 * you pass a plain function pointer that qsort() calls internally.
 * -------------------------------------------------------
 */


/* Sort by cpu_percent DESCENDING (highest CPU first, like `top`). */
static int compare_cpu_desc(const void *a, const void *b) {
    const struct Process *pa = (const struct Process *)a;
    const struct Process *pb = (const struct Process *)b;

    if (pa->cpu_percent < pb->cpu_percent) return 1;   // a goes after b
    if (pa->cpu_percent > pb->cpu_percent) return -1;  // a goes before b
    return 0;
}

void sort_by_cpu_desc(struct Process procs[], int count) {
    qsort(procs, count, sizeof(struct Process), compare_cpu_desc);
}

/*
 * Print the process table, like a mini `top` snapshot.
 *
 * CTRL column = can YOU pause/kill this process?
 *   YES  - you own it (or you are root)
 *   NO   - owned by another user; Linux will refuse the signal
 *   KERN - kernel thread; the kernel ignores SIGSTOP/SIGKILL for it
 *   INIT - PID 1; the kernel protects it
 *
 * If only_controllable is non-zero, rows with CTRL != YES are hidden.
 */
void display_table(struct Process procs[], int count, int only_controllable) {
    printf("\n%-7s %-7s %-10s %-5s %-10s %-8s %-5s %s\n",
           "PID", "PPID", "USER", "STATE", "MEM(kB)", "CPU%", "CTRL", "NAME");
    printf("-----------------------------------------------------------------------\n");

    int shown = 0;
    for (int i = 0; i < count; i++) {
        enum ControlStatus cs = control_status(&procs[i]);
        if (only_controllable && cs != CTRL_YES) continue;

        printf("%-7d %-7d %-10.10s %-5c %-10ld %-8.2f %-5s %s\n",
               procs[i].pid, procs[i].ppid, procs[i].owner, procs[i].state,
               procs[i].mem_kb, procs[i].cpu_percent,
               control_label(cs), procs[i].name);
        shown++;
    }
    printf("-----------------------------------------------------------------------\n");
    if (only_controllable) {
        printf("Showing %d of %d processes (filter ON: only ones you can control)\n",
               shown, count);
    } else {
        printf("Total processes: %d\n", count);
    }
    printf("CTRL: YES = you can control it | NO = owned by another user (needs sudo)\n"
           "      KERN = kernel thread | INIT = PID 1 (kernel-protected)\n");
}

/* ===================================================================
 * was member2_threshold.c
 * =================================================================== */
/*
 * member2_threshold.c
 * -------------------------------------------------------
 * MEMBER 2's PART B -- flagging processes that exceed CPU or
 *                       memory limits.
 * -------------------------------------------------------
 */


/*
 * Print a warning for any process whose CPU% or memory exceeds the
 * given limits. Returns how many were flagged.
 */
int check_thresholds(struct Process procs[], int count,
                      double cpu_limit, long mem_limit_kb) {
    int flagged = 0;

    for (int i = 0; i < count; i++) {
        int over_cpu = procs[i].cpu_percent > cpu_limit;
        int over_mem = procs[i].mem_kb > mem_limit_kb;

        if (over_cpu || over_mem) {
            printf("[WARNING] PID %d (%s) exceeds limit -- CPU: %.2f%% "
                   "(limit %.2f%%), MEM: %ld kB (limit %ld kB)\n",
                   procs[i].pid, procs[i].name, procs[i].cpu_percent,
                   cpu_limit, procs[i].mem_kb, mem_limit_kb);
            flagged++;
        }
    }

    return flagged;
}

/* ===================================================================
 * was member3_logger.c
 * =================================================================== */
/*
 * member3_logger.c
 * -------------------------------------------------------
 * MEMBER 3's PART B -- logging every control action taken, with
 *                       a timestamp, to a log file.
 *
 * JAVA NOTE: this is a very manual version of something like
 * java.util.logging -- we just open a file in "append" mode and
 * write formatted lines to it ourselves.
 * -------------------------------------------------------
 */


static char log_path[256] = "activity.log"; // default, can be overridden

/*
 * Call once at startup if you want a custom log file path.
 * Optional -- if never called, "activity.log" in the current
 * directory is used.
 */
void log_init(const char *path) {
    strncpy(log_path, path, sizeof(log_path) - 1);
    log_path[sizeof(log_path) - 1] = '\0';
}

/*
 * Append one line to the log file:
 *   [2026-09-21 14:03:11] KILL PID 4821 -- SUCCESS
 */
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
 * was member3_main.c
 * =================================================================== */
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

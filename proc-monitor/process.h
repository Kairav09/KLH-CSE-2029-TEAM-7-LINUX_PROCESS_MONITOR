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

#include <sys/types.h>   // pid_t, uid_t, size_t

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

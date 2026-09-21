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
 * lives in the matching .c file. Java doesn't really have this
 * split because .java files carry both declaration and body
 * together; C needs the header so multiple .c files can agree
 * on a shape without including each other's source.
 * -------------------------------------------------------
 */

#ifndef PROCESS_H
#define PROCESS_H

#define MAX_PROCESSES 1024

/* ---------------- shared data model ---------------- */
struct Process {
    int pid;
    int ppid;
    char state;           // 'R' running, 'S' sleeping, 'Z' zombie, etc.
    long mem_kb;           // VmRSS -- resident memory in kilobytes
    unsigned long utime;     // CPU ticks, user mode (raw, cumulative)
    unsigned long stime;     // CPU ticks, kernel mode (raw, cumulative)
    double cpu_percent;      // computed -- % of one CPU core since last scan
};

/* ---------------- Member 1: scanning ---------------- */
int is_all_digits(const char *s);
int parse_stat(int pid, struct Process *p);
int parse_status_mem(int pid, struct Process *p);
int scan_processes(struct Process procs[], int max_count);

/* ---------------- Member 1: cpu calc ---------------- */
struct Process *find_by_pid(struct Process procs[], int count, int pid);
void compute_cpu_percent(struct Process prev[], int prev_count,
                          struct Process curr[], int curr_count,
                          double elapsed_seconds);

/* ---------------- Member 2: display ---------------- */
void sort_by_cpu_desc(struct Process procs[], int count);
void display_table(struct Process procs[], int count);

/* ---------------- Member 2: thresholds ---------------- */
int check_thresholds(struct Process procs[], int count,
                      double cpu_limit, long mem_limit_kb);

/* ---------------- Member 3: control ---------------- */
int pause_process(int pid);
int resume_process(int pid);
int kill_process(int pid);

/* ---------------- Member 3: logger ---------------- */
void log_init(const char *path);
void log_action(const char *action, int pid, int result);

#endif // PROCESS_H

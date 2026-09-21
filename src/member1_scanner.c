/*
 * member1_scanner.c
 * -------------------------------------------------------
 * MEMBER 1's PART A -- scanning /proc for PID, PPID, STATE,
 *                       and memory (VmRSS).
 *
 * This is the same logic from the original member1_proc_scanner.c,
 * just moved into its own file now that struct Process lives in
 * process.h. No behavior changed -- only where the code lives.
 * -------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include "process.h"

int is_all_digits(const char *s) {
    if (*s == '\0') return 0;
    while (*s) {
        if (!isdigit((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

/*
 * Parse /proc/<pid>/stat for STATE, PPID, UTIME, STIME.
 * Field numbering starts right after the ')' that closes the
 * (comm) field: STATE=1, PPID=2, ..., UTIME=12, STIME=13.
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

    char *close_paren = strrchr(line, ')');
    if (!close_paren) return -1;

    p->pid = pid;

    char *rest = close_paren + 2; // skip ") "
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

/*
 * Parse /proc/<pid>/status for VmRSS (resident memory in kB).
 */
int parse_status_mem(int pid, struct Process *p) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);

    FILE *fp = fopen(path, "r");
    if (fp == NULL) return -1;

    char line[256];
    p->mem_kb = 0; // default if VmRSS line is missing (e.g. kernel threads)

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%ld", &p->mem_kb);
            break;
        }
    }

    fclose(fp);
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
        parse_status_mem(pid, &p);                // memory (best-effort)

        procs[count] = p;
        count++;
    }

    closedir(dir);
    return count;
}

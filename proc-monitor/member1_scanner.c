/*
 * member1_scanner.c
 * -------------------------------------------------------
 * MEMBER 1's PART A -- scanning /proc for PID, PPID, STATE,
 *                       name, owner, and memory (VmRSS).
 * -------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <pwd.h>
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

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

#include <stdio.h>
#include <time.h>
#include <string.h>
#include "process.h"

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
